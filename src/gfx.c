#include "gfx.h"

#include "main.h"
#include "mem.h"
#include "mutil.h"
#include "font.h"

//Gfx constants - Optimized for memory efficiency
#ifndef GFX_OTLEN_DEFAULT
#define GFX_OTLEN_DEFAULT 6  //Keep enough depth to avoid draw instability/flicker
#endif
#ifndef GFX_PRIBUFF_LEN_DEFAULT
#define GFX_PRIBUFF_LEN_DEFAULT 16384 //Keep enough primitive space to prevent overflow artifacts
#endif

//Gfx state
DISPENV disp[2];
DRAWENV draw[2];
u8 db;

static u32 *ot[2];        //Ordering tables
static u16 gfx_otlen;
static u8 *pribuff[2];    //Primitive buffers
static u32 gfx_pribuff_len;
static u8 *nextpri;       //Next primitive pointer
static u8 *nextpri_end;

#define GFX_TINT_SKIP_MAX 32
#define GFX_TINT_ONLY_MAX 64
#define GFX_COLOR_REPLACE_MAX 64

typedef struct
{
	u16 src;
	u16 dst;
} Gfx_ColorReplace;

static u8 gfx_tint_r = 0x80;
static u8 gfx_tint_g = 0x80;
static u8 gfx_tint_b = 0x80;
static u16 gfx_tint_skip[GFX_TINT_SKIP_MAX];
static u8 gfx_tint_skip_count = 0;
static u16 gfx_tint_only[GFX_TINT_ONLY_MAX];
static u8 gfx_tint_only_count = 0;
static Gfx_ColorReplace gfx_color_replace[GFX_COLOR_REPLACE_MAX];
static u8 gfx_color_replace_count = 0;

extern int DecDCTinSync(int mode);
extern int DecDCToutSync(int mode);

static void Gfx_FreeBuffers(void)
{
	if (ot[0] != NULL)
		Mem_Free(ot[0]);
	if (ot[1] != NULL)
		Mem_Free(ot[1]);
	if (pribuff[0] != NULL)
		Mem_Free(pribuff[0]);
	if (pribuff[1] != NULL)
		Mem_Free(pribuff[1]);

	ot[0] = ot[1] = NULL;
	pribuff[0] = pribuff[1] = NULL;
	nextpri = nextpri_end = NULL;
}

static void Gfx_AllocBuffers(u16 otlen, u32 pribuff_len)
{
	gfx_otlen = otlen;
	gfx_pribuff_len = pribuff_len;

	ot[0] = Mem_Alloc(sizeof(**ot) * gfx_otlen);
	ot[1] = Mem_Alloc(sizeof(**ot) * gfx_otlen);
	pribuff[0] = Mem_Alloc(gfx_pribuff_len);
	pribuff[1] = Mem_Alloc(gfx_pribuff_len);

	if (ot[0] == NULL || ot[1] == NULL || pribuff[0] == NULL || pribuff[1] == NULL)
	{
		Gfx_FreeBuffers();
		sprintf(error_msg, "[Gfx_AllocBuffers] out of memory");
		ErrorLock();
	}
}

static void *Gfx_AllocPrim(size_t size)
{
	void *prim;

	if (nextpri == NULL || nextpri + size > nextpri_end)
	{
		sprintf(error_msg, "[Gfx_AllocPrim] primitive buffer overflow");
		ErrorLock();
	}

	prim = nextpri;
	nextpri += size;
	return prim;
}

static u16 Gfx_RGBToPSXColor(u8 r, u8 g, u8 b)
{
	return (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10);
}

static boolean Gfx_ColorTintSkipped(u16 colour)
{
	u16 rgb = colour & 0x7FFF;

	for (u8 i = 0; i < gfx_tint_skip_count; i++)
	{
		if (rgb == gfx_tint_skip[i])
			return true;
	}

	return false;
}

static boolean Gfx_ColorTintTargeted(u16 colour)
{
	u16 rgb = colour & 0x7FFF;

	if (gfx_tint_only_count == 0)
		return true;

	for (u8 i = 0; i < gfx_tint_only_count; i++)
	{
		if (rgb == gfx_tint_only[i])
			return true;
	}

	return false;
}

static boolean Gfx_ReplacePSXColor(u16 colour, u16 *replace)
{
	u16 rgb = colour & 0x7FFF;
	u16 stp = colour & 0x8000;

	for (u8 i = 0; i < gfx_color_replace_count; i++)
	{
		if (rgb == gfx_color_replace[i].src)
		{
			*replace = stp | gfx_color_replace[i].dst;
			return true;
		}
	}

	return false;
}

