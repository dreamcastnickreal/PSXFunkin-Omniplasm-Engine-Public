/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "stars_act1.h"

#include "../archive.h"
#include "../mem.h"
#include "../stage.h"
#include "../animation.h"

//Stage reference from stars.json (FNF px, not engine code)
//  boyfriend: [770, 450]
//  opponent: [250, 450]
//  girlfriend: [430, 1300]
//  defaultZoom: 0.65
//  isPixelStage: False
//  directory: allfinal
//  hide_girlfriend: False

//Back_Stars_Act1 background structure
typedef struct
{
	//Stage background base structure
	StageBack back;
	
	IO_Data arc_p2tran_act1, arc_p2tran_act1_ptr[4];
	
	IO_Data arc_stat_act1, arc_stat_act1_ptr[2];
	
	//Textures
	Gfx_Tex tex_act1_bg; //bg.tim
	Gfx_Tex tex_act1_fg; //fg.tim
	Gfx_Tex tex_act1_floor; //floor.tim
	Gfx_Tex tex_act1_blend; //blend.tim
	Gfx_Tex tex_act1_sky; //sky.tim
	Gfx_Tex tex_act1_sl; //sl.tim
	Gfx_Tex tex_act1_p1; //p1.tim
	Gfx_Tex tex_p2tran_act1; //animated p2tran_act1
	Gfx_Tex tex_stat_act1; //animated stat_act1
	
	//P2tran_Act1 state
	u8 p2tran_act1_frame_0, p2tran_act1_tex_id_0;
	Animatable p2tran_act1_animatable_0;
	u8 p2tran_act1_frame_1, p2tran_act1_tex_id_1;
	Animatable p2tran_act1_animatable_1;
	
	//Stat_Act1 state
	u8 stat_act1_frame, stat_act1_tex_id;
	Animatable stat_act1_animatable;
} Back_Stars_Act1;

//P2tran_Act1 animation and rects
static const CharFrame p2tran_act1_frame[] = {
	{0, {  148,  0,  72,  67}, { 0,  0}},
	{0, {  148,  67,  72,  66}, { 0,  0}},
	{0, {  148,  133,  72,  66}, { 0,  0}},
	{1, {  148,  0,  72,  66}, { 0,  0}},
	{1, {  148,  66,  72,  66}, { 0,  0}},
	{1, {  148,  132,  68,  66}, { 0,  0}},
	{1, {  0,  148,  68,  66}, { 0,  0}},
	{1, {  68,  148,  68,  66}, { 0,  0}},
	{2, {  0,  0,  64,  67}, { 1,  0}},
	{2, {  0,  67,  64,  67}, { 1,  0}},
	{2, {  0,  134,  64,  67}, { 1,  0}},
	{2, {  64,  0,  64,  67}, { 1,  0}},
	{2, {  128,  0,  64,  67}, { 1,  0}},
	{2, {  192,  0,  64,  67}, { 1,  0}},
	{2, {  64,  67,  60,  67}, { 1,  0}},
	{2, {  64,  134,  60,  67}, { 1,  0}},
	{2, {  124,  67,  60,  67}, { 1,  0}},
	{2, {  184,  67,  60,  67}, { 1,  0}},
	{2, {  124,  134,  60,  67}, { 1,  0}},
	{2, {  184,  134,  60,  67}, { 1,  0}},
	{3, {  0,  0,  60,  67}, { 1,  0}},
	{3, {  0,  67,  60,  67}, { 1,  0}},
	{3, {  0,  134,  60,  67}, { 1,  0}},
	{3, {  60,  0,  60,  67}, { 1,  0}},
	{3, {  120,  0,  60,  67}, { 1,  0}},
	{3, {  180,  0,  60,  67}, { 1,  0}},
	{1, {  0,  227,  148,  11}, { 0,  4}},
	{1, {  0,  214,  148,  13}, { 0,  4}},
	{1, {  0,  113,  148,  35}, { 0,  2}},
	{1, {  0,  63,  148,  50}, { 0,  1}},
	{0, {  0,  195,  148,  58}, { 0,  1}},
	{1, {  0,  0,  148,  63}, { 0,  0}},
	{0, {  0,  0,  148,  65}, { 0,  0}},
	{0, {  0,  65,  148,  65}, { 0,  0}},
	{0, {  0,  130,  148,  65}, { 0,  0}},
};

static const Animation p2tran_act1_anim[] = {
	{2, (const u8[]) {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, ASCR_LOOP}},
	{2, (const u8[]) {26, 27, 28, 29, 30, 31, 32, 33, 34, ASCR_LOOP}},
};

