/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "str.h"
#include "psx.h"
#include "gfx.h"
#include "psn00b/strnoob.h"
#include "mem.h"
#include "timer.h"
#include "audio.h"
#include "stage.h"
#include "io.h"

/*
  Optimized STR player for weak PS1 targets/emulators.

  Main changes from the older player:
  - Uses native 16px MDEC macroblock slices so decoded columns map exactly to
    VRAM on real hardware.
  - No hot busy-spin while waiting for CD sectors. The wait loop yields with
    VSync(0), which is much friendlier to slower emulators.
  - No forced DrawSync(0) every decoded frame. We only block before starting
    the next MDEC input when the MDEC/GPU path is actually still busy.
  - Cached frame placement. The centering math is only recalculated when the
    frame size or draw clip changes.
  - Reduced CD/XA command spam. XA rearm is throttled and only used around
    ReadS/seek/resume instead of hammering CdMix every frame.
  - Runs gameplay and buffer flipping at a fixed 60 Hz while presenting the
    decoded STR stream at 30 fps. The CD callback assembles the next frame in
    the spare buffer while the current frame remains visible in VRAM.
  - Stop/cleanup is idempotent so the player does not do duplicate sync/stop
    work when a movie naturally ends.
*/

#define BLOCK_SIZE 16
#define STR_GAME_FPS 60
#define STR_VIDEO_FPS 30
#define STR_MAX_SLICES (SCREEN_WIDTH / BLOCK_SIZE)
#define STR_AUDIO_REARM_FRAMES 6
#define STR_SPEEDUP_STARVE_TICKS 12
#define STR_RAW_SECTOR_SIZE 2336

// All non-audio sectors in .STR files begin with this 32-byte header.
typedef struct
{
	u16 magic;          // Always 0x0160
	u16 type;           // 0x8001 for MDEC
	u16 sector_id;      // Chunk number (0 = first chunk of this frame)
	u16 sector_count;   // Total number of chunks for this frame
	u32 frame_id;       // Frame number
	u32 bs_length;      // Total length of this frame in bytes

	u16 width, height;
	u8  bs_header[8];
	u32 _reserved;
} STR_Header;

typedef struct
{
	u16 width, height;
	u32 bs_data[0x2000];   // Bitstream data read from disc
	u32 mdec_data[0x8000]; // Decompressed data to feed to MDEC
} StreamBuffer;

typedef struct
{
	StreamBuffer frames[2];
	/* Two complete slice caches: one displayed while MDEC fills the other. */
	u32 slices[2][STR_MAX_SLICES][BLOCK_SIZE * SCREEN_HEIGHT2];

	RECT slice_pos;
	RECT cached_clip;

	int frame_id;
	int sector_count;
	int dropped_frames;
	int frame_width;
	int last_frame_w;
	int last_frame_h;
	int pending_out_words;
	u8 cache_width[2];
	u16 cache_height[2];

	volatile s8 sector_pending;
	volatile s8 frame_ready;
	volatile s8 ready_frame;
	volatile s8 cur_frame;
	volatile s8 cur_slice;
	volatile s8 decode_cache;
	volatile s8 display_cache;
	volatile s8 cache_ready;
	volatile s8 out_active;
} StreamContext;

typedef struct
{
	const char* name;
	StageId id;
} STR_Def;

static StreamContext* str_ctx;
static u32 str_sectors_remaining;
static u32 str_file_end_sector;
static boolean str_file_exhausted;
static CdlLOC str_pause_loc;
static u8 str_movie_audio_rearm_frames;
static u8 str_starve_ticks;
static boolean str_stream_stopped;
static boolean str_audio_mode_valid;
static boolean str_double_speed;
static boolean str_outside_gameplay;

// Temporary sector header. Must not live on interrupt callback stack.
static STR_Header* sector_header;
static void STR_StopStream(void);

static const STR_Def str_def[] = {
	#include "strdef.h"
};

static void STR_SetMovieCdMode(void)
{
	if (str_audio_mode_valid)
		return;

	CdlFILTER filter;
	filter.file = 1;
	filter.chan = 0;
	CdControlB(CdlSetfilter, (u8*)&filter, NULL);

	CdlATV cd_vol;
	cd_vol.val0 = cd_vol.val1 = cd_vol.val2 = cd_vol.val3 = 0x40;
	CdMix(&cd_vol);

	u8 mode = CdlModeRT | CdlModeSF;
	if (str_double_speed)
		mode |= CdlModeSpeed;
	CdControlB(CdlSetmode, (u8*)&mode, NULL);

	str_audio_mode_valid = true;
}

