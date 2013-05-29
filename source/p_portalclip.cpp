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
//  Movement, collision handling.
//  Shooting and aiming.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "c_io.h"
#include "d_gi.h"
#include "d_mod.h"
#include "doomstat.h"
#include "e_exdata.h"
#include "e_states.h"
#include "e_things.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "m_collection.h"
#include "m_random.h"
#include "p_inter.h"
#include "p_mobj.h"
#include "p_maputl.h"
#include "p_map3d.h"
#include "p_mapcontext.h"
#include "p_portalclip.h"
#include "p_partcl.h"
#include "p_portal.h"
#include "p_setup.h"
#include "p_skin.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_traceengine.h"
#include "p_user.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_portal.h"
#include "r_segs.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"
#include "v_misc.h"
#include "v_video.h"



// ----------------------------------------------------------------------------
// PortalClip
static int markcells;


ClipContext*  PortalClipEngine::getContext()
{
   if(unused == NULL) {
      ClipContext *ret = new ClipContext();
      ret->setEngine(this);
      ret->markedgroups = ecalloctag(int *, markcells, sizeof(*(ret->markedgroups)), PU_LEVEL, NULL);
      return ret;
   }
   
   ClipContext *res = unused;
   
   unused = unused->next;
   res->next = NULL;
   return res;
}


void PortalClipEngine::freeContext(ClipContext *cc)
{
   cc->next = unused;
   unused = cc;
}



static inline void HitPortalGroup(int groupid, ClipContext *cc)
{
   if(cc->markedgroups[groupid >> 5] & 1 << (groupid % 32))
      return;
      
   cc->markedgroups[groupid >> 5] |= 1 << (groupid % 32);
   cc->adjacent_groups.add(groupid);
}

//
// Populates the given list with all the portal groups (by index) the mobj touches
//
static bool PIT_FindAdjacentPortals(line_t *line, MapContext *context)
{
   sector_t *secs[2];
   
   ClipContext *cc = context->clipContext();
   if(cc->bbox[BOXRIGHT]  <= line->bbox[BOXLEFT]   || 
      cc->bbox[BOXLEFT]   >= line->bbox[BOXRIGHT]  || 
      cc->bbox[BOXTOP]    <= line->bbox[BOXBOTTOM] || 
      cc->bbox[BOXBOTTOM] >= line->bbox[BOXTOP] ||
      P_BoxOnLineSide(cc->bbox, line) != -1)
      return true;
   
   // Line portals
   if(line->pflags & PS_PASSABLE)
      HitPortalGroup(line->portal->data.link.toid, cc);
   
   // Floor/ceiling portals
   if(line->frontsector->c_pflags & PS_PASSABLE && cc->thing->z + cc->thing->height > line->frontsector->ceilingheight)
      HitPortalGroup(line->frontsector->c_portal->data.link.toid, cc);
   
   if(line->frontsector->f_pflags & PS_PASSABLE && cc->thing->z < line->frontsector->ceilingheight)
      HitPortalGroup(line->frontsector->f_portal->data.link.toid, cc);
      
   if(line->frontsector->c_pflags & PS_PASSABLE && cc->thing->z + cc->thing->height > line->frontsector->ceilingheight)
      HitPortalGroup(line->frontsector->c_portal->data.link.toid, cc);
   
   if(line->frontsector->f_pflags & PS_PASSABLE && cc->thing->z < line->frontsector->ceilingheight)
      HitPortalGroup(line->frontsector->f_portal->data.link.toid, cc);
}


