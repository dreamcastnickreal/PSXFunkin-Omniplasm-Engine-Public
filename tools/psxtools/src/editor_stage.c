/*
  psxtools - stage editor module implementation.

  Imports a weekN.c (+ Makefile) as a project, shows the background
  sprites on a zoomable/panable canvas, lets you move/scale/crop/flip
  each piece, and re-exports WeekN_Load + WeekN_DrawBG with round-trip
  fidelity (port of the python tools/stagegen).
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

#include "editor_stage.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define SCREEN_CX     (SCREEN_WIDTH/2)
#define SCREEN_CY     (SCREEN_HEIGHT/2)

typedef struct EditorStage EditorStage;

struct EditorStage
{
	App *app;
	GtkWidget *drawing;
	GtkWidget *layer_list;
	GtkWidget *status;
	GtkWidget *zoom_scale;

	Project *proj;
	gboolean has_proj;

	double zoom;
	double pan_x, pan_y;
	gboolean dragging;
	int drag_target;
	double drag_sx, drag_sy;
	int sel;

	GtkWidget *props_grid;
	GtkWidget *pw_x, *pw_y, *pw_w, *pw_h, *pw_flip;
	GtkWidget *pw_src0, *pw_src1, *pw_src2, *pw_src3;
	GtkWidget *pw_px, *pw_py;
	GtkWidget *pw_layer;    // combo: bg/md/fg/hud
	GtkWidget *pw_frame;    // spin: anim preview frame
	GtkWidget *pw_opacity;  // spin: blend opacity (-1 = dynamic)
	GtkWidget *pw_mode;     // spin: blend mode
	GtkWidget *pw_cond;     // label: visibility guard (read-only)

	GtkWidget *info_songs;
	GtkWidget *info_chars;

	GtkWidget *paned;       // horizontal divider canvas|dock
	GtkWidget *side;        // right dock (reparented on undock)
	GtkWidget *btn_undock;
	GtkWidget *float_win;   // non-NULL when undocked
};

static GHashTable *pb_table = NULL;
static GdkPixbuf *sprite_pixbuf(const Sprite *s)
{
	return pb_table ? g_hash_table_lookup(pb_table, s) : NULL;
}

static void editor_redraw(EditorStage *ed)
{
	gtk_widget_queue_draw(ed->drawing);
}
static void editor_rebuild_layers(EditorStage *ed);
static void editor_refresh_props(EditorStage *ed);
static void on_layer_up_down(GtkButton *btn, gpointer user);
static void editor_toggle_dock(GtkButton *btn, gpointer user);

// ------------------------------------------------------------------ week info
static void editor_refresh_weekinfo(EditorStage *ed)
{
	GString *s = g_string_new(NULL);
	if (ed->has_proj && ed->proj && ed->proj->chart_path_count > 0)
	{
		// group songs: strip "iso/chart/<name>-<diff>.json.cht.h"
		char **songs = NULL;
		int *counts = NULL;
		int nsongs = 0;
		static const char suf[] = ".json.cht.h";
		size_t sl = sizeof(suf) - 1;
		static const char *diffs[] = {"-easy", "-normal", "-hard", "-expert"};
		for (int i = 0; i < ed->proj->chart_path_count; i++)
		{
			const char *path = ed->proj->chart_paths[i];
			const char *base = strrchr(path, '/');
			base = base ? base + 1 : path;
			size_t len = strlen(base);
			if (len > sl && strcmp(base + len - sl, suf) == 0) len -= sl;
			char *name = g_strndup(base, len);
			for (int k = 0; k < 4; k++)
			{
				size_t dl = strlen(diffs[k]);
				size_t nl = strlen(name);
				if (nl > dl && strcmp(name + nl - dl, diffs[k]) == 0)
				{
					name[nl - dl] = 0;
					break;
				}
			}
			int dup = -1;
			for (int j = 0; j < nsongs; j++)
				if (strcmp(songs[j], name) == 0) { dup = j; break; }
			if (dup < 0)
			{
				songs = g_realloc(songs, sizeof(char*) * (size_t)(nsongs + 1));
				counts = g_realloc(counts, sizeof(int) * (size_t)(nsongs + 1));
				songs[nsongs] = g_strdup(name);
				counts[nsongs] = 1;
				nsongs++;
			}
			else
				counts[dup]++;
			g_free(name);
		}
		for (int j = 0; j < nsongs; j++)
		{
			if (j > 0) g_string_append(s, "   ");
			if (songs[j][0] >= 'a' && songs[j][0] <= 'z')
				songs[j][0] = songs[j][0] - 'a' + 'A';
			if (counts[j] > 1)
				g_string_append_printf(s, "%s (x%d)", songs[j], counts[j]);
			else
				g_string_append_printf(s, "%s", songs[j]);
		}
		for (int j = 0; j < nsongs; j++) g_free(songs[j]);
		g_free(songs);
		g_free(counts);
		gtk_label_set_text(GTK_LABEL(ed->info_songs), s->str);
		g_string_truncate(s, 0);
	}
	else
		gtk_label_set_text(GTK_LABEL(ed->info_songs), "(none in weekN.c)");
	g_string_free(s, TRUE);

	s = g_string_new(NULL);
	if (ed->has_proj && ed->proj && ed->proj->char_include_count > 0)
	{
		for (int i = 0; i < ed->proj->char_include_count; i++)
		{
			const char *path = ed->proj->char_includes[i];
			const char *base = strrchr(path, '/');
			base = base ? base + 1 : path;
			char *p = g_strdup(base);
			char *dot = strrchr(p, '.');
			if (dot) *dot = 0;
			if (s->len > 0) g_string_append(s, " ");
			g_string_append(s, p);
			g_free(p);
		}
		gtk_label_set_text(GTK_LABEL(ed->info_chars), s->str);
	}
	else
		gtk_label_set_text(GTK_LABEL(ed->info_chars), "(none in weekN.c)");
	g_string_free(s, TRUE);
}

// ------------------------------------------------------------------ images
static void load_images(EditorStage *ed)
{
	if (!pb_table) pb_table = g_hash_table_new(g_direct_hash, g_direct_equal);
	for (int i = 0; i < ed->proj->sprite_count; i++)
	{
		Sprite *s = ed->proj->sprites[i];
		g_hash_table_remove(pb_table, s);
		s->has_image = false;
		if (s->tex_path && g_file_test(s->tex_path, G_FILE_TEST_EXISTS))
		{
			GError *err = NULL;
			GdkPixbuf *pb = gdk_pixbuf_new_from_file(s->tex_path, &err);
			if (pb)
			{
				s->tex_w = gdk_pixbuf_get_width(pb);
				s->tex_h = gdk_pixbuf_get_height(pb);
				g_hash_table_insert(pb_table, s, pb);
				s->has_image = true;
				if (s->src[2] <= 0 || s->src[3] <= 0)
				{
					s->src[2] = s->tex_w;
					s->src[3] = s->tex_h;
					if (s->w <= 0) s->w = s->tex_w;
					if (s->h <= 0) s->h = s->tex_h;
				}
			}
			else if (err) g_error_free(err);
		}
	}
}

// ------------------------------------------------------------------ coords
static void center_size(EditorStage *ed, int *cw, int *ch)
{
	*cw = gtk_widget_get_allocated_width(ed->drawing);
	*ch = gtk_widget_get_allocated_height(ed->drawing);
	if (*cw < 1) *cw = 1;
	if (*ch < 1) *ch = 1;
}
static void c2s(EditorStage *ed, double cx, double cy, double *sx, double *sy)
{
	int cw, ch; center_size(ed, &cw, &ch);
	*sx = (cx - cw/2.0 - ed->pan_x) / ed->zoom;
	*sy = (cy - ch/2.0 - ed->pan_y) / ed->zoom;
}
static void s2c(EditorStage *ed, double sx, double sy, double *cx, double *cy)
{
	int cw, ch; center_size(ed, &cw, &ch);
	*cx = cw/2.0 + ed->pan_x + sx * ed->zoom;
	*cy = ch/2.0 + ed->pan_y + sy * ed->zoom;
}

// ------------------------------------------------------------------ draw
static gboolean canvas_draw(GtkWidget *widget, cairo_t *cr, gpointer user)
{
	(void)widget;
	EditorStage *ed = user;
	int cw, ch; center_size(ed, &cw, &ch);
	cairo_set_source_rgb(cr, 0.19, 0.25, 0.31);
	cairo_paint(cr);

	if (ed->has_proj && ed->proj)
	{
		double x0, y0, x1, y1;
		s2c(ed, -SCREEN_CX, -SCREEN_CY, &x0, &y0);
		s2c(ed,  SCREEN_CX,  SCREEN_CY, &x1, &y1);
		cairo_rectangle(cr, x0, y0, x1 - x0, y1 - y0);
		cairo_set_source_rgb(cr, 0.5, 1.0, 0.5);
		cairo_set_line_width(cr, 1.0);
		cairo_set_dash(cr, (double[]){4,4}, 2, 0);
		cairo_stroke(cr);
		cairo_set_dash(cr, NULL, 0, 0);

		for (int i = 0; i < ed->proj->sprite_count; i++)
		{
			Sprite *s = ed->proj->sprites[i];
			double bb[4]; sprite_bbox(s, bb);
			s2c(ed, bb[0], bb[1], &x0, &y0);
			s2c(ed, bb[2], bb[3], &x1, &y1);
			double dw = x1 - x0, dh = y1 - y0;

			GdkPixbuf *pb = sprite_pixbuf(s);
			if (pb)
			{
				int sw = s->src[2] > 0 ? s->src[2] : 1;
				int sh = s->src[3] > 0 ? s->src[3] : 1;
				int sx = s->src[0] < 0 ? 0 : s->src[0];
				int sy = s->src[1] < 0 ? 0 : s->src[1];
				int pw = gdk_pixbuf_get_width(pb);
				int ph = gdk_pixbuf_get_height(pb);
				if (sx >= pw) sx = pw - 1;
				if (sy >= ph) sy = ph - 1;
				if (sx < 0) sx = 0;
				if (sy < 0) sy = 0;
				if (sx + sw > pw) sw = pw - sx;
				if (sy + sh > ph) sh = ph - sy;
				if (sw < 1) sw = 1;
				if (sh < 1) sh = 1;
				GdkPixbuf *crop = gdk_pixbuf_new_subpixbuf(pb, sx, sy, sw, sh);
				if (crop && dw > 0.5 && dh > 0.5)
				{
					cairo_save(cr);
					cairo_rectangle(cr, x0, y0, dw, dh);
					cairo_clip(cr);
					cairo_translate(cr, x0, y0);
					if (s->flip) { cairo_translate(cr, dw, 0); cairo_scale(cr, -1.0, 1.0); }
					cairo_scale(cr, dw / (double)sw, dh / (double)sh);
					gdk_cairo_set_source_pixbuf(cr, crop, 0, 0);
					cairo_paint(cr);
					cairo_restore(cr);
				}
				if (crop) g_object_unref(crop);

				if (i == ed->sel)
				{
					cairo_set_source_rgb(cr, 1.0, 0.82, 0.25);
					cairo_set_line_width(cr, 2.0);
					cairo_rectangle(cr, x0, y0, dw, dh);
					cairo_stroke(cr);
				}
			}
			else
			{
				cairo_set_source_rgb(cr, 1.0, 0.38, 0.38);
				cairo_set_line_width(cr, 1.0);
				cairo_set_dash(cr, (double[]){2,2}, 2, 0);
				cairo_rectangle(cr, x0, y0, dw, dh);
				cairo_stroke(cr);
				cairo_set_dash(cr, NULL, 0, 0);
			}
		}
	}

	double ox, oy; s2c(ed, 0, 0, &ox, &oy);
	cairo_set_source_rgb(cr, 0.25, 0.31, 0.37);
	cairo_set_line_width(cr, 1.0);
	cairo_move_to(cr, 0, oy); cairo_line_to(cr, cw, oy); cairo_stroke(cr);
	cairo_move_to(cr, ox, 0); cairo_line_to(cr, ox, ch); cairo_stroke(cr);

	char buf[160];
	snprintf(buf, sizeof(buf), "zoom %.2f   [LMB drag, RMB pan, wheel zoom]",
	         ed->zoom);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_move_to(cr, 8, ch - 8);
	cairo_set_font_size(cr, 11);
	cairo_show_text(cr, buf);
	return FALSE;
}

// ------------------------------------------------------------------ pick
static int editor_pick(EditorStage *ed, double cx, double cy)
{
	if (!ed->has_proj) return -1;
	for (int i = ed->proj->sprite_count - 1; i >= 0; i--)
	{
		Sprite *s = ed->proj->sprites[i];
		double bb[4]; sprite_bbox(s, bb);
		double x0, y0, x1, y1;
		s2c(ed, bb[0], bb[1], &x0, &y0);
		s2c(ed, bb[2], bb[3], &x1, &y1);
		if (cx >= x0 && cx <= x1 && cy >= y0 && cy <= y1) return i;
	}
	return -1;
}

// ------------------------------------------------------------------ mouse
static void editor_move_sprite(EditorStage *ed, Sprite *s, double nx, double ny)
{
	(void)ed;
	// code-driven positions (Week4 car) can't move in the editor
	if (s->dyn_x || s->dyn_y)
		return;
	if (s->anim)
	{
		// animated instance: move the raw draw position, keep the
		// frame-0 preview geometry in sync
		s->anim_x = nx + (s->anim->use_off && s->anim_frame < s->anim->frame_count
		                  ? s->anim->frames[s->anim_frame].off[0] : 0);
		s->anim_y = ny + (s->anim->use_off && s->anim_frame < s->anim->frame_count
		                  ? s->anim->frames[s->anim_frame].off[1] : 0);
		double bb[4]; sprite_bbox(s, bb);
		double dx = nx - bb[0];
		double dy = ny - bb[1];
		s->x += dx; s->y += dy;
		return;
	}
	if (s->arb_valid)
	{
		double bb[4]; sprite_bbox(s, bb);
		double dx = nx - bb[0];
		double dy = ny - bb[1];
		for (int k = 0; k < 4; k++)
		{
			s->arb[k][0] += dx;
			s->arb[k][1] += dy;
		}
	}
	else
	{
		s->x = nx;
		s->y = ny;
	}
}

static gboolean canvas_button(GtkWidget *w, GdkEventButton *e, gpointer user)
{
	(void)w;
	EditorStage *ed = user;
	if (e->button == 1)
	{
		int idx = editor_pick(ed, e->x, e->y);
		ed->sel = idx;
		editor_refresh_props(ed);
		editor_redraw(ed);
		if (idx >= 0)
		{
			Sprite *s = ed->proj->sprites[idx];
			double bb[4]; sprite_bbox(s, bb);
			double ssx, ssy;
			c2s(ed, e->x, e->y, &ssx, &ssy);
			ed->drag_target = idx;
			ed->drag_sx = ssx - bb[0];
			ed->drag_sy = ssy - bb[1];
			ed->dragging = TRUE;
		}
	}
	else if (e->button == 3)
	{
		ed->drag_target = -2;
		ed->drag_sx = e->x;
		ed->drag_sy = e->y;
		ed->dragging = TRUE;
	}
	return FALSE;
}

static gboolean canvas_motion(GtkWidget *w, GdkEventMotion *e, gpointer user)
{
	(void)w;
	EditorStage *ed = user;
	if (!ed->dragging) return FALSE;
	if (ed->drag_target == -2)
	{
		ed->pan_x += e->x - ed->drag_sx;
		ed->pan_y += e->y - ed->drag_sy;
		ed->drag_sx = e->x;
		ed->drag_sy = e->y;
		editor_redraw(ed);
		return FALSE;
	}
	if (ed->drag_target >= 0 && ed->has_proj)
	{
		Sprite *s = ed->proj->sprites[ed->drag_target];
		double bb[4]; sprite_bbox(s, bb);
		double ssx, ssy;
		c2s(ed, e->x, e->y, &ssx, &ssy);
		double nx = ssx - ed->drag_sx;
		double ny = ssy - ed->drag_sy;
		editor_move_sprite(ed, s, nx, ny);
		editor_refresh_props(ed);
		editor_redraw(ed);
	}
	return FALSE;
}

static gboolean canvas_release(GtkWidget *w, GdkEventButton *e, gpointer user)
{
	(void)w; (void)e;
	EditorStage *ed = user;
	ed->dragging = FALSE;
	return FALSE;
}

static gboolean canvas_scroll(GtkWidget *w, GdkEventScroll *e, gpointer user)
{
	(void)w;
	EditorStage *ed = user;
	double factor = (e->direction == GDK_SCROLL_UP || e->direction == GDK_SCROLL_LEFT)
	                ? 1.2 : 1.0/1.2;
	ed->zoom = ed->zoom * factor;
	if (ed->zoom < 0.1) ed->zoom = 0.1;
	if (ed->zoom > 32.0) ed->zoom = 32.0;
	gtk_range_set_value(GTK_RANGE(ed->zoom_scale), ed->zoom);
	editor_redraw(ed);
	return TRUE;
}

static void zoom_changed(GtkRange *r, gpointer user)
{
	EditorStage *ed = user;
	ed->zoom = gtk_range_get_value(r);
	editor_redraw(ed);
}

// ------------------------------------------------------------------ layers
static GtkListBoxRow *layer_make_row(EditorStage *ed, int i)
{
	Sprite *s = ed->proj->sprites[i];
	const char *tag = s->arb_valid ? "arb" : (s->draw == DRAW_ABS ? "abs" : "rect");
	char tagbuf[64];
	if (s->anim)
		snprintf(tagbuf, sizeof(tagbuf), "%s/anim", stage_layer_name(s->layer));
	else
		snprintf(tagbuf, sizeof(tagbuf), "%s/%s", stage_layer_name(s->layer), tag);
	char buf[256];
	if (s->w > 0 && s->h > 0)
		snprintf(buf, sizeof(buf), "%s  (%dx%d)", s->name,
		         (int)lround(s->w), (int)lround(s->h));
	else
		snprintf(buf, sizeof(buf), "%s", s->name);

	GtkWidget *row = gtk_list_box_row_new();
	GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	char markup[64];
	snprintf(markup, sizeof(markup), "<b><tt>%s</tt></b>", tagbuf);
	GtkWidget *tagl = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(tagl), markup);
	gtk_widget_set_halign(tagl, GTK_ALIGN_START);
	gtk_widget_set_valign(tagl, GTK_ALIGN_CENTER);
	GtkWidget *nl = gtk_label_new(buf);
	gtk_widget_set_halign(nl, GTK_ALIGN_START);
	g_object_set(nl, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_widget_set_hexpand(nl, TRUE);
	gtk_box_pack_start(GTK_BOX(hb), tagl, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hb), nl, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(row), hb);
	return GTK_LIST_BOX_ROW(row);
}

static void editor_rebuild_layers(EditorStage *ed)
{
	GtkListBox *lb = GTK_LIST_BOX(ed->layer_list);
	GList *kids = gtk_container_get_children(GTK_CONTAINER(lb));
	for (GList *l = kids; l; l = l->next)
		gtk_widget_destroy(GTK_WIDGET(l->data));
	g_list_free(kids);
	if (!ed->has_proj || !ed->proj)
	{
		GtkWidget *lbl = gtk_label_new("(no project)");
		gtk_widget_set_halign(lbl, GTK_ALIGN_START);
		gtk_widget_set_sensitive(lbl, FALSE);
		gtk_list_box_insert(lb, lbl, -1);
		gtk_widget_show_all(GTK_WIDGET(lb));
		return;
	}
	for (int i = 0; i < ed->proj->sprite_count; i++)
		gtk_list_box_insert(lb, GTK_WIDGET(layer_make_row(ed, i)), -1);
	gtk_widget_show_all(GTK_WIDGET(lb));
	if (ed->sel >= 0 && ed->sel < ed->proj->sprite_count)
		gtk_list_box_select_row(lb, gtk_list_box_get_row_at_index(lb, ed->sel));
}

static void layer_selected(GtkListBox *box, GtkListBoxRow *row, gpointer user)
{
	(void)box;
	EditorStage *ed = user;
	if (!row) return;
	ed->sel = gtk_list_box_row_get_index(row);
	editor_refresh_props(ed);
	editor_redraw(ed);
}

static void layer_move(EditorStage *ed, int delta)
{
	if (ed->sel < 0 || !ed->has_proj) return;
	int i = ed->sel, j = i + delta;
	if (j < 0 || j >= ed->proj->sprite_count) return;
	Sprite *tmp = ed->proj->sprites[i];
	ed->proj->sprites[i] = ed->proj->sprites[j];
	ed->proj->sprites[j] = tmp;
	ed->sel = j;
	editor_rebuild_layers(ed);
	editor_redraw(ed);
}

static void layer_dup(EditorStage *ed)
{
	if (ed->sel < 0 || !ed->has_proj) return;
	Sprite *s = ed->proj->sprites[ed->sel];
	Sprite *c = calloc(1, sizeof(Sprite));
	*c = *s;
	c->name = strdup(s->name); c->var_name = strdup(s->var_name);
	c->base = strdup(s->base); c->tex_path = strdup(s->tex_path);
	c->par_x_expr = s->par_x_expr ? strdup(s->par_x_expr) : NULL;
	c->par_y_expr = s->par_y_expr ? strdup(s->par_y_expr) : NULL;
	c->arc = s->arc ? strdup(s->arc) : NULL;
	c->tim = s->tim ? strdup(s->tim) : NULL;
	c->tex_field = s->tex_field ? strdup(s->tex_field) : NULL;
	c->cond = s->cond ? strdup(s->cond) : NULL;
	c->dyn_x = s->dyn_x ? strdup(s->dyn_x) : NULL;
	c->dyn_y = s->dyn_y ? strdup(s->dyn_y) : NULL;
	// c->anim stays shared (Project owns the AnimSet)
	editor_move_sprite(ed, c, s->x + 20, s->y + 20);
	ed->proj->sprite_count++;
	ed->proj->sprites = realloc(ed->proj->sprites,
		sizeof(Sprite*) * (size_t)ed->proj->sprite_count);
	ed->proj->sprites[ed->proj->sprite_count - 1] = c;
	ed->sel = ed->proj->sprite_count - 1;
	editor_rebuild_layers(ed);
	editor_refresh_props(ed);
	editor_redraw(ed);
}

static void layer_del(EditorStage *ed)
{
	if (ed->sel < 0 || !ed->has_proj) return;
	int i = ed->sel;
	free(ed->proj->sprites[i]->name);
	free(ed->proj->sprites[i]->var_name);
	free(ed->proj->sprites[i]->base);
	free(ed->proj->sprites[i]->tex_path);
	free(ed->proj->sprites[i]->par_x_expr);
	free(ed->proj->sprites[i]->par_y_expr);
	free(ed->proj->sprites[i]->arc);
	free(ed->proj->sprites[i]->tim);
	free(ed->proj->sprites[i]->tex_field);
	free(ed->proj->sprites[i]->cond);
	free(ed->proj->sprites[i]->dyn_x);
	free(ed->proj->sprites[i]->dyn_y);
	// anim is shared (Project-owned), never freed per-sprite
	free(ed->proj->sprites[i]);
	for (int k = i; k < ed->proj->sprite_count - 1; k++)
		ed->proj->sprites[k] = ed->proj->sprites[k+1];
	ed->proj->sprite_count--;
	ed->sel = -1;
	editor_rebuild_layers(ed);
	editor_refresh_props(ed);
	editor_redraw(ed);
}

// ------------------------------------------------------------------ props
static GtkWidget *prop_row(GtkWidget *grid, int row, const char *label)
{
	GtkWidget *lbl = gtk_label_new(label);
	gtk_widget_set_halign(lbl, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(grid), lbl, 0, row, 1, 1);
	GtkWidget *sp = gtk_spin_button_new_with_range(-2000, 2000, 0.05);
	gtk_widget_set_hexpand(sp, TRUE);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(sp), 3);
	gtk_grid_attach(GTK_GRID(grid), sp, 1, row, 1, 1);
	return sp;
}

// Switch an animated instance's preview frame (geometry follows the
// frame's src rect + offset; the draw position stays put).
static void anim_apply_frame(Sprite *s, int frame)
{
	if (!s->anim || s->anim->frame_count <= 0) return;
	if (frame < 0) frame = 0;
	if (frame >= s->anim->frame_count) frame = s->anim->frame_count - 1;
	s->anim_frame = frame;
	AnimFrame *f = &s->anim->frames[frame];
	s->src[0] = f->src[0]; s->src[1] = f->src[1];
	s->src[2] = f->src[2]; s->src[3] = f->src[3];
	if (s->dyn_x || s->dyn_y)
	{
		// code-driven position: only the crop follows the frame;
		// size still previews from the frame
		s->w = (double)f->src[2] * (double)s->anim->scale_x;
		s->h = (double)f->src[3] * (double)s->anim->scale_y;
		return;
	}
	s->x = s->anim_x - (s->anim->use_off ? f->off[0] : 0);
	s->y = s->anim_y - (s->anim->use_off ? f->off[1] : 0);
	s->w = (double)f->src[2] * (double)s->anim->scale_x;
	s->h = (double)f->src[3] * (double)s->anim->scale_y;
}

static void apply_spin(EditorStage *ed, const char *which, double v)
{
	if (ed->sel < 0 || !ed->has_proj) return;
	Sprite *s = ed->proj->sprites[ed->sel];
	if ((strcmp(which, "x") == 0 || strcmp(which, "y") == 0) &&
	    (s->dyn_x || s->dyn_y))
		return; // code-driven position is locked
	if (strcmp(which, "x") == 0)
	{
		double bb[4]; sprite_bbox(s, bb);
		editor_move_sprite(ed, s, v, bb[1]);
	}
	else if (strcmp(which, "y") == 0)
	{
		double bb[4]; sprite_bbox(s, bb);
		editor_move_sprite(ed, s, bb[0], v);
	}
	else if (strcmp(which, "flip") == 0)
	{
		s->flip = v != 0;
	}
	else if (strcmp(which, "w") == 0)
	{
		if (!s->arb_valid && v > 0) s->w = v;
	}
	else if (strcmp(which, "h") == 0)
	{
		if (!s->arb_valid && v > 0) s->h = v;
	}
	else if (strcmp(which, "px") == 0)
	{
		s->par_x = v;
	}
	else if (strcmp(which, "py") == 0)
	{
		s->par_y = v;
	}
	else if (strcmp(which, "frame") == 0)
	{
		if (s->anim) anim_apply_frame(s, (int)v);
	}
	else if (strcmp(which, "opacity") == 0)
	{
		s->opacity = (int)v; // -1 = dynamic (engine variable)
	}
	else if (strcmp(which, "mode") == 0)
	{
		s->blend_mode = (int)v;
		if (v != 0) s->blend = true;
	}
	else if (strcmp(which, "layer") == 0)
	{
		int L = (int)v;
		if (L >= LAYER_BG && L <= LAYER_HUD)
		{
			s->layer = (StageLayer)L;
			editor_rebuild_layers(ed);
		}
	}
	editor_redraw(ed);
}

static gboolean prop_flip(GtkToggleButton *tb, gpointer user)
{
	EditorStage *ed = user;
	if (ed->sel < 0 || !ed->has_proj) return FALSE;
	ed->proj->sprites[ed->sel]->flip = gtk_toggle_button_get_active(tb);
	editor_redraw(ed);
	return FALSE;
}

static gboolean prop_spin(GtkSpinButton *sp, gpointer user)
{
	const char *prop = g_object_get_data(G_OBJECT(sp), "prop");
	apply_spin(user, prop, gtk_spin_button_get_value(sp));
	if (strcmp(prop, "frame") == 0)
		editor_refresh_props(user); // frame switch moves x/y/w/h too
	return FALSE;
}

static void prop_layer(GtkComboBox *cb, gpointer user)
{
	apply_spin(user, "layer", (double)gtk_combo_box_get_active(cb));
}

static gboolean prop_src_spin(GtkSpinButton *sp, gpointer user)
{
	EditorStage *ed = user;
	if (ed->sel < 0 || !ed->has_proj) return FALSE;
	Sprite *s = ed->proj->sprites[ed->sel];
	int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(sp), "prop"));
	s->src[idx] = (int)gtk_spin_button_get_value(sp);
	editor_redraw(ed);
	return FALSE;
}

static void editor_refresh_props(EditorStage *ed)
{
	if (!ed->pw_x || ed->sel < 0 || !ed->has_proj) return;
	Sprite *s = ed->proj->sprites[ed->sel];
	double bb[4]; sprite_bbox(s, bb);
	gboolean dyn = (s->dyn_x || s->dyn_y) ? TRUE : FALSE;
	gtk_widget_set_sensitive(ed->pw_x, !dyn);
	gtk_widget_set_sensitive(ed->pw_y, !dyn);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ed->pw_x), bb[0]);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ed->pw_y), bb[1]);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ed->pw_w), bb[2] - bb[0]);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ed->pw_h), bb[3] - bb[1]);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ed->pw_flip), s->flip);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ed->pw_src0), s->src[0]);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ed->pw_src1), s->src[1]);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ed->pw_src2), s->src[2]);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ed->pw_src3), s->src[3]);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ed->pw_px), s->par_x);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ed->pw_py), s->par_y);
	if (ed->pw_layer)
		gtk_combo_box_set_active(GTK_COMBO_BOX(ed->pw_layer), (gint)s->layer);
	if (ed->pw_frame)
	{
		if (s->anim && s->anim->frame_count > 0)
		{
			gtk_widget_set_sensitive(ed->pw_frame, TRUE);
			GtkSpinButton *sp = GTK_SPIN_BUTTON(ed->pw_frame);
			gtk_spin_button_set_range(sp, 0, s->anim->frame_count - 1);
			gtk_spin_button_set_value(sp, s->anim_frame);
		}
		else
			gtk_widget_set_sensitive(ed->pw_frame, FALSE);
	}
	if (ed->pw_opacity)
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(ed->pw_opacity), s->opacity);
	if (ed->pw_mode)
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(ed->pw_mode), s->blend_mode);
	if (ed->pw_cond)
	{
		char buf[256];
		if (s->anim)
			snprintf(buf, sizeof(buf), "actor %s()", s->anim->draw_fn);
		else if (s->cond)
			snprintf(buf, sizeof(buf), "if (%s)", s->cond);
		else
			snprintf(buf, sizeof(buf), "(always)");
		if (dyn)
		{
			char tmp[256];
			snprintf(tmp, sizeof(tmp), "%s  [pos locked: code-driven]", buf);
			snprintf(buf, sizeof(buf), "%s", tmp);
		}
		gtk_label_set_text(GTK_LABEL(ed->pw_cond), buf);
	}
}

// ------------------------------------------------------------------ export
// Stage-editor export: round-trip header + shared graphics block.
// Returns a newly allocated string of C code (or NULL).
static char *gen_export_text(EditorStage *ed)
{
	GString *g = g_string_new(NULL);
	g_string_append_printf(g,
		"//Generated with psxtools (round-trip of a real project)\n"
		"\n"
		"//==========================================================================\n"
		"// CONTRACT: replace ONLY %s_Load and %s_DrawBG in your %s.c,\n"
		"// keeping charts, GetChart/NextStage/SetPtr and the #includes.\n"
		"// The Overlay_DataRead() order below and the deps line of the\n"
		"// Makefile must match (positional contract).\n"
		"//==========================================================================\n"
		"\n",
		ed->proj->prefix, ed->proj->prefix, ed->proj->week);
	char *core = export_graphics_block(ed->proj, NULL, NULL, NULL);
	if (core)
	{
		g_string_append(g, core);
		g_free(core);
	}
	return g_string_free(g, FALSE);
}

// Shared with the week editor: emits tex decls + Load + DrawBG for a project.
// P/prefix may be given to override the names (embedding into a target weekN.c);
// pass NULL to derive them from the project. load_extra (may be NULL) is inserted
// at the end of %s_Load, right before the closing brace.
char *export_graphics_block(Project *pr, const char *P, const char *prefix,
                            const char *load_extra)
{
	GString *g = g_string_new(NULL);
	if (!P) P = pr->prefix;
	if (!prefix) prefix = pr->week;

	g_string_append_printf(g,
		"//Graphics block generated with psxtools for %s\n"
		"//\n"
		"// The Overlay_DataRead() order of %s_Load and the deps line\n"
		"// below must match (positional contract).\n"
		"\n",
		prefix, P);

	// group sprites by base (texture variable per base)
	GList *order = NULL;  // list of "base"
	GHashTable *groups = g_hash_table_new(g_str_hash, g_str_equal); // base -> GString var
	for (int i = 0; i < pr->sprite_count; i++)
	{
		Sprite *s = pr->sprites[i];
		const char *base = s->base ? s->base : "back0";
		if (!g_hash_table_contains(groups, base))
		{
			char var[256];
			int wi = 0;
			for (const char *c = base; *c && wi < 240; c++)
				if (isalnum((unsigned char)*c) || *c == '_') var[wi++] = *c;
				else var[wi++] = '_';
			var[wi] = 0;
			if (!var[0] || !isalpha((unsigned char)var[0])) { memmove(var+4, var, strlen(var)+1); memcpy(var, "tex_", 4); }
			g_hash_table_insert(groups, (gpointer)base, g_strdup(var));
			order = g_list_append(order, g_strdup(base));
		}
	}

	g_string_append_printf(g, "// Makefile deps:\n"
		"// iso/%s/%s.exe: Overlay.%s iso/stage/hud0.tim iso/week1/hud1.tim",
		prefix, prefix, prefix);
	for (GList *l = order; l; l = l->next)
	{
		const char *base = l->data;
		// skip hud/ice (shared), only emit the block's own textures that aren't shared
		g_string_append_printf(g, " iso/%s/%s.tim", prefix, base);
	}
	g_string_append(g, "\n\n");

	g_string_append_printf(g, "//%s textures\n", P);
	for (GList *l = order; l; l = l->next)
	{
		const char *base = l->data;
		const char *var = g_hash_table_lookup(groups, base);
		g_string_append_printf(g, "static Gfx_Tex %s_tex_%s;\n", prefix, var);
	}
	g_string_append(g, "\n");

	// Load
	g_string_append_printf(g, "static void %s_Load(void)\n{\n", P);
	g_string_append(g, "\t//Load assets\n\tIO_Data overlay_data;\n\n");
	g_string_append(g, "\tGfx_LoadTex(&stage.tex_hud0, overlay_data = Overlay_DataRead(), 0); Mem_Free(overlay_data); //hud0.tim\n");
	g_string_append(g, "\tGfx_LoadTex(&stage.tex_hud1, overlay_data = Overlay_DataRead(), 0); Mem_Free(overlay_data); //hud1.tim\n");
	g_string_append(g, "\tGfx_LoadTex(&stage.tex_hud_ice, overlay_data = Overlay_DataRead(), 0); Mem_Free(overlay_data); //ice_sheet.tim\n");
	for (GList *l = order; l; l = l->next)
	{
		const char *base = l->data;
		const char *var = g_hash_table_lookup(groups, base);
		g_string_append_printf(g, "\tGfx_LoadTex(&%s_tex_%s, overlay_data = Overlay_DataRead(), 0); Mem_Free(overlay_data); //%s.tim\n",
			prefix, var, base);
	}
	if (load_extra)
		g_string_append(g, load_extra);
	g_string_append(g, "}\n\n");

	// DrawBG
	g_string_append_printf(g, "static void %s_DrawBG(void)\n{\n", P);
	g_string_append(g, "\tfixed_t fx, fy;\n\n");
	for (int i = 0; i < pr->sprite_count; i++)
	{
		Sprite *s = pr->sprites[i];
		const char *var = g_hash_table_lookup(groups, s->base ? s->base : "back0");
		char v[80];
		snprintf(v, sizeof(v), "%s_p%d", s->base ? s->base : "back", i);

		// src
		g_string_append_printf(g, "\tRECT %s_src = { %d, %d, %d, %d };\n",
			v, s->src[0], s->src[1], s->src[2], s->src[3]);

		// parallax preamble
		int wrote = 0;
		if (s->par_x != 0.0)
		{
			char *expr = mult_expr(s->par_x);
			g_string_append_printf(g, "\tfx = stage.camera.x%s;\n", expr ? expr : "");
			g_free(expr);
			wrote = 1;
		}
		if (s->par_y != 0.0)
		{
			char *expr = mult_expr(s->par_y);
			g_string_append_printf(g, "\tfy = stage.camera.y%s;\n", expr ? expr : "");
			g_free(expr);
			wrote = 1;
		}
		if (wrote) g_string_append(g, "\n");

		const char *sx_term = s->par_x != 0.0 ? " - fx" : "";
		const char *sy_term = s->par_y != 0.0 ? " - fy" : "";

		if (s->draw == DRAW_ABS)
		{
			int l = (int)round(s->x + SCREEN_CX);
			int t = (int)round(s->y + SCREEN_CY);
			int w = (int)round(s->w), h = (int)round(s->h);
			g_string_append_printf(g, "\tRECT %s_dst = { %d, %d, %d, %d };\n", v, l, t, w, h);
			g_string_append_printf(g, "\tGfx_DrawTex(&%s_tex_%s, &%s_src, &%s_dst);\n\n", prefix, var, v, v);
		}
		else if (s->arb_valid)
		{
			g_string_append_printf(g, "\tPOINT_FIXED %s_d0 = { %s%s, %s%s };\n",
				v, double_to_fixed(s->arb[0][0]), sx_term, double_to_fixed(s->arb[0][1]), sy_term);
			g_string_append_printf(g, "\tPOINT_FIXED %s_d1 = { %s%s, %s%s };\n",
				v, double_to_fixed(s->arb[1][0]), sx_term, double_to_fixed(s->arb[1][1]), sy_term);
			g_string_append_printf(g, "\tPOINT_FIXED %s_d2 = { %s%s, %s%s };\n",
				v, double_to_fixed(s->arb[2][0]), sx_term, double_to_fixed(s->arb[2][1]), sy_term);
			g_string_append_printf(g, "\tPOINT_FIXED %s_d3 = { %s%s, %s%s };\n",
				v, double_to_fixed(s->arb[3][0]), sx_term, double_to_fixed(s->arb[3][1]), sy_term);
			g_string_append_printf(g, "\tStage_DrawTexArb(&%s_tex_%s, &%s_src, &%s_d0, &%s_d1, &%s_d2, &%s_d3, stage.camera.bzoom);\n\n",
				prefix, var, v, v, v, v, v);
		}
		else
		{
			char *x = double_to_fixed(s->x), *y = double_to_fixed(s->y);
			char *w = double_to_fixed(s->w), *h = double_to_fixed(s->h);
			g_string_append_printf(g,
				"\tRECT_FIXED %s_dst = {\n\t\t%s%s,\n\t\t%s%s,\n\t\t%s,\n\t\t%s\n\t};\n",
				v, x, sx_term, y, sy_term, w, h);
			g_string_append_printf(g, "\tStage_DrawTex(&%s_tex_%s, &%s_src, &%s_dst, stage.camera.bzoom);\n\n",
				prefix, var, v, v);
			g_free(x); g_free(y); g_free(w); g_free(h);
		}
	}
	g_string_append(g, "}\n");

	for (GList *l = order; l; l = l->next) { g_free(l->data); }
	g_list_free(order);
	return g_string_free(g, FALSE);
}

// ------------------------------------------------------------------ omni export
// Omniplasm src/stage/*.c generator: Back_<X> struct + CharFrame/Animation
// tables + SetFrame/Draw helpers (verbatim) + per-layer DrawBG/MD/FG/HUD +
// Free + New. Round-trip notes (also stamped into the file):
// - song_step trigger logic around Animatable_Animate is NOT preserved
//   (a plain Animate call is emitted instead).
// - dynamic blend opacity (engine variables) is baked to 255.
// - static pieces sharing one RECT var with mid-body mutation import at
//   the first pose; offset the duplicate by hand.

static void omni_sanitize(const char *in, char *out, size_t n)
{
	size_t o = 0;
	for (; *in && o + 1 < n; in++)
	{
		unsigned char ch = (unsigned char)*in;
		out[o++] = (isalnum(ch) || ch == '_') ? (char)ch : '_';
	}
	out[o] = 0;
	if (o == 0) snprintf(out, n, "x");
}

static void omni_stage_upper(const char *in, char *out, size_t n)
{
	size_t o = 0;
	for (; *in && o + 1 < n; in++)
	{
		unsigned char ch = (unsigned char)*in;
		out[o++] = isalnum(ch) ? (char)toupper(ch) : '_';
	}
	out[o] = 0;
	if (o == 0) snprintf(out, n, "STAGE");
}

static void omni_raw_block(GString *g, const char *raw)
{
	if (!raw || !raw[0]) return;
	g_string_append(g, raw);
	if (raw[strlen(raw) - 1] != '\n')
		g_string_append_c(g, '\n');
}

static gboolean omni_field_owned_by_anim(Project *pr, const char *field)
{
	for (int i = 0; i < pr->anim_count; i++)
		if (strcmp(pr->anims[i]->tex_field, field) == 0)
			return TRUE;
	return FALSE;
}

static void omni_emit_par(GString *g, const char *ind, const char *var,
                          const char *axis, double f, double *cur)
{
	if (f == 0.0 || f == *cur) return;
	if (f == 1.0)
		g_string_append_printf(g, "%s%s = stage.camera.%s;\n", ind, var, axis);
	else
	{
		char *e = mult_expr(f);
		g_string_append_printf(g, "%s%s = stage.camera.%s%s;\n", ind, var, axis, e ? e : "");
		g_free(e);
	}
	*cur = f;
}

// Emit one static (non-anim) sprite. ind is the base indent ("\t" or "\t\t").
static void omni_emit_sprite(GString *g, Sprite *s, int idx,
                             const char *ind, double *cur_fx, double *cur_fy)
{
	char san[128], v[160];
	omni_sanitize(s->base ? s->base : "back", san, sizeof(san));
	snprintf(v, sizeof(v), "%s_p%d", san, idx);
	char ind2[16];
	snprintf(ind2, sizeof(ind2), "%s\t", ind);

	g_string_append_printf(g, "%sRECT %s_src = { %d, %d, %d, %d };\n",
		ind, v, s->src[0], s->src[1], s->src[2], s->src[3]);

	omni_emit_par(g, ind, "fx", "x", s->par_x, cur_fx);
	omni_emit_par(g, ind, "fy", "y", s->par_y, cur_fy);

	if (s->draw == DRAW_ABS || s->draw == DRAW_ABS_COL)
	{
		int l = (int)lround(s->x + SCREEN_CX);
		int t = (int)lround(s->y + SCREEN_CY);
		g_string_append_printf(g, "%sRECT %s_dst = { %d, %d, %d, %d };\n",
			ind, v, l, t, (int)lround(s->w), (int)lround(s->h));
		if (s->blend)
			g_string_append_printf(g, "%sGfx_BlendTex(&this->%s, &%s_src, &%s_dst, %d);\n",
				ind, s->tex_field, v, v, s->blend_mode);
		else
			g_string_append_printf(g, "%sGfx_DrawTex(&this->%s, &%s_src, &%s_dst);\n",
				ind, s->tex_field, v, v);
		return;
	}

	if (s->arb_valid)
	{
		for (int k = 0; k < 4; k++)
		{
			char *ax = double_to_fixed(s->arb[k][0]);
			char *ay = double_to_fixed(s->arb[k][1]);
			g_string_append_printf(g, "%sPOINT_FIXED %s_d%d = { %s%s, %s%s };\n",
				ind, v, k, ax, s->par_x != 0.0 ? " - fx" : "",
				ay, s->par_y != 0.0 ? " - fy" : "");
			g_free(ax); g_free(ay);
		}
		if (s->has_color)
			g_string_append_printf(g, "%sStage_DrawTexArbCol(&this->%s, &%s_src, &%s_d0, &%s_d1, &%s_d2, &%s_d3, %d, %d, %d, stage.camera.bzoom, stage.camera.angle);\n",
				ind, s->tex_field, v, v, v, v, v,
				s->color[0], s->color[1], s->color[2]);
		else if (s->blend)
			g_string_append_printf(g, "%sStage_BlendTexArb(&this->%s, &%s_src, &%s_d0, &%s_d1, &%s_d2, &%s_d3, stage.camera.bzoom, stage.camera.angle, %d);\n",
				ind, s->tex_field, v, v, v, v, v, s->blend_mode);
		else
			g_string_append_printf(g, "%sStage_DrawTexArb(&this->%s, &%s_src, &%s_d0, &%s_d1, &%s_d2, &%s_d3, stage.camera.bzoom, stage.camera.angle);\n",
				ind, s->tex_field, v, v, v, v, v);
		return;
	}

	// RECT_FIXED dst; dyn_x/dyn_y re-emit verbatim (Week4 car pattern)
	char *fx0 = s->dyn_x ? g_strdup(s->dyn_x) : double_to_fixed(s->x);
	char *fy0 = s->dyn_y ? g_strdup(s->dyn_y) : double_to_fixed(s->y);
	char *fw = double_to_fixed(s->w);
	char *fh = double_to_fixed(s->h);
	g_string_append_printf(g, "%sRECT_FIXED %s_dst = {\n", ind, v);
	g_string_append_printf(g, "%s%s%s,\n", ind2, fx0,
		(!s->dyn_x && s->par_x != 0.0) ? " - fx" : "");
	g_string_append_printf(g, "%s%s%s,\n", ind2, fy0,
		(!s->dyn_y && s->par_y != 0.0) ? " - fy" : "");
	g_string_append_printf(g, "%s%s,\n%s%s\n%s};\n", ind2, fw, ind2, fh, ind);
	g_free(fx0); g_free(fy0); g_free(fw); g_free(fh);

	if (s->blend)
		g_string_append_printf(g, "%sStage_BlendTexV2(&this->%s, &%s_src, &%s_dst, stage.camera.bzoom, %d, %d);\n",
			ind, s->tex_field, v, v, s->blend_mode, s->opacity < 0 ? 255 : s->opacity);
	else if (s->flip)
		g_string_append_printf(g, "%sStage_DrawTex_FlipX(&this->%s, &%s_src, &%s_dst, stage.camera.bzoom, stage.camera.angle);\n",
			ind, s->tex_field, v, v);
	else if (s->has_color)
		g_string_append_printf(g, "%sStage_DrawTexCol(&this->%s, &%s_src, &%s_dst, stage.camera.bzoom, stage.camera.angle, %d, %d, %d);\n",
			ind, s->tex_field, v, v, s->color[0], s->color[1], s->color[2]);
	else
		g_string_append_printf(g, "%sStage_DrawTex(&this->%s, &%s_src, &%s_dst, stage.camera.bzoom, stage.camera.angle);\n",
			ind, s->tex_field, v, v);
}

// Distinct playback slots used by a set's instances (sorted).
static int omni_set_slots(Project *pr, AnimSet *a, int *out, int cap)
{
	int n = 0;
	for (int i = 0; i < pr->sprite_count && n < cap; i++)
	{
		Sprite *s = pr->sprites[i];
		if (s->anim != a) continue;
		gboolean dup = FALSE;
		for (int j = 0; j < n; j++) if (out[j] == s->anim_inst) { dup = TRUE; break; }
		if (!dup) out[n++] = s->anim_inst;
	}
	for (int i = 0; i < n; i++)
		for (int j = i + 1; j < n; j++)
			if (out[j] < out[i]) { int t = out[i]; out[i] = out[j]; out[j] = t; }
	return n;
}

// TRUE when a set needs suffixed per-state symbols (more than the one
// classic shared slot). Single-state output stays byte-identical.
static gboolean omni_set_is_multi(Project *pr, AnimSet *a)
{
	int tmp[64];
	int n = omni_set_slots(pr, a, tmp, 64);
	return n > 1 || (n == 1 && tmp[0] != 0);
}

// Symbol suffix for an instance ("" for classic single-state).
static void omni_inst_suffix(Project *pr, Sprite *s, char *out, size_t n)
{
	if (s->anim && omni_set_is_multi(pr, s->anim))
		snprintf(out, n, "_%d", s->anim_inst);
	else
		out[0] = 0;
}

// Animation index played by (set, slot): first instance sprite wins.
static int omni_slot_anim(Project *pr, AnimSet *a, int slot)
{
	for (int i = 0; i < pr->sprite_count; i++)
	{
		Sprite *s = pr->sprites[i];
		if (s->anim == a && s->anim_inst == slot)
			return s->anim_anim;
	}
	return 0;
}

// Duplicate a raw helper, renaming per-instance state members
// "-><aid>_frame" -> "-><aid>_frame_<k>" (same for _tex_id) and the
// function name <fn_old> -> <fn_new>. Shared refs (tex field, arc ptr,
// frame tables) are left untouched.
static char *omni_variant_text(const char *raw, const char *aid,
                               const char *fn_old, const char *fn_new, int k)
{
	GString *g = g_string_new(NULL);
	const char *p = raw;
	size_t on = strlen(fn_old);
	if (strncmp(p, "void ", 5) == 0 && strncmp(p + 5, fn_old, on) == 0 &&
	    p[5 + on] == '(')
	{
		g_string_append(g, "void ");
		g_string_append(g, fn_new);
		p += 5 + on;
	}
	char pats[2][160];
	snprintf(pats[0], sizeof(pats[0]), "->%s_frame", aid);
	snprintf(pats[1], sizeof(pats[1]), "->%s_tex_id", aid);
	char kb[16];
	snprintf(kb, sizeof(kb), "_%d", k);
	while (*p)
	{
		gboolean done = FALSE;
		for (int t = 0; t < 2 && !done; t++)
		{
			size_t L = strlen(pats[t]);
			if (strncmp(p, pats[t], L) == 0)
			{
				char nc = p[L];
				if (!isalnum((unsigned char)nc) && nc != '_')
				{
					g_string_append(g, pats[t]);
					g_string_append(g, kb);
					p += L;
					done = TRUE;
				}
			}
		}
		if (!done)
			g_string_append_c(g, *p++);
	}
	return g_string_free(g, FALSE);
}

// Emit one animated-actor instance call.
static void omni_emit_instance(GString *g, Project *pr, Sprite *s,
                               const char *ind, double *cur_fx, double *cur_fy)
{
	AnimSet *a = s->anim;
	char sfx[16];
	omni_inst_suffix(pr, s, sfx, sizeof(sfx));
	char drawfn[220];
	snprintf(drawfn, sizeof(drawfn), "%s%s", a->draw_fn, sfx);
	omni_emit_par(g, ind, "fx", "x", s->par_x, cur_fx);
	omni_emit_par(g, ind, "fy", "y", s->par_y, cur_fy);
	// dyn_x/dyn_y hold raw code-driven position args (kitchen hands)
	char *x = s->dyn_x ? g_strdup(s->dyn_x) : double_to_fixed(s->anim_x);
	char *y = s->dyn_y ? g_strdup(s->dyn_y) : double_to_fixed(s->anim_y);
	g_string_append_printf(g, "%s%s(this, %s%s, %s%s);\n",
		ind, drawfn, x, (!s->dyn_x && s->par_x != 0.0) ? " - fx" : "",
		y, (!s->dyn_y && s->par_y != 0.0) ? " - fy" : "");
	g_free(x); g_free(y);
}

static gboolean omni_layer_has(Project *pr, StageLayer layer)
{
	for (int i = 0; i < pr->sprite_count; i++)
		if (pr->sprites[i]->layer == layer)
			return TRUE;
	return FALSE;
}

static void omni_emit_layer(GString *g, Project *pr, StageLayer layer, const char *fn)
{
	int *idx = NULL; int n = 0;
	AnimSet **sets = NULL; int *setk = NULL; int nsets = 0;
	for (int i = 0; i < pr->sprite_count; i++)
	{
		if (pr->sprites[i]->layer != layer) continue;
		idx = g_realloc(idx, sizeof(int) * (size_t)(n + 1));
		idx[n++] = i;
		AnimSet *a = pr->sprites[i]->anim;
		if (a)
		{
			int slot = pr->sprites[i]->anim_inst;
			gboolean dup = FALSE;
			for (int k = 0; k < nsets; k++)
				if (sets[k] == a && setk[k] == slot) { dup = TRUE; break; }
			if (!dup)
			{
				sets = g_realloc(sets, sizeof(AnimSet*) * (size_t)(nsets + 1));
				setk = g_realloc(setk, sizeof(int) * (size_t)(nsets + 1));
				sets[nsets] = a;
				setk[nsets] = slot;
				nsets++;
			}
		}
	}
	if (n == 0) { g_free(idx); g_free(sets); g_free(setk); return; }

	// fx/fy decl only when something actually uses them
	gboolean need_fx = FALSE, need_fy = FALSE;
	for (int k = 0; k < n; k++)
	{
		Sprite *s = pr->sprites[idx[k]];
		if (s->par_x != 0.0 || (s->dyn_x && strstr(s->dyn_x, "fx"))) need_fx = TRUE;
		if (s->par_y != 0.0 || (s->dyn_y && strstr(s->dyn_y, "fy"))) need_fy = TRUE;
	}

	g_string_append_printf(g, "void %s_%s(StageBack *back)\n{\n", pr->prefix, fn);
	g_string_append_printf(g, "\t%s *this = (%s*)back;\n\t\n", pr->prefix, pr->prefix);
	if (need_fx || need_fy)
		g_string_append(g, "\tfixed_t fx, fy;\n\t\n");
	else
		g_string_append(g, "\t(void)back;\n\t\n");

	for (int k = 0; k < nsets; k++)
	{
		AnimSet *a = sets[k];
		int slot = setk[k];
		if (!a->animatable[0]) continue;
		if (omni_set_is_multi(pr, a))
			g_string_append_printf(g, "\tAnimatable_Animate(&this->%s_%d, (void*)this, %s_SetFrame_%d);\n",
				a->animatable, slot, a->fnbase, slot);
		else
			g_string_append_printf(g, "\tAnimatable_Animate(&this->%s, (void*)this, %s_SetFrame);\n",
				a->animatable, a->fnbase);
	}
	if (nsets > 0)
		g_string_append(g, "\t\n");

	double cur_fx = 1e99, cur_fy = 1e99;
	for (int k = 0; k < n; k++)
	{
		Sprite *s = pr->sprites[idx[k]];
		if (s->cond)
		{
			double cx = 1e99, cy = 1e99;
			g_string_append_printf(g, "\tif (%s)\n\t{\n", s->cond);
			if (s->anim)
				omni_emit_instance(g, pr, s, "\t\t", &cx, &cy);
			else
				omni_emit_sprite(g, s, idx[k], "\t\t", &cx, &cy);
			g_string_append(g, "\t}\n\n");
			// assignments inside the guard may or may not run: re-emit after
			cur_fx = 1e99; cur_fy = 1e99;
		}
		else
		{
			if (s->anim)
				omni_emit_instance(g, pr, s, "\t", &cur_fx, &cur_fy);
			else
				omni_emit_sprite(g, s, idx[k], "\t", &cur_fx, &cur_fy);
			g_string_append(g, "\n");
		}
	}
	g_string_append(g, "}\n");
	g_free(idx);
	g_free(sets);
	g_free(setk);
}

char *export_omni_source(Project *pr)
{
	GString *g = g_string_new(NULL);
	const char *stage = pr->week ? pr->week : "stage";
	const char *P = pr->prefix ? pr->prefix : "Back_Stage";
	char upper[128];
	omni_stage_upper(stage, upper, sizeof(upper));

	g_string_append(g,
		"/*\n"
		"  This Source Code Form is subject to the terms of the Mozilla Public\n"
		"  License, v. 2.0. If a copy of the MPL was not distributed with this\n"
		"  file, You can obtain one at http://mozilla.org/MPL/2.0/.\n"
		"*/\n"
		"\n"
		"//Generated with psxtools (Omniplasm stage format).\n"
		"//Remember to register the New() constructor in your stage map/def\n"
		"//and to pack the ARC files it loads.\n"
		"\n");
	g_string_append_printf(g, "#include \"%s.h\"\n\n", stage);
	g_string_append(g,
		"#include \"../archive.h\"\n"
		"#include \"../mem.h\"\n"
		"#include \"../stage.h\"\n");
	if (pr->anim_count > 0)
		g_string_append(g, "#include \"../animation.h\"\n");
	g_string_append(g, "\n");

	// ---- struct ----
	g_string_append_printf(g, "//%s background structure\ntypedef struct\n{\n", P);
	g_string_append(g, "\t//Stage background base structure\n\tStageBack back;\n");
	for (int i = 0; i < pr->anim_count; i++)
	{
		AnimSet *a = pr->anims[i];
		if (a->arc_var[0])
			g_string_append_printf(g, "\t\n\tIO_Data %s, %s_ptr[%d];\n",
				a->arc_var, a->arc_var, a->tim_count > 0 ? a->tim_count : 1);
	}
	g_string_append(g, "\t\n\t//Textures\n");
	for (int i = 0; i < pr->omni_tex_count; i++)
	{
		gboolean dup = FALSE;
		for (int j = 0; j < i; j++)
			if (strcmp(pr->omni_tex_fields[j], pr->omni_tex_fields[i]) == 0)
				{ dup = TRUE; break; }
		if (dup) continue;
		if (pr->omni_tex_tims[i][0])
			g_string_append_printf(g, "\tGfx_Tex %s; //%s\n",
				pr->omni_tex_fields[i], pr->omni_tex_tims[i]);
		else
			g_string_append_printf(g, "\tGfx_Tex %s;\n", pr->omni_tex_fields[i]);
	}
	for (int i = 0; i < pr->anim_count; i++)
	{
		AnimSet *a = pr->anims[i];
		// anim tex fields already covered by the New() load order above
		// need no second decl; others (fresh actors) do
		gboolean in_loads = FALSE;
		for (int j = 0; j < pr->omni_tex_count; j++)
			if (strcmp(pr->omni_tex_fields[j], a->tex_field) == 0)
				{ in_loads = TRUE; break; }
		if (!in_loads)
			g_string_append_printf(g, "\tGfx_Tex %s; //animated %s\n",
				a->tex_field, a->id);
	}
	for (int i = 0; i < pr->anim_count; i++)
	{
		AnimSet *a = pr->anims[i];
		int slots[64];
		int nslots = omni_set_slots(pr, a, slots, 64);
		gboolean multi = omni_set_is_multi(pr, a);
		if (a->id[0])
		{
			g_string_append_printf(g, "\t\n\t//%s state\n", a->sym);
			if (!multi)
				g_string_append_printf(g, "\tu8 %s_frame, %s_tex_id;\n",
					a->id, a->id);
			else
				for (int k = 0; k < nslots; k++)
					g_string_append_printf(g, "\tu8 %s_frame_%d, %s_tex_id_%d;\n",
						a->id, slots[k], a->id, slots[k]);
		}
		if (a->animatable[0])
		{
			if (!multi)
				g_string_append_printf(g, "\tAnimatable %s;\n", a->animatable);
			else
				for (int k = 0; k < nslots; k++)
					g_string_append_printf(g, "\tAnimatable %s_%d;\n",
						a->animatable, slots[k]);
		}
	}
	g_string_append_printf(g, "} %s;\n", P);

	// ---- animation tables + helpers (verbatim) ----
	for (int i = 0; i < pr->anim_count; i++)
	{
		AnimSet *a = pr->anims[i];
		if (a->frames_raw)
		{
			g_string_append_printf(g, "\n//%s animation and rects\nstatic const CharFrame %s[] = {\n",
				a->sym, a->frame_array);
			omni_raw_block(g, a->frames_raw);
			g_string_append(g, "};\n");
		}
		if (a->anims_raw)
		{
			g_string_append_printf(g, "\nstatic const Animation %s[] = {\n", a->anim_array);
			omni_raw_block(g, a->anims_raw);
			g_string_append(g, "};\n");
		}
		if (a->setframe_raw)
		{
			g_string_append(g, "\n");
			if (!omni_set_is_multi(pr, a))
				omni_raw_block(g, a->setframe_raw);
			else
			{
				int slots[64];
				int nslots = omni_set_slots(pr, a, slots, 64);
				char sf[192], df[192];
				snprintf(sf, sizeof(sf), "%s_SetFrame", a->fnbase);
				for (int k = 0; k < nslots; k++)
				{
					snprintf(df, sizeof(df), "%s_SetFrame_%d", a->fnbase, slots[k]);
					char *v = omni_variant_text(a->setframe_raw, a->id,
					                            sf, df, slots[k]);
					omni_raw_block(g, v);
					g_free(v);
				}
			}
		}
		if (a->drawhelper_raw)
		{
			g_string_append(g, "\n");
			if (!omni_set_is_multi(pr, a))
				omni_raw_block(g, a->drawhelper_raw);
			else
			{
				int slots[64];
				int nslots = omni_set_slots(pr, a, slots, 64);
				char sf[192], df[192];
				snprintf(sf, sizeof(sf), "%s_Draw", a->fnbase);
				for (int k = 0; k < nslots; k++)
				{
					snprintf(df, sizeof(df), "%s_Draw_%d", a->fnbase, slots[k]);
					char *v = omni_variant_text(a->drawhelper_raw, a->id,
					                            sf, df, slots[k]);
					omni_raw_block(g, v);
					g_free(v);
				}
			}
		}
	}

	// ---- per-layer draw functions ----
	static const char *fns[4] = {"DrawBG", "DrawMD", "DrawFG", "DrawHUD"};
	for (int L = 0; L < 4; L++)
	{
		if (!omni_layer_has(pr, (StageLayer)L)) continue;
		g_string_append(g, "\n");
		omni_emit_layer(g, pr, (StageLayer)L, fns[L]);
	}

	// ---- Free ----
	g_string_append_printf(g, "\nvoid %s_Free(StageBack *back)\n{\n", P);
	g_string_append_printf(g, "\t%s *this = (%s*)back;\n\t\n", P, P);
	for (int i = 0; i < pr->anim_count; i++)
	{
		if (pr->anims[i]->arc_var[0])
			g_string_append_printf(g, "\t//Free %s archive\n\tMem_Free(this->%s);\n\t\n",
				pr->anims[i]->id, pr->anims[i]->arc_var);
	}
	g_string_append(g, "\t//Free structure\n\tMem_Free(this);\n}\n");

	// ---- New ----
	g_string_append_printf(g, "\nStageBack *%s_New(void)\n{\n", P);
	g_string_append_printf(g, "\t//Allocate background structure\n\t%s *this = (%s*)Mem_Alloc(sizeof(%s));\n", P, P, P);
	g_string_append(g, "\tif (this == NULL)\n\t\treturn NULL;\n\t\n");
	g_string_append(g, "\t//Set background functions\n");
	static const char *dfns[4] = {"draw_bg", "draw_md", "draw_fg", "draw_hud"};
	for (int L = 0; L < 4; L++)
	{
		if (omni_layer_has(pr, (StageLayer)L))
			g_string_append_printf(g, "\tthis->back.%s = %s_%s;\n",
				dfns[L], P, fns[L]);
		else
			g_string_append_printf(g, "\tthis->back.%s = NULL;\n", dfns[L]);
	}
	g_string_append_printf(g, "\tthis->back.free = %s_Free;\n", P);

	// ---- static texture loads, grouped by ARC ----
	int nstatic = 0;
	for (int i = 0; i < pr->omni_tex_count; i++)
	{
		if (omni_field_owned_by_anim(pr, pr->omni_tex_fields[i])) continue;
		gboolean seen = FALSE;
		for (int j = 0; j < i; j++)
			if (strcmp(pr->omni_tex_fields[j], pr->omni_tex_fields[i]) == 0)
				{ seen = TRUE; break; }
		if (!seen) nstatic++;
	}
	if (nstatic > 0)
	{
		char **gpaths = NULL; int ng = 0;
		for (int i = 0; i < pr->omni_tex_count; i++)
		{
			if (omni_field_owned_by_anim(pr, pr->omni_tex_fields[i])) continue;
			const char *ap = pr->omni_tex_arcs[i];
			if (!ap[0]) continue;
			gboolean found = FALSE;
			for (int k = 0; k < ng; k++)
				if (strcmp(gpaths[k], ap) == 0) { found = TRUE; break; }
			if (!found)
			{
				gpaths = g_realloc(gpaths, sizeof(char*) * (size_t)(ng + 1));
				gpaths[ng++] = (char*)ap;
			}
		}
		gboolean need_def = FALSE;
		for (int i = 0; i < pr->omni_tex_count; i++)
		{
			if (omni_field_owned_by_anim(pr, pr->omni_tex_fields[i])) continue;
			if (!pr->omni_tex_arcs[i][0]) { need_def = TRUE; break; }
		}
		char defarc[160];
		snprintf(defarc, sizeof(defarc), "\\\\%s\\\\BACK.ARC;1", upper);
		int ngroups = ng + (need_def ? 1 : 0);
		g_string_append(g, "\t\n\t//Load background textures\n");
		for (int gi = 0; gi < ngroups; gi++)
		{
			const char *gp = gi < ng ? gpaths[gi] : defarc;
			char var[32];
			if (gi == 0) snprintf(var, sizeof(var), "arc_back");
			else snprintf(var, sizeof(var), "arc_back%d", gi + 1);
			g_string_append_printf(g, "\tIO_Data %s = IO_Read(\"%s\");\n", var, gp);
			for (int i = 0; i < pr->omni_tex_count; i++)
			{
				if (omni_field_owned_by_anim(pr, pr->omni_tex_fields[i])) continue;
				gboolean seen = FALSE;
				for (int j = 0; j < i; j++)
					if (strcmp(pr->omni_tex_fields[j], pr->omni_tex_fields[i]) == 0)
						{ seen = TRUE; break; }
				if (seen) continue;
				const char *ap = pr->omni_tex_arcs[i];
				gboolean mine = (gi < ng) ? (ap[0] && strcmp(ap, gp) == 0) : (!ap[0]);
				if (!mine) continue;
				const char *field = pr->omni_tex_fields[i];
				char tfall[128];
				if (pr->omni_tex_tims[i][0])
					snprintf(tfall, sizeof(tfall), "%s", pr->omni_tex_tims[i]);
				else if (strncmp(field, "tex_", 4) == 0)
					snprintf(tfall, sizeof(tfall), "%s.tim", field + 4);
				else
					snprintf(tfall, sizeof(tfall), "%s.tim", field);
				g_string_append_printf(g, "\tGfx_LoadTex(&this->%s, Archive_Find(%s, \"%s\"), 0);\n",
					field, var, tfall);
			}
			g_string_append_printf(g, "\tMem_Free(%s);\n", var);
		}
		g_free(gpaths);
	}

	// ---- animated actor loads + state ----
	for (int i = 0; i < pr->anim_count; i++)
	{
		AnimSet *a = pr->anims[i];
		g_string_append_printf(g, "\t\n\t//Load %s textures\n", a->id);
		if (a->arc_path[0] && a->arc_var[0])
		{
			g_string_append_printf(g, "\tthis->%s = IO_Read(\"%s\");\n",
				a->arc_var, a->arc_path);
			for (int t = 0; t < a->tim_count; t++)
				if (a->tims[t])
					g_string_append_printf(g, "\tthis->%s_ptr[%d] = Archive_Find(this->%s, \"%s\");\n",
						a->arc_var, t, a->arc_var, a->tims[t]);
		}
		if (a->animatable[0] && a->anim_array[0])
		{
			int slots[64];
			int nslots = omni_set_slots(pr, a, slots, 64);
			gboolean multi = omni_set_is_multi(pr, a);
			if (!multi && nslots == 0) { slots[0] = 0; nslots = 1; }
			for (int k = 0; k < nslots; k++)
			{
				char sfx[16];
				if (multi)
					snprintf(sfx, sizeof(sfx), "_%d", slots[k]);
				else
					sfx[0] = 0;
				int anim = omni_slot_anim(pr, a, slots[k]);
				g_string_append_printf(g, "\t\n\tAnimatable_Init(&this->%s%s, %s);\n",
					a->animatable, sfx, a->anim_array);
				g_string_append_printf(g, "\tAnimatable_SetAnim(&this->%s%s, %d);\n",
					a->animatable, sfx, anim);
				if (a->id[0])
					g_string_append_printf(g, "\tthis->%s_frame%s = this->%s_tex_id%s = 0xFF; //Force art load\n",
						a->id, sfx, a->id, sfx);
			}
		}
	}

	g_string_append(g, "\t\n\treturn (StageBack*)this;\n}\n");
	return g_string_free(g, FALSE);
}

