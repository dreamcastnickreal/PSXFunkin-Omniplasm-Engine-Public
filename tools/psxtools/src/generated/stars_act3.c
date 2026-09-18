/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "stars_act3.h"

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

//Back_Stars_Act3 background structure
typedef struct
{
	//Stage background base structure
	StageBack back;
	
	IO_Data arc_body_act3, arc_body_act3_ptr[11];
	
	IO_Data arc_eyes_act3, arc_eyes_act3_ptr[1];
	
	IO_Data arc_headp_act3, arc_headp_act3_ptr[25];
	
	IO_Data arc_hills_act3, arc_hills_act3_ptr[1];
	
	IO_Data arc_stat_act3, arc_stat_act3_ptr[2];
	
	//Textures
	Gfx_Tex tex_act3_pipe; //pipe.tim
	Gfx_Tex tex_act3_arm; //arm.tim
	Gfx_Tex tex_act3_spot; //spot.tim
	Gfx_Tex tex_act3_p3; //p3.tim
	Gfx_Tex tex_body_act3; //animated body_act3
	Gfx_Tex tex_eyes_act3; //animated eyes_act3
	Gfx_Tex tex_headp_act3; //animated headp_act3
	Gfx_Tex tex_hills_act3; //animated hills_act3
	Gfx_Tex tex_stat_act3; //animated stat_act3
	
	//Body_Act3 state
	u8 body_act3_frame, body_act3_tex_id;
	Animatable body_act3_animatable;
	
	//Eyes_Act3 state
	u8 eyes_act3_frame, eyes_act3_tex_id;
	Animatable eyes_act3_animatable;
	
	//Headp_Act3 state
	u8 headp_act3_frame, headp_act3_tex_id;
	Animatable headp_act3_animatable;
	
	//Hills_Act3 state
	u8 hills_act3_frame, hills_act3_tex_id;
	Animatable hills_act3_animatable;
	
	//Stat_Act3 state
	u8 stat_act3_frame, stat_act3_tex_id;
	Animatable stat_act3_animatable;
} Back_Stars_Act3;

//Body_Act3 animation and rects
static const CharFrame body_act3_frame[] = {
	{0, {  0,  0,  156,  119}, { -84,  -4}},
	{1, {  0,  0,  160,  118}, { -84,  -4}},
	{2, {  0,  0,  160,  117}, { -84,  -5}},
	{0, {  0,  119,  160,  116}, { -84,  -5}},
	{3, {  0,  0,  160,  116}, { -84,  -5}},
	{2, {  0,  117,  160,  117}, { -85,  -3}},
	{1, {  0,  118,  160,  118}, { -85,  -3}},
	{3, {  0,  116,  156,  118}, { -85,  -3}},
	{4, {  0,  0,  156,  118}, { -85,  -3}},
	{5, {  0,  117,  196,  117}, { -11,  -3}},
	{5, {  0,  0,  216,  117}, { 0,  -2}},
	{6, {  0,  0,  192,  118}, { 0,  0}},
	{6, {  0,  118,  188,  118}, { -4,  0}},
	{7, {  0,  0,  188,  118}, { -5,  0}},
	{8, {  0,  0,  184,  117}, { -8,  -3}},
	{9, {  0,  0,  188,  115}, { -9,  -6}},
	{8, {  0,  117,  188,  114}, { -9,  -6}},
	{9, {  0,  115,  188,  115}, { -8,  -5}},
	{10, {  0,  0,  188,  117}, { -8,  -2}},
	{10, {  0,  117,  188,  117}, { -7,  -1}},
	{7, {  0,  118,  188,  118}, { -5,  0}},
};

static const Animation body_act3_anim[] = {
	{2, (const u8[]) {0, 1, 2, 3, 4, 5, 6, 7, 14, 15, 16, 17, 18, 19, 20, ASCR_HOLD}},
	{2, (const u8[]) {8, 9, 10, 11, 12, 13, ASCR_HOLD}},
};

