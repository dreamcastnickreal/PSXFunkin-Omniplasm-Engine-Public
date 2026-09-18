/*
  psxtools - PSXFunkin all-in-one editor/creator GUI (GTK3)
  Core model: sprites, geometry, fixed-point helpers.

  Replaces the python tools/stagegen model. Keeps geometry in float
  internally for editing, and converts to/from FIXED_DEC / camera
  parallax expressions on import/export.
*/

#ifndef PSXTOOLS_MODEL_H
#define PSXTOOLS_MODEL_H

#include <stdbool.h>
#include <stdint.h>

// ---- fixed helpers (matches src/boot/fixed.h semantics) ----
#define FXT_FIXED_SHIFT 10
#define FXT_FIXED_UNIT  (1 << FXT_FIXED_SHIFT)

// Draw modes a sprite can use on export
enum DrawMode
{
	DRAW_NONE = 0,
	DRAW_RECT,   // Stage_DrawTex
	DRAW_RECT_COL,
	DRAW_ARB,    // Stage_DrawTexArb
	DRAW_ARB_COL,
	DRAW_ABS,    // Gfx_DrawTex
	DRAW_ABS_COL,
};

// Stage layers (Omniplasm engine: Back_*_DrawBG/DrawMD/DrawFG/DrawHUD).
// Legacy weekN.c projects only use LAYER_BG.
typedef enum StageLayer
{
	LAYER_BG = 0,  // drawn behind characters (DrawBG)
	LAYER_MD,      // middle layer, above chars (DrawMD)
	LAYER_FG,      // foreground (DrawFG)
	LAYER_HUD,     // screen-space HUD (DrawHUD)
} StageLayer;

// A single stage piece (one draw call in WeekN_DrawBG)
typedef struct Sprite
{
	char *name;        // base name (png/tim file base, e.g. "back0")
	char *var_name;    // C variable name (e.g. "week5_tex_back0")
	char *tex_path;    // absolute path to the .png
	char *base;        // texture base name (which .tim/png)

	// source crop rect within texture
	int src[4];        // x, y, w, h

	enum DrawMode draw;
	bool has_color;    // *_col variant
	int color[3];      // r,g,b when has_color

	// geometry. For DRAW_RECT/ABS: x,y top-left, w,h size
	// For DRAW_ARB: arb quad of 4 points; x,y,w,h = bbox
	double x, y, w, h;
	bool flip;         // horizontal mirror (arb)

	// parallax camera multipliers
	double par_x, par_y;
	// keep original parallax C expression strings for round-trip
	char *par_x_expr, *par_y_expr;

	// quad points for DRAW_ARB (centered coords)
	double arb[4][2];
	bool arb_valid;

	// ---- Omniplasm stage format extensions ----
	StageLayer layer;   // which draw function owns this sprite
	char *arc;          // ARC path from IO_Read (e.g. "\\WEEK1\\BACK.ARC;1")
	char *tim;          // TIM file inside the ARC (e.g. "back0.tim")
	char *tex_field;    // Gfx_Tex struct field (e.g. "tex_back0")
	bool blend;         // drawn with a Stage_Blend*/Gfx_Blend* call
	int blend_mode;     // blend mode arg (Stage_BlendTexV2 / Gfx_BlendTex)
	int opacity;        // 0-255, -1 = fully opaque / not specified
	char *cond;         // optional visibility guard, e.g. "stage.song_step >= 784"

	// ---- animated-actor instance (NULL for plain static pieces) ----
	struct AnimSet *anim; // owning set (Project owns the memory)
	int anim_frame;       // preview frame index into anim->frames
	double anim_x, anim_y; // raw draw position (fixed-decoded, excl. offset)
	int anim_inst;        // playback-state slot (0 = classic shared state;
	                      // N > 0 = <Base>_Draw_N suffixed variant state)
	int anim_anim;        // animation index this instance plays (SetAnim)

	// ---- dynamic position (code-driven, e.g. Week4's this->car_x) ----
	// When set, the dst x/y comes from C code, not constants: the editor
	// locks the position and the exporter re-emits these verbatim.
	char *dyn_x, *dyn_y;

	// texture info (optional; editor loads image separately)
	int tex_w, tex_h;
	bool has_image;
} Sprite;

