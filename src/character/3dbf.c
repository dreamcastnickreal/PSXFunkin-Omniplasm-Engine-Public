/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "3dbf.h"

#include "../mem.h"
#include "../archive.h"
#include "../stage.h"
#include "../main.h"

//3DBF player types
enum
{
	TDBF_ArcMain_3DBF0,
	TDBF_ArcMain_3DBF1,
	TDBF_ArcMain_3DBF2,
	TDBF_ArcMain_3DBF3,
	TDBF_ArcMain_3DBF4,
	TDBF_ArcMain_3DBF5,
	TDBF_ArcMain_3DBF6,
	TDBF_ArcMain_3DBF7,
	TDBF_ArcMain_3DBF8,
	TDBF_ArcMain_Dead0,
	TDBF_ArcMain_Dead1,
	TDBF_ArcMain_Dead2,
	TDBF_ArcMain_Dead3,
	TDBF_ArcMain_Dead4,
	TDBF_ArcMain_Dead5,
	TDBF_ArcMain_Dead6,
	TDBF_ArcMain_Dead7,
	TDBF_ArcMain_Dead8,
	TDBF_ArcMain_Dead9,
	TDBF_ArcMain_Dead10,
	TDBF_ArcMain_Dead11,
	TDBF_ArcMain_Dead12,
	TDBF_ArcMain_Dead13,
	TDBF_ArcMain_Retry,
	
	TDBF_ArcMain_Max,
};

typedef struct
{
	//Character base structure
	Character character;
	
	//Render data and state (main ARC holds sing frames, dead ARC holds
	//Death0/Death1/Death2 frames; both resident for the simple scheme)
	IO_Data arc_main, arc_dead;
	IO_Data arc_ptr[TDBF_ArcMain_Max];
	
	Gfx_Tex tex, tex_retry;
	u8 frame, tex_id;
	
	u8 retry_bump;
} Char_3DBF;

static const u16 char_3dbf_icons[2][4] = {
	{72,216,36,36},
	{108,216,36,36}
};

