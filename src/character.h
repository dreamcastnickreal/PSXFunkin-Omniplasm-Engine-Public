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

//Per-player VAG sound bank (SPU addresses, 0 = empty slot)
#define CHARACTER_VAG_MAX    8
#define CHARACTER_VAG_DEATH0 0 //First death sound
#define CHARACTER_VAG_DEATH2 1 //Confirm-retry sound
#define CHARACTER_VAG_MISS0  2 //Miss sounds (random pick of the 3 on a miss)
#define CHARACTER_VAG_MISS1  3
#define CHARACTER_VAG_MISS2  4
//Slots 5-7 are spare

#define CHARACTER_GHOST_DEFAULT_COUNT 4
#define CHARACTER_GHOST_MAX_COUNT     8
#define CHARACTER_GHOST_MAX_OPPOSING  3
#define CHARACTER_GHOST_SLOT_WORDS    64
#define CHARACTER_GHOST_SLOT_HEIGHT   256
#define CHARACTER_GHOST_MAX_ATLAS     4
#define CHARACTER_GHOST_MAX_FREE      8

//FlxTrail-style trail defaults (matches `new FlxTrail(dad, null, 4, 24, 0.3, 0.069)`)
//length = cached images, delay = game frames between snapshots,
//alpha/diff = base opacity and per-image opacity step (0-255)
#define CHARACTER_TRAIL_DEFAULT_LENGTH 4
#define CHARACTER_TRAIL_DEFAULT_DELAY  4
#define CHARACTER_TRAIL_DEFAULT_ALPHA  76 //0.3 * 255
#define CHARACTER_TRAIL_DEFAULT_DIFF   17 //0.069 * 255

//Convert an FlxTrail 0.0-1.0 fixed_t alpha/diff to PSX 0-255 (no floats,
//pass FIXED_DEC values, e.g. CHARACTER_TRAIL_FIXED2ALPHA(FIXED_DEC(3,10)))
#define CHARACTER_TRAIL_FIXED2ALPHA(a) ((u8)(((s64)(a) * 255) >> FIXED_SHIFT))

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

//One VRAM atlas block shared by ghost and trail frames. Frames pack into
//blocks in order and spill into a new free block only when all placed
//blocks are full, so a 64x256 block is filled before another is found.
//Loaded frames stay in their blocks until dropped or the system resets.
typedef struct
{
	s16 x, y; //block origin, -1 = unplaced
	u16 pack_x, pack_y, pack_row_height;
	u16 reserve_w, reserve_h; //finder reservation size (for exact Unmark)
	u8 clut_next; //next per-block CLUT index (8 max per block)
	boolean reserved; //true = placed via finder, release with Unmark
} CharacterGhostAtlas;

//Abandoned atlas rect (plus its CLUT) returned by dropped snapshots.
//Reused by later captures so switching animations settles instead of
//fragmenting VRAM into a restart loop.
typedef struct
{
	s16 x, y, clut_x, clut_y;
	u16 w, h;
} CharacterGhostFree;

