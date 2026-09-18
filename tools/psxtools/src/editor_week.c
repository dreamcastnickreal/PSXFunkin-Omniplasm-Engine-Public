/*
  psxtools - weekN.c / weekN.h creator module.

  v2: editable song/chart list and character list, integrated with the
  stage editor (shares app->project). Generates a full weekN.c: chart
  tables, character includes, and (when a stage project is known) the
  texture Load + DrawBG from the stage editor's exported graphics block.

  Model per week:
    - a song holds up to 3 chart file paths (easy/normal/hard slots).
    - a character is an include path ("character/bf.c").
  The week number spin drives generated symbol names (weekN / WeekN).
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "editor_week.h"
#include "editor_stage.h"

#define MAX_DIFFS 3

typedef struct EWChar
{
	char *path;   // "character/bf.c"
} EWChar;

typedef struct EWSong
{
	char *name;   // display + var base name
	char *file[MAX_DIFFS]; // chart file paths ("" = empty slot)
} EWSong;

typedef struct EditorWeek
{
	App *app;
	GtkWidget *week_num;      // spin: week number
	GtkWidget *song_list;     // GtkListBox of songs
	GtkWidget *char_list;     // GtkListBox of characters
	GtkWidget *prop_stack;    // GtkStack: "song" | "character"
	GtkWidget *song_name;     // entry
	GtkWidget *dif_ent[MAX_DIFFS];
	GtkWidget *char_path;     // entry
	GtkWidget *out;
	GtkWidget *status;

	EWSong *songs;
	int song_count;
	EWChar *chars;
	int char_count;
	int sel_song, sel_char;
	gboolean populating;      // guard: programmatic entry updates

	Project *proj;            // own-loaded weekN.c project (may be NULL)
} EditorWeek;

static const char *MPL_HEADER =
"/*\n"
"  This Source Code Form is subject to the terms of the Mozilla Public\n"
"  License, v. 2.0. If a copy of the MPL was not distributed with this\n"
"  file, You can obtain one at http://mozilla.org/MPL/2.0/.\n"
"*/\n";

static const char *DIFF_LABEL[MAX_DIFFS] = {"easy", "normal", "hard"};
static const char *DIFF_FILE_SUFFIX[MAX_DIFFS] = {"-easy", "-normal", "-hard"};

static void gen_week(EditorWeek *ew);
static void rebuild_song_list(EditorWeek *ew);
static void rebuild_char_list(EditorWeek *ew);
static void populate_song_props(EditorWeek *ew);
static void populate_char_props(EditorWeek *ew);

// -------------------------------------------------------------- helpers

static char *xstrdup2(const char *s)
{
	return s ? g_strdup(s) : g_strdup("");
}

static void sanitize_var(const char *src, char *out, size_t n)
{
	size_t o = 0;
	for (const char *c = src; *c && o + 1 < n; c++)
	{
		unsigned char ch = (unsigned char)*c;
		out[o++] = (isalnum(ch) || ch == '_') ? (char)ch : '_';
	}
	out[o] = 0;
	if (o == 0) { snprintf(out, n, "song"); return; }
	if (!isalpha((unsigned char)out[0]))
	{
		memmove(out + 4, out, o + 1);
		memcpy(out, "sng_", 4);
	}
}

// Split an iso/chart/... path into (song base var, difficulty 0..2).
static void split_chart(const char *path, char *songbuf, size_t sn, int *diff)
{
	static const char suf[] = ".json.cht.h";
	size_t sl = sizeof(suf) - 1;
	*diff = 1;
	const char *base = strrchr(path, '/');
	base = base ? base + 1 : path;
	size_t len = strlen(base);
	if (len > sl && strcmp(base + len - sl, suf) == 0) len -= sl;
	char name[512];
	snprintf(name, sizeof(name), "%.*s", (int)(len < sizeof(name) - 1 ? len : sizeof(name) - 1), base);
	for (int k = 0; k < MAX_DIFFS; k++)
	{
		size_t dl = strlen(DIFF_FILE_SUFFIX[k]);
		size_t nl = strlen(name);
		if (nl > dl && strcmp(name + nl - dl, DIFF_FILE_SUFFIX[k]) == 0)
		{
			name[nl - dl] = 0;
			*diff = k;
			break;
		}
	}
	snprintf(songbuf, sn, "%s", name);
}

static void char_base_name(const char *path, char *out, size_t n)
{
	const char *base = strrchr(path, '/');
	base = base ? base + 1 : path;
	snprintf(out, n, "%s", base);
	char *dot = strrchr(out, '.');
	if (dot) *dot = 0;
}

static int char_has(const EditorWeek *ew, const char *lower_base)
{
	for (int i = 0; i < ew->char_count; i++)
	{
		char b[128];
		char_base_name(ew->chars[i].path, b, sizeof(b));
		for (char *c = b; *c; c++) *c = (char)tolower((unsigned char)*c);
		if (strcmp(b, lower_base) == 0) return 1;
	}
	return 0;
}

static void song_remove(EditorWeek *ew, int i)
{
	if (i < 0 || i >= ew->song_count) return;
	EWSong *so = &ew->songs[i];
	g_free(so->name);
	for (int k = 0; k < MAX_DIFFS; k++) g_free(so->file[k]);
	memmove(&ew->songs[i], &ew->songs[i + 1], sizeof(EWSong) * (size_t)(ew->song_count - i - 1));
	ew->song_count--;
}

