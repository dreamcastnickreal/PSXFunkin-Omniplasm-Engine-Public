/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef PSXF_GUARD_CHARACTER_H
#define PSXF_GUARD_CHARACTER_H

#include "io.h"
#include "gfx.h"

#include "fixed.h"
#include "animation.h"

//Character specs
typedef u8 CharSpec;
#define CHAR_SPEC_MISSANIM (1 << 0) //Has miss animations

#define CHARACTER_GHOST_DEFAULT_COUNT 4
#define CHARACTER_GHOST_MAX_COUNT     8
#define CHARACTER_GHOST_MAX_OPPOSING  3
#define CHARACTER_GHOST_SLOT_WORDS    64
#define CHARACTER_GHOST_SLOT_HEIGHT   256

//Character enums
typedef enum
{
	CharAnim_Idle,
	CharAnim_Left,  CharAnim_LeftAlt,
	CharAnim_Down,  CharAnim_DownAlt,
	CharAnim_Up,    CharAnim_UpAlt,
	CharAnim_Right, CharAnim_RightAlt,
	CharAnim_UnGrow, CharAnim_UnShrink,
	CharAnim_Sing,

	CharAnim_Max //Max standard/shared animation
} CharAnim;

//Character structures
typedef struct
{
	u8 tex;
	u16 src[4];
	s16 off[2];
} CharFrame;

typedef struct
{
	Gfx_Tex tex;
	CharFrame frame;
	fixed_t x, y;
	fixed_t drift_x, drift_y, distance;
	u16 atlas_x, atlas_y, atlas_words, atlas_height;
	u8 opposing_index, r, g, b;
	boolean valid, fading;
} CharacterGhostFrame;

typedef struct
{
	boolean enabled;
	boolean active;
	boolean continuous;
	boolean secondary_is_main;
	boolean capture_requested;
	boolean color_override;
	u8 color_r, color_g, color_b;
	u8 count, needed_frames, used, head;
	s16 vram_x, vram_y;
	u16 slot_words, slot_height;
	u16 pack_x, pack_y, pack_row_height;
	fixed_t request_time;
	u8 requests_at_time;
	u8 requested_anim, opposing_count;
	u8 opposing_anim[CHARACTER_GHOST_MAX_OPPOSING];
	boolean opposing_pending[CHARACTER_GHOST_MAX_OPPOSING];
	Animatable opposing_animatable[CHARACTER_GHOST_MAX_OPPOSING];
	CharacterGhostFrame frames[CHARACTER_GHOST_MAX_COUNT];
} CharacterGhosts;

typedef struct Character
{
	//Character functions
	void (*tick)(struct Character*);
	void (*set_anim)(struct Character*, u8);
	void (*free)(struct Character*);

	//Position
	fixed_t x, y;

	//Character information
	CharSpec spec;
	u16 health_i[2][4];
	u32 health_bar; //hud1.tim
	fixed_t focus_x, focus_y, focus_zoom;

	fixed_t size;
	u8 opacity;

	//Animation state
	Animatable animatable;
	Animatable animatable2;
	fixed_t sing_end;
	u16 pad_held;
	CharacterGhosts *ghosts_storage;
} Character;

/*
 * Preserve the concise character->ghosts.member spelling while keeping the
 * large ghost state optional. This expands to character->ghosts_storage[0].
 */
#define ghosts ghosts_storage[0]

//Character functions
void Character_Free(Character *this);
void Character_Init(Character *this, fixed_t x, fixed_t y);
void Character_DrawParallax(Character *this, Gfx_Tex *tex, const CharFrame *cframe, fixed_t parallax);
void Character_DrawParallaxFlipped(Character *this, Gfx_Tex *tex, const CharFrame *cframe, fixed_t parallax);
void Character_Draw(Character *this, Gfx_Tex *tex, const CharFrame *cframe);
void Character_DrawCol(Character *this, Gfx_Tex *tex, const CharFrame *cframe, u8 r, u8 g, u8 b);
void Character_DrawParallaxCol(Character *this, Gfx_Tex *tex, const CharFrame *cframe, fixed_t parallax, u8 r, u8 g, u8 b);
void Character_DrawFlipped(Character *this, Gfx_Tex *tex, const CharFrame *cframe);
void Character_DrawParallaxBlendCol(Character *this, Gfx_Tex *tex, const CharFrame *cframe, fixed_t parallax, u8 r, u8 g, u8 b, u8 mode);
void Character_DrawBlendCol(Character *this, Gfx_Tex *tex, const CharFrame *cframe, u8 r, u8 g, u8 b, u8 mode);
void Character_DrawParallaxBlendCol_Reflection(Character *this, Gfx_Tex *tex, const CharFrame *cframe, fixed_t parallax, u8 r, u8 g, u8 b, u8 mode);
void Character_DrawBlendCol_Reflection(Character *this, Gfx_Tex *tex, const CharFrame *cframe, u8 r, u8 g, u8 b, u8 mode);

void Character_GhostConfigure(Character *this, boolean enabled, s16 vram_x, s16 vram_y, u8 count);
void Character_GhostConfigureSlots(Character *this, u16 slot_words, u16 slot_height);
void Character_GhostSetActive(Character *this, boolean active);
void Character_GhostSetContinuous(Character *this, boolean continuous);
void Character_GhostSetSecondaryMain(Character *this, boolean secondary_is_main);
void Character_GhostSetColor(Character *this, u8 r, u8 g, u8 b);
void Character_GhostAnimationRequest(Character *this, u8 anim);
u8 Character_GhostOpposingCount(Character *this);
boolean Character_GhostAnimateOpposing(Character *this, u8 index, const Animation *anims,
	void *user, void (*set_frame)(void*, u8));
void Character_GhostCaptureNow(Character *this, u8 opposing_index,
	Gfx_Tex *tex, const CharFrame *cframe);

void Character_CheckStartSing(Character *this);
void Character_CheckEndSing(Character *this);
void Character_PerformIdle(Character *this);

void Character_CheckStartSing2(Character *this);
void Character_CheckEndSing2(Character *this);
void Character_PerformIdle2(Character *this);

#endif
