//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Max Waine, et al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
// Purpose: General-purpose system-specific routines, including timer
//  installation, keyboard, mouse, and joystick code.
//
// Authors: James Haley, Max Waine
//

#ifdef _MSC_VER
#include <conio.h>
#endif

#include "SDL.h"

// HAL modules
#include "../hal/i_gamepads.h"
#include "../hal/i_platform.h"
#include "../hal/i_timer.h"

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
#include "../mn_menus.h"
#include "../g_demolog.h"
#include "../g_game.h"
#include "../w_wad.h"
#include "../v_video.h"
#include "../m_argv.h"
#include "../g_bind.h"
#include "../textscreen/txt_main.h"

#ifdef _SDL_VER
extern int waitAtExit;
#endif

struct atexit_listentry_t
{
   atexit_func_t func;
   atexit_listentry_t *next;
};

static atexit_listentry_t *exit_funcs = NULL;

void I_AtExit(atexit_func_t func)
{
   atexit_listentry_t *entry;

   entry = (atexit_listentry_t *)malloc(sizeof(*entry));

   entry->func = func;
   entry->next = exit_funcs;
   exit_funcs = entry;
}

void I_Exit(int status)
{
   atexit_listentry_t *entry, *next;

   // Run through all exit functions

   entry = exit_funcs;

   while (entry != NULL)
   {
      entry->func();
      next = entry->next;
      free(entry);
      entry = next;
   }

   exit(status);
}

//
// I_BaseTiccmd
//
ticcmd_t *I_BaseTiccmd()
{
   static ticcmd_t emptycmd; // killough
   return &emptycmd;
}

int mousepresent;

extern int autorun;          // Autorun state
static SDL_Keymod oldmod; // SoM 3/20/2002: save old modifier key state

//
// I_Shutdown
//
// Added as an atexit handler.
//
void I_Shutdown()
{
   SDL_SetModState(oldmod);
}

//
// I_Init
//
void I_Init()
{
   // haleyjd 01/10/14: initialize timer
   I_InitHALTimer();

   // haleyjd 04/15/02: initialize joystick
   I_InitGamePads(MN_UpdateGamepadMenus);
 
   I_AtExit(I_Shutdown);
   
   // killough 2/21/98: avoid sound initialization if no sound & no music
   extern bool nomusicparm, nosfxparm;
   if(!(nomusicparm && nosfxparm))
      I_InitSound();
}

static i_errhandler_t i_error_handler;  // special handler for I_Error run before exiting
static thread_local char errmsg[2048];  // buffer of error message -- killough

static bool has_exited;

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

// MaxW: If we wanna exit fast, skip ENDOOM stuff regardless of player setting,
// but we DON'T wanna overwrite the user's showendoom setting in their cfg
static bool speedyexit = false;

//
// I_Quit
//
// Primary atexit routine for shutting down the game engine.
//
void I_Quit(void)
{
   has_exited = true;   /* Prevent infinitely recursive exits -- killough */
   
   // haleyjd 06/05/10: not in fatal error situations; causes heap calls
   if(error_exitcode < I_ERRORLEVEL_FATAL && demorecording)
      G_CheckDemoStatus();
   
   // sf : rearrange this so the errmsg doesn't get messed up
   if(error_exitcode >= I_ERRORLEVEL_MESSAGE)
      puts(errmsg);   // killough 8/8/98
   else if(!speedyexit) // MaxW: The user didn't Alt+F4
      I_EndDoom();

   // Shutdown joystick (after ENDOOM so user can still input to exit it)
   I_ShutdownGamePads();

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
   // ioanch 20160313: do not pause if demo logging is enabled
   if(!G_DemoLogEnabled() && 
      (error_exitcode >= I_ERRORLEVEL_NORMAL || waitAtExit))
   {
      puts("Press any key to continue\n");
      getch();
   }
#endif
}

//
// MaxW: 2017/08/04: Exit quickly and cleanly (skip ENDOOM display)
//
void I_QuitFast()
{
   puts("Eternity quit quickly.");
   speedyexit = true;
   I_Exit(0);
}

