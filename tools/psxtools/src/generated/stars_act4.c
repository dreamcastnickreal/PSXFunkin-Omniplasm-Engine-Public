/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "stars_act4.h"

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

//Back_Stars_Act4 background structure
typedef struct
{
	//Stage background base structure
	StageBack back;
	
	IO_Data arc_dead_act4, arc_dead_act4_ptr[10];
	
	IO_Data arc_obj_act4, arc_obj_act4_ptr[1];
	
	IO_Data arc_rip_act4, arc_rip_act4_ptr[3];
	
	IO_Data arc_stat_act4, arc_stat_act4_ptr[2];
	
	IO_Data arc_strike_act4, arc_strike_act4_ptr[3];
	
	//Textures
	Gfx_Tex tex_act4_go; //go.tim
	Gfx_Tex tex_act4_pipe; //pipe.tim
	Gfx_Tex tex_act4_mem; //mem.tim
	Gfx_Tex tex_act4_exe; //exe.tim
	Gfx_Tex tex_act4_light; //light.tim
	Gfx_Tex tex_dead_act4; //animated dead_act4
	Gfx_Tex tex_obj_act4; //animated obj_act4
	Gfx_Tex tex_rip_act4; //animated rip_act4
	Gfx_Tex tex_stat_act4; //animated stat_act4
	Gfx_Tex tex_strike_act4; //animated strike_act4
	
	//Dead_Act4 state
	u8 dead_act4_frame, dead_act4_tex_id;
	Animatable dead_act4_animatable;
	
	//Obj_Act4 state
	u8 obj_act4_frame, obj_act4_tex_id;
	Animatable obj_act4_animatable;
	
	//Rip_Act4 state
	u8 rip_act4_frame, rip_act4_tex_id;
	Animatable rip_act4_animatable;
	
	//Stat_Act4 state
	u8 stat_act4_frame, stat_act4_tex_id;
	Animatable stat_act4_animatable;
	
	//Strike_Act4 state
	u8 strike_act4_frame, strike_act4_tex_id;
	Animatable strike_act4_animatable;
} Back_Stars_Act4;

//Dead_Act4 animation and rects
static const CharFrame dead_act4_frame[] = {
	{0, {  0,  193,  60,  57}, { 15,  8}},
	{1, {  0,  0,  108,  90}, { 8,  4}},
	{0, {  0,  96,  128,  97}, { 3,  3}},
	{0, {  0,  0,  132,  96}, { 2,  2}},
	{2, {  0,  95,  136,  95}, { 1,  3}},
	{2, {  0,  0,  140,  95}, { 1,  3}},
	{2, {  140,  0,  108,  95}, { 5,  3}},
	{3, {  108,  0,  108,  95}, { 5,  3}},
	{0, {  132,  0,  108,  96}, { 5,  2}},
	{4, {  108,  97,  108,  96}, { 5,  2}},
	{5, {  0,  0,  108,  96}, { 5,  2}},
	{5, {  108,  0,  108,  96}, { 5,  2}},
	{5, {  0,  96,  108,  96}, { 5,  2}},
	{5, {  108,  96,  108,  96}, { 5,  2}},
	{3, {  0,  0,  108,  96}, { 5,  2}},
	{4, {  108,  0,  108,  97}, { 5,  2}},
	{3, {  108,  95,  108,  95}, { 5,  3}},
	{3, {  0,  96,  108,  95}, { 5,  3}},
	{6, {  108,  114,  108,  109}, { 5,  1}},
	{7, {  0,  0,  108,  109}, { 5,  1}},
	{7, {  0,  109,  108,  109}, { 5,  1}},
	{7, {  108,  0,  108,  109}, { 5,  1}},
	{7, {  108,  109,  108,  109}, { 5,  1}},
	{8, {  0,  0,  108,  109}, { 5,  1}},
	{8, {  0,  109,  108,  109}, { 5,  1}},
	{8, {  108,  0,  108,  109}, { 5,  1}},
	{8, {  108,  109,  108,  109}, { 5,  1}},
	{4, {  0,  0,  108,  109}, { 5,  1}},
	{4, {  0,  109,  108,  109}, { 5,  1}},
	{2, {  136,  95,  108,  114}, { 5,  0}},
	{0, {  128,  96,  108,  114}, { 5,  0}},
	{9, {  0,  0,  108,  114}, { 5,  0}},
	{9, {  0,  114,  108,  114}, { 5,  0}},
	{9, {  108,  0,  108,  114}, { 5,  0}},
	{9, {  108,  114,  108,  114}, { 5,  0}},
	{6, {  0,  0,  108,  114}, { 5,  0}},
	{6, {  0,  114,  108,  114}, { 5,  0}},
	{6, {  108,  0,  108,  114}, { 5,  0}},
	{0, {  0,  250,  44,  5}, { 0,  0}},
};

