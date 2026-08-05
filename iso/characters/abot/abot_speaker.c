#include "abot_speaker.h"

#include "../io.h"
#include "../stage.h"
#include "../timer.h"
#include "../audio.h"
#include "../mem.h"
#include "../archive.h"

//ABot speaker arc indices (3 sheets)
enum {
	ABOT_SPK_abot0,
	ABOT_SPK_abot1,
	ABOT_SPK_abot2,
};

//ABot speaker section frame constants
static const u16 abot_body_src[11][4] = {
	{0, 0, 14, 22},
	{14, 0, 28, 44},
	{42, 0, 26, 30},
	{68, 0, 56, 62},
	{124, 0, 21, 5},
	{145, 0, 17, 18},
	{162, 0, 35, 39},
	{197, 0, 34, 36},
	{231, 0, 19, 13},
	{0, 62, 133, 84},
	{0, 146, 131, 84},
};
static const s16 abot_body_off[11][4] = {
	{9, -19, 14, 22},
	{2, -41, 28, 44},
	{3, -27, 26, 30},
	{-12, -59, 56, 62},
	{6, -2, 21, 5},
	{8, -17, 17, 18},
	{-1, -36, 35, 39},
	{-1, -33, 34, 36},
	{6, -10, 19, 13},
	{-50, -81, 133, 84},
	{-50, -81, 131, 84},
};
static const u8 abot_body_texid[11] = {
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
};

static const u16 abot_body_dark_src[11][4] = {
	{131, 146, 14, 22},
	{145, 146, 28, 44},
	{173, 146, 26, 30},
	{199, 146, 56, 62},
	{0, 230, 21, 5},
	{21, 230, 17, 18},
	{38, 230, 19, 13},
	{0, 0, 35, 39},
	{35, 0, 34, 36},
	{69, 0, 133, 84},
	{0, 84, 131, 84},
};
static const s16 abot_body_dark_off[11][4] = {
	{9, -19, 14, 22},
	{2, -41, 28, 44},
	{3, -27, 26, 30},
	{-12, -59, 56, 62},
	{6, -2, 21, 5},
	{8, -17, 17, 18},
	{6, -10, 19, 13},
	{-1, -36, 35, 39},
	{-1, -33, 34, 36},
	{-50, -81, 133, 84},
	{-50, -81, 131, 84},
};
static const u8 abot_body_dark_texid[11] = {
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
};

static const u16 abot_eyes_src[2][4] = {
	{57, 230, 26, 10},
	{83, 230, 20, 6},
};
static const s16 abot_eyes_off[2][4] = {
	{-10, -68, 26, 10},
	{-7, -64, 20, 6},
};
static const u8 abot_eyes_texid[2] = {
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
};

