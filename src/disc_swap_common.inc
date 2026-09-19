#include "audio.h"
#include "font.h"
#include "gfx.h"
#include "io.h"
#include "main.h"
#include "pad.h"
#include "psx.h"
#include "trans.h"

#define DISC_SWAP_READY_TIMEOUT 900
#define DISC_SWAP_SETTLE_FRAMES 45
#define DISC_SWAP_CANCELLED 1
#define DISC_SWAP_OPEN_TIMEOUT 900
#define DISC_SWAP_CHECK_RETRIES 5

static Gfx_Tex *disc_swap_back_tex = NULL;
static Gfx_Tex *disc_swap_disc_tex = NULL;
static FontData *disc_swap_font_title = NULL;
static FontData *disc_swap_font_body = NULL;
static const char *disc_swap_screen_title = NULL;
static const char *disc_swap_screen_message = NULL;
static char disc_swap_message_buffer[128];

void DiscSwap_SetScreenAssets(Gfx_Tex *back, Gfx_Tex *disc, FontData *title_font, FontData *body_font)
{
	disc_swap_back_tex = back;
	disc_swap_disc_tex = disc;
	disc_swap_font_title = title_font;
	disc_swap_font_body = body_font;
}

static void DiscSwap_DrawFancyScreen(const char *title, const char *message)
{
	RECT back_src = {0, 0, 255, 255};
	RECT back_dst = {0, -SCREEN_WIDEADD2, SCREEN_WIDTH, SCREEN_WIDTH * 4 / 5};
	RECT disc_src = {0, 0, 128, 85};
	RECT disc_dst = {8, SCREEN_HEIGHT - 51, 64, 43};
	char disc_number[8];
	GameLoop prev_gameloop = gameloop;

	gameloop = GameLoop_Menu;

	/* Exact menu.c title style requested. */
	if (disc_swap_font_title != NULL && disc_swap_font_title->draw != NULL)
		disc_swap_font_title->draw(disc_swap_font_title, title,
			SCREEN_WIDTH2, SCREEN_HEIGHT2 - 96, FontAlign_Center);

	/* Body uses the CDR font supplied by Menu_Load(). */
	if (disc_swap_font_body != NULL && disc_swap_font_body->draw != NULL)
	{
		disc_swap_font_body->draw(disc_swap_font_body, message,
			32, 124, FontAlign_Left);
		disc_swap_font_body->draw(disc_swap_font_body, "Insert disc and close lid.",
			32, 148, FontAlign_Left);
		disc_swap_font_body->draw(disc_swap_font_body, "Press O to cancel.",
			32, 164, FontAlign_Left);
	}

	/* Draw the currently detected disc number in the same bold font used by menu.c. */
	sprintf(disc_number, "%d", currentDisc);
	if (disc_swap_font_title != NULL && disc_swap_font_title->draw != NULL)
		disc_swap_font_title->draw(disc_swap_font_title, disc_number,
			36, SCREEN_HEIGHT - 36, FontAlign_Center);

	/* Draw disc.tim as a scaled 128x85 source into a 64x43 destination. */
	if (disc_swap_disc_tex != NULL)
		Gfx_DrawTex(disc_swap_disc_tex, &disc_src, &disc_dst);

	/* Use the exact same background draw used by menu.c. */
	if (disc_swap_back_tex != NULL)
		Gfx_DrawTexCol(disc_swap_back_tex, &back_src, &back_dst,
			64 >> 1, 64 >> 1, 64 >> 1);

	Gfx_Flip();	
	gameloop = prev_gameloop;
}
static void DiscSwap_Service(void)
{
	Pad_Update();
	if (disc_swap_screen_title != NULL && disc_swap_screen_message != NULL)
		DiscSwap_DrawFancyScreen(disc_swap_screen_title, disc_swap_screen_message);
	else
		VSync(0);
}

static u8 DiscSwap_ReadStatus(void)
{
	CdControl(CdlNop, NULL, NULL);
	return CdStatus();
}

static boolean DiscSwap_DriveOpen(void)
{
	return (DiscSwap_ReadStatus() & CdlStatShellOpen) != 0;
}

static void DiscSwap_WaitFrames(int frames)
{
	while (frames-- > 0)
		DiscSwap_Service();
}

static void DiscSwap_WaitForButtonRelease(void)
{
	while (UserPressedButton())
		DiscSwap_Service();
}

static int DiscSwap_DetectDisc(void)
{
	CdlFILE file;

	/* Do not use IO_ExistFile() here.  Its file cache is associated with the
	 * previous currentDisc, so it can keep reporting the old disc after a swap.
	 * CdSearchFile() queries the CD filesystem for the currently mounted disc. */
	if (CdSearchFile(&file, "\\DISC1.ID;1") != NULL)
		return 1;
	if (CdSearchFile(&file, "\\DISC2.ID;1") != NULL)
		return 2;
	if (CdSearchFile(&file, "\\DISC3.ID;1") != NULL)
		return 3;
	if (CdSearchFile(&file, "\\DISC4.ID;1") != NULL)
		return 4;
	return 0;
}

void DisplayMessage(const char* message)
{
	disc_swap_screen_title = "CHANGE DISC";
	strncpy(disc_swap_message_buffer, message, sizeof(disc_swap_message_buffer) - 1);
	disc_swap_message_buffer[sizeof(disc_swap_message_buffer) - 1] = '\0';
	disc_swap_screen_message = disc_swap_message_buffer;
	Trans_Clear();
	DiscSwap_DrawFancyScreen(disc_swap_screen_title, disc_swap_screen_message);
	DiscSwap_DrawFancyScreen(disc_swap_screen_title, disc_swap_screen_message);
	printf("%s\n", message);
}