char *export_omni_header(Project *pr)
{
	const char *stage = pr->week ? pr->week : "stage";
	const char *P = pr->prefix ? pr->prefix : "Back_Stage";
	char upper[128];
	omni_stage_upper(stage, upper, sizeof(upper));
	GString *g = g_string_new(NULL);
	g_string_append(g,
		"/*\n"
		"  This Source Code Form is subject to the terms of the Mozilla Public\n"
		"  License, v. 2.0. If a copy of the MPL was not distributed with this\n"
		"  file, You can obtain one at http://mozilla.org/MPL/2.0/.\n"
		"*/\n"
		"\n");
	g_string_append_printf(g,
		"#ifndef PSXF_GUARD_%s_H\n#define PSXF_GUARD_%s_H\n\n", upper, upper);
	g_string_append(g, "#include \"../stage.h\"\n\n");
	g_string_append_printf(g, "//%s functions\nStageBack *%s_New();\n\n#endif\n", P, P);
	return g_string_free(g, FALSE);
}

void editor_write_export(EditorStage *ed, const char *path)
{
	char *text = gen_export_text(ed);
	if (!text) return;
	FILE *f = fopen(path, "w");
	if (f) { fputs(text, f); fclose(f); }
	g_free(text);
}

// Omni export writes the .c plus its sibling .h next to it.
void editor_write_export_omni(EditorStage *ed, const char *c_path)
{
	char *text = export_omni_source(ed->proj);
	if (text)
	{
		FILE *f = fopen(c_path, "w");
		if (f) { fputs(text, f); fclose(f); }
		g_free(text);
	}
	char h_path[4096];
	size_t L = strlen(c_path);
	if (L > 2 && strcmp(c_path + L - 2, ".c") == 0)
		snprintf(h_path, sizeof(h_path), "%.*s.h", (int)(L - 2), c_path);
	else
		snprintf(h_path, sizeof(h_path), "%s.h", c_path);
	char *h = export_omni_header(ed->proj);
	if (h)
	{
		FILE *f = fopen(h_path, "w");
		if (f) { fputs(h, f); fclose(f); }
		g_free(h);
	}
}