static void char_remove(EditorWeek *ew, int i)
{
	if (i < 0 || i >= ew->char_count) return;
	g_free(ew->chars[i].path);
	memmove(&ew->chars[i], &ew->chars[i + 1], sizeof(EWChar) * (size_t)(ew->char_count - i - 1));
	ew->char_count--;
}

// ------------------------------------------------------------ UI refresh

static void refresh_status(EditorWeek *ew)
{
	Project *gpr = ew->proj ? ew->proj : ew->app->project;
	char msg[512];
	if (gpr && gpr->sprite_count > 0)
		snprintf(msg, sizeof(msg),
			"Embedded stage from %s (%d sprites) | %d song(s), %d character(s). "
			"Song = 3 chart slots (e/n/h); NULL = empty slot.",
			gpr->week, gpr->sprite_count, ew->song_count, ew->char_count);
	else
		snprintf(msg, sizeof(msg),
			"%d song(s), %d character(s). Open a weekN.c (or the stage) to "
			"embed textures+DrawBG into the output.",
			ew->song_count, ew->char_count);
	gtk_label_set_text(GTK_LABEL(ew->status), msg);
}

static void rebuild_song_list(EditorWeek *ew)
{
	GtkListBox *lb = GTK_LIST_BOX(ew->song_list);
	ew->populating = TRUE;
	GList *kids = gtk_container_get_children(GTK_CONTAINER(lb));
	for (GList *l = kids; l; l = l->next)
		gtk_widget_destroy(GTK_WIDGET(l->data));
	g_list_free(kids);
	if (ew->song_count == 0)
	{
		GtkWidget *lbl = gtk_label_new("(no songs)");
		gtk_widget_set_halign(lbl, GTK_ALIGN_START);
		gtk_widget_set_sensitive(lbl, FALSE);
		gtk_list_box_insert(lb, lbl, -1);
		gtk_widget_show_all(GTK_WIDGET(lb));
		ew->populating = FALSE;
		return;
	}
	for (int i = 0; i < ew->song_count; i++)
	{
		EWSong *so = &ew->songs[i];
		int filled = 0;
		for (int k = 0; k < MAX_DIFFS; k++)
			if (so->file[k] && so->file[k][0]) filled++;
		char label[256];
		snprintf(label, sizeof(label), "%s  (%d/%d)", so->name ? so->name : "", filled, MAX_DIFFS);
		GtkWidget *row = gtk_list_box_row_new();
		GtkWidget *lbl = gtk_label_new(label);
		gtk_widget_set_halign(lbl, GTK_ALIGN_START);
		gtk_widget_set_hexpand(lbl, TRUE);
		gtk_container_add(GTK_CONTAINER(row), lbl);
		gtk_list_box_insert(lb, row, -1);
	}
	gtk_widget_show_all(GTK_WIDGET(lb));
	if (ew->sel_song >= 0 && ew->sel_song < ew->song_count)
		gtk_list_box_select_row(lb, gtk_list_box_get_row_at_index(lb, ew->sel_song));
	ew->populating = FALSE;
}

static void rebuild_char_list(EditorWeek *ew)
{
	GtkListBox *lb = GTK_LIST_BOX(ew->char_list);
	ew->populating = TRUE;
	GList *kids = gtk_container_get_children(GTK_CONTAINER(lb));
	for (GList *l = kids; l; l = l->next)
		gtk_widget_destroy(GTK_WIDGET(l->data));
	g_list_free(kids);
	if (ew->char_count == 0)
	{
		GtkWidget *lbl = gtk_label_new("(no characters)");
		gtk_widget_set_halign(lbl, GTK_ALIGN_START);
		gtk_widget_set_sensitive(lbl, FALSE);
		gtk_list_box_insert(lb, lbl, -1);
		gtk_widget_show_all(GTK_WIDGET(lb));
		ew->populating = FALSE;
		return;
	}
	for (int i = 0; i < ew->char_count; i++)
	{
		char b[128];
		char_base_name(ew->chars[i].path, b, sizeof(b));
		GtkWidget *row = gtk_list_box_row_new();
		GtkWidget *lbl = gtk_label_new(b);
		gtk_widget_set_halign(lbl, GTK_ALIGN_START);
		gtk_widget_set_hexpand(lbl, TRUE);
		gtk_container_add(GTK_CONTAINER(row), lbl);
		gtk_list_box_insert(lb, row, -1);
	}
	gtk_widget_show_all(GTK_WIDGET(lb));
	if (ew->sel_char >= 0 && ew->sel_char < ew->char_count)
		gtk_list_box_select_row(lb, gtk_list_box_get_row_at_index(lb, ew->sel_char));
	ew->populating = FALSE;
}

static void populate_song_props(EditorWeek *ew)
{
	ew->populating = TRUE;
	if (ew->sel_song >= 0 && ew->sel_song < ew->song_count)
	{
		EWSong *so = &ew->songs[ew->sel_song];
		gtk_entry_set_text(GTK_ENTRY(ew->song_name), so->name ? so->name : "");
		for (int k = 0; k < MAX_DIFFS; k++)
			gtk_entry_set_text(GTK_ENTRY(ew->dif_ent[k]), so->file[k] ? so->file[k] : "");
		gtk_widget_set_sensitive(ew->song_name, TRUE);
		for (int k = 0; k < MAX_DIFFS; k++) gtk_widget_set_sensitive(ew->dif_ent[k], TRUE);
	}
	else
	{
		gtk_entry_set_text(GTK_ENTRY(ew->song_name), "");
		for (int k = 0; k < MAX_DIFFS; k++) gtk_entry_set_text(GTK_ENTRY(ew->dif_ent[k]), "");
		gtk_widget_set_sensitive(ew->song_name, FALSE);
		for (int k = 0; k < MAX_DIFFS; k++) gtk_widget_set_sensitive(ew->dif_ent[k], FALSE);
	}
	ew->populating = FALSE;
}

