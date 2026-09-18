/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "stars_act2.h"

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

//Back_Stars_Act2 background structure
typedef struct
{
	//Stage background base structure
	StageBack back;
	
	IO_Data arc_stat_act2, arc_stat_act2_ptr[2];
	
	//Textures
	Gfx_Tex tex_act2_blend; //blend.tim
	Gfx_Tex tex_act2_close; //close.tim
	Gfx_Tex tex_act2_far; //far.tim
	Gfx_Tex tex_act2_pipem; //pipem.tim
	Gfx_Tex tex_act2_pipelr; //pipelr.tim
	Gfx_Tex tex_act2_scroll; //scroll.tim
	Gfx_Tex tex_act2_p2; //p2.tim
	Gfx_Tex tex_stat_act2; //animated stat_act2
	
	//Stat_Act2 state
	u8 stat_act2_frame, stat_act2_tex_id;
	Animatable stat_act2_animatable;
} Back_Stars_Act2;

//Stat_Act2 animation and rects
static const CharFrame stat_act2_frame[] = {
	{0, {  0,  0,  164,  91}, { 0,  0}},
	{0, {  0,  91,  164,  91}, { 0,  0}},
	{1, {  0,  0,  164,  91}, { 0,  0}},
	{1, {  0,  91,  164,  91}, { 0,  0}},
};

static const Animation stat_act2_anim[] = {
	{2, (const u8[]) {0, 1, 2, 3, ASCR_LOOP}},
};

void Stars_Act2_Stat_SetFrame(void *user, u8 frame)
{
	Back_Stars_Act2 *this = (Back_Stars_Act2*)user;
	
	//Check if this is a new frame
	if (frame != this->stat_act2_frame)
	{
		//Check if new art shall be loaded
		const CharFrame *cframe = &stat_act2_frame[this->stat_act2_frame = frame];
		if (cframe->tex != this->stat_act2_tex_id)
			Gfx_LoadTex(&this->tex_stat_act2, this->arc_stat_act2_ptr[this->stat_act2_tex_id = cframe->tex], 0);
	}
}

void Stars_Act2_Stat_Draw(Back_Stars_Act2 *this, fixed_t x, fixed_t y)
{
	//Draw character
	const CharFrame *cframe = &stat_act2_frame[this->stat_act2_frame];
	
	fixed_t ox = x;
	fixed_t oy = y;

	RECT src = {cframe->src[0], cframe->src[1], cframe->src[2], cframe->src[3]};
	RECT_FIXED dst = {ox, oy, FIXED_DEC(560,1), FIXED_DEC(315,1)};
	Stage_DrawTex(&this->tex_stat_act2, &src, &dst, stage.camera.bzoom, stage.camera.angle);
}

void Back_Stars_Act2_DrawBG(StageBack *back)
{
	Back_Stars_Act2 *this = (Back_Stars_Act2*)back;
	
	fixed_t fx, fy;
	
	RECT act2_blend_src = {0, 0, 144, 41};
	fx = stage.camera.x * 3 / 5;
	fy = stage.camera.y * 3 / 5;
	RECT_FIXED act2_blend_dst = {
		FIXED_DEC(-50,1) - fx,
		FIXED_DEC(-39.5,1) - fy,
		FIXED_DEC(560,1),
		FIXED_DEC(350,1)
	};
	Stage_DrawTex(&this->tex_act2_blend, &act2_blend_src, &act2_blend_dst, stage.camera.bzoom, stage.camera.angle);

	RECT act2_close_src = {0, 0, 224, 76};
	fx = stage.camera.x * 4 / 5;
	fy = stage.camera.y * 4 / 5;
	RECT_FIXED act2_close_dst = {
		FIXED_DEC(-50,1) - fx,
		FIXED_DEC(-26,1) - fy,
		FIXED_DEC(560,1),
		FIXED_DEC(350,1)
	};
	Stage_DrawTex(&this->tex_act2_close, &act2_close_src, &act2_close_dst, stage.camera.bzoom, stage.camera.angle);

	RECT act2_far_src = {0, 0, 188, 33};
	fx = stage.camera.x * 3 / 5;
	fy = stage.camera.y * 3 / 5;
	RECT_FIXED act2_far_dst = {
		FIXED_DEC(-48,1) - fx,
		FIXED_DEC(-22,1) - fy,
		FIXED_DEC(560,1),
		FIXED_DEC(350,1)
	};
	Stage_DrawTex(&this->tex_act2_far, &act2_far_src, &act2_far_dst, stage.camera.bzoom, stage.camera.angle);

	RECT act2_pipem_src = {0, 0, 156, 56};
	fx = stage.camera.x * 7 / 10;
	fy = stage.camera.y * 7 / 10;
	RECT_FIXED act2_pipem_dst = {
		FIXED_DEC(-47,1) - fx,
		FIXED_DEC(-24,1) - fy,
		FIXED_DEC(560,1),
		FIXED_DEC(350,1)
	};
	Stage_DrawTex(&this->tex_act2_pipem, &act2_pipem_src, &act2_pipem_dst, stage.camera.bzoom, stage.camera.angle);

	RECT act2_pipelr_src = {0, 0, 48, 51};
	fx = stage.camera.x * 19 / 20;
	fy = stage.camera.y * 19 / 20;
	RECT_FIXED act2_pipelr_dst = {
		FIXED_DEC(-70,1) - fx,
		FIXED_DEC(63,1) - fy,
		FIXED_DEC(560,1),
		FIXED_DEC(350,1)
	};
	Stage_DrawTex(&this->tex_act2_pipelr, &act2_pipelr_src, &act2_pipelr_dst, stage.camera.bzoom, stage.camera.angle);

	RECT act2_scroll_src = {0, 0, 228, 108};
	fx = stage.camera.x;
	fy = stage.camera.y;
	RECT_FIXED act2_scroll_dst = {
		FIXED_DEC(0,1) - fx,
		FIXED_DEC(0,1) - fy,
		FIXED_DEC(560,1),
		FIXED_DEC(350,1)
	};
	Stage_DrawTex(&this->tex_act2_scroll, &act2_scroll_src, &act2_scroll_dst, stage.camera.bzoom, stage.camera.angle);

	Animatable_Animate(&this->stat_act2_animatable, (void*)this, Stars_Act2_Stat_SetFrame);
	
	fx = stage.camera.x * 1 / 5;
	fy = stage.camera.y * 1 / 5;
	Stars_Act2_Stat_Draw(this, FIXED_DEC(-7,1) - fx, FIXED_DEC(-36,1) - fy);

}

