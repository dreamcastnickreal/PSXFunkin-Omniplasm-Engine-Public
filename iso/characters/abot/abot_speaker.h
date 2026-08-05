#ifndef PSXF_GUARD_ABOT_SPEAKER_H
#define PSXF_GUARD_ABOT_SPEAKER_H

#include "../gfx.h"
#include "../fixed.h"
#include "../io.h"

//ABot speaker structure
typedef struct {
	//ARC data
	IO_Data arc_main;
	IO_Data arc_ptr[3];

	//Textures
	Gfx_Tex tex[3];

	//State
	fixed_t bump;
	u8 current_frame;
	u8 eye_frame;
	u8 viz_frames[7];
	u8 viz_target[7];
	u8 eye_direction;
	boolean use_dark;
} ABotSpeaker;

//ABot speaker functions
void ABotSpeaker_Init(ABotSpeaker *this, boolean use_dark);
void ABotSpeaker_Tick(ABotSpeaker *this, fixed_t x, fixed_t y, fixed_t parallax);
void ABotSpeaker_Bump(ABotSpeaker *this);
void ABotSpeaker_LookLeft(ABotSpeaker *this);
void ABotSpeaker_LookRight(ABotSpeaker *this);

#endif