static void populate_char_props(EditorWeek *ew)
{
	ew->populating = TRUE;
	if (ew->sel_char >= 0 && ew->sel_char < ew->char_count)
	{
		gtk_entry_set_text(GTK_ENTRY(ew->char_path), ew->chars[ew->sel_char].path);
		gtk_widget_set_sensitive(ew->char_path, TRUE);
	}
	else
	{
		gtk_entry_set_text(GTK_ENTRY(ew->char_path), "");
		gtk_widget_set_sensitive(ew->char_path, FALSE);
	}
	ew->populating = FALSE;
}

// ------------------------------------------------------------- generators

static void append_char_load(EditorWeek *ew, GString *g)
{
	g_string_append(g, "\n\t//Load characters\n");
	int has_bf = char_has(ew, "bf");
	int has_dad = char_has(ew, "dad");
	int has_gf = char_has(ew, "gf");
	if (has_bf)
		g_string_append(g, "\tstage.player = Char_BF_New(FIXED_DEC(60,1), FIXED_DEC(100,1));\n");
	if (has_gf)
	{
		int wn = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ew->week_num));
		if (wn < 1 || wn > 9) wn = 1;
		g_string_append_printf(g,
			"\tif (stage.stage_id == StageId_%d_1)\n\t{\n"
			"\t\tstage.opponent = Char_GF_New(FIXED_DEC(0,1), FIXED_DEC(-10,1));\n"
			"\t\tstage.gf = NULL;\n\t}\n\telse\n\t{\n", wn);
		if (has_dad)
			g_string_append(g, "\t\tstage.opponent = Char_Dad_New(FIXED_DEC(-120,1), FIXED_DEC(100,1));\n");
		g_string_append(g, "\t\tstage.gf = Char_GF_New(FIXED_DEC(0,1), FIXED_DEC(-10,1));\n\t}\n");
	}
}