void Stars_Act1_P2tran_SetFrame_0(void *user, u8 frame)
{
	Back_Stars_Act1 *this = (Back_Stars_Act1*)user;
	
	//Check if this is a new frame
	if (frame != this->p2tran_act1_frame_0)
	{
		//Check if new art shall be loaded
		const CharFrame *cframe = &p2tran_act1_frame[this->p2tran_act1_frame_0 = frame];
		if (cframe->tex != this->p2tran_act1_tex_id_0)
			Gfx_LoadTex(&this->tex_p2tran_act1, this->arc_p2tran_act1_ptr[this->p2tran_act1_tex_id_0 = cframe->tex], 0);
	}
}

void Stars_Act1_P2tran_Draw_0(Back_Stars_Act1 *this, fixed_t x, fixed_t y)
{
	//Draw character
	const CharFrame *cframe = &p2tran_act1_frame[this->p2tran_act1_frame_0];
	
	fixed_t ox = x - ((fixed_t)cframe->off[0] << FIXED_SHIFT);
	fixed_t oy = y - ((fixed_t)cframe->off[1] << FIXED_SHIFT);

	RECT src = {cframe->src[0], cframe->src[1], cframe->src[2], cframe->src[3]};
	RECT_FIXED dst = {ox, oy, FIXED_DEC(272,1), FIXED_DEC(262,1)};
	Stage_DrawTex(&this->tex_p2tran_act1, &src, &dst, stage.camera.bzoom, stage.camera.angle);
}

void Stars_Act1_P2tran_SetFrame_1(void *user, u8 frame)
{
	Back_Stars_Act1 *this = (Back_Stars_Act1*)user;
	
	//Check if this is a new frame
	if (frame != this->p2tran_act1_frame_1)
	{
		//Check if new art shall be loaded
		const CharFrame *cframe = &p2tran_act1_frame[this->p2tran_act1_frame_1 = frame];
		if (cframe->tex != this->p2tran_act1_tex_id_1)
			Gfx_LoadTex(&this->tex_p2tran_act1, this->arc_p2tran_act1_ptr[this->p2tran_act1_tex_id_1 = cframe->tex], 0);
	}
}

void Stars_Act1_P2tran_Draw_1(Back_Stars_Act1 *this, fixed_t x, fixed_t y)
{
	//Draw character
	const CharFrame *cframe = &p2tran_act1_frame[this->p2tran_act1_frame_1];
	
	fixed_t ox = x - ((fixed_t)cframe->off[0] << FIXED_SHIFT);
	fixed_t oy = y - ((fixed_t)cframe->off[1] << FIXED_SHIFT);

	RECT src = {cframe->src[0], cframe->src[1], cframe->src[2], cframe->src[3]};
	RECT_FIXED dst = {ox, oy, FIXED_DEC(584,1), FIXED_DEC(255,1)};
	Stage_DrawTex(&this->tex_p2tran_act1, &src, &dst, stage.camera.bzoom, stage.camera.angle);
}

//Stat_Act1 animation and rects
static const CharFrame stat_act1_frame[] = {
	{0, {  0,  0,  164,  91}, { 0,  0}},
	{0, {  0,  91,  164,  91}, { 0,  0}},
	{1, {  0,  0,  164,  91}, { 0,  0}},
	{1, {  0,  91,  164,  91}, { 0,  0}},
};

static const Animation stat_act1_anim[] = {
	{2, (const u8[]) {0, 1, 2, 3, ASCR_LOOP}},
};

void Stars_Act1_Stat_SetFrame(void *user, u8 frame)
{
	Back_Stars_Act1 *this = (Back_Stars_Act1*)user;
	
	//Check if this is a new frame
	if (frame != this->stat_act1_frame)
	{
		//Check if new art shall be loaded
		const CharFrame *cframe = &stat_act1_frame[this->stat_act1_frame = frame];
		if (cframe->tex != this->stat_act1_tex_id)
			Gfx_LoadTex(&this->tex_stat_act1, this->arc_stat_act1_ptr[this->stat_act1_tex_id = cframe->tex], 0);
	}
}

void Stars_Act1_Stat_Draw(Back_Stars_Act1 *this, fixed_t x, fixed_t y)
{
	//Draw character
	const CharFrame *cframe = &stat_act1_frame[this->stat_act1_frame];
	
	fixed_t ox = x;
	fixed_t oy = y;

	RECT src = {cframe->src[0], cframe->src[1], cframe->src[2], cframe->src[3]};
	RECT_FIXED dst = {ox, oy, FIXED_DEC(1280,1), FIXED_DEC(720,1)};
	Stage_DrawTex(&this->tex_stat_act1, &src, &dst, stage.camera.bzoom, stage.camera.angle);
}