//3DBF player definitions
static const CharFrame char_3dbf_frame[] = {
	{TDBF_ArcMain_3DBF0,{0,0,87,108},{15,29+70}}, //0 Idle
	{TDBF_ArcMain_3DBF0,{87,0,87,108},{15,29+70}}, //1 Idle
	{TDBF_ArcMain_3DBF0,{0,108,90,108},{15,29+70}}, //2 Idle
	{TDBF_ArcMain_3DBF0,{90,108,92,109},{15,30+70}}, //3 Idle
	
	{TDBF_ArcMain_3DBF1,{0,0,86,112},{20,33+70}}, //4 Left
	{TDBF_ArcMain_3DBF1,{86,0,86,113},{20,34+70}}, //5 Left
	{TDBF_ArcMain_3DBF1,{0,113,85,112},{20,33+70}}, //6 Left
	{TDBF_ArcMain_3DBF1,{85,113,91,111},{20,32+70}}, //7 Left
	
	{TDBF_ArcMain_3DBF2,{0,0,93,102},{23,24+70}}, //8 Down
	{TDBF_ArcMain_3DBF2,{93,0,94,100},{24,22+70}}, //9 Down
	{TDBF_ArcMain_3DBF2,{0,102,90,103},{19,25+70}}, //10 Down
	{TDBF_ArcMain_3DBF2,{90,102,87,106},{15,27+70}}, //11 Down
	
	{TDBF_ArcMain_3DBF3,{0,0,89,126},{15,47+70}}, //12 Up
	{TDBF_ArcMain_3DBF3,{89,0,89,126},{16,47+70}}, //13 Up
	{TDBF_ArcMain_3DBF3,{0,126,90,127},{18,47+70}}, //14 Up
	{TDBF_ArcMain_3DBF3,{90,126,90,120},{20,40+70}}, //15 Up
	
	{TDBF_ArcMain_3DBF4,{0,0,104,111},{15,32+70}}, //16 Right
	{TDBF_ArcMain_3DBF4,{104,0,104,112},{15,33+70}}, //17 Right
	{TDBF_ArcMain_3DBF4,{0,112,102,108},{15,29+70}}, //18 Right
	{TDBF_ArcMain_3DBF4,{102,112,100,108},{15,29+70}}, //19 Right
	
	{TDBF_ArcMain_3DBF5,{0,0,89,112},{32,32+70}}, //20 LeftMiss
	{TDBF_ArcMain_3DBF5,{89,0,88,113},{32,33+70}}, //21 LeftMiss
	
	{TDBF_ArcMain_3DBF6,{0,0,87,136},{15,56+70}}, //22 DownMiss
	{TDBF_ArcMain_3DBF6,{88,0,82,126},{14,46+70}}, //23 DownMiss
	
	{TDBF_ArcMain_3DBF7,{0,0,103,128},{30,47+70}}, //24 UpMiss
	{TDBF_ArcMain_3DBF7,{103,0,101,123},{27,42+70}}, //25 UpMiss
	
	{TDBF_ArcMain_3DBF8,{0,0,141,106},{43,25+70}}, //26 RightMiss
	{TDBF_ArcMain_3DBF8,{0,106,141,109},{48,28+70}}, //27 RightMiss
	
	{TDBF_ArcMain_Dead0,{0,0,81,96},{16,16+70}}, //28 FirstDeath
	{TDBF_ArcMain_Dead0,{81,0,86,96},{20,16+70}}, //29 FirstDeath
	{TDBF_ArcMain_Dead0,{167,0,84,96},{17,16+70}}, //30 FirstDeath
	{TDBF_ArcMain_Dead0,{0,96,102,96},{32,16+70}}, //31 FirstDeath
	{TDBF_ArcMain_Dead0,{102,96,102,96},{32,16+70}}, //32 FirstDeath
	{TDBF_ArcMain_Dead1,{0,0,104,95},{32,15+70}}, //33 FirstDeath
	{TDBF_ArcMain_Dead1,{104,0,104,95},{33,15+70}}, //34 FirstDeath
	{TDBF_ArcMain_Dead1,{0,95,102,95},{32,15+70}}, //35 FirstDeath
	{TDBF_ArcMain_Dead1,{102,95,101,95},{31,15+70}}, //36 FirstDeath
	{TDBF_ArcMain_Dead2,{0,0,103,96},{33,15+70}}, //37 FirstDeath
	{TDBF_ArcMain_Dead2,{103,0,103,95},{33,15+70}}, //38 FirstDeath
	{TDBF_ArcMain_Dead2,{0,96,103,95},{33,15+70}}, //39 FirstDeath
	{TDBF_ArcMain_Dead2,{103,96,103,95},{33,15+70}}, //40 FirstDeath
	{TDBF_ArcMain_Dead3,{0,0,103,95},{33,15+70}}, //41 FirstDeath
	{TDBF_ArcMain_Dead3,{103,0,103,95},{33,15+70}}, //42 FirstDeath
	{TDBF_ArcMain_Dead3,{0,95,103,95},{33,15+70}}, //43 FirstDeath
	{TDBF_ArcMain_Dead3,{103,95,103,95},{33,15+70}}, //44 FirstDeath
	{TDBF_ArcMain_Dead4,{0,0,103,95},{33,15+70}}, //45 FirstDeath
	{TDBF_ArcMain_Dead4,{103,0,103,95},{33,15+70}}, //46 FirstDeath
	{TDBF_ArcMain_Dead4,{0,95,103,95},{33,15+70}}, //47 FirstDeath
	{TDBF_ArcMain_Dead4,{103,95,103,95},{33,15+70}}, //48 FirstDeath
	{TDBF_ArcMain_Dead5,{0,0,103,95},{33,15+70}}, //49 FirstDeath
	{TDBF_ArcMain_Dead5,{103,0,103,95},{33,15+70}}, //50 FirstDeath
	{TDBF_ArcMain_Dead5,{0,95,103,95},{33,15+70}}, //51 FirstDeath
	{TDBF_ArcMain_Dead5,{103,95,103,95},{33,15+70}}, //52 FirstDeath
	{TDBF_ArcMain_Dead6,{0,0,103,95},{33,15+70}}, //53 FirstDeath
	{TDBF_ArcMain_Dead6,{103,0,103,95},{33,15+70}}, //54 FirstDeath
	{TDBF_ArcMain_Dead6,{0,95,103,95},{33,15+70}}, //55 FirstDeath
	{TDBF_ArcMain_Dead6,{103,95,103,95},{33,15+70}}, //56 FirstDeath
	
	{TDBF_ArcMain_Dead7,{0,0,103,95},{33,15+70}}, //57 DeathLoop
	{TDBF_ArcMain_Dead7,{103,0,103,95},{33,15+70}}, //58 DeathLoop
	{TDBF_ArcMain_Dead7,{0,95,103,95},{33,15+70}}, //59 DeathLoop
	{TDBF_ArcMain_Dead7,{103,95,103,95},{33,15+70}}, //60 DeathLoop
	{TDBF_ArcMain_Dead8,{0,0,103,95},{33,15+70}}, //61 DeathLoop
	
	{TDBF_ArcMain_Dead9,{0,0,103,95},{33,15+70}}, //62 DeathConfirm
	{TDBF_ArcMain_Dead9,{103,0,103,103},{33,23+70}}, //63 DeathConfirm
	{TDBF_ArcMain_Dead9,{0,103,103,110},{33,30+70}}, //64 DeathConfirm
	{TDBF_ArcMain_Dead9,{103,103,103,102},{33,22+70}}, //65 DeathConfirm
	{TDBF_ArcMain_Dead10,{0,0,103,102},{33,22+70}}, //66 DeathConfirm
	{TDBF_ArcMain_Dead10,{103,0,103,101},{33,21+70}}, //67 DeathConfirm
	{TDBF_ArcMain_Dead10,{0,102,103,100},{33,20+70}}, //68 DeathConfirm
	{TDBF_ArcMain_Dead10,{103,102,103,100},{33,20+70}}, //69 DeathConfirm
	{TDBF_ArcMain_Dead11,{0,0,103,100},{33,20+70}}, //70 DeathConfirm
	{TDBF_ArcMain_Dead11,{103,0,103,100},{33,20+70}}, //71 DeathConfirm
	{TDBF_ArcMain_Dead11,{0,100,103,98},{33,18+70}}, //72 DeathConfirm
	{TDBF_ArcMain_Dead11,{103,100,103,98},{33,18+70}}, //73 DeathConfirm
	{TDBF_ArcMain_Dead12,{0,0,103,96},{33,16+70}}, //74 DeathConfirm
	{TDBF_ArcMain_Dead12,{103,0,103,96},{33,16+70}}, //75 DeathConfirm
	{TDBF_ArcMain_Dead12,{0,96,103,95},{33,15+70}}, //76 DeathConfirm
	{TDBF_ArcMain_Dead12,{103,96,103,95},{33,15+70}}, //77 DeathConfirm
	{TDBF_ArcMain_Dead13,{0,0,103,95},{33,15+70}}, //78 DeathConfirm
};

