/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

//Generated with psxtools (Omniplasm stage format).
//Remember to register the New() constructor in your stage map/def
//and to pack the ARC files it loads.

#include "stars_act1.h"

#include "../archive.h"
#include "../mem.h"
#include "../stage.h"
#include "../animation.h"

//Back_Stars_Act1 background structure
typedef struct
{
	//Stage background base structure
	StageBack back;
	
	IO_Data arc_stat_act1, arc_stat_act1_ptr[2];
	
	//Textures
	Gfx_Tex tex_p2tran_act1; //p2tran3.tim
	Gfx_Tex tex_stat_act1; //stat1.tim
	Gfx_Tex tex_act1_bg; //bg.tim
	Gfx_Tex tex_act1_fg; //fg.tim
	Gfx_Tex tex_act1_floor; //floor.tim
	Gfx_Tex tex_act1_blend; //blend.tim
	Gfx_Tex tex_act1_sky; //sky.tim
	Gfx_Tex tex_act1_sl; //sl.tim
	Gfx_Tex tex_act1_p1; //p1.tim
	
	//Act1_Stat state
	u8 stat_act1_frame, stat_act1_tex_id;
	Animatable stat_act1_animatable;
} Back_Stars_Act1;

//Act1_Stat animation and rects
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
	
	Animatable_Animate(&this->stat_act1_animatable, (void*)this, Stars_Act1_Stat_SetFrame);
	
	RECT floor_p2_src = { 0, 0, 244, 114 };
	fx = stage.camera.x;
	fy = stage.camera.y;
	RECT_FIXED floor_p2_dst = {
		FIXED_DEC(-314,1) - fx,
		FIXED_DEC(-64,1) - fy,
		FIXED_DEC(640,1),
		FIXED_DEC(300,1)
	};
	Stage_DrawTex(&this->tex_act1_floor, &floor_p2_src, &floor_p2_dst, stage.camera.bzoom, stage.camera.angle);

	RECT bg_p3_src = { 0, 0, 188, 73 };
	fx = stage.camera.x*8 / 10;
	fy = stage.camera.y*8 / 10;
	RECT_FIXED bg_p3_dst = {
		FIXED_DEC(-314,1) - fx,
		FIXED_DEC(-220,1) - fy,
		FIXED_DEC(640,1),
		FIXED_DEC(248,1)
	};
	Stage_DrawTex(&this->tex_act1_bg, &bg_p3_src, &bg_p3_dst, stage.camera.bzoom, stage.camera.angle);

	RECT blend_p4_src = { 0, 0, 244, 98 };
	fx = stage.camera.x;
	fy = stage.camera.y;
	RECT_FIXED blend_p4_dst = {
		FIXED_DEC(-314,1) - fx,
		FIXED_DEC(-240,1) - fy,
		FIXED_DEC(640,1),
		FIXED_DEC(258,1)
	};
	Stage_DrawTex(&this->tex_act1_blend, &blend_p4_src, &blend_p4_dst, stage.camera.bzoom, stage.camera.angle);

	RECT sl_p5_src = { 0, 0, 244, 120 };
	fx = stage.camera.x*8 / 10;
	fy = stage.camera.y*8 / 10;
	RECT_FIXED sl_p5_dst = {
		FIXED_DEC(-315,1) - fx,
		FIXED_DEC(-180,1) - fy,
		FIXED_DEC(640,1),
		FIXED_DEC(315,1)
	};
	Stage_DrawTex(&this->tex_act1_sl, &sl_p5_src, &sl_p5_dst, stage.camera.bzoom, stage.camera.angle);

	RECT sky_p6_src = { 0, 0, 244, 149 };
	fx = stage.camera.x*6 / 10;
	fy = stage.camera.y*6 / 10;
	RECT_FIXED sky_p6_dst = {
		FIXED_DEC(-314,1) - fx,
		FIXED_DEC(-240,1) - fy,
		FIXED_DEC(640,1),
		FIXED_DEC(390,1)
	};
	Stage_DrawTex(&this->tex_act1_sky, &sky_p6_src, &sky_p6_dst, stage.camera.bzoom, stage.camera.angle);

	fx = stage.camera.x*4 / 10;
	fy = stage.camera.y*4 / 10;
	Stars_Act1_Stat_Draw(this, FIXED_DEC(-314,1) - fx, FIXED_DEC(-240,1) - fy);

}

void Back_Stars_Act1_DrawFG(StageBack *back)
{
	Back_Stars_Act1 *this = (Back_Stars_Act1*)back;
	
	fixed_t fx, fy;
	
	RECT fg_p1_src = { 0, 0, 244, 43 };
	fx = stage.camera.x*17 / 10;
	fy = stage.camera.y*17 / 10;
	RECT_FIXED fg_p1_dst = {
		FIXED_DEC(-314,1) - fx,
		FIXED_DEC(0,1) - fy,
		FIXED_DEC(640,1),
		FIXED_DEC(112,1)
	};
	Stage_DrawTex(&this->tex_act1_fg, &fg_p1_src, &fg_p1_dst, stage.camera.bzoom, stage.camera.angle);

}

void Back_Stars_Act1_DrawHUD(StageBack *back)
{
	Back_Stars_Act1 *this = (Back_Stars_Act1*)back;
	
	fixed_t fx, fy;
	
	RECT p1_p0_src = { 0, 0, 164, 91 };
	fx = stage.camera.x;
	fy = stage.camera.y;
	RECT_FIXED p1_p0_dst = {
		FIXED_DEC(-160,1) - fx,
		FIXED_DEC(-120,1) - fy,
		FIXED_DEC(320,1),
		FIXED_DEC(240,1)
	};
	Stage_DrawTex(&this->tex_act1_p1, &p1_p0_src, &p1_p0_dst, stage.camera.bzoom, stage.camera.angle);

}

void Back_Stars_Act1_Free(StageBack *back)
{
	Back_Stars_Act1 *this = (Back_Stars_Act1*)back;
	
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
	Gfx_LoadTex(&this->tex_p2tran_act1, Archive_Find(arc_back, "p2tran3.tim"), 0);
	Gfx_LoadTex(&this->tex_act1_bg, Archive_Find(arc_back, "bg.tim"), 0);
	Gfx_LoadTex(&this->tex_act1_fg, Archive_Find(arc_back, "fg.tim"), 0);
	Gfx_LoadTex(&this->tex_act1_floor, Archive_Find(arc_back, "floor.tim"), 0);
	Gfx_LoadTex(&this->tex_act1_blend, Archive_Find(arc_back, "blend.tim"), 0);
	Gfx_LoadTex(&this->tex_act1_sky, Archive_Find(arc_back, "sky.tim"), 0);
	Gfx_LoadTex(&this->tex_act1_sl, Archive_Find(arc_back, "sl.tim"), 0);
	Gfx_LoadTex(&this->tex_act1_p1, Archive_Find(arc_back, "p1.tim"), 0);
	Mem_Free(arc_back);
	
	//Load stat_act1 textures
	this->arc_stat_act1 = IO_Read("\\STARS\\ACT1.ARC;1");
	this->arc_stat_act1_ptr[0] = Archive_Find(this->arc_stat_act1, "stat0.tim");
	this->arc_stat_act1_ptr[1] = Archive_Find(this->arc_stat_act1, "stat1.tim");
	
	Animatable_Init(&this->stat_act1_animatable, stat_act1_anim);
	Animatable_SetAnim(&this->stat_act1_animatable, 0);
	this->stat_act1_frame = this->stat_act1_tex_id = 0xFF; //Force art load
	
	return (StageBack*)this;
}
