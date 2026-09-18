/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "max.h"


#include "../mem.h"
#include "../archive.h"
#include "../stage.h"
#include "../main.h"

enum
{
	Max_ArcMain_Max0,
	Max_ArcMain_Max1,
	Max_ArcMain_Max2,
	Max_ArcMain_Max3,
	Max_ArcMain_Max4,

	Max_Arc_Max,
};


typedef struct
{
	//Character base structure
	Character character;
	
	//Render data and state
	IO_Data arc_main;
	IO_Data arc_ptr[Max_Arc_Max];

	Gfx_Tex tex;
	u8 frame, tex_id;
} Char_Max;


static const u16 char_max_icons[2][4] = {
	{0,0,64,64},
	{64,0,64,64}
};


//Max character definitions
static const CharFrame char_max_frame[] = {
	{Max_ArcMain_Max0, {    0,    0,  63, 112},{   7,  38}}, // 0 max0 1
	{Max_ArcMain_Max1, {    0,    0,  69, 113},{  13,  39}}, // 1 max1 1
	{Max_ArcMain_Max1, {   69,    0,  66, 113},{  12,  39}}, // 2 max1 2
	{Max_ArcMain_Max2, {    0,    0,  59, 111},{   1,  37}}, // 3 max2 1
	{Max_ArcMain_Max2, {   59,    0,  59, 111},{   2,  37}}, // 4 max2 2
	{Max_ArcMain_Max3, {    0,    0,  58, 120},{   3,  45}}, // 5 max3 1
	{Max_ArcMain_Max3, {   58,    0,  58, 118},{   3,  44}}, // 6 max3 2
	{Max_ArcMain_Max4, {    0,    0,  59, 106},{   7,  33}}, // 7 max4 1
	{Max_ArcMain_Max4, {   59,    0,  62, 107},{   8,  33}}, // 8 max4 2
};


static const Animation char_max_anim[CharAnim_Max] = {
	{1, (const u8[]){ 0, ASCR_BACK, 1}}, //CharAnim_Idle
	{2, (const u8[]){ 0, ASCR_REPEAT}}, //CharAnim_Left
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}}, //CharAnim_LeftAlt
	{2, (const u8[]){ 0, ASCR_REPEAT}}, //CharAnim_Down
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}}, //CharAnim_DownAlt
	{2, (const u8[]){ 0, ASCR_REPEAT}}, //CharAnim_Up
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}}, //CharAnim_UpAlt
	{2, (const u8[]){ 0, ASCR_REPEAT}}, //CharAnim_Right
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}}, //CharAnim_RightAlt
	{2, (const u8[]){ 0, ASCR_REPEAT}}, //CharAnim_UnGrow
	{2, (const u8[]){ 0, ASCR_REPEAT}}, //CharAnim_UnShrink
	{2, (const u8[]){ 0, ASCR_REPEAT}}, //CharAnim_Sing
};


//Max character functions
void Char_Max_SetFrame(void *user, u8 frame)
{
	Char_Max *this = (Char_Max*)user;
	
	//Check if this is a new frame
	if (frame != this->frame)
	{
		//Check if new art shall be loaded
		const CharFrame *cframe = &char_max_frame[this->frame = frame];
		if (cframe->tex != this->tex_id)
			Gfx_LoadTex(&this->tex, this->arc_ptr[this->tex_id = cframe->tex], 0);
	}
}

void Char_Max_Tick(Character *character)
{
	Char_Max *this = (Char_Max*)character;

	//Perform idle dance
	if ((character->pad_held & (INPUT_LEFT | INPUT_DOWN | INPUT_UP | INPUT_RIGHT)) == 0)
		Character_PerformIdle(character);

	//Animate and draw
	Animatable_Animate(&character->animatable, (void*)this, Char_Max_SetFrame);
	Character_Draw(character, &this->tex, &char_max_frame[this->frame]);
}

void Char_Max_SetAnim(Character *character, u8 anim)
{
	//Set animation
	Animatable_SetAnim(&character->animatable, anim);
	Character_CheckStartSing(character);
}

void Char_Max_Free(Character *character)
{
	Char_Max *this = (Char_Max*)character;
	
	//Free art
	Mem_Free(this->arc_main);
}

Character *Char_Max_New(fixed_t x, fixed_t y)
{
	//Allocate Max object
	Char_Max *this = Mem_Alloc(sizeof(Char_Max));
	if (this == NULL)
	{
		sprintf(error_msg, "[Char_Max_New] Failed to allocate Max object");
		ErrorLock();
		return NULL;
	}
	
	//Initialize character
	this->character.tick = Char_Max_Tick;
	this->character.set_anim = Char_Max_SetAnim;
	this->character.free = Char_Max_Free;
	
	Animatable_Init(&this->character.animatable, char_max_anim);
	Character_Init((Character*)this, x, y);
	
	//Set character information
	this->character.spec = 0;
	
	memcpy(this->character.health_i, char_max_icons, sizeof(char_max_icons));

	//health bar color
	this->character.health_bar = 0x00be2727;
	
	this->character.focus_x = FIXED_DEC(30.0,1);
	this->character.focus_y = FIXED_DEC(-70.0,1);
	this->character.focus_zoom = FIXED_DEC(40.0,100.0);
	
	    this->character.size = FIXED_DEC(100,100);
	
	//Load art
	this->arc_main = IO_Read("\\MAX\\MAX.ARC;1");
	
	const char **pathp = (const char *[]){
		"max0.tim",
		"max1.tim",
		"max2.tim",
		"max3.tim",
		"max4.tim",
		NULL
	};
	IO_Data *arc_ptr = this->arc_ptr;
	for (; *pathp != NULL; pathp++)
		*arc_ptr++ = Archive_Find(this->arc_main, *pathp);

	//Initialize render state
	this->tex_id = this->frame = 0xFF;
	
	return (Character*)this;
}

