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
//  Movement, collision handling.
//  Mobj/Map linking and delinking functions.
//
//-----------------------------------------------------------------------------

#include "m_bbox.h"
#include "m_collection.h"
#include "p_mobj.h"
#include "p_maputl.h"
#include "p_portalclip.h"
#include "p_portal.h"
#include "p_setup.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_portal.h"
#include "r_state.h"


bool LineIntersectsBBox(ClipContext *cc, line_t *line)
{
   return !(cc->bbox[BOXRIGHT]  <= line->bbox[BOXLEFT] || 
      cc->bbox[BOXLEFT]   >= line->bbox[BOXRIGHT]  || 
      cc->bbox[BOXTOP]    <= line->bbox[BOXBOTTOM] || 
      cc->bbox[BOXBOTTOM] >= line->bbox[BOXTOP] ||
      P_BoxOnLineSide(cc->bbox, line) != -1);
};

void CalculateBBoxForThing(ClipContext *cc, fixed_t x, fixed_t y, fixed_t radius, linkoffset_t *link)
{
   cc->bbox[BOXTOP]    = y + radius + link->y;
   cc->bbox[BOXBOTTOM] = y - radius + link->y;
   cc->bbox[BOXRIGHT]  = x + radius + link->x;
   cc->bbox[BOXLEFT]   = x - radius + link->x;
};

