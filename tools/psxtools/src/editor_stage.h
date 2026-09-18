/*
  psxtools - stage editor module.

  Imports a weekN.c (+ Makefile) as a project, shows the background
  sprites on a zoomable/panable canvas, lets you move/scale/crop/flip
  each piece, and re-exports WeekN_Load + WeekN_DrawBG with round-trip
  fidelity (port of the python tools/stagegen).
*/

#ifndef PSXTOOLS_EDITOR_STAGE_H
#define PSXTOOLS_EDITOR_STAGE_H

#include <gtk/gtk.h>
#include "app.h"

GtkWidget *editor_stage_new(App *app);

// Emits tex decls + %s_Load + %s_DrawBG for a project (shared with the week
// editor). P/prefix override the generated names; NULL derives them from the
// project. load_extra is appended at the end of Load. Returns malloc'd text.
char *export_graphics_block(Project *pr, const char *P, const char *prefix,
                            const char *load_extra);

// Omniplasm export: full Back_<X>.c (struct + tables + helpers + per-layer
// DrawBG/MD/FG/HUD + Free + New) and its Back_<X>.h. Returns malloc'd text.
char *export_omni_source(Project *pr);
char *export_omni_header(Project *pr);

#endif
