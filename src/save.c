/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

//thanks to spicyjpeg for helping me understand this code

#include "save.h"

#ifdef USE_MEMCARD
#include <libmcrd.h>
#endif
#include "mem.h"
#include "stage.h"

//HAS to be BASCUS-scusid,somename
#define savetitle "bu00:BASCUS-41205Omni"
#define savename  "PSXFunkin Omniplasm Engine"

static const u8 saveIconPalette[32] = 
{
  	0x01, 0x84, 0xAF, 0x9D, 0xD0, 0x9D, 0x12, 0xA6, 0xC8, 0x9C, 0x64, 0x8C,
	0xE9, 0xA0, 0x6C, 0xA9, 0x33, 0xB6, 0x64, 0x90, 0x4D, 0xAD, 0xFF, 0xFF,
	0xF0, 0xA5, 0x5A, 0xEB, 0xB5, 0xD6, 0x6D, 0x99
};

static const u8 saveIconImage[128] = 
{
	0x00, 0x21, 0x32, 0x33, 0x33, 0x23, 0x02, 0x00, 0x00, 0x30, 0x00, 0x00,
	0x30, 0x33, 0x00, 0x00, 0x54, 0x00, 0x50, 0x55, 0x05, 0x03, 0x05, 0x04,
	0x44, 0x55, 0x55, 0x55, 0x55, 0x50, 0x55, 0x04, 0x64, 0x75, 0x58, 0x59,
	0x99, 0x55, 0x65, 0x06, 0x54, 0x55, 0x99, 0x99, 0x88, 0x59, 0x7A, 0x20,
	0x56, 0x0B, 0x9B, 0x99, 0x99, 0x99, 0xAA, 0x30, 0x00, 0x0B, 0x90, 0x99,
	0x0B, 0x9B, 0x0A, 0x33, 0x50, 0x55, 0x95, 0x99, 0x00, 0x9B, 0x30, 0x33,
	0x00, 0x95, 0x99, 0x99, 0x99, 0x99, 0x30, 0x33, 0x03, 0xC5, 0x08, 0x80,
	0x98, 0x99, 0x30, 0x33, 0x02, 0xD7, 0x00, 0x00, 0x8B, 0x08, 0x83, 0x33,
	0x02, 0xDE, 0x0A, 0xA0, 0xBB, 0x0D, 0x33, 0x23, 0x01, 0xEE, 0x6A, 0xAA,
	0xDD, 0x0D, 0x33, 0x13, 0x11, 0xE0, 0x00, 0x00, 0xDD, 0x30, 0x23, 0x11,
	0xFF, 0x01, 0xEE, 0xEE, 0x0E, 0xC2, 0x12, 0xF1
};

static void toShiftJIS(u8 *buffer, const char *text)
{
    int pos = 0;
    for (u32 i = 0; i < strlen(text); i++) 
    {
        u8 c = text[i];
        if (c >= '0' && c <= '9') { buffer[pos++] = 0x82; buffer[pos++] = 0x4F + c - '0'; }
        else if (c >= 'A' && c <= 'Z') { buffer[pos++] = 0x82; buffer[pos++] = 0x60 + c - 'A'; }
        else if (c >= 'a' && c <= 'z') { buffer[pos++] = 0x82; buffer[pos++] = 0x81 + c - 'a'; }
        else if (c == '(') { buffer[pos++] = 0x81; buffer[pos++] = 0x69; }
        else if (c == ')') { buffer[pos++] = 0x81; buffer[pos++] = 0x6A; }
        else /* space */ { buffer[pos++] = 0x81; buffer[pos++] = 0x40; }
    }
}

static void initSaveFile(SaveFile *file, const char *name) 
{
	file->id = 0x4353;
 	file->iconDisplayFlag = 0x11;
 	file->iconBlockNum = 1;
  	toShiftJIS(file->title, name);
 	memcpy(file->iconPalette, saveIconPalette, 32);
 	memcpy(file->iconImage, saveIconImage, 128);
}

void defaultSettings()
{
	stage.prefs.songtimer = 1;

	for (StageId i = 0; i < StageId_Max; i++)
	{
		for (StageDiff j = 0; j < StageDiff_Max; j++)
			stage.prefs.savescore[i][j] = 0;
	}
}

boolean ReadSave()
{
#ifndef USE_MEMCARD
	return false;
#else
	boolean ok = false;
	SaveFile *file = NULL;
	int fd = open(savetitle, 0x0001);
	if (fd < 0) // file doesnt exist 
		return false;

	file = (SaveFile*)Mem_Alloc(sizeof(SaveFile));
	if (file == NULL)
	{
		close(fd);
		return false;
	}

	if (read(fd, (void *)file, sizeof(SaveFile)) == sizeof(SaveFile)) 
		printf("ok\n");
	else {
		printf("read error\n");
		goto done;
	}
	memcpy((void *)&stage.prefs, (const void *)file->saveData, sizeof(stage.prefs));
	ok = true;

done:
	Mem_Free(file);
	close(fd);
	return ok;
#endif
}

void WriteSave()
{	
#ifndef USE_MEMCARD
	return;
#else
	SaveFile *file;
	int fd = open(savetitle, 0x0002);

	if (fd < 0) // if save doesnt exist make one
		fd =  open(savetitle, 0x0202 | (1 << 16));

	file = (SaveFile*)Mem_Alloc(sizeof(SaveFile));
	if (file == NULL)
	{
		if (fd >= 0)
			close(fd);
		return;
	}

	initSaveFile(file, savename);
  	memcpy((void *)file->saveData, (const void *)&stage.prefs, sizeof(stage.prefs));
	
	if (fd >= 0) {
	  	if (write(fd, (void *)file, sizeof(SaveFile)) == sizeof(SaveFile)) 
	  		printf("ok\n");
	 	else 
	 		printf("write error\n");  // if save doesnt exist do a error
		close(fd);
	} 
	else 
		printf("open error %d\n", fd);  // failed to save

	Mem_Free(file);
#endif
}

//initiliaze memory card
void MCRD_Init(void)
{
#ifdef USE_MEMCARD
  InitCARD(1);
	StartCARD();
	_bu_init();	
	ChangeClearPAD(0);
#endif
}
