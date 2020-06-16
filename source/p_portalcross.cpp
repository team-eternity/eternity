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

#include "z_zone.h"

#include "c_io.h"
#include "cam_sight.h"
#include "doomstat.h"
#include "i_system.h"
#include "m_bbox.h"
#include "m_compare.h"
#include "p_maputl.h"
#include "p_portal.h"
#include "p_portalblockmap.h"
#include "p_portalcross.h"
#include "p_setup.h"
#include "polyobj.h"
#include "r_main.h"
#include "r_portal.h"
#include "r_state.h"

//==============================================================================
//
// Line portal crossing
//

//
// Data for the traversing callback for P_LinePortalCrossing
//
struct lineportalcrossing_t
{
   v2fixed_t *cur;
   v2fixed_t *fin;
   int *group;
   const line_t **passed;
   // Careful when adding new fields!
};

//
// Traversing callback for P_LinePortalCrossing
//
static bool PTR_linePortalCrossing(const intercept_t *in, void *vdata,
                                   const divline_t &trace)
{
   const auto &data = *static_cast<lineportalcrossing_t *>(vdata);
   const line_t *line = in->d.line;

   // must be a portal line
   if(!(line->pflags & PS_PASSABLE))
      return true;

   // rule out invalid cases
   if(in->frac <= 0 || in->frac > FRACUNIT)
      return true;

   // must be a valid portal line
   const linkdata_t &link = line->portal->data.link;
   // FIXME: does this really happen?
   if(link.fromid == link.toid || (!link.deltax && !link.deltay))
      return true;

   // double check: line MUST be crossed and trace origin must be in front
   int originside = P_PointOnLineSide(trace.x, trace.y, line);
   if(originside != 0 || originside ==
      P_PointOnLineSide(trace.x + trace.dx, trace.y + trace.dy, line))
   {
      return true;
   }

   // update the fields
   data.cur->x += FixedMul(trace.dx, in->frac) + link.deltax;
   data.cur->y += FixedMul(trace.dy, in->frac) + link.deltay;
   // deltaz doesn't matter because we're not using Z here
   data.fin->x += link.deltax;
   data.fin->y += link.deltay;
   if(data.group)
      *data.group = link.toid;
   if(data.passed)
      *data.passed = line;

   return false;
}

//
// P_LinePortalCrossing
//
// ioanch 20160106
// Checks if any line portals are crossed between (x, y) and (x+dx, y+dy),
// updating the target position correctly
//
v2fixed_t P_LinePortalCrossing(fixed_t x, fixed_t y, fixed_t dx, fixed_t dy,
                               int *group, const line_t **passed)
{
   v2fixed_t cur = { x, y };
   v2fixed_t fin = { x + dx, y + dy };

   if((!dx && !dy) || full_demo_version < make_full_version(340, 48) ||
      P_PortalGroupCount() <= 1)
   {
      return fin; // quick return in trivial case
   }

   bool res;

   // number should be as large as possible to prevent accidental exits on valid
   // hyperdetailed maps, but low enough to release the game on time.
   int recprotection = SECTOR_PORTAL_LOOP_PROTECTION;

   do
   {
      lineportalcrossing_t data;
      data.cur = &cur;
      data.fin = &fin;
      data.group = group;
      data.passed = passed;
      res = CAM_PathTraverse(cur.x, cur.y, fin.x, fin.y,
                             CAM_ADDLINES | CAM_REQUIRELINEPORTALS, &data,
                             PTR_linePortalCrossing);
      --recprotection;

      // Continue looking for portals after passing through one, from the new
      // coordinates
   } while (!res && recprotection);

   if(!recprotection)
      C_Printf("Warning: P_PortalCrossing loop");

   return fin;
}

//==============================================================================
//
// Precise portal crossing section
//

//
// The data
//
struct exacttraverse_t
{
   const divline_t *trace;
   const line_t *closest;
   fixed_t closestdist;
};

//
// Checks if the two ends of a divline are projected inside the linedef segment
//
static bool P_divlineInsideLine(const divline_t &dl, const line_t &line)
{
   const v2fixed_t normal = { M_FloatToFixed(line.nx), M_FloatToFixed(line.ny) };
   divline_t pdir = { dl.x, dl.y, normal.x, normal.y };
   const v2fixed_t lv1 = { line.v1->x, line.v1->y };
   const v2fixed_t lv2 = { lv1.x + line.dx, lv1.y + line.dy };
   if(P_PointOnDivlineSide(lv1.x, lv1.y, &pdir) == P_PointOnDivlineSide(lv2.x, lv2.y, &pdir))
      return false;
   pdir.x += dl.dx;
   pdir.y += dl.dy;
   if(P_PointOnDivlineSide(lv1.x, lv1.y, &pdir) == P_PointOnDivlineSide(lv2.x, lv2.y, &pdir))
      return false;
   return true;
}