void Stars_Act3_Body_SetFrame(void *user, u8 frame)
{
	Back_Stars_Act3 *this = (Back_Stars_Act3*)user;
	
	//Check if this is a new frame
	if (frame != this->body_act3_frame)
	{
		//Check if new art shall be loaded
		const CharFrame *cframe = &body_act3_frame[this->body_act3_frame = frame];
		if (cframe->tex != this->body_act3_tex_id)
			Gfx_LoadTex(&this->tex_body_act3, this->arc_body_act3_ptr[this->body_act3_tex_id = cframe->tex], 0);
	}
}

void Stars_Act3_Body_Draw(Back_Stars_Act3 *this, fixed_t x, fixed_t y)
{
	//Draw character
	const CharFrame *cframe = &body_act3_frame[this->body_act3_frame];
	
	fixed_t ox = x - ((fixed_t)cframe->off[0] << FIXED_SHIFT);
	fixed_t oy = y - ((fixed_t)cframe->off[1] << FIXED_SHIFT);

	RECT src = {cframe->src[0], cframe->src[1], cframe->src[2], cframe->src[3]};
	RECT_FIXED dst = {ox, oy, FIXED_DEC(766,1), FIXED_DEC(421,1)};
	Stage_DrawTex(&this->tex_body_act3, &src, &dst, stage.camera.bzoom, stage.camera.angle);
}

//Eyes_Act3 animation and rects
static const CharFrame eyes_act3_frame[] = {
	{0, {  0,  0,  28,  11}, { 0,  0}},
	{0, {  84,  0,  28,  9}, { -1,  0}},
	{0, {  28,  0,  28,  10}, { -1,  0}},
	{0, {  56,  0,  28,  10}, { 0,  -1}},
};

static const Animation eyes_act3_anim[] = {
	{2, (const u8[]) {0, 1, 2, 3, ASCR_LOOP}},
};

void Stars_Act3_Eyes_SetFrame(void *user, u8 frame)
{
	Back_Stars_Act3 *this = (Back_Stars_Act3*)user;
	
	//Check if this is a new frame
	if (frame != this->eyes_act3_frame)
	{
		//Check if new art shall be loaded
		const CharFrame *cframe = &eyes_act3_frame[this->eyes_act3_frame = frame];
		if (cframe->tex != this->eyes_act3_tex_id)
			Gfx_LoadTex(&this->tex_eyes_act3, this->arc_eyes_act3_ptr[this->eyes_act3_tex_id = cframe->tex], 0);
	}
}

void Stars_Act3_Eyes_Draw(Back_Stars_Act3 *this, fixed_t x, fixed_t y)
{
	//Draw character
	const CharFrame *cframe = &eyes_act3_frame[this->eyes_act3_frame];
	
	fixed_t ox = x - ((fixed_t)cframe->off[0] << FIXED_SHIFT);
	fixed_t oy = y - ((fixed_t)cframe->off[1] << FIXED_SHIFT);

	RECT src = {cframe->src[0], cframe->src[1], cframe->src[2], cframe->src[3]};
	RECT_FIXED dst = {ox, oy, FIXED_DEC(65,1), FIXED_DEC(24,1)};
	Stage_DrawTex(&this->tex_eyes_act3, &src, &dst, stage.camera.bzoom, stage.camera.angle);
}

