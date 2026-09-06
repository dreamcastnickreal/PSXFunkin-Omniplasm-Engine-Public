/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "character.h"

#include "mem.h"
#include "main.h"
#include "stage.h"

//Character functions
void Character_Free(Character *this)
{
	//Check if NULL
	if (this == NULL)
		return;

	//Free character
	this->free(this);
	if (this->ghosts_storage != NULL)
		Mem_Free(this->ghosts_storage);
	Mem_Free(this);
}

void Character_Init(Character *this, fixed_t x, fixed_t y)
{
	//Perform common character initialization
	this->x = x;
	this->y = y;
	this->opacity = 255;
	this->ghosts_storage = NULL;

	this->set_anim(this, CharAnim_Idle);
	this->pad_held = 0;

	this->sing_end = 0;

	Character_GhostConfigure(this, false, 0, 0, CHARACTER_GHOST_DEFAULT_COUNT);
}

void Character_GhostConfigure(Character *this, boolean enabled, s16 vram_x, s16 vram_y, u8 count)
{
	u8 i;

	if (!enabled)
	{
		if (this->ghosts_storage != NULL)
		{
			Mem_Free(this->ghosts_storage);
			this->ghosts_storage = NULL;
		}
		return;
	}

	if (this->ghosts_storage == NULL)
	{
		this->ghosts_storage = Mem_Alloc(sizeof(CharacterGhosts));
		if (this->ghosts_storage == NULL)
		{
			sprintf(error_msg, "[Character_GhostConfigure] Failed to allocate ghost state");
			ErrorLock();
			return;
		}
	}

	if (count == 0)
		count = CHARACTER_GHOST_DEFAULT_COUNT;
	if (count > CHARACTER_GHOST_MAX_COUNT)
		count = CHARACTER_GHOST_MAX_COUNT;

	this->ghosts.enabled = enabled;
	this->ghosts.active = enabled;
	this->ghosts.continuous = false;
	this->ghosts.secondary_is_main = false;
	this->ghosts.capture_requested = false;
	this->ghosts.color_override = false;
	this->ghosts.color_r = this->ghosts.color_g = this->ghosts.color_b = 0x80;
	this->ghosts.count = count;
	this->ghosts.needed_frames = 0;
	this->ghosts.used = 0;
	this->ghosts.head = 0;
	this->ghosts.vram_x = vram_x;
	this->ghosts.vram_y = vram_y;
	this->ghosts.slot_words = CHARACTER_GHOST_SLOT_WORDS;
	this->ghosts.slot_height = CHARACTER_GHOST_SLOT_HEIGHT;
	this->ghosts.pack_x = this->ghosts.pack_y = this->ghosts.pack_row_height = 0;
	this->ghosts.request_time = 0;
	this->ghosts.requests_at_time = 0;
	this->ghosts.requested_anim = CharAnim_Idle;
	this->ghosts.opposing_count = 0;
	for (i = 0; i < CHARACTER_GHOST_MAX_OPPOSING; i++)
	{
		this->ghosts.opposing_anim[i] = CharAnim_Idle;
		this->ghosts.opposing_pending[i] = false;
	}
	for (i = 0; i < CHARACTER_GHOST_MAX_COUNT; i++)
		this->ghosts.frames[i].valid = false;
}

void Character_GhostConfigureSlots(Character *this, u16 slot_words, u16 slot_height)
{
	if (this->ghosts_storage == NULL)
		return;
	if (slot_words != 0)
		this->ghosts.slot_words = slot_words;
	if (slot_height != 0)
		this->ghosts.slot_height = slot_height;
	this->ghosts.used = this->ghosts.head = 0;
	this->ghosts.pack_x = this->ghosts.pack_y = this->ghosts.pack_row_height = 0;
}

void Character_GhostSetActive(Character *this, boolean active)
{
	if (this->ghosts_storage == NULL)
		return;
	if (!this->ghosts.enabled)
		active = false;
	if (this->ghosts.active == active)
		return;

	this->ghosts.active = active;
	this->ghosts.capture_requested = false;
	if (!active)
	{
		this->ghosts.used = this->ghosts.head = 0;
		this->ghosts.needed_frames = 0;
		this->ghosts.pack_x = this->ghosts.pack_y = this->ghosts.pack_row_height = 0;
		this->ghosts.opposing_count = 0;
	}
}