//
// The iterator
//
static bool PIT_exactTraverse(const line_t &line, void *vdata)
{
   // only use wall portals, not edge
   if(!(line.pflags & PS_PASSABLE))
      return true;
   const linkdata_t &link = line.portal->data.link;
   if(link.fromid == link.toid || (!link.deltax && !link.deltay)) // avoid critical problems
      return true;

   auto &data = *static_cast<exacttraverse_t *>(vdata);

   int destside = P_PointOnLineSide(data.trace->x + data.trace->dx, data.trace->y + data.trace->dy,
                                    &line);
   if(destside != 1 || destside == P_PointOnLineSide(data.trace->x, data.trace->y, &line))
      return true;   // doesn't get past it

   // Trace must cross this
   if(P_PointOnDivlineSide(line.v1->x, line.v1->y, data.trace) ==
      P_PointOnDivlineSide(line.v2->x, line.v2->y, data.trace))
   {
      // check against edge cases where the trace crosses validly but the linedef vertices don't
      // sit on different sides of the divline. Happens with traces (almost) parallel in direction.
      if(!P_divlineInsideLine(*data.trace, line))
         return true;
   }

   divline_t dl;
   P_MakeDivline(&line, &dl);
   fixed_t dist = P_InterceptVector(data.trace, &dl);
   if(dist < data.closestdist)
   {
      data.closestdist = dist;
      data.closest = &line;
   }

   return true;
}

//
// The operation. Returns the closest traversed linedef
//
static const line_t *P_exactTraverseClosest(const divline_t &trace, fixed_t &frac)
{
   // Get all map blocks touched by trace's bounding box
   fixed_t bbox[4];
   M_ClearBox(bbox);
   M_AddToBox2(bbox, trace.x, trace.y);
   M_AddToBox2(bbox, trace.x + trace.dx, trace.y + trace.dy);

   bbox[BOXLEFT] -= bmaporgx;
   bbox[BOXRIGHT] -= bmaporgx;
   bbox[BOXBOTTOM] -= bmaporgy;
   bbox[BOXTOP] -= bmaporgy;

   // If the box edges lie exactly on map block edges, push them a bit to ensure edge linedefs are
   // captured
   if(!(bbox[BOXLEFT] & MAPBMASK))
      --bbox[BOXLEFT];
   if(!(bbox[BOXRIGHT] & MAPBMASK))
      ++bbox[BOXRIGHT];
   if(!(bbox[BOXBOTTOM] & MAPBMASK))
      --bbox[BOXBOTTOM];
   if(!(bbox[BOXTOP] & MAPBMASK))
      ++bbox[BOXTOP];

   // Transform into blockmap bounding box
   bbox[BOXLEFT] >>= MAPBLOCKSHIFT;
   bbox[BOXRIGHT] >>= MAPBLOCKSHIFT;
   bbox[BOXBOTTOM] >>= MAPBLOCKSHIFT;
   bbox[BOXTOP] >>= MAPBLOCKSHIFT;

   bbox[BOXLEFT] = eclamp(bbox[BOXLEFT], 0, bmapwidth - 1);
   bbox[BOXRIGHT] = eclamp(bbox[BOXRIGHT], 0, bmapwidth - 1);
   bbox[BOXBOTTOM] = eclamp(bbox[BOXBOTTOM], 0, bmapheight - 1);
   bbox[BOXTOP] = eclamp(bbox[BOXTOP], 0, bmapheight - 1);

   edefstructvar(exacttraverse_t, data);
   data.trace = &trace;
   data.closestdist = D_MAXINT;

   // Collect all linedefs from this map block
   for(int y = bbox[BOXBOTTOM]; y <= bbox[BOXTOP]; ++y)
      for(int x = bbox[BOXLEFT]; x <= bbox[BOXRIGHT]; ++x)
      {
         int b = y * bmapwidth + x;
         const PODCollection<portalblockentry_t> &block = gPortalBlockmap[b];
         for(const portalblockentry_t &entry : block)
         {
            if(entry.type != portalblocktype_e::line)
               continue;
            if(!PIT_exactTraverse(*entry.line, &data))
               goto nextblock;
         }
      nextblock:
         ;
      }

   frac = data.closestdist;
   return data.closest;
}

