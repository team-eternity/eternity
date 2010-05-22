// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//----------------------------------------------------------------------------
//
// DESCRIPTION:
//
//   General-purpose system-specific routines, including timer
//   installation, keyboard, mouse, and joystick code.
//
//-----------------------------------------------------------------------------

#ifdef _MSC_VER
#include <conio.h>
#endif

#include "SDL.h"

#include "../z_zone.h"
#include "../c_runcmd.h"
#include "../d_gi.h"
#include "../i_system.h"
#include "../i_sound.h"
#include "../i_video.h"
#include "../doomstat.h"
#include "../m_misc.h"
#include "../m_syscfg.h"
#include "../g_game.h"
#include "../w_wad.h"
#include "../v_video.h"
#include "../m_argv.h"
#include "../g_bind.h"
#include "../textscreen/txt_main.h"

#ifdef _SDL_VER
extern int waitAtExit;
#endif

ticcmd_t *I_BaseTiccmd(void)
{
   static ticcmd_t emptycmd; // killough
   return &emptycmd;
}


// SoM 3/13/2002: SDL time. 1000 ticks per second.
void I_WaitVBL(int count)
{
   SDL_Delay((count*500)/TICRATE);
}


// Most of the following has been rewritten by Lee Killough

//
// I_GetTime
//
static Uint32 basetime=0;

int  I_GetTime_RealTime (void)
{
   Uint32        ticks;
   
   // milliseconds since SDL initialization
   ticks = SDL_GetTicks();
   
   return ((ticks - basetime)*TICRATE)/1000;
}

//
// I_GetTicks
//
// haleyjd 10/26/08
//
unsigned int I_GetTicks(void)
{
   return SDL_GetTicks();
}

void I_SetTime(int newtime)
{
   // SoM 3/14/2002: Uh, this function is never called. ??
}

//sf: made a #define, changed to 16
#define CLOCK_BITS 16

// killough 4/13/98: Make clock rate adjustable by scale factor
int realtic_clock_rate = 100;
static int64_t I_GetTime_Scale = 1 << CLOCK_BITS;

int I_GetTime_Scaled(void)
{
   return (int)(((int64_t)I_GetTime_RealTime() * I_GetTime_Scale) >> CLOCK_BITS);
}


static int I_GetTime_FastDemo(void)
{
   static int fasttic;
   return fasttic++;
}


static int I_GetTime_Error(void)
{
   I_FatalError(I_ERR_KILL, "Error: GetTime() used before initialization\n");
   return 0;
}

int (*I_GetTime)(void) = I_GetTime_Error;  // killough

int mousepresent;
int joystickpresent;  // phares 4/3/98

// haleyjd 04/15/02: SDL joystick support

// current device number -- saved in config file
int i_SDLJoystickNum;
 
// pointer to current joystick device information
SDL_Joystick *sdlJoystick = NULL;

int keyboard_installed = 0;
extern int autorun;          // Autorun state
static SDLMod oldmod; // SoM 3/20/2002: save old modifier key state

void I_Shutdown(void)
{
   SDL_SetModState(oldmod);
   
   // haleyjd 04/15/02: shutdown joystick
   if(joystickpresent && sdlJoystick && i_SDLJoystickNum >= 0)
   {
      if(SDL_JoystickOpened(i_SDLJoystickNum))
         SDL_JoystickClose(sdlJoystick);
      
      joystickpresent = false;
   }
}

jsdata_t *joysticks = NULL;
int numJoysticks = 0;
int sdlJoystickNumButtons;

//
// I_EnumerateJoysticks
//
// haleyjd 04/15/02
//
void I_EnumerateJoysticks(void)
{
   int i;
   
   numJoysticks = SDL_NumJoysticks();

   if(joysticks)
   {
      for(i = 0; joysticks[i].description; i++)
      {
	 Z_Free(joysticks[i].description);
      }
      Z_Free(joysticks);
   }

   joysticks = Z_Malloc((numJoysticks + 1) * sizeof(jsdata_t),
                        PU_STATIC, NULL);

   for(i = 0; i < numJoysticks; i++)
   {
      joysticks[i].description = 
	 Z_Strdup(SDL_JoystickName(i), PU_STATIC, NULL);
   }

   // last element is a dummy
   joysticks[numJoysticks].description = NULL;
}


//
// I_SetJoystickDevice
//
// haleyjd 04/15/02
//
boolean I_SetJoystickDevice(int deviceNum)
{
   if(deviceNum >= SDL_NumJoysticks())
      return false;
   else
   {
      sdlJoystick = SDL_JoystickOpen(deviceNum);

      if(!sdlJoystick)
	 return false;

      // check that the device has at least 2 axes and
      // 4 buttons
      if(SDL_JoystickNumAxes(sdlJoystick) < 2 ||
	 (sdlJoystickNumButtons = SDL_JoystickNumButtons(sdlJoystick)) < 4)
	 return false;

      return true;
   }
}