static const Animation char_3dbf_anim[PlayerAnim_Max] = {
	{2, (const u8[]){ 0,  1,  2,  3, ASCR_BACK, 1}},    //CharAnim_Idle
	{2, (const u8[]){ 4,  5,  6,  7, ASCR_BACK, 1}}, //CharAnim_Left
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}},   //CharAnim_LeftAlt
	{2, (const u8[]){ 8,  9, 10, 11, ASCR_BACK, 1}}, //CharAnim_Down
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}},   //CharAnim_DownAlt
	{2, (const u8[]){12, 13, 14, 15, ASCR_BACK, 1}}, //CharAnim_Up
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}},   //CharAnim_UpAlt
	{2, (const u8[]){16, 17, 18, 19, ASCR_BACK, 1}}, //CharAnim_Right
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}},   //CharAnim_RightAlt
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}},   //CharAnim_UnGrow
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}},   //CharAnim_UnShrink
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}},   //CharAnim_Sing
	
	{1, (const u8[]){ 4, 20, 20, 21, ASCR_BACK, 1}}, //PlayerAnim_LeftMiss
	{1, (const u8[]){ 8, 22, 22, 23, ASCR_BACK, 1}}, //PlayerAnim_DownMiss
	{1, (const u8[]){12, 24, 24, 25, ASCR_BACK, 1}}, //PlayerAnim_UpMiss
	{1, (const u8[]){16, 26, 26, 27, ASCR_BACK, 1}}, //PlayerAnim_RightMiss
	
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}},   //PlayerAnim_Peace
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}},   //PlayerAnim_Sweat
	
	{0, (const u8[]){ASCR_CHGANI, PlayerAnim_Death0}}, //PlayerAnim_Dead0 (legacy, unused)
	{0, (const u8[]){ASCR_CHGANI, PlayerAnim_Death1}}, //PlayerAnim_Dead1 (legacy, unused)
	{0, (const u8[]){ASCR_CHGANI, PlayerAnim_Death1}}, //PlayerAnim_Dead2 (legacy, unused)
	{0, (const u8[]){ASCR_CHGANI, PlayerAnim_Death1}}, //PlayerAnim_Dead3 (legacy, unused)
	{0, (const u8[]){ASCR_CHGANI, PlayerAnim_Death1}}, //PlayerAnim_Dead4 (legacy, unused)
	{0, (const u8[]){ASCR_CHGANI, PlayerAnim_Death1}}, //PlayerAnim_Dead5 (legacy, unused)
	{0, (const u8[]){ASCR_CHGANI, PlayerAnim_Death1}}, //PlayerAnim_Dead6 (legacy, unused)
	{0, (const u8[]){ASCR_CHGANI, PlayerAnim_Death1}}, //PlayerAnim_Dead7 (legacy, unused)
	
	{2, (const u8[]){28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	                 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
	                 54, 55, 56, ASCR_CHGANI, PlayerAnim_Death1}}, //PlayerAnim_Death0
	{2, (const u8[]){57, 58, 59, 60, 61, ASCR_LOOP}},           //PlayerAnim_Death1
	{2, (const u8[]){62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72,
	                 73, 74, 75, 76, 77, 78, ASCR_BACK, 1}},      //PlayerAnim_Death2
};