static void STR_EnableDoubleSpeed(void)
{
	if (str_double_speed)
		return;

	str_double_speed = true;
	str_starve_ticks = 0;

	/* Promote a legacy high-rate stream without pausing or seeking it. */
	u8 mode = CdlModeRT | CdlModeSpeed | CdlModeSF;
	CdControlF(CdlSetmode, &mode);
}

static void STR_RearmMovieAudio(void)
{
	CdlFILTER filter;
	filter.file = 1;
	filter.chan = 0;
	CdControlF(CdlSetfilter, (u8*)&filter);
}

static void STR_ResetDecodeState(void)
{
	str_ctx->frame_id = -1;
	str_ctx->sector_count = 0;
	str_ctx->dropped_frames = 0;
	str_ctx->sector_pending = 0;
	str_ctx->frame_ready = 0;
	str_ctx->ready_frame = 0;
	str_ctx->cur_frame = 0;
	str_ctx->cur_slice = 0;
	str_ctx->decode_cache = 0;
	str_ctx->display_cache = 0;
	str_ctx->cache_ready = 0;
	str_ctx->out_active = 0;
	str_ctx->frame_width = 0;
	str_ctx->last_frame_w = -1;
	str_ctx->last_frame_h = -1;
	str_ctx->pending_out_words = 0;
	str_ctx->cached_clip.x = -32768;
	str_ctx->cached_clip.y = -32768;
	str_ctx->cached_clip.w = -1;
	str_ctx->cached_clip.h = -1;
}

void cd_sector_handler(void)
{
	StreamBuffer *frame = &str_ctx->frames[str_ctx->cur_frame];

	CdGetSector(sector_header, sizeof(STR_Header) / 4);

	if (str_sectors_remaining != 0)
	{
		str_sectors_remaining--;
		if (str_sectors_remaining == 0)
			str_file_exhausted = true;
	}

	/* XA audio sectors are interleaved with the MDEC sectors in an STR. */
	if (sector_header->magic != 0x0160)
		return;

	if (sector_header->type != 0x8001)
		return;

	if ((int)sector_header->frame_id < str_ctx->frame_id)
	{
		str_ctx->frame_ready = -1;
		return;
	}

	if ((int)sector_header->frame_id > str_ctx->frame_id)
	{
		if (str_ctx->frame_id >= 0 && str_ctx->sector_count != 0)
			str_ctx->dropped_frames++;

		str_ctx->frame_id = sector_header->frame_id;
		str_ctx->sector_count = sector_header->sector_count;
		str_ctx->cur_frame ^= 1;

		frame = &str_ctx->frames[str_ctx->cur_frame];
		frame->width  = (sector_header->width  + 15) & 0xfff0;
		frame->height = (sector_header->height + 15) & 0xfff0;
	}

	str_ctx->sector_count--;
	CdGetSector(
		&(frame->bs_data[(2016 / 4) * sector_header->sector_id]),
		2016 / 4
	);

	/*
	 * Publish as soon as every sector is present. Waiting for the following
	 * frame header loses the final movie frame and shortens the prefetch window.
	 */
	if (str_ctx->sector_count == 0)
	{
		str_ctx->ready_frame = str_ctx->cur_frame;
		str_ctx->frame_ready = 1;
	}
}

void mdec_dma_handler(void)
{
	if (str_ctx->sector_pending)
	{
		cd_sector_handler();
		str_ctx->sector_pending--;
	}

	str_ctx->cur_slice++;
	str_ctx->slice_pos.x += BLOCK_SIZE;

	if (str_ctx->slice_pos.x < str_ctx->frame_width)
	{
		DecDCTout(
			str_ctx->slices[str_ctx->decode_cache][str_ctx->cur_slice],
			str_ctx->pending_out_words
		);
	}
	else
	{
		str_ctx->display_cache = str_ctx->decode_cache;
		str_ctx->decode_cache ^= 1;
		str_ctx->cache_ready = 1;
		str_ctx->out_active = 0;
	}
}

void cd_event_handler(u8 event, u8 *payload)
{
	(void)payload;

	if (event != CdlDataReady)
		return;

	/* Avoid CD DMA and MDEC output DMA fighting each other. */
	if (DecDCTinSync(1) || DecDCToutSync(1))
	{
		/* Preserve every ready event instead of collapsing several into one. */
		if (str_ctx->sector_pending < 127)
			str_ctx->sector_pending++;
	}
	else
		cd_sector_handler();
}