extern boolean unicodeinput;

void I_InitKeyboard(void)
{   
   keyboard_installed = 1;

   if(unicodeinput)
      SDL_EnableUNICODE(1);
}

void I_Init(void)
{
   int clock_rate = realtic_clock_rate, p;
   
   if((p = M_CheckParm("-speed")) && p < myargc-1 &&
      (p = atoi(myargv[p+1])) >= 10 && p <= 1000)
      clock_rate = p;
   
   basetime = SDL_GetTicks();
   
   // killough 4/14/98: Adjustable speedup based on realtic_clock_rate
   if(fastdemo)
   {
      I_GetTime = I_GetTime_FastDemo;
   }
   else
   {
      if (clock_rate != 100)
      {
         I_GetTime_Scale = ((int64_t) clock_rate << CLOCK_BITS) / 100;
         I_GetTime = I_GetTime_Scaled;
      }
      else
         I_GetTime = I_GetTime_RealTime;
   }

   // haleyjd 04/15/02: initialize joystick
   I_EnumerateJoysticks();

   if(i_SDLJoystickNum != -1)
   {
      joystickpresent = I_SetJoystickDevice(i_SDLJoystickNum);
   }
   else
   {
      joystickpresent = false;
   }
      
   // killough 3/6/98: end of keyboard / autorun state changes
      
   atexit(I_Shutdown);
   
   // killough 2/21/98: avoid sound initialization if no sound & no music
   { 
      extern boolean nomusicparm, nosfxparm;
      if(!(nomusicparm && nosfxparm))
         I_InitSound();
   }
}


static char errmsg[2048];  // buffer of error message -- killough

static int has_exited;

static boolean error_exit; // haleyjd: if true, an error has occurred.

//
// I_Quit
//
// Primary atexit routine for shutting down the game engine.
//
void I_Quit(void)
{
   has_exited = 1;   /* Prevent infinitely recursive exits -- killough */
   
   if(demorecording)
      G_CheckDemoStatus();
   
   // sf : rearrange this so the errmsg doesn't get messed up
   if(error_exit)
      puts(errmsg);   // killough 8/8/98
   else
      I_EndDoom();

   // SoM: 7/5/2002: Why I didn't remember this in the first place I'll never know.
   // haleyjd 10/09/05: moved down here
   SDL_Quit();

   // haleyjd 03/18/10: none of these should be called in fatal error situations.
   if(!error_exit)
   {
      M_SaveDefaults();
      M_SaveSysConfig();
      G_SaveDefaults(); // haleyjd
   }
   
   // Under Visual C++, the console window likes to rudely slam
   // shut -- this can stop it, but is now optional
#ifdef _MSC_VER
   if(error_exit || waitAtExit)
   {
      puts("\nPress any key to continue");
      getch();
   }
#endif
}

//
// I_FatalError
//
// haleyjd 05/21/10: Call this for super-evil errors such as heap corruption,
// system-related problems, etc.
//
void I_FatalError(int code, const char *error, ...)
{
   if(code == I_ERR_ABORT)
   {
      abort();
   }
   else
   {
      error_exit = true; // haleyjd: flag an error appropriately

      if(!*errmsg)   // ignore all but the first message -- killough
      {
         va_list argptr;
         va_start(argptr,error);
         pvsnprintf(errmsg, sizeof(errmsg), error, argptr);
         va_end(argptr);
      }

      if(!has_exited)    // If it hasn't exited yet, exit now -- killough
      {
         has_exited = 1; // Prevent infinitely recursive exits -- killough
         exit(-1);
      }
   }
}

//
// I_Error
//
void I_Error(const char *error, ...) // killough 3/20/98: add const
{
   if(!*errmsg)   // ignore all but the first message -- killough
   {
      va_list argptr;
      va_start(argptr,error);
      pvsnprintf(errmsg, sizeof(errmsg), error, argptr);
      va_end(argptr);
   }
   
   if(!has_exited)    // If it hasn't exited yet, exit now -- killough
   {
      has_exited = 1; // Prevent infinitely recursive exits -- killough
      exit(-1);
   }
}

void I_ErrorVA(const char *error, va_list args)
{
   if(!*errmsg)
      pvsnprintf(errmsg, sizeof(errmsg), error, args);

   if(!has_exited)
   {
      has_exited = 1;
      exit(-1);
   }
}