static void editor_open(EditorStage *ed)
{
	GtkWidget *dlg = gtk_file_chooser_dialog_new(
		"Open project (weekN.c or stage/*.c)...", GTK_WINDOW(ed->app->window),
		GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL,
		"_Open", GTK_RESPONSE_ACCEPT, NULL);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), "src");
	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT)
	{
		char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
		Project *pr = project_load(path);
		if (pr && (pr->sprite_count > 0 || pr->anim_count > 0))
		{
			if (ed->proj) project_free(ed->proj);
			ed->proj = pr;
			ed->app->project = pr;   /* shared, borrowed: see app.h */
			ed->has_proj = TRUE;
			ed->sel = 0;
			ed->zoom = 1.0;
			ed->pan_x = ed->pan_y = 0.0;
			load_images(ed);
			editor_rebuild_layers(ed);
			editor_refresh_props(ed);
			editor_refresh_weekinfo(ed);
			if (pr->kind == PROJ_OMNI_STAGE)
			{
				char msg[256];
				snprintf(msg, sizeof(msg),
					"Stage %s (%s): %d pieces, %d animated actor(s).",
					pr->week, pr->prefix,
					pr->sprite_count, pr->anim_count);
				gtk_label_set_text(GTK_LABEL(ed->status), msg);
			}
			editor_redraw(ed);
		}
		else
		{
			gtk_label_set_text(GTK_LABEL(ed->status),
				"Could not parse (weekN.c with Load/DrawBG or stage Back_* with DrawBG/MD/FG/HUD)");
			if (pr) project_free(pr);
		}
		g_free(path);
	}
	gtk_widget_destroy(dlg);
}

