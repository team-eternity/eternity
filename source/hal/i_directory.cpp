// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 David Hill
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//    Directory Manipulation
//
//-----------------------------------------------------------------------------

#include "i_directory.h"

#include "i_platform.h"
#include "m_qstr.h"

#if EE_CURRENT_PLATFORM == EE_PLATFORM_LINUX
#include <sys/stat.h>
#include <sys/types.h>
#endif


//----------------------------------------------------------------------------|
// Global Functions                                                           |
//

//
// I_CreateDirectory
//
bool I_CreateDirectory(qstring const &path)
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
   return "/usr/share/eternity/base";
   #endif

   return nullptr;
}

// EOF