void Character_GhostSetContinuous(Character *this, boolean continuous)
{
	if (this->ghosts_storage == NULL)
		return;
	this->ghosts.continuous = continuous;
}

void Character_GhostSetSecondaryMain(Character *this, boolean secondary_is_main)
{
	if (this->ghosts_storage == NULL)
		return;
	this->ghosts.secondary_is_main = secondary_is_main;
	if (this->ghosts.enabled && secondary_is_main)
		this->ghosts.capture_requested = true;
}

void Character_GhostAnimationRequest(Character *this, u8 anim)
{
	if (this->ghosts_storage == NULL)
		return;
	if (!this->ghosts.enabled)
		return;

	if (this->ghosts.requests_at_time != 0 && this->ghosts.request_time == stage.note_scroll)
	{
		u8 opposing = this->ghosts.requests_at_time - 1;
		if (opposing < CHARACTER_GHOST_MAX_OPPOSING)
		{
			this->ghosts.opposing_anim[opposing] = this->ghosts.requested_anim;
			this->ghosts.opposing_pending[opposing] = true;
			this->ghosts.opposing_count = opposing + 1;
			this->ghosts.needed_frames = this->ghosts.opposing_count;
			if (this->ghosts.needed_frames > this->ghosts.count)
				this->ghosts.needed_frames = this->ghosts.count;
		}
		if (this->ghosts.requests_at_time < CHARACTER_GHOST_MAX_OPPOSING + 1)
			this->ghosts.requests_at_time++;
		this->ghosts.requested_anim = anim;
	}
	else
	{
		this->ghosts.request_time = stage.note_scroll;
		this->ghosts.requests_at_time = 1;
		this->ghosts.requested_anim = anim;
		this->ghosts.opposing_count = 0;
		this->ghosts.needed_frames = 0;
		this->ghosts.used = this->ghosts.head = 0;
		this->ghosts.pack_x = this->ghosts.pack_y = this->ghosts.pack_row_height = 0;
	}
}

void Character_GhostSetColor(Character *this, u8 r, u8 g, u8 b)
{
	if (this->ghosts_storage == NULL)
		return;
	this->ghosts.color_override = true;
	this->ghosts.color_r = r;
	this->ghosts.color_g = g;
	this->ghosts.color_b = b;
}

u8 Character_GhostOpposingCount(Character *this)
{
	if (this->ghosts_storage == NULL)
		return 0;
	if (!this->ghosts.enabled)
		return 0;
	return this->ghosts.opposing_count;
}

boolean Character_GhostAnimateOpposing(Character *this, u8 index, const Animation *anims,
	void *user, void (*set_frame)(void*, u8))
{
	if (this->ghosts_storage == NULL)
		return false;
	if (!this->ghosts.enabled || index >= this->ghosts.opposing_count)
		return false;

	if (this->ghosts.opposing_pending[index])
	{
		if (stage.paused)
			return false;
		Animatable_Init(&this->ghosts.opposing_animatable[index], anims);
		Animatable_SetAnim(&this->ghosts.opposing_animatable[index], this->ghosts.opposing_anim[index]);
		this->ghosts.opposing_pending[index] = false;
	}
	if (stage.paused)
		return false;
	Animatable_Animate(&this->ghosts.opposing_animatable[index], user, set_frame);
	if (Animatable_Ended(&this->ghosts.opposing_animatable[index]) ||
	    (this->ghosts.opposing_animatable[index].frame_changed &&
	     (this->ghosts.opposing_animatable[index].anim_p[0] == ASCR_REPEAT ||
	      this->ghosts.opposing_animatable[index].anim_p[0] == ASCR_CHGANI ||
	      this->ghosts.opposing_animatable[index].anim_p[0] == ASCR_BACK)))
	{
		u8 i;
		for (i = 0; i < this->ghosts.used; i++)
		{
			CharacterGhostFrame *ghost = &this->ghosts.frames[i];
			if (ghost->valid && !ghost->fading && ghost->opposing_index == index)
			{
				ghost->x = this->x;
				ghost->y = this->y;
				ghost->distance = 0;
				ghost->fading = true;
			}
		}
	}
	return this->ghosts.opposing_animatable[index].frame_changed;
}