static u16 Gfx_TintPSXColor(u16 colour)
{
	u16 stp = colour & 0x8000;
	u16 r;
	u16 g;
	u16 b;
	u16 replace;

	if (Gfx_ReplacePSXColor(colour, &replace))
		return replace;

	if (Gfx_ColorTintSkipped(colour))
		return colour;
	if (!Gfx_ColorTintTargeted(colour))
		return colour;

	r = colour & 0x1F;
	g = (colour >> 5) & 0x1F;
	b = (colour >> 10) & 0x1F;

	r = (r * gfx_tint_r) >> 7;
	g = (g * gfx_tint_g) >> 7;
	b = (b * gfx_tint_b) >> 7;

	if (r > 0x1F)
		r = 0x1F;
	if (g > 0x1F)
		g = 0x1F;
	if (b > 0x1F)
		b = 0x1F;

	return stp | r | (g << 5) | (b << 10);
}

static void Gfx_TintImage16(const RECT *rect, u16 *data)
{
	u32 count = (u32)rect->w * (u32)rect->h;

	while (count-- > 0)
	{
		*data = Gfx_TintPSXColor(*data);
		data++;
	}
}

static boolean Gfx_TintActive(void)
{
	return gfx_tint_r != 0x80 || gfx_tint_g != 0x80 || gfx_tint_b != 0x80 || gfx_color_replace_count != 0;
}

static void Gfx_LoadImageTinted16(const RECT *rect, const u32 *src)
{
	RECT load_rect = *rect;
	u32 words = ((u32)rect->w * (u32)rect->h + 1) >> 1;
	u32 *copy = Mem_Alloc(words * sizeof(u32));

	if (copy == NULL)
	{
		sprintf(error_msg, "[Gfx_LoadImageTinted16] out of memory");
		ErrorLock();
	}

	for (u32 i = 0; i < words; i++)
		copy[i] = src[i];

	Gfx_TintImage16(rect, (u16*)copy);
	LoadImage(&load_rect, copy);
	DrawSync(0);
	Mem_Free(copy);
}

void Gfx_SetTint(u8 r, u8 g, u8 b)
{
	gfx_tint_r = r;
	gfx_tint_g = g;
	gfx_tint_b = b;
}

void Gfx_ClearTint(void)
{
	Gfx_SetTint(0x80, 0x80, 0x80);
}

void Gfx_ClearTintSkipColors(void)
{
	gfx_tint_skip_count = 0;
}

void Gfx_AddTintSkipColor(u8 r, u8 g, u8 b)
{
	if (gfx_tint_skip_count >= GFX_TINT_SKIP_MAX)
		return;

	gfx_tint_skip[gfx_tint_skip_count++] = Gfx_RGBToPSXColor(r, g, b);
}

void Gfx_ClearTintOnlyColors(void)
{
	gfx_tint_only_count = 0;
}

void Gfx_AddTintOnlyColor(u8 r, u8 g, u8 b)
{
	if (gfx_tint_only_count >= GFX_TINT_ONLY_MAX)
		return;

	gfx_tint_only[gfx_tint_only_count++] = Gfx_RGBToPSXColor(r, g, b);
}

void Gfx_ClearColorReplaces(void)
{
	gfx_color_replace_count = 0;
}

void Gfx_AddColorReplace(u8 src_r, u8 src_g, u8 src_b, u8 dst_r, u8 dst_g, u8 dst_b)
{
	if (gfx_color_replace_count >= GFX_COLOR_REPLACE_MAX)
		return;

	gfx_color_replace[gfx_color_replace_count].src = Gfx_RGBToPSXColor(src_r, src_g, src_b);
	gfx_color_replace[gfx_color_replace_count].dst = Gfx_RGBToPSXColor(dst_r, dst_g, dst_b);
	gfx_color_replace_count++;
}