static const Animation dead_act4_anim[] = {
	{2, (const u8[]) {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, ASCR_HOLD}},
	{2, (const u8[]) {38, ASCR_LOOP}},
};

void Stars_Act4_Dead_SetFrame(void *user, u8 frame)
{
	Back_Stars_Act4 *this = (Back_Stars_Act4*)user;
	
	//Check if this is a new frame
	if (frame != this->dead_act4_frame)
	{
		//Check if new art shall be loaded
		const CharFrame *cframe = &dead_act4_frame[this->dead_act4_frame = frame];
		if (cframe->tex != this->dead_act4_tex_id)
			Gfx_LoadTex(&this->tex_dead_act4, this->arc_dead_act4_ptr[this->dead_act4_tex_id = cframe->tex], 0);
	}
}

void Stars_Act4_Dead_Draw(Back_Stars_Act4 *this, fixed_t x, fixed_t y)
{
	//Draw character
	const CharFrame *cframe = &dead_act4_frame[this->dead_act4_frame];
	
	fixed_t ox = x - ((fixed_t)cframe->off[0] << FIXED_SHIFT);
	fixed_t oy = y - ((fixed_t)cframe->off[1] << FIXED_SHIFT);

	RECT src = {cframe->src[0], cframe->src[1], cframe->src[2], cframe->src[3]};
	RECT_FIXED dst = {ox, oy, FIXED_DEC(375,1), FIXED_DEC(245,1)};
	Stage_DrawTex(&this->tex_dead_act4, &src, &dst, stage.camera.bzoom, stage.camera.angle);
}

//Obj_Act4 animation and rects
static const CharFrame obj_act4_frame[] = {
	{0, {  112,  0,  48,  42}, { -6,  -11}},
	{0, {  200,  40,  36,  38}, { -12,  -11}},
	{0, {  112,  42,  44,  35}, { -8,  -12}},
	{0, {  68,  49,  40,  37}, { -10,  -11}},
	{0, {  160,  0,  40,  40}, { -8,  -11}},
	{0, {  108,  77,  40,  36}, { -11,  -12}},
	{0, {  160,  40,  40,  39}, { -10,  -8}},
	{0, {  68,  0,  44,  49}, { -9,  -6}},
	{0, {  200,  0,  40,  40}, { -11,  -10}},
	{0, {  0,  0,  68,  60}, { 0,  0}},
};

static const Animation obj_act4_anim[] = {
	{2, (const u8[]) {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, ASCR_LOOP}},
};

void Stars_Act4_Obj_SetFrame(void *user, u8 frame)
{
	Back_Stars_Act4 *this = (Back_Stars_Act4*)user;
	
	//Check if this is a new frame
	if (frame != this->obj_act4_frame)
	{
		//Check if new art shall be loaded
		const CharFrame *cframe = &obj_act4_frame[this->obj_act4_frame = frame];
		if (cframe->tex != this->obj_act4_tex_id)
			Gfx_LoadTex(&this->tex_obj_act4, this->arc_obj_act4_ptr[this->obj_act4_tex_id = cframe->tex], 0);
	}
}

void Stars_Act4_Obj_Draw(Back_Stars_Act4 *this, fixed_t x, fixed_t y)
{
	//Draw character
	const CharFrame *cframe = &obj_act4_frame[this->obj_act4_frame];
	
	fixed_t ox = x - ((fixed_t)cframe->off[0] << FIXED_SHIFT);
	fixed_t oy = y - ((fixed_t)cframe->off[1] << FIXED_SHIFT);

	RECT src = {cframe->src[0], cframe->src[1], cframe->src[2], cframe->src[3]};
	RECT_FIXED dst = {ox, oy, FIXED_DEC(66,1), FIXED_DEC(59,1)};
	Stage_DrawTex(&this->tex_obj_act4, &src, &dst, stage.camera.bzoom, stage.camera.angle);
}

