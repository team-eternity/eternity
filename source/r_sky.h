// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Sky rendering.
//
//-----------------------------------------------------------------------------

#ifndef R_SKY_H__
#define R_SKY_H__

#include "m_fixed.h"

// The sky map is 256*128*4 maps.
#define ANGLETOSKYSHIFT         22

// Necessary height of sky to prevent stretching for looking up
enum
{
   SKY_FREELOOK_HEIGHT = 200,
};

// haleyjd: information on sky flats
struct skyflat_t
{
   // static information (specified via GameModeInfo)
   const char *flatname;   // flat name
   const char *deftexture; // default texture, if any (may be null)

   // runtime information
   int flatnum;      // corresponding flat number
   int texture;      // looked-up wall texture number from directory
   int columnoffset; // scrolling info
};

// haleyjd: hashed sky texture information

#define NUMSKYCHAINS 13

struct skytexture_t
{
   int     texturenum; // hash key
   int     height;     // true height of texture
   fixed_t texturemid; // vertical offset
   skytexture_t *next; // next skytexture in hash chain
};

extern int stretchsky;

// init sky at start of level
void R_StartSky();

// sky texture info hashing functions
skytexture_t *R_GetSkyTexture(int);
void R_ClearSkyTextures();

bool R_IsSkyFlat(int picnum);
skyflat_t *R_SkyFlatForIndex(int skynum);
skyflat_t *R_SkyFlatForPicnum(int picnum);

#endif

//----------------------------------------------------------------------------
//
// $Log: r_sky.h,v $
// Revision 1.4  1998/05/03  22:56:25  killough
// Add m_fixed.h #include
//
// Revision 1.3  1998/05/01  14:15:29  killough
// beautification
//
// Revision 1.2  1998/01/26  19:27:46  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:09  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
