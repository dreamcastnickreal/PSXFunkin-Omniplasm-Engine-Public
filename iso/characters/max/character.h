enum
{
	Max_ArcMain_max0,
	Max_ArcMain_max1,
	Max_ArcMain_max2,
	Max_ArcMain_max3,
	Max_ArcMain_max4,

	Max_Arc_Max,
};

//Max definitions
static const CharFrame char_max_frame[] = {
	{Max_ArcMain_max0, {  0,   0,  63, 112},{  7, 38}},
	{Max_ArcMain_max1, {  0,   0,  69, 113},{ 13, 39}},
	{Max_ArcMain_max1, { 69,   0,  66, 113},{ 12, 39}},
	{Max_ArcMain_max2, {  0,   0,  59, 111},{  1, 37}},
	{Max_ArcMain_max2, { 59,   0,  59, 111},{  2, 37}},
	{Max_ArcMain_max3, {  0,   0,  58, 120},{  3, 45}},
	{Max_ArcMain_max3, { 58,   0,  58, 118},{  3, 44}},
	{Max_ArcMain_max4, {  0,   0,  59, 106},{  7, 33}},
	{Max_ArcMain_max4, { 59,   0,  62, 107},{  8, 33}},
};

	const char **pathp = (const char *[]){
		"max0.tim",
		"max1.tim",
		"max2.tim",
		"max3.tim",
		"max4.tim",
		NULL,
	};

iso/characters/max/main.arc: iso/characters/max/max0.tim iso/characters/max/max1.tim iso/characters/max/max2.tim iso/characters/max/max3.tim iso/characters/max/max4.tim 