//
// I_FatalError
//
// haleyjd 05/21/10: Call this for super-evil errors such as heap corruption,
// system-related problems, etc.
//
void I_FatalError(int code, E_FORMAT_STRING(const char *error), ...)
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
         has_exited = true; // Prevent infinitely recursive exits -- killough
         I_Exit(-1);
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
void I_ExitWithMessage(E_FORMAT_STRING(const char *msg), ...)
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

   if(!has_exited)       // If it hasn't exited yet, exit now -- killough
   {
      has_exited = true; // Prevent infinitely recursive exits -- killough
      I_Exit(0);
   }
}

//
// Set a special handler for I_Error that runs prior to exiting
//
void I_SetErrorHandler(const i_errhandler_t handler)
{
   i_error_handler = handler;
}

//
// I_Error
//
// Normal error reporting / exit routine.
//
void I_Error(E_FORMAT_STRING(const char *error), ...)
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

   if(i_error_handler)
      i_error_handler(errmsg);

   if(!has_exited)       // If it hasn't exited yet, exit now -- killough
   {
      has_exited = true; // Prevent infinitely recursive exits -- killough
      I_Exit(-1);
   }
   else
      I_FatalError(I_ERR_ABORT, "I_Error: double faulted\n");
}

//
// I_ErrorVA
//
// haleyjd: varargs version of I_Error used chiefly by libConfuse.
//
void I_ErrorVA(E_FORMAT_STRING(const char *error), va_list args)
{
   // do not demote error level
   if(error_exitcode < I_ERRORLEVEL_NORMAL)
      error_exitcode = I_ERRORLEVEL_NORMAL;

   if(!*errmsg)
      pvsnprintf(errmsg, sizeof(errmsg), error, args);

   if(!has_exited)
   {
      has_exited = true;
      I_Exit(-1);
   }
   else
      I_FatalError(I_ERR_ABORT, "I_ErrorVA: double faulted\n");
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
   int lumpnum;
   
   // haleyjd: it's possible to have quit before we even initialized
   // GameModeInfo, so be sure it's valid before using it here. Also,
   // allow ENDOOM disable in configuration.
   if(!GameModeInfo || !showendoom)
      return;
   
   if((lumpnum = wGlobalDir.checkNumForName(GameModeInfo->endTextName)) < 0)
      return;

   endoom_data = (unsigned char *)wGlobalDir.cacheLumpNum(lumpnum, PU_STATIC);
   
   // Set up text mode screen   
   if(!TXT_Init())
      return;
   
   // Make sure the new window has the right title and icon
   TXT_SetWindowTitle("Thanks for using the Eternity Engine!");
   
   // Write the data to the screen memory   
   screendata = TXT_GetScreenData();
   memcpy(screendata, endoom_data, 4000);
   
   // Wait for 10 seconds, or until a keypress or mouse click
   // haleyjd: delay period specified in config (default = 350)
   start_ms = i_haltimer.GetTime();
   
   while(i_haltimer.GetTime() < start_ms + endoomdelay)
   {
      TXT_UpdateScreen();

      if(TXT_GetChar() > 0)
         break;

      TXT_Sleep(0);
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

VARIABLE_BOOLEAN(leds_always_off, nullptr,  yesno);
CONSOLE_VARIABLE(i_ledsoff, leds_always_off, 0) {}

#ifdef _SDL_VER
VARIABLE_BOOLEAN(waitAtExit, nullptr, yesno);
CONSOLE_VARIABLE(i_waitatexit, waitAtExit, 0) {}

VARIABLE_BOOLEAN(showendoom, nullptr, yesno);
CONSOLE_VARIABLE(i_showendoom, showendoom, 0) {}

VARIABLE_INT(endoomdelay, nullptr, 35, 3500, nullptr);
CONSOLE_VARIABLE(i_endoomdelay, endoomdelay, 0) {}
#endif

// EOF

