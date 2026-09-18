/*
  psxtools - all-in-one PSXFunkin editor/creator GUI (GTK3).

  Architecture: a main window with a module sidebar (left) and a
  GtkStack of module pages (right). Each tool is a module that
  registers a page factory.
*/

#ifndef PSXTOOLS_APP_H
#define PSXTOOLS_APP_H

#include <gtk/gtk.h>

#include "model.h"
#include "project.h"

typedef struct App App;

// A registered tool module
typedef struct Module
{
	const char *name;         // sidebar label
	const char *icon_name;    // gtk icon name, may be NULL
	GtkWidget *(*factory)(App *app); // returns the module page (a widget)
} Module;

struct App
{
	GtkWidget *window;
	GtkWidget *stack;

	GtkWidget *statusbar;

	// current project (shared across modules). Borrowed pointer: the module
	// that loaded it owns the memory and updates this field when it replaces
	// or frees the project. Other modules must read it without keeping refs.
	Project *project;
};

// Registration of the built-in modules
const Module *app_modules(int *count);

// Small helpers used by modules
void app_set_status(App *app, const char *fmt, ...);

#endif
