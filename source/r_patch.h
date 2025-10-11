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
//------------------------------------------------------------------------------
//
// Purpose: Patches.
//  A patch holds one or more columns. Patches are used for sprites and all
//  masked pictures, and we compose textures from the TEXTURE1/2 lists of
//  patches.
//
// haleyjd 11/21/10: Separated from r_defs.h to help resolve circular
// #include dependency problems.
//
// Authors: James Haley
//

#ifndef R_PATCH_H__
#define R_PATCH_H__

// Required for: byte, stdint types
#include "doomtype.h"

#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(push, 1)
#endif

// posts are runs of non masked source pixels
struct post_t
{
    byte topdelta; // -1 is the last post in a column
    byte length;   // length data bytes follows
};

// column_t is a list of 0 or more post_t, (byte)-1 terminated
using column_t = post_t;

struct patch_t
{
    int16_t width, height; // bounding box size
    int16_t leftoffset;    // pixels to the left of origin
    int16_t topoffset;     // pixels below the origin
    int32_t columnofs[8];  // only [width] used
};

#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(pop)
#endif

#endif

// EOF

