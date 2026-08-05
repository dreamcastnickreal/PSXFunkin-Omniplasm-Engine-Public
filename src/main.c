/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "main.h"

#include "timer.h"
#include "io.h"
#include "gfx.h"
#include "audio.h"
#include "pad.h"
#include "str.h"
#include "font.h"

#include "menu.h"
#include "save.h"
#include "stage.h"
#include "pause.h"

#include "disc_swap_disc1.h"
#include "disc_swap_disc2.h"
#include "disc_swap_disc3.h"

//Game loop
GameLoop gameloop;

//Error handler
char error_msg[0x200];

void ErrorLock(void)
{
	while (1)
	{
		#ifdef PSXF_PC
			MsgPrint(error_msg);
			exit(1);
		#else
			if (fonts.font_cdr.draw_col != NULL)
			{
				fonts.font_cdr.draw(
					&fonts.font_cdr,
					"A fatal error has occured",
					(gameloop == GameLoop_Stage) ? FIXED_DEC(-SCREEN_WIDTH2 + 10,1) : 10,
					(gameloop == GameLoop_Stage) ? FIXED_DEC(-SCREEN_HEIGHT2 + 20,1) : 20,
					FontAlign_Left
				);
				fonts.font_cdr.draw(
					&fonts.font_cdr,
					error_msg,
					(gameloop == GameLoop_Stage) ? FIXED_DEC(-SCREEN_WIDTH2 + 10,1) : 10,
					(gameloop == GameLoop_Stage) ? FIXED_DEC(-SCREEN_HEIGHT2 + 36,1) : 36,
					FontAlign_Left
				);
			}
			Gfx_Flip();
		#endif
	}
}

//#define MEM_STAT //This will enable the Mem_GetStat function which returns information about available memory in the heap
#define MEM_POOL //Enable memory pool for frequently allocated objects (optimized to 24 entries)

#define MEM_IMPLEMENTATION
#include "mem.h"
#undef MEM_IMPLEMENTATION

#ifndef PSXF_STDMEM
extern u8 __heap_base[];
extern u8 __heap_end[];
#endif

//Entry point
int main(int argc, char **argv)
{
	//Remember arguments
	my_argc = argc;
	my_argv = argv;
	
	//Initialize system
	PSX_Init();
	
	Mem_Init((void*)__heap_base, (size_t)(__heap_end - __heap_base));
	
	IO_Init();
	Audio_Init();

	Timer_Init();

#ifndef PSXF_PC
	//Set GPU video mode to match detected system before graphics init
	SetVideoMode(timer_pal);
#endif

	Gfx_Init();
	Pad_Init();
	MCRD_Init();
	Str_Init();

	//if not found a save, enable some options
	if (ReadSave() == false)
	{
		//options that's already enable for be more easy
		stage.prefs.songtimer = true;
	}
	
	//Start game
	gameloop = GameLoop_Menu;
	CheckCurrentDisc();
	Menu_Load(MenuPage_Opening);
	
	//Game loop
	while (true)
	{
		//Prepare frame
		Timer_Tick();
		Audio_ProcessXA();
		Pad_Update();
		
		#ifdef MEM_STAT
			//Memory stats
			size_t mem_used, mem_size, mem_max;
			Mem_GetStat(&mem_used, &mem_size, &mem_max);
			#ifndef MEM_BAR
				if (fonts.font_cdr.draw_col != NULL)
				{
					char mem_text[64];
					sprintf(mem_text, "mem: %08X/%08X max %08X", mem_used, mem_size, mem_max);
					fonts.font_cdr.draw(
						&fonts.font_cdr,
						mem_text,
						(gameloop == GameLoop_Stage) ? FIXED_DEC(-SCREEN_WIDTH2 + 10,1) : 10,
						(gameloop == GameLoop_Stage) ? FIXED_DEC(-SCREEN_HEIGHT2 + 20,1) : 20,
						FontAlign_Left
					);
				}
			#endif
		#endif
		
		//Tick and draw game
		switch (gameloop)
		{
			case GameLoop_Menu:
				Menu_Tick();
				break;
			case GameLoop_Stage:
				Stage_Tick();
				break;
			case GameLoop_Pause:
				PausedState();
				break;
		}
		
		//Flip gfx buffers
		Gfx_Flip();
	}
	
	//Deinitialize system
	Pad_Quit();
	Gfx_Quit();
	Audio_Quit();
	IO_Quit();
	
	PSX_Quit();
	return 0;
}