void Back_Stars_Act1_DrawBG(StageBack *back)
{
	Back_Stars_Act1 *this = (Back_Stars_Act1*)back;
	
	fixed_t fx, fy;
	
	RECT act1_bg_src = {0, 0, 188, 73};
	fx = stage.camera.x * 4 / 5;
	fy = stage.camera.y * 4 / 5;
	RECT_FIXED act1_bg_dst = {
		FIXED_DEC(-207,1) - fx,
		FIXED_DEC(-65,1) - fy,
		FIXED_DEC(960,1),
		FIXED_DEC(600,1)
	};
	Stage_DrawTex(&this->tex_act1_bg, &act1_bg_src, &act1_bg_dst, stage.camera.bzoom, stage.camera.angle);

	RECT act1_floor_src = {0, 0, 244, 114};
	fx = stage.camera.x;
	fy = stage.camera.y;
	RECT_FIXED act1_floor_dst = {
		FIXED_DEC(-230,1) - fx,
		FIXED_DEC(-64,1) - fy,
		FIXED_DEC(960,1),
		FIXED_DEC(600,1)
	};
	Stage_DrawTex(&this->tex_act1_floor, &act1_floor_src, &act1_floor_dst, stage.camera.bzoom, stage.camera.angle);

	RECT act1_blend_src = {0, 0, 244, 98};
	RECT_FIXED act1_blend_dst = {
		FIXED_DEC(-230,1) - fx,
		FIXED_DEC(-91,1) - fy,
		FIXED_DEC(960,1),
		FIXED_DEC(600,1)
	};
	Stage_DrawTex(&this->tex_act1_blend, &act1_blend_src, &act1_blend_dst, stage.camera.bzoom, stage.camera.angle);

	RECT act1_sky_src = {0, 0, 244, 149};
	fx = stage.camera.x * 3 / 5;
	fy = stage.camera.y * 3 / 5;
	RECT_FIXED act1_sky_dst = {
		FIXED_DEC(-185,1) - fx,
		FIXED_DEC(-66,1) - fy,
		FIXED_DEC(960,1),
		FIXED_DEC(600,1)
	};
	Stage_DrawTex(&this->tex_act1_sky, &act1_sky_src, &act1_sky_dst, stage.camera.bzoom, stage.camera.angle);

	RECT act1_sl_src = {0, 0, 244, 120};
	fx = stage.camera.x * 4 / 5;
	fy = stage.camera.y * 4 / 5;
	RECT_FIXED act1_sl_dst = {
		FIXED_DEC(-210,1) - fx,
		FIXED_DEC(-64,1) - fy,
		FIXED_DEC(960,1),
		FIXED_DEC(600,1)
	};
	Stage_DrawTex(&this->tex_act1_sl, &act1_sl_src, &act1_sl_dst, stage.camera.bzoom, stage.camera.angle);

	Animatable_Animate(&this->p2tran_act1_animatable_0, (void*)this, Stars_Act1_P2tran_SetFrame_0);
	
	fx = stage.camera.x * 1 / 5;
	fy = stage.camera.y * 1 / 5;
	Stars_Act1_P2tran_Draw_0(this, FIXED_DEC(0,1) - fx, FIXED_DEC(33,1) - fy);

	Animatable_Animate(&this->p2tran_act1_animatable_1, (void*)this, Stars_Act1_P2tran_SetFrame_1);
	
	Stars_Act1_P2tran_Draw_1(this, FIXED_DEC(0,1) - fx, FIXED_DEC(32,1) - fy);

	Animatable_Animate(&this->stat_act1_animatable, (void*)this, Stars_Act1_Stat_SetFrame);
	
	fx = stage.camera.x * 2 / 5;
	fy = stage.camera.y * 2 / 5;
	Stars_Act1_Stat_Draw(this, FIXED_DEC(-90,1) - fx, FIXED_DEC(-86,1) - fy);

}

void Back_Stars_Act1_DrawFG(StageBack *back)
{
	Back_Stars_Act1 *this = (Back_Stars_Act1*)back;
	
	fixed_t fx, fy;
	
	RECT act1_fg_src = {0, 0, 244, 43};
	fx = stage.camera.x * 17 / 10;
	fy = stage.camera.y * 17 / 10;
	RECT_FIXED act1_fg_dst = {
		FIXED_DEC(-253,1) - fx,
		FIXED_DEC(-78,1) - fy,
		FIXED_DEC(960,1),
		FIXED_DEC(600,1)
	};
	Stage_DrawTex(&this->tex_act1_fg, &act1_fg_src, &act1_fg_dst, stage.camera.bzoom, stage.camera.angle);

}