StreamBuffer *get_next_frame(void)
{
	u16 wait_frames = 0;

	while (!str_ctx->frame_ready)
	{
		// Never burn a whole frame in a tight CPU spin on weak emulators.
		VSync(0);
		if (++wait_frames >= 300)
		{
			STR_StopStream();
			return NULL;
		}
	}

	if (str_ctx->frame_ready < 0)
		return NULL;

	str_ctx->frame_ready = 0;
	return &str_ctx->frames[str_ctx->ready_frame];
}

static void STR_InitStream(void)
{
	EnterCriticalSection();
	DecDCToutCallback(&mdec_dma_handler);
	CdReadyCallback(&cd_event_handler);
	ExitCriticalSection();

	DecDCTvlcCopyTableV3((VLC_TableV3*)0x1f800000);

	stage.movie_is_playing = true;
	stage.movie_paused = false;
	stage.movie_pos = 0;
	stage.audio_last_pos_before_movie = Audio_TellXA_Milli();
	/* Stop the song's XA read before the STR takes ownership of the drive.
	 * The STR's own file-1/channel-0 XA sectors remain audible through ReadS. */
	Audio_HandoffXA();
	str_movie_audio_rearm_frames = STR_AUDIO_REARM_FRAMES;
	str_starve_ticks = 0;
	str_stream_stopped = false;
	str_audio_mode_valid = false;
	str_double_speed = false;

	STR_ResetDecodeState();
}

static void STR_StopStream(void)
{
	if (str_stream_stopped)
		return;

	str_stream_stopped = true;

	/* Stop callbacks before yielding CD ownership. CdlPause is deliberately
	 * non-blocking: the following owner will synchronize/seek as needed, and
	 * waiting here makes a real drive visibly hold the final movie frame. */
	EnterCriticalSection();
	CdReadyCallback(NULL);
	ExitCriticalSection();
	CdControlF(CdlPause, NULL);

	// Only block for GPU/MDEC if there was active output work.
	if (str_ctx != NULL && str_ctx->out_active)
	{
		DrawSync(0);
		DecDCToutSync(0);
	}
	DecDCTinSync(0);

	EnterCriticalSection();
	DecDCToutCallback(NULL);
	ExitCriticalSection();

	stage.str_cleanup_notes = true;
	Stage_ClearPassedMovieNotes();
	stage.str_cleanup_notes = false;
	stage.movie_is_playing = false;
	stage.movie_paused = false;
	stage.swap_grace_frames = 60;
	str_audio_mode_valid = false;
}

void Str_StopMovie(void)
{
	if (stage.movie_is_playing)
		STR_StopStream();
}

void Str_SetPaused(boolean paused)
{
	if (paused == stage.movie_paused)
		return;

	stage.movie_paused = paused;

	if (paused)
	{
		/* Stop new sectors from entering the decoder before asking the drive to
		 * pause. Otherwise callbacks can continue draining the STR toward EOF
		 * while the pause menu is open. */
		EnterCriticalSection();
		CdReadyCallback(NULL);
		ExitCriticalSection();
		CdControlB(CdlGetlocL, NULL, (u8*)&str_pause_loc);
		CdControlB(CdlPause, NULL, NULL);
		DecDCTinSync(0);
		DecDCToutSync(0);
	}
	else
	{
		u32 resume_sector = CdPosToInt(&str_pause_loc);
		str_ctx->frame_id = -1;
		str_ctx->sector_count = 0;
		str_ctx->sector_pending = 0;
		str_ctx->frame_ready = 0;
		str_ctx->out_active = 0;
		str_file_exhausted = false;
		str_sectors_remaining =
			(resume_sector < str_file_end_sector) ?
			(str_file_end_sector - resume_sector) : 0;
		str_audio_mode_valid = false;
		STR_SetMovieCdMode();
		EnterCriticalSection();
		CdReadyCallback(&cd_event_handler);
		ExitCriticalSection();
		CdControlF(CdlReadS, (u8*)&str_pause_loc);
		STR_RearmMovieAudio();
		str_movie_audio_rearm_frames = STR_AUDIO_REARM_FRAMES;
	}
}