static void Character_GhostDrawStored(Character *this)
{
	u8 i;
	fixed_t old_x = this->x;
	fixed_t old_y = this->y;

	for (i = 0; i < this->ghosts.used; i++)
	{
		CharacterGhostFrame *ghost = &this->ghosts.frames[i];
		u8 opacity;
		RECT src;
		RECT_FIXED dst;
		if (!ghost->valid)
			continue;

		opacity = 255 - ((u32)ghost->distance * 255 / FIXED_DEC(10,1));
		if (ghost->fading)
		{
			this->x = ghost->x + FIXED_MUL(ghost->drift_x, ghost->distance);
			this->y = ghost->y + FIXED_MUL(ghost->drift_y, ghost->distance);
		}
		else
		{
			/* Until its last animation frame, the ghost remains attached to
			 * the character's original draw position. */
			this->x = old_x;
			this->y = old_y;
		}
		src.x = ghost->frame.src[0];
		src.y = ghost->frame.src[1];
		src.w = ghost->frame.src[2];
		src.h = ghost->frame.src[3];
		dst.x = this->x - stage.camera.x - FIXED_MUL(FIXED_DEC(ghost->frame.off[0],1),this->size);
		dst.y = this->y - stage.camera.y - FIXED_MUL(FIXED_DEC(ghost->frame.off[1],1),this->size);
		dst.w = FIXED_MUL(src.w << FIXED_SHIFT, this->size);
		dst.h = FIXED_MUL(src.h << FIXED_SHIFT, this->size);
		/* Additive blending lets opacity zero contribute nothing, producing a
		 * real fade instead of darkening the background like mode 0. */
		Stage_BlendTexColOpacity(&ghost->tex, &src, &dst,
			stage.camera.bzoom, stage.camera.angle,
			ghost->r, ghost->g, ghost->b, 0, opacity);

		if (!stage.paused && ghost->fading)
		{
			ghost->distance += FIXED_DEC(1,1);
			if (ghost->distance >= FIXED_DEC(10,1))
				ghost->valid = false;
		}
	}
	this->x = old_x;
	this->y = old_y;
}

