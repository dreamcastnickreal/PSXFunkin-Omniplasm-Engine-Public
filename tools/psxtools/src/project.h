/*
  psxtools - project loading (Makefile + weekN.c) and weekN.c parsing.
*/

#ifndef PSXTOOLS_PROJECT_H
#define PSXTOOLS_PROJECT_H

#include "model.h"

// Project kind: legacy weekN.c (overlay exe) vs Omniplasm src/stage/*.c
// (Back_* struct with DrawBG/DrawMD/DrawFG/DrawHUD + ARC loading).
typedef enum ProjectKind
{
	PROJ_LEGACY_WEEK = 0, // weekN.c + Makefile, WeekN_Load/WeekN_DrawBG
	PROJ_OMNI_STAGE,      // Back_Xxx_New in src/stage/<name>.c
} ProjectKind;

// A loaded stage project
typedef struct Project
{
	ProjectKind kind;
	char *week;        // legacy: "week5" | omni: stage name ("kitchen")
	char *prefix;      // legacy: "Week5" | omni: "Back_Kitchen"
	char *c_path;      // abs path to the weekN.c / stage .c file
	char *project_root;// abs path to repo root (Makefile lives here)
	char *week_dir;    // legacy: abs path to iso/weekN | omni: dir of the .c file
	char *makefile;    // abs path to Makefile (may not exist for omni)

	Sprite **sprites;
	int sprite_count;

	// omni only: animated actors (Week4 henchmen pattern). Project owns them;
	// instance Sprites point at these via Sprite.anim (do not free per-sprite).
	struct AnimSet **anims;
	int anim_count;

	// raw extracted sections for the week editor (filled lazily)
	char *drawbg_body;
	char *drawmd_body;  // omni only (DrawMD)
	char *drawfg_body;  // omni only (DrawFG)
	char *drawhud_body; // omni only (DrawHUD)
	char *new_body;     // omni only (Back_Xxx_New, holds the IO_Read loads)
	char *load_body;
	char *tick_body;
	char *getchart_body;

	char **load_order;      // legacy: C var names in Load order
	                            // omni: tex fields (tex_back0, ...) in New() order
	int load_order_count;

	// omni only: every texture loaded in Back_Xxx_New (even code-driven
	// ones with no static draw call, e.g. animated hands). Parallel arrays.
	char **omni_tex_fields; // "tex_back0"
	char **omni_tex_tims;   // "back0.tim" ("" when unknown)
	char **omni_tex_arcs;   // "\\WEEK1\\BACK.ARC;1" ("" when unknown)
	int omni_tex_count;

	char **deps;            // .tim dependency list from Makefile for this week
	int deps_count;
	char **deps_full;       // full res://-style paths (iso/weekN/backX.tim)
	int deps_full_count;

	// week metadata read from weekN.c source
	char **char_includes;   // "character/bf.c", ... (order as included)
	int char_include_count;
	char **chart_paths;     // "iso/chart/<song>.json.cht.h" (order as included)
	int chart_path_count;

	char *warning;          // non-NULL if Makefile has order-only '|' issues
} Project;

// Load a project from a weekN.c path. Returns NULL on hard failure.
Project *project_load(const char *c_path);
void project_free(Project *p);

// Parse helpers exposed for the week editor
typedef struct WeekModel
{
	char *prefix;
	char *week;
	char **char_includes;   int char_include_count;
	char *gf_macro;         // "CHAR_GF_TUTORIAL" or NULL
	// chars: slot -> fn + x + y
	char *char_fn[3];       // player, opponent, gf
	double char_xy[3][2];
	bool char_used[3];
	double gf_parallax;
	bool has_gf_parallax;
	char **chart_vars;      int chart_var_count;  // e.g. week1_cht_bopeebo_easy
	char **chart_paths;     int chart_path_count; // iso/chart/... path
	char *getchart_src;
	char *setptr[9];        // load/tick/drawbg/drawmd/drawfg/free/getchart/loadscreen/nextstage (NULL = unset)
} WeekModel;

// Extract week model (chars, charts, setptr) from a full weekN.c source.
WeekModel *week_model_parse(const char *source, const char *prefix, const char *week);
void week_model_free(WeekModel *m);

#endif
