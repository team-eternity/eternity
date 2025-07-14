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
// Purpose: Typedefs related to to textures etc.,
//  isolated here to make it easier separating modules.
//
// Authors: James Haley
//

#ifndef D_TEXTUR_H__
#define D_TEXTUR_H__

#include "doomtype.h"

// NOTE: Checking all BOOM sources, there is nothing used called pic_t.

// haleyjd: This file is a descendant of lumpy.h, which could be found, amongst
// other places, in id's utility package, and, most significantly, in the ROTT
// source. pic_t is a format dating back to DOOM's very early alphas. Preserved
// due to historical curiosity.

//
// Flats?
//
// a pic is an unmasked block of pixels
struct pic_t
{
    byte width;
    byte height;
    byte data;
};

#endif

//----------------------------------------------------------------------------
//
// $Log: d_textur.h,v $
// Revision 1.3  1998/05/04  21:34:18  thldrmn
// commenting and reformatting
//
// Revision 1.2  1998/01/26  19:26:33  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:54  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
