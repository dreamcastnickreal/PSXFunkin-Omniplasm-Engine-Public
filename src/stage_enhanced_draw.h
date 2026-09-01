/*
  Enhanced Stage Drawing Functions Header for All Stars PSX
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef PSXF_GUARD_STAGE_ENHANCED_DRAW_H
#define PSXF_GUARD_STAGE_ENHANCED_DRAW_H

#include "psx.h"
#include "gfx.h"
#include "fixed.h"

// Enhanced drawing functions for All Stars with full control
void Stage_DrawTexAll(Gfx_Tex *tex, const RECT *src, const RECT_FIXED *dst, fixed_t zoom, fixed_t rotation, u8 angle, u8 r, u8 g, u8 b, u8 alpha, boolean flip_x, boolean flip_y, boolean clipped);
void Stage_DrawBlendTexAll(Gfx_Tex *tex, const RECT *src, const RECT_FIXED *dst, fixed_t zoom, fixed_t rotation, u8 angle, u8 r, u8 g, u8 b, u8 alpha, boolean flip_x, boolean flip_y, boolean clipped, u8 mode);

#endif