static void editor_export(EditorStage *ed)
{
	if (!ed->has_proj || !ed->proj)
	{
		gtk_label_set_text(GTK_LABEL(ed->status), "Open a project first.");
		return;
	}
	gboolean omni = ed->proj->kind == PROJ_OMNI_STAGE;
	GtkWidget *dlg = gtk_file_chooser_dialog_new(
		omni ? "Save stage (Back_X.c + .h)" : "Save C code (DrawBG)",
		GTK_WINDOW(ed->app->window),
		GTK_FILE_CHOOSER_ACTION_SAVE, "_Cancel", GTK_RESPONSE_CANCEL,
		"_Save", GTK_RESPONSE_ACCEPT, NULL);
	char def[128];
	if (omni)
		snprintf(def, sizeof(def), "%s.c", ed->proj->week);
	else
		snprintf(def, sizeof(def), "%s_drawbg_export.c", ed->proj->prefix);
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), def);
	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT)
	{
		char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
		if (omni)
			editor_write_export_omni(ed, path);
		else
			editor_write_export(ed, path);
		g_free(path);
	}
	gtk_widget_destroy(dlg);
}

// ------------------------------------------------------------------ module
static void on_layer_up_down(GtkButton *btn, gpointer user)
{
	(void)user;
	EditorStage *ed = g_object_get_data(G_OBJECT(btn), "ed");
	int delta = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "delta"));
	if (ed) layer_move(ed, delta);
}

