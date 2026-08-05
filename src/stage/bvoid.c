/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "bvoid.h"

#include "../archive.h"
#include "../mem.h"
#include "../stage.h"

//BVoid background structure
typedef struct
{
	//Stage background base structure
	StageBack back;
} Back_BVoid;

//BVoid background functions
void Back_BVoid_Free(StageBack *back)
{
	Back_BVoid *this = (Back_BVoid*)back;
	
	//Free structure
	Mem_Free(this);
}

StageBack *Back_BVoid_New(void)
{
	//Allocate background structure
	Back_BVoid *this = (Back_BVoid*)Mem_Alloc(sizeof(Back_BVoid));
	if (this == NULL)
		return NULL;
	
	//Set background functions
	this->back.draw_hud = NULL;
	this->back.draw_fg = NULL;
	this->back.draw_md = NULL;
	this->back.draw_bg = NULL;
	this->back.free = Back_BVoid_Free;

	//Use non-pitch black background
	Gfx_SetClear(0, 0, 0);
	
	return (StageBack*)this;
}
