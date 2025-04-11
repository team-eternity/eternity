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
//   
//  OpenGL Texture Mapping Functions
//  haleyjd 05/15/11
//
//-----------------------------------------------------------------------------

#ifdef EE_FEATURE_OPENGL

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

static GLuint boundtexture;

//
// GL_BindTextureAndRemember
//
// Binds the texture unconditionally and remembers the fact that this texture
// is the currently bound texture.
//
void GL_BindTextureAndRemember(GLuint texture)
{
   glBindTexture(GL_TEXTURE_2D, texture);
   boundtexture = texture;
}

//
// GL_BindTextureIfNeeded
//
// Binds the texture only if it isn't already remembered as the currently bound
// texture.
//
void GL_BindTextureIfNeeded(GLuint texture)
{
   if(boundtexture != texture)
      glBindTexture(GL_TEXTURE_2D, texture);
}


//
// Rebinds the currently-bound texture
//
void GL_RebindBoundTexture()
{
   if(boundtexture)
      glBindTexture(GL_TEXTURE_2D, boundtexture);
}

//
// GL_ClearBoundTexture
//
// Zeroes out the remembered bound texture name.
//
void GL_ClearBoundTexture()
{
   boundtexture = 0;
}

#endif

// EOF