void Back_Stars_Act2_DrawHUD(StageBack *back)
{
	Back_Stars_Act2 *this = (Back_Stars_Act2*)back;
	
	fixed_t fx, fy;
	
	RECT act2_p2_src = {0, 0, 164, 91};
	fx = stage.camera.x;
	fy = stage.camera.y;
	RECT_FIXED act2_p2_dst = {
		FIXED_DEC(0,1) - fx,
		FIXED_DEC(0,1) - fy,
		FIXED_DEC(320,1),
		FIXED_DEC(180,1)
	};
	Stage_DrawTex(&this->tex_act2_p2, &act2_p2_src, &act2_p2_dst, stage.camera.bzoom, stage.camera.angle);

}

void Back_Stars_Act2_Free(StageBack *back)
{
	Back_Stars_Act2 *this = (Back_Stars_Act2*)back;
	
	//Free stat_act2 archive
	Mem_Free(this->arc_stat_act2);
	
	//Free structure
	Mem_Free(this);
}

StageBack *Back_Stars_Act2_New(void)
{
	//Allocate background structure
	Back_Stars_Act2 *this = (Back_Stars_Act2*)Mem_Alloc(sizeof(Back_Stars_Act2));
	if (this == NULL)
		return NULL;
	
	//Set background functions
	this->back.draw_bg = Back_Stars_Act2_DrawBG;
	this->back.draw_md = NULL;
	this->back.draw_fg = NULL;
	this->back.draw_hud = Back_Stars_Act2_DrawHUD;
	this->back.free = Back_Stars_Act2_Free;
	
	//Load background textures
	IO_Data arc_back = IO_Read("\\STARS\\ACT2.ARC;1");
	Gfx_LoadTex(&this->tex_act2_blend, Archive_Find(arc_back, "blend.tim"), 0);
	Gfx_LoadTex(&this->tex_act2_close, Archive_Find(arc_back, "close.tim"), 0);
	Gfx_LoadTex(&this->tex_act2_far, Archive_Find(arc_back, "far.tim"), 0);
	Gfx_LoadTex(&this->tex_act2_pipem, Archive_Find(arc_back, "pipem.tim"), 0);
	Gfx_LoadTex(&this->tex_act2_pipelr, Archive_Find(arc_back, "pipelr.tim"), 0);
	Gfx_LoadTex(&this->tex_act2_scroll, Archive_Find(arc_back, "scroll.tim"), 0);
	Gfx_LoadTex(&this->tex_act2_p2, Archive_Find(arc_back, "p2.tim"), 0);
	Mem_Free(arc_back);
	
	//Load stat_act2 textures
	this->arc_stat_act2 = IO_Read("\\STARS\\ACT2.ARC;1");
	this->arc_stat_act2_ptr[0] = Archive_Find(this->arc_stat_act2, "stat0.tim");
	this->arc_stat_act2_ptr[1] = Archive_Find(this->arc_stat_act2, "stat1.tim");
	
	Animatable_Init(&this->stat_act2_animatable, stat_act2_anim);
	Animatable_SetAnim(&this->stat_act2_animatable, 0);
	this->stat_act2_frame = this->stat_act2_tex_id = 0xFF; //Force art load
	
	return (StageBack*)this;
}