//Headp_Act3 animation and rects
static const CharFrame headp_act3_frame[] = {
	{0, {  0,  0,  84,  122}, { 0,  -4}},
	{1, {  0,  0,  84,  111}, { 0,  -4}},
	{0, {  168,  0,  84,  117}, { 0,  -4}},
	{0, {  0,  122,  84,  122}, { 0,  -4}},
	{0, {  84,  0,  84,  121}, { 0,  -4}},
	{2, {  0,  122,  84,  116}, { 0,  -4}},
	{3, {  84,  138,  88,  117}, { -1,  -2}},
	{4, {  88,  119,  88,  117}, { 0,  -2}},
	{4, {  0,  0,  88,  119}, { 0,  -2}},
	{5, {  0,  0,  88,  134}, { 0,  0}},
	{3, {  0,  0,  84,  146}, { -1,  -2}},
	{6, {  88,  0,  88,  141}, { 0,  -4}},
	{7, {  0,  111,  84,  105}, { 0,  -4}},
	{8, {  0,  0,  84,  106}, { 0,  -4}},
	{8, {  0,  106,  84,  106}, { 0,  -4}},
	{8, {  84,  0,  84,  106}, { 0,  -4}},
	{9, {  0,  110,  84,  109}, { 0,  -4}},
	{1, {  84,  0,  84,  110}, { 0,  -4}},
	{7, {  80,  0,  84,  105}, { 0,  -4}},
	{8, {  168,  0,  84,  106}, { 0,  -4}},
	{9, {  84,  0,  84,  109}, { 0,  -4}},
	{1, {  160,  110,  84,  110}, { 0,  -4}},
	{9, {  168,  0,  84,  109}, { 0,  -4}},
	{7, {  164,  0,  84,  105}, { 0,  -4}},
	{1, {  168,  0,  84,  110}, { 0,  -4}},
	{9, {  168,  109,  84,  108}, { 0,  -4}},
	{3, {  84,  0,  88,  138}, { 0,  -6}},
	{6, {  0,  0,  88,  144}, { 0,  -3}},
	{10, {  88,  130,  88,  122}, { 0,  0}},
	{11, {  88,  0,  88,  128}, { 0,  0}},
	{12, {  0,  0,  88,  135}, { 0,  0}},
	{3, {  172,  0,  84,  133}, { -1,  -1}},
	{11, {  0,  0,  88,  129}, { 0,  0}},
	{13, {  0,  117,  88,  116}, { 0,  0}},
	{14, {  0,  0,  88,  117}, { 0,  -2}},
	{11, {  88,  128,  88,  128}, { 0,  0}},
	{5, {  88,  0,  88,  134}, { 0,  0}},
	{12, {  88,  0,  88,  135}, { 0,  0}},
	{10, {  0,  0,  88,  130}, { 0,  0}},
	{11, {  0,  129,  88,  123}, { 0,  0}},
	{14, {  0,  117,  88,  117}, { 0,  0}},
	{14, {  88,  0,  88,  117}, { 0,  0}},
	{15, {  88,  122,  88,  121}, { 0,  -4}},
	{5, {  0,  134,  88,  122}, { 0,  0}},
	{13, {  88,  0,  88,  116}, { 0,  0}},
	{14, {  88,  117,  88,  117}, { 0,  0}},
	{16, {  0,  121,  88,  120}, { 0,  0}},
	{5, {  88,  134,  88,  122}, { 0,  0}},
	{9, {  84,  109,  84,  109}, { 0,  -4}},
	{9, {  0,  0,  84,  110}, { 0,  -4}},
	{17, {  80,  110,  84,  104}, { 0,  -4}},
	{18, {  0,  108,  84,  102}, { 0,  -6}},
	{8, {  84,  106,  84,  106}, { 0,  -4}},
	{8, {  168,  106,  80,  111}, { 0,  -4}},
	{2, {  80,  0,  80,  121}, { 0,  -4}},
	{0, {  168,  117,  80,  122}, { 0,  -4}},
	{2, {  84,  121,  80,  121}, { 0,  -4}},
	{1, {  0,  111,  80,  116}, { 0,  -4}},
	{19, {  80,  0,  80,  105}, { -1,  -3}},
	{17, {  164,  110,  80,  109}, { 0,  -4}},
	{7, {  84,  105,  80,  110}, { 0,  -4}},
	{19, {  160,  0,  80,  105}, { -1,  -3}},
	{20, {  80,  109,  80,  108}, { 0,  -4}},
	{7, {  164,  105,  80,  110}, { 0,  -4}},
	{20, {  160,  109,  80,  108}, { 0,  -4}},
	{19, {  80,  105,  80,  105}, { 0,  -4}},
	{18, {  80,  0,  80,  106}, { 0,  -4}},
	{2, {  160,  0,  80,  117}, { 0,  -4}},
	{19, {  160,  105,  80,  105}, { 0,  -4}},
	{18, {  160,  0,  80,  106}, { 0,  -4}},
	{21, {  0,  0,  80,  105}, { 0,  -4}},
	{18, {  84,  106,  80,  106}, { 0,  -4}},
	{20, {  0,  0,  80,  109}, { 0,  -4}},
	{17, {  0,  0,  80,  110}, { -2,  -2}},
	{18, {  164,  106,  80,  106}, { 0,  -4}},
	{20, {  0,  109,  80,  109}, { 0,  -4}},
	{17, {  0,  110,  80,  110}, { -2,  -2}},
	{17, {  80,  0,  80,  110}, { -2,  -2}},
	{19, {  0,  0,  80,  106}, { 0,  -4}},
	{2, {  164,  117,  80,  117}, { 0,  -4}},
	{0, {  84,  121,  80,  122}, { 0,  -4}},
	{2, {  0,  0,  80,  122}, { 0,  -4}},
	{1, {  80,  111,  80,  116}, { 0,  -4}},
	{21, {  0,  105,  80,  105}, { 0,  -4}},
	{19, {  0,  106,  80,  106}, { 0,  -4}},
	{18, {  0,  0,  80,  108}, { 0,  -4}},
	{21, {  80,  0,  80,  105}, { 0,  -4}},
	{20, {  80,  0,  80,  109}, { 0,  -4}},
	{7, {  0,  0,  80,  111}, { -2,  -2}},
	{17, {  160,  0,  80,  110}, { -2,  -2}},
	{21, {  160,  0,  80,  105}, { 0,  -4}},
	{20, {  160,  0,  80,  109}, { 0,  -4}},
	{16, {  88,  0,  88,  120}, { 0,  0}},
	{13, {  88,  116,  88,  116}, { 0,  0}},
	{22, {  0,  128,  88,  122}, { 0,  0}},
	{4, {  0,  119,  88,  119}, { 0,  0}},
	{12, {  0,  135,  88,  121}, { 0,  0}},
	{22, {  88,  0,  88,  122}, { 0,  0}},
	{4, {  88,  0,  88,  119}, { 0,  0}},
	{23, {  0,  0,  88,  117}, { 0,  0}},
	{12, {  88,  135,  88,  121}, { 0,  0}},
	{22, {  88,  122,  88,  122}, { 0,  0}},
	{23, {  0,  117,  88,  117}, { 0,  0}},
	{24, {  0,  135,  88,  121}, { 0,  0}},
	{15, {  0,  0,  88,  122}, { 0,  0}},
	{15, {  0,  122,  88,  122}, { 0,  0}},
	{23, {  88,  0,  88,  117}, { 0,  0}},
	{10, {  88,  0,  88,  130}, { 0,  0}},
	{24, {  0,  0,  88,  135}, { 0,  0}},
	{24, {  88,  0,  88,  135}, { 0,  0}},
	{22, {  0,  0,  88,  128}, { 0,  0}},
	{16, {  88,  120,  88,  120}, { 0,  0}},
	{23, {  88,  117,  88,  117}, { 0,  0}},
	{24, {  88,  135,  88,  121}, { 0,  0}},
	{10, {  0,  130,  88,  123}, { 0,  0}},
	{15, {  88,  0,  88,  122}, { 0,  0}},
	{13, {  0,  0,  88,  117}, { 0,  0}},
	{16, {  0,  0,  88,  121}, { 0,  0}},
};