static void gen_week(EditorWeek *ew)
{
	int wn = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ew->week_num));
	if (wn < 1 || wn > 9) wn = 1;
	char week[16], Week[16];
	snprintf(week, sizeof(week), "week%d", wn);
	snprintf(Week, sizeof(Week), "Week%d", wn);

	GString *g = g_string_new(NULL);
	g_string_append(g, MPL_HEADER);
	g_string_append_printf(g, "\n#include \"%s.h\"\n\n", week);
	g_string_append(g,
		"#include \"boot/stage.h\"\n"
		"#include \"boot/archive.h\"\n"
		"#include \"boot/main.h\"\n"
		"#include \"boot/mem.h\"\n\n");

	// ---- charts ----
	g_string_append(g, "//Charts\n");
	GHashTable *done = g_hash_table_new(g_str_hash, g_str_equal);
	for (int i = 0; i < ew->song_count; i++)
	{
		EWSong *so = &ew->songs[i];
		for (int k = 0; k < MAX_DIFFS; k++)
		{
			if (!so->file[k] || !so->file[k][0]) continue;
			char sb[128]; int diff;
			split_chart(so->file[k], sb, sizeof(sb), &diff);
			char svar[160];
			sanitize_var(sb, svar, sizeof(svar));
			char var[256];
			snprintf(var, sizeof(var), "%s_cht_%s_%s", week, svar, DIFF_LABEL[diff]);
			if (g_hash_table_contains(done, var)) continue;
			g_hash_table_add(done, g_strdup(var));
			g_string_append_printf(g, "static u8 %s[] = {\n\t#include \"%s\"\n};\n", var, so->file[k]);
		}
	}
	g_string_append(g, "\n");
	if (ew->song_count > 0)
	{
		g_string_append_printf(g, "static IO_Data %s_cht[][3] = {\n", week);
		for (int i = 0; i < ew->song_count; i++)
		{
			EWSong *so = &ew->songs[i];
			g_string_append(g, "\t{\n");
			for (int k = 0; k < MAX_DIFFS; k++)
			{
				if (so->file[k] && so->file[k][0])
				{
					char sb[128]; int diff;
					split_chart(so->file[k], sb, sizeof(sb), &diff);
					char svar[160];
					sanitize_var(sb, svar, sizeof(svar));
					g_string_append_printf(g, "\t\t(IO_Data)%s_cht_%s_%s,\n", week, svar, DIFF_LABEL[diff]);
				}
				else
					g_string_append(g, "\t\tNULL,\n");
			}
			g_string_append(g, "\t},\n");
		}
		g_string_append_printf(g, "};\n\n");
	}
	else
		g_string_append(g, "// (no songs)\n\n");

	// ---- characters ----
	g_string_append(g, "//Characters\n");
	for (int i = 0; i < ew->char_count; i++)
	{
		if (ew->chars[i].path && ew->chars[i].path[0])
			g_string_append_printf(g, "#include \"%s\"\n", ew->chars[i].path);
	}
	g_string_append(g, "\n");

	// ---- graphics (tex decls + Load + DrawBG) ----
	GString *ce = g_string_new(NULL);
	append_char_load(ew, ce);
	Project *gpr = ew->proj ? ew->proj : ew->app->project;
	if (gpr && gpr->sprite_count > 0)
	{
		char *blk = export_graphics_block(gpr, Week, week, ce->str);
		if (blk)
		{
			g_string_append(g, blk);
			g_free(blk);
		}
	}
	else
	{
		g_string_append_printf(g, "//Week %d textures\n", wn);
		g_string_append_printf(g, "static Gfx_Tex %s_tex_back0;\n\n", week);
		g_string_append_printf(g, "static void %s_Load(void)\n{\n"
			"\t//Load assets\n\tIO_Data overlay_data;\n\n"
			"\tGfx_LoadTex(&stage.tex_hud0, overlay_data = Overlay_DataRead(), 0); Mem_Free(overlay_data); //hud0.tim\n"
			"\tGfx_LoadTex(&stage.tex_hud1, overlay_data = Overlay_DataRead(), 0); Mem_Free(overlay_data); //hud1.tim\n"
			"\tGfx_LoadTex(&stage.tex_hud_ice, overlay_data = Overlay_DataRead(), 0); Mem_Free(overlay_data); //ice_sheet.tim\n"
			"\tGfx_LoadTex(&%s_tex_back0, overlay_data = Overlay_DataRead(), 0); Mem_Free(overlay_data); //back0.tim\n%s"
			"}\n\n", Week, week, ce->str);
		g_string_append_printf(g, "static void %s_DrawBG(void)\n{\n"
			"\tfixed_t fx, fy;\n"
			"\tfx = stage.camera.x;\n"
			"\tfy = stage.camera.y;\n\n"
			"\tRECT back_src = {0, 0, 255, 255};\n"
			"\tRECT_FIXED back_dst = {\n"
			"\t\tFIXED_DEC(-185,1) - fx,\n"
			"\t\tFIXED_DEC(-125,1) - fy,\n"
			"\t\tFIXED_DEC(353,1),\n"
			"\t\tFIXED_DEC(267,1)\n\t};\n\n"
			"\tStage_DrawTex(&%s_tex_back0, &back_src, &back_dst, stage.camera.bzoom);\n"
			"}\n", Week, week);
	}
	g_string_free(ce, TRUE);
	g_string_append(g, "\n");

	// ---- GetChart ----
	g_string_append_printf(g, "static IO_Data %s_GetChart(void)\n{\n", Week);
	if (ew->song_count > 0)
		g_string_append_printf(g, "\treturn %s_cht[stage.stage_id - StageId_%d_1][stage.stage_diff];\n", week, wn);
	else
		g_string_append(g, "\treturn NULL;\n");
	g_string_append(g, "}\n\n");

	g_string_append_printf(g, "static boolean %s_LoadScreen(void)\n{\n\treturn false;\n}\n\n", Week);

	// ---- NextStage ----
	g_string_append_printf(g, "static boolean %s_NextStage(void)\n{\n", Week);
	g_string_append(g, "\tswitch (stage.stage_id)\n\t{\n");
	if (ew->song_count > 1)
	{
		for (int i = 0; i < ew->song_count - 1; i++)
		{
			const char *nm = ew->songs[i].name ? ew->songs[i].name : "";
			g_string_append_printf(g,
				"\t\tcase StageId_%d_%d: //%s\n"
				"\t\t\tstage.stage_id = StageId_%d_%d;\n"
				"\t\t\treturn true;\n", wn, i + 1, nm, wn, i + 2);
		}
	}
	if (ew->song_count > 0)
	{
		const char *nm = ew->songs[ew->song_count - 1].name ? ew->songs[ew->song_count - 1].name : "";
		g_string_append_printf(g, "\t\tcase StageId_%d_%d: //%s\n\t\t\treturn false;\n",
			wn, ew->song_count, nm);
	}
	g_string_append(g, "\t\tdefault:\n\t\t\treturn false;\n\t}\n}\n\n");

	// ---- SetPtr ----
	g_string_append_printf(g, "void %s_SetPtr(void)\n{\n", Week);
	g_string_append(g, "\t//Set pointers\n");
	g_string_append_printf(g,
		"\tstageoverlay_load = %s_Load;\n"
		"\tstageoverlay_tick = NULL;\n"
		"\tstageoverlay_drawbg = %s_DrawBG;\n"
		"\tstageoverlay_drawmd = NULL;\n"
		"\tstageoverlay_drawfg = NULL;\n"
		"\tstageoverlay_free = NULL;\n"
		"\tstageoverlay_getchart = %s_GetChart;\n"
		"\tstageoverlay_loadscreen = %s_LoadScreen;\n"
		"\tstageoverlay_nextstage = %s_NextStage;\n"
		"}\n", Week, Week, Week, Week, Week);

	gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(ew->out)),
	                         g->str, (gint)g->len);
	g_string_free(g, TRUE);
	g_hash_table_destroy(done);
}

// ---------------------------------------------------- load from a project

