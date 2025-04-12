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
// Purpose: Win32 GUI stuff (message boxes, folder dialogues, etc.).
// Authors: Max Waine
//

#include "../hal/i_platform.h"

#if EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ShlObj.h>

#include "../m_qstr.h"

qstring I_OpenWindowsDirectory()
{
   BROWSEINFOA info {
      nullptr, nullptr, nullptr,
      " Select the folder where your game files (IWADs) are stored",
      BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE
   };

   if(LPITEMIDLIST pidl = SHBrowseForFolderA(&info); pidl)
   {
      TCHAR path[MAX_PATH];
      SHGetPathFromIDListA(pidl, path);

      IMalloc *imalloc = nullptr;
      if(SUCCEEDED(SHGetMalloc(&imalloc)))
      {
         imalloc->Free(pidl);
         imalloc->Release();
      }


      return qstring(path);
   }

   return qstring();
}

bool I_TryIWADSearchAgain()
{
   return MessageBoxA(
      nullptr,
      "No game files (IWADs) found in selected folder. Do you wish to select a new one?",
      nullptr,
      MB_YESNO | MB_SETFOREGROUND
   ) == IDYES;
}

#endif

// EOF