static const Animation headp_act3_anim[] = {
	{2, (const u8[]) {0, ASCR_LOOP}},
	{2, (const u8[]) {1, 2, 3, 4, 5, 9, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 28, 29, 30, 31, 32, 33, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, ASCR_HOLD}},
	{2, (const u8[]) {6, 7, 8, 10, 11, 26, 27, 34, ASCR_LOOP}},
};

void Stars_Act3_Headp_SetFrame(void *user, u8 frame)
{
	Back_Stars_Act3 *this = (Back_Stars_Act3*)user;
	
	//Check if this is a new frame
	if (frame != this->headp_act3_frame)
	{
		//Check if new art shall be loaded
		const CharFrame *cframe = &headp_act3_frame[this->headp_act3_frame = frame];
		if (cframe->tex != this->headp_act3_tex_id)
			Gfx_LoadTex(&this->tex_headp_act3, this->arc_headp_act3_ptr[this->headp_act3_tex_id = cframe->tex], 0);
	}
}

void Stars_Act3_Headp_Draw(Back_Stars_Act3 *this, fixed_t x, fixed_t y)
{
	//Draw character
	const CharFrame *cframe = &headp_act3_frame[this->headp_act3_frame];
	
	fixed_t ox = x - ((fixed_t)cframe->off[0] << FIXED_SHIFT);
	fixed_t oy = y - ((fixed_t)cframe->off[1] << FIXED_SHIFT);

	RECT src = {cframe->src[0], cframe->src[1], cframe->src[2], cframe->src[3]};
	RECT_FIXED dst = {ox, oy, FIXED_DEC(225,1), FIXED_DEC(341,1)};
	Stage_DrawTex(&this->tex_headp_act3, &src, &dst, stage.camera.bzoom, stage.camera.angle);
}

