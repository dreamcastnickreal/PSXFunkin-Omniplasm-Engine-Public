/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef PSXF_GUARD_STRPLAY_H
#define PSXF_GUARD_STRPLAY_H

#include "psx.h"
#include <libpress.h>

void Str_Init(void);
void Str_PlayFile(CdlFILE* file);
void Str_PlayFileEx(CdlFILE* file, boolean keep_game_xa, int fps_num, int fps_den);
void Str_Play(const char *filedir);
void Str_CanPlayDef(void);
void Str_StopMovie(void);
void Str_SetPaused(boolean paused);
void Str_SwitchToAltMiss(boolean to_miss, u32 pos);
void Str_StopStreamAlt(void);

extern boolean movie_is_playing;
#endif