//
// As above, but focused for line portal crossing by walking things, where it's critical not to miss
//
v2fixed_t P_PrecisePortalCrossing(fixed_t x, fixed_t y, fixed_t dx, fixed_t dy,
                                  portalcrossingoutcome_t &outcome)
{
   v2fixed_t cur = { x, y };
   v2fixed_t fin = { x + dx, y + dy };
   if((!dx && !dy) || full_demo_version < make_full_version(340, 48) ||
      P_PortalGroupCount() <= 1)
   {
      return fin; // quick return in trivial case
   }

   // number should be as large as possible to prevent accidental exits on valid
   // hyperdetailed maps, but low enough to release the game on time.
   int recprotection = SECTOR_PORTAL_LOOP_PROTECTION;

   const line_t *crossed;
   int crosscount = 0;
   do
   {
      divline_t trace = { cur.x, cur.y, fin.x - cur.x, fin.y - cur.y };
      fixed_t frac;
      crossed = P_exactTraverseClosest(trace, frac);
      if(crossed)
      {
         const linkdata_t &ldata = crossed->portal->data.link;
         cur.x += ldata.deltax + FixedMul(fin.x - cur.x, frac);
         cur.y += ldata.deltay + FixedMul(fin.y - cur.y, frac);
         fin.x += ldata.deltax;
         fin.y += ldata.deltay;
         outcome.finalgroup = ldata.toid;
         outcome.lastpassed = crossed;
         if(++crosscount >= 2)
            outcome.multipassed = true;
      }
      --recprotection;
   } while(crossed && recprotection);

   return fin;
}

//==============================================================================

//
// P_ExtremeSectorAtPoint
//
// ioanch 20160107
// If point x/y resides in a sector with portal, pass through it
//
sector_t *P_ExtremeSectorAtPoint(fixed_t x, fixed_t y, surf_e surf,
                                 sector_t *sector)
{
   if(!sector) // if not preset
      sector = R_PointInSubsector(x, y)->sector;

   int numgroups = P_PortalGroupCount();
   if(numgroups <= 1 || full_demo_version < make_full_version(340, 48) ||
      !gMapHasSectorPortals || sector->groupid == R_NOGROUP)
   {
      return sector; // just return the current sector in this case
   }

   int loopprotection = SECTOR_PORTAL_LOOP_PROTECTION;

   while(sector->srf[surf].pflags & PS_PASSABLE && loopprotection--)
   {
      const linkdata_t &link = sector->srf[surf].portal->data.link;

      // Also quit early if the planez is obscured by a dynamic horizontal plane
      // or if deltax and deltay are somehow zero
      if((!(sector->srf[surf].pflags & PF_ATTACHEDPORTAL) &&
         isInner(surf, sector->srf[surf].height, link.planez)) || (!link.deltax && !link.deltay))
      {
         return sector;
      }

      // adding deltaz doesn't matter because a sector portal MUST be in a
      // different location

      // move into the new sector
      x += link.deltax;
      y += link.deltay;
      sector = R_PointInSubsector(x, y)->sector;
   }

   if(loopprotection < 0)
      C_Printf("Warning: P_ExtremeSectorAtPoint loop");

   return sector;
}

sector_t *P_ExtremeSectorAtPoint(const Mobj *mo, surf_e surf)
{
   return P_ExtremeSectorAtPoint(mo->x, mo->y, surf, mo->subsector->sector);
}

//==============================================================================
//
// Trans portal block walking
//

