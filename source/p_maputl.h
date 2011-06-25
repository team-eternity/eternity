// Emacs style mode select   -*- C++ -*- 
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

#ifndef P_MAPUTL_H__
#define P_MAPUTL_H__

struct sector_t;
struct line_t;
class  Mobj;
class  MapContext;
class  ClipContext;

// mapblocks are used to check movement against lines and things
#define MAPBLOCKUNITS   128
#define MAPBLOCKSIZE    (MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT   (FRACBITS+7)
#define MAPBMASK        (MAPBLOCKSIZE-1)
#define MAPBTOFRAC      (MAPBLOCKSHIFT-FRACBITS)

#define PT_ADDLINES     1
#define PT_ADDTHINGS    2
#define PT_EARLYOUT     4

struct divline_t;


struct intercept_t
{
  fixed_t     frac;           // along trace line
  bool        isaline;
  union {
    Mobj* thing;
    line_t* line;
  } d;
};


struct open_t
{
   // P_LineOpening
   fixed_t    top;      // top of line opening
   fixed_t    bottom;   // bottom of line opening
   fixed_t    range;    // height of opening: top - bottom
   fixed_t    lowfloor;     // lowest floorheight involved   
   fixed_t    secfloor; // SoM 11/3/02: considering only sector floor
   fixed_t    secceil;  // SoM 11/3/02: considering only sector ceiling

   // moved front and back outside P_LineOpening and changed -- phares 3/7/98
   // them to these so we can pick up the new friction value
   // in PIT_CheckLine()
   sector_t   *frontsector; // made global
   sector_t   *backsector;  // made global
};

extern open_t opening;


typedef bool (*traverser_t)(intercept_t *in);

fixed_t P_AproxDistance (fixed_t dx, fixed_t dy);
int     P_PointOnLineSide (fixed_t x, fixed_t y, line_t *line);
int     P_PointOnDivlineSide (fixed_t x, fixed_t y, divline_t *line);
void    P_MakeDivline (line_t *li, divline_t *dl);
fixed_t P_InterceptVector (divline_t *v2, divline_t *v1);
int     P_BoxOnLineSide (fixed_t *tmbox, line_t *ld);

//SoM 9/2/02: added mo parameter for 3dside clipping
void    P_LineOpening (line_t *linedef, Mobj *mo, ClipContext *cc);

void P_UnsetThingPosition(Mobj *thing);
void P_SetThingPosition(Mobj *thing);
bool ThingIsOnLine(Mobj *t, line_t *l);  // killough 3/15/98

angle_t P_PointToAngle(fixed_t xo, fixed_t yo, fixed_t x, fixed_t y);



bool P_BlockLinesIterator (int x, int y, bool func(line_t *, MapContext *), MapContext *c);
bool P_BlockThingsIterator(int x, int y, bool func(Mobj   *, MapContext *), MapContext *c);



#endif  // __P_MAPUTL__

//----------------------------------------------------------------------------
//
// $Log: p_maputl.h,v $
// Revision 1.1  1998/05/03  22:19:26  killough
// External declarations formerly in p_local.h
//
//
//----------------------------------------------------------------------------
