// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 David Hill et al.
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
//    Directory Manipulation
//
//-----------------------------------------------------------------------------

#include "../z_zone.h"

#include "i_directory.h"

#include "i_platform.h"
#include "../m_qstr.h"


//=============================================================================
//
// Global Functions
//

//
// I_CreateDirectory
//
bool I_CreateDirectory(const qstring &path)
{
#if EE_CURRENT_PLATFORM == EE_PLATFORM_LINUX
   if(!mkdir(path.constPtr(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH))
      return true;
#endif

   return false;
}

//
// I_PlatformInstallDirectory
//
const char *I_PlatformInstallDirectory()
{
#if EE_CURRENT_PLATFORM == EE_PLATFORM_LINUX
   struct stat sbuf;

   // Prefer /usr/local, but fall back to just /usr.
   if(!stat("/usr/local/share/eternity/base", &sbuf) && S_ISDIR(sbuf.st_mode))
      return "/usr/local/share/eternity/base";
   else
      return "/usr/share/eternity/base";
#endif

   return nullptr;
}

// EOF

