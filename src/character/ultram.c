#include "ultram.h"

#include "../mem.h"
#include "../archive.h"
#include "../stage.h"
#include "../main.h"

//UltraM character structure
enum
{
	UltraM_ArcMain_UltraM0,
	UltraM_ArcMain_UltraM1,
	UltraM_ArcMain_UltraM2,
	UltraM_ArcMain_UltraM3,
	UltraM_ArcMain_UltraM4,
	UltraM_ArcMain_UltraM5,
	UltraM_ArcMain_UltraM6,
	UltraM_ArcMain_UltraM7,
	UltraM_ArcMain_UltraM8,
	UltraM_ArcMain_UltraM9,
	UltraM_ArcMain_UltraM10,
	UltraM_ArcMain_UltraM11,
	UltraM_ArcMain_UltraM12,
	UltraM_ArcMain_UltraM13,
	UltraM_ArcMain_UltraM14,
	UltraM_ArcMain_UltraM15,
	UltraM_ArcMain_UltraM16,
	UltraM_ArcMain_UltraM17,
	UltraM_ArcMain_UltraM18,
	UltraM_ArcMain_UltraM19,
	UltraM_ArcMain_UltraM20,
	UltraM_ArcMain_UltraM21,

	UltraM_Arc_Max,
};

typedef struct
{
	//Character base structure
	Character character;
	
	//Render data and state
	IO_Data arc_main;
	IO_Data arc_ptr[UltraM_Arc_Max];
	
	Gfx_Tex tex;
	u8 frame, tex_id;
} Char_UltraM;

//UltraM character definitions
static const u16 char_ultram_icons[2][4] = {
	{72,0,36,36},
	{108,0,36,36}
};

static const CharFrame char_ultram_frame[] = {
	{UltraM_ArcMain_UltraM0,{0,0,117,196},{54,196}}, //0 Idle
	{UltraM_ArcMain_UltraM0,{117,0,114,197},{52,197}}, //1 Idle
	{UltraM_ArcMain_UltraM1,{0,0,112,198},{50,198}}, //2 Idle
	{UltraM_ArcMain_UltraM1,{112,0,111,198},{51,198}}, //3 Idle
	{UltraM_ArcMain_UltraM2,{0,0,112,198},{54,198}}, //4 Idle
	{UltraM_ArcMain_UltraM2,{112,0,117,195},{57,196}}, //5 Idle
	{UltraM_ArcMain_UltraM3,{0,0,122,192},{62,192}}, //6 Idle
	{UltraM_ArcMain_UltraM3,{122,0,124,193},{63,194}}, //7 Idle
	{UltraM_ArcMain_UltraM4,{0,0,122,194},{62,195}}, //8 Idle
	{UltraM_ArcMain_UltraM4,{122,0,121,194},{61,195}}, //9 Idle
	
	{UltraM_ArcMain_UltraM5,{0,0,156,194},{152,194}}, //10 Left
	{UltraM_ArcMain_UltraM6,{0,0,152,192},{153,192}}, //11 Left
	{UltraM_ArcMain_UltraM7,{0,0,150,187},{152,187}}, //12 Left
	{UltraM_ArcMain_UltraM8,{0,0,172,179},{160,178}}, //13 Left
	
	{UltraM_ArcMain_UltraM9,{0,0,228,173},{40,168}}, //14 Right
	{UltraM_ArcMain_UltraM10,{0,0,226,177},{40,171}}, //15 Right
	{UltraM_ArcMain_UltraM11,{0,0,214,189},{40,183}}, //16 Right
	{UltraM_ArcMain_UltraM12,{0,0,195,206},{41,199}}, //17 Right
	
	{UltraM_ArcMain_UltraM13,{0,0,166,174},{115,173}}, //18 Down
	{UltraM_ArcMain_UltraM14,{0,0,163,174},{113,173}}, //19 Down
	{UltraM_ArcMain_UltraM15,{0,0,152,174},{103,173}}, //20 Down
	{UltraM_ArcMain_UltraM16,{0,0,171,171},{95,169}}, //21 Down
	{UltraM_ArcMain_UltraM17,{0,0,171,171},{95,169}}, //22 Down
	
	{UltraM_ArcMain_UltraM18,{0,0,163,248},{105,247}}, //23 Up
	{UltraM_ArcMain_UltraM19,{0,0,168,247},{106,246}}, //24 Up
	{UltraM_ArcMain_UltraM20,{0,0,175,237},{110,236}}, //25 Up
	{UltraM_ArcMain_UltraM21,{0,0,197,211},{122,209}}, //26 Up
};

