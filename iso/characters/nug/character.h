enum
{
	Nug_ArcMain_nug0,
	Nug_ArcMain_nug1,
	Nug_ArcMain_nug2,
	Nug_ArcMain_nug3,
	Nug_ArcMain_nug4,

	Nug_Arc_Max,
};

//Nug definitions
static const CharFrame char_nug_frame[] = {
	{Nug_ArcMain_nug0, {  0,   0,  54,  72},{  8, 23}},
	{Nug_ArcMain_nug0, { 54,   0,  54,  72},{  8, 23}},
	{Nug_ArcMain_nug1, {  0,   0,  56,  71},{  2, 22}},
	{Nug_ArcMain_nug1, { 56,   0,  55,  71},{  3, 23}},
	{Nug_ArcMain_nug2, {  0,   0,  51,  79},{  4, 30}},
	{Nug_ArcMain_nug2, { 51,   0,  53,  77},{  4, 29}},
	{Nug_ArcMain_nug3, {  0,   0,  55,  68},{  5, 20}},
	{Nug_ArcMain_nug3, { 55,   0,  54,  69},{  4, 20}},
	{Nug_ArcMain_nug4, {  0,   0,  55,  71},{  5, 23}},
	{Nug_ArcMain_nug4, { 55,   0,  54,  72},{  5, 23}},
};

	const char **pathp = (const char *[]){
		"nug0.tim",
		"nug1.tim",
		"nug2.tim",
		"nug3.tim",
		"nug4.tim",
		NULL,
	};

iso/characters/nug/main.arc: iso/characters/nug/nug0.tim iso/characters/nug/nug1.tim iso/characters/nug/nug2.tim iso/characters/nug/nug3.tim iso/characters/nug/nug4.tim 