#include "abot_speaker.h"

#include "../io.h"
#include "../stage.h"
#include "../timer.h"
#include "../audio.h"
#include "../mem.h"
#include "../archive.h"

//ABot speaker arc indices (6 sheets)
enum {
	ABOT_SPK_body0,
	ABOT_SPK_body1,
	ABOT_SPK_body2,
	ABOT_SPK_body3,
	ABOT_SPK_bg0,
	ABOT_SPK_viz0,
};

//ABot speaker section frame constants
static const u16 abot_body_src[4][4] = {
	{0, 0, 204, 90},
	{0, 90, 204, 90},
	{0, 0, 204, 90},
	{0, 90, 204, 90},
};
static const s16 abot_body_off[4][4] = {
	{0, 0, 204, 90},
	{0, 0, 204, 90},
	{0, 0, 204, 90},
	{0, 0, 204, 90},
};
static const u8 abot_body_texid[4] = {
	ABOT_SPK_body0,
	ABOT_SPK_body0,
	ABOT_SPK_body1,
	ABOT_SPK_body1,
};

static const u16 abot_body_dark_src[4][4] = {
	{0, 0, 204, 90},
	{0, 90, 204, 90},
	{0, 0, 204, 90},
	{0, 90, 204, 90},
};
static const s16 abot_body_dark_off[4][4] = {
	{0, 0, 204, 90},
	{0, 0, 204, 90},
	{0, 0, 204, 90},
	{0, 0, 204, 90},
};
static const u8 abot_body_dark_texid[4] = {
	ABOT_SPK_body2,
	ABOT_SPK_body2,
	ABOT_SPK_body3,
	ABOT_SPK_body3,
};

static const u16 abot_eyes_src[2][4] = {
	{0, 0, 26, 10},
	{0, 10, 26, 10},
};
static const s16 abot_eyes_off[2][4] = {
	{-13, -61, 26, 10},
	{-13, -61, 26, 10},
};
static const u16 abot_viz_src[42][4] = {
	{0, 0, 17, 49},
	{17, 0, 17, 49},
	{34, 0, 17, 49},
	{51, 0, 17, 49},
	{68, 0, 17, 49},
	{85, 0, 17, 49},
	{102, 0, 14, 52},
	{116, 0, 14, 52},
	{130, 0, 14, 52},
	{144, 0, 14, 52},
	{158, 0, 14, 52},
	{172, 0, 14, 52},
	{186, 0, 14, 54},
	{200, 0, 14, 54},
	{214, 0, 14, 54},
	{228, 0, 14, 54},
	{242, 0, 14, 54},
	{0, 54, 14, 54},
	{14, 54, 14, 54},
	{28, 54, 14, 54},
	{42, 54, 14, 54},
	{56, 54, 14, 54},
	{70, 54, 14, 54},
	{84, 54, 14, 54},
	{98, 54, 16, 54},
	{114, 54, 16, 54},
	{130, 54, 16, 54},
	{146, 54, 16, 54},
	{162, 54, 16, 54},
	{178, 54, 16, 54},
	{194, 54, 17, 52},
	{211, 54, 17, 52},
	{228, 54, 17, 52},
	{0, 108, 17, 52},
	{17, 108, 17, 52},
	{34, 108, 17, 52},
	{51, 108, 18, 48},
	{69, 108, 18, 48},
	{87, 108, 18, 48},
	{105, 108, 18, 48},
	{123, 108, 18, 48},
	{141, 108, 18, 48},
};
static const s16 abot_viz_off[42][4] = {
	{-51, -21, 17, 49},
	{-51, -21, 17, 49},
	{-51, -21, 17, 49},
	{-51, -21, 17, 49},
	{-51, -21, 17, 49},
	{-51, -21, 17, 49},
	{-66, -19, 14, 52},
	{-66, -19, 14, 52},
	{-66, -19, 14, 52},
	{-66, -19, 14, 52},
	{-66, -19, 14, 52},
	{-66, -19, 14, 52},
	{-80, -18, 14, 54},
	{-80, -18, 14, 54},
	{-80, -18, 14, 54},
	{-80, -18, 14, 54},
	{-80, -18, 14, 54},
	{-80, -18, 14, 54},
	{-96, -18, 14, 54},
	{-96, -18, 14, 54},
	{-96, -18, 14, 54},
	{-96, -18, 14, 54},
	{-96, -18, 14, 54},
	{-96, -18, 14, 54},
	{-110, -18, 16, 54},
	{-110, -18, 16, 54},
	{-110, -18, 16, 54},
	{-110, -18, 16, 54},
	{-110, -18, 16, 54},
	{-110, -18, 16, 54},
	{-123, -19, 17, 52},
	{-123, -19, 17, 52},
	{-123, -19, 17, 52},
	{-123, -19, 17, 52},
	{-123, -19, 17, 52},
	{-123, -19, 17, 52},
	{-136, -20, 18, 48},
	{-136, -20, 18, 48},
	{-136, -20, 18, 48},
	{-136, -20, 18, 48},
	{-136, -20, 18, 48},
	{-136, -20, 18, 48},
};
static const u8 abot_viz_texid[42] = {
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
	ABOT_SPK_viz0,
};

