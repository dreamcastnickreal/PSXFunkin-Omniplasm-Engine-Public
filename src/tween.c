/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "tween.h"

#include "timer.h"

//Easing functions
static fixed_t Easing_CalculateValue(Tween* tween)
{
	//Divide the elapsed time by the desired time to normalize it
	fixed_t time = FIXED_DIV(tween->elapsed_time, tween->duration);
	
	//Calculate the easing value based on the specified ease
	switch (tween->ease)
	{
		case EASING_LINEAR:
			//Linear easing: progress linearly with time
			return time;
		break;
		case EASING_QUAD_IN:
			//Quadratic easing in: start slowly and accelerate
			return FIXED_MUL(time, time);
		break;
		case EASING_QUAD_OUT:
			//Quadratic easing out: start quickly and decelerate
			return time * 2 - FIXED_MUL(time, time);
		break;
		case EASING_QUAD_IN_OUT:
			if (time < FIXED_UNIT >> 1)
				return FIXED_MUL(FIXED_MUL(FIXED_UNIT << 1, time), time); //Quadratic easing in
			else
				return FIXED_UNIT - FIXED_MUL(FIXED_DEC(2,1), FIXED_MUL(time - FIXED_UNIT, time - FIXED_UNIT)); //Quadratic easing out
		break;
	}
	
	return FIXED_UNIT; //Default to max value if no easing method specified
}

//Tween functions
void Tween_InitWithValue(Tween* tween, fixed_t initial_value, fixed_t final_value, fixed_t duration, Eases ease, u8 flags)
{
	//Initialize tween state
	tween->value_pointer = NULL;
	tween->initial_value = initial_value;
	tween->final_value = final_value;
	tween->duration = duration;
	tween->ease = ease;
	tween->flags = flags;
	tween->elapsed_time = 0;
	
	//Set the current value to the initial or final value depending of the backward flag
	tween->current_value = (tween->flags & TWEEN_FLAGS_BACKWARD) ? final_value : initial_value;
}

void Tween_InitWithVariable(Tween* tween, fixed_t* valuep, fixed_t final_value, fixed_t duration, Eases ease, u8 flags)
{
	Tween_InitWithValue(tween, *valuep, final_value, duration, ease, flags);
	
	//Initialize tween pointers
	tween->value_pointer = valuep;
	*tween->value_pointer = tween->current_value;
}

void Tween_Tick(Tween* tween)
{
	if (tween->value_pointer != NULL)
		*tween->value_pointer = tween->current_value;
	
	//Update the current value based on the easing function
	if (tween->flags & TWEEN_FLAGS_BACKWARD)
		tween->current_value = tween->final_value + FIXED_MUL((tween->initial_value - tween->final_value), Easing_CalculateValue(tween));
	else
		tween->current_value = tween->initial_value + FIXED_MUL((tween->final_value - tween->initial_value), Easing_CalculateValue(tween));
	
	//Update the elapsed time since the tween started
	if (tween->elapsed_time != tween->duration)
		tween->elapsed_time += timer_dt;
	
	//Reset the elapsed time if the loop flag is set, otherwise stop updating the tween
	if (tween->elapsed_time > tween->duration)
	{
		if (tween->flags & TWEEN_FLAGS_LOOP)
			tween->elapsed_time = 0;
		else
			tween->elapsed_time = tween->duration;
	}
}

//Retrieve the current value of the tween
fixed_t Tween_GetValue(Tween* tween)
{
	return tween->current_value;
}

// Enhanced tween functions for All Stars
void Tween_SetEase(Tween* tween, Eases ease)
{
	tween->ease = ease;
}

void Tween_SetDuration(Tween* tween, fixed_t duration)
{
	tween->duration = duration;
}

void Tween_Reset(Tween* tween)
{
	tween->elapsed_time = 0;
	tween->current_value = (tween->flags & TWEEN_FLAGS_BACKWARD) ? tween->final_value : tween->initial_value;
	if (tween->value_pointer != NULL)
		*tween->value_pointer = tween->current_value;
}

void Tween_Stop(Tween* tween)
{
	tween->elapsed_time = tween->duration;
}

