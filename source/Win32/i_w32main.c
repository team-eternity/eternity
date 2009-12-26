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
#error i_w32main.c is for Windows only
#endif

#include <windows.h>
#include "SDL_syswm.h"

extern int __cdecl I_W32ExceptionHandler(PEXCEPTION_POINTERS ep);
extern int common_main(int argc, char **argv);
extern void I_Error(const char *error, ...);

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
      I_Error("Exception caught in main: see CRASHLOG.TXT for info\n");
   }

   return 0;
}
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
         LONG window_style = GetWindowLongPtr(info.window, GWL_STYLE);
         window_style &= ~WS_SYSMENU;
         SetWindowLongPtr(info.window, GWL_STYLE, window_style);
      }
   }
}

// EOF