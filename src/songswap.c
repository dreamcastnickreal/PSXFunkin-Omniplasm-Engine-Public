/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "songswap.h"

#include "stage.h"
#include "audio.h"
#include "str.h"
#include "timer.h"
#include "disc_swap_disc1.h"
#include "disc_swap_disc2.h"
#include "disc_swap_disc3.h"

#define SONGSWAP_FULL_RELOAD (STAGE_LOAD_STAGE | STAGE_LOAD_PLAYER | STAGE_LOAD_PLAYER2 | STAGE_LOAD_OPPONENT | STAGE_LOAD_OPPONENT2 | STAGE_LOAD_GIRLFRIEND)

static boolean SongSwap_JustStep(void)
{
	return (stage.flag & STAGE_FLAG_JUST_STEP) != 0;
}

void SongSwap_PlayXAAtCentiseconds(s32 centiseconds)
{
	if (currentDisc == 1) {
		Audio_PlayXA_TrackDisc1Centiseconds(stage.stage_def->music_track, 0x40, stage.stage_def->music_channel, false, centiseconds);
	} else if (currentDisc == 2) {
		Audio_PlayXA_TrackDisc2Centiseconds(stage.stage_def->music_track, 0x40, stage.stage_def->music_channel, false, centiseconds);
	} else if (currentDisc == 3) {
		Audio_PlayXA_TrackDisc3Centiseconds(stage.stage_def->music_track, 0x40, stage.stage_def->music_channel, false, centiseconds);
	}
}

static void SongSwap_SyncChartToXA(void)
{
	s32 audio_ms = (s32)Audio_TellXA_Milli() - stage.offset;
	if (audio_ms < 0)
		audio_ms = 0;

	fixed_t song_time = ((fixed_t)audio_ms << FIXED_SHIFT) / 1000;
	fixed_t note_scroll =
		((fixed_t)stage.step_base << FIXED_SHIFT) +
		FIXED_MUL(song_time - stage.time_base, stage.step_crochet);

	stage.song_time = song_time;
	stage.interp_ms = song_time;
	stage.interp_time = 0;
	stage.note_scroll = note_scroll;

	stage.song_step = note_scroll >> FIXED_SHIFT;
	if (note_scroll < 0)
		stage.song_step -= 11;
	stage.song_step /= 12;
	stage.song_beat = stage.song_step / 4;

	/* Do not replay the cutscene trigger after correcting the timeline. */
	stage.flag &= ~STAGE_FLAG_JUST_STEP;
}

static void SongSwap_PlayMovie(CdlFILE *file, s32 music_start_centiseconds)
{
	Str_PlayFileEx(file, true, 30, 1);
	SongSwap_PlayXAAtCentiseconds(music_start_centiseconds);
	Audio_WaitPlayXA();
	Audio_ResumeXA();
	SongSwap_SyncChartToXA();
	Timer_Reset();
}

static void SongSwap_LoadHud(const char *grid_path)
{
	Gfx_LoadTex(&stage.tex_hud0, IO_Read("\\STAGE\\HUD0.TIM;1"), GFX_LOADTEX_FREE);
	Gfx_LoadTex(&stage.tex_hud1, IO_Read(grid_path), GFX_LOADTEX_FREE);
	Gfx_LoadTex(&stage.tex_hude, IO_Read("\\STAGE\\HUDE.TIM;1"), GFX_LOADTEX_FREE);
}

static void SongSwap_RottenSmoothie(void)
{
	if (!SongSwap_JustStep())
		return;

	switch (stage.song_step)
	{
		case 1312:
			if (!stage.movie_is_playing)
			{
				// grace is now video-only 30 fps - use game XA
				Str_PlayFileEx(&stage.str_grace_lba, false, 30, 1);
				SongSwap_PlayXAAtCentiseconds(16400);
				Audio_WaitPlayXA();
				Audio_ResumeXA();
				SongSwap_SyncChartToXA();
				Timer_Reset();
			}
			break;

		case 1824:
			Str_StopMovie();
			break;
	}
}

static void SongSwap_AllStars(void)
{
	if (!SongSwap_JustStep())
		return;

	switch (stage.song_step)
	{
		case 0:
			if (!stage.movie_is_playing && stage.has_asintro)
			{
				// asintro is GIF-derived native 24 fps video-only
				Str_PlayFileEx(&stage.str_asintro_lba, false, 30, 1);
				SongSwap_PlayXAAtCentiseconds(310);
				Audio_WaitPlayXA();
				Audio_ResumeXA();
				SongSwap_SyncChartToXA();
				Timer_Reset();
			}
			break;
	}
}

void SongSwap_Tick(void)
{
	switch (stage.stage_id)
	{
		case StageId_4_8:
			SongSwap_AllStars();
			break;
		case StageId_5_2:
			SongSwap_RottenSmoothie();
			break;

		default:
			break;
	}
}