static boolean Gfx_ClipTexRect(RECT *src, RECT *dst)
{
	s32 sx = src->x;
	s32 sy = src->y;
	s32 sw = src->w;
	s32 sh = src->h;
	s32 dx = dst->x;
	s32 dy = dst->y;
	s32 dw = dst->w;
	s32 dh = dst->h;
	s32 clip;
	s32 tex_clip;

	if (dw == 0 || dh == 0 || sw == 0 || sh == 0)
		return false;

	if (dw < 0 || dh < 0)
		return true;

	if (dx >= SCREEN_WIDTH || dy >= SCREEN_HEIGHT || dx + dw <= 0 || dy + dh <= 0)
		return false;

	if (dx < 0)
	{
		clip = -dx;
		tex_clip = (clip * sw) / dw;
		sx += tex_clip;
		sw -= tex_clip;
		dw -= clip;
		dx = 0;
	}
	if (dy < 0)
	{
		clip = -dy;
		tex_clip = (clip * sh) / dh;
		sy += tex_clip;
		sh -= tex_clip;
		dh -= clip;
		dy = 0;
	}
	if (dx + dw > SCREEN_WIDTH)
	{
		clip = (dx + dw) - SCREEN_WIDTH;
		tex_clip = (clip * sw) / dw;
		sw -= tex_clip;
		dw -= clip;
	}
	if (dy + dh > SCREEN_HEIGHT)
	{
		clip = (dy + dh) - SCREEN_HEIGHT;
		tex_clip = (clip * sh) / dh;
		sh -= tex_clip;
		dh -= clip;
	}

	if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0)
		return false;

	src->x = sx;
	src->y = sy;
	src->w = sw;
	src->h = sh;
	dst->x = dx;
	dst->y = dy;
	dst->w = dw;
	dst->h = dh;
	return true;
}

//Gfx functions
void Gfx_Init(void)
{
	Gfx_AllocBuffers(GFX_OTLEN_DEFAULT, GFX_PRIBUFF_LEN_DEFAULT);

	//Reset GPU
	ResetGraph(0);
	
	//Initialize display environment
	SetDefDispEnv(&disp[0], 0, 0, 320, 240);
	SetDefDispEnv(&disp[1], 0, 240, 320, 240);
	//Initialize draw environment
	SetDefDrawEnv(&draw[0], 0, 240, 320, 240);
	SetDefDrawEnv(&draw[1], 0, 0, 320, 240);
	
	//Set video mode depending on BIOS region
	switch(*(char*)0xbfc7ff52)
	{
		case 'E':
			SetVideoMode(MODE_PAL);
#ifdef USE_LIBSND
			SsSetTickMode(SS_TICK50);
#endif
			disp[0].screen.y = disp[1].screen.y = 24;
			break;
		default:
			SetVideoMode(MODE_NTSC);
#ifdef USE_LIBSND
			SsSetTickMode(SS_TICK60);
#endif
			break;
	}
	
	//Set draw background
	draw[0].isbg = draw[1].isbg = 1;
	setRGB0(&draw[0], 0, 0, 0);
	setRGB0(&draw[1], 0, 0, 0);

	Gfx_ClearTint();
	Gfx_ClearTintSkipColors();
	Gfx_ClearTintOnlyColors();
	Gfx_ClearColorReplaces();
	Gfx_AddTintSkipColor(0x00, 0x00, 0x00);
	Gfx_AddTintSkipColor(0xFF, 0xFF, 0xFF);
	Gfx_AddTintSkipColor(0xC0, 0xCA, 0xCC);

	FontData_Load(&fonts.font_cdr, Font_CDR, NULL);
	
	//Initialize drawing state
	nextpri = pribuff[0];
	nextpri_end = pribuff[0] + gfx_pribuff_len;
	db = 0;
	ClearOTagR(ot[0], gfx_otlen);
	ClearOTagR(ot[1], gfx_otlen);
	Gfx_Flip();
	Gfx_Flip();
}

void Gfx_Quit(void)
{
	Gfx_FreeBuffers();
}

void Gfx_Flip(void)
{
	//Sync
	DrawSync(0);
	VSync(0);
	
	//Apply environments
	PutDispEnv(&disp[db]);
	PutDrawEnv(&draw[db]);
	
	//Enable display
	SetDispMask(1);
	
	//Draw screen
	DrawOTag(ot[db] + gfx_otlen - 1);
	
	//Flip buffers
	db ^= 1;
	nextpri = pribuff[db];
	nextpri_end = pribuff[db] + gfx_pribuff_len;
	ClearOTagR(ot[db], gfx_otlen);
}

void Gfx_FlipWithoutOT(void)
{
	VSync(0);

  DrawOTag(ot[db] + gfx_otlen - 1);
  db ^= 1;

  nextpri = pribuff[db];
	nextpri_end = pribuff[db] + gfx_pribuff_len;
	ClearOTagR(ot[db], gfx_otlen);

  PutDrawEnv(&draw[db]);
  PutDispEnv(&disp[db]);
  //Enable display
	SetDispMask(1);
}

RECT* Gfx_GetDrawClip(void)
{
	return &draw[db].clip;
}

void Gfx_SetClear(u8 r, u8 g, u8 b)
{
	setRGB0(&draw[0], r, g, b);
	setRGB0(&draw[1], r, g, b);
}

void Gfx_EnableClear(void)
{
	draw[0].isbg = draw[1].isbg = 1;
}

void Gfx_DisableClear(void)
{
	draw[0].isbg = draw[1].isbg = 0;
}