//Rip_Act4 animation and rects
static const CharFrame rip_act4_frame[] = {
	{0, {  0,  95,  180,  50}, { -7,  -12}},
	{0, {  0,  0,  192,  49}, { -5,  -12}},
	{1, {  0,  175,  200,  48}, { 0,  -14}},
	{0, {  0,  49,  200,  46}, { 0,  -16}},
	{2, {  0,  95,  192,  43}, { -3,  -15}},
	{2, {  0,  138,  188,  43}, { -4,  -19}},
	{0, {  0,  145,  188,  47}, { -7,  -15}},
	{1, {  0,  62,  184,  57}, { -7,  -4}},
	{1, {  0,  0,  184,  62}, { -7,  0}},
	{1, {  0,  119,  184,  56}, { -6,  -5}},
	{2, {  0,  0,  180,  48}, { -7,  -14}},
	{2, {  0,  48,  180,  47}, { -7,  -14}},
	{0, {  0,  192,  180,  49}, { -7,  -12}},
};

static const Animation rip_act4_anim[] = {
	{2, (const u8[]) {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, ASCR_LOOP}},
};

void Stars_Act4_Rip_SetFrame(void *user, u8 frame)
{
	Back_Stars_Act4 *this = (Back_Stars_Act4*)user;
	
	//Check if this is a new frame
	if (frame != this->rip_act4_frame)
	{
		//Check if new art shall be loaded
		const CharFrame *cframe = &rip_act4_frame[this->rip_act4_frame = frame];
		if (cframe->tex != this->rip_act4_tex_id)
			Gfx_LoadTex(&this->tex_rip_act4, this->arc_rip_act4_ptr[this->rip_act4_tex_id = cframe->tex], 0);
	}
}

void Stars_Act4_Rip_Draw(Back_Stars_Act4 *this, fixed_t x, fixed_t y)
{
	//Draw character
	const CharFrame *cframe = &rip_act4_frame[this->rip_act4_frame];
	
	fixed_t ox = x - ((fixed_t)cframe->off[0] << FIXED_SHIFT);
	fixed_t oy = y - ((fixed_t)cframe->off[1] << FIXED_SHIFT);

	RECT src = {cframe->src[0], cframe->src[1], cframe->src[2], cframe->src[3]};
	RECT_FIXED dst = {ox, oy, FIXED_DEC(398,1), FIXED_DEC(122,1)};
	Stage_DrawTex(&this->tex_rip_act4, &src, &dst, stage.camera.bzoom, stage.camera.angle);
}

//Stat_Act4 animation and rects
static const CharFrame stat_act4_frame[] = {
	{0, {  0,  0,  176,  99}, { 0,  0}},
	{0, {  0,  99,  176,  99}, { 0,  0}},
	{1, {  0,  0,  176,  99}, { 0,  0}},
	{1, {  0,  99,  176,  99}, { 0,  0}},
};

static const Animation stat_act4_anim[] = {
	{2, (const u8[]) {0, 1, 2, 3, ASCR_LOOP}},
};

void Stars_Act4_Stat_SetFrame(void *user, u8 frame)
{
	Back_Stars_Act4 *this = (Back_Stars_Act4*)user;
	
	//Check if this is a new frame
	if (frame != this->stat_act4_frame)
	{
		//Check if new art shall be loaded
		const CharFrame *cframe = &stat_act4_frame[this->stat_act4_frame = frame];
		if (cframe->tex != this->stat_act4_tex_id)
			Gfx_LoadTex(&this->tex_stat_act4, this->arc_stat_act4_ptr[this->stat_act4_tex_id = cframe->tex], 0);
	}
}

