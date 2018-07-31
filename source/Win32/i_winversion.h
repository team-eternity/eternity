//
// The Eternity Engine
// Copyright(C) 2018 James Haley, Max Waine, et al.
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
//----------------------------------------------------------------------------
//
// Purpose: Ugly hack to check if Windows is 10 or higher,
//          to be removed once XP support is dropped.
// Authors: Max Waine
//

#ifndef I_WINVERSION_H__
#define I_WINVERSION_H__

#include "../hal/i_platform.h"

#if EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS

#include <windows.h>

// MaxW: Necessary for a specific check that seems to help with audio issues
// FIXME: When we finally axe XP support we can remove all this Windows 10 nonsense
// as we can just #include <versionhelper.h>
#ifndef _WIN32_WINNT_WIN10
#define _WIN32_WINNT_WIN10 0x0A00
#endif

//
// Checks if Windows version is 10 or higher, for audio kludge
//
inline bool I_IsWindows10OrHigher()
{
   OSVERSIONINFOEXW osvi = { sizeof(osvi) };
   const DWORDLONG dwlConditionMask =
      VerSetConditionMask(
         VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL),
         VER_MINORVERSION, VER_GREATER_EQUAL);
   osvi.dwMajorVersion = HIBYTE(_WIN32_WINNT_WIN10);
   osvi.dwMinorVersion = LOBYTE(_WIN32_WINNT_WIN10);
   osvi.wServicePackMajor = 1;

   return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION, dwlConditionMask) != FALSE;
}

#endif

#endif

// EOF