static void Character_GhostCapture(Character *this, Gfx_Tex *tex,
	const CharFrame *cframe, u8 anim, u8 opposing_index)
{
	CharacterGhostFrame *ghost;
	RECT region;
	s16 slot_x, slot_y, clut_x;
	u16 frame_words;
	u8 i;

	if (!this->ghosts.enabled ||
	    (!this->ghosts.active && !this->ghosts.capture_requested) ||
	    (!this->ghosts.capture_requested && !this->ghosts.continuous && !this->ghosts.secondary_is_main))
		return;

	region.x = cframe->src[0];
	region.y = cframe->src[1];
	region.w = cframe->src[2];
	region.h = cframe->src[3];
	frame_words = ((region.x & ((1 << tex->pxshift) - 1)) + region.w +
	               (1 << tex->pxshift) - 1) >> tex->pxshift;

	/* Once a direction owns an atlas rectangle, update that rectangle in
	 * place as its animation advances. Its drift and fade are intentionally
	 * left untouched. */
	for (i = 0; i < this->ghosts.used; i++)
	{
		ghost = &this->ghosts.frames[i];
		if (ghost->opposing_index != opposing_index)
			continue;
		if (!ghost->valid || frame_words > ghost->atlas_words ||
		    region.h > ghost->atlas_height)
		{
			this->ghosts.capture_requested = false;
			return;
		}
		slot_x = ghost->atlas_x;
		slot_y = ghost->atlas_y;
		clut_x = this->ghosts.vram_x + i * 16;
		if (Gfx_CopyTexRegion(&ghost->tex, tex, &region, slot_x, slot_y,
		    clut_x, this->ghosts.vram_y + this->ghosts.slot_height - 1))
		{
			ghost->frame = *cframe;
			ghost->frame.src[0] = ((slot_x & 0x3F) << tex->pxshift) +
			                      (region.x & ((1 << tex->pxshift) - 1));
			ghost->frame.src[1] = slot_y & 0xFF;
		}
		this->ghosts.capture_requested = false;
		return;
	}

	if (this->ghosts.needed_frames == 0 ||
	    this->ghosts.used >= this->ghosts.needed_frames)
	{
		this->ghosts.capture_requested = false;
		return;
	}

	if (this->ghosts.pack_x + frame_words > this->ghosts.slot_words)
	{
		this->ghosts.pack_x = 0;
		this->ghosts.pack_y += this->ghosts.pack_row_height;
		this->ghosts.pack_row_height = 0;
	}
	if (frame_words > this->ghosts.slot_words ||
	    this->ghosts.pack_y + region.h > this->ghosts.slot_height - 1)
	{
		this->ghosts.capture_requested = false;
		return;
	}

	ghost = &this->ghosts.frames[this->ghosts.used];
	slot_x = this->ghosts.vram_x + this->ghosts.pack_x;
	slot_y = this->ghosts.vram_y + this->ghosts.pack_y;
	clut_x = this->ghosts.vram_x + this->ghosts.used * 16;

	if (Gfx_CopyTexRegion(&ghost->tex, tex, &region, slot_x, slot_y,
	    clut_x, this->ghosts.vram_y + this->ghosts.slot_height - 1))
	{
		ghost->frame = *cframe;
		ghost->frame.src[0] = ((slot_x & 0x3F) << tex->pxshift) +
		                      (region.x & ((1 << tex->pxshift) - 1));
		ghost->frame.src[1] = slot_y & 0xFF;
		ghost->x = this->x;
		ghost->y = this->y;
		/* distance runs from 0 to 10, so a 0.5 directional drift produces
		 * exactly 5 units of travel over the fade. */
		ghost->drift_x = 0;
		ghost->drift_y = 0;
		ghost->distance = 0;
		ghost->r = ghost->g = ghost->b = 0x80;
		ghost->fading = false;
		ghost->atlas_x = slot_x;
		ghost->atlas_y = slot_y;
		ghost->atlas_words = frame_words;
		ghost->atlas_height = region.h;
		ghost->opposing_index = opposing_index;
		switch (anim)
		{
			case CharAnim_Left:
			case CharAnim_LeftAlt:
				ghost->drift_x = -FIXED_DEC(1,2);
				break;
			case CharAnim_Down:
			case CharAnim_DownAlt:
				ghost->drift_y = FIXED_DEC(1,2);
				break;
			case CharAnim_Up:
			case CharAnim_UpAlt:
				ghost->drift_y = -FIXED_DEC(1,2);
				break;
			case CharAnim_Right:
			case CharAnim_RightAlt:
				ghost->drift_x = FIXED_DEC(1,2);
				break;
			default:
				break;
		}
		ghost->valid = true;
		this->ghosts.used++;
		this->ghosts.head = this->ghosts.used;
		this->ghosts.pack_x += frame_words;
		if (region.h > this->ghosts.pack_row_height)
			this->ghosts.pack_row_height = region.h;
	}
	this->ghosts.capture_requested = false;
}

void Character_GhostCaptureNow(Character *this, u8 opposing_index,
	Gfx_Tex *tex, const CharFrame *cframe)
{
	u8 i;
	Animatable *animatable;
	if (this->ghosts_storage == NULL)
		return;
	if (!this->ghosts.enabled || opposing_index >= this->ghosts.opposing_count)
		return;
	this->ghosts.capture_requested = true;
	Character_GhostCapture(this, tex, cframe,
		this->ghosts.opposing_anim[opposing_index], opposing_index);

	/* A one-frame animation can reach its last frame before its ghost slot
	 * exists, so apply the transition again immediately after capture. */
	animatable = &this->ghosts.opposing_animatable[opposing_index];
	if (Animatable_Ended(animatable) ||
	    (animatable->frame_changed &&
	     (animatable->anim_p[0] == ASCR_REPEAT ||
	      animatable->anim_p[0] == ASCR_CHGANI ||
	      animatable->anim_p[0] == ASCR_BACK)))
	{
		for (i = 0; i < this->ghosts.used; i++)
		{
			CharacterGhostFrame *ghost = &this->ghosts.frames[i];
			if (ghost->valid && !ghost->fading &&
			    ghost->opposing_index == opposing_index)
			{
				ghost->x = this->x;
				ghost->y = this->y;
				ghost->distance = 0;
				ghost->fading = true;
			}
		}
	}
}

