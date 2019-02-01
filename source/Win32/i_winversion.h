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

//
// Checks if Windows version is 10 or higher, for audio kludge.
// I wish we could use the Win 8.1 API and Versionhelpers.h
//
inline bool I_IsWindows10OrHigher()
{
#pragma comment(lib, "version.lib")

   static const CHAR kernel32[] = "\\kernel32.dll";
   CHAR *path;
   void *ver, *block;
   UINT  dirLength;
   DWORD versionSize;
   UINT  blockSize;
   VS_FIXEDFILEINFO *vInfo;
   WORD  majorVer;

   path = emalloc(CHAR *, sizeof(*path) * MAX_PATH);

   dirLength = GetSystemDirectory(path, MAX_PATH);
   if(dirLength >= MAX_PATH || dirLength == 0 || dirLength > MAX_PATH - earrlen(kernel32))
      I_Error("I_IsWindows10OrHigher: Location of kernel32.dll longer than MAX_PATH.");
   memcpy(path + dirLength, kernel32, sizeof(kernel32));

   versionSize = GetFileVersionInfoSize(path, nullptr);
   if(versionSize == 0)
      abort();
   ver = emalloc(void *, versionSize);
   if(!GetFileVersionInfo(path, 0, versionSize, ver))
      I_Error("I_IsWindows10OrHigher: GetFileVersionInfo failed.");
   if(!VerQueryValue(ver, "\\", &block, &blockSize) || blockSize < sizeof(VS_FIXEDFILEINFO))
      I_Error("I_IsWindows10OrHigher: VerQueryValue failed.");
   vInfo = static_cast<VS_FIXEDFILEINFO *>(block);
   majorVer = HIWORD(vInfo->dwProductVersionMS);
   //minorVer = LOWORD(vInfo->dwProductVersionMS);
   //buildNum = HIWORD(vInfo->dwProductVersionLS);
   efree(path);
   efree(ver);
   return majorVer >= 10;
}

#endif

#endif

// EOF