//
// P_simpleBlockWalker
//
// ioanch 20160108: simple variant of the function below, for maps without
// portals
//
inline static bool P_simpleBlockWalker(const fixed_t bbox[4], bool xfirst, void *data,
                                       bool (*func)(int x, int y, int groupid, void *data))
{
   int xl = (bbox[BOXLEFT] - bmaporgx) >> MAPBLOCKSHIFT;
   int xh = (bbox[BOXRIGHT] - bmaporgx) >> MAPBLOCKSHIFT;
   int yl = (bbox[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
   int yh = (bbox[BOXTOP] - bmaporgy) >> MAPBLOCKSHIFT;

   if(xl < 0)
      xl = 0;
   if(yl < 0)
      yl = 0;
   if(xh >= bmapwidth)
      xh = bmapwidth - 1;
   if(yh >= bmapheight)
      yh = bmapheight - 1;

   int x, y;
   if(xfirst)
   {
      for(x = xl; x <= xh; ++x)
         for(y = yl; y <= yh; ++y)
            if(!func(x, y, R_NOGROUP, data))
               return false;
   }
   else
      for(y = yl; y <= yh; ++y)
         for(x = xl; x <= xh; ++x)
            if(!func(x, y, R_NOGROUP, data))
               return false;

   return true;
}

//
// Checks if a portal blockmap entry touches a box
//
static bool P_boxTouchesBlockPortal(const portalblockentry_t &entry, const fixed_t bbox[4])
{
   auto checkline = [bbox](const line_t &line) {
      if(!M_BoxesTouch(line.bbox, bbox))
         return false;
      return P_BoxOnLineSide(bbox, &line) == -1;
   };

   if(entry.type == portalblocktype_e::line)
      return checkline(*entry.line);
   // Sector
   // Quick check for location
   const sector_t &sector = *entry.sector;
   if(!M_BoxesTouch(pSectorBoxes[&sector - sectors].box, bbox))
      return false;
   if(R_PointInSubsector(bbox[BOXLEFT] / 2 + bbox[BOXRIGHT] / 2,
      bbox[BOXBOTTOM] / 2 + bbox[BOXTOP] / 2)->sector == &sector)
   {
      return true;
   }
   for(int i = 0; i < sector.linecount; ++i)
      if(checkline(*sector.lines[i]))
         return true;
   return false;
}

//
// P_TransPortalBlockWalker
//
// ioanch 20160107
// Having a bounding box in a group id, visit all blocks it touches as well as
// whatever is behind portals
//
bool P_TransPortalBlockWalker(const fixed_t bbox[4], int groupid, bool xfirst,
                              void *data,
                              bool (*func)(int x, int y, int groupid, void *data))
{
   int gcount = P_PortalGroupCount();
   if(gcount <= 1 || groupid == R_NOGROUP ||
      full_demo_version < make_full_version(340, 48))
   {
      return P_simpleBlockWalker(bbox, xfirst, data, func);
   }

   // OPTIMIZE: if needed, use some global store instead of malloc
   bool *accessedgroupids = ecalloc(bool *, gcount, sizeof(*accessedgroupids));
   accessedgroupids[groupid] = true;
   auto portalqueue = ecalloc(const portalblockentry_t **, gcount, sizeof(portalblockentry_t *));
   int queuehead = 0;
   int queueback = 0;

   // initialize link with zero link because we're visiting the current area.
   // groupid has already been set in the parameter
   const linkoffset_t *link = &zerolink;

   fixed_t movedBBox[4] = { bbox[0], bbox[1], bbox[2], bbox[3] };

   do
   {
      movedBBox[BOXLEFT] += link->x;
      movedBBox[BOXRIGHT] += link->x;
      movedBBox[BOXBOTTOM] += link->y;
      movedBBox[BOXTOP] += link->y;
      // set the blocks to be visited
      int xl = (movedBBox[BOXLEFT] - bmaporgx) >> MAPBLOCKSHIFT;
      int xh = (movedBBox[BOXRIGHT] - bmaporgx) >> MAPBLOCKSHIFT;
      int yl = (movedBBox[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
      int yh = (movedBBox[BOXTOP] - bmaporgy) >> MAPBLOCKSHIFT;

      if(xl < 0)
         xl = 0;
      if(yl < 0)
         yl = 0;
      if(xh >= bmapwidth)
         xh = bmapwidth - 1;
      if(yh >= bmapheight)
         yh = bmapheight - 1;

      // Define a function to use in the 'for' blocks
      auto operate = [accessedgroupids, portalqueue, &queueback, func,
                      &groupid, data, gcount, &movedBBox] (int x, int y) -> bool
      {
         // Check for portals
         const PODCollection<portalblockentry_t> &block = gPortalBlockmap[y * bmapwidth + x];
         for(const portalblockentry_t &entry : block)
         {
            if(accessedgroupids[entry.ldata->toid])
               continue;
            if(entry.type == portalblocktype_e::sector &&
               ((!entry.isceiling && !(entry.sector->srf.floor.pflags & PS_PASSABLE)) ||
                ( entry.isceiling && !(entry.sector->srf.ceiling.pflags & PS_PASSABLE)) ))
            {
               continue;   // be careful to skip concealed portals.
            }
            if(!P_boxTouchesBlockPortal(entry, movedBBox))
               continue;

            accessedgroupids[entry.ldata->toid] = true;
            portalqueue[queueback++] = &entry;
            
            P_FitLinkOffsetsToPortal(*entry.ldata);
         }

         // now call the function
         if(!func(x, y, groupid, data))
         {
            // make it possible to escape by func returning false
            return false;
         }
         return true;
      };

      int x, y;
      if(xfirst)
      {
         for(x = xl; x <= xh; ++x)
            for(y = yl; y <= yh; ++y)
               if(!operate(x, y))
               {
                  efree(portalqueue);
                  efree(accessedgroupids);
                  return false;
               }
      }
      else
         for(y = yl; y <= yh; ++y)
            for(x = xl; x <= xh; ++x)
               if(!operate(x, y))
               {
                  efree(portalqueue);
                  efree(accessedgroupids);
                  return false;
               }


      // look at queue
      link = &zerolink;
      if(queuehead < queueback)
      {
         do
         {
            link = P_GetLinkOffset(groupid, portalqueue[queuehead]->ldata->toid);
            ++queuehead;

            // make sure to reject trivial (zero) links
         } while(!link->x && !link->y && queuehead < queueback);

         // if a valid link has been found, also update current groupid
         if(link->x || link->y)
            groupid = portalqueue[queuehead - 1]->ldata->toid;
      }

      // if a valid link has been found (and groupid updated) continue
   } while(link->x || link->y);

   // we now have the list of accessedgroupids
   
   efree(portalqueue);
   efree(accessedgroupids);
   return true;
}

//==============================================================================

//
// P_SectorTouchesThingVertically
//
// ioanch 20160115: true if a thing touches a sector vertically
//
bool P_SectorTouchesThingVertically(const sector_t *sector, const Mobj *mobj)
{
   fixed_t topz = mobj->z + mobj->height;
   if(topz < sector->srf.floor.height || mobj->z > sector->srf.ceiling.height)
      return false;
   if(sector->srf.floor.pflags & PS_PASSABLE && topz < P_FloorPortalZ(*sector))
      return false;
   if(sector->srf.ceiling.pflags & PS_PASSABLE && mobj->z > P_CeilingPortalZ(*sector))
      return false;
   return true;
}

//==============================================================================

//
// P_ThingReachesGroupVertically
//
// Simple function that just checks if mobj is in a position that vertically
// points to groupid. This does NOT change gGroupVisit.
//
// Returns the sector that's reached, or nullptr if group is not reached
//
sector_t *P_PointReachesGroupVertically(fixed_t cx, fixed_t cy, fixed_t cmidz,
                                        int cgroupid, int tgroupid,
                                        sector_t *csector, fixed_t midzhint,
                                        uint8_t *floorceiling)
{
   if(cgroupid == tgroupid)
      return csector;

   static bool *groupVisit;
   if(!groupVisit)
   {
      groupVisit = ecalloctag(bool *, P_PortalGroupCount(),
                              sizeof(*groupVisit), PU_LEVEL, (void**)&groupVisit);
   }
   else
      memset(groupVisit, 0, sizeof(*groupVisit) * P_PortalGroupCount());
   groupVisit[cgroupid] = true;

   uint8_t fcflag[2];

   surf_e surfs[2];

   if(midzhint < cmidz)
   {
      surfs[0] = surf_floor;
      surfs[1] = surf_ceil;
      fcflag[0] = sector_t::floor;
      fcflag[1] = sector_t::ceiling;
   }
   else
   {
      surfs[0] = surf_ceil;
      surfs[1] = surf_floor;
      fcflag[0] = sector_t::ceiling;
      fcflag[1] = sector_t::floor;
   }

   sector_t *sector;
   int groupid;
   fixed_t x, y;

   for(int i = 0; i < 2; ++i)
   {
      sector = csector;
      x = cx;
      y = cy;

      while(sector->srf[surfs[i]].pflags & PS_PASSABLE)
      {
         const linkdata_t &ldata = sector->srf[surfs[i]].portal->data.link;
         x += ldata.deltax;
         y += ldata.deltay;
         sector = R_PointInSubsector(x, y)->sector;
         groupid = sector->groupid;
         if(groupid == tgroupid)
         {
            if(floorceiling)
               *floorceiling = fcflag[i];
            return sector;
         }
         if(groupVisit[groupid])
            break;
         groupVisit[groupid] = true;
      }
   }
   return nullptr;
}

sector_t *P_ThingReachesGroupVertically(const Mobj *mo, int groupid,
                                        fixed_t midzhint)
{
   return P_PointReachesGroupVertically(mo->x, mo->y, mo->z + mo->height / 2,
                                        mo->groupid, groupid, mo->subsector->sector, midzhint);
}

// EOF