static const u16 abot_bg_src[1][4] = {
	{46, 0, 121, 71},
};
static const s16 abot_bg_off[1][4] = {
	{-38, -8, 121, 71},
};
static const s16 abot_eye_bg_off[1][4] = {
	{-8, -57, 40, 15},
};

//ABot speaker init
void ABotSpeaker_Init(ABotSpeaker *this, boolean use_dark)
{
	//Initialize state
	this->bump = 0;
	this->body_frame = 0;
	for (int i = 0; i < 7; i++)
	{
		this->viz_frames[i] = 5;
		this->viz_target[i] = 5;
	}
	this->eye_direction = 0;
	this->use_dark = use_dark;
	this->frozen = false;

	//Load speaker graphics from group ARCs
	this->arc_bg = IO_Read("\\GCHAR\\ABOT\\BG.ARC;1");
	this->arc_ptr[4] = Archive_Find(this->arc_bg, "bg0.tim");
	this->arc_body = IO_Read("\\GCHAR\\ABOT\\BODY.ARC;1");
	this->arc_ptr[0] = Archive_Find(this->arc_body, "body0.tim");
	this->arc_ptr[1] = Archive_Find(this->arc_body, "body1.tim");
	this->arc_ptr[2] = Archive_Find(this->arc_body, "body2.tim");
	this->arc_ptr[3] = Archive_Find(this->arc_body, "body3.tim");
	this->arc_viz = IO_Read("\\GCHAR\\ABOT\\VIZ.ARC;1");
	this->arc_ptr[5] = Archive_Find(this->arc_viz, "viz0.tim");

	//Preload bg (eyes + stereoBG) and viz textures
	Gfx_LoadTex(&this->tex_bg, this->arc_ptr[4], 0);
	Gfx_LoadTex(&this->tex_viz, this->arc_ptr[5], 0);

	//Body textures loaded lazily on frame change
	this->current_body_tex = 0xFF;
}