static const u16 abot_viz_src[42][4] = {
	{103, 230, 15, 24},
	{118, 230, 13, 13},
	{131, 230, 12, 7},
	{143, 230, 14, 25},
	{157, 230, 12, 14},
	{169, 230, 11, 7},
	{180, 230, 14, 25},
	{194, 230, 12, 14},
	{206, 230, 11, 6},
	{217, 230, 13, 25},
	{230, 230, 12, 14},
	{242, 230, 11, 6},
	{131, 84, 17, 49},
	{148, 84, 17, 42},
	{165, 84, 16, 34},
	{181, 84, 14, 52},
	{195, 84, 14, 46},
	{209, 84, 14, 35},
	{223, 84, 14, 54},
	{237, 84, 14, 47},
	{0, 168, 14, 38},
	{14, 168, 14, 54},
	{28, 168, 14, 48},
	{42, 168, 14, 38},
	{56, 168, 16, 54},
	{72, 168, 15, 46},
	{87, 168, 15, 38},
	{102, 168, 14, 25},
	{116, 168, 12, 15},
	{128, 168, 12, 7},
	{140, 168, 17, 52},
	{157, 168, 16, 44},
	{173, 168, 16, 36},
	{189, 168, 14, 25},
	{203, 168, 13, 15},
	{216, 168, 11, 8},
	{227, 168, 18, 48},
	{0, 222, 15, 26},
	{15, 222, 14, 16},
	{29, 222, 12, 8},
	{0, 0, 17, 41},
	{17, 0, 16, 35},
};
static const s16 abot_viz_off[42][4] = {
	{-41, -43, 15, 24},
	{-40, -32, 13, 13},
	{-38, -26, 12, 7},
	{-56, -42, 14, 25},
	{-55, -31, 12, 14},
	{-54, -24, 11, 7},
	{-70, -41, 14, 25},
	{-69, -30, 12, 14},
	{-68, -22, 11, 6},
	{-87, -41, 13, 25},
	{-87, -30, 12, 14},
	{-87, -22, 11, 6},
	{-43, -68, 17, 49},
	{-43, -61, 17, 42},
	{-43, -53, 16, 34},
	{-57, -69, 14, 52},
	{-57, -63, 14, 46},
	{-57, -52, 14, 35},
	{-71, -70, 14, 54},
	{-71, -63, 14, 47},
	{-71, -54, 14, 38},
	{-87, -70, 14, 54},
	{-87, -64, 14, 48},
	{-87, -54, 14, 38},
	{-102, -70, 16, 54},
	{-102, -62, 15, 46},
	{-102, -54, 15, 38},
	{-102, -41, 14, 25},
	{-102, -31, 12, 15},
	{-102, -23, 12, 7},
	{-115, -69, 17, 52},
	{-115, -61, 16, 44},
	{-115, -53, 16, 36},
	{-115, -42, 14, 25},
	{-115, -32, 13, 15},
	{-115, -25, 11, 8},
	{-129, -67, 18, 48},
	{-129, -45, 15, 26},
	{-129, -35, 14, 16},
	{-129, -27, 12, 8},
	{-129, -60, 17, 41},
	{-129, -54, 16, 35},
};
static const u8 abot_viz_texid[42] = {
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot0,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot1,
	ABOT_SPK_abot2,
	ABOT_SPK_abot2,
};

static const u16 abot_bg_src[1][4] = {
	{33, 0, 121, 71},
};
static const s16 abot_bg_off[1][4] = {
	{-23, -5, 121, 71},
};
static const u8 abot_bg_texid[1] = {
	ABOT_SPK_abot2,
};

static const s16 abot_eye_bg_off[1][4] = {
	{8, -54, 40, 15},
};

//ABot speaker init
void ABotSpeaker_Init(ABotSpeaker *this, boolean use_dark)
{
	//Initialize state
	this->bump = 0;
	this->current_frame = 0;
	this->eye_frame = 0;
	for (int i = 0; i < 7; i++)
	{
		this->viz_frames[i] = 5;
		this->viz_target[i] = 5;
	}
	this->eye_direction = 0;
	this->use_dark = use_dark;

	//Load speaker graphics
	this->arc_main = IO_Read("\\ARCMAIN\\ABOT.ARC;1");

	const char **pathp = (const char *[]){
		"abot0.tim", //0
		"abot1.tim", //1
		"abot2.tim", //2
		NULL
	};
	IO_Data *arc_ptr = this->arc_ptr;
	for (; *pathp != NULL; pathp++)
		*arc_ptr++ = Archive_Find(this->arc_main, *pathp);

	//Load all textures
	Gfx_LoadTex(&this->tex[0], this->arc_ptr[0], 0);
	Gfx_LoadTex(&this->tex[1], this->arc_ptr[1], 0);
	Gfx_LoadTex(&this->tex[2], this->arc_ptr[2], 0);
}