void Gfx_LoadTex(Gfx_Tex *tex, IO_Data data, Gfx_LoadTex_Flag flag)
{
	//Catch NULL data
	if (data == NULL)
	{
		sprintf(error_msg, "[Gfx_LoadTex] data is NULL");
		ErrorLock();
	}
	
	//Read TIM information
	TIM_IMAGE tparam;
	OpenTIM(data);
	ReadTIM(&tparam);
	
	if (tex != NULL)
	{
		tex->tim_mode = tparam.mode;
		tex->pxshift = (2 - (tparam.mode & 0x3));
	}
	
	//Upload pixel data to framebuffer
	if (!(flag & GFX_LOADTEX_NOTEX))
	{
		if (tex != NULL)
		{
			tex->tim_prect = *tparam.prect;
			tex->tpage = getTPage(tparam.mode, 0, tparam.prect->x, tparam.prect->y);
		}
		if (Gfx_TintActive() && ((tparam.mode & 0x3) == 2))
			Gfx_LoadImageTinted16(tparam.prect, (u32*)tparam.paddr);
		else
		{
			LoadImage(tparam.prect, (u32*)tparam.paddr);
			DrawSync(0);
		}
	}
	
	//Upload CLUT to framebuffer if present
	if ((tparam.mode & 0x8) && !(flag & GFX_LOADTEX_NOCLUT))
	{
		if (tex != NULL)
		{
			tex->tim_crect = *tparam.crect;
			tex->clut = getClut(tparam.crect->x, tparam.crect->y);
		}
		if (Gfx_TintActive())
			Gfx_LoadImageTinted16(tparam.crect, (u32*)tparam.caddr);
		else
		{
			LoadImage(tparam.crect, (u32*)tparam.caddr);
			DrawSync(0);
		}
	}
	
	//Free data
	if (flag & GFX_LOADTEX_FREE)
		Mem_Free(data);
}

void Gfx_LoadTexCustomClut(Gfx_Tex *tex, IO_Data data, Gfx_LoadTex_Flag flag, s16 clut_x, s16 clut_y)
{
	if (data == NULL)
	{
		sprintf(error_msg, "[Gfx_LoadTexCustomClut] data is NULL");
		ErrorLock();
	}

	TIM_IMAGE tparam;
	OpenTIM(data);
	ReadTIM(&tparam);

	if (tex != NULL)
	{
		tex->tim_mode = tparam.mode;
		tex->pxshift = (2 - (tparam.mode & 0x3));
	}

	if (!(flag & GFX_LOADTEX_NOTEX))
	{
		if (tex != NULL)
		{
			tex->tim_prect = *tparam.prect;
			tex->tpage = getTPage(tparam.mode, 0, tparam.prect->x, tparam.prect->y);
		}
		if (Gfx_TintActive() && ((tparam.mode & 0x3) == 2))
			Gfx_LoadImageTinted16(tparam.prect, (u32*)tparam.paddr);
		else
		{
			LoadImage(tparam.prect, (u32*)tparam.paddr);
			DrawSync(0);
		}
	}

	if ((tparam.mode & 0x8) && !(flag & GFX_LOADTEX_NOCLUT))
	{
		RECT crect = *tparam.crect;
		crect.x = clut_x;
		crect.y = clut_y;

		if (tex != NULL)
		{
			tex->tim_crect = crect;
			tex->clut = getClut(crect.x, crect.y);
		}
		if (Gfx_TintActive())
			Gfx_LoadImageTinted16(&crect, (u32*)tparam.caddr);
		else
		{
			LoadImage(&crect, (u32*)tparam.caddr);
			DrawSync(0);
		}
	}

	if (flag & GFX_LOADTEX_FREE)
		Mem_Free(data);
}

