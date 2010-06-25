// Emacs style mode select   -*- C -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Map utility functions
//
//-----------------------------------------------------------------------------

#ifndef __P_MAPUTL__
#define __P_MAPUTL__

#include "r_defs.h"

// mapblocks are used to check movement against lines and things
#define MAPBLOCKUNITS   128
#define MAPBLOCKSIZE    (MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT   (FRACBITS+7)
#define MAPBMASK        (MAPBLOCKSIZE-1)
#define MAPBTOFRAC      (MAPBLOCKSHIFT-FRACBITS)

#define PT_ADDLINES     1
#define PT_ADDTHINGS    2
#define PT_EARLYOUT     4

typedef struct divline_s
{
   fixed_t     x;
   fixed_t     y;
   fixed_t     dx;
   fixed_t     dy;
} divline_t;


// SoM: linetracer_t can be cast to divline_t for the appropriate functions but holds much more
// data which is needed for making tracers correctly travel through portals
typedef struct linetracer_s
{
   fixed_t     x;
   fixed_t     y;
   fixed_t     dx;
   fixed_t     dy;

   // Moved crappy globals here
   fixed_t     z; // replaces shootz
   int         la_damage;
   fixed_t     attackrange;
   fixed_t     aimslope;
   fixed_t     topslope, bottomslope;

   // SoM: used by aiming TPT
   fixed_t     originx;
   fixed_t     originy;
   fixed_t     originz;

   fixed_t     sin;
   fixed_t     cos;

   // Accumulated travel along the line. Should be the XY distance between (x,y) 
   // and (originx, originy) 
   fixed_t movefrac;


   boolean finished;
} linetracer_t;



typedef struct intercept_s
{
  fixed_t     frac;           // along trace line
  boolean     isaline;
  union {
    mobj_t* thing;
    line_t* line;
  } d;
} intercept_t;

typedef boolean (*traverser_t)(intercept_t *in);

fixed_t P_AproxDistance (fixed_t dx, fixed_t dy);
int     P_PointOnLineSide (fixed_t x, fixed_t y, line_t *line);
int     P_PointOnDivlineSide (fixed_t x, fixed_t y, divline_t *line);
void    P_MakeDivline (line_t *li, divline_t *dl);
fixed_t P_InterceptVector (divline_t *v2, divline_t *v1);
int     P_BoxOnLineSide (fixed_t *tmbox, line_t *ld);

//SoM 9/2/02: added mo parameter for 3dside clipping
void    P_LineOpening (line_t *linedef, mobj_t *mo);

void    P_UnsetThingPosition(mobj_t *thing);
void    P_SetThingPosition(mobj_t *thing);
boolean P_BlockLinesIterator (int x, int y, boolean func(line_t *));
boolean P_BlockThingsIterator(int x, int y, boolean func(mobj_t *));
boolean ThingIsOnLine(mobj_t *t, line_t *l);  // killough 3/15/98
boolean P_PathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                           int flags, boolean trav(intercept_t *));

angle_t P_PointToAngle(fixed_t xo, fixed_t yo, fixed_t x, fixed_t y);

extern linetracer_t trace;

#endif  // __P_MAPUTL__

//----------------------------------------------------------------------------
//
// $Log: p_maputl.h,v $
// Revision 1.1  1998/05/03  22:19:26  killough
// External declarations formerly in p_local.h
//
//
//----------------------------------------------------------------------------