void Stars_Act4_Stat_Draw(Back_Stars_Act4 *this, fixed_t x, fixed_t y)
{
	//Draw character
	const CharFrame *cframe = &stat_act4_frame[this->stat_act4_frame];
	
	fixed_t ox = x;
	fixed_t oy = y;

	RECT src = {cframe->src[0], cframe->src[1], cframe->src[2], cframe->src[3]};
	RECT_FIXED dst = {ox, oy, FIXED_DEC(348,1), FIXED_DEC(196,1)};
	Stage_DrawTex(&this->tex_stat_act4, &src, &dst, stage.camera.bzoom, stage.camera.angle);
}

//Strike_Act4 animation and rects
static const CharFrame strike_act4_frame[] = {
	{0, {  0,  0,  184,  216}, { 2,  0}},
	{1, {  0,  0,  184,  216}, { 1,  0}},
	{2, {  0,  0,  180,  216}, { -4,  0}},
};

static const Animation strike_act4_anim[] = {
	{2, (const u8[]) {0, 1, 2, ASCR_LOOP}},
};

void Stars_Act4_Strike_SetFrame(void *user, u8 frame)
{
	Back_Stars_Act4 *this = (Back_Stars_Act4*)user;
	
	//Check if this is a new frame
	if (frame != this->strike_act4_frame)
	{
		//Check if new art shall be loaded
		const CharFrame *cframe = &strike_act4_frame[this->strike_act4_frame = frame];
		if (cframe->tex != this->strike_act4_tex_id)
			Gfx_LoadTex(&this->tex_strike_act4, this->arc_strike_act4_ptr[this->strike_act4_tex_id = cframe->tex], 0);
	}
}

void Stars_Act4_Strike_Draw(Back_Stars_Act4 *this, fixed_t x, fixed_t y)
{
	//Draw character
	const CharFrame *cframe = &strike_act4_frame[this->strike_act4_frame];
	
	fixed_t ox = x - ((fixed_t)cframe->off[0] << FIXED_SHIFT);
	fixed_t oy = y - ((fixed_t)cframe->off[1] << FIXED_SHIFT);

	RECT src = {cframe->src[0], cframe->src[1], cframe->src[2], cframe->src[3]};
	RECT_FIXED dst = {ox, oy, FIXED_DEC(193,1), FIXED_DEC(215,1)};
	Stage_DrawTex(&this->tex_strike_act4, &src, &dst, stage.camera.bzoom, stage.camera.angle);
}

