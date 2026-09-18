/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "character.h"

#include "mem.h"
#include "main.h"
#include "stage.h"

//Forward declarations for the shared ghost/trail atlas block helpers
static void Character_GhostReleaseAtlases(Character *this, u8 keep_from);
static void Character_GhostResetPack(Character *this);
static boolean Character_GhostFreeTake(Character *this, u16 frame_words, u16 h,
	s16 *out_x, s16 *out_y, s16 *out_cx, s16 *out_cy);
static void Character_GhostFreePush(Character *this, s16 x, s16 y,
	u16 w, u16 h, s16 clut_x, s16 clut_y);

//Character functions
void Character_Free(Character *this)
{
	//Check if NULL
	if (this == NULL)
		return;

	//Free character
	this->free(this);
	if (this->ghosts_storage != NULL)
	{
		//Release finder-placed atlas blocks back to free VRAM
		Character_GhostReleaseAtlases(this, 0);
		Mem_Free(this->ghosts_storage);
	}
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
			Character_GhostReleaseAtlases(this, 0);
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
	else
	{
		//Releasing the previous atlas blocks; frames are re-captured
		Character_GhostReleaseAtlases(this, 0);
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
	this->ghosts.no_healthbar_color = true;
	this->ghosts.color_r = this->ghosts.color_g = this->ghosts.color_b = 0x80;
	this->ghosts.count = count;
	this->ghosts.needed_frames = 0;
	this->ghosts.used = 0;
	this->ghosts.head = 0;
	this->ghosts.vram_x = vram_x;
	this->ghosts.vram_y = vram_y;
	this->ghosts.vram_auto = (vram_x < 0 || vram_y < 0);
	this->ghosts.slot_words = CHARACTER_GHOST_SLOT_WORDS;
	this->ghosts.slot_height = CHARACTER_GHOST_SLOT_HEIGHT;
	//Block 0 is the configured atlas; extras spill over as needed
	this->ghosts.atlases[0].x = vram_x;
	this->ghosts.atlases[0].y = vram_y;
	this->ghosts.atlases[0].pack_x = 0;
	this->ghosts.atlases[0].pack_y = 0;
	this->ghosts.atlases[0].pack_row_height = 0;
	this->ghosts.atlases[0].reserve_w = 0;
	this->ghosts.atlases[0].reserve_h = 0;
	this->ghosts.atlases[0].clut_next = 0;
	this->ghosts.atlases[0].reserved = false;
	this->ghosts.atlas_count = 1;
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

	//Trail defaults (legacy ghost mode, trail inactive)
	this->ghosts.trail_mode = false;
	this->ghosts.trail_length = CHARACTER_TRAIL_DEFAULT_LENGTH;
	this->ghosts.trail_delay = CHARACTER_TRAIL_DEFAULT_DELAY;
	this->ghosts.trail_timer = 0;
	this->ghosts.trail_alpha = CHARACTER_TRAIL_DEFAULT_ALPHA;
	this->ghosts.trail_diff = CHARACTER_TRAIL_DEFAULT_DIFF;
	this->ghosts.trail_used = 0;
	this->ghosts.free_count = 0;
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
	this->ghosts.trail_used = 0;
	//Extra blocks held frames that are no longer needed: release them.
	//Block 0 is kept (re-placed below when automatic) so its VRAM is reused.
	Character_GhostReleaseAtlases(this, 1);
	this->ghosts.atlases[0].pack_x = 0;
	this->ghosts.atlases[0].pack_y = 0;
	this->ghosts.atlases[0].pack_row_height = 0;
	this->ghosts.atlases[0].clut_next = 0;
	//Auto atlases are re-placed for the new size
	if (this->ghosts.vram_auto)
	{
		CharacterGhostAtlas *ab = &this->ghosts.atlases[0];
		if (ab->reserved && ab->x >= 0 && ab->y >= 0)
		{
			Gfx_VramUnmark(ab->x, ab->y, (s16)ab->reserve_w, (s16)ab->reserve_h);
			ab->reserved = false;
		}
		ab->x = ab->y = -1;
		ab->reserve_w = ab->reserve_h = 0;
		this->ghosts.vram_x = this->ghosts.vram_y = -1;
	}
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
		this->ghosts.trail_used = 0;
		Character_GhostResetPack(this);
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
	//Trail snapshots run on their own timer and persist across animations;
	//legacy per-note resets must not touch the shared pack cursors, or new
	//captures would pack over rects the live trail still references
	if (this->ghosts.trail_mode)
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
		Character_GhostResetPack(this);
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

void Character_GhostSetNoHealthbarColor(Character *this, boolean no_healthbar_color)
{
	if (this->ghosts_storage == NULL)
		return;
	this->ghosts.no_healthbar_color = no_healthbar_color;
}

//Resolve a snapshot tint: explicit SetColor wins, then the health bar
//(0xFFRRGGBB like Stage_DrawHealthBar) unless opted out, else draw color
static void Character_GhostTint(Character *this, u8 r, u8 g, u8 b,
	u8 *out_r, u8 *out_g, u8 *out_b)
{
	u32 hb;

	if (this->ghosts.color_override)
	{
		*out_r = this->ghosts.color_r;
		*out_g = this->ghosts.color_g;
		*out_b = this->ghosts.color_b;
		return;
	}
	if (!this->ghosts.no_healthbar_color)
	{
		hb = this->health_bar;
		*out_r = (u8)((hb >> 16) & 0xFF);
		*out_g = (u8)((hb >> 8) & 0xFF);
		*out_b = (u8)(hb & 0xFF);
		return;
	}
	*out_r = r;
	*out_g = g;
	*out_b = b;
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

//Shared VRAM atlas blocks for ghost and trail frames.
//Frames pack into blocks in order and spill into a new free block only when
//every placed block is full, so e.g. a 64x256 block is filled before another
//freespace is found. Loaded frames stay in their blocks until dropped or the
//system resets; finder-placed blocks are released with Unmark when freed.
//
//Allocation sizes are bucketed (copies and draws stay exact): frames that
//differ by a pixel or two across animations share the same few rect shapes
//instead of fragmenting VRAM into a restart loop.
static u16 Character_GhostBucketW(u16 w)
{
	if (w <= 16)
		return 16;
	if (w <= 32)
		return 32;
	if (w <= 48)
		return 48;
	return 64;
}

static u16 Character_GhostBucketH(u16 h)
{
	if (h <= 64)
		return 64;
	if (h <= 128)
		return 128;
	if (h <= 192)
		return 192;
	//Cap at slot_height - 1: a 256 bucket could never satisfy
	//pack_y + h <= slot_height - 1 at any position (killed all sings)
	if (h <= 255)
		return 255;
	return h;
}
static u16 Character_GhostAtlasReserveW(Character *this)
{
	u16 w = this->ghosts.slot_words;
	if (w < 128)
		w = 128; //room for the per-block CLUT strip
	return w;
}

//Place one atlas block of slot size via the finder and reserve it
static s8 Character_GhostNewAtlas(Character *this)
{
	CharacterGhostAtlas *ab;
	s16 x, y;
	u16 w, h;

	if (this->ghosts.atlas_count >= CHARACTER_GHOST_MAX_ATLAS)
		return -1;
	w = this->ghosts.slot_words;
	h = this->ghosts.slot_height;
	if (w == 0 || w > 64 || h == 0 || h > 256)
		return -1;
	w = Character_GhostAtlasReserveW(this);
	if (!Gfx_VramFindFree(w, h, &x, &y))
		return -1;

	ab = &this->ghosts.atlases[this->ghosts.atlas_count];
	ab->x = x;
	ab->y = y;
	ab->pack_x = ab->pack_y = ab->pack_row_height = 0;
	ab->reserve_w = w;
	ab->reserve_h = h;
	ab->clut_next = 0;
	ab->reserved = true;
	Gfx_VramMark(x, y, (s16)w, (s16)h);
	return (s8)this->ghosts.atlas_count++;
}

//Reset every block's pack cursor (loaded frames stay, rects are reused;
//abandoned rects are forgotten since the pack area restarts clean)
static void Character_GhostResetPack(Character *this)
{
	u8 i;
	for (i = 0; i < this->ghosts.atlas_count; i++)
	{
		this->ghosts.atlases[i].pack_x = 0;
		this->ghosts.atlases[i].pack_y = 0;
		this->ghosts.atlases[i].pack_row_height = 0;
		this->ghosts.atlases[i].clut_next = 0;
	}
	this->ghosts.free_count = 0;
}

//Release finder-placed blocks from keep_from on (explicit block 0 is only
//unmarked when included: its VRAM otherwise belongs to the user)
static void Character_GhostReleaseAtlases(Character *this, u8 keep_from)
{
	u8 i;
	for (i = keep_from; i < this->ghosts.atlas_count; i++)
	{
		CharacterGhostAtlas *ab = &this->ghosts.atlases[i];
		if (ab->reserved && ab->x >= 0 && ab->y >= 0)
		{
			Gfx_VramUnmark(ab->x, ab->y, (s16)ab->reserve_w, (s16)ab->reserve_h);
			ab->reserved = false;
		}
		ab->x = ab->y = -1;
		ab->pack_x = ab->pack_y = ab->pack_row_height = 0;
		ab->reserve_w = ab->reserve_h = 0;
		ab->clut_next = 0;
	}
	if (this->ghosts.atlas_count > keep_from)
		this->ghosts.atlas_count = keep_from;
	//Abandoned rects may point into released blocks: forget them all
	this->ghosts.free_count = 0;
}

//Probe placed blocks in order for a free rect (no state change). Returns the
//block or -1, placing a new free block when every placed block is full.
static s8 Character_GhostSlotAlloc(Character *this, u16 frame_words, u16 h,
	s16 *out_x, s16 *out_y)
{
	CharacterGhostAtlas *ab;
	u16 pack_x, pack_y;
	u8 i;

	if (frame_words == 0 || h == 0 || frame_words > this->ghosts.slot_words ||
	    h > this->ghosts.slot_height - 1)
		return -1;

	for (i = 0; i < this->ghosts.atlas_count; i++)
	{
		ab = &this->ghosts.atlases[i];
		if (ab->x < 0 || ab->y < 0 || ab->clut_next >= 8)
			continue;
		pack_x = ab->pack_x;
		pack_y = ab->pack_y;
		if (pack_x + frame_words > this->ghosts.slot_words)
		{
			pack_x = 0;
			pack_y += ab->pack_row_height;
		}
		if (pack_y + h > this->ghosts.slot_height - 1)
			continue;
		*out_x = ab->x + (s16)pack_x;
		*out_y = ab->y + (s16)pack_y;
		return (s8)i;
	}

	//Every placed block is full: find another freespace
	i = (u8)Character_GhostNewAtlas(this);
	if ((s8)i < 0)
		return -1;
	ab = &this->ghosts.atlases[i];
	*out_x = ab->x;
	*out_y = ab->y;
	return (s8)i;
}

static void Character_GhostSlotCommit(Character *this, s8 block, u16 frame_words, u16 h)
{
	CharacterGhostAtlas *ab;

	if (block < 0 || (u8)block >= this->ghosts.atlas_count)
		return;
	ab = &this->ghosts.atlases[block];
	if (ab->pack_x + frame_words > this->ghosts.slot_words)
	{
		ab->pack_x = 0;
		ab->pack_y += ab->pack_row_height;
		ab->pack_row_height = 0;
	}
	ab->pack_x += frame_words;
	if (h > ab->pack_row_height)
		ab->pack_row_height = h;
}

//Take an abandoned rect that fits (first fit). Freed rects sit behind the
//append-only pack cursors, so reuse can never overlap a fresh pack.
static boolean Character_GhostFreeTake(Character *this, u16 frame_words, u16 h,
	s16 *out_x, s16 *out_y, s16 *out_cx, s16 *out_cy)
{
	u8 i;

	for (i = 0; i < this->ghosts.free_count; i++)
	{
		CharacterGhostFree *fr = &this->ghosts.free_rects[i];
		if (frame_words <= fr->w && h <= fr->h)
		{
			*out_x = fr->x;
			*out_y = fr->y;
			*out_cx = fr->clut_x;
			*out_cy = fr->clut_y;
			this->ghosts.free_rects[i] = this->ghosts.free_rects[--this->ghosts.free_count];
			return true;
		}
	}
	return false;
}

//Return a dropped snapshot's rect (+ CLUT) to the pool. Beyond the cap the
//rect is simply forgotten (bounded leak, same as before).
static void Character_GhostFreePush(Character *this, s16 x, s16 y,
	u16 w, u16 h, s16 clut_x, s16 clut_y)
{
	CharacterGhostFree *fr;

	if (this->ghosts.free_count >= CHARACTER_GHOST_MAX_FREE)
		return;
	fr = &this->ghosts.free_rects[this->ghosts.free_count++];
	fr->x = x;
	fr->y = y;
	fr->w = w;
	fr->h = h;
	fr->clut_x = clut_x;
	fr->clut_y = clut_y;
}

//Next per-block CLUT slot (each block owns a strip of 8, consumed by the
//caller only after its copy succeeds so failures never leak indices)
static boolean Character_GhostAtlasClut(Character *this, s8 block, s16 *out_x, s16 *out_y)
{
	CharacterGhostAtlas *ab;

	if (block < 0 || (u8)block >= this->ghosts.atlas_count)
		return false;
	ab = &this->ghosts.atlases[block];
	if (ab->x < 0 || ab->y < 0 || ab->clut_next >= 8)
		return false;
	*out_x = ab->x + (s16)((u16)ab->clut_next * 16);
	*out_y = ab->y + (s16)(this->ghosts.slot_height - 1);
	return true;
}

//Place block 0 (the configured atlas) in free VRAM on first capture.
//Configure with negative vram_x/vram_y for this: the finder avoids the
//framebuffers and every HUD, character, stage, ARC and uploaded TIM, then
//reserves the rect so other atlases avoid it.
static boolean Character_GhostPlaceAtlas(Character *this)
{
	CharacterGhostAtlas *ab;
	s16 vram_x, vram_y;
	u16 need_w, need_h;

	if (this->ghosts.atlas_count == 0)
		return false;
	ab = &this->ghosts.atlases[0];
	if (!this->ghosts.vram_auto)
		return ab->x >= 0 && ab->y >= 0;
	if (ab->x >= 0 && ab->y >= 0)
		return true; //already placed

	//Copies must fit a single texture page; leave room for the CLUT strip
	if (this->ghosts.slot_words == 0 || this->ghosts.slot_words > 64 ||
	    this->ghosts.slot_height == 0 || this->ghosts.slot_height > 256)
		return false;
	need_w = Character_GhostAtlasReserveW(this);
	need_h = this->ghosts.slot_height;

	if (!Gfx_VramFindFree(need_w, need_h, &vram_x, &vram_y))
		return false;
	ab->x = vram_x;
	ab->y = vram_y;
	ab->reserve_w = need_w;
	ab->reserve_h = need_h;
	ab->reserved = true;
	this->ghosts.vram_x = vram_x;
	this->ghosts.vram_y = vram_y;
	Gfx_VramMark(vram_x, vram_y, (s16)need_w, (s16)need_h);
	return true;
}

static void Character_GhostCapture(Character *this, Gfx_Tex *tex,
	const CharFrame *cframe, u8 anim, u8 opposing_index)
{
	CharacterGhostFrame *ghost;
	RECT region;
	s16 slot_x, slot_y, clut_x, clut_y;
	s8 block;
	u16 frame_words, slot_w, slot_h;
	u8 i;

	if (!this->ghosts.enabled ||
	    (!this->ghosts.active && !this->ghosts.capture_requested) ||
	    (!this->ghosts.capture_requested && !this->ghosts.continuous && !this->ghosts.secondary_is_main))
		return;

	//Auto atlases are placed in free VRAM on first capture
	if (!Character_GhostPlaceAtlas(this))
	{
		this->ghosts.capture_requested = false;
		return;
	}

	region.x = cframe->src[0];
	region.y = cframe->src[1];
	region.w = cframe->src[2];
	region.h = cframe->src[3];
	frame_words = ((region.x & ((1 << tex->pxshift) - 1)) + region.w +
	               (1 << tex->pxshift) - 1) >> tex->pxshift;
	//Bucket the allocation size (copies and draws stay exact) so frames
	//that differ by a pixel or two share rect shapes instead of fragmenting
	slot_w = Character_GhostBucketW(frame_words);
	slot_h = Character_GhostBucketH((u16)region.h);

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
		//Reuse the frame's own CLUT: with more than one atlas block the
		//slot may live outside block 0, so never recompute it by index
		if (Gfx_CopyTexRegion(&ghost->tex, tex, &region, slot_x, slot_y,
		    ghost->tex.tim_crect.x, ghost->tex.tim_crect.y))
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

	//Share the atlas blocks with trail frames: reuse an abandoned rect
	//first, else probe for a fresh one (spills into a new free block)
	if (Character_GhostFreeTake(this, slot_w, slot_h,
	    &slot_x, &slot_y, &clut_x, &clut_y))
	{
		block = -1;
	}
	else
	{
		block = Character_GhostSlotAlloc(this, slot_w, slot_h, &slot_x, &slot_y);
		if (block < 0 || !Character_GhostAtlasClut(this, block, &clut_x, &clut_y))
		{
			this->ghosts.capture_requested = false;
			return;
		}
	}

	ghost = &this->ghosts.frames[this->ghosts.used];

	if (Gfx_CopyTexRegion(&ghost->tex, tex, &region, slot_x, slot_y,
	    clut_x, clut_y))
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
		Character_GhostTint(this, 0x80, 0x80, 0x80,
			&ghost->r, &ghost->g, &ghost->b);
		ghost->fading = false;
		ghost->atlas_x = slot_x;
		ghost->atlas_y = slot_y;
		ghost->atlas_words = slot_w;
		ghost->atlas_height = slot_h;
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
		if (block >= 0)
		{
			//Fresh pack: consume its CLUT slot and keep the rect
			this->ghosts.atlases[block].clut_next++;
			Character_GhostSlotCommit(this, block, slot_w, slot_h);
		}
		//Free-list rects carry their own rect + CLUT: nothing to commit
	}
	else if (block < 0)
	{
		//Copy failed: return the taken rect to the pool
		Character_GhostFreePush(this, slot_x, slot_y,
			slot_w, slot_h, clut_x, clut_y);
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
		Character_GhostTint(this, r, g, b,
			&this->ghosts.frames[i].r,
			&this->ghosts.frames[i].g,
			&this->ghosts.frames[i].b);
	}
	Character_GhostDrawStored(this);
	Character_GhostCapture(this, tex, cframe, this->animatable.anim, 0xFF);
}

//FlxTrail-style trail ("trail", legacy ghost code above is kept as-is)
//Unlike legacy ghosts (which capture per sung note and drift/fade),
//a trail snapshots the character every trail_delay frames into a ring of
//trail_length images, newest first, each drawn with
//opacity = trail_alpha - index * trail_diff.

static u8 Character_TrailFixedToAlpha(fixed_t a)
{
	s64 v = ((s64)a * 255) >> FIXED_SHIFT;
	if (v < 0)
		v = 0;
	if (v > 255)
		v = 255;
	return (u8)v;
}

void Character_TrailConfigure(Character *this, boolean enabled, s16 vram_x, s16 vram_y,
	u8 length, u8 delay, fixed_t alpha, fixed_t diff)
{
	u8 i;

	if (!enabled)
	{
		if (this->ghosts_storage != NULL)
		{
			Character_GhostReleaseAtlases(this, 0);
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
			sprintf(error_msg, "[Character_TrailConfigure] Failed to allocate trail state");
			ErrorLock();
			return;
		}
		//Fresh storage, use default atlas and no color override
		this->ghosts.slot_words = CHARACTER_GHOST_SLOT_WORDS;
		this->ghosts.slot_height = CHARACTER_GHOST_SLOT_HEIGHT;
		this->ghosts.color_override = false;
		this->ghosts.no_healthbar_color = true;
		this->ghosts.color_r = this->ghosts.color_g = this->ghosts.color_b = 0x80;
		this->ghosts.continuous = false;
		this->ghosts.secondary_is_main = false;
		this->ghosts.capture_requested = false;
		this->ghosts.request_time = 0;
		this->ghosts.requests_at_time = 0;
		this->ghosts.requested_anim = CharAnim_Idle;
		this->ghosts.opposing_count = 0;
		for (i = 0; i < CHARACTER_GHOST_MAX_OPPOSING; i++)
		{
			this->ghosts.opposing_anim[i] = CharAnim_Idle;
			this->ghosts.opposing_pending[i] = false;
		}
		this->ghosts.needed_frames = 0;
		this->ghosts.used = 0;
		this->ghosts.head = 0;
		this->ghosts.count = CHARACTER_GHOST_DEFAULT_COUNT;
	}
	else
	{
		//Releasing the previous atlas blocks; frames are re-captured
		Character_GhostReleaseAtlases(this, 0);
	}

	if (length == 0)
		length = CHARACTER_TRAIL_DEFAULT_LENGTH;
	if (length > CHARACTER_GHOST_MAX_COUNT)
		length = CHARACTER_GHOST_MAX_COUNT;

	this->ghosts.enabled = true;
	this->ghosts.active = true;
	this->ghosts.trail_mode = true;
	this->ghosts.vram_x = vram_x;
	this->ghosts.vram_y = vram_y;
	this->ghosts.vram_auto = (vram_x < 0 || vram_y < 0);
	this->ghosts.trail_length = length;
	this->ghosts.trail_delay = delay;
	this->ghosts.trail_timer = 0; //snapshot immediately on next draw
	this->ghosts.trail_alpha = Character_TrailFixedToAlpha(alpha);
	this->ghosts.trail_diff = Character_TrailFixedToAlpha(diff);
	this->ghosts.trail_used = 0;
	//Block 0 is the configured atlas; extras spill over as needed
	this->ghosts.atlases[0].x = vram_x;
	this->ghosts.atlases[0].y = vram_y;
	this->ghosts.atlases[0].pack_x = 0;
	this->ghosts.atlases[0].pack_y = 0;
	this->ghosts.atlases[0].pack_row_height = 0;
	this->ghosts.atlases[0].reserve_w = 0;
	this->ghosts.atlases[0].reserve_h = 0;
	this->ghosts.atlases[0].clut_next = 0;
	this->ghosts.atlases[0].reserved = false;
	this->ghosts.atlas_count = 1;
	this->ghosts.free_count = 0;
	for (i = 0; i < CHARACTER_GHOST_MAX_COUNT; i++)
		this->ghosts.frames[i].valid = false;
}

void Character_TrailSetLength(Character *this, u8 length)
{
	if (this->ghosts_storage == NULL)
		return;
	if (length == 0)
		length = CHARACTER_TRAIL_DEFAULT_LENGTH;
	if (length > CHARACTER_GHOST_MAX_COUNT)
		length = CHARACTER_GHOST_MAX_COUNT;
	this->ghosts.trail_length = length;
	//Shrinking returns the dropped snapshots' rects to the pool so their
	//VRAM stays reusable instead of fragmenting the blocks
	while (this->ghosts.trail_used > length)
	{
		CharacterGhostFrame *dropped = &this->ghosts.frames[--this->ghosts.trail_used];
		if (dropped->valid)
		{
			Character_GhostFreePush(this,
				(s16)dropped->atlas_x, (s16)dropped->atlas_y,
				dropped->atlas_words, dropped->atlas_height,
				dropped->tex.tim_crect.x, dropped->tex.tim_crect.y);
			dropped->valid = false;
		}
	}
}

void Character_TrailSetDelay(Character *this, u8 delay)
{
	if (this->ghosts_storage == NULL)
		return;
	this->ghosts.trail_delay = delay;
}

void Character_TrailSetAlpha(Character *this, fixed_t alpha)
{
	if (this->ghosts_storage == NULL)
		return;
	this->ghosts.trail_alpha = Character_TrailFixedToAlpha(alpha);
}

void Character_TrailSetDiff(Character *this, fixed_t diff)
{
	if (this->ghosts_storage == NULL)
		return;
	this->ghosts.trail_diff = Character_TrailFixedToAlpha(diff);
}

void Character_TrailSetActive(Character *this, boolean active)
{
	if (this->ghosts_storage == NULL)
		return;
	if (!this->ghosts.enabled)
		active = false;
	if (this->ghosts.active == active)
		return;

	this->ghosts.active = active;
	this->ghosts.trail_timer = 0;
	if (!active)
	{
		//Mirror the legacy ghost reset so re-enabling starts clean
		this->ghosts.trail_used = 0;
		Character_GhostResetPack(this);
	}
}

void Character_TrailClear(Character *this)
{
	u8 i;
	if (this->ghosts_storage == NULL)
		return;
	this->ghosts.trail_used = 0;
	this->ghosts.trail_timer = 0;
	Character_GhostResetPack(this);
	for (i = 0; i < CHARACTER_GHOST_MAX_COUNT; i++)
		this->ghosts.frames[i].valid = false;
}

static void Character_TrailDrawStored(Character *this)
{
	u8 i;
	fixed_t old_x = this->x;
	fixed_t old_y = this->y;

	for (i = 0; i < this->ghosts.trail_used; i++)
	{
		CharacterGhostFrame *trail = &this->ghosts.frames[i];
		s32 opacity;
		RECT src;
		RECT_FIXED dst;
		if (!trail->valid)
			continue;

		//FlxTrail alpha falloff: newest is brightest, oldest faintest
		opacity = (s32)this->ghosts.trail_alpha - (s32)i * (s32)this->ghosts.trail_diff;
		if (opacity <= 0)
			continue;
		if (opacity > 255)
			opacity = 255;

		//Trail images stay where they were snapshotted (no drift)
		this->x = trail->x;
		this->y = trail->y;

		src.x = trail->frame.src[0];
		src.y = trail->frame.src[1];
		src.w = trail->frame.src[2];
		src.h = trail->frame.src[3];
		dst.x = this->x - stage.camera.x - FIXED_MUL(FIXED_DEC(trail->frame.off[0],1),this->size);
		dst.y = this->y - stage.camera.y - FIXED_MUL(FIXED_DEC(trail->frame.off[1],1),this->size);
		dst.w = FIXED_MUL(src.w << FIXED_SHIFT, this->size);
		dst.h = FIXED_MUL(src.h << FIXED_SHIFT, this->size);
		//Blended draws keep the additive afterimage glow; per-slot alpha
		//scaling fakes the opacity falloff (newest brightest in front)
		Stage_DrawBlendTexAll(&trail->tex, &src, &dst,
			stage.camera.bzoom, stage.camera.angle, 0,
			trail->r, trail->g, trail->b, (u8)opacity,
			false, false, false, 0);
	}
	this->x = old_x;
	this->y = old_y;
}

//Share an existing snapshot's VRAM slot: overwrite the oldest snapshot whose
//slot fits the new animation frame, then move it to the front (newest).
//Frames that differ by a pixel or two (normal across one animation) each
//settle into a fitting slot instead of skipping, going stale and snapping.
static boolean Character_TrailReuseFit(Character *this, Gfx_Tex *tex,
	const CharFrame *cframe, const RECT *region, u16 frame_words,
	u8 r, u8 g, u8 b, u8 count)
{
	CharacterGhostFrame reused;
	u8 i, j;

	if (count == 0 || count > CHARACTER_GHOST_MAX_COUNT)
		return false;
	for (i = count; i-- > 0; )
	{
		reused = this->ghosts.frames[i];
		if (!reused.valid || frame_words > reused.atlas_words ||
		    region->h > reused.atlas_height)
			continue;

		if (!Gfx_CopyTexRegion(&reused.tex, tex, region,
		    reused.atlas_x, reused.atlas_y,
		    reused.tex.tim_crect.x, reused.tex.tim_crect.y))
			return false;

		reused.frame = *cframe;
		reused.frame.src[0] = ((reused.atlas_x & 0x3F) << tex->pxshift) +
		                        (region->x & ((1 << tex->pxshift) - 1));
		reused.frame.src[1] = reused.atlas_y & 0xFF;
		reused.x = this->x;
		reused.y = this->y;
		Character_GhostTint(this, r, g, b, &reused.r, &reused.g, &reused.b);
		for (j = i; j > 0; j--)
			this->ghosts.frames[j] = this->ghosts.frames[j - 1];
		this->ghosts.frames[0] = reused;
		return true;
	}
	return false;
}

static void Character_TrailCapture(Character *this, Gfx_Tex *tex,
	const CharFrame *cframe, u8 r, u8 g, u8 b)
{
	CharacterGhostFrame *trail;
	CharacterGhostFrame recycled;
	RECT region;
	s16 slot_x, slot_y, clut_x, clut_y;
	s8 block;
	u16 frame_words, slot_w, slot_h;
	u8 i, length;

	if (this->ghosts_storage == NULL || !this->ghosts.enabled || !this->ghosts.active)
		return;
	if (stage.paused)
		return;

	length = this->ghosts.trail_length;
	if (length == 0 || length > CHARACTER_GHOST_MAX_COUNT)
		return;

	//Auto atlases are placed in free VRAM on first capture (back off and
	//retry later instead of scanning every frame when VRAM is full)
	if (!Character_GhostPlaceAtlas(this))
	{
		this->ghosts.trail_timer = this->ghosts.trail_delay;
		return;
	}

	//FlxTrail delay: snapshot once every trail_delay frames (0 = every frame)
	if (this->ghosts.trail_timer != 0)
	{
		this->ghosts.trail_timer--;
		return;
	}
	this->ghosts.trail_timer = this->ghosts.trail_delay;

	region.x = cframe->src[0];
	region.y = cframe->src[1];
	region.w = cframe->src[2];
	region.h = cframe->src[3];
	frame_words = ((region.x & ((1 << tex->pxshift) - 1)) + region.w +
	               (1 << tex->pxshift) - 1) >> tex->pxshift;
	//Bucket the allocation size like the ghost path (copies stay exact)
	slot_w = Character_GhostBucketW(frame_words);
	slot_h = Character_GhostBucketH((u16)region.h);

	if (this->ghosts.trail_used < length)
	{
		//Abandoned rects first (previous animations settle back into them),
		//else probe for a fresh one (spills into a new free block)
		if (Character_GhostFreeTake(this, slot_w, slot_h,
		    &slot_x, &slot_y, &clut_x, &clut_y))
		{
			block = -1;
		}
		else
		{
			block = Character_GhostSlotAlloc(this, slot_w, slot_h, &slot_x, &slot_y);
			if (block < 0 || !Character_GhostAtlasClut(this, block, &clut_x, &clut_y))
			{
				//Every block is full: share a fitting snapshot's slot when
				//the new animation frame fits one, else skip (no wedge)
				if (this->ghosts.trail_used == 0)
					return;
				Character_TrailReuseFit(this, tex, cframe, &region, frame_words,
					r, g, b, this->ghosts.trail_used);
				return;
			}
		}

		//Make room at index 0 (newest first)
		for (i = this->ghosts.trail_used; i > 0; i--)
			this->ghosts.frames[i] = this->ghosts.frames[i - 1];
		trail = &this->ghosts.frames[0];

		if (Gfx_CopyTexRegion(&trail->tex, tex, &region, slot_x, slot_y,
		    clut_x, clut_y))
		{
			trail->frame = *cframe;
			trail->frame.src[0] = ((slot_x & 0x3F) << tex->pxshift) +
			                      (region.x & ((1 << tex->pxshift) - 1));
			trail->frame.src[1] = slot_y & 0xFF;
			trail->x = this->x;
			trail->y = this->y;
			trail->drift_x = 0;
			trail->drift_y = 0;
			trail->distance = 0;
			Character_GhostTint(this, r, g, b, &trail->r, &trail->g, &trail->b);
			trail->fading = false;
			trail->atlas_x = slot_x;
			trail->atlas_y = slot_y;
			trail->atlas_words = slot_w;
			trail->atlas_height = slot_h;
			trail->opposing_index = 0xFF;
			trail->valid = true;
			this->ghosts.trail_used++;
			if (block >= 0)
			{
				this->ghosts.atlases[block].clut_next++;
				Character_GhostSlotCommit(this, block, slot_w, slot_h);
			}
		}
		else
		{
			//Copy failed: undo the shift, returning a taken rect
			for (i = 0; i < this->ghosts.trail_used; i++)
				this->ghosts.frames[i] = this->ghosts.frames[i + 1];
			if (block < 0)
				Character_GhostFreePush(this, slot_x, slot_y,
					slot_w, slot_h, clut_x, clut_y);
		}
		return;
	}

	//Buffer full: share a fitting snapshot's slot when it fits
	if (Character_TrailReuseFit(this, tex, cframe, &region, frame_words,
	    r, g, b, length))
		return;

	//Buffer full and no fitting slot: reuse an abandoned rect first
	//(previous directions settle back into them), else probe a fresh one
	recycled = this->ghosts.frames[length - 1];
	if (Character_GhostFreeTake(this, slot_w, slot_h,
	    &slot_x, &slot_y, &clut_x, &clut_y))
	{
		block = -1;
	}
	else
	{
		block = Character_GhostSlotAlloc(this, slot_w, slot_h, &slot_x, &slot_y);
		if (block < 0 || !Character_GhostAtlasClut(this, block, &clut_x, &clut_y))
		{
			//Every block is full and VRAM is out: restart the trail so it
			//keeps working (loaded frames are re-captured from here)
			Character_GhostResetPack(this);
			this->ghosts.trail_used = 0;
			return;
		}
	}

	for (i = length - 1; i > 0; i--)
		this->ghosts.frames[i] = this->ghosts.frames[i - 1];
	trail = &this->ghosts.frames[0];

	if (Gfx_CopyTexRegion(&trail->tex, tex, &region, slot_x, slot_y,
	    clut_x, clut_y))
	{
		trail->frame = *cframe;
		trail->frame.src[0] = ((slot_x & 0x3F) << tex->pxshift) +
		                      (region.x & ((1 << tex->pxshift) - 1));
		trail->frame.src[1] = slot_y & 0xFF;
		trail->x = this->x;
		trail->y = this->y;
		trail->drift_x = 0;
		trail->drift_y = 0;
		trail->distance = 0;
		Character_GhostTint(this, r, g, b, &trail->r, &trail->g, &trail->b);
		trail->fading = false;
		trail->atlas_x = slot_x;
		trail->atlas_y = slot_y;
		trail->atlas_words = slot_w;
		trail->atlas_height = slot_h;
		trail->opposing_index = 0xFF;
		trail->valid = true;
		if (block >= 0)
		{
			this->ghosts.atlases[block].clut_next++;
			Character_GhostSlotCommit(this, block, slot_w, slot_h);
		}
		//The dropped snapshot's rect + CLUT go back to the pool intact so
		//a later direction switch reuses them instead of fragmenting
		if (recycled.valid)
		{
			Character_GhostFreePush(this,
				(s16)recycled.atlas_x, (s16)recycled.atlas_y,
				recycled.atlas_words, recycled.atlas_height,
				recycled.tex.tim_crect.x, recycled.tex.tim_crect.y);
		}
	}
	else
	{
		//Copy failed: undo the shift, returning a taken rect
		for (i = 0; i < length - 1; i++)
			this->ghosts.frames[i] = this->ghosts.frames[i + 1];
		if (block < 0)
		{
			Character_GhostFreePush(this, slot_x, slot_y,
				slot_w, slot_h, clut_x, clut_y);
		}
	}
}

static void Character_TrailDrawAndCapture(Character *this, Gfx_Tex *tex,
	const CharFrame *cframe, u8 r, u8 g, u8 b)
{
	u8 i;
	if (this->ghosts_storage == NULL || !this->ghosts.enabled || !this->ghosts.active)
		return;

	//Re-tint live snapshots when overridden or following the health bar
	//(so flipping the bool recolors the whole trail immediately)
	if (this->ghosts.color_override || !this->ghosts.no_healthbar_color)
	{
		for (i = 0; i < this->ghosts.trail_used; i++)
		{
			Character_GhostTint(this, r, g, b,
				&this->ghosts.frames[i].r,
				&this->ghosts.frames[i].g,
				&this->ghosts.frames[i].b);
		}
	}
	Character_TrailDrawStored(this);
	Character_TrailCapture(this, tex, cframe, r, g, b);
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
	if (this->ghosts_storage != NULL && this->ghosts.enabled && this->ghosts.trail_mode)
		Character_TrailDrawAndCapture(this, tex, cframe, 0x80, 0x80, 0x80);
	else
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
	if (this->ghosts_storage != NULL && this->ghosts.enabled && this->ghosts.trail_mode)
		Character_TrailDrawAndCapture(this, tex, cframe, r, g, b);
	else
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
