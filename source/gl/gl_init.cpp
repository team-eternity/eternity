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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   
//  OpenGL Initialization/Setup Functions
//  haleyjd 05/15/11
//
//-----------------------------------------------------------------------------

#ifdef EE_FEATURE_OPENGL

#include <stdio.h>

#include "gl_includes.h"
#include "gl_init.h"

//=============================================================================
//
// Globals
//

// Active loaded version of OpenGL
GL_versioninfo GL_version;

//=============================================================================
//
// Routines
//

//
// GL_GetVersion
//
// Retrieves the version of OpenGL that is active.
//
// FIXME: unused?
//
#if 0
static void GL_GetVersion()
{
   const char *versionStr;
   int majversion = 0, minversion = 0;

   // Default to 1.0
   GL_version.majorversion = 1;
   GL_version.minorversion = 0;

   versionStr = (const char *)(glGetString(GL_VERSION));

   if(sscanf(versionStr, "%d.%d", &majversion, &minversion) == 2)
   {
      GL_version.majorversion = majversion;
      GL_version.minorversion = minversion;
   }
}
#endif

#endif

// EOF