static void findAdjacentPortals(ClipContext *cc)
{
   int top, left, right, bottom;
   int yh, yl, xh, xl;
   int bx, by;
   
   HitPortalGroup(cc->centergroup, cc);

   for(size_t i = 0; i < cc->adjacent_groups.getLength(); ++i)
   {
      linkoffset_t *link = P_GetLinkOffset(cc->adjacent_groups[0], cc->adjacent_groups[i]);
      
      cc->bbox[BOXLEFT]   = cc->thing->x + cc->thing->radius + link->x;
      cc->bbox[BOXRIGHT]  = cc->thing->x - cc->thing->radius + link->x;
      cc->bbox[BOXTOP]    = cc->thing->y + cc->thing->radius + link->y;
      cc->bbox[BOXBOTTOM] = cc->thing->y - cc->thing->radius + link->y;
   
      yh = (cc->bbox[BOXTOP]    - bmaporgy)>>MAPBLOCKSHIFT;
      yl = (cc->bbox[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
      xh = (cc->bbox[BOXLEFT]   - bmaporgx)>>MAPBLOCKSHIFT;
      xl = (cc->bbox[BOXRIGHT]  - bmaporgx)>>MAPBLOCKSHIFT;
      
      for(by = yl; by <= yh; by++)
      {
         for(bx = xl; bx <= xh; bx++)
         {
            if(bx < 0 || bx >= bmapwidth || by < 0 || by >= bmapheight)
               continue;
            
            P_BlockLinesIterator(bx, by, PIT_FindAdjacentPortals, cc);
         }
      }
   }
}


//
// tryMove
//
// Attempt to move to a new position,
// crossing special lines unless MF_TELEPORT is set.
//
// killough 3/15/98: allow dropoff as option
//
bool PortalClipEngine::tryMove(Mobj *thing, fixed_t x, fixed_t y, int dropoff, ClipContext *cc)
{
   return false;
}



void PortalClipEngine::unsetThingPosition(Mobj *thing)
{
   P_LogThingPosition(thing, "unset");
   
   if(!(thing->flags & MF_NOSECTOR))
   {
      Mobj **sprev = thing->sprev;
      Mobj  *snext = thing->snext;
      if((*sprev = snext))  // unlink from sector list
         snext->sprev = sprev;

      thing->old_sectorlist = thing->touching_sectorlist;
      thing->touching_sectorlist = NULL; // to be restored by P_SetThingPosition
   }

   if(!(thing->flags & MF_NOBLOCKMAP))
   {
      P_RemoveMobjBlockLinks(thing);
   }

   thing->groupid = R_NOGROUP;
}



void PortalClipEngine::setThingPosition(Mobj *thing)
{
   // link into subsector
   subsector_t *ss = thing->subsector = R_PointInSubsector(thing->x, thing->y);

   thing->groupid = ss->sector->groupid;

   P_LogThingPosition(thing, " set ");
   
   // Collect the portal groups
   ClipContext *cc = getContext();
   cc->thing = thing;
   cc->adjacent_groups.makeEmpty();
   cc->adjacent_groups.add(ss->sector->groupid);
   memset(cc->markedgroups, 0, sizeof(*(cc->markedgroups)) * markcells);

   // Link into sector here
   if(!(thing->flags & MF_NOSECTOR))
   {
      Mobj **link = &ss->sector->thinglist;
      Mobj *snext = *link;
      if((thing->snext = snext))
         snext->sprev = &thing->snext;
      thing->sprev = link;
      *link = thing;

      thing->touching_sectorlist = createSecNodeList(thing, thing->x, thing->y);
      //thing->old_sectorlist = NULL;
   }

   int xl, xh, yl, yh, mask;
   int xc, yc, bx, by;

   for(size_t groupindex = 0; groupindex < cc->adjacent_groups.getLength(); ++groupindex)
   {
      linkoffset_t *link = P_GetLinkOffset(cc->adjacent_groups[0], cc->adjacent_groups[groupindex]);
      
      cc->bbox[BOXLEFT]   = thing->x + thing->radius + link->x;
      cc->bbox[BOXRIGHT]  = thing->x - thing->radius + link->x;
      cc->bbox[BOXTOP]    = thing->y + thing->radius + link->y;
      cc->bbox[BOXBOTTOM] = thing->y - thing->radius + link->y;
   
      xc = (thing->x - bmaporgx)>>MAPBLOCKSHIFT;
      yc = (thing->y - bmaporgy)>>MAPBLOCKSHIFT;
      yh = (cc->bbox[BOXTOP]    - bmaporgy)>>MAPBLOCKSHIFT;
      yl = (cc->bbox[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
      xh = (cc->bbox[BOXLEFT]   - bmaporgx)>>MAPBLOCKSHIFT;
      xl = (cc->bbox[BOXRIGHT]  - bmaporgx)>>MAPBLOCKSHIFT;

      mask = 0;

      for(by = yl; by <= yh; ++by)
      {
         for(bx = xl; bx <= xh; ++bx)
         {
            if(bx < 0 || bx >= bmapwidth || by < 0 || by >= bmapheight)
               continue;

            // link into blockmap
            if(!(thing->flags & MF_NOBLOCKMAP))
            {
               P_AddMobjBlockLink
               (
                  thing, bx, by, 
                  mask | (by < yh ? SOUTH_ADJACENT : 0) | (bx < xh ? EAST_ADJACENT : 0) | (bx == xc && by == yc ? CENTER_ADJACENT : 0)
               );
            }

            // Get adjacent portal groups
            P_BlockLinesIterator(bx, by, PIT_FindAdjacentPortals, cc);

            mask |= WEST_ADJACENT;
         }
         mask |= NORTH_ADJACENT;
         mask &= ~WEST_ADJACENT;
      }
   }


}


void PortalClipEngine::mapLoaded()
{
   markcells = (P_PortalGroupCount() >> 5) + (P_PortalGroupCount() % 32 ? 1 : 0);
   
   ClipContext *cc = unused;   
   while(cc)
   {
      cc->markedgroups = ecalloctag(int *, markcells, sizeof(*(cc->markedgroups)), PU_LEVEL, NULL);
      cc = cc->next;
   }
}