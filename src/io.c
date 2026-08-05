/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "io.h"

#include "mem.h"
#include "audio.h"
#include "main.h"
#include "stage.h"

//IO functions
static boolean io_xa_resume_pending = false;
static s32 io_xa_resume_seconds = 0;
static u8 io_asset_batch_depth = 0;

#define IO_FILE_CACHE_COUNT 64
#define IO_FILE_CACHE_PATH  64
typedef struct
{
	boolean valid;
	s8 disc;
	char path[IO_FILE_CACHE_PATH];
	CdlFILE file;
} IO_FileCacheEntry;

static IO_FileCacheEntry io_file_cache[IO_FILE_CACHE_COUNT];
static u8 io_file_cache_next = 0;
extern int currentDisc;

static boolean IO_ShouldPreserveSongXA(void)
{
	return gameloop == GameLoop_Stage &&
		stage.state == StageState_Play &&
		stage.chart_data != NULL &&
		stage.note_scroll >= 0 &&
		!stage.movie_is_playing &&
		!stage.paused &&
		Audio_PlayingXA();
}

static void IO_TryResumeSongXA(void)
{
	if (io_asset_batch_depth != 0)
		return;

	if (!io_xa_resume_pending)
		return;

	io_xa_resume_pending = false;

	if (gameloop != GameLoop_Stage ||
		stage.state != StageState_Play ||
		stage.chart_data == NULL ||
		stage.note_scroll < 0 ||
		stage.movie_is_playing ||
		stage.paused)
		return;

	if (stage.music_disc_active == 1)
		Audio_PlayXA_TrackDisc1((XA_Track1)stage.music_track_active, 0x40, stage.music_channel_active, false, io_xa_resume_seconds);
	else if (stage.music_disc_active == 2)
		Audio_PlayXA_TrackDisc2((XA_Track2)stage.music_track_active, 0x40, stage.music_channel_active, false, io_xa_resume_seconds);
	else if (stage.music_disc_active == 3)
		Audio_PlayXA_TrackDisc3((XA_Track3)stage.music_track_active, 0x40, stage.music_channel_active, false, io_xa_resume_seconds);
	else
		return;

	Audio_ChannelXA(stage.music_channel_active);

	{
		fixed_t audio_time = (fixed_t)Audio_TellXA_Milli() - stage.offset;
		if (audio_time < 0)
			audio_time = 0;
		stage.interp_ms = (fixed_t)(((s64)audio_time * FIXED_UNIT) / 1000);
		stage.interp_time = 0;
		stage.song_time = (io_xa_resume_seconds << FIXED_SHIFT);
	}

	stage.swap_grace_frames = 30;
}
void IO_Init(void)
{
	//Initialize CD IO
	CdInit();
	memset(io_file_cache, 0, sizeof(io_file_cache));
	io_file_cache_next = 0;
	io_asset_batch_depth = 0;
}

void IO_BeginAssetBatch(void)
{
	io_asset_batch_depth++;
}

void IO_EndAssetBatch(boolean resume_xa)
{
	if (io_asset_batch_depth == 0)
		return;

	io_asset_batch_depth--;
	if (io_asset_batch_depth != 0)
		return;

	if (resume_xa)
		IO_TryResumeSongXA();
	else
		io_xa_resume_pending = false;
}

void IO_Quit(void)
{
	
}

boolean IO_ExistFile(const char* path)
{
	CdlFILE file;

	/* Optional/conditional assets participate in the same per-disc cache as
	 * normal reads, so every song benefits even when it probes before loading. */
	for (u8 i = 0; i < IO_FILE_CACHE_COUNT; i++)
	{
		IO_FileCacheEntry *entry = &io_file_cache[i];
		if (entry->valid && entry->disc == currentDisc && strcmp(entry->path, path) == 0)
			return true;
	}

	if (!CdSearchFile(&file, (char*)path))
		return false;

	{
		IO_FileCacheEntry *entry = &io_file_cache[io_file_cache_next++ % IO_FILE_CACHE_COUNT];
		entry->valid = true;
		entry->disc = currentDisc;
		strncpy(entry->path, path, IO_FILE_CACHE_PATH - 1);
		entry->path[IO_FILE_CACHE_PATH - 1] = '\0';
		entry->file = file;
	}
	return true;
}

