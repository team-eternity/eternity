//
// The Eternity Engine
// Copyright (C) 2019 James Haley et al.
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
// Purpose: TEXTURES lump, from ZDoom
// Authors: Ioan Chera
//

#ifndef XL_TEXTURES_H_
#define XL_TEXTURES_H_

#include "m_collection.h"
#include "m_qstr.h"

//
// TEXTURES top item
//
struct XLTexture
{
   //
   // Texture patch
   //
   struct Patch
   {
      qstring name;
      int x;
      int y;
   };

   bool optional;
   qstring name;
   int width;
   int height;
   Collection<Patch> patches;
};

extern Collection<XLTexture> xlTextures;

void XL_ParseTextures();

#endif
// EOF