static void Character_GhostDrawAndCapture(Character *this, Gfx_Tex *tex,
	const CharFrame *cframe, u8 r, u8 g, u8 b)
{
	u8 i;
	if (this->ghosts_storage == NULL || !this->ghosts.enabled || !this->ghosts.active)
		return;

	for (i = 0; i < this->ghosts.used; i++)
	{
		this->ghosts.frames[i].r = this->ghosts.color_override ? this->ghosts.color_r : r;
		this->ghosts.frames[i].g = this->ghosts.color_override ? this->ghosts.color_g : g;
		this->ghosts.frames[i].b = this->ghosts.color_override ? this->ghosts.color_b : b;
	}
	Character_GhostDrawStored(this);
	Character_GhostCapture(this, tex, cframe, this->animatable.anim, 0xFF);
}

void Character_DrawParallax(Character *this, Gfx_Tex *tex, const CharFrame *cframe, fixed_t parallax)
{
	//Draw character
	fixed_t x = this->x - FIXED_MUL(stage.camera.x, parallax) - FIXED_MUL(FIXED_DEC(cframe->off[0],1),this->size);
	fixed_t y = this->y - FIXED_MUL(stage.camera.y, parallax) - FIXED_MUL(FIXED_DEC(cframe->off[1],1),this->size);

	RECT src = {cframe->src[0], cframe->src[1], cframe->src[2], cframe->src[3]};
	RECT_FIXED dst = {x, y, src.w << FIXED_SHIFT, src.h << FIXED_SHIFT};

	dst.w = FIXED_MUL(dst.w, this->size);
	dst.h = FIXED_MUL(dst.h, this->size);

	if (stage.bluemode)
		Stage_DrawTexColOpacity(tex, &src, &dst, stage.camera.bzoom, stage.camera.angle, 0, 0, 255, this->opacity);
	else
		Stage_DrawTexOpacity(tex, &src, &dst, stage.camera.bzoom, stage.camera.angle, this->opacity);
}

void Character_DrawParallaxFlipped(Character *this, Gfx_Tex *tex, const CharFrame *cframe, fixed_t parallax)
{
	//Draw character
	fixed_t x = this->x - FIXED_MUL(stage.camera.x, parallax) + FIXED_MUL(FIXED_DEC(cframe->off[0],1),this->size) - FIXED_MUL(FIXED_DEC(cframe->src[2],1),this->size);
	fixed_t y = this->y - FIXED_MUL(stage.camera.y, parallax) - FIXED_MUL(FIXED_DEC(cframe->off[1],1),this->size);

	RECT src = {cframe->src[0], cframe->src[1], cframe->src[2], cframe->src[3]};
	RECT_FIXED dst = {x, y, src.w << FIXED_SHIFT, src.h << FIXED_SHIFT};

	dst.w = FIXED_MUL(dst.w, this->size);
	dst.h = FIXED_MUL(dst.h, this->size);

	if (stage.camera.angle == 0)
	{
		RECT sdst = {
			SCREEN_WIDTH2 + ((FIXED_MUL(dst.x, stage.camera.bzoom) + FIXED_DEC(1,2)) >> FIXED_SHIFT),
			SCREEN_HEIGHT2 + ((FIXED_MUL(dst.y, stage.camera.bzoom) + FIXED_DEC(1,2)) >> FIXED_SHIFT),
			FIXED_MUL(dst.w, stage.camera.bzoom) >> FIXED_SHIFT,
			FIXED_MUL(dst.h, stage.camera.bzoom) >> FIXED_SHIFT,
		};
		Gfx_DrawTexColFlipX(tex, &src, &sdst, 0x80, 0x80, 0x80);
	}
	else
		Stage_DrawTex_FlipX(tex, &src, &dst, stage.camera.bzoom, stage.camera.angle);
}

void Character_Draw(Character *this, Gfx_Tex *tex, const CharFrame *cframe)
{
	Character_DrawParallax(this, tex, cframe, FIXED_UNIT);
	/* The PSX ordering table is LIFO, so submitting ghosts after the main
	 * sprite places them visually behind it. */
	Character_GhostDrawAndCapture(this, tex, cframe, 0x80, 0x80, 0x80);
}

void Character_DrawParallaxCol(Character *this, Gfx_Tex *tex, const CharFrame *cframe, fixed_t parallax, u8 r, u8 g, u8 b)
{
	//Draw character
	fixed_t x = this->x - FIXED_MUL(stage.camera.x, parallax) - FIXED_MUL(FIXED_DEC(cframe->off[0],1),this->size);
	fixed_t y = this->y - FIXED_MUL(stage.camera.y, parallax) - FIXED_MUL(FIXED_DEC(cframe->off[1],1),this->size);

	RECT src = {cframe->src[0], cframe->src[1], cframe->src[2], cframe->src[3]};
	RECT_FIXED dst = {x, y, src.w << FIXED_SHIFT, src.h << FIXED_SHIFT};
	Stage_DrawTexColOpacity(tex, &src, &dst, stage.camera.bzoom, stage.camera.angle, r, g, b, this->opacity);
}