GtkWidget *editor_stage_new(App *app)
{
	EditorStage *ed = calloc(1, sizeof(EditorStage));
	ed->app = app;
	ed->zoom = 1.0;
	ed->sel = -1;
	ed->has_proj = FALSE;
	ed->dragging = FALSE;
	ed->drag_target = -1;
	ed->float_win = NULL;

	// horizontal divider: canvas | dock (draggable -> resizable)
	GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_paned_set_position(GTK_PANED(paned), 320);
	ed->paned = paned;

	// canvas
	ed->drawing = gtk_drawing_area_new();
	g_signal_connect(ed->drawing, "draw", G_CALLBACK(canvas_draw), ed);
	g_signal_connect(ed->drawing, "button-press-event", G_CALLBACK(canvas_button), ed);
	g_signal_connect(ed->drawing, "motion-notify-event", G_CALLBACK(canvas_motion), ed);
	g_signal_connect(ed->drawing, "button-release-event", G_CALLBACK(canvas_release), ed);
	g_signal_connect(ed->drawing, "scroll-event", G_CALLBACK(canvas_scroll), ed);
	gtk_widget_add_events(ed->drawing, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
		GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK);
	gtk_paned_pack1(GTK_PANED(paned), ed->drawing, TRUE, TRUE);

	// right dock
	GtkWidget *side = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_widget_set_size_request(side, 250, -1);
	ed->side = side;
	gtk_paned_pack2(GTK_PANED(paned), side, FALSE, FALSE);

	GtkWidget *tb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(side), tb, FALSE, FALSE, 2);
	GtkWidget *b_open = gtk_button_new_with_label("Open project...");
	g_signal_connect_swapped(b_open, "clicked", G_CALLBACK(editor_open), ed);
	gtk_box_pack_start(GTK_BOX(tb), b_open, FALSE, FALSE, 0);
	GtkWidget *b_exp = gtk_button_new_with_label("Export C...");
	g_signal_connect_swapped(b_exp, "clicked", G_CALLBACK(editor_export), ed);
	gtk_box_pack_start(GTK_BOX(tb), b_exp, FALSE, FALSE, 0);
	ed->btn_undock = gtk_button_new_with_label("Undock");
	g_signal_connect(ed->btn_undock, "clicked", G_CALLBACK(editor_toggle_dock), ed);
	gtk_box_pack_start(GTK_BOX(tb), ed->btn_undock, FALSE, FALSE, 0);

	// zoom
	GtkWidget *zb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(side), zb, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(zb), gtk_label_new("Zoom:"), FALSE, FALSE, 0);
	ed->zoom_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.1, 8.0, 0.05);
	gtk_range_set_value(GTK_RANGE(ed->zoom_scale), 1.0);
	g_signal_connect(ed->zoom_scale, "value-changed", G_CALLBACK(zoom_changed), ed);
	gtk_widget_set_hexpand(ed->zoom_scale, TRUE);
	gtk_box_pack_start(GTK_BOX(zb), ed->zoom_scale, TRUE, TRUE, 0);

	// notebook: wrap the whole dock in tabs so low res can switch sections
	GtkWidget *nb = gtk_notebook_new();
	gtk_widget_set_vexpand(nb, TRUE);
	gtk_box_pack_start(GTK_BOX(side), nb, TRUE, TRUE, 2);

	// ---- tab "Stage": layers + properties ----
	GtkWidget *tab_es = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

	GtkWidget *ll = gtk_label_new("Layers (draw order)");
	gtk_widget_set_halign(ll, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(tab_es), ll, FALSE, FALSE, 2);
	GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_vexpand(sc, TRUE);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(tab_es), sc, TRUE, TRUE, 2);
	ed->layer_list = gtk_list_box_new();
	gtk_container_add(GTK_CONTAINER(sc), ed->layer_list);
	g_signal_connect(ed->layer_list, "row-selected", G_CALLBACK(layer_selected), ed);

	GtkWidget *lbtn = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_box_pack_start(GTK_BOX(tab_es), lbtn, FALSE, FALSE, 2);
	GtkWidget *b_up = gtk_button_new_with_label("Up");
	g_object_set_data(G_OBJECT(b_up), "ed", ed);
	g_object_set_data(G_OBJECT(b_up), "delta", GINT_TO_POINTER(-1));
	g_signal_connect(b_up, "clicked", G_CALLBACK(on_layer_up_down), NULL);
	gtk_box_pack_start(GTK_BOX(lbtn), b_up, TRUE, FALSE, 0);
	GtkWidget *b_dn = gtk_button_new_with_label("Down");
	g_object_set_data(G_OBJECT(b_dn), "ed", ed);
	g_object_set_data(G_OBJECT(b_dn), "delta", GINT_TO_POINTER(1));
	g_signal_connect(b_dn, "clicked", G_CALLBACK(on_layer_up_down), NULL);
	gtk_box_pack_start(GTK_BOX(lbtn), b_dn, TRUE, FALSE, 0);
	GtkWidget *b_dup = gtk_button_new_with_label("Dup");
	g_signal_connect_swapped(b_dup, "clicked", G_CALLBACK(layer_dup), ed);
	gtk_box_pack_start(GTK_BOX(lbtn), b_dup, TRUE, FALSE, 0);
	GtkWidget *b_del = gtk_button_new_with_label("Del");
	g_signal_connect_swapped(b_del, "clicked", G_CALLBACK(layer_del), ed);
	gtk_box_pack_start(GTK_BOX(lbtn), b_del, TRUE, FALSE, 0);

	GtkWidget *pl = gtk_label_new("Properties");
	gtk_widget_set_halign(pl, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(tab_es), pl, FALSE, FALSE, 2);
	GtkWidget *props_sc = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_vexpand(props_sc, TRUE);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(props_sc),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(tab_es), props_sc, TRUE, TRUE, 2);
	ed->props_grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(ed->props_grid), 6);
	gtk_grid_set_row_spacing(GTK_GRID(ed->props_grid), 3);
	gtk_container_add(GTK_CONTAINER(props_sc), ed->props_grid);

	int r = 0;
	const char *labels[4] = {"X", "Y", "W", "H"};
	const char *pkeys[4] = {"x", "y", "w", "h"};
	GtkWidget **targets[4] = {&ed->pw_x, &ed->pw_y, &ed->pw_w, &ed->pw_h};
	for (int k = 0; k < 4; k++)
	{
		GtkWidget *sp = prop_row(ed->props_grid, r, labels[k]);
		*targets[k] = sp;
		g_object_set_data(G_OBJECT(sp), "prop", (gpointer)pkeys[k]);
		g_signal_connect(sp, "value-changed", G_CALLBACK(prop_spin), ed);
		r++;
	}

	const char *slabels[4] = {"src x", "src y", "src w", "src h"};
	GtkWidget **stargets[4] = {&ed->pw_src0, &ed->pw_src1, &ed->pw_src2, &ed->pw_src3};
	for (int k = 0; k < 4; k++)
	{
		GtkWidget *sp = gtk_spin_button_new_with_range(0, 4096, 1);
		gtk_widget_set_hexpand(sp, TRUE);
		GtkWidget *lbl = gtk_label_new(slabels[k]);
		gtk_widget_set_halign(lbl, GTK_ALIGN_START);
		gtk_grid_attach(GTK_GRID(ed->props_grid), lbl, 0, r, 1, 1);
		gtk_grid_attach(GTK_GRID(ed->props_grid), sp, 1, r, 1, 1);
		*stargets[k] = sp;
		g_object_set_data(G_OBJECT(sp), "prop", GINT_TO_POINTER(k));
		g_signal_connect(sp, "value-changed", G_CALLBACK(prop_src_spin), ed);
		r++;
	}

	// flip checkbox
	{
		GtkWidget *fl = gtk_label_new("Flip X");
		gtk_widget_set_halign(fl, GTK_ALIGN_START);
		gtk_grid_attach(GTK_GRID(ed->props_grid), fl, 0, r, 1, 1);
		ed->pw_flip = gtk_check_button_new();
		gtk_grid_attach(GTK_GRID(ed->props_grid), ed->pw_flip, 1, r, 1, 1);
		g_signal_connect(ed->pw_flip, "toggled", G_CALLBACK(prop_flip), ed);
		r++;
	}

	// parallax
	const char *plabels[2] = {"parallax x*", "parallax y*"};
	const char *pkeys2[2] = {"px", "py"};
	GtkWidget **ptargets[2] = {&ed->pw_px, &ed->pw_py};
	for (int k = 0; k < 2; k++)
	{
		GtkWidget *sp = gtk_spin_button_new_with_range(-4, 4, 0.05);
		gtk_spin_button_set_digits(GTK_SPIN_BUTTON(sp), 2);
		gtk_widget_set_hexpand(sp, TRUE);
		GtkWidget *lbl = gtk_label_new(plabels[k]);
		gtk_widget_set_halign(lbl, GTK_ALIGN_START);
		gtk_grid_attach(GTK_GRID(ed->props_grid), lbl, 0, r, 1, 1);
		gtk_grid_attach(GTK_GRID(ed->props_grid), sp, 1, r, 1, 1);
		*ptargets[k] = sp;
		g_object_set_data(G_OBJECT(sp), "prop", (gpointer)pkeys2[k]);
		g_signal_connect(sp, "value-changed", G_CALLBACK(prop_spin), ed);
		r++;
	}

	// stage layer (Omniplasm DrawBG/MD/FG/HUD; legacy weekN only uses bg)
	{
		GtkWidget *lbl = gtk_label_new("Layer");
		gtk_widget_set_halign(lbl, GTK_ALIGN_START);
		gtk_grid_attach(GTK_GRID(ed->props_grid), lbl, 0, r, 1, 1);
		ed->pw_layer = gtk_combo_box_text_new();
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ed->pw_layer), "bg");
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ed->pw_layer), "md");
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ed->pw_layer), "fg");
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ed->pw_layer), "hud");
		gtk_widget_set_hexpand(ed->pw_layer, TRUE);
		gtk_grid_attach(GTK_GRID(ed->props_grid), ed->pw_layer, 1, r, 1, 1);
		g_signal_connect(ed->pw_layer, "changed", G_CALLBACK(prop_layer), ed);
		r++;
	}

	// anim preview frame + blend params
	{
		GtkWidget *lbl = gtk_label_new("anim frame");
		gtk_widget_set_halign(lbl, GTK_ALIGN_START);
		gtk_grid_attach(GTK_GRID(ed->props_grid), lbl, 0, r, 1, 1);
		ed->pw_frame = gtk_spin_button_new_with_range(0, 63, 1);
		gtk_widget_set_hexpand(ed->pw_frame, TRUE);
		gtk_grid_attach(GTK_GRID(ed->props_grid), ed->pw_frame, 1, r, 1, 1);
		g_object_set_data(G_OBJECT(ed->pw_frame), "prop", (gpointer)"frame");
		g_signal_connect(ed->pw_frame, "value-changed", G_CALLBACK(prop_spin), ed);
		r++;
	}
	{
		GtkWidget *lbl = gtk_label_new("opacity (-1=dyn)");
		gtk_widget_set_halign(lbl, GTK_ALIGN_START);
		gtk_grid_attach(GTK_GRID(ed->props_grid), lbl, 0, r, 1, 1);
		ed->pw_opacity = gtk_spin_button_new_with_range(-1, 255, 1);
		gtk_widget_set_hexpand(ed->pw_opacity, TRUE);
		gtk_grid_attach(GTK_GRID(ed->props_grid), ed->pw_opacity, 1, r, 1, 1);
		g_object_set_data(G_OBJECT(ed->pw_opacity), "prop", (gpointer)"opacity");
		g_signal_connect(ed->pw_opacity, "value-changed", G_CALLBACK(prop_spin), ed);
		r++;
	}
	{
		GtkWidget *lbl = gtk_label_new("blend mode");
		gtk_widget_set_halign(lbl, GTK_ALIGN_START);
		gtk_grid_attach(GTK_GRID(ed->props_grid), lbl, 0, r, 1, 1);
		ed->pw_mode = gtk_spin_button_new_with_range(0, 7, 1);
		gtk_widget_set_hexpand(ed->pw_mode, TRUE);
		gtk_grid_attach(GTK_GRID(ed->props_grid), ed->pw_mode, 1, r, 1, 1);
		g_object_set_data(G_OBJECT(ed->pw_mode), "prop", (gpointer)"mode");
		g_signal_connect(ed->pw_mode, "value-changed", G_CALLBACK(prop_spin), ed);
		r++;
	}
	{
		GtkWidget *lbl = gtk_label_new("condition");
		gtk_widget_set_halign(lbl, GTK_ALIGN_START);
		gtk_grid_attach(GTK_GRID(ed->props_grid), lbl, 0, r, 1, 1);
		ed->pw_cond = gtk_label_new("(always)");
		gtk_widget_set_halign(ed->pw_cond, GTK_ALIGN_START);
		gtk_label_set_line_wrap(GTK_LABEL(ed->pw_cond), TRUE);
		gtk_label_set_selectable(GTK_LABEL(ed->pw_cond), TRUE);
		gtk_grid_attach(GTK_GRID(ed->props_grid), ed->pw_cond, 1, r, 1, 1);
		r++;
	}

	// week info (songs + characters) - read-only v1, in its own tab
	GtkWidget *tab_sem = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	{
		GtkWidget *w1 = gtk_label_new("Songs");
		gtk_widget_set_halign(w1, GTK_ALIGN_START);
		gtk_box_pack_start(GTK_BOX(tab_sem), w1, FALSE, FALSE, 2);
		GtkWidget *sc1 = gtk_scrolled_window_new(NULL, NULL);
		gtk_widget_set_vexpand(sc1, TRUE);
		gtk_box_pack_start(GTK_BOX(tab_sem), sc1, TRUE, TRUE, 2);
		ed->info_songs = gtk_label_new("(none in weekN.c)");
		gtk_widget_set_halign(ed->info_songs, GTK_ALIGN_START);
		gtk_label_set_line_wrap(GTK_LABEL(ed->info_songs), TRUE);
		gtk_widget_set_margin_start(ed->info_songs, 4);
		gtk_widget_set_margin_top(ed->info_songs, 2);
		gtk_widget_set_margin_bottom(ed->info_songs, 2);
		gtk_container_add(GTK_CONTAINER(sc1), ed->info_songs);

		GtkWidget *w2 = gtk_label_new("Characters");
		gtk_widget_set_halign(w2, GTK_ALIGN_START);
		gtk_box_pack_start(GTK_BOX(tab_sem), w2, FALSE, FALSE, 2);
		GtkWidget *sc2 = gtk_scrolled_window_new(NULL, NULL);
		gtk_widget_set_vexpand(sc2, TRUE);
		gtk_box_pack_start(GTK_BOX(tab_sem), sc2, TRUE, TRUE, 2);
		ed->info_chars = gtk_label_new("(none in weekN.c)");
		gtk_widget_set_halign(ed->info_chars, GTK_ALIGN_START);
		gtk_label_set_line_wrap(GTK_LABEL(ed->info_chars), TRUE);
		gtk_widget_set_margin_start(ed->info_chars, 4);
		gtk_widget_set_margin_top(ed->info_chars, 2);
		gtk_widget_set_margin_bottom(ed->info_chars, 2);
		gtk_container_add(GTK_CONTAINER(sc2), ed->info_chars);

		GtkWidget *hl = gtk_label_new(
			"(song/character lists are read-only here; editing belongs to the week editor)");
		gtk_widget_set_halign(hl, GTK_ALIGN_START);
		gtk_label_set_line_wrap(GTK_LABEL(hl), TRUE);
		gtk_box_pack_start(GTK_BOX(tab_sem), hl, FALSE, FALSE, 2);
	}

	gtk_notebook_append_page(GTK_NOTEBOOK(nb), tab_es, gtk_label_new("Stage"));
	gtk_notebook_append_page(GTK_NOTEBOOK(nb), tab_sem, gtk_label_new("Week"));

	// status (bottom, outside the notebook)
	ed->status = gtk_label_new("(no project)");
	gtk_widget_set_halign(ed->status, GTK_ALIGN_START);
	gtk_label_set_line_wrap(GTK_LABEL(ed->status), TRUE);
	gtk_widget_set_hexpand(ed->status, TRUE);
	gtk_box_pack_start(GTK_BOX(side), ed->status, FALSE, FALSE, 4);

	g_object_set_data_full(G_OBJECT(paned), "editor", ed, NULL);
	editor_rebuild_layers(ed);
	return paned;
}