void GetBlockmapBoundsFromBBox(ClipContext *cc, int &xl, int &yl, int &xh, int &yh)
{
   xl = (cc->bbox[BOXLEFT  ] - bmaporgx) >> MAPBLOCKSHIFT;
   xh = (cc->bbox[BOXRIGHT ] - bmaporgx) >> MAPBLOCKSHIFT;
   yl = (cc->bbox[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
   yh = (cc->bbox[BOXTOP   ] - bmaporgy) >> MAPBLOCKSHIFT;
};

void HitPortalGroup(int groupid, adjacencytype_e type, ClipContext *cc)
{
   if(cc->getMarkedGroups()->mark(groupid))
      return;

   auto &adjacencydata = cc->adjacent_groups.addNew();
   adjacencydata.group = groupid;
   adjacencydata.linktype = type;
};

void CheckSectorPortals(sector_t *sector, ClipContext *cc)
{
   Mobj *thing = cc->thing;

   if(sector->c_pflags & PS_PASSABLE && thing->z + thing->height > sector->ceilingheight)
      HitPortalGroup(sector->c_portal->data.link.toid, AG_CEILPORTAL, cc);
   
   if(sector->f_pflags & PS_PASSABLE && thing->z < sector->floorheight)
      HitPortalGroup(sector->f_portal->data.link.toid, AG_FLOORPORTAL, cc);
};

//
// Populates the given list with all the portal groups (by index) the mobj touches
bool PIT_FindAdjacentPortals(line_t *line, MapContext *context)
{
   ClipContext *cc = context->clipContext();
   if(!LineIntersectsBBox(cc, line))
      return true;
   
   // Line portals
   if(line->pflags & PS_PASSABLE)
      HitPortalGroup(line->portal->data.link.toid, AG_LINEPORTAL, cc);
   
   // Floor/ceiling portals
   if(line->frontsector)
      CheckSectorPortals(line->frontsector, cc);
   
   if(line->backsector)
      CheckSectorPortals(line->backsector, cc);

   return true;
}


//
// findAdjacentPortals
//
// Does what it says, says what it does.
void PortalClipEngine::findAdjacentPortals(ClipContext *cc)
{
   Mobj *thing = cc->thing;
   int yh, yl, xh, xl;
   int bx, by;

   cc->adjacent_groups.makeEmpty();
   cc->getMarkedGroups()->clearMarks();
   cc->getMarkedLines()->clearMarks();

   HitPortalGroup(cc->thing->subsector->sector->groupid, AG_CURRENTLOCATION, cc);

   for(size_t i = 0; i < cc->adjacent_groups.getLength(); ++i)
   {
      linkoffset_t *link = P_GetLinkOffset(cc->adjacent_groups[0].group, cc->adjacent_groups[i].group);
      
      CalculateBBoxForThing(cc, cc->x, cc->y, cc->thing->radius, link);
      GetBlockmapBoundsFromBBox(cc, xl, yl, xh, yh);
      
      for(by = yl; by <= yh; by++)
      {
         for(bx = xl; bx <= xh; bx++)
         {
            if(bx < 0 || bx >= bmapwidth || by < 0 || by >= bmapheight)
               continue;
            
            blockLinesIterator(bx, by, PIT_FindAdjacentPortals, cc);
         }
      }
   }
}



void PortalClipEngine::unlinkMobjFromSectors(Mobj *mobj)
{
   ClipEngine::unlinkMobjFromSectors(mobj);

   delSectorLinks(mobj->sectorlinks);
   mobj->sectorlinks = NULL;
}


void PortalClipEngine::delSeclist(msecnode_t *node)
{
   I_Error("delSeclist not used in portal clipping engine.");
}

void PortalClipEngine::delSectorLinks(msecnode_t *node)
{
   msecnode_t *rover, *next;

   for(rover = node; rover; rover = next)
   {
      next = rover->m_tnext;

      if(rover->m_snext)
         rover->m_snext->m_sprev = rover->m_sprev;

      if(rover->m_sprev)
         rover->m_sprev->m_snext = rover->m_snext;
      else
         rover->m_sector->thingnodelist = rover->m_snext;

      putSecnode(rover);
   }
}


void PortalClipEngine::delTouchingThingChain(msecnode_t *node)
{
   msecnode_t *rover, *next;

   for(rover = node; rover; rover = next)
   {
      next = rover->m_tnext;

      if(rover->m_snext)
         rover->m_snext->m_sprev = rover->m_sprev;

      if(rover->m_sprev)
         rover->m_sprev->m_snext = rover->m_snext;
      else
         rover->m_sector->touching_thinglist = rover->m_snext;

      putSecnode(rover);
   }
}


void PortalClipEngine::unsetThingPosition(Mobj *thing)
{
   P_LogThingPosition(thing, "unset");
   
   if(!(thing->flags & MF_NOSECTOR))
   {
      unlinkMobjFromSectors(thing);
      delTouchingThingChain(thing->touching_sectorlist);
      thing->touching_sectorlist = NULL;
   }

   if(!(thing->flags & MF_NOBLOCKMAP) && thing->blocklinks)
   {
      P_RemoveMobjBlockLinks(thing);
   }

   thing->groupid = R_NOGROUP;
}


//
// addMobjBlockLinks
//
// Iterates through the blockmap nodes a thing is touching, and adds links to them including 
// flags to indicate adjacency information.
void PortalClipEngine::addMobjBlockLinks(ClipContext *cc)
{
   int xl, xh, yl, yh, mask;
   int xc, yc, bx, by;
   Mobj *thing = cc->thing;

   for(size_t groupindex = 0; groupindex < cc->adjacent_groups.getLength(); ++groupindex)
   {
      linkoffset_t *link = P_GetLinkOffset(cc->adjacent_groups[0].group, cc->adjacent_groups[groupindex].group);
      
      xc = (thing->x - bmaporgx)>>MAPBLOCKSHIFT;
      yc = (thing->y - bmaporgy)>>MAPBLOCKSHIFT;

      CalculateBBoxForThing(cc, thing->x, thing->y, thing->radius, link);
      GetBlockmapBoundsFromBBox(cc, xl, yl, xh, yh);

      mask = 0;

      for(by = yl; by <= yh; ++by)
      {
         for(bx = xl; bx <= xh; ++bx)
         {
            if(!(bx < 0 || bx >= bmapwidth || by < 0 || by >= bmapheight))
            {
               P_AddMobjBlockLink
               (
                  thing, bx, by, 
                  mask | (by < yh ? SOUTH_ADJACENT : 0) | (bx < xh ? EAST_ADJACENT : 0) | (bx == xc && by == yc ? CENTER_ADJACENT : 0)
               );
            }
            mask |= WEST_ADJACENT;
         }
         mask |= NORTH_ADJACENT;
         mask &= ~WEST_ADJACENT;
      }
   }
}


//
// addMobjSectorLink
//
// Adds a node to the sector's thingnodelist which allows things to be attached to multiple
// sectors at the same time (portals, you know)
void PortalClipEngine::addMobjSectorLink(Mobj *mobj, sector_t *sector)
{
   msecnode_t *node = getSecnode();

   node->m_sector = sector;
   node->m_thing = mobj;

   node->m_tnext = mobj->sectorlinks;
   node->m_tprev = NULL;
   mobj->sectorlinks = node;

   if(sector->thingnodelist)
      sector->thingnodelist->m_sprev = node;

   node->m_snext = sector->thingnodelist;
   node->m_sprev = NULL;
   sector->thingnodelist = node;
}


//
// CheckTouchedSector
//
// Checks to see if a sector has been marked in the give context, and if not, the context's mobj
// is added as a touching mobj for the sector.
static void CheckTouchedSector(sector_t *sector, ClipContext *cc)
{
   int secnum = sector - sectors;

   if(!sector || cc->getMarkedSectors()->mark(secnum))
      return;

   msecnode_t *node = ClipEngine::getSecnode();
   Mobj *thing = cc->thing;

   node->m_sector = sector;
   node->m_thing  = thing;
   node->m_tprev  = NULL;
   node->m_tnext  = thing->touching_sectorlist;
   
   if(thing->touching_sectorlist)
      thing->touching_sectorlist->m_tprev = node;

   thing->touching_sectorlist = node;

   node->m_sprev = NULL;
   node->m_snext = sector->touching_thinglist;
   if(sector->touching_thinglist)
      sector->touching_thinglist->m_sprev = node;

   sector->touching_thinglist = node;
}


static bool PIT_FindSectors(line_t *line, MapContext *context)
{
   ClipContext *cc = context->clipContext();
   if(!LineIntersectsBBox(cc, line))
      return true;

   CheckTouchedSector(line->frontsector, cc);
   CheckTouchedSector(line->backsector, cc);
   return true;
}


//
// gatherSectorLinks
//
// Generates linking nodes for each sector a thing is inside of, and nodes for sectors a thing is touching.
// This function assumes that the cc->adjacent_groups collection is current for the mobj.
void PortalClipEngine::gatherSectorLinks(Mobj *thing, ClipContext *cc)
{
   int xl, xh, yl, yh;
   int bx, by;

   cc->getMarkedSectors()->clearMarks();
   CheckTouchedSector(thing->subsector->sector, cc);

   linkMobjToSector(thing, thing->subsector->sector);

   cc->getMarkedLines()->clearMarks();

   for(size_t groupindex = 0; groupindex < cc->adjacent_groups.getLength(); ++groupindex)
   {
      // link into subsector
      linkoffset_t *link = P_GetLinkOffset(thing->groupid, cc->adjacent_groups[groupindex].group);
      subsector_t *ss = R_PointInSubsector(thing->x + link->x, thing->y + link->y);
      addMobjSectorLink(thing, ss->sector);

      CalculateBBoxForThing(cc, cc->x, cc->y, cc->thing->radius, link);
      GetBlockmapBoundsFromBBox(cc, xl, yl, xh, yh);

      for(by = yl; by <= yh; ++by)
      {
         for(bx = xl; bx <= xh; ++bx)
         {
            if(bx < 0 || bx >= bmapwidth || by < 0 || by >= bmapheight)
               continue;

            blockLinesIterator(bx, by, PIT_FindSectors, cc);
         }
      }
	}
}


void PortalClipEngine::setThingPosition(Mobj *thing)
{
   auto *cc = getContext();
   setThingPosition(thing, cc, true);
   cc->done();
}


//
// setThingPosition
//
// Links the given Mobj into the map structures based on mobj flags. If findPortals is false,
// the function assumes that cc->adjacent_portals is current for the thing at the thing's position.
void PortalClipEngine::setThingPosition(Mobj *thing, ClipContext *cc, bool findPortals)
{
   // link into subsector
   subsector_t *ss = thing->subsector = R_PointInSubsector(thing->x, thing->y);
   thing->groupid = ss->sector->groupid;

   P_LogThingPosition(thing, " set ");
   
   if((thing->flags & (MF_NOSECTOR|MF_NOBLOCKMAP)) != (MF_NOSECTOR|MF_NOBLOCKMAP))
   {
      // Collect the portal groups
      cc->thing = thing;
      cc->x = thing->x;
      cc->y = thing->y;

      if(findPortals)
         findAdjacentPortals(cc);

      // Link into sector here
      if(!(thing->flags & MF_NOSECTOR))
         gatherSectorLinks(thing, cc);

      if(!(thing->flags & MF_NOBLOCKMAP))
         addMobjBlockLinks(cc);
   }
}
