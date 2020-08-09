// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Win32-specific File Name Handling Routines.
//      haleyjd 10/28/04
//
//      Note: You cannot include most of DOOM's header files into this
//      module, due to namespace conflicts with the Win32 API.  Keep it
//      isolated.
//
//-----------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN

#include <stdlib.h>
#include <windows.h>
#include <winbase.h>

#include "../d_keywds.h"

extern void I_FatalError(int code, E_FORMAT_STRING(const char *error), ...)
extern void M_GetFilePath(const char *fn, char *base, size_t len);

//
// WIN_GetExeDir
//
// When compiling for Windows, D_DoomExeDir calls this instead of
// the generalized M_GetFilePath on argv[0].  This is necessary 
// because some versions of Windows deliberately strip argv[0] down
// to the file name, removing all path information. This causes a
// serious malfunction under Windows XP which can only be corrected
// by resorting to the use of these Win32 API functions.
// GetModuleFileName guarantees a full path will be returned in
// the provided buffer.
//
void WIN_GetExeDir(char *buffer, unsigned int size)
{
   // get the name of the current process's module
   DWORD nRet = GetModuleFileName(nullptr, (LPTSTR)buffer, (DWORD)size);
   char *dupstr;

   // if 0 or if the full buffer size, it's not a value we can use
   // and the only available option is to exit the program
   if(!nRet || nRet == size)
      I_FatalError(0, "WIN_GetExeDir: could not determine module file name.\n");

   dupstr = strdup(buffer);

   // remove the file name, leaving only the full path
   M_GetFilePath(dupstr, buffer, size);

   free(dupstr);
}

// EOF