static void load_from_project(EditorWeek *ew, Project *pr)
{
	/* charts: group by (song, slot) */
	for (int i = 0; i < pr->chart_path_count; i++)
	{
		const char *path = pr->chart_paths[i];
		char sb[128]; int diff;
		split_chart(path, sb, sizeof(sb), &diff);
		/* find existing song with this name */
		int songix = -1;
		for (int j = 0; j < ew->song_count; j++)
			if (strcmp(ew->songs[j].name, sb) == 0) { songix = j; break; }
		if (songix < 0)
		{
			ew->songs = g_realloc(ew->songs, sizeof(EWSong) * (size_t)(ew->song_count + 1));
			memset(&ew->songs[ew->song_count], 0, sizeof(EWSong));
			ew->songs[ew->song_count].name = g_strdup(sb);
			for (int k = 0; k < MAX_DIFFS; k++) ew->songs[ew->song_count].file[k] = g_strdup("");
			songix = ew->song_count;
			ew->song_count++;
		}
		int slot = (diff >= 0 && diff < MAX_DIFFS) ? diff : 1;
		g_free(ew->songs[songix].file[slot]);
		ew->songs[songix].file[slot] = g_strdup(path);
	}
	/* characters */
	for (int i = 0; i < pr->char_include_count; i++)
	{
		ew->chars = g_realloc(ew->chars, sizeof(EWChar) * (size_t)(ew->char_count + 1));
		ew->chars[ew->char_count].path = g_strdup(pr->char_includes[i]);
		ew->char_count++;
	}
	if (pr->week && strlen(pr->week) >= 5 && pr->week[0] == 'w' &&
	    pr->week[4] >= '1' && pr->week[4] <= '9')
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(ew->week_num), pr->week[4] - '0');
	ew->sel_song = 0;
	ew->sel_char = 0;
}

static void on_open(EditorWeek *ew)
{
	GtkWidget *dlg = gtk_file_chooser_dialog_new(
		"Open weekN.c...", GTK_WINDOW(ew->app->window),
		GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL,
		"_Open", GTK_RESPONSE_ACCEPT, NULL);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), "src");
	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT)
	{
		char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
		Project *pr = project_load(path);
		if (pr)
		{
			if (ew->proj) project_free(ew->proj);
			ew->proj = pr;
			for (int i = ew->song_count; i > 0; i--) song_remove(ew, 0);
			for (int i = ew->char_count; i > 0; i--) char_remove(ew, 0);
			load_from_project(ew, pr);
			rebuild_song_list(ew);
			rebuild_char_list(ew);
			populate_song_props(ew);
			populate_char_props(ew);
			gtk_stack_set_visible_child_name(GTK_STACK(ew->prop_stack), "song");
			refresh_status(ew);
			gen_week(ew);
		}
		else
			gtk_label_set_text(GTK_LABEL(ew->status),
				"Could not parse weekN.c.");
		g_free(path);
	}
	gtk_widget_destroy(dlg);
}

// --------------------------------------------------------------- input

static void on_week_changed(GtkSpinButton *sp, gpointer user)
{
	(void)sp;
	gen_week(user);
}

static void on_name_changed(GtkEditable *e, gpointer user)
{
	EditorWeek *ew = user;
	if (ew->populating) return;
	if (ew->sel_song >= 0 && ew->sel_song < ew->song_count)
	{
		g_free(ew->songs[ew->sel_song].name);
		ew->songs[ew->sel_song].name = xstrdup2(gtk_entry_get_text(GTK_ENTRY(e)));
		rebuild_song_list(ew);
	}
	gen_week(ew);
}

static void on_diff_changed(GtkEditable *e, gpointer user)
{
	EditorWeek *ew = user;
	if (ew->populating) return;
	int k = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(e), "diffk"));
	if (ew->sel_song >= 0 && ew->sel_song < ew->song_count)
	{
		g_free(ew->songs[ew->sel_song].file[k]);
		ew->songs[ew->sel_song].file[k] = xstrdup2(gtk_entry_get_text(GTK_ENTRY(e)));
		rebuild_song_list(ew);
	}
	gen_week(ew);
}

static void on_char_changed(GtkEditable *e, gpointer user)
{
	EditorWeek *ew = user;
	if (ew->populating) return;
	if (ew->sel_char >= 0 && ew->sel_char < ew->char_count)
	{
		g_free(ew->chars[ew->sel_char].path);
		ew->chars[ew->sel_char].path = xstrdup2(gtk_entry_get_text(GTK_ENTRY(e)));
		rebuild_char_list(ew);
	}
	gen_week(ew);
}

static void on_browse_diff(GtkButton *b, gpointer user)
{
	EditorWeek *ew = user;
	int k = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "diffk"));
	if (ew->sel_song < 0 || ew->sel_song >= ew->song_count) return;
	GtkWidget *dlg = gtk_file_chooser_dialog_new(
		"Select chart (.cht.h)", GTK_WINDOW(ew->app->window),
		GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL,
		"_Open", GTK_RESPONSE_ACCEPT, NULL);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), "iso/chart");
	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT)
	{
		char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
		char rel[1024];
		const char *root = ew->proj ? ew->proj->project_root : ew->app->project ? ew->app->project->project_root : NULL;
		if (root && strstr(path, root) == path && path[strlen(root)] == '/')
			snprintf(rel, sizeof(rel), "iso/chart/%s", strrchr(path, '/') + 1);
		else
			snprintf(rel, sizeof(rel), "%s", path);
		gtk_entry_set_text(GTK_ENTRY(ew->dif_ent[k]), rel);
		g_free(path);
	}
	gtk_widget_destroy(dlg);
}

static void on_browse_char(GtkButton *b, gpointer user)
{
	(void)b;
	EditorWeek *ew = user;
	if (ew->sel_char < 0 || ew->sel_char >= ew->char_count) return;
	GtkWidget *dlg = gtk_file_chooser_dialog_new(
		"Select character (.c)", GTK_WINDOW(ew->app->window),
		GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL,
		"_Open", GTK_RESPONSE_ACCEPT, NULL);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), "src/character");
	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT)
	{
		char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
		char rel[1024];
		const char *root = ew->app->project ? ew->app->project->project_root : NULL;
		if (root && strstr(path, root) == path && path[strlen(root)] == '/')
			snprintf(rel, sizeof(rel), "character/%s", strrchr(path, '/') + 1);
		else
			snprintf(rel, sizeof(rel), "%s", path);
		gtk_entry_set_text(GTK_ENTRY(ew->char_path), rel);
		g_free(path);
	}
	gtk_widget_destroy(dlg);
}