static const Animation char_ultram_anim[CharAnim_Max] = {
	{2, (const u8[]){ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,  ASCR_CHGANI, CharAnim_Idle}},		//CharAnim_Idle
	{2, (const u8[]){ 10, 11, 12, 13,  ASCR_BACK, 2}},		//CharAnim_Left
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}},		//CharAnim_LeftAlt
	{2, (const u8[]){ 18, 19, 20, 21, 22,  ASCR_BACK, 2}},		//CharAnim_Down
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}},		//CharAnim_DownAlt
	{2, (const u8[]){ 23, 24, 25, 26,  ASCR_BACK, 2}},		//CharAnim_Up
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}},		//CharAnim_UpAlt
	{2, (const u8[]){ 14, 15, 16, 17,  ASCR_BACK, 2}},		//CharAnim_Right
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}},		//CharAnim_RightAlt
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}},		//CharAnim_UnGrow
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}},		//CharAnim_UnShrink
	{0, (const u8[]){ASCR_CHGANI, CharAnim_Idle}},		//CharAnim_Sing
};

//UltraM character functions
void Char_UltraM_SetFrame(void *user, u8 frame)
{
	Char_UltraM *this = (Char_UltraM*)user;
	
	//Check if this is a new frame
	if (frame != this->frame)
	{
		//Check if new art shall be loaded
		const CharFrame *cframe = &char_ultram_frame[this->frame = frame];
		if (cframe->tex != this->tex_id)
			Gfx_LoadTex(&this->tex, this->arc_ptr[this->tex_id = cframe->tex], 0);
	}
}

void Char_UltraM_Tick(Character *character)
{
	Char_UltraM *this = (Char_UltraM*)character;
	
	//Perform idle dance
	if ((character->pad_held & (INPUT_LEFT | INPUT_DOWN | INPUT_UP | INPUT_RIGHT)) == 0)
		Character_PerformIdle(character);
	
	//Animate and draw
	Animatable_Animate(&character->animatable, (void*)this, Char_UltraM_SetFrame);
	Character_Draw(character, &this->tex, &char_ultram_frame[this->frame]);
}

void Char_UltraM_SetAnim(Character *character, u8 anim)
{
	//Set animation
	Animatable_SetAnim(&character->animatable, anim);
	Character_CheckStartSing(character);
}

void Char_UltraM_Free(Character *character)
{
	Char_UltraM *this = (Char_UltraM*)character;
	
	//Free art
	Mem_Free(this->arc_main);
}

Character *Char_UltraM_New(fixed_t x, fixed_t y, fixed_t scale)
{
	//Allocate ultram object
	Char_UltraM *this = Mem_Alloc(sizeof(Char_UltraM));
	if (this == NULL)
	{
		sprintf(error_msg, "[Char_UltraM_New] Failed to allocate ultram object");
		ErrorLock();
		return NULL;
	}
	
	//Initialize character
	this->character.tick = Char_UltraM_Tick;
	this->character.set_anim = Char_UltraM_SetAnim;
	this->character.free = Char_UltraM_Free;
	
	Animatable_Init(&this->character.animatable, char_ultram_anim);
	Character_Init((Character*)this, x, y);
	
	//Set character information
	this->character.spec = 0;
	
	memcpy(this->character.health_i, char_ultram_icons, sizeof(char_ultram_icons));
	
	//health bar color
	this->character.health_bar = 0xFFD02537;
	
	this->character.focus_x = FIXED_DEC(-40,1);
	this->character.focus_y = FIXED_DEC(-90,1);
	this->character.focus_zoom = FIXED_DEC(65,100);
	
	this->character.size = FIXED_MUL(FIXED_DEC(222,100),scale);
	
	//Load art
	this->arc_main = IO_Read("\\OCHAR\\ULTRAM.ARC;1");
	
	const char **pathp = (const char *[]){
		"ultram0.tim",
		"ultram1.tim",
		"ultram2.tim",
		"ultram3.tim",
		"ultram4.tim",
		"ultram5.tim",
		"ultram6.tim",
		"ultram7.tim",
		"ultram8.tim",
		"ultram9.tim",
		"ultram10.tim",
		"ultram11.tim",
		"ultram12.tim",
		"ultram13.tim",
		"ultram14.tim",
		"ultram15.tim",
		"ultram16.tim",
		"ultram17.tim",
		"ultram18.tim",
		"ultram19.tim",
		"ultram20.tim",
		"ultram21.tim",

		NULL
	};
	IO_Data *arc_ptr = this->arc_ptr;
	for (; *pathp != NULL; pathp++)
		*arc_ptr++ = Archive_Find(this->arc_main, *pathp);
	
	//Initialize render state
	this->tex_id = this->frame = 0xFF;
	
	return (Character*)this;
}
