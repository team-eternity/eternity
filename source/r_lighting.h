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
// Purpose: Light tables and related constants
//  Split out of r_defs.h for dependency-hell resolution.
//
// Authors: James Haley, Max Waine
//

#ifndef R_LIGHTING_H__
#define R_LIGHTING_H__

#include "doomtype.h"

// This could be wider for >8 bit display.
// Indeed, true color support is posibble
// precalculating 24bpp lightmap/colormap LUT.
// from darkening PLAYPAL to all black.
// Could use even more than 32 levels.

using lighttable_t = byte;

// sf: moved from r_main.h for coloured lighting
static constexpr int   MAXLIGHTZ       = 128;
static constexpr int   MAXLIGHTSCALE   = 48;
static constexpr int   LIGHTSCALESHIFT = 12;
static constexpr int   LIGHTZSHIFT     = 20;
static constexpr float LIGHTZDIV       = 16.0f;

// Lighting constants.

// clang-format off

// SoM: I am really speechless at this... just... why?
// Lighting in doom was originally clamped off to just 16 brightness levels
// for sector lighting. Simply changing the constants is enough to change this
// it seriously boggles the mind why this wasn't done in doom from the start
// except for maybe memory usage savings.
//#define OLDMAPS
#ifdef OLDMAPS
    static constexpr int LIGHTLEVELS   = 16;
    static constexpr int LIGHTSEGSHIFT =  4;
    static constexpr int LIGHTBRIGHT   =  1;
#else
    static constexpr int LIGHTLEVELS   = 32;
    static constexpr int LIGHTSEGSHIFT =  3;
    static constexpr int LIGHTBRIGHT   =  2;
#endif

// clang-format on

// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.

static constexpr int NUMCOLORMAPS = 32;

//
// Lighting LUT.
// Used for z-depth cuing per column/row,
//  and other lighting effects (sector ambient, flash).
//

// killough 3/20/98: Allow colormaps to be dynamic (e.g. underwater)
extern int            numcolormaps; // killough 4/4/98: dynamic number of maps
extern lighttable_t **colormaps;
// killough 3/20/98, 4/4/98: end dynamic colormaps

extern int extralight;

#endif

// EOF