static void song_list_selected(GtkListBox *box, GtkListBoxRow *row, gpointer user)
{
	(void)box;
	EditorWeek *ew = user;
	if (ew->populating) return;
	int idx = row ? gtk_list_box_row_get_index(row) : -1;
	if (idx < 0 || idx >= ew->song_count) idx = -1;
	ew->sel_song = idx;
	populate_song_props(ew);
	gtk_stack_set_visible_child_name(GTK_STACK(ew->prop_stack), "song");
}

static void char_list_selected(GtkListBox *box, GtkListBoxRow *row, gpointer user)
{
	(void)box;
	EditorWeek *ew = user;
	if (ew->populating) return;
	int idx = row ? gtk_list_box_row_get_index(row) : -1;
	if (idx < 0 || idx >= ew->char_count) idx = -1;
	ew->sel_char = idx;
	populate_char_props(ew);
	gtk_stack_set_visible_child_name(GTK_STACK(ew->prop_stack), "character");
}

static void on_add_song(EditorWeek *ew)
{
	ew->songs = g_realloc(ew->songs, sizeof(EWSong) * (size_t)(ew->song_count + 1));
	memset(&ew->songs[ew->song_count], 0, sizeof(EWSong));
	char nm[32];
		snprintf(nm, sizeof(nm), "song%d", ew->song_count + 1);
	ew->songs[ew->song_count].name = g_strdup(nm);
	for (int k = 0; k < MAX_DIFFS; k++) ew->songs[ew->song_count].file[k] = g_strdup("");
	ew->song_count++;
	ew->sel_song = ew->song_count - 1;
	ew->sel_char = -1;
	gtk_stack_set_visible_child_name(GTK_STACK(ew->prop_stack), "song");
	rebuild_song_list(ew);
	populate_song_props(ew);
	refresh_status(ew);
	gen_week(ew);
}

static void on_del_song(EditorWeek *ew)
{
	song_remove(ew, ew->sel_song);
	if (ew->sel_song >= ew->song_count) ew->sel_song = ew->song_count - 1;
	if (ew->sel_song >= 0) populate_song_props(ew);
	rebuild_song_list(ew);
	refresh_status(ew);
	gen_week(ew);
}

static void on_song_move(GtkButton *b, gpointer user)
{
	EditorWeek *ew = user;
	int delta = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "delta"));
	if (ew->sel_song < 0) return;
	int j = ew->sel_song + delta;
	if (j < 0 || j >= ew->song_count) return;
	EWSong t = ew->songs[ew->sel_song];
	ew->songs[ew->sel_song] = ew->songs[j];
	ew->songs[j] = t;
	ew->sel_song = j;
	gtk_stack_set_visible_child_name(GTK_STACK(ew->prop_stack), "song");
	rebuild_song_list(ew);
	populate_song_props(ew);
	gen_week(ew);
}

static void on_add_char(EditorWeek *ew)
{
	ew->chars = g_realloc(ew->chars, sizeof(EWChar) * (size_t)(ew->char_count + 1));
	ew->chars[ew->char_count].path = g_strdup("character/gf.c");
	ew->char_count++;
	ew->sel_char = ew->char_count - 1;
	ew->sel_song = -1;
	gtk_stack_set_visible_child_name(GTK_STACK(ew->prop_stack), "character");
	rebuild_char_list(ew);
	populate_char_props(ew);
	refresh_status(ew);
	gen_week(ew);
}

static void on_del_char(EditorWeek *ew)
{
	char_remove(ew, ew->sel_char);
	if (ew->sel_char >= ew->char_count) ew->sel_char = ew->char_count - 1;
	if (ew->sel_char >= 0) populate_char_props(ew);
	rebuild_char_list(ew);
	refresh_status(ew);
	gen_week(ew);
}

static void on_generate(EditorWeek *ew)
{
	gen_week(ew);
}

static void on_save(EditorWeek *ew)
{
	GtkWidget *dlg = gtk_file_chooser_dialog_new(
		"Save weekN.c", GTK_WINDOW(ew->app->window),
		GTK_FILE_CHOOSER_ACTION_SAVE, "_Cancel", GTK_RESPONSE_CANCEL,
		"_Save", GTK_RESPONSE_ACCEPT, NULL);
	int wn = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ew->week_num));
	if (wn < 1 || wn > 9) wn = 1;
	char def[32];
	snprintf(def, sizeof(def), "week%d.c", wn);
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), def);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), "src");
	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT)
	{
		char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
		GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ew->out));
		GtkTextIter s, e;
		gtk_text_buffer_get_bounds(buf, &s, &e);
		char *text = gtk_text_buffer_get_text(buf, &s, &e, FALSE);
		FILE *f = fopen(path, "w");
		if (f) { fputs(text, f); fclose(f); }
		g_free(text);
		g_free(path);
	}
	gtk_widget_destroy(dlg);
}

// ----------------------------------------------------------------- module