boolean Gfx_CopyTexRegion(Gfx_Tex *dst, const Gfx_Tex *src, const RECT *region,
	s16 vram_x, s16 vram_y, s16 clut_x, s16 clut_y)
{
	RECT copy;
	RECT clut_copy;
	s16 pixels_per_word;

	if (dst == NULL || src == NULL || region == NULL || region->w <= 0 || region->h <= 0)
		return false;

	pixels_per_word = 1 << src->pxshift;
	copy.x = src->tim_prect.x + (region->x >> src->pxshift);
	copy.y = src->tim_prect.y + region->y;
	copy.w = ((region->x & (pixels_per_word - 1)) + region->w +
	          pixels_per_word - 1) >> src->pxshift;
	copy.h = region->h;

	if (vram_x < 0 || vram_y < 0 || vram_x + copy.w > 1024 || vram_y + copy.h > 512)
		return false;
	/* A sampled frame may not cross a 256x256 texture-page boundary. */
	if ((vram_x & 0x3F) + copy.w > 64 || (vram_y & 0xFF) + copy.h > 256)
		return false;
	if ((src->tim_mode & 0x3) != 0 || !(src->tim_mode & 0x8) ||
	    clut_x < 0 || clut_y < 0 || clut_x + 16 > 1024 || clut_y >= 512)
		return false;

	MoveImage(&copy, vram_x, vram_y);
	clut_copy = src->tim_crect;
	clut_copy.w = 16;
	clut_copy.h = 1;
	MoveImage(&clut_copy, clut_x, clut_y);
	DrawSync(0);

	*dst = *src;
	dst->tim_prect.x = vram_x;
	dst->tim_prect.y = vram_y;
	dst->tim_prect.w = copy.w;
	dst->tim_prect.h = copy.h;
	dst->tpage = getTPage(src->tim_mode, 0, vram_x, vram_y);
	dst->tim_crect.x = clut_x;
	dst->tim_crect.y = clut_y;
	dst->tim_crect.w = 16;
	dst->tim_crect.h = 1;
	dst->clut = getClut(clut_x, clut_y);
	return true;
}

void Gfx_DrawRect(const RECT *rect, u8 r, u8 g, u8 b)
{
	//Add quad
	POLY_F4 *quad = Gfx_AllocPrim(sizeof(POLY_F4));
	setPolyF4(quad);
	setXYWH(quad, rect->x, rect->y, rect->w, rect->h);
	setRGB0(quad, r, g, b);
	
	addPrim(ot[db], quad);
}

void Gfx_BlendRect(const RECT *rect, u8 r, u8 g, u8 b, u8 mode)
{
	//Add quad
	POLY_F4 *quad = Gfx_AllocPrim(sizeof(POLY_F4));
	setPolyF4(quad);
	setXYWH(quad, rect->x, rect->y, rect->w, rect->h);
	setRGB0(quad, r, g, b);
	setSemiTrans(quad, 1);
	
	addPrim(ot[db], quad);
	
	//Add tpage change (this controls transparency mode)
	DR_TPAGE *tpage = Gfx_AllocPrim(sizeof(DR_TPAGE));
	setDrawTPage(tpage, 0, 1, getTPage(0, mode, 0, 0));
	
	addPrim(ot[db], tpage);
}

void Gfx_BlendTex(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 mode)
{
	//Manipulate rects to comply with GPU restrictions
	RECT csrc, cdst;
	csrc = *src;
	cdst = *dst;
	
	if (dst->w < 0)
		csrc.x--;
	if (dst->h < 0)
		csrc.y--;
	
	if ((csrc.x + csrc.w) >= 0x100)
	{
		csrc.w = 0xFF - csrc.x;
		cdst.w = cdst.w * csrc.w / src->w;
	}
	if ((csrc.y + csrc.h) >= 0x100)
	{
		csrc.h = 0xFF - csrc.y;
		cdst.h = cdst.h * csrc.h / src->h;
	}
	//Add quad
	POLY_FT4 *quad = Gfx_AllocPrim(sizeof(POLY_FT4));
	setPolyFT4(quad);
	setUVWH(quad, csrc.x, csrc.y, csrc.w, csrc.h);
	setXYWH(quad, cdst.x, cdst.y, cdst.w, cdst.h);
	setRGB0(quad, 0x80, 0x80, 0x80);
	setSemiTrans(quad, 1);
	quad->tpage = tex->tpage | getTPage(0, mode, 0, 0);
	quad->clut = tex->clut;
	
	addPrim(ot[db], quad);
}

void Gfx_BlendTexV2(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 mode, u8 opacity)
{
	RECT csrc, cdst;
	csrc = *src;
	cdst = *dst;

	if (dst->w < 0)
		csrc.x--;
	if (dst->h < 0)
		csrc.y--;

	if ((csrc.x + csrc.w) >= 0x100)
	{
		csrc.w = 0xFF - csrc.x;
		cdst.w = cdst.w * csrc.w / src->w;
	}

	if ((csrc.y + csrc.h) >= 0x100)
	{
		csrc.h = 0xFF - csrc.y;
		cdst.h = cdst.h * csrc.h / src->h;
	}

	POLY_FT4 *quad = Gfx_AllocPrim(sizeof(POLY_FT4));
	setPolyFT4(quad);
	setUVWH(quad, csrc.x, csrc.y, csrc.w, csrc.h);
	setXYWH(quad, cdst.x, cdst.y, cdst.w, cdst.h);

	//TRUE 0–255 opacity
	setRGB0(quad, opacity, opacity, opacity);

	setSemiTrans(quad, 1);

	quad->tpage = tex->tpage | getTPage(0, mode, 0, 0);
	quad->clut  = tex->clut;

	addPrim(ot[db], quad);
}