void Back_Stars_Act1_DrawHUD(StageBack *back)
{
	Back_Stars_Act1 *this = (Back_Stars_Act1*)back;
	
	fixed_t fx, fy;
	
	RECT act1_p1_src = {0, 0, 164, 91};
	fx = stage.camera.x;
	fy = stage.camera.y;
	RECT_FIXED act1_p1_dst = {
		FIXED_DEC(0,1) - fx,
		FIXED_DEC(0,1) - fy,
		FIXED_DEC(320,1),
		FIXED_DEC(180,1)
	};
	Stage_DrawTex(&this->tex_act1_p1, &act1_p1_src, &act1_p1_dst, stage.camera.bzoom, stage.camera.angle);

}

void Back_Stars_Act1_Free(StageBack *back)
{
	Back_Stars_Act1 *this = (Back_Stars_Act1*)back;
	
	//Free p2tran_act1 archive
	Mem_Free(this->arc_p2tran_act1);
	
	//Free stat_act1 archive
	Mem_Free(this->arc_stat_act1);
	
	//Free structure
	Mem_Free(this);
}

StageBack *Back_Stars_Act1_New(void)
{
	//Allocate background structure
	Back_Stars_Act1 *this = (Back_Stars_Act1*)Mem_Alloc(sizeof(Back_Stars_Act1));
	if (this == NULL)
		return NULL;
	
	//Set background functions
	this->back.draw_bg = Back_Stars_Act1_DrawBG;
	this->back.draw_md = NULL;
	this->back.draw_fg = Back_Stars_Act1_DrawFG;
	this->back.draw_hud = Back_Stars_Act1_DrawHUD;
	this->back.free = Back_Stars_Act1_Free;
	
	//Load background textures
	IO_Data arc_back = IO_Read("\\STARS\\ACT1.ARC;1");
	Gfx_LoadTex(&this->tex_act1_bg, Archive_Find(arc_back, "bg.tim"), 0);
	Gfx_LoadTex(&this->tex_act1_fg, Archive_Find(arc_back, "fg.tim"), 0);
	Gfx_LoadTex(&this->tex_act1_floor, Archive_Find(arc_back, "floor.tim"), 0);
	Gfx_LoadTex(&this->tex_act1_blend, Archive_Find(arc_back, "blend.tim"), 0);
	Gfx_LoadTex(&this->tex_act1_sky, Archive_Find(arc_back, "sky.tim"), 0);
	Gfx_LoadTex(&this->tex_act1_sl, Archive_Find(arc_back, "sl.tim"), 0);
	Gfx_LoadTex(&this->tex_act1_p1, Archive_Find(arc_back, "p1.tim"), 0);
	Mem_Free(arc_back);
	
	//Load p2tran_act1 textures
	this->arc_p2tran_act1 = IO_Read("\\STARS\\ACT1.ARC;1");
	this->arc_p2tran_act1_ptr[0] = Archive_Find(this->arc_p2tran_act1, "p2tran0.tim");
	this->arc_p2tran_act1_ptr[1] = Archive_Find(this->arc_p2tran_act1, "p2tran1.tim");
	this->arc_p2tran_act1_ptr[2] = Archive_Find(this->arc_p2tran_act1, "p2tran2.tim");
	this->arc_p2tran_act1_ptr[3] = Archive_Find(this->arc_p2tran_act1, "p2tran3.tim");
	
	Animatable_Init(&this->p2tran_act1_animatable_0, p2tran_act1_anim);
	Animatable_SetAnim(&this->p2tran_act1_animatable_0, 0);
	this->p2tran_act1_frame_0 = this->p2tran_act1_tex_id_0 = 0xFF; //Force art load
	
	Animatable_Init(&this->p2tran_act1_animatable_1, p2tran_act1_anim);
	Animatable_SetAnim(&this->p2tran_act1_animatable_1, 1);
	this->p2tran_act1_frame_1 = this->p2tran_act1_tex_id_1 = 0xFF; //Force art load
	
	//Load stat_act1 textures
	this->arc_stat_act1 = IO_Read("\\STARS\\ACT1.ARC;1");
	this->arc_stat_act1_ptr[0] = Archive_Find(this->arc_stat_act1, "stat0.tim");
	this->arc_stat_act1_ptr[1] = Archive_Find(this->arc_stat_act1, "stat1.tim");
	
	Animatable_Init(&this->stat_act1_animatable, stat_act1_anim);
	Animatable_SetAnim(&this->stat_act1_animatable, 0);
	this->stat_act1_frame = this->stat_act1_tex_id = 0xFF; //Force art load
	
	return (StageBack*)this;
}