int UserPressedButton(void)
{
	return (pad_state.press & PAD_SELECT) || (pad_state.press & PAD_START) || (pad_state.press & PAD_CIRCLE);
}

int CheckDiscIndicator(void)
{
	return DiscSwap_DetectDisc() != 0;
}

void CheckCurrentDisc(void)
{
	int detected_disc = DiscSwap_DetectDisc();
	if (detected_disc != 0)
		currentDisc = detected_disc;
}

int CdTrayReq(int req, void* param)
{
	(void)req;
	(void)param;
	return DiscSwap_DriveOpen() ? CdlTrayOpen : CdlTrayClose;
}

static boolean DiscSwap_WaitForDriveReady(int timeout)
{
	int ready;

	while (timeout-- > 0)
	{
		if (!DiscSwap_DriveOpen())
		{
			ready = CdDiskReady(1);
			if (ready == CdlComplete)
				return true;
		}
		DiscSwap_Service();
	}

	return false;
}

static boolean DiscSwap_ResetChangedDisc(void)
{
	u8 result[8];

	// Stop any old-sector activity before asking the BIOS to see the new disc.
	CdReadBreak();
	CdControlB(CdlPause, NULL, NULL);
	CdSync(0, NULL);
	DiscSwap_WaitFrames(DISC_SWAP_SETTLE_FRAMES);

	CdInit();
	CdSync(0, NULL);
	DiscSwap_WaitFrames(DISC_SWAP_SETTLE_FRAMES);

	if (!DiscSwap_WaitForDriveReady(DISC_SWAP_READY_TIMEOUT))
		return false;

	CdControlB(CdlStandby, NULL, NULL);
	CdSync(0, NULL);
	DiscSwap_WaitFrames(DISC_SWAP_SETTLE_FRAMES);

	// Force a TOC read; emulators like DuckStation refresh the swapped image here.
	CdControlB(CdlGetTN, NULL, result);
	CdSync(0, NULL);
	DiscSwap_WaitFrames(DISC_SWAP_SETTLE_FRAMES);

	return true;
}

static int DiscSwap_DetectReadyDisc(void)
{
	int retry;
	int detected_disc;

	for (retry = 0; retry < DISC_SWAP_CHECK_RETRIES; retry++)
	{
		DisplayMessage("Initializing new disc...");
		if (!DiscSwap_ResetChangedDisc())
		{
			DisplayMessage("Waiting for disc drive...");
			DiscSwap_WaitFrames(60);
			continue;
		}

		DisplayMessage("Reading disc ID...");
		detected_disc = DiscSwap_DetectDisc();
		if (detected_disc != 0)
			return detected_disc;

		DisplayMessage("Disc ID not found, retrying...");
		DiscSwap_WaitFrames(60);
	}

	return 0;
}

int CdSwitchDisc(void)
{
	int timeout;
	int detected_disc;
	boolean saw_open = false;

	DisplayMessage("Insert requested disc, then close lid.");

	Audio_StopXA();
	CdControlB(CdlPause, NULL, NULL);
	CdSync(0, 0);

	DiscSwap_WaitForButtonRelease();
	for (timeout = 0; timeout < DISC_SWAP_OPEN_TIMEOUT; timeout++)
	{
		if (DiscSwap_DriveOpen())
			saw_open = true;
		if (saw_open && !DiscSwap_DriveOpen())
			break;
		DiscSwap_Service();
		if (pad_state.press & PAD_CIRCLE)
			break;
	}
	if (pad_state.press & PAD_CIRCLE)
	{
		DiscSwap_WaitForButtonRelease();
		return DISC_SWAP_CANCELLED;
	}
	DiscSwap_WaitForButtonRelease();

	if (saw_open)
		DisplayMessage("Close lid and wait...");
	else
		DisplayMessage("Checking disc...");

	while (DiscSwap_DriveOpen())
	{
		DiscSwap_Service();
		if (pad_state.press & PAD_CIRCLE)
		{
			DiscSwap_WaitForButtonRelease();
			return DISC_SWAP_CANCELLED;
		}
	}

	detected_disc = DiscSwap_DetectReadyDisc();
	if (detected_disc != 0)
	{
		currentDisc = detected_disc;

		Audio_ReloadXAFiles();
		return 0;
	}

	return -3;
}

int HandleDiscSwap(void)
{
	int switch_result = CdSwitchDisc();

	if (switch_result == 0)
	{
		char message[48];
		sprintf(message, "Disc %d loaded.", currentDisc);
		DisplayMessage(message);
		return 0;
	}

	if (switch_result == DISC_SWAP_CANCELLED)
	{
		DisplayMessage("Disc swap cancelled.");
		return DISC_SWAP_CANCELLED;
	}

	if (switch_result == -3)
		DisplayMessage("DISC1.ID, DISC2.ID, DISC3.ID, or DISC4.ID was not found.");
	else
		DisplayMessage("Failed to load new disc.");
	return switch_result;
}

void CheckDiscSwap(void)
{
	u8 status;

	CdControl(CdlNop, NULL, NULL);
	status = CdStatus();

	if (status & CdlStatShellOpen)
		DisplayMessage("Disc change detected.");
}

void addFileToList(const char *filename)
{
	(void)filename;
}

void traverseDirectory(const char *path)
{
	(void)path;
}

void listAllFiles(void)
{
}

void loadFile(const char *filename)
{
	(void)filename;
}

void loadAllFiles(void)
{
}

void freeFileList(void)
{
}