// ---- Omniplasm animated actors (Week4 henchmen pattern) ----
//
// An animated actor is a CharFrame table + Animation table + a
// <Base>_SetFrame sampler + a <Base>_Draw helper, driven through an
// Animatable. Each <Base>_Draw(this, x, y) call site in a DrawBG/MD/FG/HUD
// body is one on-screen instance (edited as a Sprite with anim != NULL).

// One animation frame: TIM slot in the archive + crop + draw offset.
typedef struct AnimFrame
{
	int tex;      // index into the actor's TIM list
	int src[4];   // crop rect within that TIM
	int off[2];   // draw offset, subtracted from the draw position
} AnimFrame;

// One animation: playback speed + frame/control-code sequence.
typedef struct AnimDef
{
	int speed;
	int *seq;        // frame indices and ASCR_* codes (0xFB-0xFF), in order
	int seq_count;
	char *raw;       // verbatim "(const u8[]){...}" text for round-trip
} AnimDef;

typedef struct AnimSet
{
	char *id;          // "hench" (from tex_arc member names)
	char *sym;         // "Henchmen" (symbol fragment from fn names)
	char *fnbase;      // "Week4_Henchmen" (<Base> of SetFrame/Draw fns)
	char *draw_fn;     // "Week4_Henchmen_Draw" (instance call target)
	char *tex_field;   // "tex_hench"
	char *arc_var;     // "arc_hench" (struct IO_Data member)
	char *arc_path;    // "\\WEEK4\\HENCH.ARC;1"
	char **tims;       // TIM names in arc ptr order
	int tim_count;
	char *frame_array; // "henchmen_frame"
	char *anim_array;  // "henchmen_anim"
	char *animatable;  // "hench_animatable"

	AnimFrame *frames;
	int frame_count;
	AnimDef *anims;
	int anim_count;

	// Draw-helper traits (parsed from the <Base>_Draw body).
	int scale_x, scale_y; // dst pixel scale, e.g. 3 for the kitchen hands
	bool use_off;         // helper subtracts cframe->off from the position
	bool flip;            // helper draws with a FlipX call

	// Verbatim sources for fidelity round-trip (tables + helpers).
	char *frames_raw;     // inside of "{ ... }" of the CharFrame table
	char *anims_raw;      // inside of "{ ... }" of the Animation table
	char *setframe_raw;   // full "void <Base>_SetFrame..." function text
	char *drawhelper_raw; // full "void <Base>_Draw..." function text
} AnimSet;

void anim_set_free(AnimSet *a);

// Layer helpers
const char *stage_layer_name(StageLayer layer);   // "bg" | "md" | "fg" | "hud"
StageLayer stage_layer_from_fn(const char *fn);   // "DrawBG" -> LAYER_BG, ...
StageLayer stage_layer_from_name(const char *name); // "bg"/"md"/"fg"/"hud" -> layer

// Renders bbox (lx,ty,rx,by) in centered coords into out[4]
void sprite_bbox(const Sprite *s, double out[4]);

// Convert a C initializer field string to float (handles FIXED_DEC,
// SCREEN_WIDTH, integer, and arithmetic with +/- fx/fy terms).
double fixed_field_to_double(const char *token);

// Format a double as a compact FIXED_DEC(num,den) string. Returns malloc'd.
char *double_to_fixed(double v);

// Parse a camera parallax expression ("camera.x * 5 >> 2", "camera.y >> 1")
// and return the real multiplier. 0.0 if no camera term.
double eval_cam_mult(const char *expr, char axis);

// Format a float camera multiplier as a C expression ("*5 >> 2", "*3 / 2").
// Returns malloc'd string, or NULL if factor is 0.
char *mult_expr(double f);

#endif