static void STR_PrepareFramePlacement(StreamBuffer *frame)
{
	RECT *fb_clip = Gfx_GetDrawClip();

	if (str_ctx->last_frame_w == frame->width &&
		str_ctx->last_frame_h == frame->height &&
		str_ctx->cached_clip.x == fb_clip->x &&
		str_ctx->cached_clip.y == fb_clip->y &&
		str_ctx->cached_clip.w == fb_clip->w &&
		str_ctx->cached_clip.h == fb_clip->h)
	{
		str_ctx->slice_pos.x = fb_clip->x + ((fb_clip->w - frame->width) / 2);
		str_ctx->slice_pos.y = fb_clip->y + ((fb_clip->h - frame->height) / 2);
		str_ctx->slice_pos.w = BLOCK_SIZE;
		str_ctx->slice_pos.h = frame->height;
		str_ctx->frame_width = str_ctx->slice_pos.x + frame->width;
		str_ctx->pending_out_words = (BLOCK_SIZE * str_ctx->slice_pos.h) / 2;
		return;
	}

	str_ctx->cached_clip = *fb_clip;
	str_ctx->last_frame_w = frame->width;
	str_ctx->last_frame_h = frame->height;

	str_ctx->slice_pos.x = fb_clip->x + ((fb_clip->w - frame->width) / 2);
	str_ctx->slice_pos.y = fb_clip->y + ((fb_clip->h - frame->height) / 2);
	str_ctx->slice_pos.w = BLOCK_SIZE;
	str_ctx->slice_pos.h = frame->height;
	str_ctx->frame_width = str_ctx->slice_pos.x + frame->width;
	str_ctx->pending_out_words = (BLOCK_SIZE * str_ctx->slice_pos.h) / 2;
}

static void STR_RenderFrame(StreamBuffer *frame)
{
	VLC_Context vlc_ctx;
	DecDCTvlcStart(&vlc_ctx, frame->mdec_data, sizeof(frame->mdec_data) / 4, frame->bs_data);

	if (!stage.movie_is_playing)
		return;

	// Do not force a full GPU sync every frame. Only wait if the previous MDEC
	// path is still active before feeding the next frame.
	if (str_ctx->out_active)
		DecDCToutSync(0);
	DecDCTinSync(0);
	/* The last MDEC callback may have queued a LoadImage still using the GPU. */
	DrawSync(0);

	DecDCTin(frame->mdec_data, DECDCT_MODE_16BPP);

	STR_PrepareFramePlacement(frame);

	str_ctx->cur_slice = 0;
	str_ctx->cache_width[str_ctx->decode_cache] = frame->width / BLOCK_SIZE;
	str_ctx->cache_height[str_ctx->decode_cache] = frame->height;
	str_ctx->out_active = 1;
	DecDCTout(
		str_ctx->slices[str_ctx->decode_cache][0],
		str_ctx->pending_out_words
	);
}

static void STR_RestoreVideoBackground(void)
{
	if (!str_ctx->cache_ready)
		return;

	u8 cache = str_ctx->display_cache;
	u8 slice_count = str_ctx->cache_width[cache];
	s16 height = str_ctx->cache_height[cache];
	RECT *clip = Gfx_GetDrawClip();
	RECT dst;

	dst.x = clip->x + ((clip->w - (slice_count * BLOCK_SIZE)) / 2);
	dst.y = clip->y + ((clip->h - height) / 2);
	dst.w = BLOCK_SIZE;
	dst.h = height;

	for (u8 slice = 0; slice < slice_count; slice++)
	{
		LoadImage(&dst, str_ctx->slices[cache][slice]);
		dst.x += BLOCK_SIZE;
	}
}

/*
 * Run once per 60 Hz game tick. A completed frame is consumed only on a
 * 30 fps presentation slot; until then it stays prefetched in the alternate
 * stream buffer and the already uploaded frame remains on screen.
 */
