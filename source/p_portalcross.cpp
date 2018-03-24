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
#include "p_maputl.h"
#include "p_portal.h"
#include "p_portalcross.h"
#include "p_setup.h"
#include "polyobj.h"
#include "r_main.h"
#include "r_portal.h"

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
   bool *passed;
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
      *data.passed = true;

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
                               int *group, bool *passed)
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
// P_ExtremeSectorAtPoint
//
// ioanch 20160107
// If point x/y resides in a sector with portal, pass through it
//
sector_t *P_ExtremeSectorAtPoint(fixed_t x, fixed_t y, bool ceiling,
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

   auto pflags = ceiling ? &sector_t::c_pflags : &sector_t::f_pflags;
   auto portal = ceiling ? &sector_t::c_portal : &sector_t::f_portal;

   int loopprotection = SECTOR_PORTAL_LOOP_PROTECTION;

   while(sector->*pflags & PS_PASSABLE && loopprotection--)
   {
      const linkdata_t &link = (sector->*portal)->data.link;

      // Also quit early if the planez is obscured by a dynamic horizontal plane
      // or if deltax and deltay are somehow zero
      if((ceiling ? !(sector->c_pflags & PF_ATTACHEDPORTAL) &&
          sector->ceilingheight < link.planez
          : !(sector->f_pflags & PF_ATTACHEDPORTAL) &&
          sector->floorheight > link.planez) ||
         (!link.deltax && !link.deltay))
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

sector_t *P_ExtremeSectorAtPoint(const Mobj *mo, bool ceiling)
{
   return P_ExtremeSectorAtPoint(mo->x, mo->y, ceiling, mo->subsector->sector);
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
   int *groupqueue = ecalloc(int *, gcount, sizeof(*groupqueue));
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
      auto operate = [accessedgroupids, groupqueue, &queueback, func,
                      &groupid, data, gcount] (int x, int y) -> bool
      {
         // Check for portals
         const portallist_t &block = gBlockGroups[y * bmapwidth + x];
         for(int i = 0; i < block.count; ++i)
         {
            // Add to queue and visitlist
            if(!accessedgroupids[block.links[i]->toid])
            {
               accessedgroupids[block.links[i]->toid] = true;
               groupqueue[queueback++] = block.links[i]->toid;
            }
         }

         // Also check for polyobjects
         for(const DLListItem<polymaplink_t> *plink
             = polyblocklinks[y * bmapwidth + x]; plink; plink = plink->dllNext)
         {
            const polyobj_t *po = (*plink)->po;
            for(size_t i = 0; i < po->numPortals; ++i)
            {
               if(po->portals[i]->type != R_LINKED)
                  continue;
               int groupid = po->portals[i]->data.link.toid;
               // TODO: use the portal itself, not the group ID
               if(!accessedgroupids[groupid])
               {
                  accessedgroupids[groupid] = true;
                  groupqueue[queueback++] = groupid;
               }
            }
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
                  efree(groupqueue);
                  efree(accessedgroupids);
                  return false;
               }
      }
      else
         for(y = yl; y <= yh; ++y)
            for(x = xl; x <= xh; ++x)
               if(!operate(x, y))
               {
                  efree(groupqueue);
                  efree(accessedgroupids);
                  return false;
               }


      // look at queue
      link = &zerolink;
      if(queuehead < queueback)
      {
         do
         {
            link = P_GetLinkOffset(groupid, groupqueue[queuehead]);
            ++queuehead;

            // make sure to reject trivial (zero) links
         } while(!link->x && !link->y && queuehead < queueback);

         // if a valid link has been found, also update current groupid
         if(link->x || link->y)
            groupid = groupqueue[queuehead - 1];
      }

      // if a valid link has been found (and groupid updated) continue
   } while(link->x || link->y);

   // we now have the list of accessedgroupids
   
   efree(groupqueue);
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
   if(topz < sector->floorheight || mobj->z > sector->ceilingheight)
      return false;
   if(sector->f_pflags & PS_PASSABLE && topz < P_FloorPortalZ(*sector))
      return false;
   if(sector->c_pflags & PS_PASSABLE && mobj->z > P_CeilingPortalZ(*sector))
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
                                        sector_t *csector, fixed_t midzhint)
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

   unsigned sector_t::*pflags[2];
   portal_t *sector_t::*portal[2];

   if(midzhint < cmidz)
   {
      pflags[0] = &sector_t::f_pflags;
      pflags[1] = &sector_t::c_pflags;
      portal[0] = &sector_t::f_portal;
      portal[1] = &sector_t::c_portal;
   }
   else
   {
      pflags[0] = &sector_t::c_pflags;
      pflags[1] = &sector_t::f_pflags;
      portal[0] = &sector_t::c_portal;
      portal[1] = &sector_t::f_portal;
   }

   sector_t *sector;
   int groupid;
   fixed_t x, y;

   const linkoffset_t *link;

   for(int i = 0; i < 2; ++i)
   {
      sector = csector;
      groupid = cgroupid;
      x = cx;
      y = cy;

      while(sector->*pflags[i] & PS_PASSABLE)
      {
         link = P_GetLinkOffset(groupid, (sector->*portal[i])->data.link.toid);
         x += link->x;
         y += link->y;
         sector = R_PointInSubsector(x, y)->sector;
         groupid = sector->groupid;
         if(groupid == tgroupid)
            return sector;
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