void Back_Stars_Act4_DrawBG(StageBack *back)
{
	Back_Stars_Act4 *this = (Back_Stars_Act4*)back;
	
	fixed_t fx, fy;
	
	RECT act4_go_src = {0, 0, 88, 9};
	fx = stage.camera.x;
	fy = stage.camera.y;
	RECT_FIXED act4_go_dst = {
		FIXED_DEC(-44,1) - fx,
		FIXED_DEC(-4.5,1) - fy,
		FIXED_DEC(88,1),
		FIXED_DEC(14,1)
	};
	Stage_BlendTexV2(&this->tex_act4_go, &act4_go_src, &act4_go_dst, stage.camera.bzoom, 0, 0);

	RECT act4_pipe_src = {0, 0, 60, 59};
	RECT_FIXED act4_pipe_dst = {
		FIXED_DEC(89,1) - fx,
		FIXED_DEC(62,1) - fy,
		FIXED_DEC(56,1),
		FIXED_DEC(58,1)
	};
	Stage_DrawTex(&this->tex_act4_pipe, &act4_pipe_src, &act4_pipe_dst, stage.camera.bzoom, stage.camera.angle);

	RECT act4_mem_src = {0, 0, 188, 142};
	fx = stage.camera.x * 11 / 10;
	fy = stage.camera.y * 11 / 10;
	RECT_FIXED act4_mem_dst = {
		FIXED_DEC(-4,1) - fx,
		FIXED_DEC(28,1) - fy,
		FIXED_DEC(253,1),
		FIXED_DEC(198,1)
	};
	Stage_BlendTexV2(&this->tex_act4_mem, &act4_mem_src, &act4_mem_dst, stage.camera.bzoom, 0, 0);

	RECT act4_exe_src = {0, 0, 188, 142};
	RECT_FIXED act4_exe_dst = {
		FIXED_DEC(128,1) - fx,
		FIXED_DEC(-2,1) - fy,
		FIXED_DEC(253,1),
		FIXED_DEC(198,1)
	};
	Stage_BlendTexV2(&this->tex_act4_exe, &act4_exe_src, &act4_exe_dst, stage.camera.bzoom, 0, 0);

	RECT act4_light_src = {0, 0, 164, 91};
	fx = stage.camera.x;
	fy = stage.camera.y;
	RECT_FIXED act4_light_dst = {
		FIXED_DEC(40,1) - fx,
		FIXED_DEC(3,1) - fy,
		FIXED_DEC(640,1),
		FIXED_DEC(360,1)
	};
	Stage_BlendTexV2(&this->tex_act4_light, &act4_light_src, &act4_light_dst, stage.camera.bzoom, 0, 64);

	Animatable_Animate(&this->dead_act4_animatable, (void*)this, Stars_Act4_Dead_SetFrame);
	
	Stars_Act4_Dead_Draw(this, FIXED_DEC(34,1) - fx, FIXED_DEC(-8.5,1) - fy);

	Animatable_Animate(&this->obj_act4_animatable, (void*)this, Stars_Act4_Obj_SetFrame);
	
	Stars_Act4_Obj_Draw(this, FIXED_DEC(0,1) - fx, FIXED_DEC(0,1) - fy);

	Animatable_Animate(&this->rip_act4_animatable, (void*)this, Stars_Act4_Rip_SetFrame);
	
	fx = stage.camera.x >> 1;
	fy = stage.camera.y >> 1;
	Stars_Act4_Rip_Draw(this, FIXED_DEC(-18,1) - fx, FIXED_DEC(2.5,1) - fy);

	Animatable_Animate(&this->stat_act4_animatable, (void*)this, Stars_Act4_Stat_SetFrame);
	
	fx = stage.camera.x * 3 / 10;
	fy = stage.camera.y * 3 / 10;
	Stars_Act4_Stat_Draw(this, FIXED_DEC(-7.5,1) - fx, FIXED_DEC(-30,1) - fy);

	Animatable_Animate(&this->strike_act4_animatable, (void*)this, Stars_Act4_Strike_SetFrame);
	
	fx = stage.camera.x;
	fy = stage.camera.y;
	Stars_Act4_Strike_Draw(this, FIXED_DEC(55,1) - fx, FIXED_DEC(4,1) - fy);

}

void Back_Stars_Act4_Free(StageBack *back)
{
	Back_Stars_Act4 *this = (Back_Stars_Act4*)back;
	
	//Free dead_act4 archive
	Mem_Free(this->arc_dead_act4);
	
	//Free obj_act4 archive
	Mem_Free(this->arc_obj_act4);
	
	//Free rip_act4 archive
	Mem_Free(this->arc_rip_act4);
	
	//Free stat_act4 archive
	Mem_Free(this->arc_stat_act4);
	
	//Free strike_act4 archive
	Mem_Free(this->arc_strike_act4);
	
	//Free structure
	Mem_Free(this);
}