void Character_DrawCol(Character *this, Gfx_Tex *tex, const CharFrame *cframe, u8 r, u8 g, u8 b)
{
	Character_DrawParallaxCol(this, tex, cframe, FIXED_UNIT, r, g, b);
	Character_GhostDrawAndCapture(this, tex, cframe, r, g, b);
}

void Character_DrawFlipped(Character *this, Gfx_Tex *tex, const CharFrame *cframe)
{
	Character_DrawParallaxFlipped(this, tex, cframe, FIXED_UNIT);
}

void Character_DrawParallaxBlendCol(Character *this, Gfx_Tex *tex, const CharFrame *cframe, fixed_t parallax, u8 r, u8 g, u8 b, u8 mode)
{
	fixed_t x = this->x - FIXED_MUL(stage.camera.x, parallax) - FIXED_MUL(FIXED_DEC(cframe->off[0],1),this->size);
	fixed_t y = this->y - FIXED_MUL(stage.camera.y, parallax) - FIXED_MUL(FIXED_DEC(cframe->off[1],1),this->size);

	RECT src = {
		cframe->src[0],
		cframe->src[1],
		cframe->src[2],
		cframe->src[3]
	};

	RECT_FIXED dst = {
		x,
		y,
		src.w << FIXED_SHIFT,
		src.h << FIXED_SHIFT
	};

	Stage_BlendTexCol( tex, &src, &dst, stage.camera.bzoom, stage.camera.angle, r, g, b, mode);
}

void Character_DrawBlendCol(Character *this, Gfx_Tex *tex, const CharFrame *cframe, u8 r, u8 g, u8 b, u8 mode)
{
	Character_DrawParallaxBlendCol( this, tex, cframe, FIXED_UNIT, r, g, b, mode);
}

void Character_DrawParallaxBlendCol_Reflection(Character *this, Gfx_Tex *tex, const CharFrame *cframe, fixed_t parallax, u8 r, u8 g, u8 b, u8 mode)
{
	// original position
	fixed_t x = this->x - FIXED_MUL(stage.camera.x, parallax) - FIXED_MUL(FIXED_DEC(cframe->off[0],1),this->size);
	fixed_t y = this->y - FIXED_MUL(stage.camera.y, parallax) - FIXED_MUL(FIXED_DEC(cframe->off[1],1),this->size);

	RECT src = {
		cframe->src[0],
		cframe->src[1],
		cframe->src[2],
		cframe->src[3]
	};

	// base sprite size
	fixed_t w = src.w << FIXED_SHIFT;
	fixed_t h = src.h << FIXED_SHIFT;

	// reflection starts BELOW original sprite, offset 4px up
	RECT_FIXED dst = {
		x,
		y + h - FIXED_DEC(8,1),
		w,
		h
	};

	// dim reflection
	u8 rr = r / 2;
	u8 gg = g / 2;
	u8 bb = b / 2;

	// draw using Y-flip version
	Stage_BlendTexCol_FlipY(tex, &src, &dst, stage.camera.bzoom, stage.camera.angle, rr, gg, bb, mode);
}

void Character_DrawBlendCol_Reflection(Character *this, Gfx_Tex *tex, const CharFrame *cframe, u8 r, u8 g, u8 b, u8 mode)
{
	Character_DrawParallaxBlendCol_Reflection(this, tex, cframe, FIXED_UNIT, r, g, b, mode);
}

void Character_CheckStartSing(Character *this)
{
	//Update sing end if singing animation
	if (this->animatable.anim == CharAnim_Left ||
	    this->animatable.anim == CharAnim_LeftAlt ||
	    this->animatable.anim == CharAnim_Down ||
	    this->animatable.anim == CharAnim_DownAlt ||
	    this->animatable.anim == CharAnim_Up ||
	    this->animatable.anim == CharAnim_UpAlt ||
	    this->animatable.anim == CharAnim_Right ||
	    this->animatable.anim == CharAnim_RightAlt ||
	    ((this->spec & CHAR_SPEC_MISSANIM) &&
	    (this->animatable.anim == PlayerAnim_LeftMiss ||
	     this->animatable.anim == PlayerAnim_DownMiss ||
	     this->animatable.anim == PlayerAnim_UpMiss ||
	     this->animatable.anim == PlayerAnim_RightMiss)))
		this->sing_end = stage.note_scroll + (FIXED_DEC(12,1) << 2); //1 beat
}