//Hills_Act3 animation and rects
static const CharFrame hills_act3_frame[] = {
	{0, {  0,  0,  172,  43}, { 0,  0}},
	{0, {  0,  86,  168,  43}, { -1,  0}},
	{0, {  0,  43,  172,  43}, { 0,  0}},
};

static const Animation hills_act3_anim[] = {
	{2, (const u8[]) {0, 1, 2, ASCR_LOOP}},
};

void Stars_Act3_Hills_SetFrame(void *user, u8 frame)
{
	Back_Stars_Act3 *this = (Back_Stars_Act3*)user;
	
	//Check if this is a new frame
	if (frame != this->hills_act3_frame)
	{
		//Check if new art shall be loaded
		const CharFrame *cframe = &hills_act3_frame[this->hills_act3_frame = frame];
		if (cframe->tex != this->hills_act3_tex_id)
			Gfx_LoadTex(&this->tex_hills_act3, this->arc_hills_act3_ptr[this->hills_act3_tex_id = cframe->tex], 0);
	}
}

void Stars_Act3_Hills_Draw(Back_Stars_Act3 *this, fixed_t x, fixed_t y)
{
	//Draw character
	const CharFrame *cframe = &hills_act3_frame[this->hills_act3_frame];
	
	fixed_t ox = x - ((fixed_t)cframe->off[0] << FIXED_SHIFT);
	fixed_t oy = y - ((fixed_t)cframe->off[1] << FIXED_SHIFT);

	RECT src = {cframe->src[0], cframe->src[1], cframe->src[2], cframe->src[3]};
	RECT_FIXED dst = {ox, oy, FIXED_DEC(867,1), FIXED_DEC(219,1)};
	Stage_DrawTex(&this->tex_hills_act3, &src, &dst, stage.camera.bzoom, stage.camera.angle);
}

//Stat_Act3 animation and rects
static const CharFrame stat_act3_frame[] = {
	{0, {  0,  0,  184,  92}, { 0,  0}},
	{0, {  0,  92,  184,  92}, { 0,  0}},
	{1, {  0,  0,  184,  92}, { 0,  0}},
	{1, {  0,  92,  184,  92}, { 0,  0}},
};

static const Animation stat_act3_anim[] = {
	{2, (const u8[]) {0, 1, 2, 3, ASCR_LOOP}},
};

void Stars_Act3_Stat_SetFrame(void *user, u8 frame)
{
	Back_Stars_Act3 *this = (Back_Stars_Act3*)user;
	
	//Check if this is a new frame
	if (frame != this->stat_act3_frame)
	{
		//Check if new art shall be loaded
		const CharFrame *cframe = &stat_act3_frame[this->stat_act3_frame = frame];
		if (cframe->tex != this->stat_act3_tex_id)
			Gfx_LoadTex(&this->tex_stat_act3, this->arc_stat_act3_ptr[this->stat_act3_tex_id = cframe->tex], 0);
	}
}

void Stars_Act3_Stat_Draw(Back_Stars_Act3 *this, fixed_t x, fixed_t y)
{
	//Draw character
	const CharFrame *cframe = &stat_act3_frame[this->stat_act3_frame];
	
	fixed_t ox = x;
	fixed_t oy = y;

	RECT src = {cframe->src[0], cframe->src[1], cframe->src[2], cframe->src[3]};
	RECT_FIXED dst = {ox, oy, FIXED_DEC(932,1), FIXED_DEC(472,1)};
	Stage_DrawTex(&this->tex_stat_act3, &src, &dst, stage.camera.bzoom, stage.camera.angle);
}

