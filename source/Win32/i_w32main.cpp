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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//------------------------------------------------------------------------------
//
// Purpose: Win32-specific main function, used in release mode. Provides proper
//  exception handling instead of the silent dump-out to console that the
//  SDL parachute now provides.
//
// Authors: James Haley, Max Waine
//

#include "../hal/i_platform.h"

#ifdef _MSC_VER

#include <windows.h>
#include "SDL_syswm.h"

#include "../d_keywds.h"
#include "../i_system.h"

extern void I_W32InitExceptionHandler(void);
extern int __cdecl I_W32ExceptionHandler(PEXCEPTION_POINTERS ep);
extern int common_main(int argc, char **argv);

int disable_sysmenu;

//
// Enables the Win32 console window's close button.
//
static void I_untweakConsole()
{
#if _WIN32_WINNT > 0x500
    HWND hwnd = GetConsoleWindow();

    if(hwnd)
    {
        HMENU hMenu = GetSystemMenu(hwnd, FALSE);
        EnableMenuItem(hMenu, SC_CLOSE, MF_ENABLED);
    }
#endif
}

//
// Disable the Win32 console window's close button and set its title.
//
static void I_tweakConsole()
{
#if _WIN32_WINNT > 0x500
    HWND hwnd = GetConsoleWindow();

    if(hwnd)
    {
        HMENU hMenu = GetSystemMenu(hwnd, FALSE);
        EnableMenuItem(hMenu, SC_CLOSE, MF_DISABLED | MF_GRAYED);
        I_AtExit(I_untweakConsole);
    }
    SetConsoleTitle("Eternity Engine System Console");
#endif
}

#if !defined(_DEBUG)
int main(int argc, char **argv)
{
    I_W32InitExceptionHandler();

    __try
    {
        I_tweakConsole();
        common_main(argc, argv);
    }
    __except(I_W32ExceptionHandler(GetExceptionInformation()))
    {
        I_FatalError(0, "Exception caught in main: see CRASHLOG.TXT for info, and in the same directory please upload "
                        "eternity.dmp along with the crash log\n");
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
void I_DisableSysMenu(SDL_Window *window)
{
    if(disable_sysmenu)
    {
        SDL_SysWMinfo info;

        SDL_VERSION(&info.version); // this is important!

        if(SDL_GetWindowWMInfo(window, &info))
        {
            LONG_PTR window_style  = GetWindowLongPtr(info.info.win.window, GWL_STYLE);
            window_style          &= ~WS_SYSMENU;
            SetWindowLongPtr(info.info.win.window, GWL_STYLE, window_style);
        }
    }
}

#endif

// EOF