void Character_CheckEndSing(Character *this)
{
	if ((this->animatable.anim == CharAnim_Left ||
	     this->animatable.anim == CharAnim_LeftAlt ||
	     this->animatable.anim == CharAnim_Down ||
	     this->animatable.anim == CharAnim_DownAlt ||
	     this->animatable.anim == CharAnim_Up ||
	     this->animatable.anim == CharAnim_UpAlt ||
	     this->animatable.anim == CharAnim_Right ||
	     this->animatable.anim == CharAnim_RightAlt ||
	    ((this->spec & CHAR_SPEC_MISSANIM) &&
	    (this->animatable.anim == PlayerAnim_LeftMiss ||
	     this->animatable.anim == PlayerAnim_DownMiss ||
	     this->animatable.anim == PlayerAnim_UpMiss ||
	     this->animatable.anim == PlayerAnim_RightMiss))) &&
	    stage.note_scroll >= this->sing_end)
		this->set_anim(this, CharAnim_Idle);
}

void Character_PerformIdle(Character *this)
{
	Character_CheckEndSing(this);
	if (stage.flag & STAGE_FLAG_JUST_STEP)
	{
		if (Animatable_Ended(&this->animatable) &&
		    (this->animatable.anim != CharAnim_Left &&
		     this->animatable.anim != CharAnim_LeftAlt &&
		     this->animatable.anim != CharAnim_Down &&
		     this->animatable.anim != CharAnim_DownAlt &&
		     this->animatable.anim != CharAnim_Up &&
		     this->animatable.anim != CharAnim_UpAlt &&
		     this->animatable.anim != CharAnim_Right &&
		     this->animatable.anim != CharAnim_RightAlt) &&
		    (stage.song_step & 0x7) == 0)
			this->set_anim(this, CharAnim_Idle);
	}
}

void Character_CheckStartSing2(Character *this)
{
    if (this->animatable2.anim == CharAnim_Left ||
        this->animatable2.anim == CharAnim_LeftAlt ||
        this->animatable2.anim == CharAnim_Down ||
        this->animatable2.anim == CharAnim_DownAlt ||
        this->animatable2.anim == CharAnim_Up ||
        this->animatable2.anim == CharAnim_UpAlt ||
        this->animatable2.anim == CharAnim_Right ||
        this->animatable2.anim == CharAnim_RightAlt)
    {
        this->sing_end = stage.note_scroll + (FIXED_DEC(12,1) << 2); // 1 beat
    }
}

void Character_CheckEndSing2(Character *this)
{
    if ((this->animatable2.anim == CharAnim_Left ||
         this->animatable2.anim == CharAnim_LeftAlt ||
         this->animatable2.anim == CharAnim_Down ||
         this->animatable2.anim == CharAnim_DownAlt ||
         this->animatable2.anim == CharAnim_Up ||
         this->animatable2.anim == CharAnim_UpAlt ||
         this->animatable2.anim == CharAnim_Right ||
         this->animatable2.anim == CharAnim_RightAlt) &&
        stage.note_scroll >= this->sing_end)
    {
        this->set_anim(this, CharAnim_Idle);
    }
}

void Character_PerformIdle2(Character *this)
{
    Character_CheckEndSing2(this);
    if (stage.flag & STAGE_FLAG_JUST_STEP)
    {
        if (Animatable_Ended(&this->animatable2) &&
            (this->animatable2.anim != CharAnim_Left &&
             this->animatable2.anim != CharAnim_LeftAlt &&
             this->animatable2.anim != CharAnim_Down &&
             this->animatable2.anim != CharAnim_DownAlt &&
             this->animatable2.anim != CharAnim_Up &&
             this->animatable2.anim != CharAnim_UpAlt &&
             this->animatable2.anim != CharAnim_Right &&
             this->animatable2.anim != CharAnim_RightAlt) &&
            (stage.song_step & 0x7) == 0)
        {
            this->set_anim(this, CharAnim_Idle);
        }
    }
}