void Back_Stars_Act3_DrawBG(StageBack *back)
{
	Back_Stars_Act3 *this = (Back_Stars_Act3*)back;
	
	fixed_t fx, fy;
	
	RECT act3_pipe_src = {0, 0, 132, 107};
	fx = stage.camera.x;
	fy = stage.camera.y;
	RECT_FIXED act3_pipe_dst = {
		FIXED_DEC(11.5,1) - fx,
		FIXED_DEC(60.5,1) - fy,
		FIXED_DEC(258,1),
		FIXED_DEC(212,1)
	};
	Stage_DrawTex(&this->tex_act3_pipe, &act3_pipe_src, &act3_pipe_dst, stage.camera.bzoom, stage.camera.angle);

	RECT act3_arm_src = {0, 0, 104, 166};
	fx = stage.camera.x * 4 / 5;
	fy = stage.camera.y * 4 / 5;
	RECT_FIXED act3_arm_dst = {
		FIXED_DEC(-117.5,1) - fx,
		FIXED_DEC(-77.5,1) - fy,
		FIXED_DEC(251,1),
		FIXED_DEC(412,1)
	};
	Stage_DrawTex(&this->tex_act3_arm, &act3_arm_src, &act3_arm_dst, stage.camera.bzoom, stage.camera.angle);

	RECT act3_spot_src = {0, 0, 176, 122};
	fx = stage.camera.x;
	fy = stage.camera.y;
	RECT_FIXED act3_spot_dst = {
		FIXED_DEC(-155,1) - fx,
		FIXED_DEC(-30,1) - fy,
		FIXED_DEC(904,1),
		FIXED_DEC(480,1)
	};
	Stage_BlendTexV2(&this->tex_act3_spot, &act3_spot_src, &act3_spot_dst, stage.camera.bzoom, 0, 178);

	Animatable_Animate(&this->body_act3_animatable, (void*)this, Stars_Act3_Body_SetFrame);
	
	fx = stage.camera.x * 4 / 5;
	fy = stage.camera.y * 4 / 5;
	Stars_Act3_Body_Draw(this, FIXED_DEC(-118.5,1) - fx, FIXED_DEC(-5,1) - fy);

	Animatable_Animate(&this->eyes_act3_animatable, (void*)this, Stars_Act3_Eyes_SetFrame);
	
	Stars_Act3_Eyes_Draw(this, FIXED_DEC(-17.5,1) - fx, FIXED_DEC(10.5,1) - fy);

	Animatable_Animate(&this->headp_act3_animatable, (void*)this, Stars_Act3_Headp_SetFrame);
	
	Stars_Act3_Headp_Draw(this, FIXED_DEC(-47.5,1) - fx, FIXED_DEC(-21,1) - fy);

	Animatable_Animate(&this->hills_act3_animatable, (void*)this, Stars_Act3_Hills_SetFrame);
	
	fx = stage.camera.x * 2 / 5;
	fy = stage.camera.y * 2 / 5;
	Stars_Act3_Hills_Draw(this, FIXED_DEC(-70.5,1) - fx, FIXED_DEC(45,1) - fy);

	Animatable_Animate(&this->stat_act3_animatable, (void*)this, Stars_Act3_Stat_SetFrame);
	
	fx = stage.camera.x * 1 / 5;
	fy = stage.camera.y * 1 / 5;
	Stars_Act3_Stat_Draw(this, FIXED_DEC(-100.5,1) - fx, FIXED_DEC(-28,1) - fy);

}

void Back_Stars_Act3_DrawHUD(StageBack *back)
{
	Back_Stars_Act3 *this = (Back_Stars_Act3*)back;
	
	fixed_t fx, fy;
	
	RECT act3_p3_src = {0, 0, 164, 91};
	fx = stage.camera.x;
	fy = stage.camera.y;
	RECT_FIXED act3_p3_dst = {
		FIXED_DEC(0,1) - fx,
		FIXED_DEC(0,1) - fy,
		FIXED_DEC(320,1),
		FIXED_DEC(180,1)
	};
	Stage_BlendTexV2(&this->tex_act3_p3, &act3_p3_src, &act3_p3_dst, stage.camera.bzoom, 0, 0);

}

