/*
  psxtools - project loading + weekN.c parser (port of python tools/stagegen).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

#include "project.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define SCREEN_CX     (SCREEN_WIDTH/2)
#define SCREEN_CY     (SCREEN_HEIGHT/2)

static char *xstrdup(const char *s)
{
	if (!s) return NULL;
	size_t n = strlen(s) + 1;
	char *r = malloc(n);
	if (r) memcpy(r, s, n);
	return r;
}

static char *xstrndup(const char *s, size_t n)
{
	char *r = malloc(n + 1);
	if (!r) return NULL;
	memcpy(r, s, n);
	r[n] = 0;
	return r;
}

// trim whitespace in place
static void trim_str(char *s)
{
	if (!s) return;
	int a = 0;
	while (s[a] && isspace((unsigned char)s[a])) a++;
	int b = (int)strlen(s);
	while (b > a && isspace((unsigned char)s[b-1])) b--;
	memmove(s, s + a, (size_t)(b - a));
	s[b - a] = 0;
}

static char *read_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *buf = malloc((size_t)sz + 1);
	if (!buf) { fclose(f); return NULL; }
	if (sz > 0 && fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
		free(buf); fclose(f); return NULL;
	}
	buf[sz] = 0;
	fclose(f);
	return buf;
}

static void strlst_push(char ***arr, int *count, const char *s)
{
	*arr = realloc(*arr, sizeof(char*) * ((size_t)(*count) + 1));
	(*arr)[*count] = xstrdup(s);
	(*count)++;
}

// ---- Makefile parsing ----
static bool parse_makefile(const char *path, const char *week,
                           char ***deps_out, int *deps_count_out,
                           char ***deps_full_out, int *deps_full_count_out,
                           char **warning_out)
{
	char *text = read_file(path);
	if (!text) return false;

	// find: "iso/weekN/weekN.exe: <rest>"
	char needle[64];
	snprintf(needle, sizeof(needle), "iso/%s/%s.exe:", week, week);
	char *p = strstr(text, needle);
	if (!p) { free(text); return false; }
	p += strlen(needle);

	// rest until end of line
	char *line_end = strchr(p, '\n');
	char *rest = xstrndup(p, line_end ? (size_t)(line_end - p) : strlen(p));

	// split before/after '|'
	char *before = rest;
	char *after = strchr(rest, '|');
	if (after)
	{
		*after = 0;
		after++;
	}

	// deps: whitespace-separated, skipping overlay.*
	char *save = NULL;
	for (char *tok = strtok_r(before, " \t", &save); tok; tok = strtok_r(NULL, " \t", &save))
	{
		if (strlen(tok) >= 8 && strncasecmp(tok, "overlay.", 8) == 0) continue;
		if (tok[0] == 0) continue;
		strlst_push(deps_out, deps_count_out, tok);
		// full path: replace iso prefix with <root>/iso (root is repo root)
		strlst_push(deps_full_out, deps_full_count_out, tok);
	}

	// order-only check
	if (after)
	{
		char *warn = xstrdup("");
		int has = 0;
		for (char *tok = strtok(after, " \t"); tok; tok = strtok(NULL, " \t"))
		{
			if (strlen(tok) >= 8 && strncasecmp(tok, "overlay.", 8) == 0) continue;
			if (tok[0] == 0) continue;
			has = 1;
			size_t n = strlen(warn) + strlen(tok) + 1;
			warn = realloc(warn, n + 1);
			strcat(warn, tok);
			strcat(warn, " ");
		}
		if (has)
		{
			char buf[1024];
			snprintf(buf, sizeof(buf),
				"The Makefile has order-only prerequisites after '|' on the %s line: "
				"%s. GNU make excludes them from $^, so they do NOT enter the exe pack, "
				"but %s_Load reads them -> corrupt textures. Remove the '|'.",
				week, warn, week);
			*warning_out = xstrdup(buf);
			free(warn);
		}
		free(warn);
	}
	free(rest);
	free(text);
	return true;
}

// ---- weekN.c parsing ----

// find "static void <Prefix>_Load(...) { <body> until matching close brace at col 0 }"
// Returns body (without the final closing brace) or NULL.
static char *extract_fn_body(const char *src, const char *prefix, const char *fnname)
{
	char needle[128];
	snprintf(needle, sizeof(needle), "static void %s_%s", prefix, fnname);
	const char *p = strstr(src, needle);
	if (!p) return NULL;
	// find first '{'
	const char *lb = strchr(p, '{');
	if (!lb) return NULL;
	// find the matching brace at line start (pattern "\n}")
	// The week C files put "}" on its own line at col 0.
	const char *q = lb + 1;
	while (*q)
	{
		if (*q == '\n' && q[1] == '}')
		{
			return xstrndup(lb + 1, (size_t)(q - lb - 1));
		}
		q++;
	}
	return NULL;
}

static void parse_load_order(const char *load_body, char ***order, int *count)
{
	// Gfx_LoadTex(&NAME, overlay_data = Overlay_DataRead(), ...);
	*order = NULL; *count = 0;
	const char *p = load_body;
	while ((p = strstr(p, "Gfx_LoadTex(&")) != NULL)
	{
		p += strlen("Gfx_LoadTex(&");
		const char *amp = strchr(p, ',');
		if (!amp) break;
		size_t n = (size_t)(amp - p);
		// trim
		while (n > 0 && (p[n-1]==' '||p[n-1]=='\t'||p[n-1]=='\n')) n--;
		strlst_push(order, count, xstrndup(p, n));
		p = amp;
	}
}

// Resolve each load var to its base name (png/tim base) by Makefile position.
static void resolve_bases(int load_count,
                          const char **deps_full, int deps_count,
	                      char ***bases_out, int *bases_count)
{
	*bases_out = NULL; *bases_count = 0;
	for (int i = 0; i < load_count; i++)
	{
		const char *dep = (i < deps_count) ? deps_full[i] : NULL;
		char *base = NULL;
		if (dep)
		{
			// strip dir + extension
			const char *slash = strrchr(dep, '/');
			const char *name = slash ? slash + 1 : dep;
			const char *dot = strrchr(name, '.');
			if (dot)
				base = xstrndup(name, (size_t)(dot - name));
			else
				base = xstrdup(name);
		}
		else
			base = NULL;
		strlst_push(bases_out, bases_count, base ? base : "");
		free(base);
	}
}

// Split comma-separated initializer list ignoring commas inside parentheses.
static char **split_top_level(const char *s, int *out_count)
{
	char **fields = NULL;
	int count = 0;
	int depth = 0;
	char cur[2048]; int curlen = 0;
	for (const char *p = s; *p; p++)
	{
		char ch = *p;
		if (ch == '(') depth++;
		else if (ch == ')') depth--;
		if (ch == ',' && depth == 0)
		{
			cur[curlen] = 0;
			// trim
			int a=0; while (a<curlen && isspace((unsigned char)cur[a])) a++;
			int b=curlen; while (b>a && isspace((unsigned char)cur[b-1])) b--;
			strlst_push(&fields, &count, xstrndup(cur+a, (size_t)(b-a)));
			curlen = 0;
		}
		else
		{
			if (curlen < (int)sizeof(cur)-1) cur[curlen++] = ch;
		}
	}
	if (curlen > 0)
	{
		cur[curlen] = 0;
		int a=0; while (a<curlen && isspace((unsigned char)cur[a])) a++;
		int b=curlen; while (b>a && isspace((unsigned char)cur[b-1])) b--;
		strlst_push(&fields, &count, xstrndup(cur+a, (size_t)(b-a)));
	}
	*out_count = count;
	return fields;
}

// Compute the parallax (camera mult) in effect before a given line index.
static void parallax_at_line(const char *body, int before_line,
                             double *px, double *py)
{
	*px = 0.0; *py = 0.0;
	int line = 0;
	const char *p = body;
	char fx[256] = "", fy[256] = "";
	while (*p && line < before_line)
	{
		const char *nl = strchr(p, '\n');
		size_t linelen = nl ? (size_t)(nl - p) : strlen(p);
		char *l = xstrndup(p, linelen);
		// strip // comment
		char *cm = strstr(l, "//");
		if (cm) *cm = 0;
		// fx = <expr>;
		char *fxp = NULL, *fyp = NULL;
		char *eq = strstr(l, "fx =");
		if (eq) { fxp = eq + 3; char *semi = strchr(fxp, ';'); if (semi) *semi = 0; }
		char *eqy = strstr(l, "fy =");
		if (eqy) { fyp = eqy + 3; char *semi = strchr(fyp, ';'); if (semi) *semi = 0; }
		if (fxp) { snprintf(fx, sizeof(fx), "%s", fxp+1); }
		if (fyp) { snprintf(fy, sizeof(fy), "%s", fyp+1); }
		free(l);
		p = nl ? nl + 1 : p + strlen(p);
		line++;
	}
	*px = eval_cam_mult(fx, 'x');
	*py = eval_cam_mult(fy, 'y');
}

static Sprite *sprite_new(const char *week_dir, const char *base,
                          const char *var_name)
{
	Sprite *s = calloc(1, sizeof(Sprite));
	s->base = xstrdup(base);
	s->var_name = xstrdup(var_name);
	s->name = xstrdup(base);
	s->draw = DRAW_RECT;
	s->src[0] = s->src[1] = 0;
	s->src[2] = s->src[3] = 0;
	s->layer = LAYER_BG;
	s->opacity = -1;
	// try to find the png
	if (week_dir && base && base[0])
	{
		char path[1024];
		snprintf(path, sizeof(path), "%s/%s.png", week_dir, base);
		s->tex_path = xstrdup(path);
	}
	return s;
}

static void sprite_free(Sprite *s)
{
	if (!s) return;
	free(s->name);
	free(s->var_name);
	free(s->tex_path);
	free(s->base);
	free(s->par_x_expr);
	free(s->par_y_expr);
	free(s->arc);
	free(s->tim);
	free(s->tex_field);
	free(s->cond);
	free(s->dyn_x);
	free(s->dyn_y);
	free(s);
}

// ================= Omniplasm src/stage/*.c support =================
//
// Engine format (see PSXFunkin-Omniplasm-Engine-Public/src/stage):
//   typedef struct { StageBack back; Gfx_Tex tex_back0; ... } Back_Xxx;
//   void Back_Xxx_DrawBG/DrawMD/DrawFG/DrawHUD(StageBack *back) { ... }
//   StageBack *Back_Xxx_New(void) {
//     IO_Data arc_back = IO_Read("\\WEEK1\\BACK.ARC;1");
//     Gfx_LoadTex(&this->tex_back0, Archive_Find(arc_back, "back0.tim"), 0);
//     ...
//   }
// Draw calls: Stage_DrawTex(&tex, &src, &dst, bzoom, angle),
// Stage_DrawTexArb(...), Stage_BlendTexV2(..., mode, opacity),
// Stage_DrawTex_FlipX(...), Gfx_DrawTex/Gfx_BlendTex(...), plus
// Col/Opacity/Rotate/All variants. Parallax via fx/fy like the legacy
// format. Layers map DrawBG->bg, DrawMD->md, DrawFG->fg, DrawHUD->hud.

static bool is_omni_source(const char *src)
{
	return strstr(src, "StageBack *Back_") != NULL;
}

// "kitchen" -> "Kitchen", "week1" -> "Week1" (fallback when the struct
// name cannot be read from the source itself).
static void omni_capitalize(const char *in, char *out, size_t n)
{
	size_t i = 0;
	for (; in[i] && i + 1 < n; i++)
		out[i] = (i == 0) ? (char)toupper((unsigned char)in[i]) : in[i];
	out[i] = 0;
	if (i == 0)
		snprintf(out, n, "Stage");
}

// Read the real struct prefix from "StageBack *Back_<X>_New".
static void omni_read_prefix(const char *src, const char *fallback,
                             char *out, size_t n)
{
	const char *np = strstr(src, "StageBack *Back_");
	if (np)
	{
		char tmp[128];
		if (sscanf(np + strlen("StageBack *Back_"), "%127[A-Za-z0-9_]", tmp) == 1)
		{
			size_t L = strlen(tmp);
			if (L > 4 && strcmp(tmp + L - 4, "_New") == 0)
				tmp[L - 4] = 0;
			snprintf(out, n, "Back_%s", tmp);
			return;
		}
	}
	{
		char cap[128];
		omni_capitalize(fallback ? fallback : "stage", cap, sizeof(cap));
		snprintf(out, n, "Back_%s", cap);
	}
}

// Extract "void <prefix>_<fn>(StageBack ...)" (or "<prefix>_New(void)")
// body up to the col-0 closing brace. Returns malloc'd body or NULL.
static char *extract_omni_fn(const char *src, const char *prefix, const char *fnname)
{
	char needle[192];
	const char *p;
	if (strcmp(fnname, "New") == 0)
	{
		snprintf(needle, sizeof(needle), "%s_New(void)", prefix);
		p = strstr(src, needle);
	}
	else
	{
		snprintf(needle, sizeof(needle), "void %s_%s(StageBack", prefix, fnname);
		p = strstr(src, needle);
	}
	if (!p) return NULL;
	const char *lb = strchr(p, '{');
	if (!lb) return NULL;
	const char *q = lb + 1;
	while (*q)
	{
		if (*q == '\n' && q[1] == '}')
			return xstrndup(lb + 1, (size_t)(q - lb - 1));
		q++;
	}
	return NULL;
}

typedef struct OmniMap { char *a; char *b; char *c; } OmniMap;

static void omnimap_push(OmniMap **m, int *n, const char *a, const char *b, const char *c)
{
	*m = realloc(*m, sizeof(OmniMap) * ((size_t)(*n) + 1));
	(*m)[*n].a = xstrdup(a ? a : "");
	(*m)[*n].b = xstrdup(b ? b : "");
	(*m)[*n].c = xstrdup(c ? c : "");
	(*n)++;
}

static const OmniMap *omnimap_find(const OmniMap *m, int n, const char *a)
{
	for (int i = n - 1; i >= 0; i--)
		if (strcmp(m[i].a, a) == 0)
			return &m[i];
	return NULL;
}

static void omnimap_free(OmniMap *m, int n)
{
	for (int i = 0; i < n; i++) { free(m[i].a); free(m[i].b); free(m[i].c); }
	free(m);
}

// Last C identifier in [start, end): strips "[...]" suffixes.
static void last_ident(const char *start, const char *end, char *out, size_t n)
{
	const char *e = end;
	while (e > start && isspace((unsigned char)e[-1])) e--;
	if (e > start && e[-1] == ']')
	{
		e--; // skip ']'
		while (e > start && e[-1] != '[') e--;
		if (e > start) e--; // skip '['
		while (e > start && isspace((unsigned char)e[-1])) e--;
	}
	const char *stop = e;
	while (e > start && (isalnum((unsigned char)e[-1]) || e[-1] == '_')) e--;
	size_t L = (size_t)(stop - e);
	if (L >= n) L = n - 1;
	memcpy(out, e, L);
	out[L] = 0;
}

// First identifier of an expression: strips "&", "this->" and spaces.
static void expr_ident(const char *s, char *out, size_t n)
{
	while (*s == ' ' || *s == '\t' || *s == '&') s++;
	if (strncmp(s, "this->", 6) == 0) s += 6;
	size_t i = 0;
	while (s[i] && (isalnum((unsigned char)s[i]) || s[i] == '_') && i + 1 < n)
		{ out[i] = s[i]; i++; }
	out[i] = 0;
}

static bool try_parse_int(const char *s, int *out)
{
	if (!s) return false;
	while (*s == ' ' || *s == '\t') s++;
	char *end = NULL;
	long v = strtol(s, &end, 10);
	if (end == s) return false;
	while (*end == ' ' || *end == '\t') end++;
	if (*end != 0) return false;
	*out = (int)v;
	return true;
}

static const char *match_paren(const char *open)
{
	int d = 0;
	for (const char *c = open; *c; c++)
	{
		if (*c == '(') d++;
		else if (*c == ')')
		{
			d--;
			if (d == 0) return c;
		}
	}
	return NULL;
}

// <lhs> = IO_Read("<path>")  ->  io map (ident -> path)
// <lhs> = Archive_Find(<arcident>, "<tim>")  ->  finds map (lhs -> arc, tim)
static void omni_collect_maps(const char *src, OmniMap **io_out, int *io_n,
                              OmniMap **find_out, int *find_n)
{
	for (const char *q = src; (q = strstr(q, "IO_Read(\"")) != NULL;)
	{
		const char *qs = q + strlen("IO_Read(\"");
		const char *qe = strchr(qs, '"');
		if (!qe) break;
		char *path = xstrndup(qs, (size_t)(qe - qs));
		const char *ls = q;
		while (ls > src && ls[-1] != '\n') ls--;
		const char *eq = NULL;
		for (const char *c = ls; c < q; c++)
			if (*c == '=') eq = c;
		if (eq)
		{
			char ident[128];
			last_ident(ls, eq, ident, sizeof(ident));
			if (ident[0])
				omnimap_push(io_out, io_n, ident, path, NULL);
		}
		free(path);
		q = qe + 1;
	}
	for (const char *q = src; (q = strstr(q, "Archive_Find(")) != NULL;)
	{
		const char *open = strchr(q, '(');
		const char *comma = open ? strchr(open, ',') : NULL;
		const char *t1 = comma ? strchr(comma, '"') : NULL;
		const char *t2 = t1 ? strchr(t1 + 1, '"') : NULL;
		if (!comma || !t1 || !t2) { q += strlen("Archive_Find("); continue; }
		char arc[128] = "";
		{
			const char *a = open + 1;
			while (*a == ' ' || *a == '\t') a++;
			if (strncmp(a, "this->", 6) == 0) a += 6;
			int i = 0;
			while (a[i] && (isalnum((unsigned char)a[i]) || a[i] == '_') && i < 120)
				{ arc[i] = a[i]; i++; }
			arc[i] = 0;
		}
		char *tim = xstrndup(t1 + 1, (size_t)(t2 - t1 - 1));
		const char *ls = q;
		while (ls > src && ls[-1] != '\n') ls--;
		const char *eq = NULL;
		for (const char *c = ls; c < q; c++)
			if (*c == '=' && !(c[1] == '=') && !(c > ls && c[-1] == '='))
				eq = c;
		if (eq)
		{
			char ident[128];
			last_ident(ls, eq, ident, sizeof(ident));
			if (ident[0])
				omnimap_push(find_out, find_n, ident, arc, tim);
		}
		free(tim);
		q = t2 + 1;
	}
}

// Gfx_LoadTex(&this-><field>, ...) in order -> loads map
// (field -> tim file -> ARC path, "" when not resolvable).
static void omni_collect_loads(const char *src, const OmniMap *io, int io_n,
                               const OmniMap *finds, int find_n,
                               OmniMap **loads_out, int *loads_n)
{
	for (const char *q = src; (q = strstr(q, "Gfx_LoadTex(")) != NULL;)
	{
		const char *open = strchr(q, '(');
		const char *close = open ? match_paren(open) : NULL;
		if (!close) { q += strlen("Gfx_LoadTex("); continue; }
		char *inner = xstrndup(open + 1, (size_t)(close - open - 1));
		int argc = 0;
		char **argv = split_top_level(inner, &argc);
		free(inner);
		if (argc >= 2)
		{
			char field[128];
			expr_ident(argv[0], field, sizeof(field));
			if (strncmp(field, "tex", 3) == 0)
			{
				char tim[128] = "";
				char arcpath[512] = "";
				const char *af = strstr(argv[1], "Archive_Find(");
				if (af)
				{
					const char *ao = strchr(af, '(');
					const char *cm = ao ? strchr(ao, ',') : NULL;
					const char *t1 = cm ? strchr(cm, '"') : NULL;
					const char *t2 = t1 ? strchr(t1 + 1, '"') : NULL;
					char arcid[128] = "";
					if (ao)
					{
						const char *a = ao + 1;
						while (*a == ' ' || *a == '\t') a++;
						if (strncmp(a, "this->", 6) == 0) a += 6;
						int i = 0;
						while (a[i] && (isalnum((unsigned char)a[i]) || a[i] == '_') && i < 120)
							{ arcid[i] = a[i]; i++; }
						arcid[i] = 0;
					}
					if (t1 && t2)
					{
						size_t L = (size_t)(t2 - t1 - 1);
						if (L >= sizeof(tim)) L = sizeof(tim) - 1;
						memcpy(tim, t1 + 1, L);
						tim[L] = 0;
					}
					const OmniMap *m = omnimap_find(io, io_n, arcid);
					if (m) snprintf(arcpath, sizeof(arcpath), "%s", m->b);
				}
				else
				{
					char ref[128];
					expr_ident(argv[1], ref, sizeof(ref));
					const OmniMap *m = omnimap_find(finds, find_n, ref);
					if (m)
					{
						snprintf(tim, sizeof(tim), "%s", m->c);
						const OmniMap *a = omnimap_find(io, io_n, m->b);
						if (a) snprintf(arcpath, sizeof(arcpath), "%s", a->b);
					}
				}
				omnimap_push(loads_out, loads_n, field, tim, arcpath);
			}
		}
		for (int k = 0; k < argc; k++) free(argv[k]);
		free(argv);
		q = close + 1;
	}
}

typedef struct DrawFnInfo
{
	const char *name;
	bool arb, blend, flip, color, gfx;
} DrawFnInfo;

// NOTE: longer names first so prefix matches lose (e.g. Stage_DrawTex
// must not win over Stage_DrawTexArb at the same position).
static const DrawFnInfo DRAW_FNS[] = {
	{"Stage_DrawTexArbCol", true, false, false, true, false},
	{"Stage_BlendTexArbCol", true, true, false, true, false},
	{"Stage_DrawTexArb", true, false, false, false, false},
	{"Stage_BlendTexArb", true, true, false, false, false},
	{"Stage_DrawTexColOpacity", false, false, false, true, false},
	{"Stage_BlendTexColOpacity", false, true, false, true, false},
	{"Stage_DrawTexCol_FlipX", false, false, true, true, false},
	{"Stage_DrawTex_FlipX", false, false, true, false, false},
	{"Stage_DrawTexRotateCol", false, false, false, true, false},
	{"Stage_DrawTexRotate", false, false, false, false, false},
	{"Stage_DrawTexOpacity", false, false, false, false, false},
	{"Stage_BlendTexCol", false, true, false, true, false},
	{"Stage_DrawTexCol", false, false, false, true, false},
	{"Stage_DrawBlendTexAll", false, true, false, false, false},
	{"Stage_DrawTexAll", false, false, false, false, false},
	{"Stage_BlendTexV2", false, true, false, false, false},
	{"Stage_BlendTex", false, true, false, false, false},
	{"Stage_DrawTex", false, false, false, false, false},
	{"Gfx_BlendTex", false, true, false, false, true},
	{"Gfx_DrawTex", false, false, false, false, true},
	{NULL, false, false, false, false, false},
};

static const char *find_draw_call(const char *p, const DrawFnInfo **out)
{
	const char *best = NULL;
	const DrawFnInfo *bf = NULL;
	for (const DrawFnInfo *d = DRAW_FNS; d->name; d++)
	{
		size_t L = strlen(d->name);
		const char *q = p;
		while ((q = strstr(q, d->name)) != NULL)
		{
			const char *r = q + L;
			while (*r == ' ' || *r == '\t') r++;
			if (*r == '(' &&
			    (q == p || (!isalnum((unsigned char)q[-1]) && q[-1] != '_')))
			{
				if (!best || q < best) { best = q; bf = d; }
				break;
			}
			q += L;
		}
	}
	if (out) *out = bf;
	return best;
}

static bool parse_rect_inner(const char *lb, double out[4], int want)
{
	const char *rb = strchr(lb, '}');
	if (!rb) return false;
	char *inner = xstrndup(lb + 1, (size_t)(rb - lb - 1));
	int cnt = 0;
	char **f = split_top_level(inner, &cnt);
	bool ok = cnt >= want;
	for (int k = 0; k < cnt && k < 4; k++)
		out[k] = fixed_field_to_double(f[k]);
	for (int k = 0; k < cnt; k++) free(f[k]);
	free(f);
	free(inner);
	return ok;
}

// "TYPE var = {" declaration anywhere in body -> '{'
static const char *find_rect_decl(const char *body, const char *type, const char *var)
{
	char needle[300];
	snprintf(needle, sizeof(needle), "%s %s = {", type, var);
	const char *sp = strstr(body, needle);
	if (!sp)
	{
		snprintf(needle, sizeof(needle), "%s %s={", type, var);
		sp = strstr(body, needle);
	}
	return sp ? strchr(sp, '{') : NULL;
}

// "var = (CAST){" assignment (static-frame animation style) -> '{'
static const char *find_rect_assign(const char *body, const char *var, const char *cast)
{
	char needle[300];
	snprintf(needle, sizeof(needle), "%s = (%s){", var, cast);
	const char *sp = strstr(body, needle);
	if (!sp)
	{
		snprintf(needle, sizeof(needle), "%s = (%s) {", var, cast);
		sp = strstr(body, needle);
	}
	return sp ? strchr(sp, '{') : NULL;
}

static bool omni_find_src(const char *body, const char *var, double out[4])
{
	const char *lb = find_rect_decl(body, "RECT", var);
	if (!lb) lb = find_rect_assign(body, var, "RECT");
	if (!lb) return false;
	return parse_rect_inner(lb, out, 4);
}

static bool omni_find_point(const char *body, const char *var, double out[2])
{
	const char *lb = find_rect_decl(body, "POINT_FIXED", var);
	if (!lb) return false;
	double v[4] = {0, 0, 0, 0};
	if (!parse_rect_inner(lb, v, 2)) return false;
	out[0] = v[0]; out[1] = v[1];
	return true;
}

static void free_fields(char **f, int n)
{
	for (int k = 0; k < n; k++) free(f[k]);
	free(f);
}

// dst initializer fields as raw strings (caller frees with free_fields).
// Returns false when the dst var has no initializer in the body.
static bool omni_find_dst_raw(const char *body, const char *var,
                              char ***out_f, int *out_n, bool *is_abs)
{
	const char *lb = find_rect_decl(body, "RECT_FIXED", var);
	if (lb) *is_abs = false;
	else
	{
		lb = find_rect_assign(body, var, "RECT_FIXED");
		if (lb) *is_abs = false;
		else
		{
			lb = find_rect_decl(body, "RECT", var);
			if (!lb) lb = find_rect_assign(body, var, "RECT");
			if (!lb) return false;
			*is_abs = true;
		}
	}
	const char *rb = strchr(lb, '}');
	if (!rb) return false;
	char *inner = xstrndup(lb + 1, (size_t)(rb - lb - 1));
	int cnt = 0;
	char **f = split_top_level(inner, &cnt);
	free(inner);
	if (cnt < 4) { free_fields(f, cnt); return false; }
	*out_f = f; *out_n = cnt;
	return true;
}

// A dst x/y field is code-driven (not a plain constant) when it touches
// struct state, e.g. Week4's "this->car_x - fx".
static bool is_dyn_field(const char *f)
{
	return strstr(f, "->") != NULL;
}

// Nearest enclosing single-line "if (<cond>) {" above call_line.
// Returns malloc'd condition or NULL. Used to keep song_step-conditional
// pieces (e.g. Kitchen's window/jumpscare overlays) round-tripping.
static char *omni_enclosing_if(const char *body, long call_line)
{
	int n = 0;
	for (const char *c = body; *c; c++)
		if (*c == '\n') n++;
	n += 2;
	const char **starts = malloc(sizeof(char*) * (size_t)n);
	int nl = 0;
	starts[nl++] = body;
	for (const char *c = body; *c; c++)
		if (*c == '\n' && c[1])
			starts[nl++] = c + 1;
	char *found = NULL;
	for (long k = call_line - 1; k >= 0 && k >= call_line - 30; k--)
	{
		const char *l = starts[k];
		while (*l == ' ' || *l == '\t' || *l == '}') l++;
		const char *ifp = NULL;
		if (strncmp(l, "if (", 4) == 0 || strncmp(l, "if(", 3) == 0)
			ifp = l;
		else if (strncmp(l, "else", 4) == 0)
		{
			const char *e = strstr(l, "if (");
			if (!e) e = strstr(l, "if(");
			ifp = e;
		}
		if (!ifp) continue;
		const char *lp = strchr(ifp, '(');
		if (!lp) continue;
		int d = 0;
		const char *c = lp, *cond_end = NULL;
		for (; *c && *c != '\n'; c++)
		{
			if (*c == '(') d++;
			else if (*c == ')')
			{
				d--;
				if (d == 0) { cond_end = c; break; }
			}
		}
		if (!cond_end) continue;
		// opening brace: same line ("if (..) {") or next line (Allman)
		if (!strchr(cond_end, '{'))
		{
			if (k + 1 >= call_line || k + 1 >= nl) continue;
			const char *nl2 = starts[k + 1];
			while (*nl2 == ' ' || *nl2 == '\t') nl2++;
			if (*nl2 != '{') continue;
		}
		// depth check: from the opening '{' through the lines before
		// the call; enclosing requires depth > 0 at the call.
		int depth = 0;
		for (const char *s = cond_end; *s && *s != '\n'; s++)
		{
			if (*s == '{') depth++;
			else if (*s == '}') depth--;
		}
		for (long j = k + 1; j < call_line && j < nl; j++)
		{
			const char *le = (j + 1 < nl) ? starts[j + 1]
			                              : starts[j] + strlen(starts[j]);
			for (const char *s = starts[j]; s < le; s++)
			{
				if (*s == '{') depth++;
				else if (*s == '}') depth--;
			}
		}
		if (depth > 0)
		{
			found = xstrndup(lp + 1, (size_t)(cond_end - lp - 1));
			trim_str(found);
			break;
		}
	}
	free(starts);
	return found;
}

static void tim_base(const char *tim, char *out, size_t n)
{
	const char *slash = strrchr(tim, '/');
	const char *name = slash ? slash + 1 : tim;
	const char *dot = strrchr(name, '.');
	size_t L = dot ? (size_t)(dot - name) : strlen(name);
	if (L == 0 || n == 0)
	{
		snprintf(out, n, "back");
		return;
	}
	if (L >= n) L = n - 1;
	memcpy(out, name, L);
	out[L] = 0;
}

static void omni_parse_draws(Project *pr, const char *body, StageLayer layer,
                             const OmniMap *loads, int load_n)
{
	const char *p = body;
	while (*p)
	{
		const DrawFnInfo *fi = NULL;
		const char *at = find_draw_call(p, &fi);
		if (!at) break;
		const char *open = strchr(at, '(');
		const char *close = open ? match_paren(open) : NULL;
		if (!close) { p = at + strlen(fi->name); continue; }

		char *inner = xstrndup(open + 1, (size_t)(close - open - 1));
		int argc = 0;
		char **argv = split_top_level(inner, &argc);
		free(inner);

		const char *next = close + 1;
		Sprite *s = NULL;

		if (argc >= 3)
		{
			char field[128];
			expr_ident(argv[0], field, sizeof(field));
			const OmniMap *lm = omnimap_find(loads, load_n, field);
			if (lm)
			{
				char srcvar[128];
				expr_ident(argv[1], srcvar, sizeof(srcvar));
				s = calloc(1, sizeof(Sprite));
				s->opacity = -1;
				s->layer = layer;
				s->blend = fi->blend;
				s->flip = fi->flip;
				s->tex_field = xstrdup(field);
				s->tim = xstrdup(lm->b);
				s->arc = xstrdup(lm->c);
				char tb[128];
				tim_base(lm->b, tb, sizeof(tb));
				s->base = xstrdup(tb);
				s->name = xstrdup(tb);
				s->var_name = xstrdup(field);
				{
					char path[2048];
					snprintf(path, sizeof(path), "%s/%s.png", pr->week_dir, tb);
					s->tex_path = xstrdup(path);
				}
				double sv[4] = {0, 0, 0, 0};
				if (omni_find_src(body, srcvar, sv))
				{
					s->src[0] = (int)round(sv[0]);
					s->src[1] = (int)round(sv[1]);
					s->src[2] = (int)round(sv[2]);
					s->src[3] = (int)round(sv[3]);
				}
				bool geom_ok = false;
				if (fi->arb && argc >= 6)
				{
					double pts[4][2];
					bool ok = true;
					for (int k = 0; k < 4; k++)
					{
						char pv[128];
						expr_ident(argv[2 + k], pv, sizeof(pv));
						if (!omni_find_point(body, pv, pts[k])) { ok = false; break; }
					}
					if (ok)
					{
						for (int k = 0; k < 4; k++)
							{ s->arb[k][0] = pts[k][0]; s->arb[k][1] = pts[k][1]; }
						s->arb_valid = true;
						s->draw = (fi->color || fi->blend) ? DRAW_ARB_COL : DRAW_ARB;
						geom_ok = true;
					}
				}
				else if (!fi->arb)
				{
					char dstv[128];
					expr_ident(argv[2], dstv, sizeof(dstv));
					char **df = NULL;
					int dfn = 0;
					bool is_abs = false;
					if (omni_find_dst_raw(body, dstv, &df, &dfn, &is_abs))
					{
						double dv[4] = {0, 0, 0, 0};
						for (int k = 0; k < 4; k++)
							dv[k] = fixed_field_to_double(df[k]);
						// code-driven x/y (e.g. this->car_x): keep the raw
						// expression for verbatim re-emit, lock the editor pos
						if (!is_abs)
						{
							if (is_dyn_field(df[0])) s->dyn_x = xstrdup(df[0]);
							if (is_dyn_field(df[1])) s->dyn_y = xstrdup(df[1]);
						}
						if (is_abs || fi->gfx)
						{
							s->draw = fi->color ? DRAW_ABS_COL : DRAW_ABS;
							s->x = dv[0] - SCREEN_CX;
							s->y = dv[1] - SCREEN_CY;
							s->w = dv[2];
							s->h = dv[3];
						}
						else
						{
							s->draw = (fi->color || fi->blend) ? DRAW_RECT_COL : DRAW_RECT;
							s->x = dv[0]; s->y = dv[1];
							s->w = dv[2]; s->h = dv[3];
						}
						free_fields(df, dfn);
						geom_ok = true;
					}
				}
				if (geom_ok)
				{
					long line = 0;
					{
						size_t off = (size_t)(at - body);
						for (size_t i = 0; i < off; i++)
							if (body[i] == '\n') line++;
					}
					double px, py;
					parallax_at_line(body, (int)line, &px, &py);
					s->par_x = px;
					s->par_y = py;
					s->cond = omni_enclosing_if(body, line);
					if (strcmp(fi->name, "Stage_BlendTexV2") == 0 && argc >= 6)
					{
						int v;
						if (try_parse_int(argv[4], &v)) s->blend_mode = v;
						if (try_parse_int(argv[5], &v)) s->opacity = v;
					}
					else if (strcmp(fi->name, "Gfx_BlendTex") == 0 && argc >= 4)
					{
						int v;
						if (try_parse_int(argv[3], &v)) s->blend_mode = v;
					}
					else if (strstr(fi->name, "Opacity") != NULL && argc >= 6)
					{
						int v;
						if (try_parse_int(argv[argc - 1], &v)) s->opacity = v;
					}
					if (fi->color && argc >= 8)
					{
						int c[3];
						if (try_parse_int(argv[5], &c[0]) &&
						    try_parse_int(argv[6], &c[1]) &&
						    try_parse_int(argv[7], &c[2]))
						{
							s->has_color = true;
							s->color[0] = c[0];
							s->color[1] = c[1];
							s->color[2] = c[2];
						}
					}
					// unique display name per repeated texture
					int dup = 0;
					for (int j = 0; j < pr->sprite_count; j++)
						if (strcmp(pr->sprites[j]->tex_field, field) == 0) dup++;
					if (dup > 0)
					{
						char nb[160];
						snprintf(nb, sizeof(nb), "%s~%d", tb, dup + 1);
						free(s->name);
						s->name = xstrdup(nb);
					}
					pr->sprites = realloc(pr->sprites,
						sizeof(Sprite*) * (size_t)(pr->sprite_count + 1));
					pr->sprites[pr->sprite_count++] = s;
					s = NULL;
				}
				if (s)
				{
					free(s->tex_field); free(s->tim); free(s->arc);
					free(s->base); free(s->name); free(s->var_name);
					free(s->tex_path);
					free(s->dyn_x); free(s->dyn_y);
					free(s);
				}
			}
		}
		for (int k = 0; k < argc; k++) free(argv[k]);
		free(argv);
		p = next;
	}
}

// basename without extension; handles '/' and '\\' separators.
static char *xbase_noext(const char *path)
{
	const char *s1 = strrchr(path, '/');
	const char *s2 = strrchr(path, '\\');
	const char *b = (s1 > s2) ? s1 : s2;
	b = b ? b + 1 : path;
	const char *dot = strrchr(b, '.');
	size_t L = dot ? (size_t)(dot - b) : strlen(b);
	char *r = malloc(L + 1);
	if (r) { memcpy(r, b, L); r[L] = 0; }
	return r;
}

static char *xdir_name(const char *path)
{
	const char *s1 = strrchr(path, '/');
	const char *s2 = strrchr(path, '\\');
	const char *s = (s1 > s2) ? s1 : s2;
	if (!s) return xstrdup(".");
	return xstrndup(path, (size_t)(s - path));
}

// ============ Omniplasm animated actors (Week4 henchmen pattern) ============
//
//   static const CharFrame henchmen_frame[] = {
//     {0, { 0, 0, 99, 99}, { 71, 98}}, ...
//   };
//   static const Animation henchmen_anim[] = {
//     {1, (const u8[]){0, 0, 1, ..., ASCR_BACK, 1}}, ...
//   };
//   void Week4_Henchmen_SetFrame(void *user, u8 frame) {
//     ... Gfx_LoadTex(&this->tex_hench, this->arc_hench_ptr[...] ...);
//   }
//   void Week4_Henchmen_Draw(Back_Week4 *this, fixed_t x, fixed_t y) {
//     ... Stage_DrawTex(&this->tex_hench, &src, &dst, ...);
//   }
//   ... in DrawBG: Animatable_Animate(&this->hench_animatable, (void*)this,
//                                     Week4_Henchmen_SetFrame);
//                  Week4_Henchmen_Draw(this, FIXED_DEC(-50,1) - fx, ...);
//
// Each <Base>_Draw(this, x, y) call site becomes one editor sprite
// (previewed with animation frame 0); the tables + helpers round-trip
// verbatim while the call sites are regenerated from the sprites.

static const char *match_brace(const char *open)
{
	int d = 0;
	bool in_str = false, in_chr = false, in_lc = false, in_bc = false;
	for (const char *c = open; *c; c++)
	{
		if (in_lc) { if (*c == '\n') in_lc = false; continue; }
		if (in_bc) { if (*c == '*' && c[1] == '/') { in_bc = false; c++; } continue; }
		if (in_str) { if (*c == '\\' && c[1]) c++; else if (*c == '"') in_str = false; continue; }
		if (in_chr) { if (*c == '\\' && c[1]) c++; else if (*c == '\'') in_chr = false; continue; }
		if (*c == '/' && c[1] == '/') { in_lc = true; c++; continue; }
		if (*c == '/' && c[1] == '*') { in_bc = true; c++; continue; }
		if (*c == '"') { in_str = true; continue; }
		if (*c == '\'') { in_chr = true; continue; }
		if (*c == '{') d++;
		else if (*c == '}')
		{
			d--;
			if (d == 0) return c;
		}
	}
	return NULL;
}

// Split on commas at brace/paren depth 0. Returns malloc'd trimmed tokens.
static char **split_brace_top(const char *s, int *out_count)
{
	char **fields = NULL;
	int count = 0;
	int db = 0, dp = 0;
	const char *start = s;
	for (const char *p = s; ; p++)
	{
		char ch = *p;
		if (ch == '{') db++;
		else if (ch == '}') db--;
		else if (ch == '(') dp++;
		else if (ch == ')') dp--;
		if ((ch == ',' && db == 0 && dp == 0) || ch == 0)
		{
			const char *a = start, *b = p;
			while (a < b && isspace((unsigned char)*a)) a++;
			while (b > a && isspace((unsigned char)*(b-1))) b--;
			char *t = xstrndup(a, (size_t)(b - a));
			fields = realloc(fields, sizeof(char*) * ((size_t)count + 1));
			fields[count++] = t;
			if (ch == 0) break;
			start = p + 1;
		}
	}
	*out_count = count;
	return fields;
}

// Text inside the first {...} in s (malloc'd) or NULL.
static char *brace_inside(const char *s)
{
	const char *o = strstr(s, "{");
	const char *c = o ? match_brace(o) : NULL;
	if (!o || !c) return NULL;
	return xstrndup(o + 1, (size_t)(c - o - 1));
}

// Engine animation.h control codes (u8 sequence values).
static int ascr_value(const char *tok)
{
	if (strcmp(tok, "ASCR_REPEAT") == 0) return 0xFF;
	if (strcmp(tok, "ASCR_CHGANI") == 0) return 0xFE;
	if (strcmp(tok, "ASCR_BACK") == 0) return 0xFD;
	if (strcmp(tok, "ASCR_LOOP") == 0) return 0xFC;
	if (strcmp(tok, "ASCR_HOLD") == 0) return 0xFB;
	return -1;
}

// Find "TYPE <name>[...] = { ... }" from `from`. Puts the table name in
// `name`, the malloc'd inner text in *raw_out, the char after the table in
// *next. Returns the '{' or NULL.
static const char *find_table(const char *from, const char *type, char *name, size_t nn,
                              char **raw_out, const char **next)
{
	char needle[64];
	snprintf(needle, sizeof(needle), "%s ", type);
	const char *q = from;
	while ((q = strstr(q, needle)) != NULL)
	{
		const char *n = q + strlen(needle);
		while (*n == ' ' || *n == '\t') n++;
		size_t i = 0;
		while (n[i] && (isalnum((unsigned char)n[i]) || n[i] == '_') && i + 1 < nn)
			{ name[i] = n[i]; i++; }
		name[i] = 0;
		const char *br = n + i;
		while (*br == ' ' || *br == '\t') br++;
		q = n + (i ? i : 1);
		if (!name[0] || *br != '[')
			continue;
		const char *open = strchr(br, '{');
		const char *close = open ? match_brace(open) : NULL;
		if (!open || !close)
			return NULL;
		if (raw_out) *raw_out = xstrndup(open + 1, (size_t)(close - open - 1));
		if (next) *next = close + 1;
		return open;
	}
	if (next) *next = NULL;
	return NULL;
}

static void parse_frame_entries(const char *inner, AnimFrame **out, int *n)
{
	const char *p = inner;
	while (*p)
	{
		while (*p && *p != '{') p++;
		if (!*p) break;
		const char *close = match_brace(p);
		if (!close) break;
		char *entry = xstrndup(p + 1, (size_t)(close - p - 1));
		int tc = 0;
		char **toks = split_brace_top(entry, &tc);
		if (tc >= 3)
		{
			AnimFrame f;
			memset(&f, 0, sizeof(f));
			f.tex = atoi(toks[0]);
			char *s1 = brace_inside(toks[1]);
			char *s2 = brace_inside(toks[2]);
			if (s1)
			{
				int c = 0;
				char **v = split_brace_top(s1, &c);
				for (int k = 0; k < c && k < 4; k++) f.src[k] = atoi(v[k]);
				for (int k = 0; k < c; k++) free(v[k]);
				free(v);
				if (s2)
				{
					c = 0;
					v = split_brace_top(s2, &c);
					for (int k = 0; k < c && k < 2; k++) f.off[k] = atoi(v[k]);
					for (int k = 0; k < c; k++) free(v[k]);
					free(v);
				}
				*out = realloc(*out, sizeof(AnimFrame) * ((size_t)(*n) + 1));
				(*out)[(*n)++] = f;
			}
			free(s1); free(s2);
		}
		for (int k = 0; k < tc; k++) free(toks[k]);
		free(toks);
		free(entry);
		p = close + 1;
	}
}

static void parse_anim_entries(const char *inner, AnimDef **out, int *n)
{
	const char *p = inner;
	while (*p)
	{
		while (*p && *p != '{') p++;
		if (!*p) break;
		const char *close = match_brace(p);
		if (!close) break;
		char *entry = xstrndup(p + 1, (size_t)(close - p - 1));
		int tc = 0;
		char **toks = split_brace_top(entry, &tc);
		if (tc >= 2)
		{
			AnimDef a;
			memset(&a, 0, sizeof(a));
			a.speed = atoi(toks[0]);
			a.raw = xstrndup(p, (size_t)(close - p + 1));
			char *seq = brace_inside(toks[1]);
			if (seq)
			{
				int c = 0;
				char **v = split_brace_top(seq, &c);
				for (int k = 0; k < c; k++)
				{
					int val;
					if (!try_parse_int(v[k], &val))
						val = ascr_value(v[k]);
					a.seq = realloc(a.seq, sizeof(int) * ((size_t)a.seq_count + 1));
					a.seq[a.seq_count++] = val;
				}
				for (int k = 0; k < c; k++) free(v[k]);
				free(v); free(seq);
			}
			*out = realloc(*out, sizeof(AnimDef) * ((size_t)(*n) + 1));
			(*out)[(*n)++] = a;
		}
		for (int k = 0; k < tc; k++) free(toks[k]);
		free(toks);
		free(entry);
		p = close + 1;
	}
}

// Full "void <fn>(...) { ... }" text (to the col-0 closing brace), malloc'd.
static char *omni_fn_text(const char *src, const char *fn)
{
	char needle[192];
	snprintf(needle, sizeof(needle), "void %s(", fn);
	const char *p = strstr(src, needle);
	if (!p) return NULL;
	const char *lb = strchr(p, '{');
	if (!lb) return NULL;
	const char *q = lb + 1;
	while (*q)
	{
		if (*q == '\n' && q[1] == '}')
			return xstrndup(p, (size_t)(q + 2 - p));
		q++;
	}
	return NULL;
}

// First "&<known-frame-array>[" reference in a helper body.
static void body_frame_array(const char *body, char **known, int known_n, char *out, size_t nn)
{
	out[0] = 0;
	for (const char *q = body; (q = strstr(q, "&")) != NULL; q++)
	{
		const char *n = q + 1;
		char tmp[128];
		size_t i = 0;
		while (n[i] && (isalnum((unsigned char)n[i]) || n[i] == '_') && i + 1 < sizeof(tmp))
			{ tmp[i] = n[i]; i++; }
		tmp[i] = 0;
		if (tmp[0] && n[i] == '[')
		{
			for (int k = 0; k < known_n; k++)
				if (strcmp(known[k], tmp) == 0)
				{
					snprintf(out, nn, "%s", tmp);
					return;
				}
		}
	}
}

// "tex_..." field drawn by a helper body.
static void body_tex_field(const char *body, char *out, size_t nn)
{
	out[0] = 0;
	const char *q = strstr(body, "&this->tex");
	if (!q) return;
	expr_ident(q, out, nn);
}

// "<ident>_ptr[" member sampled by a SetFrame body (the ARC ptr array).
static void body_arc_ptr(const char *body, char *out, size_t nn)
{
	out[0] = 0;
	const char *q = strstr(body, "_ptr[");
	if (!q) return;
	const char *e = q;
	while (e > body && (isalnum((unsigned char)e[-1]) || e[-1] == '_')) e--;
	size_t L = (size_t)(q + 4 - e);
	if (L >= nn) L = nn - 1;
	memcpy(out, e, L);
	out[L] = 0;
}

typedef struct AnimWork
{
	char *base;          // "Week4_Henchmen"
	char *frame_array;
	char *tex_field;     // from SetFrame (or Draw helper fallback)
	char *arc_ptr;       // "arc_hench_ptr"
	char *draw_fn;       // base + "_Draw"
	char *setframe_fn;   // base + "_SetFrame"
	char *setframe_raw;
	char *drawhelper_raw;
	int scale_x, scale_y;
	bool use_off, flip;
} AnimWork;

static void animwork_free(AnimWork *w)
{
	free(w->base); free(w->frame_array); free(w->tex_field); free(w->arc_ptr);
	free(w->draw_fn); free(w->setframe_fn);
	free(w->setframe_raw); free(w->drawhelper_raw);
}

static AnimWork *animwork_find(AnimWork *works, int nworks, const char *base)
{
	for (int i = 0; i < nworks; i++)
		if (strcmp(works[i].base, base) == 0)
			return &works[i];
	return NULL;
}

// Animatable_SetAnim(&this->MEMBER, N) in the New body -> N, or -1.
static int omni_setanim_for(const char *new_body, const char *member)
{
	if (!new_body || !member || !member[0]) return -1;
	char needle[192];
	snprintf(needle, sizeof(needle), "Animatable_SetAnim(&this->%s,", member);
	const char *p = strstr(new_body, needle);
	if (!p) return -1;
	p += strlen(needle);
	while (*p == ' ' || *p == '\t') p++;
	char *end = NULL;
	long v = strtol(p, &end, 10);
	if (end == p || v < 0 || v > 63) return -1;
	return (int)v;
}

// One "<Base>_Draw[_k](this, x, y)" instance -> editor sprite, previewed
// on the first frame of the animation its state plays (SetAnim).
static void omni_anim_instance(Project *pr, AnimSet *a, const char *body,
                               StageLayer layer, double x, double y, long line,
                               int kinst, int animidx)
{
	if (!a->frame_count) return;
	int preview = 0;
	if (animidx >= 0 && animidx < a->anim_count && a->anims[animidx].seq_count > 0)
	{
		int f0 = a->anims[animidx].seq[0];
		if (f0 >= 0 && f0 < a->frame_count)
			preview = f0;
	}
	AnimFrame *f0 = &a->frames[preview];
	Sprite *s = calloc(1, sizeof(Sprite));
	s->opacity = -1;
	s->layer = layer;
	s->anim = a;
	s->anim_frame = preview;
	s->anim_inst = kinst;
	s->anim_anim = animidx;
	s->anim_x = x; s->anim_y = y;
	s->draw = DRAW_RECT;
	s->flip = a->flip;
	s->tex_field = xstrdup(a->tex_field);
	char tb[128];
	if (f0->tex >= 0 && f0->tex < a->tim_count && a->tims[f0->tex])
		tim_base(a->tims[f0->tex], tb, sizeof(tb));
	else
		snprintf(tb, sizeof(tb), "%s%d", a->id, f0->tex);
	s->base = xstrdup(tb);
	s->name = xstrdup(a->id);
	s->var_name = xstrdup(a->tex_field);
	s->src[0] = f0->src[0]; s->src[1] = f0->src[1];
	s->src[2] = f0->src[2]; s->src[3] = f0->src[3];
	s->x = x - (a->use_off ? f0->off[0] : 0);
	s->y = y - (a->use_off ? f0->off[1] : 0);
	s->w = (double)f0->src[2] * (double)a->scale_x;
	s->h = (double)f0->src[3] * (double)a->scale_y;
	s->tim = xstrdup((f0->tex >= 0 && f0->tex < a->tim_count && a->tims[f0->tex])
	                 ? a->tims[f0->tex] : "");
	s->arc = xstrdup(a->arc_path ? a->arc_path : "");
	{
		char path[2048];
		snprintf(path, sizeof(path), "%s/%s.png", pr->week_dir, tb);
		s->tex_path = xstrdup(path);
	}
	double px, py;
	parallax_at_line(body, (int)line, &px, &py);
	s->par_x = px;
	s->par_y = py;
	s->cond = omni_enclosing_if(body, line);
	int dup = 0;
	for (int j = 0; j < pr->sprite_count; j++)
		if (pr->sprites[j]->anim == a) dup++;
	if (dup > 0)
	{
		char nb[160];
		snprintf(nb, sizeof(nb), "%s~%d", a->id, dup + 1);
		free(s->name);
		s->name = xstrdup(nb);
	}
	pr->sprites = realloc(pr->sprites, sizeof(Sprite*) * (size_t)(pr->sprite_count + 1));
	pr->sprites[pr->sprite_count++] = s;
}

static void omni_anim_instances(Project *pr, AnimSet *a, const char *body, StageLayer layer)
{
	// classic "<Base>_Draw(this," (slot 0) and per-state "<Base>_Draw_<k>(this,"
	size_t fnlen = strlen(a->draw_fn);
	for (const char *p = body; (p = strstr(p, a->draw_fn)) != NULL;)
	{
		const char *r = p + fnlen;
		int k = -1;
		if (strncmp(r, "(this,", 6) == 0)
			k = 0;
		else if (*r == '_')
		{
			char *end = NULL;
			long v = strtol(r + 1, &end, 10);
			if (end > r + 1 && strncmp(end, "(this,", 6) == 0)
				k = (int)v;
		}
		if (k < 0) { p = r; continue; }
		const char *open = strchr(p, '(');
		const char *close = open ? match_paren(open) : NULL;
		if (!close) { p = open ? open + 1 : r + 1; continue; }
		char *inner = xstrndup(open + 1, (size_t)(close - open - 1));
		int argc = 0;
		char **argv = split_top_level(inner, &argc);
		free(inner);
		const char *next = close + 1;
		if (argc >= 2)
		{
			double x = fixed_field_to_double(argv[0]);
			double y = fixed_field_to_double(argv[1]);
			long line = 0;
			{
				size_t off = (size_t)(p - body);
				for (size_t i = 0; i < off; i++)
					if (body[i] == '\n') line++;
			}
			// playback state member for this slot -> its SetAnim index
			char member[160];
			if (k == 0 || !a->animatable[0])
				snprintf(member, sizeof(member), "%s", a->animatable);
			else
				snprintf(member, sizeof(member), "%s_%d", a->animatable, k);
			int animidx = omni_setanim_for(pr->new_body, member);
			if (animidx < 0) animidx = 0;
			// code-driven draw position (e.g. this->handb_y): keep the
			// raw expression for verbatim re-emit, lock the editor pos
			const char *dx = strstr(argv[0], "->") ? argv[0] : NULL;
			const char *dy = strstr(argv[1], "->") ? argv[1] : NULL;
			omni_anim_instance(pr, a, body, layer, x, y, line, k, animidx);
			if (dx || dy)
			{
				Sprite *s = pr->sprites[pr->sprite_count - 1];
				if (dx) s->dyn_x = xstrdup(dx);
				if (dy) s->dyn_y = xstrdup(dy);
			}
		}
		for (int k2 = 0; k2 < argc; k2++) free(argv[k2]);
		free(argv);
		p = next;
	}
}

static void omni_parse_anims(Project *pr, const char *src, const OmniMap *io, int io_n)
{
	typedef struct { char *name; char *raw; AnimFrame *frames; int nframes; } FrameArr;
	typedef struct { char *name; char *raw; AnimDef *anims; int nanims; } AnimArrIn;
	FrameArr *farrs = NULL; int nfarrs = 0;
	AnimArrIn *aarrs = NULL; int naarrs = 0;

	{
		const char *next = src;
		while (next)
		{
			char name[128]; char *raw = NULL;
			const char *nx = NULL;
			if (!find_table(next, "CharFrame", name, sizeof(name), &raw, &nx)) break;
			FrameArr fa;
			fa.name = xstrdup(name); fa.raw = raw; fa.frames = NULL; fa.nframes = 0;
			parse_frame_entries(raw, &fa.frames, &fa.nframes);
			farrs = realloc(farrs, sizeof(FrameArr) * ((size_t)nfarrs + 1));
			farrs[nfarrs++] = fa;
			next = nx;
		}
	}
	{
		const char *next = src;
		while (next)
		{
			char name[128]; char *raw = NULL;
			const char *nx = NULL;
			if (!find_table(next, "Animation", name, sizeof(name), &raw, &nx)) break;
			AnimArrIn aa;
			aa.name = xstrdup(name); aa.raw = raw; aa.anims = NULL; aa.nanims = 0;
			parse_anim_entries(raw, &aa.anims, &aa.nanims);
			aarrs = realloc(aarrs, sizeof(AnimArrIn) * ((size_t)naarrs + 1));
			aarrs[naarrs++] = aa;
			next = nx;
		}
	}

	if (nfarrs == 0)
		goto cleanup;

	char **known = malloc(sizeof(char*) * (size_t)nfarrs);
	for (int i = 0; i < nfarrs; i++) known[i] = farrs[i].name;

	AnimWork *works = NULL; int nworks = 0;

	// "<Base>_SetFrame(void *user, u8 frame)" definitions
	for (const char *q = src; (q = strstr(q, "_SetFrame(void")) != NULL; q++)
	{
		const char *e = q;
		while (e > src && (isalnum((unsigned char)e[-1]) || e[-1] == '_')) e--;
		size_t L = (size_t)(q - e);
		if (L == 0 || L >= 150) continue;
		char base[160];
		memcpy(base, e, L); base[L] = 0;
		if (animwork_find(works, nworks, base)) continue;
		char fn[192];
		snprintf(fn, sizeof(fn), "%s_SetFrame", base);
		char *raw = omni_fn_text(src, fn);
		if (!raw) continue;
		char farr[128];
		body_frame_array(raw, known, nfarrs, farr, sizeof(farr));
		if (!farr[0]) { free(raw); continue; }
		AnimWork w;
		memset(&w, 0, sizeof(w));
		w.base = xstrdup(base);
		w.frame_array = xstrdup(farr);
		w.setframe_fn = xstrdup(fn);
		w.setframe_raw = raw;
		w.scale_x = w.scale_y = 1;
		char tex[128], ap[128];
		body_tex_field(raw, tex, sizeof(tex));
		body_arc_ptr(raw, ap, sizeof(ap));
		if (tex[0]) w.tex_field = xstrdup(tex);
		if (ap[0]) w.arc_ptr = xstrdup(ap);
		works = realloc(works, sizeof(AnimWork) * ((size_t)nworks + 1));
		works[nworks++] = w;
	}

	// "<Base>_Draw(Back_... *this, ...)" definitions
	for (const char *q = src; (q = strstr(q, "_Draw(Back_")) != NULL; q++)
	{
		const char *e = q;
		while (e > src && (isalnum((unsigned char)e[-1]) || e[-1] == '_')) e--;
		size_t L = (size_t)(q - e);
		if (L == 0 || L >= 150) continue;
		char base[160];
		memcpy(base, e, L); base[L] = 0;
		char fn[192];
		snprintf(fn, sizeof(fn), "%s_Draw", base);
		char *raw = omni_fn_text(src, fn);
		if (!raw) continue;
		char farr[128];
		body_frame_array(raw, known, nfarrs, farr, sizeof(farr));
		if (!farr[0]) { free(raw); continue; }
		AnimWork *w = animwork_find(works, nworks, base);
		if (!w)
		{
			AnimWork nw;
			memset(&nw, 0, sizeof(nw));
			nw.base = xstrdup(base);
			nw.frame_array = xstrdup(farr);
			nw.scale_x = nw.scale_y = 1;
			char tex[128];
			body_tex_field(raw, tex, sizeof(tex));
			if (tex[0]) nw.tex_field = xstrdup(tex);
			works = realloc(works, sizeof(AnimWork) * ((size_t)nworks + 1));
			works[nworks++] = nw;
			w = &works[nworks - 1];
		}
		free(w->draw_fn); free(w->drawhelper_raw);
		w->draw_fn = xstrdup(fn);
		w->drawhelper_raw = raw;
		const DrawFnInfo *fi = NULL;
		find_draw_call(raw, &fi);
		if (fi && fi->flip) w->flip = true;
		if (strstr(raw, "cframe->off")) w->use_off = true;
		{
			const char *sw = strstr(raw, "src.w *");
			if (sw)
			{
				int v = atoi(sw + strlen("src.w *"));
				if (v > 0) w->scale_x = v;
			}
			const char *sh = strstr(raw, "src.h *");
			if (sh)
			{
				int v = atoi(sh + strlen("src.h *"));
				if (v > 0) w->scale_y = v;
			}
		}
	}

	// Animatable_Init(&this->MEMBER, ANIMARR) in New()
	typedef struct { char *member; char *animarr; } PairMI;
	PairMI *inits = NULL; int ninits = 0;
	if (pr->new_body)
	{
		for (const char *q = pr->new_body; (q = strstr(q, "Animatable_Init(&this->")) != NULL;)
		{
			q += strlen("Animatable_Init(&this->");
			char member[128], arr[128];
			expr_ident(q, member, sizeof(member));
			const char *cm = strchr(q, ',');
			if (!cm) break;
			expr_ident(cm + 1, arr, sizeof(arr));
			if (member[0] && arr[0])
			{
				inits = realloc(inits, sizeof(PairMI) * ((size_t)ninits + 1));
				inits[ninits].member = xstrdup(member);
				inits[ninits].animarr = xstrdup(arr);
				ninits++;
			}
			q = cm + 1;
		}
	}

	// Animatable_Animate(&this->MEMBER, (void*)this, SETFN) in draw bodies
	typedef struct { char *member; char *setfn; } PairMS;
	PairMS *animates = NULL; int nanimates = 0;
	{
		const char *bodies[4] = {pr->drawbg_body, pr->drawmd_body,
		                         pr->drawfg_body, pr->drawhud_body};
		for (int b = 0; b < 4; b++)
		{
			if (!bodies[b]) continue;
			for (const char *q = bodies[b]; (q = strstr(q, "Animatable_Animate(&this->")) != NULL;)
			{
				q += strlen("Animatable_Animate(&this->");
				char member[128], fn[192];
				expr_ident(q, member, sizeof(member));
				const char *cm = strchr(q, ',');
				const char *vp = cm ? strstr(cm, "(void*)this,") : NULL;
				if (!vp) break;
				expr_ident(vp + strlen("(void*)this,"), fn, sizeof(fn));
				if (member[0] && fn[0])
				{
					animates = realloc(animates, sizeof(PairMS) * ((size_t)nanimates + 1));
					animates[nanimates].member = xstrdup(member);
					animates[nanimates].setfn = xstrdup(fn);
					nanimates++;
				}
				q = vp + 1;
			}
		}
	}

	// Build one AnimSet per SetFrame+Draw pair
	for (int i = 0; i < nworks; i++)
	{
		AnimWork *w = &works[i];
		if (!w->setframe_fn || !w->draw_fn || !w->tex_field) continue;

		// link animatable member via the Animate call, then the anim array
		char *member = NULL, *animarr = NULL;
		for (int k = 0; k < nanimates; k++)
			if (strcmp(animates[k].setfn, w->setframe_fn) == 0)
				{ member = animates[k].member; break; }
		if (member)
		{
			for (int k = 0; k < ninits; k++)
				if (strcmp(inits[k].member, member) == 0)
					{ animarr = inits[k].animarr; break; }
		}
		if (!animarr && naarrs == 1)
			animarr = aarrs[0].name;

		// ARC var/tims: arc_ptr "arc_hench_ptr" -> var "arc_hench"
		char arcvar[128] = "";
		if (w->arc_ptr)
		{
			size_t L = strlen(w->arc_ptr);
			if (L > 4 && strcmp(w->arc_ptr + L - 4, "_ptr") == 0)
				L -= 4;
			if (L >= sizeof(arcvar)) L = sizeof(arcvar) - 1;
			memcpy(arcvar, w->arc_ptr, L);
			arcvar[L] = 0;
		}
		const char *arcpath = "";
		if (arcvar[0])
		{
			const OmniMap *m = omnimap_find(io, io_n, arcvar);
			if (m) arcpath = m->b;
		}
		char **tims = NULL; int ntims = 0;
		if (w->arc_ptr)
		{
			size_t alen = strlen(w->arc_ptr);
			for (const char *p = src; (p = strstr(p, w->arc_ptr)) != NULL; p++)
			{
				const char *r = p + alen;
				if (*r != '[' || !isdigit((unsigned char)r[1])) continue;
				int idx = atoi(r + 1);
				const char *cb = strchr(r, ']');
				const char *eq = cb ? strchr(cb, '=') : NULL;
				const char *af = eq ? strstr(eq, "Archive_Find(") : NULL;
				if (!af || (cb && memchr(cb, '\n', (size_t)(af - cb)))) continue;
				const char *cm = strchr(af, ',');
				const char *t1 = cm ? strchr(cm, '"') : NULL;
				const char *t2 = t1 ? strchr(t1 + 1, '"') : NULL;
				if (!t1 || !t2 || idx < 0 || idx > 64) continue;
				while (ntims <= idx)
				{
					tims = realloc(tims, sizeof(char*) * ((size_t)ntims + 1));
					tims[ntims++] = NULL;
				}
				free(tims[idx]);
				tims[idx] = xstrndup(t1 + 1, (size_t)(t2 - t1 - 1));
			}
		}

		AnimSet *a = calloc(1, sizeof(AnimSet));
		a->fnbase = xstrdup(w->base);
		a->draw_fn = xstrdup(w->draw_fn);
		a->tex_field = xstrdup(w->tex_field);
		if (strncmp(w->tex_field, "tex_", 4) == 0)
			a->id = xstrdup(w->tex_field + 4);
		else
			a->id = xstrdup(w->tex_field);
		{
			const char *u = strchr(w->base, '_');
			a->sym = xstrdup(u ? u + 1 : w->base);
		}
		a->arc_var = xstrdup(arcvar);
		a->arc_path = xstrdup(arcpath);
		a->tims = tims;
		a->tim_count = ntims;
		a->frame_array = xstrdup(w->frame_array);
		a->anim_array = xstrdup(animarr ? animarr : "");
		a->animatable = xstrdup(member ? member : "");
		a->scale_x = w->scale_x;
		a->scale_y = w->scale_y;
		a->use_off = w->use_off;
		a->flip = w->flip;
		a->setframe_raw = w->setframe_raw; w->setframe_raw = NULL;
		a->drawhelper_raw = w->drawhelper_raw; w->drawhelper_raw = NULL;
		// transfer frame table (geometry + verbatim init)
		for (int k = 0; k < nfarrs; k++)
			if (strcmp(farrs[k].name, w->frame_array) == 0)
			{
				a->frames = farrs[k].frames; farrs[k].frames = NULL;
				a->frame_count = farrs[k].nframes; farrs[k].nframes = 0;
				a->frames_raw = farrs[k].raw; farrs[k].raw = NULL;
				break;
			}
		if (animarr)
		{
			for (int k = 0; k < naarrs; k++)
				if (strcmp(aarrs[k].name, animarr) == 0)
				{
					a->anims = aarrs[k].anims; aarrs[k].anims = NULL;
					a->anim_count = aarrs[k].nanims; aarrs[k].nanims = 0;
					a->anims_raw = aarrs[k].raw; aarrs[k].raw = NULL;
					break;
				}
		}
		if (a->frame_count == 0) { anim_set_free(a); continue; }
		pr->anims = realloc(pr->anims, sizeof(AnimSet*) * ((size_t)pr->anim_count + 1));
		pr->anims[pr->anim_count++] = a;

		if (pr->drawbg_body)
			omni_anim_instances(pr, a, pr->drawbg_body, LAYER_BG);
		if (pr->drawmd_body)
			omni_anim_instances(pr, a, pr->drawmd_body, LAYER_MD);
		if (pr->drawfg_body)
			omni_anim_instances(pr, a, pr->drawfg_body, LAYER_FG);
		if (pr->drawhud_body)
			omni_anim_instances(pr, a, pr->drawhud_body, LAYER_HUD);
	}

	for (int i = 0; i < nworks; i++) animwork_free(&works[i]);
	free(works);
	for (int i = 0; i < ninits; i++) { free(inits[i].member); free(inits[i].animarr); }
	free(inits);
	for (int i = 0; i < nanimates; i++) { free(animates[i].member); free(animates[i].setfn); }
	free(animates);
	free(known);

cleanup:
	for (int i = 0; i < nfarrs; i++)
	{
		free(farrs[i].name); free(farrs[i].raw); free(farrs[i].frames);
	}
	free(farrs);
	for (int i = 0; i < naarrs; i++)
	{
		free(aarrs[i].name); free(aarrs[i].raw);
		for (int k = 0; k < aarrs[i].nanims; k++)
		{
			free(aarrs[i].anims[k].seq);
			free(aarrs[i].anims[k].raw);
		}
		free(aarrs[i].anims);
	}
	free(aarrs);
}

static Project *project_load_omni(const char *c_path, char *src)
{
	Project *pr = calloc(1, sizeof(Project));
	pr->kind = PROJ_OMNI_STAGE;
	pr->c_path = xstrdup(c_path);

	char *stage = xbase_noext(c_path);
	pr->week = stage; // stage name doubles as "week"
	char prefix[160];
	omni_read_prefix(src, stage, prefix, sizeof(prefix));
	pr->prefix = xstrdup(prefix);

	pr->week_dir = xdir_name(c_path);

	// project root: <root>/src/stage/<name>.c -> <root>
	{
		char *d = xstrdup(pr->week_dir);
		for (int i = 0; i < 2; i++)
		{
			size_t L = strlen(d);
			while (L > 0 && (d[L-1] == '/' || d[L-1] == '\\')) d[--L] = 0;
			char *s1 = strrchr(d, '/');
			char *s2 = strrchr(d, '\\');
			char *s = (s1 > s2) ? s1 : s2;
			const char *b = s ? s + 1 : d;
			if ((i == 0 && strcmp(b, "stage") == 0) ||
			    (i == 1 && strcmp(b, "src") == 0))
			{
				if (s) *s = 0;
				else break;
			}
			else break;
		}
		if (d[0] == 0) { free(d); d = xstrdup(pr->week_dir); }
		pr->project_root = d;
		{
			char buf[2048];
			snprintf(buf, sizeof(buf), "%s/Makefile", pr->project_root);
			pr->makefile = xstrdup(buf);
		}
	}

	// Makefile deps are optional for omni stages (ARC-based loading)
	parse_makefile(pr->makefile, pr->week,
	               &pr->deps, &pr->deps_count,
	               &pr->deps_full, &pr->deps_full_count,
	               &pr->warning);

	pr->drawbg_body = extract_omni_fn(src, prefix, "DrawBG");
	pr->drawmd_body = extract_omni_fn(src, prefix, "DrawMD");
	pr->drawfg_body = extract_omni_fn(src, prefix, "DrawFG");
	pr->drawhud_body = extract_omni_fn(src, prefix, "DrawHUD");
	pr->new_body = extract_omni_fn(src, prefix, "New");

	OmniMap *io = NULL, *finds = NULL, *loads = NULL;
	int io_n = 0, find_n = 0, load_n = 0;
	omni_collect_maps(src, &io, &io_n, &finds, &find_n);
	omni_collect_loads(src, io, io_n, finds, find_n, &loads, &load_n);

	for (int i = 0; i < load_n; i++)
		strlst_push(&pr->load_order, &pr->load_order_count, loads[i].a);

	pr->omni_tex_count = load_n;
	if (load_n > 0)
	{
		pr->omni_tex_fields = calloc((size_t)load_n, sizeof(char*));
		pr->omni_tex_tims = calloc((size_t)load_n, sizeof(char*));
		pr->omni_tex_arcs = calloc((size_t)load_n, sizeof(char*));
		for (int i = 0; i < load_n; i++)
		{
			pr->omni_tex_fields[i] = xstrdup(loads[i].a);
			pr->omni_tex_tims[i] = xstrdup(loads[i].b);
			pr->omni_tex_arcs[i] = xstrdup(loads[i].c);
			// deps views for the UI: tim name (or field) + ARC path
			strlst_push(&pr->deps, &pr->deps_count,
			            loads[i].b[0] ? loads[i].b : loads[i].a);
			strlst_push(&pr->deps_full, &pr->deps_full_count,
			            loads[i].c[0] ? loads[i].c : loads[i].a);
		}
	}

	if (pr->drawbg_body)
		omni_parse_draws(pr, pr->drawbg_body, LAYER_BG, loads, load_n);
	if (pr->drawmd_body)
		omni_parse_draws(pr, pr->drawmd_body, LAYER_MD, loads, load_n);
	if (pr->drawfg_body)
		omni_parse_draws(pr, pr->drawfg_body, LAYER_FG, loads, load_n);
	if (pr->drawhud_body)
		omni_parse_draws(pr, pr->drawhud_body, LAYER_HUD, loads, load_n);

	omni_parse_anims(pr, src, io, io_n);

	omnimap_free(io, io_n);
	omnimap_free(finds, find_n);
	omnimap_free(loads, load_n);
	return pr;
}

static Project *project_load_legacy(const char *c_path, char *src);

Project *project_load(const char *c_path)
{
	char *src = read_file(c_path);
	if (!src)
		return NULL;
	Project *pr;
	if (is_omni_source(src))
		pr = project_load_omni(c_path, src);
	else
		pr = project_load_legacy(c_path, src);
	free(src);
	return pr;
}

static Project *project_load_legacy(const char *c_path, char *src)
{
	Project *pr = calloc(1, sizeof(Project));
	pr->kind = PROJ_LEGACY_WEEK;
	pr->c_path = xstrdup(c_path);

	// week = basename dir (e.g. "week5")
	// project root = dirname(parent(dirname(c_path))) ... c_path = src/weekN/weekN.c
	{
		char *dir = xstrdup(c_path);
		char *slash = strrchr(dir, '/');
		if (slash) *slash = 0;      // dir = .../src/weekN
		char *base = strrchr(dir, '/');
		pr->week = xstrdup(base ? base + 1 : dir);
		// project root = dirname(dir) twice
		char *ddir = xstrdup(dir);
		char *d2 = strrchr(ddir, '/');
		if (d2) *d2 = 0;            // ddir = .../src
		char *d3 = strrchr(ddir, '/');
		if (d3) *d3 = 0;            // ddir = .../PSXFunkin (project root)
		pr->project_root = xstrdup(ddir);
		free(ddir);
		free(dir);

		// week_dir = project_root/iso/week
		{
			char buf[2048];
			snprintf(buf, sizeof(buf), "%s/iso/%s", pr->project_root, pr->week);
			pr->week_dir = xstrdup(buf);
			snprintf(buf, sizeof(buf), "%s/Makefile", pr->project_root);
			pr->makefile = xstrdup(buf);
		}
	}

	// prefix = capitalized week
	{
		char *w = pr->week;
		pr->prefix = xstrdup(w);
		if (pr->prefix[0] >= 'a' && pr->prefix[0] <= 'z')
			pr->prefix[0] = pr->prefix[0] - 'a' + 'A';
	}

	// Makefile deps
	parse_makefile(pr->makefile, pr->week,
	               &pr->deps, &pr->deps_count,
	               &pr->deps_full, &pr->deps_full_count,
	               &pr->warning);

	// --- parse weekN.c (src already read by project_load) ---

	char *load_body = extract_fn_body(src, pr->prefix, "Load");
	char *drawbg_body = extract_fn_body(src, pr->prefix, "DrawBG");
	pr->load_body = load_body;
	pr->drawbg_body = drawbg_body;

	// load order
	parse_load_order(load_body ? load_body : "", &pr->load_order, &pr->load_order_count);

	// resolve bases
	char **bases = NULL; int bases_count = 0;
	{
		// build array of const char* for deps_full
		const char **df = malloc(sizeof(char*) * (size_t)(pr->deps_full_count ? pr->deps_full_count : 1));
		for (int i = 0; i < pr->deps_full_count; i++) df[i] = pr->deps_full[i];
		const char **lo = malloc(sizeof(char*) * (size_t)(pr->load_order_count ? pr->load_order_count : 1));
		for (int i = 0; i < pr->load_order_count; i++) lo[i] = pr->load_order[i];
		resolve_bases(pr->load_order_count, df, pr->deps_full_count,
		              &bases, &bases_count);
		free(df); free(lo);
	}

	// map var -> base
	// build sprites by scanning drawbg_body
	if (drawbg_body)
	{
		Sprite **sprites = NULL;
		int sprite_count = 0;

		// position of first matching draw call
		const char *p = drawbg_body;
		const char *next = p;
		while (next)
		{
			// find earliest occurrence of any draw function
			const char *cand[3];
			cand[0] = strstr(p, "Stage_DrawTex");
			cand[1] = strstr(p, "Gfx_DrawTex");
			const char *fn = NULL;
			const char *best = NULL;
			if (cand[0] && (!best || cand[0] < best)) { best = cand[0]; fn = "Stage_DrawTex"; }
			if (cand[1] && (!best || cand[1] < best)) { best = cand[1]; fn = "Gfx_DrawTex"; }
			if (!best) break;
			p = next = best;

			const char *paren = strchr(p, '(');
			if (!paren) { p += 1; continue; }
			// &VAR , &SRCVAR , rest...
			const char *amp1 = paren + 1;
			while (*amp1 && *amp1 != '&') amp1++;
			if (*amp1 != '&') { p = paren + 1; continue; }
			const char *var_start = amp1 + 1;
			const char *comma1 = strchr(var_start, ',');
			if (!comma1) { p = paren + 1; continue; }
			char *var = xstrndup(var_start, (size_t)(comma1 - var_start));
			// trim
			trim_str(var);

			const char *amp2 = comma1 + 1;
			while (*amp2 && *amp2 != '&') amp2++;
			const char *src_start = amp2 + 1;
			const char *comma2 = strchr(src_start, ',');
			char *src_name = NULL;
			if (comma2)
				src_name = xstrndup(src_start, (size_t)(comma2 - src_start));
			else
				src_name = xstrdup(src_start);
			trim_str(src_name);

			// find base for this var
			char *base = NULL;
			for (int i = 0; i < pr->load_order_count; i++)
				if (strcmp(pr->load_order[i], var) == 0)
				{
					if (i < bases_count) base = xstrdup(bases[i]);
					break;
				}
			if (!base || !base[0]) { free(var); free(src_name); p = comma1 ? comma1 : paren+1; continue; }

			Sprite *s = sprite_new(pr->week_dir, base, var);
			s->draw = (strcmp(fn, "Stage_DrawTex") == 0) ? DRAW_RECT :
			          (strcmp(fn, "Gfx_DrawTex") == 0) ? DRAW_ABS : DRAW_ABS;

			// src rect: search "RECT <src_name> = { ... }" in body
			{
				char needle[256];
				snprintf(needle, sizeof(needle), "%s_src = {", src_name);
				// but src_name may already be e.g. "back_src". We search "RECT back_src = {"
				char needle2[300];
				snprintf(needle2, sizeof(needle2), "RECT %s = {", src_name);
				const char *sp = strstr(drawbg_body, needle2);
				if (sp)
				{
					const char *lb = strchr(sp, '{');
					const char *rb = strchr(lb, '}');
					char *inner = xstrndup(lb+1, (size_t)(rb - lb - 1));
					int cnt;
					char **f = split_top_level(inner, &cnt);
					for (int k = 0; k < cnt && k < 4; k++)
						s->src[k] = (int)round(fixed_field_to_double(f[k]));
					for (int k = 0; k < cnt; k++) free(f[k]);
					free(f);
					free(inner);
				}
				(void)needle;
			}

			// parallax at this call's line
			{
				long line = 0;
				{
					size_t off = (size_t)(p - drawbg_body);
					for (size_t i = 0; i < off; i++) if (drawbg_body[i]=='\n') line++;
				}
				double px, py;
				parallax_at_line(drawbg_body, (int)line, &px, &py);
				s->par_x = px; s->par_y = py;
			}

			// geometry per function
			if (s->draw == DRAW_RECT)
			{
				// find the RECT_FIXED dst var referenced after src
				// rest part: after second comma, look for "&DNAME"
				const char *after = comma2 ? comma2 : (comma1+strlen(src_name));
				const char *amp = strstr(after, "&");
				if (amp)
				{
					const char *dst_start = amp + 1;
					const char *comma = strchr(dst_start, ',');
					const char *close = strchr(dst_start, ')');
					const char *end = comma && comma < close ? comma : close;
					char *dname = xstrndup(dst_start, (size_t)(end - dst_start));
					{ int a=0; while(a<(int)strlen(dname)&&isspace((unsigned char)dname[a]))a++; int b=strlen(dname); while(b>a&&isspace((unsigned char)dname[b-1]))b--; dname[b]=0; memmove(dname,dname+a,strlen(dname)+1); }
					char needle[300];
					snprintf(needle, sizeof(needle), "RECT_FIXED %s = {", dname);
					const char *dp = strstr(drawbg_body, needle);
					if (dp)
					{
						const char *lb = strchr(dp, '{');
						const char *rb = strchr(lb, '}');
						char *inner = xstrndup(lb+1, (size_t)(rb - lb - 1));
						int cnt;
						char **f = split_top_level(inner, &cnt);
						if (cnt >= 4)
						{
							s->x = fixed_field_to_double(f[0]);
							s->y = fixed_field_to_double(f[1]);
							s->w = fixed_field_to_double(f[2]);
							s->h = fixed_field_to_double(f[3]);
						}
						for (int k = 0; k < cnt; k++) free(f[k]);
						free(f);
						free(inner);
					}
					free(dname);
				}
			}
			else if (s->draw == DRAW_ABS)
			{
				const char *after = comma2 ? comma2 : (comma1+strlen(src_name));
				const char *amp = strstr(after, "&");
				if (amp)
				{
					const char *dst_start = amp + 1;
					const char *comma = strchr(dst_start, ',');
					const char *close = strchr(dst_start, ')');
					const char *end = comma && comma < close ? comma : close;
					char *dname = xstrndup(dst_start, (size_t)(end - dst_start));
					{ int a=0; while(a<(int)strlen(dname)&&isspace((unsigned char)dname[a]))a++; int b=strlen(dname); while(b>a&&isspace((unsigned char)dname[b-1]))b--; dname[b]=0; memmove(dname,dname+a,strlen(dname)+1); }
					char needle[300];
					snprintf(needle, sizeof(needle), "RECT %s = {", dname);
					const char *dp = strstr(drawbg_body, needle);
					if (dp)
					{
						const char *lb = strchr(dp, '{');
						const char *rb = strchr(lb, '}');
						char *inner = xstrndup(lb+1, (size_t)(rb - lb - 1));
						int cnt;
						char **f = split_top_level(inner, &cnt);
						if (cnt >= 4)
						{
							double l = fixed_field_to_double(f[0]);
							double t = fixed_field_to_double(f[1]);
							s->w = fixed_field_to_double(f[2]);
							s->h = fixed_field_to_double(f[3]);
							s->x = l - SCREEN_CX;
							s->y = t - SCREEN_CY;
						}
						for (int k = 0; k < cnt; k++) free(f[k]);
						free(f);
						free(inner);
					}
					free(dname);
				}
			}

			sprites = realloc(sprites, sizeof(Sprite*) * (size_t)(sprite_count + 1));
			sprites[sprite_count++] = s;

			free(var);
			free(src_name);
			free(base);

			// advance past this call's closing paren
			const char *cp = strchr(paren, ';');
			p = cp ? cp + 1 : paren + 1;
		}
		pr->sprites = sprites;
		pr->sprite_count = sprite_count;
	}

	for (int i = 0; i < bases_count; i++) free(bases[i]);
	free(bases);

	// --- week metadata: character includes + chart includes ---
	{
		const char *p = src;
		while (1)
		{
			const char *inc = strstr(p, "#include \"iso/chart/");
			if (!inc) break;
			const char *q = inc + strlen("#include \"");
			const char *end = strchr(q, '"');
			if (!end) break;
			char *path = xstrndup(q, (size_t)(end - q));
			pr->chart_paths = realloc(pr->chart_paths,
				sizeof(char*) * (size_t)(pr->chart_path_count + 1));
			pr->chart_paths[pr->chart_path_count++] = path;
			p = end;
		}
		p = src;
		while (1)
		{
			const char *inc = strstr(p, "#include \"character/");
			if (!inc) break;
			const char *q = inc + strlen("#include \"");
			const char *end = strchr(q, '"');
			if (!end) break;
			char *path = xstrndup(q, (size_t)(end - q));
			pr->char_includes = realloc(pr->char_includes,
				sizeof(char*) * (size_t)(pr->char_include_count + 1));
			pr->char_includes[pr->char_include_count++] = path;
		p = end;
	}
	}

	return pr;
}

void project_free(Project *pr)
{
	if (!pr) return;
	free(pr->week); free(pr->prefix); free(pr->c_path);
	free(pr->project_root); free(pr->week_dir); free(pr->makefile);
	free(pr->load_body); free(pr->drawbg_body);
	free(pr->drawmd_body); free(pr->drawfg_body); free(pr->drawhud_body);
	free(pr->new_body);
	free(pr->tick_body); free(pr->getchart_body);
	free(pr->warning);
	for (int i = 0; i < pr->sprite_count; i++) sprite_free(pr->sprites[i]);
	free(pr->sprites);
	for (int i = 0; i < pr->anim_count; i++) anim_set_free(pr->anims[i]);
	free(pr->anims);
	for (int i = 0; i < pr->load_order_count; i++) free(pr->load_order[i]);
	free(pr->load_order);
	for (int i = 0; i < pr->omni_tex_count; i++)
	{
		free(pr->omni_tex_fields[i]);
		free(pr->omni_tex_tims[i]);
		free(pr->omni_tex_arcs[i]);
	}
	free(pr->omni_tex_fields);
	free(pr->omni_tex_tims);
	free(pr->omni_tex_arcs);
	for (int i = 0; i < pr->deps_count; i++) free(pr->deps[i]);
	free(pr->deps);
	for (int i = 0; i < pr->deps_full_count; i++) free(pr->deps_full[i]);
	free(pr->deps_full);
	for (int i = 0; i < pr->char_include_count; i++) free(pr->char_includes[i]);
	free(pr->char_includes);
	for (int i = 0; i < pr->chart_path_count; i++) free(pr->chart_paths[i]);
	free(pr->chart_paths);
	free(pr);
}

// ---- week model parsing (for the week creator) ----

WeekModel *week_model_parse(const char *source, const char *prefix, const char *week)
{
	(void)source; (void)prefix; (void)week;
	WeekModel *m = calloc(1, sizeof(WeekModel));
	m->prefix = xstrdup(prefix);
	m->week = xstrdup(week);
	return m;
}

void week_model_free(WeekModel *m)
{
	if (!m) return;
	free(m->prefix); free(m->week);
	free(m->gf_macro); free(m->getchart_src);
	for (int i = 0; i < m->char_include_count; i++) free(m->char_includes[i]);
	free(m->char_includes);
	for (int i = 0; i < m->chart_var_count; i++) free(m->chart_vars[i]);
	free(m->chart_vars);
	for (int i = 0; i < m->chart_path_count; i++) free(m->chart_paths[i]);
	free(m->chart_paths);
	for (int i = 0; i < 3; i++) free(m->char_fn[i]);
	for (int i = 0; i < 9; i++) free(m->setptr[i]);
	free(m);
}
