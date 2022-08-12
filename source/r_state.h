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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Refresh/render internal state variables (global).
//
//-----------------------------------------------------------------------------

#ifndef R_STATE_H__
#define R_STATE_H__

// Required for: lighttable_t (FIXME?), angle_t
#include "doomdef.h"
#include "r_lighting.h"
#include "tables.h"

struct camera_t;
struct line_t;
struct node_t;
struct fnode_t;
struct player_t;
struct sector_t;
struct sectorbox_t;
struct sectorinterp_t;
struct seg_t;
struct side_t;
struct soundzone_t;
struct spritedef_t;
struct subsector_t;
struct vertex_t;
struct visplane_t;

//
// Refresh internal data structures,
//  for rendering.
//

// needed for pre rendering (fracs)
extern fixed_t *spritewidth;
extern fixed_t *spriteoffset;
extern fixed_t *spritetopoffset;
// SoM: Needed for cardboard
extern float   *spriteheight;
// ioanch: portal sprite copying cache info
struct spritespan_t
{
   float top;
   float bottom;
   float side;
};
extern spritespan_t **r_spritespan;

extern lighttable_t **colormaps;         // killough 3/20/98, 4/4/98

extern int firstflat;

// SoM: Because all textures and flats are stored in the same array, the 
// translation tables are now combined.
extern int         *texturetranslation;

// Sprite....
extern int firstspritelump;
extern int lastspritelump;
extern int numspritelumps;

//
// Lookup tables for map data.
//
extern int              numsprites;
extern spritedef_t      *sprites;

extern int              numvertexes;
extern vertex_t         *vertexes;

extern int              numsegs;
extern seg_t            *segs;

extern int              numsectors;
extern sector_t         *sectors;
extern sectorinterp_t   *sectorinterps;
extern sectorbox_t      *pSectorBoxes;

extern int              numsoundzones;
extern soundzone_t      *soundzones;

extern int              numsubsectors;
extern subsector_t      *subsectors;

extern int              numnodes;
extern node_t           *nodes;
extern fnode_t          *fnodes;

extern int              numlines;
extern int              numlinesPlusExtra;
extern line_t           *lines;

extern int              numsides;
extern side_t           *sides;

        // sf: for scripting
extern int              numthings;

//
// POV data.
//
extern const player_t   *viewplayer;
extern camera_t         *viewcamera;
extern angle_t          clipangle;
extern int              viewangletox[FINEANGLES/2];
extern angle_t          *xtoviewangle;  // killough 2/8/98

#endif

//----------------------------------------------------------------------------
//
// $Log: r_state.h,v $
// Revision 1.6  1998/05/01  14:49:12  killough
// beautification
//
// Revision 1.5  1998/04/06  04:40:54  killough
// Make colormaps completely dynamic
//
// Revision 1.4  1998/03/23  03:39:48  killough
// Add support for arbitrary number of colormaps
//
// Revision 1.3  1998/02/09  03:23:56  killough
// Change array decl to use MAX screen width/height
//
// Revision 1.2  1998/01/26  19:27:47  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:09  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