//ABot speaker tick
void ABotSpeaker_Tick(ABotSpeaker *this, fixed_t x, fixed_t y, fixed_t parallax)
{
	x -= FIXED_DEC(100,1);

	//Toggle freeze on Select press
	if (pad_state.press & PAD_SELECT)
		this->frozen = !this->frozen;

	if (!this->frozen)
	{
		//Decay bump and advance body animation
		if (this->bump > 0)
		{
			this->bump -= timer_dt;
			//Advance body animation frame
			if (this->body_frame < 3)
				this->body_frame++;
			if (this->bump < 0)
				this->bump = 0;
		}
	}

	//Auto-track eyes: look left at opponent, right at player
	if (stage.cur_section != NULL)
		this->eye_direction = (stage.cur_section->flag & SECTION_FLAG_OPPFOCUS) ? 1 : 0;

	//Update visualizer from XA loudness with smooth envelope
	if (!this->frozen)
	{
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
	}

	//Draw body composite frame (timeline animation)
	{
		u16 bi = this->body_frame;
		u8 needed = abot_body_texid[bi];
		if (needed != this->current_body_tex)
		{
			Gfx_LoadTex(&this->tex_body, this->arc_ptr[needed], 0);
			this->current_body_tex = needed;
		}
		const RECT body_src = {abot_body_src[bi][0], abot_body_src[bi][1], abot_body_src[bi][2], abot_body_src[bi][3]};
		RECT_FIXED body_dst = {
			x - FIXED_MUL(stage.camera.x, parallax) - FIXED_DEC(abot_body_off[bi][0],1),
			y - FIXED_MUL(stage.camera.y, parallax) - FIXED_DEC(abot_body_off[bi][1],1),
			FIXED_DEC(abot_body_off[bi][2],1),
			FIXED_DEC(abot_body_off[bi][3],1)
		};
		Stage_DrawTex(&this->tex_body, &body_src, &body_dst, stage.camera.bzoom, stage.camera.angle);
	}
	//Draw dark body composite frame if enabled
	if (this->use_dark)
	{
		u16 bi = this->body_frame;
		u8 needed = abot_body_dark_texid[bi];
		if (needed != this->current_body_tex)
		{
			Gfx_LoadTex(&this->tex_body, this->arc_ptr[needed], 0);
			this->current_body_tex = needed;
		}
		const RECT bd_src = {abot_body_dark_src[bi][0], abot_body_dark_src[bi][1], abot_body_dark_src[bi][2], abot_body_dark_src[bi][3]};
		RECT_FIXED bd_dst = {
			x - FIXED_MUL(stage.camera.x, parallax) - FIXED_DEC(abot_body_dark_off[bi][0],1),
			y - FIXED_MUL(stage.camera.y, parallax) - FIXED_DEC(abot_body_dark_off[bi][1],1),
			FIXED_DEC(abot_body_dark_off[bi][2],1),
			FIXED_DEC(abot_body_dark_off[bi][3],1)
		};
		Stage_DrawTex(&this->tex_body, &bd_src, &bd_dst, stage.camera.bzoom, stage.camera.angle);
	}
	
	//Draw eyes
	{
		u8 efi = (this->eye_direction == 0) ? 0 : (2 > 1 ? 1 : 0);
		const RECT eye_src = {abot_eyes_src[efi][0], abot_eyes_src[efi][1], abot_eyes_src[efi][2], abot_eyes_src[efi][3]};
		RECT_FIXED eye_dst = {
			x - FIXED_MUL(stage.camera.x, parallax) - FIXED_DEC(abot_eyes_off[efi][0],1),
			y - FIXED_MUL(stage.camera.y, parallax) - FIXED_DEC(abot_eyes_off[efi][1],1),
			FIXED_DEC(abot_eyes_off[efi][2],1),
			FIXED_DEC(abot_eyes_off[efi][3],1)
		};
		Stage_DrawTex(&this->tex_bg, &eye_src, &eye_dst, stage.camera.bzoom, stage.camera.angle);
	}

	//Draw white square behind eyes (eye_bg)
	{
		RECT_FIXED ws_dst = {
			x - FIXED_MUL(stage.camera.x, parallax) - FIXED_DEC(abot_eye_bg_off[0][0],1),
			y - FIXED_MUL(stage.camera.y, parallax) - FIXED_DEC(abot_eye_bg_off[0][1],1),
			FIXED_DEC(abot_eye_bg_off[0][2],1),
			FIXED_DEC(abot_eye_bg_off[0][3],1)
		};
		Stage_DrawRect(&ws_dst, stage.camera.bzoom, 255, 255, 255);
	}
	
	//Draw visualizer bars (7 bars x 6 frames)
	//Bar 1 at x=0, y=0
	{
		u8 frame = this->viz_frames[0];
		if (frame >= 6) frame = 5;
		u16 fi = 0 + frame;
		const RECT viz_src = {abot_viz_src[fi][0], abot_viz_src[fi][1], abot_viz_src[fi][2], abot_viz_src[fi][3]};
		RECT_FIXED viz_dst = {
			x - FIXED_MUL(stage.camera.x, parallax) - FIXED_DEC(abot_viz_off[fi][0],1),
			y - FIXED_MUL(stage.camera.y, parallax) - FIXED_DEC(abot_viz_off[fi][1],1),
			FIXED_DEC(abot_viz_off[fi][2],1),
			FIXED_DEC(abot_viz_off[fi][3],1)
		};
		Stage_DrawTex(&this->tex_viz, &viz_src, &viz_dst, stage.camera.bzoom, stage.camera.angle);
	}
	//Bar 2 at x=59, y=-8
	{
		u8 frame = this->viz_frames[1];
		if (frame >= 6) frame = 5;
		u16 fi = 6 + frame;
		const RECT viz_src = {abot_viz_src[fi][0], abot_viz_src[fi][1], abot_viz_src[fi][2], abot_viz_src[fi][3]};
		RECT_FIXED viz_dst = {
			x - FIXED_MUL(stage.camera.x, parallax) - FIXED_DEC(abot_viz_off[fi][0],1),
			y - FIXED_MUL(stage.camera.y, parallax) - FIXED_DEC(abot_viz_off[fi][1],1),
			FIXED_DEC(abot_viz_off[fi][2],1),
			FIXED_DEC(abot_viz_off[fi][3],1)
		};
		Stage_DrawTex(&this->tex_viz, &viz_src, &viz_dst, stage.camera.bzoom, stage.camera.angle);
	}
	//Bar 3 at x=56, y=-3.5
	{
		u8 frame = this->viz_frames[2];
		if (frame >= 6) frame = 5;
		u16 fi = 12 + frame;
		const RECT viz_src = {abot_viz_src[fi][0], abot_viz_src[fi][1], abot_viz_src[fi][2], abot_viz_src[fi][3]};
		RECT_FIXED viz_dst = {
			x - FIXED_MUL(stage.camera.x, parallax) - FIXED_DEC(abot_viz_off[fi][0],1),
			y - FIXED_MUL(stage.camera.y, parallax) - FIXED_DEC(abot_viz_off[fi][1],1),
			FIXED_DEC(abot_viz_off[fi][2],1),
			FIXED_DEC(abot_viz_off[fi][3],1)
		};
		Stage_DrawTex(&this->tex_viz, &viz_src, &viz_dst, stage.camera.bzoom, stage.camera.angle);
	}
	//Bar 4 at x=66, y=-0.4
	{
		u8 frame = this->viz_frames[3];
		if (frame >= 6) frame = 5;
		u16 fi = 18 + frame;
		const RECT viz_src = {abot_viz_src[fi][0], abot_viz_src[fi][1], abot_viz_src[fi][2], abot_viz_src[fi][3]};
		RECT_FIXED viz_dst = {
			x - FIXED_MUL(stage.camera.x, parallax) - FIXED_DEC(abot_viz_off[fi][0],1),
			y - FIXED_MUL(stage.camera.y, parallax) - FIXED_DEC(abot_viz_off[fi][1],1),
			FIXED_DEC(abot_viz_off[fi][2],1),
			FIXED_DEC(abot_viz_off[fi][3],1)
		};
		Stage_DrawTex(&this->tex_viz, &viz_src, &viz_dst, stage.camera.bzoom, stage.camera.angle);
	}
	//Bar 5 at x=54, y=0.5
	{
		u8 frame = this->viz_frames[4];
		if (frame >= 6) frame = 5;
		u16 fi = 24 + frame;
		const RECT viz_src = {abot_viz_src[fi][0], abot_viz_src[fi][1], abot_viz_src[fi][2], abot_viz_src[fi][3]};
		RECT_FIXED viz_dst = {
			x - FIXED_MUL(stage.camera.x, parallax) - FIXED_DEC(abot_viz_off[fi][0],1),
			y - FIXED_MUL(stage.camera.y, parallax) - FIXED_DEC(abot_viz_off[fi][1],1),
			FIXED_DEC(abot_viz_off[fi][2],1),
			FIXED_DEC(abot_viz_off[fi][3],1)
		};
		Stage_DrawTex(&this->tex_viz, &viz_src, &viz_dst, stage.camera.bzoom, stage.camera.angle);
	}
	//Bar 6 at x=52, y=4.7
	{
		u8 frame = this->viz_frames[5];
		if (frame >= 6) frame = 5;
		u16 fi = 30 + frame;
		const RECT viz_src = {abot_viz_src[fi][0], abot_viz_src[fi][1], abot_viz_src[fi][2], abot_viz_src[fi][3]};
		RECT_FIXED viz_dst = {
			x - FIXED_MUL(stage.camera.x, parallax) - FIXED_DEC(abot_viz_off[fi][0],1),
			y - FIXED_MUL(stage.camera.y, parallax) - FIXED_DEC(abot_viz_off[fi][1],1),
			FIXED_DEC(abot_viz_off[fi][2],1),
			FIXED_DEC(abot_viz_off[fi][3],1)
		};
		Stage_DrawTex(&this->tex_viz, &viz_src, &viz_dst, stage.camera.bzoom, stage.camera.angle);
	}
	//Bar 7 at x=51, y=7
	{
		u8 frame = this->viz_frames[6];
		if (frame >= 6) frame = 5;
		u16 fi = 36 + frame;
		const RECT viz_src = {abot_viz_src[fi][0], abot_viz_src[fi][1], abot_viz_src[fi][2], abot_viz_src[fi][3]};
		RECT_FIXED viz_dst = {
			x - FIXED_MUL(stage.camera.x, parallax) - FIXED_DEC(abot_viz_off[fi][0],1),
			y - FIXED_MUL(stage.camera.y, parallax) - FIXED_DEC(abot_viz_off[fi][1],1),
			FIXED_DEC(abot_viz_off[fi][2],1),
			FIXED_DEC(abot_viz_off[fi][3],1)
		};
		Stage_DrawTex(&this->tex_viz, &viz_src, &viz_dst, stage.camera.bzoom, stage.camera.angle);
	}

	//Draw background
	{
		const RECT bg_src = {abot_bg_src[0][0], abot_bg_src[0][1], abot_bg_src[0][2], abot_bg_src[0][3]};
		RECT_FIXED bg_dst = {
			x - FIXED_MUL(stage.camera.x, parallax) - FIXED_DEC(abot_bg_off[0][0],1),
			y - FIXED_MUL(stage.camera.y, parallax) - FIXED_DEC(abot_bg_off[0][1],1),
			FIXED_DEC(abot_bg_off[0][2],1),
			FIXED_DEC(abot_bg_off[0][3],1)
		};
		Stage_DrawTex(&this->tex_bg, &bg_src, &bg_dst, stage.camera.bzoom, stage.camera.angle);
	}

}

//ABot speaker bump
void ABotSpeaker_Bump(ABotSpeaker *this)
{
	this->bump = FIXED_DEC(4,1);
	//Reset body animation on beat
	this->body_frame = 0;
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