// ------------------------------------------------------------------ dock
static void editor_dock(EditorStage *ed)
{
	if (!ed->float_win) return;
	GtkWidget *fw = ed->float_win;
	ed->float_win = NULL;              // clear before destroy (avoids re-entrancy)
	g_object_ref(ed->side);            // keep alive across reparent
	gtk_container_remove(GTK_CONTAINER(fw), ed->side);
	gtk_widget_destroy(fw);
	gtk_paned_pack2(GTK_PANED(ed->paned), ed->side, TRUE, TRUE);
	gtk_widget_show(ed->side);
	g_object_unref(ed->side);
	gtk_button_set_label(GTK_BUTTON(ed->btn_undock), "Undock");
}

static void editor_undock(EditorStage *ed)
{
	if (ed->float_win || !ed->side) return;
	g_object_ref(ed->side);
	gtk_container_remove(GTK_CONTAINER(ed->paned), ed->side);

	ed->float_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(ed->float_win), "psxtools - Stage");
	gtk_window_set_default_size(GTK_WINDOW(ed->float_win), 320, 600);
	gtk_window_set_transient_for(GTK_WINDOW(ed->float_win),
	                             GTK_WINDOW(ed->app->window));
	gtk_container_add(GTK_CONTAINER(ed->float_win), ed->side);
	g_signal_connect_swapped(ed->float_win, "destroy", G_CALLBACK(editor_dock), ed);
	gtk_widget_show(ed->float_win);
	g_object_unref(ed->side);
	gtk_button_set_label(GTK_BUTTON(ed->btn_undock), "Dock");
}

static void editor_toggle_dock(GtkButton *btn, gpointer user)
{
	(void)btn;
	EditorStage *ed = user;
	if (ed->float_win) editor_dock(ed);
	else editor_undock(ed);
}
