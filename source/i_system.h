//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//------------------------------------------------------------------------------
//
// Purpose: System-specific interface stuff.
// Authors: James Haley, Max Waine, Ioan Chera
//

#ifndef I_SYSTEM_H__
#define I_SYSTEM_H__

#include <stdarg.h>

#include "d_keywds.h"

struct ticcmd_t;
struct SDL_Window;

using i_errhandler_t = void (*)(char *errmsg);

using atexit_func_t = void (*)(void);
// Schedule a function to be called when the program exits.
void I_AtExit(atexit_func_t func);

[[noreturn]] void I_Exit(int status);

// Called by DoomMain.
void I_Init();

//
// Called by D_DoomLoop,
// called before processing any tics in a frame
// (just after displaying a frame).
// Time consuming syncronous operations
// are performed here (joystick reading).
// Can call D_PostEvent.
//

void I_StartFrame();

//
// Called by D_DoomLoop,
// called before processing each tic in a frame.
// Quick syncronous operations are performed here.
// Can call D_PostEvent.

void I_StartTicInWindow(SDL_Window *window);

// Asynchronous interrupt functions should maintain private queues
// that are read by the synchronous functions
// to be converted into events.

// Either returns a null ticcmd,
// or calls a loadable driver to build it.
// This ticcmd will then be modified by the gameloop
// for normal input.

ticcmd_t *I_BaseTiccmd();

// atexit handler -- killough

void I_Quit();

// MaxW: 2017/08/04: Quit cleanly, ignore ENDOOM

void I_QuitFast();

// haleyjd 05/21/10: error codes for I_FatalError
enum
{
    I_ERR_KILL, // exit and do not perform shutdown actions
    I_ERR_ABORT // call abort()
};

// haleyjd 06/05/10
void I_ExitWithMessage(E_FORMAT_STRING(const char *msg), ...) E_PRINTF(1, 2);

void I_SetErrorHandler(const i_errhandler_t handler);

// MaxW: 2018/06/20: No more need for an #ifdef!
// haleyjd 05/21/10
[[noreturn]] void I_FatalError(int code, E_FORMAT_STRING(const char *error), ...) E_PRINTF(2, 3);
// killough 3/20/98: add const
[[noreturn]] void I_Error(E_FORMAT_STRING(const char *error), ...) E_PRINTF(1, 2);
[[noreturn]] void I_ErrorVA(E_FORMAT_STRING(const char *error), va_list args) E_PRINTF(1, 0);

#ifdef RANGECHECK
#define I_Assert(condition, ...) if(!(condition)) I_Error(__VA_ARGS__)
#else
#define I_Assert(condition, ...)
#endif

extern int mousepresent; // killough

void I_EndDoom(); // killough 2/22/98: endgame screen

int I_CheckAbort();

#endif

//----------------------------------------------------------------------------
//
// $Log: i_system.h,v $
// Revision 1.7  1998/05/03  22:33:43  killough
// beautification, remove unnecessary #includes
//
// Revision 1.6  1998/04/27  01:52:47  killough
// Add __attribute__ to I_Error for gcc checking
//
// Revision 1.5  1998/04/10  06:34:07  killough
// Add adaptive gametic timer
//
// Revision 1.4  1998/03/23  03:17:19  killough
// Add keyboard FIFO queue and make I_Error arg const
//
// Revision 1.3  1998/02/23  04:28:30  killough
// Add ENDOOM support
//
// Revision 1.2  1998/01/26  19:26:59  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:10  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
