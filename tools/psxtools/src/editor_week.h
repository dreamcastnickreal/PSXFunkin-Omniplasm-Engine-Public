/*
  psxtools - weekN.c / weekN.h creator module.

  Lets you assemble a complete week: choose characters, chart mapping,
  background, and generate a full weekN.c + weekN.h ready to compile.
*/

#ifndef PSXTOOLS_EDITOR_WEEK_H
#define PSXTOOLS_EDITOR_WEEK_H

#include <gtk/gtk.h>
#include "app.h"

GtkWidget *editor_week_new(App *app);

#endif
