/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef PSXF_GUARD_TWEEN_H
#define PSXF_GUARD_TWEEN_H

#include "psx.h"

#include "fixed.h"

#define TWEEN_FLAGS_LOOP		(1 << 1) //If set, the Tween will restart from the initial value after reaching the final value
#define TWEEN_FLAGS_BACKWARD	(1 << 2) //If set, the Tween will play in reverse from final value to initial value

//Easing methods enumeration
typedef enum
{
	EASING_LINEAR,			//Linear easing: progress linearly with time
	EASING_QUAD_IN, 		//Quadratic easing in: start slowly and accelerate
	EASING_QUAD_OUT, 		//Quadratic easing out: start quickly and decelerate
	EASING_QUAD_IN_OUT, 	//Quadratic easing in and out: start slowly, accelerate, then decelerate
	
} Eases;

//Tween struct
typedef struct
{
	fixed_t initial_value; 			//Initial value of the tween
	fixed_t* value_pointer; 		//Pointer to the initial value of the tween (used when you want change the initial value directly)
	fixed_t final_value; 			//Final value of the tween
	fixed_t current_value; 			//Current value of the tween
	Eases ease;  					//Easing method used for the tween
	fixed_t duration; 				//Total duration of the tween in seconds
	u8 flags;						//The flags of the tween
	
	//"Private" members (You probably not will need use these members)
	
	fixed_t elapsed_time;			//Elapsed time since the tween started
} Tween;

//Tween functions
void Tween_InitWithValue(Tween* tween, fixed_t initial_value, fixed_t final_value, fixed_t duration, Eases ease, u8 flags);
void Tween_InitWithVariable(Tween* tween, fixed_t* valuep, fixed_t final_value, fixed_t duration, Eases ease, u8 flags);

void Tween_Tick(Tween* tween);

fixed_t Tween_GetValue(Tween* tween); //Retrieve the current value of the tween

// Enhanced tween functions for All Stars
void Tween_SetEase(Tween* tween, Eases ease);
void Tween_SetDuration(Tween* tween, fixed_t duration);
void Tween_Reset(Tween* tween);
void Tween_Stop(Tween* tween);
boolean Tween_IsComplete(Tween* tween);
fixed_t Tween_GetProgress(Tween* tween); // Returns progress from 0 to 1

// Advanced easing functions
fixed_t Tween_EaseInCubic(fixed_t t);
fixed_t Tween_EaseOutCubic(fixed_t t);
fixed_t Tween_EaseInOutCubic(fixed_t t);
fixed_t Tween_EaseInQuart(fixed_t t);
fixed_t Tween_EaseOutQuart(fixed_t t);
fixed_t Tween_EaseInOutQuart(fixed_t t);
fixed_t Tween_EaseInBounce(fixed_t t);
fixed_t Tween_EaseOutBounce(fixed_t t);
fixed_t Tween_EaseInOutBounce(fixed_t t);

#endif