GtkWidget *editor_week_new(App *app)
{
	EditorWeek *ew = calloc(1, sizeof(EditorWeek));
	ew->app = app;
	ew->sel_song = -1;
	ew->sel_char = -1;

	/* defaults: one empty song + bf/dad/gf */
	ew->songs = calloc(1, sizeof(EWSong));
	ew->songs[0].name = g_strdup("song1");
	for (int k = 0; k < MAX_DIFFS; k++) ew->songs[0].file[k] = g_strdup("");
	ew->song_count = 1;
	ew->chars = calloc(3, sizeof(EWChar));
	ew->chars[0].path = g_strdup("character/bf.c");
	ew->chars[1].path = g_strdup("character/dad.c");
	ew->chars[2].path = g_strdup("character/gf.c");
	ew->char_count = 3;

	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_size_request(vbox, 980, 640);

	GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(vbox), bar, FALSE, FALSE, 6);

	gtk_box_pack_start(GTK_BOX(bar), gtk_label_new("Week number:"), FALSE, FALSE, 0);
	ew->week_num = gtk_spin_button_new_with_range(1, 9, 1);
	g_signal_connect(ew->week_num, "value-changed", G_CALLBACK(on_week_changed), ew);
	gtk_box_pack_start(GTK_BOX(bar), ew->week_num, FALSE, FALSE, 0);

	GtkWidget *b_open = gtk_button_new_with_label("Load weekN.c...");
	g_signal_connect_swapped(b_open, "clicked", G_CALLBACK(on_open), ew);
	gtk_box_pack_start(GTK_BOX(bar), b_open, FALSE, FALSE, 0);

	GtkWidget *b_gen = gtk_button_new_with_label("Generate");
	g_signal_connect_swapped(b_gen, "clicked", G_CALLBACK(on_generate), ew);
	gtk_box_pack_start(GTK_BOX(bar), b_gen, FALSE, FALSE, 0);

	GtkWidget *b_save = gtk_button_new_with_label("Save weekN.c...");
	g_signal_connect_swapped(b_save, "clicked", G_CALLBACK(on_save), ew);
	gtk_box_pack_start(GTK_BOX(bar), b_save, FALSE, FALSE, 0);

	/* main split: lists | output/details */
	GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_paned_set_position(GTK_PANED(paned), 330);
	gtk_box_pack_start(GTK_BOX(vbox), paned, TRUE, TRUE, 0);

	/* --- left: songs + chars listbox (notebook) --- */
	GtkWidget *left_nb = gtk_notebook_new();
	gtk_paned_pack1(GTK_PANED(paned), left_nb, FALSE, FALSE);

	// songs page
	GtkWidget *song_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	GtkWidget *song_sc = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_vexpand(song_sc, TRUE);
	gtk_box_pack_start(GTK_BOX(song_page), song_sc, TRUE, TRUE, 0);
	ew->song_list = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(ew->song_list), GTK_SELECTION_SINGLE);
	g_signal_connect(ew->song_list, "row-selected", G_CALLBACK(song_list_selected), ew);
	gtk_container_add(GTK_CONTAINER(song_sc), ew->song_list);
	GtkWidget *sh = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(song_page), sh, FALSE, FALSE, 0);
	{ GtkWidget *b = gtk_button_new_with_label("+ Song");
	  g_signal_connect_swapped(b, "clicked", G_CALLBACK(on_add_song), ew);
	  gtk_box_pack_start(GTK_BOX(sh), b, TRUE, TRUE, 0); }
	{ GtkWidget *b = gtk_button_new_with_label("- Remove");
	  g_signal_connect_swapped(b, "clicked", G_CALLBACK(on_del_song), ew);
	  gtk_box_pack_start(GTK_BOX(sh), b, TRUE, TRUE, 0); }
	{ GtkWidget *b = gtk_button_new_with_label("UP");
	  g_object_set_data(G_OBJECT(b), "delta", GINT_TO_POINTER(-1));
	  g_signal_connect_swapped(b, "clicked", G_CALLBACK(on_song_move), ew);
	  gtk_box_pack_start(GTK_BOX(sh), b, TRUE, TRUE, 0); }
	{ GtkWidget *b = gtk_button_new_with_label("DOWN");
	  g_object_set_data(G_OBJECT(b), "delta", GINT_TO_POINTER(1));
	  g_signal_connect_swapped(b, "clicked", G_CALLBACK(on_song_move), ew);
	  gtk_box_pack_start(GTK_BOX(sh), b, TRUE, TRUE, 0); }
	gtk_notebook_append_page(GTK_NOTEBOOK(left_nb), song_page, gtk_label_new("Songs"));

	// chars page
	GtkWidget *char_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	GtkWidget *char_sc = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_vexpand(char_sc, TRUE);
	gtk_box_pack_start(GTK_BOX(char_page), char_sc, TRUE, TRUE, 0);
	ew->char_list = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(ew->char_list), GTK_SELECTION_SINGLE);
	g_signal_connect(ew->char_list, "row-selected", G_CALLBACK(char_list_selected), ew);
	gtk_container_add(GTK_CONTAINER(char_sc), ew->char_list);
	GtkWidget *ch = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(char_page), ch, FALSE, FALSE, 0);
	{ GtkWidget *b = gtk_button_new_with_label("+ Character");
	  g_signal_connect_swapped(b, "clicked", G_CALLBACK(on_add_char), ew);
	  gtk_box_pack_start(GTK_BOX(ch), b, TRUE, TRUE, 0); }
	{ GtkWidget *b = gtk_button_new_with_label("- Remove");
	  g_signal_connect_swapped(b, "clicked", G_CALLBACK(on_del_char), ew);
	  gtk_box_pack_start(GTK_BOX(ch), b, TRUE, TRUE, 0); }
	gtk_notebook_append_page(GTK_NOTEBOOK(left_nb), char_page, gtk_label_new("Characters"));

	/* --- right: details | code --- */
	GtkWidget *right_nb = gtk_notebook_new();
	gtk_paned_pack2(GTK_PANED(paned), right_nb, TRUE, TRUE);

	// details page: stack (song | character)
	GtkWidget *det_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_vexpand(det_page, TRUE);
	ew->prop_stack = gtk_stack_new();
	gtk_box_pack_start(GTK_BOX(det_page), ew->prop_stack, TRUE, TRUE, 0);

	// song props
	GtkWidget *p_song = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_halign(p_song, GTK_ALIGN_FILL);
	{ GtkWidget *l = gtk_label_new("Song:");
	  gtk_widget_set_halign(l, GTK_ALIGN_START);
	  gtk_box_pack_start(GTK_BOX(p_song), l, FALSE, FALSE, 0); }
	ew->song_name = gtk_entry_new();
	g_signal_connect(ew->song_name, "changed", G_CALLBACK(on_name_changed), ew);
	gtk_box_pack_start(GTK_BOX(p_song), ew->song_name, FALSE, FALSE, 0);
	static const char *dt[MAX_DIFFS] = {"Easy", "Normal", "Hard"};
	for (int k = 0; k < MAX_DIFFS; k++)
	{
		GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
		GtkWidget *lbl = gtk_label_new(dt[k]);
		gtk_widget_set_size_request(lbl, 60, -1);
		gtk_box_pack_start(GTK_BOX(row), lbl, FALSE, FALSE, 0);
		ew->dif_ent[k] = gtk_entry_new();
		g_object_set_data(G_OBJECT(ew->dif_ent[k]), "diffk", GINT_TO_POINTER(k));
		g_signal_connect(ew->dif_ent[k], "changed", G_CALLBACK(on_diff_changed), ew);
		gtk_box_pack_start(GTK_BOX(row), ew->dif_ent[k], TRUE, TRUE, 0);
		GtkWidget *bb = gtk_button_new_with_label("...");
		g_object_set_data(G_OBJECT(bb), "diffk", GINT_TO_POINTER(k));
		g_signal_connect_swapped(bb, "clicked", G_CALLBACK(on_browse_diff), ew);
		gtk_box_pack_start(GTK_BOX(row), bb, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(p_song), row, FALSE, FALSE, 0);
	}
	GtkWidget *hint = gtk_label_new("Slots: one .cht.h file per difficulty. \"\" = NULL in the table. The name is derived from the file (e.g. bopeebo-hard.json.cht.h -> bopeebo/hard).");
	gtk_label_set_line_wrap(GTK_LABEL(hint), TRUE);
	gtk_widget_set_halign(hint, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(p_song), hint, FALSE, FALSE, 0);
	gtk_stack_add_named(GTK_STACK(ew->prop_stack), p_song, "song");

	// char props
	GtkWidget *p_char = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	{ GtkWidget *l = gtk_label_new("Character (include):");
	  gtk_widget_set_halign(l, GTK_ALIGN_START);
	  gtk_box_pack_start(GTK_BOX(p_char), l, FALSE, FALSE, 0); }
	GtkWidget *crow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	ew->char_path = gtk_entry_new();
	g_signal_connect(ew->char_path, "changed", G_CALLBACK(on_char_changed), ew);
	gtk_box_pack_start(GTK_BOX(crow), ew->char_path, TRUE, TRUE, 0);
	{ GtkWidget *bb = gtk_button_new_with_label("...");
	  g_signal_connect_swapped(bb, "clicked", G_CALLBACK(on_browse_char), ew);
	  gtk_box_pack_start(GTK_BOX(crow), bb, FALSE, FALSE, 0); }
	gtk_box_pack_start(GTK_BOX(p_char), crow, FALSE, FALSE, 0);
	GtkWidget *chint = gtk_label_new("E.g. character/bf.c. Only the include is emitted; bf/dad/gf enable the New() template in Load.");
	gtk_label_set_line_wrap(GTK_LABEL(chint), TRUE);
	gtk_widget_set_halign(chint, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(p_char), chint, FALSE, FALSE, 0);
	gtk_stack_add_named(GTK_STACK(ew->prop_stack), p_char, "character");
	gtk_stack_set_visible_child_name(GTK_STACK(ew->prop_stack), "song");

	gtk_notebook_append_page(GTK_NOTEBOOK(right_nb), det_page, gtk_label_new("Details"));

	// code page
	GtkWidget *code_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_vexpand(sc, TRUE);
	gtk_box_pack_start(GTK_BOX(code_page), sc, TRUE, TRUE, 0);
	ew->out = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(ew->out), FALSE);
	gtk_text_view_set_monospace(GTK_TEXT_VIEW(ew->out), TRUE);
	gtk_container_add(GTK_CONTAINER(sc), ew->out);
	gtk_notebook_append_page(GTK_NOTEBOOK(right_nb), code_page, gtk_label_new("Code"));

	ew->status = gtk_label_new("");
	gtk_widget_set_halign(ew->status, GTK_ALIGN_START);
	gtk_label_set_line_wrap(GTK_LABEL(ew->status), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), ew->status, FALSE, FALSE, 4);

	/* seed from the stage editor's open project, if any */
	if (app->project)
	{
		load_from_project(ew, app->project);
	}
	rebuild_song_list(ew);
	rebuild_char_list(ew);
	populate_song_props(ew);
	populate_char_props(ew);
	refresh_status(ew);
	gen_week(ew);

	g_object_set_data_full(G_OBJECT(vbox), "weekedit", ew, NULL);
	return vbox;
}