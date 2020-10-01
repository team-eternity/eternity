//
// The Eternity Engine
// Copyright(C) 2017 James Haley, Ioan Chera, et al.
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
// Purpose: linked portal crossing calculations. Separated from p_portal due to
//          becoming too many.
// Authors: Ioan Chera
//

#ifndef P_PORTALCROSS_H_
#define P_PORTALCROSS_H_

#include "m_vector.h"
#include "r_defs.h"

struct sector_t;

//
// P_LinePortalCrossing
//
// ioanch 20160106: function to trace one or more paths through potential line
// portals. Needed because some objects are spawned at given offsets from
// others, and there's no other way to detect line portal change.
//
v2fixed_t P_LinePortalCrossing(fixed_t x, fixed_t y, fixed_t dx, fixed_t dy,
                               int *group = nullptr,
                               const line_t **passed = nullptr);

template <typename T>
inline static v2fixed_t P_LinePortalCrossing(T &&u, fixed_t dx, fixed_t dy,
                                             int *group = nullptr)
{
   return P_LinePortalCrossing(u.x, u.y, dx, dy, group);
}

template <typename T, typename U>
inline static v2fixed_t P_LinePortalCrossing(T &&u, U &&dv, int *group = nullptr)
{
   return P_LinePortalCrossing(u.x, u.y, dv.x, dv.y, group);
}

//
// Portal crossing outcome
//
struct portalcrossingoutcome_t
{
   int finalgroup;            // the final reached group ID
   const line_t *lastpassed;  // the last passed linedef
   bool multipassed;          // whether multiple lines have been passed
};

v2fixed_t P_PrecisePortalCrossing(fixed_t x, fixed_t y, fixed_t dx, fixed_t dy,
                                  portalcrossingoutcome_t &outcome);

//
// P_ExtremeSectorAtPoint
// ioanch 20160107
//
sector_t *P_ExtremeSectorAtPoint(fixed_t x, fixed_t y, surf_e surf,
                                 sector_t *preCalcSector = nullptr);

sector_t *P_ExtremeSectorAtPoint(const Mobj *mo, surf_e surf);
//
// P_TransPortalBlockWalker
// ioanch 20160107
//
bool P_TransPortalBlockWalker(const fixed_t bbox[4], int groupid, bool xfirst,
                              void *data,
                              bool (*func)(int x, int y, int groupid, void *data));

// variant with generic callable
template <typename C> inline static
bool P_TransPortalBlockWalker(const fixed_t bbox[4], int groupid, bool xfirst,
                              C &&callable)
{
   return P_TransPortalBlockWalker(bbox, groupid, xfirst, &callable,
                                   [](int x, int y, int groupid, void *data)
                                   {
                                      auto c = static_cast<C *>(data);
                                      return (*c)(x, y, groupid);
                                   });
}

//
// P_SectorTouchesThing
// ioanch 20160115
//
bool P_SectorTouchesThingVertically(const sector_t *sector, const Mobj *mobj);

// ioanch 20160222
sector_t *P_PointReachesGroupVertically(fixed_t cx, fixed_t cy, fixed_t cmidz,
                                        int cgroupid, int tgroupid,
                                        sector_t *csector, fixed_t midzhint,
                                        uint8_t *floorceiling = nullptr);
sector_t *P_ThingReachesGroupVertically(const Mobj *mo, int groupid,
                                        fixed_t midzhint);

#endif

// EOF

