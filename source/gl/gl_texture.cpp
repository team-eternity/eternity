// Emacs style mode select   -*- C++ -*-
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
//  OpenGL Texture Mapping Functions
//  haleyjd 05/15/11
//
//-----------------------------------------------------------------------------

#include "gl_includes.h"

//
// GL_MakeTextureDimension
//
// Gets the next higher power of two, which will be a suitable texture 
// dimension for standard OpenGL textures.
//
// Based on:
// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
//
unsigned int GL_MakeTextureDimension(unsigned int i)
{
   if(i)
   {
      --i;
      i |= i >> 1;
      i |= i >> 2;
      i |= i >> 4;
      i |= i >> 8;
      i |= i >> 16;
      ++i;
   }

   return i;
}

// EOF

