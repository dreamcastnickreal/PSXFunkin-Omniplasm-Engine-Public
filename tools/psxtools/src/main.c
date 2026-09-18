/*
  psxtools - all-in-one PSXFunkin editor/creator GUI.

  Main window: a module sidebar on the left + a GtkStack of pages on the
  right. Each tool (stage editor, week creator, future packers) is a
  module registered in app_modules().
*/

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "app.h"
#include "editor_stage.h"
#include "editor_week.h"

void app_set_status(App *app, const char *fmt, ...)
{
	if (!app || !app->statusbar) return;
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	gtk_label_set_text(GTK_LABEL(app->statusbar), buf);
}

const Module *app_modules(int *count)
{
	static Module mods[] = {
		{"Stage editor", "image-x-generic", editor_stage_new},
		{"Week editor (weekN)", "document-new", editor_week_new},
	};
	*count = sizeof(mods) / sizeof(mods[0]);
	return mods;
}

static void on_activate(GtkApplication *gapp, gpointer user)
{
	App *app = user;
	GtkWidget *win = gtk_application_window_new(gapp);
	app->window = win;
	gtk_window_set_title(GTK_WINDOW(win), "psxtools - PSXFunkin editor");
	gtk_window_set_default_size(GTK_WINDOW(win), 1000, 640);

	// sidebar | stack in a draggable paned divider
	int n = 0;
	const Module *mods = app_modules(&n);
	GtkWidget *side = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_size_request(side, 220, -1);
	GtkWidget *list = gtk_stack_sidebar_new();
	gtk_box_pack_start(GTK_BOX(side), list, TRUE, TRUE, 0);

	// stack
	GtkWidget *stack = gtk_stack_new();
	app->stack = stack;
	gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_NONE);
	gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(list), GTK_STACK(stack));

	for (int i = 0; i < n; i++)
	{
		GtkWidget *page = mods[i].factory(app);
		gtk_widget_set_hexpand(page, TRUE);
		gtk_stack_add_named(GTK_STACK(stack), page, mods[i].name);
		{
			GValue v = G_VALUE_INIT;
			g_value_init(&v, G_TYPE_STRING);
			g_value_set_string(&v, mods[i].name);
			gtk_container_child_set_property(GTK_CONTAINER(stack), page, "title", &v);
			g_value_set_string(&v, mods[i].icon_name);
			gtk_container_child_set_property(GTK_CONTAINER(stack), page, "icon-name", &v);
			g_value_unset(&v);
		}
	}

	GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_paned_set_position(GTK_PANED(paned), 220);
	gtk_paned_pack1(GTK_PANED(paned), side, FALSE, FALSE);
	gtk_paned_pack2(GTK_PANED(paned), stack, TRUE, TRUE);

	// status bar at bottom (full width); vbox is the single window child
	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(win), vbox);
	gtk_box_pack_start(GTK_BOX(vbox), paned, TRUE, TRUE, 0);
	app->statusbar = gtk_label_new("Ready.");
	gtk_widget_set_halign(app->statusbar, GTK_ALIGN_START);
	gtk_widget_set_valign(app->statusbar, GTK_ALIGN_START);
	gtk_widget_set_margin_start(app->statusbar, 8);
	gtk_widget_set_margin_bottom(app->statusbar, 4);
	gtk_box_pack_start(GTK_BOX(vbox), app->statusbar, FALSE, FALSE, 0);

	gtk_widget_show_all(win);
}

static void on_shutdown(GApplication *gapp, gpointer user)
{
	(void)gapp;
	App *app = user;
	if (app->project) project_free(app->project);
}

int main(int argc, char **argv)
{
	GtkApplication *gapp = gtk_application_new("com.psxfunkin.psxtools",
	                                           G_APPLICATION_DEFAULT_FLAGS);
	App *app = calloc(1, sizeof(App));
	g_signal_connect(gapp, "activate", G_CALLBACK(on_activate), app);
	g_signal_connect(gapp, "shutdown", G_CALLBACK(on_shutdown), app);
	int status = g_application_run(G_APPLICATION(gapp), argc, argv);
	g_object_unref(gapp);
	free(app);
	return status;
}
