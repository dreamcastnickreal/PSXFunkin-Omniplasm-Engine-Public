/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "nug.h"


#include "../mem.h"
#include "../archive.h"
#include "../stage.h"
#include "../main.h"

enum
{
	Nug_ArcMain_Nug0,
	Nug_ArcMain_Nug1,
	Nug_ArcMain_Nug2,
	Nug_ArcMain_Nug3,
	Nug_ArcMain_Nug4,

	Nug_Arc_Max,
};


typedef struct
{
	//Character base structure
	Character character;
	
	//Render data and state
	IO_Data arc_main;
	IO_Data arc_ptr[Nug_Arc_Max];

	Gfx_Tex tex;
	u8 frame, tex_id;
} Char_Nug;


static const u16 char_nug_icons[2][4] = {
	{0,0,64,64},
	{64,0,64,64}
};


//Nug character definitions
static const CharFrame char_nug_frame[] = {
	{Nug_ArcMain_Nug0, {    0,    0,  54,  72},{   8,  23}}, // 0 nug0 1
	{Nug_ArcMain_Nug0, {   54,    0,  54,  72},{   8,  23}}, // 1 nug0 2
	{Nug_ArcMain_Nug1, {    0,    0,  56,  71},{   2,  22}}, // 2 nug1 1
	{Nug_ArcMain_Nug1, {   56,    0,  55,  71},{   3,  23}}, // 3 nug1 2
	{Nug_ArcMain_Nug2, {    0,    0,  51,  79},{   4,  30}}, // 4 nug2 1
	{Nug_ArcMain_Nug2, {   51,    0,  53,  77},{   4,  29}}, // 5 nug2 2
	{Nug_ArcMain_Nug3, {    0,    0,  55,  68},{   5,  20}}, // 6 nug3 1
	{Nug_ArcMain_Nug3, {   55,    0,  54,  69},{   4,  20}}, // 7 nug3 2
	{Nug_ArcMain_Nug4, {    0,    0,  55,  71},{   5,  23}}, // 8 nug4 1
	{Nug_ArcMain_Nug4, {   55,    0,  54,  72},{   5,  23}}, // 9 nug4 2
};


static const Animation char_nug_anim[CharAnim_Max] = {
	{1, (const u8[]){ 8, 9, ASCR_REPEAT}}, //CharAnim_Idle
	{2, (const u8[]){ 8, 9, ASCR_REPEAT}}, //CharAnim_Left
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}}, //CharAnim_LeftAlt
	{2, (const u8[]){ 8, 9, ASCR_REPEAT}}, //CharAnim_Down
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}}, //CharAnim_DownAlt
	{2, (const u8[]){ 8, 9, ASCR_REPEAT}}, //CharAnim_Up
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}}, //CharAnim_UpAlt
	{2, (const u8[]){ 8, 9, ASCR_REPEAT}}, //CharAnim_Right
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}}, //CharAnim_RightAlt
	{2, (const u8[]){ 8, 9, ASCR_REPEAT}}, //CharAnim_UnGrow
	{2, (const u8[]){ 8, 9, ASCR_REPEAT}}, //CharAnim_UnShrink
	{2, (const u8[]){ 8, 9, ASCR_REPEAT}}, //CharAnim_Sing
};


//Nug character functions
void Char_Nug_SetFrame(void *user, u8 frame)
{
	Char_Nug *this = (Char_Nug*)user;
	
	//Check if this is a new frame
	if (frame != this->frame)
	{
		//Check if new art shall be loaded
		const CharFrame *cframe = &char_nug_frame[this->frame = frame];
		if (cframe->tex != this->tex_id)
			Gfx_LoadTex(&this->tex, this->arc_ptr[this->tex_id = cframe->tex], 0);
	}
}

void Char_Nug_Tick(Character *character)
{
	Char_Nug *this = (Char_Nug*)character;

	//Perform idle dance
	if ((character->pad_held & (INPUT_LEFT | INPUT_DOWN | INPUT_UP | INPUT_RIGHT)) == 0)
		Character_PerformIdle(character);

	//Animate and draw
	Animatable_Animate(&character->animatable, (void*)this, Char_Nug_SetFrame);
	Character_Draw(character, &this->tex, &char_nug_frame[this->frame]);
}

void Char_Nug_SetAnim(Character *character, u8 anim)
{
	//Set animation
	Animatable_SetAnim(&character->animatable, anim);
	Character_CheckStartSing(character);
}

void Char_Nug_Free(Character *character)
{
	Char_Nug *this = (Char_Nug*)character;
	
	//Free art
	Mem_Free(this->arc_main);
}

Character *Char_Nug_New(fixed_t x, fixed_t y)
{
	//Allocate Nug object
	Char_Nug *this = Mem_Alloc(sizeof(Char_Nug));
	if (this == NULL)
	{
		sprintf(error_msg, "[Char_Nug_New] Failed to allocate Nug object");
		ErrorLock();
		return NULL;
	}
	
	//Initialize character
	this->character.tick = Char_Nug_Tick;
	this->character.set_anim = Char_Nug_SetAnim;
	this->character.free = Char_Nug_Free;
	
	Animatable_Init(&this->character.animatable, char_nug_anim);
	Character_Init((Character*)this, x, y);
	
	//Set character information
	this->character.spec = 0;
	
	memcpy(this->character.health_i, char_nug_icons, sizeof(char_nug_icons));

	//health bar color
	this->character.health_bar = 0x00be2727;
	
	this->character.focus_x = FIXED_DEC(-40.0,1);
	this->character.focus_y = FIXED_DEC(-40.0,1);
	this->character.focus_zoom = FIXED_DEC(40.0,100.0);
	
	    this->character.size = FIXED_DEC(100,100);
	
	//Load art
	this->arc_main = IO_Read("\\NUG\\NUG.ARC;1");
	
	const char **pathp = (const char *[]){
		"nug0.tim",
		"nug1.tim",
		"nug2.tim",
		"nug3.tim",
		"nug4.tim",
		NULL
	};
	IO_Data *arc_ptr = this->arc_ptr;
	for (; *pathp != NULL; pathp++)
		*arc_ptr++ = Archive_Find(this->arc_main, *pathp);

	//Initialize render state
	this->tex_id = this->frame = 0xFF;
	
	return (Character*)this;
}