static void Str_Update(u8 *video_clock)
{
	/* Restore a clean movie background before this tick's HUD ordering table. */
	STR_RestoreVideoBackground();
	Gfx_FlipWithoutOT();

	/* Loading and between-song movies have no active stage pause menu. In
	 * those contexts Start keeps its traditional skip behavior. */
	if (str_outside_gameplay && (pad_state.press & PAD_START))
	{
		STR_StopStream();
		return;
	}

	/* Stage_Tick owns the normal pause menu. Keep calling it while paused so
	 * the menu receives input and draws over the cached movie frame. */
	if (stage.note_scroll >= 0)
		Stage_Tick();

	Str_SetPaused(stage.paused);


	if (!stage.movie_is_playing || stage.movie_paused)
		return;

	if (*video_clock < STR_GAME_FPS)
		*video_clock += STR_VIDEO_FPS;
	if (*video_clock < STR_GAME_FPS)
		return;

	if (str_ctx->frame_ready < 0)
	{
		STR_StopStream();
		return;
	}

	/* Keep the presentation slot pending if disc/MDEC input is late. */
	if (str_ctx->frame_ready == 0)
	{
		if (str_file_exhausted)
		{
			/* FINAL's 1x mux is about 0.3 seconds shorter than its 34.5s
			 * source. Keep its last decoded frame up for that missing tail so
			 * the cutscene reaches the intended chart-resume timestamp. */
			STR_StopStream();
		}
		else if (!str_double_speed && ++str_starve_ticks >= STR_SPEEDUP_STARVE_TICKS)
			STR_EnableDoubleSpeed();
		return;
	}

	str_starve_ticks = 0;
	*video_clock -= STR_GAME_FPS;
	str_ctx->frame_ready = 0;
	STR_RenderFrame(&str_ctx->frames[str_ctx->ready_frame]);
}

void Str_Init(void)
{
	DecDCTReset(0);
	stage.movie_is_playing = false;
	stage.movie_paused = false;
	str_audio_mode_valid = false;
	str_stream_stopped = true;
}

void Str_PlayFile(CdlFILE* file)
{
	str_ctx = Mem_Alloc(sizeof(StreamContext));
	sector_header = Mem_Alloc(sizeof(STR_Header));

	if (str_ctx == NULL || sector_header == NULL)
	{
		if (str_ctx != NULL)
			Mem_Free(str_ctx);
		if (sector_header != NULL)
			Mem_Free(sector_header);
		str_ctx = NULL;
		sector_header = NULL;
		return;
	}

	STR_InitStream();
	STR_SetMovieCdMode();
	/*
	 * mkpsxiso may expose a raw STR's original 2336-byte size or its ISO
	 * 2048-byte logical size, depending on tool version.
	 */
	if ((file->size % STR_RAW_SECTOR_SIZE) == 0)
		str_sectors_remaining = file->size / STR_RAW_SECTOR_SIZE;
	else
		str_sectors_remaining = (file->size + IO_SECT_SIZE - 1) / IO_SECT_SIZE;
	str_file_end_sector = CdPosToInt(&file->pos) + str_sectors_remaining;
	str_file_exhausted = false;

	Timer_Tick();
	CdControl(CdlReadS, (u8*)&file->pos, 0);

	/*
	 * Prime and display frame zero instead of discarding it. Once uploaded it
	 * remains current in VRAM while the callback preloads frame one.
	 */
	StreamBuffer *first_frame = get_next_frame();
	if (first_frame == NULL)
		STR_StopStream();
	else
		STR_RenderFrame(first_frame);

	u8 video_clock = 0;
	u8 movie_ms_remainder = 0;

	while (stage.movie_is_playing)
	{
		Timer_Tick();
		/* STR gameplay uses a stable 60 Hz simulation step. */
		timer_dt = FIXED_DIV(FIXED_UNIT, STR_GAME_FPS);
		Pad_Update();

		/* Advance on the fixed gameplay clock so pause time is excluded from
		 * movie position and step-synchronized movie behavior. */
		if (!stage.movie_paused)
		{
			movie_ms_remainder += 40;
			stage.movie_pos += 16 + (movie_ms_remainder >= 60);
			if (movie_ms_remainder >= 60)
				movie_ms_remainder -= 60;
		}

		if (str_movie_audio_rearm_frames != 0)
		{
			STR_RearmMovieAudio();
			str_movie_audio_rearm_frames--;
		}
		Str_Update(&video_clock);
	}

	if (str_ctx != NULL)
	{
		STR_StopStream();
		Mem_Free(str_ctx);
		Mem_Free(sector_header);
		str_ctx = NULL;
		sector_header = NULL;
	}

	Gfx_EnableClear();

	/* IO/XA configure the drive when they take ownership. */
	str_audio_mode_valid = false;
}

void Str_Play(const char *filedir)
{
	CdlFILE file;

	IO_FindFile(&file, filedir);
	CdSync(0, 0);


	str_outside_gameplay = true;
	Str_PlayFile(&file);
	str_outside_gameplay = false;
}

void Str_CanPlayDef(void)
{
	for (u8 i = 0; i < COUNT_OF(str_def); i++)
	{
		if (str_def[i].id == stage.stage_id && stage.story)
			Str_Play(str_def[i].name);
	}
}