//3DBF player functions
void Char_3DBF_SetFrame(void *user, u8 frame)
{
	Char_3DBF *this = (Char_3DBF*)user;
	
	//Check if this is a new frame
	if (frame != this->frame)
	{
		//Check if new art shall be loaded
		const CharFrame *cframe = &char_3dbf_frame[this->frame = frame];
		if (cframe->tex != this->tex_id)
			Gfx_LoadTex(&this->tex, this->arc_ptr[this->tex_id = cframe->tex], 0);
	}
}

void Char_3DBF_Tick(Character *character)
{
	Char_3DBF *this = (Char_3DBF*)character;
	
	//Handle animation updates
	if ((character->pad_held & (INPUT_LEFT | INPUT_DOWN | INPUT_UP | INPUT_RIGHT)) == 0 ||
	    (character->animatable.anim != CharAnim_Left &&
	     character->animatable.anim != CharAnim_LeftAlt &&
	     character->animatable.anim != CharAnim_Down &&
	     character->animatable.anim != CharAnim_DownAlt &&
	     character->animatable.anim != CharAnim_Up &&
	     character->animatable.anim != CharAnim_UpAlt &&
	     character->animatable.anim != CharAnim_Right &&
	     character->animatable.anim != CharAnim_RightAlt))
		Character_CheckEndSing(character);
	
	if (stage.flag & STAGE_FLAG_JUST_STEP)
	{
		//Perform idle dance
		if (Animatable_Ended(&character->animatable) &&
			(character->animatable.anim != CharAnim_Left &&
		     character->animatable.anim != CharAnim_LeftAlt &&
		     character->animatable.anim != PlayerAnim_LeftMiss &&
		     character->animatable.anim != CharAnim_Down &&
		     character->animatable.anim != CharAnim_DownAlt &&
		     character->animatable.anim != PlayerAnim_DownMiss &&
		     character->animatable.anim != CharAnim_Up &&
		     character->animatable.anim != CharAnim_UpAlt &&
		     character->animatable.anim != PlayerAnim_UpMiss &&
		     character->animatable.anim != CharAnim_Right &&
		     character->animatable.anim != CharAnim_RightAlt &&
		     character->animatable.anim != PlayerAnim_RightMiss) &&
			(stage.song_step & 0x7) == 0)
			character->set_anim(character, CharAnim_Idle);
	}
	
	//Retry screen prompt while Death1 loops or Death2 confirms
	//(submitted before the sprite so it draws in front of it)
	if (character->animatable.anim == PlayerAnim_Death1 ||
	    character->animatable.anim == PlayerAnim_Death2)
	{
		//Draw input options
		RECT button_src = {
			 0, 96,
			16, 16
		};
		RECT_FIXED button_dst = {
			character->x - FIXED_DEC(32,1) - stage.camera.x,
			character->y - FIXED_DEC(88,1) - stage.camera.y,
			FIXED_DEC(16,1),
			FIXED_DEC(16,1),
		};
		
		//Cross - Retry
		Stage_DrawTex(&this->tex_retry, &button_src, &button_dst, FIXED_MUL(stage.camera.zoom, stage.bump), stage.camera.angle);
		
		//Circle - Blueball
		button_src.x = 16;
		button_dst.y += FIXED_DEC(56,1);
		Stage_DrawTex(&this->tex_retry, &button_src, &button_dst, FIXED_MUL(stage.camera.zoom, stage.bump), stage.camera.angle);
		
		//Draw 'RETRY' (idle blink)
		u8 retry_frame = 1 + (this->retry_bump >> 2);
		if (retry_frame >= 3)
			retry_frame = 0;
		
		if (++this->retry_bump >= 55)
			this->retry_bump = 0;
		
		RECT retry_src = {
			(retry_frame & 1) ? 48 : 0,
			(retry_frame >> 1) << 5,
			48,
			32
		};
		RECT_FIXED retry_dst = {
			character->x -  FIXED_DEC(7,1) - stage.camera.x,
			character->y - FIXED_DEC(92,1) - stage.camera.y,
			FIXED_DEC(48,1),
			FIXED_DEC(32,1),
		};
		Stage_DrawTex(&this->tex_retry, &retry_src, &retry_dst, FIXED_MUL(stage.camera.zoom, stage.bump), stage.camera.angle);
	}
	
	//Animate and draw character
	Animatable_Animate(&character->animatable, (void*)this, Char_3DBF_SetFrame);
	Character_Draw(character, &this->tex, &char_3dbf_frame[this->frame]);
}