StageBack *Back_Stars_Act4_New(void)
{
	//Allocate background structure
	Back_Stars_Act4 *this = (Back_Stars_Act4*)Mem_Alloc(sizeof(Back_Stars_Act4));
	if (this == NULL)
		return NULL;
	
	//Set background functions
	this->back.draw_bg = Back_Stars_Act4_DrawBG;
	this->back.draw_md = NULL;
	this->back.draw_fg = NULL;
	this->back.draw_hud = NULL;
	this->back.free = Back_Stars_Act4_Free;
	
	//Load background textures
	IO_Data arc_back = IO_Read("\\STARS\\ACT4.ARC;1");
	Gfx_LoadTex(&this->tex_act4_go, Archive_Find(arc_back, "go.tim"), 0);
	Gfx_LoadTex(&this->tex_act4_pipe, Archive_Find(arc_back, "pipe.tim"), 0);
	Gfx_LoadTex(&this->tex_act4_mem, Archive_Find(arc_back, "mem.tim"), 0);
	Gfx_LoadTex(&this->tex_act4_exe, Archive_Find(arc_back, "exe.tim"), 0);
	Gfx_LoadTex(&this->tex_act4_light, Archive_Find(arc_back, "light.tim"), 0);
	Mem_Free(arc_back);
	
	//Load dead_act4 textures
	this->arc_dead_act4 = IO_Read("\\STARS\\ACT4.ARC;1");
	this->arc_dead_act4_ptr[0] = Archive_Find(this->arc_dead_act4, "dead1.tim");
	this->arc_dead_act4_ptr[1] = Archive_Find(this->arc_dead_act4, "dead9.tim");
	this->arc_dead_act4_ptr[2] = Archive_Find(this->arc_dead_act4, "dead0.tim");
	this->arc_dead_act4_ptr[3] = Archive_Find(this->arc_dead_act4, "dead8.tim");
	this->arc_dead_act4_ptr[4] = Archive_Find(this->arc_dead_act4, "dead6.tim");
	this->arc_dead_act4_ptr[5] = Archive_Find(this->arc_dead_act4, "dead7.tim");
	this->arc_dead_act4_ptr[6] = Archive_Find(this->arc_dead_act4, "dead3.tim");
	this->arc_dead_act4_ptr[7] = Archive_Find(this->arc_dead_act4, "dead4.tim");
	this->arc_dead_act4_ptr[8] = Archive_Find(this->arc_dead_act4, "dead5.tim");
	this->arc_dead_act4_ptr[9] = Archive_Find(this->arc_dead_act4, "dead2.tim");
	
	Animatable_Init(&this->dead_act4_animatable, dead_act4_anim);
	Animatable_SetAnim(&this->dead_act4_animatable, 0);
	this->dead_act4_frame = this->dead_act4_tex_id = 0xFF; //Force art load
	
	//Load obj_act4 textures
	this->arc_obj_act4 = IO_Read("\\STARS\\ACT4.ARC;1");
	this->arc_obj_act4_ptr[0] = Archive_Find(this->arc_obj_act4, "obj.tim");
	
	Animatable_Init(&this->obj_act4_animatable, obj_act4_anim);
	Animatable_SetAnim(&this->obj_act4_animatable, 0);
	this->obj_act4_frame = this->obj_act4_tex_id = 0xFF; //Force art load
	
	//Load rip_act4 textures
	this->arc_rip_act4 = IO_Read("\\STARS\\ACT4.ARC;1");
	this->arc_rip_act4_ptr[0] = Archive_Find(this->arc_rip_act4, "rip1.tim");
	this->arc_rip_act4_ptr[1] = Archive_Find(this->arc_rip_act4, "rip0.tim");
	this->arc_rip_act4_ptr[2] = Archive_Find(this->arc_rip_act4, "rip2.tim");
	
	Animatable_Init(&this->rip_act4_animatable, rip_act4_anim);
	Animatable_SetAnim(&this->rip_act4_animatable, 0);
	this->rip_act4_frame = this->rip_act4_tex_id = 0xFF; //Force art load
	
	//Load stat_act4 textures
	this->arc_stat_act4 = IO_Read("\\STARS\\ACT4.ARC;1");
	this->arc_stat_act4_ptr[0] = Archive_Find(this->arc_stat_act4, "stat0.tim");
	this->arc_stat_act4_ptr[1] = Archive_Find(this->arc_stat_act4, "stat1.tim");
	
	Animatable_Init(&this->stat_act4_animatable, stat_act4_anim);
	Animatable_SetAnim(&this->stat_act4_animatable, 0);
	this->stat_act4_frame = this->stat_act4_tex_id = 0xFF; //Force art load
	
	//Load strike_act4 textures
	this->arc_strike_act4 = IO_Read("\\STARS\\ACT4.ARC;1");
	this->arc_strike_act4_ptr[0] = Archive_Find(this->arc_strike_act4, "strike0.tim");
	this->arc_strike_act4_ptr[1] = Archive_Find(this->arc_strike_act4, "strike1.tim");
	this->arc_strike_act4_ptr[2] = Archive_Find(this->arc_strike_act4, "strike2.tim");
	
	Animatable_Init(&this->strike_act4_animatable, strike_act4_anim);
	Animatable_SetAnim(&this->strike_act4_animatable, 0);
	this->strike_act4_frame = this->strike_act4_tex_id = 0xFF; //Force art load
	
	return (StageBack*)this;
}
