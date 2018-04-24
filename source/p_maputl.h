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
//      Map utility functions
//
//-----------------------------------------------------------------------------

#ifndef P_MAPUTL_H__
#define P_MAPUTL_H__

#include "linkoffs.h"   // ioanch 20160108: for R_NOGROUP
#include "m_vector.h"
#include "tables.h" // for angle_t

struct line_t;
class  Mobj;
struct mobjinfo_t;
struct polyobj_s; // ioanch 20160114

// mapblocks are used to check movement against lines and things
#define MAPBLOCKUNITS   128
#define MAPBLOCKSIZE    (MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT   (FRACBITS+7)
#define MAPBMASK        (MAPBLOCKSIZE-1)
#define MAPBTOFRAC      (MAPBLOCKSHIFT-FRACBITS)

#define PT_ADDLINES     1
#define PT_ADDTHINGS    2
#define PT_EARLYOUT     4

struct divline_t
{
   fixed_t     x;
   fixed_t     y;
   fixed_t     dx;
   fixed_t     dy;
};

//
// A divline_t with a normal included
//
struct dlnormal_t
{
   divline_t dl;
   float nx;      // likely copied from linedef
   float ny;
};

// SoM: linetracer_t contains a divline_t for the appropriate functions but 
// holds much more data which is needed for making tracers correctly travel
// through portals
struct linetracer_t
{
   divline_t   dl;

   // Moved crappy globals here
   fixed_t     z;            // replaces shootz
   int         la_damage;
   fixed_t     attackrange;
   fixed_t     aimslope;
   fixed_t     topslope;
   fixed_t     bottomslope;
   Mobj       *thing;
   uint32_t    aimflagsmask; // killough 8/2/98: for more intelligent autoaiming
   fixed_t     sin;
   fixed_t     cos;
};

struct intercept_t
{
  fixed_t     frac;           // along trace line
  bool        isaline;
  union d_u
  {
    Mobj   *thing;
    line_t *line;
  } d;
};

typedef bool (*traverser_t)(intercept_t *in, void *context);

fixed_t P_AproxDistance(fixed_t dx, fixed_t dy);
int     P_PointOnLineSide(fixed_t x, fixed_t y, const line_t *line);
int     P_PointOnDivlineSide(fixed_t x, fixed_t y, const divline_t *line);
void    P_MakeDivline(const line_t *li, divline_t *dl);
fixed_t P_InterceptVector(const divline_t *v2, const divline_t *v1);
int     P_BoxOnLineSide(const fixed_t *tmbox, const line_t *ld);
// ioanch 20160123: for linedef portal clipping.
v2fixed_t P_BoxLinePoint(const fixed_t bbox[4], const line_t *ld);
bool P_BoxesIntersect(const fixed_t bbox1[4], const fixed_t bbox2[4]);
int P_BoxOnDivlineSide(const fixed_t *tmbox, const divline_t &dl);

//SoM 9/2/02: added mo parameter for 3dside clipping
// ioanch 20150113: added optional portal detection
void    P_LineOpening (const line_t *linedef, const Mobj *mo,
                       bool portaldetect = false, uint32_t *lineclipflags = nullptr);

void P_UnsetThingPosition(Mobj *thing);
void P_SetThingPosition(Mobj *thing);
bool P_BlockLinesIterator (int x, int y, bool func(line_t *, polyobj_s *, void *),
                           int groupid = R_NOGROUP, void *context = nullptr);
bool P_BlockThingsIterator(int x, int y, int groupid, bool (*func)(Mobj *));
inline static bool P_BlockThingsIterator(int x, int y, bool func(Mobj *))
{
   // ioanch 20160108: avoid code duplication
   return P_BlockThingsIterator(x, y, R_NOGROUP, func);
}
bool ThingIsOnLine(const Mobj *t, const line_t *l);  // killough 3/15/98
bool P_PathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                    int flags, traverser_t trav, void *context = nullptr);

angle_t P_PointToAngle(fixed_t xo, fixed_t yo, fixed_t x, fixed_t y);

bool P_ShootThing(const intercept_t *in,
                  Mobj *shooter,
                  fixed_t attackrange_local,
                  fixed_t sourcez,
                  fixed_t aimslope,
                  fixed_t attackrange_total,
                  const divline_t &dl,
                  size_t puffidx,
                  int damage);

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