typedef struct
{
	boolean enabled;
	boolean active;
	boolean continuous;
	boolean secondary_is_main;
	boolean capture_requested;
	boolean color_override;
	boolean no_healthbar_color; //true = never tint from health bar (default)
	u8 color_r, color_g, color_b;
	u8 count, needed_frames, used, head;
	s16 vram_x, vram_y;
	boolean vram_auto; //true = place atlas in free VRAM on first capture
	u16 slot_words, slot_height;
	CharacterGhostAtlas atlases[CHARACTER_GHOST_MAX_ATLAS];
	u8 atlas_count; //placed blocks (block 0 mirrors vram_x/vram_y)
	CharacterGhostFree free_rects[CHARACTER_GHOST_MAX_FREE];
	u8 free_count; //abandoned rects reusable by later captures
	fixed_t request_time;
	u8 requests_at_time;
	u8 requested_anim, opposing_count;
	u8 opposing_anim[CHARACTER_GHOST_MAX_OPPOSING];
	boolean opposing_pending[CHARACTER_GHOST_MAX_OPPOSING];
	Animatable opposing_animatable[CHARACTER_GHOST_MAX_OPPOSING];
	CharacterGhostFrame frames[CHARACTER_GHOST_MAX_COUNT];

	//FlxTrail-style trail settings ("trail", legacy ghost fields above untouched)
	//Trail reuses frames[], the atlas blocks, slot_words/height, active,
	//enabled and color_override above.
	boolean trail_mode;  //true = FlxTrail snapshot trail, false = legacy ghost
	u8 trail_length;     //FlxTrail length (cached images, max CHARACTER_GHOST_MAX_COUNT)
	u8 trail_delay;      //game frames between snapshots (FlxTrail delay, 0 = every frame)
	u8 trail_timer;      //countdown to next snapshot
	u8 trail_alpha;      //base opacity 0-255 (FlxTrail alpha, e.g. 0.3 -> 76)
	u8 trail_diff;       //opacity step per image 0-255 (FlxTrail diff, e.g. 0.069 -> 17)
	u8 trail_used;       //valid cached trail images in frames[]
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
	boolean death_simple; //true = simple Death0/Death1/Death2 scheme (see player.h)
	u16 health_i[2][4];
	u32 health_bar; //hud1.tim
	fixed_t focus_x, focus_y, focus_zoom;

	fixed_t size;
	u8 opacity;
	u32 vag_sounds[CHARACTER_VAG_MAX]; //per-player VAG bank (SPU addrs, 0 = empty)

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
boolean Character_LoadVagSound(Character *this, u8 slot, const char *path);
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
//Pass negative vram_x/vram_y for automatic placement: the atlas is put in
//free VRAM on first capture, avoiding the framebuffers and every HUD,
//character, stage, ARC and uploaded TIM (see Gfx_VramFindFree)
void Character_GhostSetActive(Character *this, boolean active);
void Character_GhostSetContinuous(Character *this, boolean continuous);
void Character_GhostSetSecondaryMain(Character *this, boolean secondary_is_main);
void Character_GhostSetColor(Character *this, u8 r, u8 g, u8 b);
void Character_GhostSetNoHealthbarColor(Character *this, boolean no_healthbar_color);
void Character_GhostAnimationRequest(Character *this, u8 anim);
u8 Character_GhostOpposingCount(Character *this);
boolean Character_GhostAnimateOpposing(Character *this, u8 index, const Animation *anims,
	void *user, void (*set_frame)(void*, u8));
void Character_GhostCaptureNow(Character *this, u8 opposing_index,
	Gfx_Tex *tex, const CharFrame *cframe);

//FlxTrail-style trail functions ("trail", legacy ghost code above is kept as-is)
//Configures the ghost storage for trail snapshots:
//  length = FlxTrail length (cached images)
//  delay  = FlxTrail delay (game frames between snapshots, 0 = every frame)
//  alpha  = FlxTrail alpha as fixed_t base opacity (e.g. FIXED_DEC(3,10) for 0.3)
//  diff   = FlxTrail diff as fixed_t opacity step per image (e.g. FIXED_DEC(69,1000))
//Example (Psych `new FlxTrail(dad, null, 4, 24, 0.3, 0.069)`):
//  Character_TrailConfigure(dad, true, -1, -1, 4, 24,
//      FIXED_DEC(3,10), FIXED_DEC(69,1000));
void Character_TrailConfigure(Character *this, boolean enabled, s16 vram_x, s16 vram_y,
	u8 length, u8 delay, fixed_t alpha, fixed_t diff);
void Character_TrailSetLength(Character *this, u8 length);
void Character_TrailSetDelay(Character *this, u8 delay);
void Character_TrailSetAlpha(Character *this, fixed_t alpha);
void Character_TrailSetDiff(Character *this, fixed_t diff);
void Character_TrailSetActive(Character *this, boolean active);
void Character_TrailClear(Character *this);

void Character_CheckStartSing(Character *this);
void Character_CheckEndSing(Character *this);
void Character_PerformIdle(Character *this);

void Character_CheckStartSing2(Character *this);
void Character_CheckEndSing2(Character *this);
void Character_PerformIdle2(Character *this);

#endif
