/*
  Enhanced Stage Drawing Functions for All Stars PSX
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "stage.h"
#include "debug.h"
#include "gfx.h"
#include "mutil.h"

//Enhanced drawing function with all features
void Stage_DrawTexAll(Gfx_Tex *tex, const RECT *src, const RECT_FIXED *dst, fixed_t zoom, fixed_t rotation, u8 angle, u8 r, u8 g, u8 b, u8 alpha, boolean flip_x, boolean flip_y, boolean clipped)
{
	if (tex == NULL || src == NULL || dst == NULL)
		return;

	// Set clipping if requested
	if (clipped)
		Stage_SetDrawClipped(true);

	fixed_t xz = dst->x;
	fixed_t yz = dst->y;
	fixed_t wz = dst->w;
	fixed_t hz = dst->h;

	// Position rotation (hudangle) - rotates position around screen center
	u8 rotationAngle = rotation / FIXED_UNIT;
	fixed_t cos_r = FIXED_DEC(MUtil_Cos(rotationAngle), 256);
	fixed_t sin_r = FIXED_DEC(MUtil_Sin(rotationAngle), 256);
	fixed_t rotatedX = FIXED_MUL(xz, cos_r) - FIXED_MUL(yz, sin_r);
	fixed_t rotatedY = FIXED_MUL(xz, sin_r) + FIXED_MUL(yz, cos_r);

	// Compute screen-space rect from rotated position + zoom (all in fixed_t)
	fixed_t l = (SCREEN_WIDTH2  * FIXED_UNIT) + FIXED_MUL(rotatedX, zoom) + FIXED_DEC(1,2);
	fixed_t t = (SCREEN_HEIGHT2 * FIXED_UNIT) + FIXED_MUL(rotatedY, zoom) + FIXED_DEC(1,2);
	fixed_t rotx = l + FIXED_MUL(wz, zoom);
	fixed_t bot = t + FIXED_MUL(hz, zoom);

	// Sprite rotation (angle) - rotate corners around center of screen-space rect
	fixed_t cx = (l + rotx) / 2;
	fixed_t cy = (t + bot) / 2;
	fixed_t hw = (rotx - l) / 2;
	fixed_t hh = (bot - t) / 2;

	u8 spriteAngle = angle + rotationAngle;
	fixed_t cos_a = FIXED_DEC(MUtil_Cos(spriteAngle), 256);
	fixed_t sin_a = FIXED_DEC(MUtil_Sin(spriteAngle), 256);

	// Compute rotated corners (still in fixed_t), convert to screen POINTs at end
	POINT s0 = {
		(int)((cx + FIXED_MUL(-hw, cos_a) - FIXED_MUL(-hh, sin_a)) / FIXED_UNIT),
		(int)((cy + FIXED_MUL(-hw, sin_a) + FIXED_MUL(-hh, cos_a)) / FIXED_UNIT)
	};
	POINT s1 = {
		(int)((cx + FIXED_MUL( hw, cos_a) - FIXED_MUL(-hh, sin_a)) / FIXED_UNIT),
		(int)((cy + FIXED_MUL( hw, sin_a) + FIXED_MUL(-hh, cos_a)) / FIXED_UNIT)
	};
	POINT s2 = {
		(int)((cx + FIXED_MUL(-hw, cos_a) - FIXED_MUL( hh, sin_a)) / FIXED_UNIT),
		(int)((cy + FIXED_MUL(-hw, sin_a) + FIXED_MUL( hh, cos_a)) / FIXED_UNIT)
	};
	POINT s3 = {
		(int)((cx + FIXED_MUL( hw, cos_a) - FIXED_MUL( hh, sin_a)) / FIXED_UNIT),
		(int)((cy + FIXED_MUL( hw, sin_a) + FIXED_MUL( hh, cos_a)) / FIXED_UNIT)
	};

	// Handle flipping by swapping corners
	if (flip_x)
	{
		POINT temp = s0; s0 = s1; s1 = temp;
		temp = s2; s2 = s3; s3 = temp;
	}

	if (flip_y)
	{
		POINT temp = s0; s0 = s2; s2 = temp;
		temp = s1; s1 = s3; s3 = temp;
	}

	// Draw with color and alpha (use Gfx directly - corners are already screen-space)
	if (alpha == 255)
		Gfx_DrawTexArbCol(tex, src, &s0, &s1, &s2, &s3, r, g, b);
	else
		Gfx_DrawTexArbCol(tex, src, &s0, &s1, &s2, &s3,
			(r * alpha) >> 8, (g * alpha) >> 8, (b * alpha) >> 8);

	// Restore clipping state
	if (clipped)
		Stage_SetDrawClipped(false);
}

//Enhanced blending drawing function
void Stage_DrawBlendTexAll(Gfx_Tex *tex, const RECT *src, const RECT_FIXED *dst, fixed_t zoom, fixed_t rotation, u8 angle, u8 r, u8 g, u8 b, u8 alpha, boolean flip_x, boolean flip_y, boolean clipped, u8 mode)
{
	if (tex == NULL || src == NULL || dst == NULL)
		return;

	// Set clipping if requested
	if (clipped)
		Stage_SetDrawClipped(true);

	fixed_t xz = dst->x;
	fixed_t yz = dst->y;
	fixed_t wz = dst->w;
	fixed_t hz = dst->h;

	// Position rotation (hudangle) - rotates position around screen center
	u8 rotationAngle = rotation / FIXED_UNIT;
	fixed_t cos_r = FIXED_DEC(MUtil_Cos(rotationAngle), 256);
	fixed_t sin_r = FIXED_DEC(MUtil_Sin(rotationAngle), 256);
	fixed_t rotatedX = FIXED_MUL(xz, cos_r) - FIXED_MUL(yz, sin_r);
	fixed_t rotatedY = FIXED_MUL(xz, sin_r) + FIXED_MUL(yz, cos_r);

	// Compute screen-space rect from rotated position + zoom (all in fixed_t)
	fixed_t l = (SCREEN_WIDTH2  * FIXED_UNIT) + FIXED_MUL(rotatedX, zoom) + FIXED_DEC(1,2);
	fixed_t t = (SCREEN_HEIGHT2 * FIXED_UNIT) + FIXED_MUL(rotatedY, zoom) + FIXED_DEC(1,2);
	fixed_t rotx = l + FIXED_MUL(wz, zoom);
	fixed_t bot = t + FIXED_MUL(hz, zoom);

	// Sprite rotation (angle) - rotate corners around center of screen-space rect
	fixed_t cx = (l + rotx) / 2;
	fixed_t cy = (t + bot) / 2;
	fixed_t hw = (rotx - l) / 2;
	fixed_t hh = (bot - t) / 2;

	u8 spriteAngle = angle + rotationAngle;
	fixed_t cos_a = FIXED_DEC(MUtil_Cos(spriteAngle), 256);
	fixed_t sin_a = FIXED_DEC(MUtil_Sin(spriteAngle), 256);

	// Compute rotated corners (still in fixed_t), convert to screen POINTs at end
	POINT s0 = {
		(int)((cx + FIXED_MUL(-hw, cos_a) - FIXED_MUL(-hh, sin_a)) / FIXED_UNIT),
		(int)((cy + FIXED_MUL(-hw, sin_a) + FIXED_MUL(-hh, cos_a)) / FIXED_UNIT)
	};
	POINT s1 = {
		(int)((cx + FIXED_MUL( hw, cos_a) - FIXED_MUL(-hh, sin_a)) / FIXED_UNIT),
		(int)((cy + FIXED_MUL( hw, sin_a) + FIXED_MUL(-hh, cos_a)) / FIXED_UNIT)
	};
	POINT s2 = {
		(int)((cx + FIXED_MUL(-hw, cos_a) - FIXED_MUL( hh, sin_a)) / FIXED_UNIT),
		(int)((cy + FIXED_MUL(-hw, sin_a) + FIXED_MUL( hh, cos_a)) / FIXED_UNIT)
	};
	POINT s3 = {
		(int)((cx + FIXED_MUL( hw, cos_a) - FIXED_MUL( hh, sin_a)) / FIXED_UNIT),
		(int)((cy + FIXED_MUL( hw, sin_a) + FIXED_MUL( hh, cos_a)) / FIXED_UNIT)
	};

	// Handle flipping by swapping corners
	if (flip_x)
	{
		POINT temp = s0; s0 = s1; s1 = temp;
		temp = s2; s2 = s3; s3 = temp;
	}

	if (flip_y)
	{
		POINT temp = s0; s0 = s2; s2 = temp;
		temp = s1; s1 = s3; s3 = temp;
	}

	// Draw with blending (use Gfx directly - corners are already screen-space)
	Gfx_BlendTexArbCol(tex, src, &s0, &s1, &s2, &s3,
		(r * alpha) >> 8, (g * alpha) >> 8, (b * alpha) >> 8, mode);

	// Restore clipping state
	if (clipped)
		Stage_SetDrawClipped(false);
}
