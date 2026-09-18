/*
  psxtools - core model implementation.
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

#include "model.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

static char *xstrdup(const char *s)
{
	if (!s) return NULL;
	size_t n = strlen(s) + 1;
	char *r = malloc(n);
	if (r) memcpy(r, s, n);
	return r;
}

void sprite_bbox(const Sprite *s, double out[4])
{
	if (s->arb_valid)
	{
		double lx = s->arb[0][0], rx = s->arb[0][0];
		double ty = s->arb[0][1], by = s->arb[0][1];
		for (int i = 1; i < 4; i++)
		{
			if (s->arb[i][0] < lx) lx = s->arb[i][0];
			if (s->arb[i][0] > rx) rx = s->arb[i][0];
			if (s->arb[i][1] < ty) ty = s->arb[i][1];
			if (s->arb[i][1] > by) by = s->arb[i][1];
		}
		out[0] = lx; out[1] = ty; out[2] = rx; out[3] = by;
		return;
	}
	out[0] = s->x;
	out[1] = s->y;
	out[2] = s->x + s->w;
	out[3] = s->y + s->h;
}

// Evaluate a single "FIXED_DEC(a,b)" or plain integer token that is either
// standalone or part of a +/- chain. We sum the chain ignoring +/- fx/fy.
static double eval_field_chain(const char *tok)
{
	if (!tok) return 0.0;
	char t[256];
	strncpy(t, tok, sizeof(t)-1); t[sizeof(t)-1] = 0;
	// strip whitespace
	{
		char *out = t, *in = t;
		for (; *in; in++) if (*in != ' ' && *in != '\t') *out++ = *in;
		*out = 0;
	}
	if (t[0] == 0) return 0.0;
	if (strcmp(t, "SCREEN_WIDTH") == 0) return SCREEN_WIDTH;
	if (strcmp(t, "SCREEN_HEIGHT") == 0) return SCREEN_HEIGHT;

	double total = 0.0;
	const char *p = t;
	int first = 1;
	while (*p)
	{
		int sign = 1;
		if (*p == '+' || *p == '-')
		{
			if (*p == '-') sign = -1;
			p++;
		}
		else if (!first)
		{
			// missing operator between terms; treat as plus
		}
		// skip whitespace
		while (*p == ' ') p++;
		// if term is fx/fy marker, ignore it entirely
		if ((p[0]=='f'||p[0]=='F') && (p[1]=='x'||p[1]=='X'||p[1]=='y'||p[1]=='Y'))
		{
			while (*p && *p != '+' && *p != '-') p++;
			first = 0;
			continue;
		}
		// FIXED_DEC(a,b)
		if (strncmp(p, "FIXED_DEC", 9) == 0)
		{
			p += 9;
			while (*p && *p != '(') p++;
			if (*p == '(')
			{
				p++;
				long a = strtol(p, (char**)&p, 10);
				while (*p && *p != ',') p++;
				if (*p == ',') p++;
				long b = strtol(p, (char**)&p, 10);
				while (*p && *p != ')') p++;
				if (*p) p++;
				total += sign * (double)a / (double)b;
			}
		}
		else
		{
			// plain integer
			char *end = NULL;
			double v = strtod(p, &end);
			if (end == p) break;
			total += sign * v;
			p = end;
		}
		first = 0;
	}
	return total;
}

double fixed_field_to_double(const char *token)
{
	if (!token) return 0.0;
	// remove +/- fx / fy terms and calculate the chain
	return eval_field_chain(token);
}

static int gcd_int(int a, int b)
{
	a = abs(a); b = abs(b);
	while (b) { int t = a % b; a = b; b = t; }
	return a ? a : 1;
}

char *double_to_fixed(double v)
{
	char buf[64];
	if (fabs(v - round(v)) < 1e-6)
	{
		snprintf(buf, sizeof(buf), "FIXED_DEC(%d,1)", (int)round(v));
		return xstrdup(buf);
	}
	// find best den among common divisors
	const int dens[] = {10, 100, 1000};
	for (size_t i = 0; i < sizeof(dens)/sizeof(dens[0]); i++)
	{
		double num = v * dens[i];
		if (fabs(num - round(num)) < 1e-4)
		{
			snprintf(buf, sizeof(buf), "FIXED_DEC(%d,%d)", (int)round(num), dens[i]);
			return xstrdup(buf);
		}
	}
	// fall back to a rational approximation
	for (int den = 1; den <= 1000000; den++)
	{
		double num = v * den;
		if (fabs(num - round(num)) < 1e-6)
		{
			int n = (int)round(num);
			int g = gcd_int(n, den);
			snprintf(buf, sizeof(buf), "FIXED_DEC(%d,%d)", n / g, den / g);
			return xstrdup(buf);
		}
	}
	snprintf(buf, sizeof(buf), "FIXED_DEC(%d,1)", (int)round(v));
	return xstrdup(buf);
}

double eval_cam_mult(const char *expr, char axis)
{
	if (!expr) return 0.0;
	// find "camera.x"/"camera.y"
	const char *needle = (axis == 'x') ? "camera.x" : "camera.y";
	const char *p = strstr(expr, needle);
	if (!p) return 0.0;
	p += strlen(needle);
	// skip parens
	while (*p == ' ' || *p == '(' || *p == ')') p++;
	// bare reference ("fx = stage.camera.x;") means full follow
	if (*p == 0 || *p == ';' || *p == '\n') return 1.0;
	// forms: *A >> B | *A / B | >> B | / B
	long A, B;
	if (sscanf(p, "*%ld >> %ld", &A, &B) == 2)
		return (double)A / (double)(1 << B);
	if (sscanf(p, "*%ld / %ld", &A, &B) == 2)
		return (double)A / (double)B;
	if (sscanf(p, ">> %ld", &B) == 1)
		return 1.0 / (double)(1 << B);
	if (sscanf(p, "/ %ld", &B) == 1)
		return 1.0 / (double)B;
	// unknown suffix after the camera ref: assume full follow rather than 0
	return 1.0;
}

char *mult_expr(double f)
{
	char buf[64];
	if (f == 0.0) return NULL;
	// '*a >> B'  => a / 2^B = f
	for (int B = 1; B <= 10; B++)
	{
		double a = f * (double)(1 << B);
		if (fabs(a - round(a)) < 1e-4)
		{
			snprintf(buf, sizeof(buf), "*%d >> %d", (int)round(a), B);
			return xstrdup(buf);
		}
	}
	// '*a / b'
	const long bs[] = {2, 3, 4, 8, 16, 10, 100};
	for (size_t i = 0; i < sizeof(bs)/sizeof(bs[0]); i++)
	{
		double a = f * (double)bs[i];
		if (fabs(a - round(a)) < 1e-4)
		{
			snprintf(buf, sizeof(buf), "*%d / %ld", (int)round(a), bs[i]);
			return xstrdup(buf);
		}
	}
	(void)snprintf(buf, sizeof(buf), "*%.2f", f);
	return xstrdup(buf);
}

const char *stage_layer_name(StageLayer layer)
{
	switch (layer)
	{
		case LAYER_MD: return "md";
		case LAYER_FG: return "fg";
		case LAYER_HUD: return "hud";
		case LAYER_BG:
		default: return "bg";
	}
}

StageLayer stage_layer_from_fn(const char *fn)
{
	if (!fn) return LAYER_BG;
	if (strcmp(fn, "DrawMD") == 0) return LAYER_MD;
	if (strcmp(fn, "DrawFG") == 0) return LAYER_FG;
	if (strcmp(fn, "DrawHUD") == 0) return LAYER_HUD;
	return LAYER_BG;
}

StageLayer stage_layer_from_name(const char *name)
{
	if (!name) return LAYER_BG;
	char low[16];
	size_t n = strlen(name);
	if (n >= sizeof(low)) n = sizeof(low) - 1;
	for (size_t i = 0; i < n; i++)
		low[i] = (char)tolower((unsigned char)name[i]);
	low[n] = 0;
	if (strcmp(low, "md") == 0 || strcmp(low, "middle") == 0)
		return LAYER_MD;
	if (strcmp(low, "fg") == 0 || strcmp(low, "fore") == 0 ||
	    strcmp(low, "front") == 0)
		return LAYER_FG;
	if (strcmp(low, "hud") == 0)
		return LAYER_HUD;
	return LAYER_BG;
}

void anim_set_free(AnimSet *a)
{
	if (!a) return;
	free(a->id); free(a->sym); free(a->fnbase);
	free(a->draw_fn);
	free(a->tex_field); free(a->arc_var); free(a->arc_path);
	for (int i = 0; i < a->tim_count; i++) free(a->tims[i]);
	free(a->tims);
	free(a->frame_array); free(a->anim_array); free(a->animatable);
	free(a->frames);
	for (int i = 0; i < a->anim_count; i++)
	{
		free(a->anims[i].seq);
		free(a->anims[i].raw);
	}
	free(a->anims);
	free(a->frames_raw); free(a->anims_raw);
	free(a->setframe_raw); free(a->drawhelper_raw);
	free(a);
}
