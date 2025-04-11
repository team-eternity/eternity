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
//  OpenGL Header Includes
//  haleyjd 05/15/11
//
//-----------------------------------------------------------------------------

#ifndef GL_HEADERS_H__
#define GL_HEADERS_H__

#ifdef EE_FEATURE_OPENGL

// haleyjd: Borrowed from SDL for cross-platform support
#ifdef _WIN32
   #define WIN32_LEAN_AND_MEAN
   #ifndef NOMINMAX
      #define NOMINMAX       /* Don't define min() and max() */
   #endif
   #include <windows.h>
#endif
#if defined(__MACOSX__) || defined(__APPLE__)
   #include <OpenGL/gl.h>  /* Header File For The OpenGL Library */
#elif defined(__MACOS__)
   #include <gl.h>         /* Header File For The OpenGL Library */
#else
   #include <GL/gl.h>      /* Header File For The OpenGL Library */
#endif

#endif

#endif

// EOF