void IO_FindFile(CdlFILE *file, const char *path)
{
	if (!io_xa_resume_pending && IO_ShouldPreserveSongXA())
	{
		io_xa_resume_seconds = (s32)(stage.song_time >> FIXED_SHIFT);
		if (io_xa_resume_seconds < 0)
			io_xa_resume_seconds = 0;
		io_xa_resume_pending = true;
	}

	// File searches and reads share the drive with XA.
	Audio_StopXA();

	for (u8 i = 0; i < IO_FILE_CACHE_COUNT; i++)
	{
		IO_FileCacheEntry *entry = &io_file_cache[i];
		if (entry->valid && entry->disc == currentDisc && strcmp(entry->path, path) == 0)
		{
			*file = entry->file;
			goto prepare_read;
		}
	}

	printf("[IO_FindFile] Searching for %s\n", path);
	if (!CdSearchFile(file, (char*)path))
	{
		sprintf(error_msg, "[IO_FindFile] %s not found", path);
		ErrorLock();
		return;
	}

	{
		IO_FileCacheEntry *entry = &io_file_cache[io_file_cache_next++ % IO_FILE_CACHE_COUNT];
		entry->valid = true;
		entry->disc = currentDisc;
		strncpy(entry->path, path, IO_FILE_CACHE_PATH - 1);
		entry->path[IO_FILE_CACHE_PATH - 1] = '\0';
		entry->file = *file;
	}

prepare_read:
	return;
}

void IO_SeekFile(CdlFILE *file)
{
	//Seek to file position
	CdControlB(CdlSeekL, (u8*)&file->pos, NULL);
}

IO_Data IO_ReadFile(CdlFILE *file)
{
	//Read file then sync
	IO_Data buffer = IO_AsyncReadFile(file);
	CdReadSync(0, NULL);
	IO_TryResumeSongXA();
	return buffer;
}

IO_Data IO_AsyncReadFile(CdlFILE *file)
{
	
	//Get number of sectors for the file
	size_t sects = (file->size + IO_SECT_SIZE - 1) / IO_SECT_SIZE;
	
	//Allocate a buffer for the file
	size_t size;
	IO_Data buffer = (IO_Data)Mem_Alloc(size = (IO_SECT_SIZE * sects));
	if (buffer == NULL)
	{
		sprintf(error_msg, "[IO_AsyncReadFile] Malloc (size %X) fail", size);
		ErrorLock();
		return NULL;
	}
	
	//Read file
	CdControl(CdlSetloc, (u8*)&file->pos, NULL);
	CdRead(sects, buffer, CdlModeSpeed);
	return buffer;
}

IO_Data IO_Read(const char *path)
{
	printf("[IO_Read] Reading file %s\n", path);
	
	//Search for file
	CdlFILE file;
	IO_FindFile(&file, path);
	
	//Read file then sync
	IO_Data buffer = IO_AsyncReadFile(&file);
	CdReadSync(0, NULL);
	IO_TryResumeSongXA();
	return buffer;
}

IO_Data IO_AsyncRead(const char *path)
{
	printf("[IO_ReadAsync] Reading file %s\n", path);
	
	//Search for file
	CdlFILE file;
	IO_FindFile(&file, path);
	
	//Read file
	return IO_AsyncReadFile(&file);
}

boolean IO_IsSeeking(void)
{
	CdControl(CdlNop, NULL, NULL);
	return (CdStatus() & (CdlStatSeek)) != 0;
}

boolean IO_IsReading(void)
{
	CdControl(CdlNop, NULL, NULL);
	return (CdStatus() & (CdlStatSeek | CdlStatRead)) != 0;
}
