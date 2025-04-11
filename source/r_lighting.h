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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//  Light tables and related constants
//  Split out of r_defs.h for dependency-hell resolution.
//
//-----------------------------------------------------------------------------

#ifndef R_LIGHTING_H__
#define R_LIGHTING_H__

#include "doomtype.h"

// This could be wider for >8 bit display.
// Indeed, true color support is posibble
// precalculating 24bpp lightmap/colormap LUT.
// from darkening PLAYPAL to all black.
// Could use even more than 32 levels.

typedef byte  lighttable_t; 

// sf: moved from r_main.h for coloured lighting
#define MAXLIGHTZ        128
#define MAXLIGHTSCALE     48
#define LIGHTSCALESHIFT   12
#define LIGHTZSHIFT       20
#define LIGHTZDIV         16.0f

// Lighting constants.

// SoM: I am really speechless at this... just... why?
// Lighting in doom was originally clamped off to just 16 brightness levels
// for sector lighting. Simply changing the constants is enough to change this
// it seriously boggles the mind why this wasn't done in doom from the start 
// except for maybe memory usage savings. 
//#define OLDMAPS
#ifdef OLDMAPS
   #define LIGHTLEVELS       16
   #define LIGHTSEGSHIFT      4
   #define LIGHTBRIGHT        1
#else
   #define LIGHTLEVELS       32
   #define LIGHTSEGSHIFT      3
   #define LIGHTBRIGHT        2
#endif


// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.

#define NUMCOLORMAPS 32

//
// Lighting LUT.
// Used for z-depth cuing per column/row,
//  and other lighting effects (sector ambient, flash).
//

// killough 3/20/98: Allow colormaps to be dynamic (e.g. underwater)
extern int numcolormaps;    // killough 4/4/98: dynamic number of maps
extern lighttable_t **colormaps;
// killough 3/20/98, 4/4/98: end dynamic colormaps

extern int           extralight;

#endif

// EOF

