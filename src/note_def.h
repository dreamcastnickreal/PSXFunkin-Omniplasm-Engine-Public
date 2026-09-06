#ifndef PSXF_GUARD_NOTE_DEF_H
#define PSXF_GUARD_NOTE_DEF_H

#include "psx.h"

// Note X position definitions
extern int note_x4k_normal[8];
extern int note_x4k_flipped[8];
extern int note_x5k_normal[10];
extern int note_x5k_flipped[10];
extern int note_x6k_normal[12];
extern int note_x6k_flipped[12];
extern int note_x7k_normal[14];
extern int note_x7k_flipped[14];
extern int note_x9k_normal[18];
extern int note_x9k_flipped[18];

// Rotten Smoothie middle (centered BF, hidden opponent) defs
extern int note_x4k_rotten_smoothie_middle[8];
extern int note_x5k_rotten_smoothie_middle[10];
extern int note_x6k_rotten_smoothie_middle[12];
extern int note_x7k_rotten_smoothie_middle[14];
extern int note_x9k_rotten_smoothie_middle[18];

// Note key / anim tables (moved from stage.c)
extern u16 note_key4k[];
extern u16 note_key5k[];
extern u16 note_key6k[];
extern u16 note_key7k[];
extern u16 note_key9k[];

extern u8 note_anims4k[4][3];
extern u8 note_anims5k[5][3];
extern u8 note_anims6k[6][3];
extern u8 note_anims7k[7][3];
extern u8 note_anims9k[9][3];

#endif