boolean Tween_IsComplete(Tween* tween)
{
	return tween->elapsed_time >= tween->duration;
}

fixed_t Tween_GetProgress(Tween* tween)
{
	if (tween->duration == 0)
		return FIXED_UNIT;
	return FIXED_DIV(tween->elapsed_time, tween->duration);
}

// Advanced easing functions
fixed_t Tween_EaseInCubic(fixed_t t)
{
	return FIXED_MUL(FIXED_MUL(t, t), t);
}

fixed_t Tween_EaseOutCubic(fixed_t t)
{
	fixed_t t_minus_1 = t - FIXED_UNIT;
	return FIXED_UNIT + FIXED_MUL(FIXED_MUL(t_minus_1, t_minus_1), t_minus_1);
}

fixed_t Tween_EaseInOutCubic(fixed_t t)
{
	if (t < FIXED_DEC(5, 10))
		return FIXED_MUL(FIXED_DEC(4, 1), FIXED_MUL(FIXED_MUL(t, t), t));
	else
	{
		fixed_t t_shifted = t - FIXED_UNIT;
		return FIXED_UNIT + FIXED_MUL(FIXED_DEC(4, 1), FIXED_MUL(FIXED_MUL(t_shifted, t_shifted), t_shifted));
	}
}

fixed_t Tween_EaseInQuart(fixed_t t)
{
	return FIXED_MUL(FIXED_MUL(FIXED_MUL(t, t), t), t);
}

fixed_t Tween_EaseOutQuart(fixed_t t)
{
	fixed_t t_minus_1 = t - FIXED_UNIT;
	return FIXED_UNIT - FIXED_MUL(FIXED_MUL(FIXED_MUL(t_minus_1, t_minus_1), t_minus_1), t_minus_1);
}

fixed_t Tween_EaseInOutQuart(fixed_t t)
{
	if (t < FIXED_DEC(5, 10))
		return FIXED_MUL(FIXED_DEC(8, 1), FIXED_MUL(FIXED_MUL(FIXED_MUL(t, t), t), t));
	else
	{
		fixed_t t_shifted = t - FIXED_UNIT;
		return FIXED_UNIT - FIXED_MUL(FIXED_DEC(8, 1), FIXED_MUL(FIXED_MUL(FIXED_MUL(t_shifted, t_shifted), t_shifted), t_shifted));
	}
}

fixed_t Tween_EaseInBounce(fixed_t t)
{
	return FIXED_UNIT - Tween_EaseOutBounce(FIXED_UNIT - t);
}

fixed_t Tween_EaseOutBounce(fixed_t t)
{
	if (t < FIXED_DEC(4, 11))
	{
		return FIXED_MUL(FIXED_DEC(121, 16), FIXED_MUL(t, t));
	}
	else if (t < FIXED_DEC(8, 11))
	{
		fixed_t t_adj = t - FIXED_DEC(6, 11);
		return FIXED_DEC(3, 4) + FIXED_MUL(FIXED_DEC(121, 16), FIXED_MUL(t_adj, t_adj));
	}
	else if (t < FIXED_DEC(10, 11))
	{
		fixed_t t_adj = t - FIXED_DEC(9, 11);
		return FIXED_DEC(15, 16) + FIXED_MUL(FIXED_DEC(121, 16), FIXED_MUL(t_adj, t_adj));
	}
	else
	{
		fixed_t t_adj = t - FIXED_DEC(21, 22);
		return FIXED_DEC(63, 64) + FIXED_MUL(FIXED_DEC(121, 16), FIXED_MUL(t_adj, t_adj));
	}
}

fixed_t Tween_EaseInOutBounce(fixed_t t)
{
	if (t < FIXED_DEC(5, 10))
		return FIXED_MUL(Tween_EaseInBounce(FIXED_MUL(t, FIXED_DEC(2, 1))), FIXED_DEC(5, 10));
	else
		return FIXED_DEC(5, 10) + FIXED_MUL(Tween_EaseOutBounce(FIXED_MUL(t, FIXED_DEC(2, 1)) - FIXED_UNIT), FIXED_DEC(5, 10));
}
