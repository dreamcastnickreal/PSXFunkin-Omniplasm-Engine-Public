#ifndef PSXF_GUARD_GFX_H
#define PSXF_GUARD_GFX_H

#include "psx.h"
#include "io.h"
#include "fixed.h"

//Gfx constants
#define SCREEN_WIDTH   320
#define SCREEN_HEIGHT  240
#define SCREEN_WIDTH2  (SCREEN_WIDTH >> 1)
#define SCREEN_HEIGHT2 (SCREEN_HEIGHT >> 1)

#define SCREEN_WIDEADD (SCREEN_WIDTH - 320)
#define SCREEN_TALLADD (SCREEN_HEIGHT - 240)
#define SCREEN_WIDEADD2 (SCREEN_WIDEADD >> 1)
#define SCREEN_TALLADD2 (SCREEN_TALLADD >> 1)

#define SCREEN_WIDEOADD (SCREEN_WIDEADD > 0 ? SCREEN_WIDEADD : 0)
#define SCREEN_TALLOADD (SCREEN_TALLADD > 0 ? SCREEN_TALLADD : 0)
#define SCREEN_WIDEOADD2 (SCREEN_WIDEOADD >> 1)
#define SCREEN_TALLOADD2 (SCREEN_TALLOADD >> 1)

//Gfx structures
typedef struct
{
	u32 tim_mode;
	RECT tim_prect, tim_crect;
	u16 tpage, clut;
	u8 pxshift;
} Gfx_Tex;

//Gfx functions
void Gfx_Init(void);
void Gfx_Quit(void);
void Gfx_Flip(void);
void Gfx_FlipWithoutOT(void);
RECT* Gfx_GetDrawClip(void);
void Gfx_SetClear(u8 r, u8 g, u8 b);
void Gfx_EnableClear(void);
void Gfx_DisableClear(void);
void Gfx_SetTint(u8 r, u8 g, u8 b);
void Gfx_ClearTint(void);
void Gfx_AddTintSkipColor(u8 r, u8 g, u8 b);
void Gfx_ClearTintSkipColors(void);
void Gfx_AddTintOnlyColor(u8 r, u8 g, u8 b);
void Gfx_ClearTintOnlyColors(void);
void Gfx_AddColorReplace(u8 src_r, u8 src_g, u8 src_b, u8 dst_r, u8 dst_g, u8 dst_b);
void Gfx_ClearColorReplaces(void);

typedef u8 Gfx_LoadTex_Flag;
#define GFX_LOADTEX_FREE   (1 << 0)
#define GFX_LOADTEX_NOTEX  (1 << 1)
#define GFX_LOADTEX_NOCLUT (1 << 2)
void Gfx_LoadTex(Gfx_Tex *tex, IO_Data data, Gfx_LoadTex_Flag flag);
void Gfx_LoadTexCustomClut(Gfx_Tex *tex, IO_Data data, Gfx_LoadTex_Flag flag, s16 clut_x, s16 clut_y);
boolean Gfx_CopyTexRegion(Gfx_Tex *dst, const Gfx_Tex *src, const RECT *region,
	s16 vram_x, s16 vram_y, s16 clut_x, s16 clut_y);

void Gfx_DrawRect(const RECT *rect, u8 r, u8 g, u8 b);
void Gfx_BlendRect(const RECT *rect, u8 r, u8 g, u8 b, u8 mode);
void Gfx_BlitTexCol(Gfx_Tex *tex, const RECT *src, s32 x, s32 y, u8 r, u8 g, u8 b);
void Gfx_BlitTex(Gfx_Tex *tex, const RECT *src, s32 x, s32 y);
void Gfx_DrawTexRotateCol(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 angle, fixed_t hx, fixed_t hy, u8 r, u8 g, u8 b);
void Gfx_DrawTexRotateColFlipped(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 angle, fixed_t hx, fixed_t hy, u8 r, u8 g, u8 b);
void Gfx_DrawTexRotate(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 angle, fixed_t hx, fixed_t hy);
void Gfx_DrawTexCol(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 r, u8 g, u8 b);
void Gfx_DrawTexColFlipX(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 r, u8 g, u8 b);
void Gfx_DrawTexColFlipY(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 r, u8 g, u8 b);
void Gfx_DrawTexColClipped(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 r, u8 g, u8 b);
void Gfx_DrawTex(Gfx_Tex *tex, const RECT *src, const RECT *dst);
void Gfx_DrawTexArbCol(Gfx_Tex *tex, const RECT *src, const POINT *p0, const POINT *p1, const POINT *p2, const POINT *p3, u8 r, u8 g, u8 b);
void Gfx_DrawTexArb(Gfx_Tex *tex, const RECT *src, const POINT *p0, const POINT *p1, const POINT *p2, const POINT *p3);
void Gfx_BlendTexArbCol(Gfx_Tex *tex, const RECT *src, const POINT *p0, const POINT *p1, const POINT *p2, const POINT *p3, u8 r, u8 g, u8 b, u8 mode);
void Gfx_BlendTexArb(Gfx_Tex *tex, const RECT *src, const POINT *p0, const POINT *p1, const POINT *p2, const POINT *p3, u8 mode);
void Gfx_BlendTex(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 mode);
void Gfx_BlendTexV2(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 mode, u8 opacity);
void Gfx_BlendTexRotateCol(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 angle, fixed_t hx, fixed_t hy, u8 mode, u8 r, u8 g, u8 b);
void Gfx_BlendTexRotate(Gfx_Tex *tex, const RECT *src, const RECT *dst, u8 angle, fixed_t hx, fixed_t hy, u8 mode);

#endif
