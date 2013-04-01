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

// HAL modules
#include "../hal/i_gamepads.h"

#include "../z_zone.h"
#include "../c_io.h"
#include "../c_runcmd.h"
#include "../d_event.h"
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

//
// I_BaseTiccmd
//
ticcmd_t *I_BaseTiccmd()
{
   static ticcmd_t emptycmd; // killough
   return &emptycmd;
}

//
// I_WaitVBL
//
// SoM 3/13/2002: SDL time. 1000 ticks per second.
//
void I_WaitVBL(int count)
{
   SDL_Delay((count*500)/TICRATE);
}

//=============================================================================
//
// I_GetTime
// Most of the following has been rewritten by Lee Killough
//

static Uint32 basetime=0;

//
// I_GetTime_RealTime
//
int I_GetTime_RealTime()
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
unsigned int I_GetTicks()
{
   return SDL_GetTicks();
}

//
// I_SetTime
//
void I_SetTime(int newtime)
{
   // SoM 3/14/2002: Uh, this function is never called. ??
}

//sf: made a #define, changed to 16
#define CLOCK_BITS 16

// killough 4/13/98: Make clock rate adjustable by scale factor
int realtic_clock_rate = 100;
static int64_t I_GetTime_Scale = 1 << CLOCK_BITS;

//
// I_GetTime_Scaled
//
int I_GetTime_Scaled()
{
   return (int)(((int64_t)I_GetTime_RealTime() * I_GetTime_Scale) >> CLOCK_BITS);
}

//
// I_GetTime_FastDemo
//
static int I_GetTime_FastDemo()
{
   static int fasttic;
   return fasttic++;
}

//
// I_GetTime_Error
//
static int I_GetTime_Error()
{
   I_Error("Error: GetTime() used before initialization\n");
   return 0;
}

int (*I_GetTime)(void) = I_GetTime_Error;  // killough

int mousepresent;

int keyboard_installed = 0;
extern int autorun;          // Autorun state
static SDLMod oldmod; // SoM 3/20/2002: save old modifier key state

//
// I_Shutdown
//
// Added as an atexit handler.
//
void I_Shutdown()
{
   SDL_SetModState(oldmod);
   
   // haleyjd 04/15/02: shutdown joystick
   I_ShutdownGamePads();
}

extern bool unicodeinput;

//
// I_InitKeyboard
//
void I_InitKeyboard()
{   
   keyboard_installed = 1;

   if(unicodeinput)
      SDL_EnableUNICODE(1);

   // haleyjd 05/10/11: moved here from video module
   // enable key repeat
   SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY/2, SDL_DEFAULT_REPEAT_INTERVAL*4);
}

//
// I_Init
//
void I_Init()
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
   I_InitGamePads();
      
   // killough 3/6/98: end of keyboard / autorun state changes
      
   atexit(I_Shutdown);
   
   // killough 2/21/98: avoid sound initialization if no sound & no music
   extern bool nomusicparm, nosfxparm;
   if(!(nomusicparm && nosfxparm))
      I_InitSound();
}


static char errmsg[2048];  // buffer of error message -- killough

static int has_exited;

enum
{
   I_ERRORLEVEL_NONE,    // no error
   I_ERRORLEVEL_MESSAGE, // not really an error, just an exit message
   I_ERRORLEVEL_NORMAL,  // a "normal" error (such as a missing patch)
   I_ERRORLEVEL_FATAL    // kill with a vengeance
};

// haleyjd: if non-0, an error has occurred. The level of error is set
// by the function which flagged the error condition, as per the above
// enumeration.
static int error_exitcode;

#define IFNOTFATAL(a) if(error_exitcode < I_ERRORLEVEL_FATAL) a