void Back_Stars_Act3_Free(StageBack *back)
{
	Back_Stars_Act3 *this = (Back_Stars_Act3*)back;
	
	//Free body_act3 archive
	Mem_Free(this->arc_body_act3);
	
	//Free eyes_act3 archive
	Mem_Free(this->arc_eyes_act3);
	
	//Free headp_act3 archive
	Mem_Free(this->arc_headp_act3);
	
	//Free hills_act3 archive
	Mem_Free(this->arc_hills_act3);
	
	//Free stat_act3 archive
	Mem_Free(this->arc_stat_act3);
	
	//Free structure
	Mem_Free(this);
}

StageBack *Back_Stars_Act3_New(void)
{
	//Allocate background structure
	Back_Stars_Act3 *this = (Back_Stars_Act3*)Mem_Alloc(sizeof(Back_Stars_Act3));
	if (this == NULL)
		return NULL;
	
	//Set background functions
	this->back.draw_bg = Back_Stars_Act3_DrawBG;
	this->back.draw_md = NULL;
	this->back.draw_fg = NULL;
	this->back.draw_hud = Back_Stars_Act3_DrawHUD;
	this->back.free = Back_Stars_Act3_Free;
	
	//Load background textures
	IO_Data arc_back = IO_Read("\\STARS\\ACT3.ARC;1");
	Gfx_LoadTex(&this->tex_act3_pipe, Archive_Find(arc_back, "pipe.tim"), 0);
	Gfx_LoadTex(&this->tex_act3_arm, Archive_Find(arc_back, "arm.tim"), 0);
	Gfx_LoadTex(&this->tex_act3_spot, Archive_Find(arc_back, "spot.tim"), 0);
	Gfx_LoadTex(&this->tex_act3_p3, Archive_Find(arc_back, "p3.tim"), 0);
	Mem_Free(arc_back);
	
	//Load body_act3 textures
	this->arc_body_act3 = IO_Read("\\STARS\\ACT3.ARC;1");
	this->arc_body_act3_ptr[0] = Archive_Find(this->arc_body_act3, "body8.tim");
	this->arc_body_act3_ptr[1] = Archive_Find(this->arc_body_act3, "body6.tim");
	this->arc_body_act3_ptr[2] = Archive_Find(this->arc_body_act3, "body7.tim");
	this->arc_body_act3_ptr[3] = Archive_Find(this->arc_body_act3, "body9.tim");
	this->arc_body_act3_ptr[4] = Archive_Find(this->arc_body_act3, "body10.tim");
	this->arc_body_act3_ptr[5] = Archive_Find(this->arc_body_act3, "body0.tim");
	this->arc_body_act3_ptr[6] = Archive_Find(this->arc_body_act3, "body1.tim");
	this->arc_body_act3_ptr[7] = Archive_Find(this->arc_body_act3, "body2.tim");
	this->arc_body_act3_ptr[8] = Archive_Find(this->arc_body_act3, "body5.tim");
	this->arc_body_act3_ptr[9] = Archive_Find(this->arc_body_act3, "body4.tim");
	this->arc_body_act3_ptr[10] = Archive_Find(this->arc_body_act3, "body3.tim");
	
	Animatable_Init(&this->body_act3_animatable, body_act3_anim);
	Animatable_SetAnim(&this->body_act3_animatable, 0);
	this->body_act3_frame = this->body_act3_tex_id = 0xFF; //Force art load
	
	//Load eyes_act3 textures
	this->arc_eyes_act3 = IO_Read("\\STARS\\ACT3.ARC;1");
	this->arc_eyes_act3_ptr[0] = Archive_Find(this->arc_eyes_act3, "eyes.tim");
	
	Animatable_Init(&this->eyes_act3_animatable, eyes_act3_anim);
	Animatable_SetAnim(&this->eyes_act3_animatable, 0);
	this->eyes_act3_frame = this->eyes_act3_tex_id = 0xFF; //Force art load
	
	//Load headp_act3 textures
	this->arc_headp_act3 = IO_Read("\\STARS\\ACT3.ARC;1");
	this->arc_headp_act3_ptr[0] = Archive_Find(this->arc_headp_act3, "headp10.tim");
	this->arc_headp_act3_ptr[1] = Archive_Find(this->arc_headp_act3, "headp12.tim");
	this->arc_headp_act3_ptr[2] = Archive_Find(this->arc_headp_act3, "headp11.tim");
	this->arc_headp_act3_ptr[3] = Archive_Find(this->arc_headp_act3, "headp21.tim");
	this->arc_headp_act3_ptr[4] = Archive_Find(this->arc_headp_act3, "headp210.tim");
	this->arc_headp_act3_ptr[5] = Archive_Find(this->arc_headp_act3, "headp24.tim");
	this->arc_headp_act3_ptr[6] = Archive_Find(this->arc_headp_act3, "headp20.tim");
	this->arc_headp_act3_ptr[7] = Archive_Find(this->arc_headp_act3, "headp15.tim");
	this->arc_headp_act3_ptr[8] = Archive_Find(this->arc_headp_act3, "headp14.tim");
	this->arc_headp_act3_ptr[9] = Archive_Find(this->arc_headp_act3, "headp13.tim");
	this->arc_headp_act3_ptr[10] = Archive_Find(this->arc_headp_act3, "headp25.tim");
	this->arc_headp_act3_ptr[11] = Archive_Find(this->arc_headp_act3, "headp26.tim");
	this->arc_headp_act3_ptr[12] = Archive_Find(this->arc_headp_act3, "headp22.tim");
	this->arc_headp_act3_ptr[13] = Archive_Find(this->arc_headp_act3, "headp213.tim");
	this->arc_headp_act3_ptr[14] = Archive_Find(this->arc_headp_act3, "headp211.tim");
	this->arc_headp_act3_ptr[15] = Archive_Find(this->arc_headp_act3, "headp28.tim");
	this->arc_headp_act3_ptr[16] = Archive_Find(this->arc_headp_act3, "headp29.tim");
	this->arc_headp_act3_ptr[17] = Archive_Find(this->arc_headp_act3, "headp16.tim");
	this->arc_headp_act3_ptr[18] = Archive_Find(this->arc_headp_act3, "headp18.tim");
	this->arc_headp_act3_ptr[19] = Archive_Find(this->arc_headp_act3, "headp19.tim");
	this->arc_headp_act3_ptr[20] = Archive_Find(this->arc_headp_act3, "headp17.tim");
	this->arc_headp_act3_ptr[21] = Archive_Find(this->arc_headp_act3, "headp110.tim");
	this->arc_headp_act3_ptr[22] = Archive_Find(this->arc_headp_act3, "headp27.tim");
	this->arc_headp_act3_ptr[23] = Archive_Find(this->arc_headp_act3, "headp212.tim");
	this->arc_headp_act3_ptr[24] = Archive_Find(this->arc_headp_act3, "headp23.tim");
	
	Animatable_Init(&this->headp_act3_animatable, headp_act3_anim);
	Animatable_SetAnim(&this->headp_act3_animatable, 0);
	this->headp_act3_frame = this->headp_act3_tex_id = 0xFF; //Force art load
	
	//Load hills_act3 textures
	this->arc_hills_act3 = IO_Read("\\STARS\\ACT3.ARC;1");
	this->arc_hills_act3_ptr[0] = Archive_Find(this->arc_hills_act3, "hills.tim");
	
	Animatable_Init(&this->hills_act3_animatable, hills_act3_anim);
	Animatable_SetAnim(&this->hills_act3_animatable, 0);
	this->hills_act3_frame = this->hills_act3_tex_id = 0xFF; //Force art load
	
	//Load stat_act3 textures
	this->arc_stat_act3 = IO_Read("\\STARS\\ACT3.ARC;1");
	this->arc_stat_act3_ptr[0] = Archive_Find(this->arc_stat_act3, "stat0.tim");
	this->arc_stat_act3_ptr[1] = Archive_Find(this->arc_stat_act3, "stat1.tim");
	
	Animatable_Init(&this->stat_act3_animatable, stat_act3_anim);
	Animatable_SetAnim(&this->stat_act3_animatable, 0);
	this->stat_act3_frame = this->stat_act3_tex_id = 0xFF; //Force art load
	
	return (StageBack*)this;
}