void Gfx_BlitTexCol(Gfx_Tex *tex, const RECT *src, s32 x, s32 y, u8 r, u8 g, u8 b)
{
	//Add sprite
	SPRT *sprt = Gfx_AllocPrim(sizeof(SPRT));
	setSprt(sprt);
	setXY0(sprt, x, y);
	setWH(sprt, src->w, src->h);
	setUV0(sprt, src->x, src->y);
	setRGB0(sprt, r, g, b);
	sprt->clut = tex->clut;
	
	addPrim(ot[db], sprt);
	
	//Add tpage change (TODO: reduce tpage changes)
	DR_TPAGE *tpage = Gfx_AllocPrim(sizeof(DR_TPAGE));
	setDrawTPage(tpage, 0, 1, tex->tpage);
	
	addPrim(ot[db], tpage);
}

void Gfx_BlitTex(Gfx_Tex *tex, const RECT *src, s32 x, s32 y)
{
	Gfx_BlitTexCol(tex, src, x, y, 0x80, 0x80, 0x80);
}

void Gfx_DrawTexRotateCol(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 angle, fixed_t hx, fixed_t hy, u8 r, u8 g, u8 b)
{	
	//Manipulate rects to comply with GPU restrictions
    RECT csrc = *src;
    RECT cdst = *dst;

    if (dst->w < 0)
        csrc.x--;
    if (dst->h < 0)
        csrc.y--;

    if ((csrc.x + csrc.w) >= 0x100)
    {
        csrc.w = 0xFF - csrc.x;
        cdst.w = cdst.w * csrc.w / src->w;
    }
    if ((csrc.y + csrc.h) >= 0x100)
    {
        csrc.h = 0xFF - csrc.y;
        cdst.h = cdst.h * csrc.h / src->h;
    }

    s16 sinVal = MUtil_Sin(angle);
    s16 cosVal = MUtil_Cos(angle);

    hx = hx * (cdst.w / csrc.w);
    hy = hy * (cdst.h / csrc.h);

    // Get rotated points
    POINT points[4] = {
        {0 - hx, 0 - hy},
        {cdst.w - hx, 0 - hy},
        {0 - hx, cdst.h - hy},
        {cdst.w - hx, cdst.h - hy}
    };

    for (int i = 0; i < 4; i++)
    {
        MUtil_RotatePoint(&points[i], sinVal, cosVal);
        points[i].x += cdst.x;
        points[i].y += cdst.y;
    }
	
	//Add quad
	POLY_FT4 *quad = Gfx_AllocPrim(sizeof(POLY_FT4));
	setPolyFT4(quad);
	setUVWH(quad, src->x, csrc.y, csrc.w, csrc.h);
    setXY4(quad, points[0].x, points[0].y, points[1].x, points[1].y, points[2].x, points[2].y, points[3].x, points[3].y);
	setRGB0(quad, r, g, b);
	quad->tpage = tex->tpage;
	quad->clut = tex->clut;
	
	addPrim(ot[db], quad);
}

void Gfx_DrawTexRotateColFlipped(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 angle, fixed_t hx, fixed_t hy, u8 r, u8 g, u8 b)
{	
	//Manipulate rects to comply with GPU restrictions
    RECT csrc = *src;
    RECT cdst = *dst;

    if (dst->w < 0)
        csrc.x--;
    if (dst->h < 0)
        csrc.y--;

    if ((csrc.x + csrc.w) >= 0x100)
    {
        csrc.w = 0xFF - csrc.x;
        cdst.w = cdst.w * csrc.w / src->w;
    }
    if ((csrc.y + csrc.h) >= 0x100)
    {
        csrc.h = 0xFF - csrc.y;
        cdst.h = cdst.h * csrc.h / src->h;
    }

    s16 sinVal = MUtil_Sin(angle);
    s16 cosVal = MUtil_Cos(angle);

    hx = hx * (cdst.w / csrc.w);
    hy = hy * (cdst.h / csrc.h);

    // Get rotated points
    POINT points[4] = {
        {0 - hx, 0 - hy},
        {cdst.w - hx, 0 - hy},
        {0 - hx, cdst.h - hy},
        {cdst.w - hx, cdst.h - hy}
    };

    for (int i = 0; i < 4; i++)
    {
        MUtil_RotatePoint(&points[i], sinVal, cosVal);
        points[i].x += cdst.x;
        points[i].y += cdst.y;
    }
	
	//Add quad
	POLY_FT4 *quad = Gfx_AllocPrim(sizeof(POLY_FT4));
	setPolyFT4(quad);
	setUVWH(quad, -csrc.x, csrc.y, -csrc.w, csrc.h);
    setXY4(quad, points[0].x, points[0].y, points[1].x, points[1].y, points[2].x, points[2].y, points[3].x, points[3].y);
	setRGB0(quad, r, g, b);
	quad->tpage = tex->tpage;
	quad->clut = tex->clut;
	
	addPrim(ot[db], quad);
}

