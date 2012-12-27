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

#include "tables.h" // for angle_t

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
  union d_u
  {
    Mobj* thing;
    line_t* line;
  } d;
};


typedef bool (*traverser_t)(intercept_t *in);

fixed_t P_AproxDistance (fixed_t dx, fixed_t dy);
int     P_PointOnLineSide (fixed_t x, fixed_t y, line_t *line);
int     P_PointOnDivlineSide (fixed_t x, fixed_t y, divline_t *line);
void    P_MakeDivline (line_t *li, divline_t *dl);
fixed_t P_InterceptVector (divline_t *v2, divline_t *v1);
int     P_BoxOnLineSide (fixed_t *tmbox, line_t *ld);

bool ThingIsOnLine(Mobj *t, line_t *l);  // killough 3/15/98

angle_t P_PointToAngle(fixed_t xo, fixed_t yo, fixed_t x, fixed_t y);

//
// P_LogThingPosition
//
// haleyjd 04/15/2010: thing position logging for debugging demo problems.
// Pass a NULL mobj to close the log.
//
#ifdef THING_LOGGING
void P_LogThingPosition(Mobj *mo, const char *caller);
#else
#define P_LogThingPosition(a, b)
#endif

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
