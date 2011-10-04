// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2009 James Haley
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
//   Win32-specific main function, used in release mode. Provides proper
//   exception handling instead of the silent dump-out to console that the 
//   SDL parachute now provides.   
//
//-----------------------------------------------------------------------------

#ifndef _WIN32
#ifndef __MINGW32__
#error i_w32main.cpp is for Windows only
#endif
#endif

#include <windows.h>
#include "SDL_syswm.h"
#ifdef __MINGW32__
#include "seh/seh.h"
#endif

extern int __cdecl I_W32ExceptionHandler(PEXCEPTION_POINTERS ep);
extern int common_main(int argc, char **argv);
extern void I_FatalError(int code, const char *error, ...);

int disable_sysmenu;

#ifndef _DEBUG
int main(int argc, char **argv)
{
   __try
   {
      common_main(argc, argv);
   }
   __except(I_W32ExceptionHandler(GetExceptionInformation()))
   {
      I_FatalError(0, "Exception caught in main: see CRASHLOG.TXT for info\n");
   }
#ifdef __MINGW32__
   __end_except
#endif

   return 0;
}
#endif

// haleyjd 02/04/10: use GetWindowLongPtr on platforms that support 64-bit
// compilation, but not on 6.0, because it doesn't have the function in the
// default headers.
#if _MSC_VER >= 1400
#define EEGETWINDOWLONG GetWindowLongPtr
#define EESETWINDOWLONG SetWindowLongPtr
#else
#define EEGETWINDOWLONG GetWindowLong
#define EESETWINDOWLONG SetWindowLong
#endif

//
// I_DisableSysMenu
//
// haleyjd 11/13/09: Optional disabling of the system menu in Windows after
// video init to avoid problems with the alt+space key combo. Without this
// option, the default control scheme for DOOM becomes broken in windowed
// mode under the default SDL setup.
//
void I_DisableSysMenu(void)
{
   if(disable_sysmenu)
   {
      SDL_SysWMinfo info;
      
      SDL_VERSION(&info.version); // this is important!
      
      if(SDL_GetWMInfo(&info))
      {
         LONG window_style = EEGETWINDOWLONG(info.window, GWL_STYLE);
         window_style &= ~WS_SYSMENU;
         EESETWINDOWLONG(info.window, GWL_STYLE, window_style);
      }
   }
}

// EOF