void Gfx_DrawTexRotate(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 angle, fixed_t hx, fixed_t hy)
{
	Gfx_DrawTexRotateCol(tex, src, dst, angle, hx, hy, 128, 128, 128);
}

void Gfx_DrawTexCol(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 r, u8 g, u8 b)
{
	//Manipulate rects to comply with GPU restrictions
	RECT csrc, cdst;
	csrc = *src;
	cdst = *dst;
	
	if (dst->w < 0)
		csrc.x--;
	if (dst->h < 0)
		csrc.y--;
	
	if ((csrc.x + csrc.w) >= 0x100)
	{
		csrc.w = 0xFF - csrc.x;
		cdst.w = cdst.w * csrc.w / src->w;
	}
	if ((csrc.y + csrc.h) >= 0x100)
	{
		csrc.h = 0xFF - csrc.y;
		cdst.h = cdst.h * csrc.h / src->h;
	}
	//Add quad
	POLY_FT4 *quad = Gfx_AllocPrim(sizeof(POLY_FT4));
	setPolyFT4(quad);
	setUVWH(quad, csrc.x, csrc.y, csrc.w, csrc.h);
	setXYWH(quad, cdst.x, cdst.y, cdst.w, cdst.h);
	setRGB0(quad, r, g, b);
	quad->tpage = tex->tpage;
	quad->clut = tex->clut;
	
	addPrim(ot[db], quad);
}

void Gfx_DrawTexColFlipX(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 r, u8 g, u8 b)
{
	RECT csrc = *src;
	RECT cdst = *dst;

	if ((csrc.x + csrc.w) >= 0x100)
	{
		csrc.w = 0xFF - csrc.x;
		cdst.w = cdst.w * csrc.w / src->w;
	}
	if ((csrc.y + csrc.h) >= 0x100)
	{
		csrc.h = 0xFF - csrc.y;
		cdst.h = cdst.h * csrc.h / src->h;
	}

	POLY_FT4 *quad = Gfx_AllocPrim(sizeof(POLY_FT4));
	setPolyFT4(quad);
	setXYWH(quad, cdst.x, cdst.y, cdst.w, cdst.h);
	setUV4(
		quad,
		csrc.x + csrc.w, csrc.y,
		csrc.x,          csrc.y,
		csrc.x + csrc.w, csrc.y + csrc.h,
		csrc.x,          csrc.y + csrc.h
	);
	setRGB0(quad, r, g, b);
	quad->tpage = tex->tpage;
	quad->clut = tex->clut;

	addPrim(ot[db], quad);
}

void Gfx_DrawTexColFlipY(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 r, u8 g, u8 b)
{
	RECT csrc = *src;
	RECT cdst = *dst;

	if ((csrc.x + csrc.w) >= 0x100)
	{
		csrc.w = 0xFF - csrc.x;
		cdst.w = cdst.w * csrc.w / src->w;
	}

	if ((csrc.y + csrc.h) >= 0x100)
	{
		csrc.h = 0xFF - csrc.y;
		cdst.h = cdst.h * csrc.h / src->h;
	}

	POLY_FT4 *quad = Gfx_AllocPrim(sizeof(POLY_FT4));
	setPolyFT4(quad);
	setXYWH(quad, cdst.x, cdst.y, cdst.w, cdst.h);

	// ENABLE SEMI-TRANSPARENCY (BLEND MODE)
	setSemiTrans(quad, 1);

	// FLIP Y UVs
	setUV4(
		quad,
		csrc.x,          csrc.y + csrc.h,
		csrc.x + csrc.w, csrc.y + csrc.h,
		csrc.x,          csrc.y,
		csrc.x + csrc.w, csrc.y
	);

	setRGB0(quad, r, g, b);

	quad->tpage = tex->tpage;
	quad->clut  = tex->clut;

	addPrim(ot[db], quad);
}

void Gfx_DrawTexColClipped(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 r, u8 g, u8 b)
{
	RECT csrc = *src;
	RECT cdst = *dst;

	if (!Gfx_ClipTexRect(&csrc, &cdst))
		return;

	Gfx_DrawTexCol(tex, &csrc, &cdst, r, g, b);
}