void Char_3DBF_SetAnim(Character *character, u8 anim)
{
	//Perform animation checks
	switch (anim)
	{
		case PlayerAnim_Death0:
			//Begin adjusting focus
			character->focus_x = FIXED_DEC(0,1);
			character->focus_y = FIXED_DEC(-40,1);
			character->focus_zoom = FIXED_DEC(125,100);
			break;
	}
	
	//Set animation
	Animatable_SetAnim(&character->animatable, anim);
	Character_CheckStartSing(character);
}

void Char_3DBF_Free(Character *character)
{
	Char_3DBF *this = (Char_3DBF*)character;
	
	//Free art
	Mem_Free(this->arc_main);
	Mem_Free(this->arc_dead);
}

Character *Char_3DBF_New(fixed_t x, fixed_t y, fixed_t scale)
{
	//Allocate 3DBF object
	Char_3DBF *this = Mem_Alloc(sizeof(Char_3DBF));
	if (this == NULL)
	{
		sprintf(error_msg, "[Char_3DBF_New] Failed to allocate 3DBF object");
		ErrorLock();
		return NULL;
	}
	
	//Initialize character
	this->character.tick = Char_3DBF_Tick;
	this->character.set_anim = Char_3DBF_SetAnim;
	this->character.free = Char_3DBF_Free;
	
	Animatable_Init(&this->character.animatable, char_3dbf_anim);
	Character_Init((Character*)this, x, y);
	
	//Set character information
	this->character.spec = CHAR_SPEC_MISSANIM;
	this->character.death_simple = true;
	
	memcpy(this->character.health_i, char_3dbf_icons, sizeof(char_3dbf_icons));

	//health bar color
	this->character.health_bar = 0xFF29B5D6;
	
	this->character.focus_x = FIXED_DEC(-50,1);
	this->character.focus_y = FIXED_DEC(-65,1);
	this->character.focus_zoom = FIXED_DEC(1,1);
	
	this->character.size = FIXED_MUL(FIXED_DEC(100,100),scale);
	
	//Load art
	this->arc_main = IO_Read("\\PCHAR\\3DBF.ARC;1");
	this->arc_dead = IO_Read("\\PCHAR\\3DDEAD.ARC;1");
	
	const char **pathp = (const char *[]){
		"3dbf0.tim", //TDBF_ArcMain_3DBF0
		"3dbf1.tim", //TDBF_ArcMain_3DBF1
		"3dbf2.tim", //TDBF_ArcMain_3DBF2
		"3dbf3.tim", //TDBF_ArcMain_3DBF3
		"3dbf4.tim", //TDBF_ArcMain_3DBF4
		"3dbf5.tim", //TDBF_ArcMain_3DBF5
		"3dbf6.tim", //TDBF_ArcMain_3DBF6
		"3dbf7.tim", //TDBF_ArcMain_3DBF7
		"3dbf8.tim", //TDBF_ArcMain_3DBF8
		NULL
	};
	IO_Data *arc_ptr = this->arc_ptr;
	for (; *pathp != NULL; pathp++)
		*arc_ptr++ = Archive_Find(this->arc_main, *pathp);
	
	pathp = (const char *[]){
		"dead0.tim",  //TDBF_ArcMain_Dead0
		"dead1.tim",  //TDBF_ArcMain_Dead1
		"dead2.tim",  //TDBF_ArcMain_Dead2
		"dead3.tim",  //TDBF_ArcMain_Dead3
		"dead4.tim",  //TDBF_ArcMain_Dead4
		"dead5.tim",  //TDBF_ArcMain_Dead5
		"dead6.tim",  //TDBF_ArcMain_Dead6
		"dead7.tim",  //TDBF_ArcMain_Dead7
		"dead8.tim",  //TDBF_ArcMain_Dead8
		"dead9.tim",  //TDBF_ArcMain_Dead9
		"dead10.tim", //TDBF_ArcMain_Dead10
		"dead11.tim", //TDBF_ArcMain_Dead11
		"dead12.tim", //TDBF_ArcMain_Dead12
		"dead13.tim", //TDBF_ArcMain_Dead13
		"retry.tim",  //TDBF_ArcMain_Retry
		NULL
	};
	for (; *pathp != NULL; pathp++)
		*arc_ptr++ = Archive_Find(this->arc_dead, *pathp);
	
	//Load retry art from its table slot (bf-layout prompt: 48x32 RETRY +
	//16x16 buttons; flag must be 0 so pixels + CLUT actually upload)
	Gfx_LoadTex(&this->tex_retry, this->arc_ptr[TDBF_ArcMain_Retry], 0);
	
	//Initialize render state
	this->tex_id = this->frame = 0xFF;
	this->retry_bump = 0;
	
	//FlxTrail-style trail (inactive by default, auto VRAM on first capture)
	Character_TrailConfigure((Character*)this, true, -1, -1, 4, 24,
		FIXED_DEC(3,10), FIXED_DEC(69,1000));
	Character_GhostSetNoHealthbarColor((Character*)this, true);
	Character_TrailSetActive((Character*)this, false);
	
	//Death sounds into this player's own bank (missing files are skipped)
	Character_LoadVagSound((Character*)this, CHARACTER_VAG_DEATH0, "\\SOUNDS\\DEATH0.VAG;1");
	Character_LoadVagSound((Character*)this, CHARACTER_VAG_DEATH2, "\\SOUNDS\\DEATH2.VAG;1");
	
	//Miss sounds into this player's own bank (a random one plays per miss)
	Character_LoadVagSound((Character*)this, CHARACTER_VAG_MISS0, "\\SOUNDS\\MISS1.VAG;1");
	Character_LoadVagSound((Character*)this, CHARACTER_VAG_MISS1, "\\SOUNDS\\MISS2.VAG;1");
	Character_LoadVagSound((Character*)this, CHARACTER_VAG_MISS2, "\\SOUNDS\\MISS3.VAG;1");
	
	return (Character*)this;
}
