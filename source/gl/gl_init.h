// Emacs style mode select -*- C++ -*- vi:ts=3:sw=3:set et:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 James Haley
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//
//  OpenGL Initialization/Setup Functions
//  haleyjd 05/15/11
//
//-----------------------------------------------------------------------------

#ifndef GL_INIT_H__
#define GL_INIT_H__

#ifdef EE_FEATURE_OPENGL

//
// GL_versioninfo
//
// Structure holds OpenGL version information
//
struct GL_versioninfo
{
   int minorversion;
   int majorversion;

   static int makeVersion(int major, int minor)
   {
      return major * 10 + minor;
   }

   int compareVersion(int version)
   {
      int compositeversion = makeVersion(majorversion, minorversion);

      return version - compositeversion;
   }
};

extern GL_versioninfo GL_version;

#endif

#endif

// EOF