//ABot speaker tick
void ABotSpeaker_Tick(ABotSpeaker *this, fixed_t x, fixed_t y, fixed_t parallax)
{
	//Decay bump
	if (this->bump > 0)
	{
		this->bump -= timer_dt;
		if (this->bump < 0)
			this->bump = 0;
	}

	//Auto-track eyes: look left at opponent, right at player
	if (stage.cur_section != NULL)
		this->eye_direction = (stage.cur_section->flag & SECTION_FLAG_OPPFOCUS) ? 1 : 0;

	//Update visualizer from XA loudness with smooth envelope
	if (Audio_PlayingXA())
	{
		for (int i = 0; i < 7; i++)
		{
			//Get loudness (0-255) and map to target frame
			u8 loud = Audio_GetLoudness(i);
			u8 tgt = (loud * 6) >> 8;
			if (tgt >= 6) tgt = 5;
			this->viz_target[i] = tgt;

			//Smooth +/-1 step toward target
			if (this->viz_frames[i] < this->viz_target[i])
				this->viz_frames[i]++;
			else if (this->viz_frames[i] > this->viz_target[i])
				this->viz_frames[i]--;
		}
	}
	else
	{
		//No audio: fade bars to silent
		for (int i = 0; i < 7; i++)
		{
			if (this->viz_frames[i] < 5)
				this->viz_frames[i]++;
		}
	}

	//Draw visualizer bars (7 bars x 6 frames)
	//Bar 1 at x=0, y=0
	{
		u8 frame = this->viz_frames[0];
		if (frame >= 6) frame = 5;
		u16 fi = 0 + frame;
		const RECT viz_src = {abot_viz_src[fi][0], abot_viz_src[fi][1], abot_viz_src[fi][2], abot_viz_src[fi][3]};
		RECT_FIXED viz_dst = {
			x - FIXED_DEC(abot_viz_off[fi][0],1) - this->bump - FIXED_MUL(stage.camera.x, parallax),
			y - FIXED_DEC(abot_viz_off[fi][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
			FIXED_DEC(abot_viz_off[fi][2],1),
			FIXED_DEC(abot_viz_off[fi][3],1)
		};
		Stage_DrawTex(&this->tex[abot_viz_texid[fi]], &viz_src, &viz_dst, stage.camera.bzoom, stage.camera.angle);
	}
	//Bar 2 at x=59, y=-8
	{
		u8 frame = this->viz_frames[1];
		if (frame >= 6) frame = 5;
		u16 fi = 6 + frame;
		const RECT viz_src = {abot_viz_src[fi][0], abot_viz_src[fi][1], abot_viz_src[fi][2], abot_viz_src[fi][3]};
		RECT_FIXED viz_dst = {
			x - FIXED_DEC(abot_viz_off[fi][0],1) - this->bump - FIXED_MUL(stage.camera.x, parallax),
			y - FIXED_DEC(abot_viz_off[fi][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
			FIXED_DEC(abot_viz_off[fi][2],1),
			FIXED_DEC(abot_viz_off[fi][3],1)
		};
		Stage_DrawTex(&this->tex[abot_viz_texid[fi]], &viz_src, &viz_dst, stage.camera.bzoom, stage.camera.angle);
	}
	//Bar 3 at x=56, y=-3.5
	{
		u8 frame = this->viz_frames[2];
		if (frame >= 6) frame = 5;
		u16 fi = 12 + frame;
		const RECT viz_src = {abot_viz_src[fi][0], abot_viz_src[fi][1], abot_viz_src[fi][2], abot_viz_src[fi][3]};
		RECT_FIXED viz_dst = {
			x - FIXED_DEC(abot_viz_off[fi][0],1) - this->bump - FIXED_MUL(stage.camera.x, parallax),
			y - FIXED_DEC(abot_viz_off[fi][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
			FIXED_DEC(abot_viz_off[fi][2],1),
			FIXED_DEC(abot_viz_off[fi][3],1)
		};
		Stage_DrawTex(&this->tex[abot_viz_texid[fi]], &viz_src, &viz_dst, stage.camera.bzoom, stage.camera.angle);
	}
	//Bar 4 at x=66, y=-0.4
	{
		u8 frame = this->viz_frames[3];
		if (frame >= 6) frame = 5;
		u16 fi = 18 + frame;
		const RECT viz_src = {abot_viz_src[fi][0], abot_viz_src[fi][1], abot_viz_src[fi][2], abot_viz_src[fi][3]};
		RECT_FIXED viz_dst = {
			x - FIXED_DEC(abot_viz_off[fi][0],1) - this->bump - FIXED_MUL(stage.camera.x, parallax),
			y - FIXED_DEC(abot_viz_off[fi][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
			FIXED_DEC(abot_viz_off[fi][2],1),
			FIXED_DEC(abot_viz_off[fi][3],1)
		};
		Stage_DrawTex(&this->tex[abot_viz_texid[fi]], &viz_src, &viz_dst, stage.camera.bzoom, stage.camera.angle);
	}
	//Bar 5 at x=54, y=0.5
	{
		u8 frame = this->viz_frames[4];
		if (frame >= 6) frame = 5;
		u16 fi = 24 + frame;
		const RECT viz_src = {abot_viz_src[fi][0], abot_viz_src[fi][1], abot_viz_src[fi][2], abot_viz_src[fi][3]};
		RECT_FIXED viz_dst = {
			x - FIXED_DEC(abot_viz_off[fi][0],1) - this->bump - FIXED_MUL(stage.camera.x, parallax),
			y - FIXED_DEC(abot_viz_off[fi][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
			FIXED_DEC(abot_viz_off[fi][2],1),
			FIXED_DEC(abot_viz_off[fi][3],1)
		};
		Stage_DrawTex(&this->tex[abot_viz_texid[fi]], &viz_src, &viz_dst, stage.camera.bzoom, stage.camera.angle);
	}
	//Bar 6 at x=52, y=4.7
	{
		u8 frame = this->viz_frames[5];
		if (frame >= 6) frame = 5;
		u16 fi = 30 + frame;
		const RECT viz_src = {abot_viz_src[fi][0], abot_viz_src[fi][1], abot_viz_src[fi][2], abot_viz_src[fi][3]};
		RECT_FIXED viz_dst = {
			x - FIXED_DEC(abot_viz_off[fi][0],1) - this->bump - FIXED_MUL(stage.camera.x, parallax),
			y - FIXED_DEC(abot_viz_off[fi][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
			FIXED_DEC(abot_viz_off[fi][2],1),
			FIXED_DEC(abot_viz_off[fi][3],1)
		};
		Stage_DrawTex(&this->tex[abot_viz_texid[fi]], &viz_src, &viz_dst, stage.camera.bzoom, stage.camera.angle);
	}
	//Bar 7 at x=51, y=7
	{
		u8 frame = this->viz_frames[6];
		if (frame >= 6) frame = 5;
		u16 fi = 36 + frame;
		const RECT viz_src = {abot_viz_src[fi][0], abot_viz_src[fi][1], abot_viz_src[fi][2], abot_viz_src[fi][3]};
		RECT_FIXED viz_dst = {
			x - FIXED_DEC(abot_viz_off[fi][0],1) - this->bump - FIXED_MUL(stage.camera.x, parallax),
			y - FIXED_DEC(abot_viz_off[fi][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
			FIXED_DEC(abot_viz_off[fi][2],1),
			FIXED_DEC(abot_viz_off[fi][3],1)
		};
		Stage_DrawTex(&this->tex[abot_viz_texid[fi]], &viz_src, &viz_dst, stage.camera.bzoom, stage.camera.angle);
	}

	//Draw body parts (all pieces)
	{
		{
			const RECT body_src = {abot_body_src[0][0], abot_body_src[0][1], abot_body_src[0][2], abot_body_src[0][3]};
			RECT_FIXED body_dst = {
				x - FIXED_DEC(abot_body_off[0][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_off[0][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_off[0][2],1),
				FIXED_DEC(abot_body_off[0][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_texid[0]], &body_src, &body_dst, stage.camera.bzoom, stage.camera.angle);
		}
		{
			const RECT body_src = {abot_body_src[1][0], abot_body_src[1][1], abot_body_src[1][2], abot_body_src[1][3]};
			RECT_FIXED body_dst = {
				x - FIXED_DEC(abot_body_off[1][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_off[1][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_off[1][2],1),
				FIXED_DEC(abot_body_off[1][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_texid[1]], &body_src, &body_dst, stage.camera.bzoom, stage.camera.angle);
		}
		{
			const RECT body_src = {abot_body_src[2][0], abot_body_src[2][1], abot_body_src[2][2], abot_body_src[2][3]};
			RECT_FIXED body_dst = {
				x - FIXED_DEC(abot_body_off[2][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_off[2][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_off[2][2],1),
				FIXED_DEC(abot_body_off[2][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_texid[2]], &body_src, &body_dst, stage.camera.bzoom, stage.camera.angle);
		}
		{
			const RECT body_src = {abot_body_src[3][0], abot_body_src[3][1], abot_body_src[3][2], abot_body_src[3][3]};
			RECT_FIXED body_dst = {
				x - FIXED_DEC(abot_body_off[3][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_off[3][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_off[3][2],1),
				FIXED_DEC(abot_body_off[3][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_texid[3]], &body_src, &body_dst, stage.camera.bzoom, stage.camera.angle);
		}
		{
			const RECT body_src = {abot_body_src[4][0], abot_body_src[4][1], abot_body_src[4][2], abot_body_src[4][3]};
			RECT_FIXED body_dst = {
				x - FIXED_DEC(abot_body_off[4][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_off[4][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_off[4][2],1),
				FIXED_DEC(abot_body_off[4][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_texid[4]], &body_src, &body_dst, stage.camera.bzoom, stage.camera.angle);
		}
		{
			const RECT body_src = {abot_body_src[5][0], abot_body_src[5][1], abot_body_src[5][2], abot_body_src[5][3]};
			RECT_FIXED body_dst = {
				x - FIXED_DEC(abot_body_off[5][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_off[5][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_off[5][2],1),
				FIXED_DEC(abot_body_off[5][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_texid[5]], &body_src, &body_dst, stage.camera.bzoom, stage.camera.angle);
		}
		{
			const RECT body_src = {abot_body_src[6][0], abot_body_src[6][1], abot_body_src[6][2], abot_body_src[6][3]};
			RECT_FIXED body_dst = {
				x - FIXED_DEC(abot_body_off[6][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_off[6][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_off[6][2],1),
				FIXED_DEC(abot_body_off[6][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_texid[6]], &body_src, &body_dst, stage.camera.bzoom, stage.camera.angle);
		}
		{
			const RECT body_src = {abot_body_src[7][0], abot_body_src[7][1], abot_body_src[7][2], abot_body_src[7][3]};
			RECT_FIXED body_dst = {
				x - FIXED_DEC(abot_body_off[7][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_off[7][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_off[7][2],1),
				FIXED_DEC(abot_body_off[7][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_texid[7]], &body_src, &body_dst, stage.camera.bzoom, stage.camera.angle);
		}
		{
			const RECT body_src = {abot_body_src[8][0], abot_body_src[8][1], abot_body_src[8][2], abot_body_src[8][3]};
			RECT_FIXED body_dst = {
				x - FIXED_DEC(abot_body_off[8][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_off[8][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_off[8][2],1),
				FIXED_DEC(abot_body_off[8][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_texid[8]], &body_src, &body_dst, stage.camera.bzoom, stage.camera.angle);
		}
		{
			const RECT body_src = {abot_body_src[9][0], abot_body_src[9][1], abot_body_src[9][2], abot_body_src[9][3]};
			RECT_FIXED body_dst = {
				x - FIXED_DEC(abot_body_off[9][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_off[9][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_off[9][2],1),
				FIXED_DEC(abot_body_off[9][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_texid[9]], &body_src, &body_dst, stage.camera.bzoom, stage.camera.angle);
		}
		{
			const RECT body_src = {abot_body_src[10][0], abot_body_src[10][1], abot_body_src[10][2], abot_body_src[10][3]};
			RECT_FIXED body_dst = {
				x - FIXED_DEC(abot_body_off[10][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_off[10][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_off[10][2],1),
				FIXED_DEC(abot_body_off[10][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_texid[10]], &body_src, &body_dst, stage.camera.bzoom, stage.camera.angle);
		}
	}
	//Draw dark body parts if enabled
	if (this->use_dark)
	{
		{
			const RECT bd_src = {abot_body_dark_src[0][0], abot_body_dark_src[0][1], abot_body_dark_src[0][2], abot_body_dark_src[0][3]};
			RECT_FIXED bd_dst = {
				x - FIXED_DEC(abot_body_dark_off[0][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_dark_off[0][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_dark_off[0][2],1),
				FIXED_DEC(abot_body_dark_off[0][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_dark_texid[0]], &bd_src, &bd_dst, stage.camera.bzoom, stage.camera.angle);
		}
		{
			const RECT bd_src = {abot_body_dark_src[1][0], abot_body_dark_src[1][1], abot_body_dark_src[1][2], abot_body_dark_src[1][3]};
			RECT_FIXED bd_dst = {
				x - FIXED_DEC(abot_body_dark_off[1][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_dark_off[1][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_dark_off[1][2],1),
				FIXED_DEC(abot_body_dark_off[1][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_dark_texid[1]], &bd_src, &bd_dst, stage.camera.bzoom, stage.camera.angle);
		}
		{
			const RECT bd_src = {abot_body_dark_src[2][0], abot_body_dark_src[2][1], abot_body_dark_src[2][2], abot_body_dark_src[2][3]};
			RECT_FIXED bd_dst = {
				x - FIXED_DEC(abot_body_dark_off[2][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_dark_off[2][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_dark_off[2][2],1),
				FIXED_DEC(abot_body_dark_off[2][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_dark_texid[2]], &bd_src, &bd_dst, stage.camera.bzoom, stage.camera.angle);
		}
		{
			const RECT bd_src = {abot_body_dark_src[3][0], abot_body_dark_src[3][1], abot_body_dark_src[3][2], abot_body_dark_src[3][3]};
			RECT_FIXED bd_dst = {
				x - FIXED_DEC(abot_body_dark_off[3][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_dark_off[3][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_dark_off[3][2],1),
				FIXED_DEC(abot_body_dark_off[3][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_dark_texid[3]], &bd_src, &bd_dst, stage.camera.bzoom, stage.camera.angle);
		}
		{
			const RECT bd_src = {abot_body_dark_src[4][0], abot_body_dark_src[4][1], abot_body_dark_src[4][2], abot_body_dark_src[4][3]};
			RECT_FIXED bd_dst = {
				x - FIXED_DEC(abot_body_dark_off[4][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_dark_off[4][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_dark_off[4][2],1),
				FIXED_DEC(abot_body_dark_off[4][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_dark_texid[4]], &bd_src, &bd_dst, stage.camera.bzoom, stage.camera.angle);
		}
		{
			const RECT bd_src = {abot_body_dark_src[5][0], abot_body_dark_src[5][1], abot_body_dark_src[5][2], abot_body_dark_src[5][3]};
			RECT_FIXED bd_dst = {
				x - FIXED_DEC(abot_body_dark_off[5][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_dark_off[5][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_dark_off[5][2],1),
				FIXED_DEC(abot_body_dark_off[5][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_dark_texid[5]], &bd_src, &bd_dst, stage.camera.bzoom, stage.camera.angle);
		}
		{
			const RECT bd_src = {abot_body_dark_src[6][0], abot_body_dark_src[6][1], abot_body_dark_src[6][2], abot_body_dark_src[6][3]};
			RECT_FIXED bd_dst = {
				x - FIXED_DEC(abot_body_dark_off[6][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_dark_off[6][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_dark_off[6][2],1),
				FIXED_DEC(abot_body_dark_off[6][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_dark_texid[6]], &bd_src, &bd_dst, stage.camera.bzoom, stage.camera.angle);
		}
		{
			const RECT bd_src = {abot_body_dark_src[7][0], abot_body_dark_src[7][1], abot_body_dark_src[7][2], abot_body_dark_src[7][3]};
			RECT_FIXED bd_dst = {
				x - FIXED_DEC(abot_body_dark_off[7][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_dark_off[7][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_dark_off[7][2],1),
				FIXED_DEC(abot_body_dark_off[7][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_dark_texid[7]], &bd_src, &bd_dst, stage.camera.bzoom, stage.camera.angle);
		}
		{
			const RECT bd_src = {abot_body_dark_src[8][0], abot_body_dark_src[8][1], abot_body_dark_src[8][2], abot_body_dark_src[8][3]};
			RECT_FIXED bd_dst = {
				x - FIXED_DEC(abot_body_dark_off[8][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_dark_off[8][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_dark_off[8][2],1),
				FIXED_DEC(abot_body_dark_off[8][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_dark_texid[8]], &bd_src, &bd_dst, stage.camera.bzoom, stage.camera.angle);
		}
		{
			const RECT bd_src = {abot_body_dark_src[9][0], abot_body_dark_src[9][1], abot_body_dark_src[9][2], abot_body_dark_src[9][3]};
			RECT_FIXED bd_dst = {
				x - FIXED_DEC(abot_body_dark_off[9][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_dark_off[9][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_dark_off[9][2],1),
				FIXED_DEC(abot_body_dark_off[9][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_dark_texid[9]], &bd_src, &bd_dst, stage.camera.bzoom, stage.camera.angle);
		}
		{
			const RECT bd_src = {abot_body_dark_src[10][0], abot_body_dark_src[10][1], abot_body_dark_src[10][2], abot_body_dark_src[10][3]};
			RECT_FIXED bd_dst = {
				x - FIXED_DEC(abot_body_dark_off[10][0],1) - FIXED_MUL(stage.camera.x, parallax),
				y - FIXED_DEC(abot_body_dark_off[10][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
				FIXED_DEC(abot_body_dark_off[10][2],1),
				FIXED_DEC(abot_body_dark_off[10][3],1)
			};
			Stage_DrawTex(&this->tex[abot_body_dark_texid[10]], &bd_src, &bd_dst, stage.camera.bzoom, stage.camera.angle);
		}
	}

	//Draw white square behind eyes (eye_bg)
	{
		RECT_FIXED ws_dst = {
			x - FIXED_DEC(abot_eye_bg_off[0][0],1) - FIXED_MUL(stage.camera.x, parallax),
			y - FIXED_DEC(abot_eye_bg_off[0][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
			FIXED_DEC(abot_eye_bg_off[0][2],1),
			FIXED_DEC(abot_eye_bg_off[0][3],1)
		};
		Stage_DrawRect(&ws_dst, stage.camera.bzoom, 255, 255, 255);
	}

	//Draw eyes
	{
		u8 efi = (this->eye_direction == 0) ? 0 : (2 > 1 ? 1 : 0);
		const RECT eye_src = {abot_eyes_src[efi][0], abot_eyes_src[efi][1], abot_eyes_src[efi][2], abot_eyes_src[efi][3]};
		RECT_FIXED eye_dst = {
			x - FIXED_DEC(abot_eyes_off[efi][0],1) - FIXED_MUL(stage.camera.x, parallax),
			y - FIXED_DEC(abot_eyes_off[efi][1],1) - this->bump - FIXED_MUL(stage.camera.y, parallax),
			FIXED_DEC(abot_eyes_off[efi][2],1),
			FIXED_DEC(abot_eyes_off[efi][3],1)
		};
		Stage_DrawTex(&this->tex[abot_eyes_texid[efi]], &eye_src, &eye_dst, stage.camera.bzoom, stage.camera.angle);
	}

	//Draw background
	{
		const RECT bg_src = {abot_bg_src[0][0], abot_bg_src[0][1], abot_bg_src[0][2], abot_bg_src[0][3]};
		RECT_FIXED bg_dst = {
			x - FIXED_DEC(abot_bg_off[0][0],1) - FIXED_MUL(stage.camera.x, parallax),
			y - FIXED_DEC(abot_bg_off[0][1],1) - FIXED_MUL(stage.camera.y, parallax),
			FIXED_DEC(abot_bg_off[0][2],1),
			FIXED_DEC(abot_bg_off[0][3],1)
		};
		Stage_DrawTex(&this->tex[abot_bg_texid[0]], &bg_src, &bg_dst, stage.camera.bzoom, stage.camera.angle);
	}

}

//ABot speaker bump
void ABotSpeaker_Bump(ABotSpeaker *this)
{
	this->bump = FIXED_DEC(4,1);
	//Boost all viz bars on beat
	for (int i = 0; i < 7; i++)
		this->viz_target[i] = 5;
}

//ABot speaker look
void ABotSpeaker_LookLeft(ABotSpeaker *this)
{
	this->eye_direction = 1;
}

void ABotSpeaker_LookRight(ABotSpeaker *this)
{
	this->eye_direction = 0;
}
