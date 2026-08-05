#ifndef PSXF_GUARD_ABOT_SPEAKER_H
#define PSXF_GUARD_ABOT_SPEAKER_H

#include "../gfx.h"
#include "../fixed.h"
#include "../io.h"

//ABot speaker structure
typedef struct {
	//ARC data per group
	IO_Data arc_bg;
	IO_Data arc_body;
	IO_Data arc_viz;
	IO_Data arc_ptr[6];

	//Textures (loaded lazily per group)
	Gfx_Tex tex_body;
	Gfx_Tex tex_viz;
	Gfx_Tex tex_bg;
	u8 current_body_tex; //0-3 = which body tex loaded, 0xFF = none

	//State
	fixed_t bump;
	u8 body_frame;
	u8 viz_frames[7];
	u8 viz_target[7];
	u8 eye_direction;
	boolean use_dark;
	boolean frozen;
} ABotSpeaker;

//ABot speaker functions
void ABotSpeaker_Init(ABotSpeaker *this, boolean use_dark);
void ABotSpeaker_Tick(ABotSpeaker *this, fixed_t x, fixed_t y, fixed_t parallax);
void ABotSpeaker_Bump(ABotSpeaker *this);
void ABotSpeaker_LookLeft(ABotSpeaker *this);
void ABotSpeaker_LookRight(ABotSpeaker *this);

#endif