void I_Sleep(int ms)
{
   SDL_Delay(ms);
}

// haleyjd: made everything optional
int showendoom;
int endoomdelay;

//
// I_EndDoom
//
// killough 2/22/98: Add support for ENDBOOM, which is PC-specific
// killough 8/1/98:  change back to ENDOOM
// haleyjd 10/09/05: ENDOOM emulation thanks to fraggle and
//                   Chocolate DOOM!
//
void I_EndDoom(void)
{
   unsigned char *endoom_data;
   unsigned char *screendata;
   int start_ms;
   boolean waiting;
   
   // haleyjd: it's possible to have quit before we even initialized
   // gameModeInfo, so be sure it's valid before using it here. Also,
   // allow ENDOOM disable in configuration.
   if(!GameModeInfo || !showendoom)
      return;
   
   endoom_data = W_CacheLumpName(GameModeInfo->endTextName, PU_STATIC);
   
   // Set up text mode screen   
   TXT_Init();
   
   // Make sure the new window has the right title and icon
   SDL_WM_SetCaption("Thanks for using the Eternity Engine!", NULL);
   
   // Write the data to the screen memory   
   screendata = TXT_GetScreenData();
   memcpy(screendata, endoom_data, 4000);
   
   // Wait for 10 seconds, or until a keypress or mouse click
   // haleyjd: delay period specified in config (default = 350)
   waiting = true;
   start_ms = I_GetTime();
   
   while(waiting && I_GetTime() < start_ms + endoomdelay)
   {
      TXT_UpdateScreen();

      if(TXT_GetChar() >= 0)
         waiting = false;

      TXT_Sleep(1);
   }
   
   // Shut down text mode screen   
   TXT_Shutdown();
}

// check for ESC button pressed, regardless of keyboard handler
int I_CheckAbort(void)
{
   SDL_Event ev;

   if(SDL_PollEvent(&ev))
   {
      switch(ev.type)
      {
      case SDL_KEYDOWN:
         if(ev.key.keysym.sym == SDLK_ESCAPE)
            return true;
      default:
         break;
      }
   }

   return false;
}

/*************************
        CONSOLE COMMANDS
 *************************/
int leds_always_off;

VARIABLE_BOOLEAN(leds_always_off, NULL,     yesno);
VARIABLE_INT(realtic_clock_rate, NULL,  0, 500, NULL);

CONSOLE_VARIABLE(i_gamespeed, realtic_clock_rate, 0)
{
   if (realtic_clock_rate != 100)
   {
      I_GetTime_Scale = ((int64_t) realtic_clock_rate << CLOCK_BITS) / 100;
      I_GetTime = I_GetTime_Scaled;
   }
   else
      I_GetTime = I_GetTime_RealTime;
   
   //ResetNet();         // reset the timers and stuff
}

CONSOLE_VARIABLE(i_ledsoff, leds_always_off, 0) {}

// haleyjd 04/15/02: windows joystick commands
CONSOLE_COMMAND(i_joystick, 0)
{
   if(Console.argc != 1)
      return;

   i_SDLJoystickNum = atoi(Console.argv[0]);

   if(i_SDLJoystickNum != -1)
   {
      joystickpresent = I_SetJoystickDevice(i_SDLJoystickNum);
   }
   else
   {
      joystickpresent = false;
   }
}

#ifdef _SDL_VER
VARIABLE_BOOLEAN(waitAtExit, NULL, yesno);
CONSOLE_VARIABLE(i_waitatexit, waitAtExit, 0) {}

VARIABLE_BOOLEAN(showendoom, NULL, yesno);
CONSOLE_VARIABLE(i_showendoom, showendoom, 0) {}

VARIABLE_INT(endoomdelay, NULL, 35, 3500, NULL);
CONSOLE_VARIABLE(i_endoomdelay, endoomdelay, 0) {}
#endif

extern void I_Sound_AddCommands();
extern void I_Video_AddCommands();
extern void I_Input_AddCommands();
extern void Ser_AddCommands();

        // add system specific commands
void I_AddCommands()
{
   C_AddCommand(i_ledsoff);
   C_AddCommand(i_gamespeed);
   C_AddCommand(i_joystick);

#ifdef _SDL_VER
   C_AddCommand(i_waitatexit);
   C_AddCommand(i_showendoom);
   C_AddCommand(i_endoomdelay);
#endif
   
   I_Video_AddCommands();
   I_Sound_AddCommands();
   Ser_AddCommands();
}

//----------------------------------------------------------------------------
//
// $Log: i_system.c,v $
//
//----------------------------------------------------------------------------