//
// I_Quit
//
// Primary atexit routine for shutting down the game engine.
//
void I_Quit(void)
{
   has_exited = 1;   /* Prevent infinitely recursive exits -- killough */
   
   // haleyjd 06/05/10: not in fatal error situations; causes heap calls
   if(error_exitcode < I_ERRORLEVEL_FATAL && demorecording)
      G_CheckDemoStatus();
   
   // sf : rearrange this so the errmsg doesn't get messed up
   if(error_exitcode >= I_ERRORLEVEL_MESSAGE)
      puts(errmsg);   // killough 8/8/98
   else
      I_EndDoom();

   // SoM: 7/5/2002: Why I didn't remember this in the first place I'll never know.
   // haleyjd 10/09/05: moved down here
   SDL_Quit();

   // haleyjd 03/18/10: none of these should be called in fatal error situations.
   //         06/06/10: check each call, as an I_FatalError called from any of this
   //                   code could escalate the error status.

   IFNOTFATAL(M_SaveDefaults());
   IFNOTFATAL(M_SaveSysConfig());
   IFNOTFATAL(G_SaveDefaults()); // haleyjd
   
#ifdef _MSC_VER
   // Under Visual C++, the console window likes to rudely slam
   // shut -- this can stop it, but is now optional except when an error occurs
   if(error_exitcode >= I_ERRORLEVEL_NORMAL || waitAtExit)
   {
      puts("Press any key to continue\n");
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
   // Flag a fatal error, so that some shutdown code will not be executed;
   // chiefly, saving the configuration files, which can malfunction in
   // unpredictable ways when heap corruption is present. We do this even
   // if an error has already occurred, since, for example, if a Z_ChangeTag
   // error happens during M_SaveDefaults, we do not want to subsequently
   // run M_SaveSysConfig etc. in I_Quit.
   error_exitcode = I_ERRORLEVEL_FATAL;

   if(code == I_ERR_ABORT)
   {
      // kill with utmost contempt
      abort();
   }
   else
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
      else
         abort(); // double fault, must abort
   }
}

//
// I_ExitWithMessage
//
// haleyjd 06/05/10: exit with a message which is not technically an error. The
// code used to call I_Error for this, but it wasn't semantically correct.
//
void I_ExitWithMessage(const char *msg, ...)
{
   // do not demote error level
   if(error_exitcode < I_ERRORLEVEL_MESSAGE)
      error_exitcode = I_ERRORLEVEL_MESSAGE; // just a message

   if(!*errmsg)   // ignore all but the first message -- killough
   {
      va_list argptr;
      va_start(argptr, msg);
      pvsnprintf(errmsg, sizeof(errmsg), msg, argptr);
      va_end(argptr);
   }

   if(!has_exited)    // If it hasn't exited yet, exit now -- killough
   {
      has_exited = 1; // Prevent infinitely recursive exits -- killough
      exit(0);
   }
}

//
// I_Error
//
// Normal error reporting / exit routine.
//
void I_Error(const char *error, ...) // killough 3/20/98: add const
{
   // do not demote error level
   if(error_exitcode < I_ERRORLEVEL_NORMAL)
      error_exitcode = I_ERRORLEVEL_NORMAL; // a normal error

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
   else
      I_FatalError(I_ERR_ABORT, "I_Error: double faulted\n");
}

//
// I_ErrorVA
//
// haleyjd: varargs version of I_Error used chiefly by libConfuse.
//
void I_ErrorVA(const char *error, va_list args)
{
   // do not demote error level
   if(error_exitcode < I_ERRORLEVEL_NORMAL)
      error_exitcode = I_ERRORLEVEL_NORMAL;

   if(!*errmsg)
      pvsnprintf(errmsg, sizeof(errmsg), error, args);

   if(!has_exited)
   {
      has_exited = 1;
      exit(-1);
   }
   else
      I_FatalError(I_ERR_ABORT, "I_ErrorVA: double faulted\n");
}

//
// I_Sleep
//
// haleyjd: routine to sleep a fixed number of milliseconds.
//
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
void I_EndDoom()
{
   unsigned char *endoom_data;
   unsigned char *screendata;
   int start_ms;
   bool waiting;
   
   // haleyjd: it's possible to have quit before we even initialized
   // GameModeInfo, so be sure it's valid before using it here. Also,
   // allow ENDOOM disable in configuration.
   if(!GameModeInfo || !showendoom)
      return;
   
   endoom_data = (unsigned char *)wGlobalDir.cacheLumpName(GameModeInfo->endTextName, PU_STATIC);
   
   // Set up text mode screen   
   if(!TXT_Init())
      return;
   
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
int I_CheckAbort()
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

#ifdef _SDL_VER
VARIABLE_BOOLEAN(waitAtExit, NULL, yesno);
CONSOLE_VARIABLE(i_waitatexit, waitAtExit, 0) {}

VARIABLE_BOOLEAN(showendoom, NULL, yesno);
CONSOLE_VARIABLE(i_showendoom, showendoom, 0) {}

VARIABLE_INT(endoomdelay, NULL, 35, 3500, NULL);
CONSOLE_VARIABLE(i_endoomdelay, endoomdelay, 0) {}
#endif

// EOF