void Gfx_DrawTex(Gfx_Tex *tex, const RECT *src, const RECT *dst)
{
	Gfx_DrawTexCol(tex, src, dst, 128, 128, 128);
}

void Gfx_DrawTexArbCol(Gfx_Tex *tex, const RECT *src, const POINT *p0, const POINT *p1, const POINT *p2, const POINT *p3, u8 r, u8 g, u8 b)
{
	//Add quad
	POLY_FT4 *quad = Gfx_AllocPrim(sizeof(POLY_FT4));
	setPolyFT4(quad);
	setUVWH(quad, src->x, src->y, src->w, src->h);
	setXY4(quad, p0->x, p0->y, p1->x, p1->y, p2->x, p2->y, p3->x, p3->y);
	setRGB0(quad, r, g, b);
	quad->tpage = tex->tpage;
	quad->clut = tex->clut;
	
	addPrim(ot[db], quad);
}

void Gfx_DrawTexArb(Gfx_Tex *tex, const RECT *src, const POINT *p0, const POINT *p1, const POINT *p2, const POINT *p3)
{
	Gfx_DrawTexArbCol(tex, src, p0, p1, p2, p3, 128, 128, 128);
}

void Gfx_BlendTexArbCol(Gfx_Tex *tex, const RECT *src, const POINT *p0, const POINT *p1, const POINT *p2, const POINT *p3, u8 r, u8 g, u8 b, u8 mode)
{
	//Add quad
	POLY_FT4 *quad = Gfx_AllocPrim(sizeof(POLY_FT4));
	setPolyFT4(quad);
	setUVWH(quad, src->x, src->y, src->w, src->h);
	setXY4(quad, p0->x, p0->y, p1->x, p1->y, p2->x, p2->y, p3->x, p3->y);
	setRGB0(quad, r, g, b);
	setSemiTrans(quad, 1);
	quad->tpage = tex->tpage | getTPage(0, mode, 0, 0);
	quad->clut = tex->clut;
	
	addPrim(ot[db], quad);
}

void Gfx_BlendTexArb(Gfx_Tex *tex, const RECT *src, const POINT *p0, const POINT *p1, const POINT *p2, const POINT *p3, u8 mode)
{
	Gfx_BlendTexArbCol(tex, src, p0, p1, p2, p3, 128, 128, 128, mode);
}

void Gfx_BlendTexRotateCol(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 angle, fixed_t hx, fixed_t hy, u8 mode, u8 r, u8 g, u8 b)
{	
	//Manipulate rects to comply with GPU restrictions
    RECT csrc = *src;
    RECT cdst = *dst;

    if (dst->w < 0)
        csrc.x--;
    if (dst->h < 0)
        csrc.y--;

    if ((csrc.x + csrc.w) >= 0x100)
    {
        csrc.w = 0xFF - csrc.x;
        cdst.w = cdst.w * csrc.w / src->w;
    }
    if ((csrc.y + csrc.h) >= 0x100)
    {
        csrc.h = 0xFF - csrc.y;
        cdst.h = cdst.h * csrc.h / src->h;
    }

    s16 sinVal = MUtil_Sin(angle);
    s16 cosVal = MUtil_Cos(angle);

    hx = hx * (cdst.w / csrc.w);
    hy = hy * (cdst.h / csrc.h);

    // Get rotated points
    POINT points[4] = {
        {0 - hx, 0 - hy},
        {cdst.w - hx, 0 - hy},
        {0 - hx, cdst.h - hy},
        {cdst.w - hx, cdst.h - hy}
    };

    for (int i = 0; i < 4; i++)
    {
        MUtil_RotatePoint(&points[i], sinVal, cosVal);
        points[i].x += cdst.x;
        points[i].y += cdst.y;
    }
	
	//Add quad
	POLY_FT4 *quad = Gfx_AllocPrim(sizeof(POLY_FT4));
	setPolyFT4(quad);
	setUVWH(quad, src->x, csrc.y, csrc.w, csrc.h);
    setXY4(quad, points[0].x, points[0].y, points[1].x, points[1].y, points[2].x, points[2].y, points[3].x, points[3].y);
	setRGB0(quad, r, g, b);
	setSemiTrans(quad, 1);
	quad->tpage = tex->tpage | getTPage(0, mode, 0, 0);
	quad->clut = tex->clut;
	
	addPrim(ot[db], quad);
}

void Gfx_BlendTexRotate(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 angle, fixed_t hx, fixed_t hy, u8 mode)
{
	Gfx_BlendTexRotateCol(tex, src, dst, angle, hx, hy, mode, 128, 128, 128);
}
