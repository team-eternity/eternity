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
struct lineopening_t;
class  Mobj;
struct mobjinfo_t;
struct polyobj_t; // ioanch 20160114
struct sector_t;
struct subsector_t;

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
   union
   {
      struct
      {
         fixed_t     x;
         fixed_t     y;
      };
      v2fixed_t v;
   };
   union
   {
      struct
      {
         fixed_t     dx;
         fixed_t     dy;
      };
      v2fixed_t dv;
   };
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
   bool        aimflagsmask; // killough 8/2/98: for more intelligent autoaiming
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
inline static fixed_t P_AproxDistance(v2fixed_t dv)
{
   return P_AproxDistance(dv.x, dv.y);
}

int P_PointOnLineSideClassic(fixed_t x, fixed_t y, const line_t *line);
int P_PointOnLineSidePrecise(fixed_t x, fixed_t y, const line_t *line);
extern int (*P_PointOnLineSide)(fixed_t x, fixed_t y, const line_t *line);

int P_PointOnDivlineSideClassic(fixed_t x, fixed_t y, const divline_t *line);
int P_PointOnDivlineSidePrecise(fixed_t x, fixed_t y, const divline_t *line);
extern int (*P_PointOnDivlineSide)(fixed_t x, fixed_t y, const divline_t *line);

void    P_MakeDivline(const line_t *li, divline_t *dl);
fixed_t P_InterceptVector(const divline_t *v2, const divline_t *v1);
int     P_BoxOnLineSide(const fixed_t *tmbox, const line_t *ld);
// ioanch 20160123: for linedef portal clipping.
v2fixed_t P_BoxLinePoint(const fixed_t bbox[4], const line_t *ld);
int P_LineIsCrossed(const line_t &line, const divline_t &dl);
bool P_IsInVoid(fixed_t x, fixed_t y, const subsector_t &ss);
bool P_BoxesIntersect(const fixed_t bbox1[4], const fixed_t bbox2[4]);
int P_BoxOnDivlineSideFloat(const float *box, v2float_t start, v2float_t delta);

//SoM 9/2/02: added mo parameter for 3dside clipping
// ioanch 20150113: added optional portal detection

lineopening_t P_LineOpening(const line_t *linedef, const Mobj *mo,
                            const v2fixed_t *ppoint = nullptr, bool portaldetect = false,
                            uint32_t *lineclipflags = nullptr);
lineopening_t P_SlopeOpening(v2fixed_t pos);
lineopening_t P_SlopeOpeningPortalAware(v2fixed_t pos);

void P_UnsetThingPosition(Mobj *thing);
void P_SetThingPosition(Mobj *thing);
bool P_BlockLinesIterator (int x, int y, bool func(line_t *, polyobj_t *, void *),
                           int groupid = R_NOGROUP, void *context = nullptr);
bool P_BlockThingsIterator(int x, int y, int groupid, bool (*func)(Mobj *, void *),
                           void *context = nullptr);
inline static bool P_BlockThingsIterator(int x, int y, bool func(Mobj *, void *),
                                         void *context = nullptr)
{
   // ioanch 20160108: avoid code duplication
   return P_BlockThingsIterator(x, y, R_NOGROUP, func, context);
}

void P_ExactBoxLinePoints(const fixed_t *tmbox, const line_t &line, v2fixed_t &i1,
                          v2fixed_t &i2);

bool ThingIsOnLine(const Mobj *t, const line_t *l);  // killough 3/15/98
bool P_PathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                    int flags, traverser_t trav, void *context = nullptr);

angle_t P_PointToAngle(fixed_t xo, fixed_t yo, fixed_t x, fixed_t y);
angle_t P_DoubleToAngle(double a);

void P_RotatePoint(fixed_t &x, fixed_t &y, const angle_t angle);

bool P_CheckShootPlane(const sector_t &sidesector, fixed_t origx, fixed_t origy,
                       fixed_t origz, fixed_t aimslope, v2fixed_t prevedgepos,
                       fixed_t prevfrac, fixed_t attackrange,
                       fixed_t shootcos, fixed_t shootsin,
                       fixed_t &x, fixed_t &y, fixed_t &z, bool &hitplane, int &updown);
bool P_CheckShootSkyHack(const line_t &li, fixed_t x, fixed_t y, fixed_t z);
bool P_CheckShootSkyLikeEdgePortal(const line_t &li, v2fixed_t edgepos, fixed_t z);

bool P_ShootThing(const intercept_t *in,
                  Mobj *shooter,
                  fixed_t attackrange_local,
                  fixed_t sourcez,
                  fixed_t aimslope,
                  fixed_t attackrange_total,
                  const divline_t &dl,
                  size_t puffidx,
                  int damage);
bool P_CheckThingAimAvailability(const Mobj *th, const Mobj *source, bool aimflagsmask);
bool P_CheckThingAimSlopes(const Mobj *th, fixed_t origindist, fixed_t infrac, linetracer_t &atrace);

v2fixed_t P_GetSafeLineNormal(const line_t &line);
bool P_SegmentIntersectsSector(v2fixed_t v1, v2fixed_t v2, const sector_t &sector);

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
