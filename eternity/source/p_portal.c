// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2006 Stephen McGranahan
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
//      Linked portals
//      SoM created 02/13/06
//
//-----------------------------------------------------------------------------


#include "c_io.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_bsp.h"
#include "r_things.h"
#include "p_setup.h"
#include "p_map.h"
#include "p_inter.h"
#include "m_bbox.h"
#include "p_maputl.h"
#include "p_map3d.h"

#ifdef R_LINKEDPORTALS

// SoM: Linked portals
// This list is allocated PU_LEVEL and is nullified in P_InitPortals. When the 
// table is built it is allocated as a linear buffer of groupcount * groupcount
// entries. The entries are arranged much like pixels in a screen buffer where 
// the offset is located in linktable[startgroup * groupcount + targetgroup]
linkoffset_t **linktable = NULL;

// Ok, this is kinda tricky. The group list is a double pointer. The list is
// allocated static and is a expandable list. Each entry in the list is another
// list of ints which contain the sector numbers in that group. This sub-list is
// allocated PU_LEVEL because it is level specific.
static sector_t ***grouplist = NULL;
static int  groupcount = 0, grouplimit = 0;

// This flag is a big deal. Heh, if this is true a whole lot of code will 
// operate differently. This flag is cleared on P_PortalInit and is ONLY to be
// set true by P_BuildLinkTable.
boolean useportalgroups = false;

void P_InitPortals(void)
{
   int i;
   linktable = NULL;

   groupcount = 0;
   for(i = 0; i < grouplimit; ++i)
      grouplist[i] = NULL;

   useportalgroups = false;
}

//
// R_SetSectorGroupID
//
// SoM: yes this is hackish, I admit :(
// This sets all mobjs inside the sector to have the sector id
//
void R_SetSectorGroupID(sector_t *sector, int groupid)
{
   mobj_t *mo;

   sector->groupid = groupid;

   // SoM: soundorg ids need to be set too
   sector->soundorg.groupid  = groupid;
   sector->csoundorg.groupid = groupid;

   for(mo = sector->thinglist; mo; mo = mo->snext)
      mo->groupid = groupid;
}


//
// P_CreatePortalGroup
//
// This function creates a portal group using the given sector as a starting
// point. The function will run through the sector's lines list, and add 
// attached sectors to the group's sector list. As each sector is added the 
// currently forming group's id is assigned to that sector. This will continue
// until every attached sector has been added to the list, thus defining a 
// closed subspace of the map.
//
int P_CreatePortalGroup(sector_t *from)
{
   static sector_t **list = NULL;
   static int listmax = 0;

   int count = 0, i, sec, p;
   sector_t *sec2;
   line_t *line;

   // Sector already has a group
   if(from->groupid != R_NOGROUP)
      return from->groupid;

   if(listmax <= numsectors)
   {
      listmax = numsectors + 1;
      list = (sector_t **)realloc(list, sizeof(sector_t *) * listmax);
   }

   if(groupcount == grouplimit)
   {
      grouplimit = grouplimit ? (grouplimit << 1) : 8;
      grouplist = 
         (sector_t ***)realloc(grouplist, sizeof(sector_t *) * grouplimit);
   }

   list[count++] = from;
   R_SetSectorGroupID(from, groupcount);
   for(sec = 0; sec < count; ++sec)
   {
      from = list[sec];

      for(i = 0; i < from->linecount; ++i)
      {
         // add any sectors to the list which aren't already there.
         line = from->lines[i];
         if((sec2 = line->frontsector))
         {
            for(p = 0; p < count; ++p)
            {
               if(sec2 == list[p])
                  break;
            }
            // if we didn't find the sector in the list, add it
            if(p == count)
            {
               list[count++] = sec2;
               R_SetSectorGroupID(sec2, groupcount);
            }
         }

         if((sec2 = line->backsector))
         {
            for(p = 0; p < count; ++p)
            {
               if(sec2 == list[p])
                  break;
            }
            // if we didn't find the sector in the list, add it
            if(p == count)
            {
               list[count++] = sec2;
               R_SetSectorGroupID(sec2, groupcount);
            }
         }
      }
   }
   list[count++] = NULL;

   grouplist[groupcount] = 
      (sector_t **)Z_Malloc(count * sizeof(sector_t *), PU_LEVEL, 0);
   
   memcpy(grouplist[groupcount], list, count * sizeof(sector_t *));

   return groupcount++;
}

//
// P_GetLinkOffset
//
linkoffset_t *P_GetLinkOffset(int startgroup, int targetgroup)
{
   if(!linktable)
   {
#ifdef RANGECHECK
      doom_printf("P_GetLinkOffset called with no linktable allocated.\n");
#endif
      return NULL;
   }

   if(startgroup < 0 || startgroup >= groupcount)
   {
#ifdef RANGECHECK
      doom_printf("P_GetLinkOffset called with start groupid out of bounds.\n");
#endif
      return NULL;
   }

   if(targetgroup < 0 || targetgroup >= groupcount)
   {
#ifdef RANGECHECK
      doom_printf("P_GetLinkOffset called with target groupid out of bounds.\n");
#endif
      return NULL;
   }

   return linktable[startgroup * groupcount + targetgroup];
}

//
// P_AddLinkOffset
//

// Returns 0 if the link offset was added successfully, 1 if the start group is out of bounds,
// and 2 of the target group is out of bounds.
int P_AddLinkOffset(int startgroup, int targetgroup, 
                    fixed_t x, fixed_t y, fixed_t z)
{
   linkoffset_t *link;

#ifdef RANGECHECK
   if(!linktable)
      I_Error("P_AddLinkOffset: no linktable allocated.\n");
#endif

   if(startgroup < 0 || startgroup >= groupcount)
      return 1; //I_Error("P_AddLinkOffset: start groupid %d out of bounds.\n", startgroup);

   if(targetgroup < 0 || targetgroup >= groupcount)
      return 2; //I_Error("P_AddLinkOffset: target groupid %d out of bounds.\n", targetgroup);

   if(startgroup == targetgroup)
      return 0;

   link = (linkoffset_t *)Z_Malloc(sizeof(linkoffset_t), PU_LEVEL, 0);
   linktable[startgroup * groupcount + targetgroup] = link;
   
   link->x = x;
   link->y = y;
   link->z = z;

   return 0;
}

//
// P_CheckLinkedPortal
//
static boolean P_CheckLinkedPortal(rportal_t *portal, sector_t *sec)
{
   int i = sec - sectors;
   linkoffset_t *link;

   if(!portal || !sec)
      return true;
   if(portal->type != R_LINKED)
      return true;

   if(portal->data.camera.groupid == sec->groupid)
   {
      C_Printf(FC_ERROR "P_BuildLinkTable: sector %i portal references the "
               "portal group to which it belongs.\n"
               "Linked portals are disabled.\a\n", i);
      return false;
   }

   if(portal->data.camera.groupid < 0 || 
      portal->data.camera.groupid >= groupcount)
   {
      C_Printf(FC_ERROR "P_BuildLinkTable: sector %i portal has a groupid out "
               "of range.\nLinked portals are disabled.\a\n", i);
      return false;
   }

   if(sec->groupid < 0 || 
      sec->groupid >= groupcount)
   {
      C_Printf(FC_ERROR "P_BuildLinkTable: sector %i does not belong to a "
               "portal group.\nLinked portals are disabled.\a\n", i);
      return false;
   }


   // We've found a linked portal so add the entry to the table
   if(!(link = P_GetLinkOffset(sec->groupid, portal->data.camera.groupid)))
   {
      int ret = P_AddLinkOffset(sec->groupid, portal->data.camera.groupid, 
                                portal->data.camera.deltax, portal->data.camera.deltay, 
                                portal->data.camera.deltaz);
      if(ret)
         return false;
   }
   else
   {
      // Check for consistency
      if(link->x != portal->data.camera.deltax ||
         link->y != portal->data.camera.deltay ||
         link->z != portal->data.camera.deltaz)
      {
         C_Printf(FC_ERROR "P_BuildLinkTable: sector %i in group %i contains "
                  "inconsistent reference to group %i.\n"
                  "Linked portals are disabled.\a\n", 
                  i, sec->groupid, portal->data.camera.groupid);
         return false;
      }
   }

   return true;
}

//
// P_GatherLinks
//
static void P_GatherLinks(int group, fixed_t dx, fixed_t dy, fixed_t dz, 
                          int from)
{
   int i, p;
   linkoffset_t *link, **linklist, **fromlist;

   // The main group has an indrect link with every group that links to a group
   // that has a direct link to it, or any group that has a link to a group the 
   // main group has an indirect link to. huh.

   // First step: run through the list of groups this group has direct links to
   // from there, run the function again with each direct link.
   if(from == R_NOGROUP)
   {
      linklist = linktable + group * groupcount;

      for(i = 0; i < groupcount; ++i)
      {
         if(i == group)
            continue;

         if((link = linklist[i]))
            P_GatherLinks(group, link->x, link->y, link->z, i);
      }

      return;
   }

   linklist = linktable + group * groupcount;
   fromlist = linktable + from * groupcount;

   // Second step run through the linked group's link list. Ignore any groups 
   // the main group is already linked to. Add the deltas and add the entries,
   // then call this function for groups the linked group links to.
   for(p = 0; p < groupcount; ++p)
   {
      if(p == group || p == from)
         continue;

      if(!(link = fromlist[p]) || linklist[p])
         continue;

      P_AddLinkOffset(group, p, dx + link->x, dy + link->y, dz + link->z);
      P_GatherLinks(group, dx + link->x, dy + link->y, dz + link->z, p);
   }
}

//
// P_BuildLinkTable
//
void P_BuildLinkTable(void)
{
   int i, p;
   sector_t *sec;
   linkoffset_t *link, *backlink;

   if(!groupcount)
      return;

   // SoM: the last line of the table (starting at groupcount * groupcount) is
   // used as a temporary list for gathering links.
   linktable = 
      (linkoffset_t **)Z_Calloc(1, sizeof(linkoffset_t *)*groupcount*groupcount,
                                PU_LEVEL, 0);

   // Run through the sectors check for invalid portal references.
   for(i = 0; i < numsectors; i++)
   {
      sec = sectors + i;
      
      // Make sure there are no groups that reference themselves or invalid group
      // id numbers.
      if(sec->groupid < R_NOGROUP || sec->groupid >= groupcount)
      {
         C_Printf(FC_ERROR "P_BuildLinkTable: sector %i has a groupid out of "
                  "range.\nLinked portals are disabled.\a\n", i);
         return;
      }

      if(!P_CheckLinkedPortal(sec->c_portal, sec))
         return;

      if(!P_CheckLinkedPortal(sec->f_portal, sec))
         return;

      for(p = 0; p < sec->linecount; ++p)
      {
         if(!P_CheckLinkedPortal(sec->lines[p]->portal, sec))
            return;
      }
      // Sector checks out...
   }

   // Now the fun begins! Checking the actual groups for correct backlinks.
   // this needs to be done before the indirect link information is gathered to
   // make sure every link is two-way.
   for(i = 0; i < groupcount; ++i)
   {
      for(p = 0; p < groupcount; ++p)
      {
         if(p == i)
            continue;
         link = P_GetLinkOffset(i, p);
         backlink = P_GetLinkOffset(p, i);

         // check the links
         if(link && backlink)
         {
            if(backlink->x != -link->x ||
               backlink->y != -link->y ||
               backlink->z != -link->z)
            {
               C_Printf(FC_ERROR "Portal groups %i and %i link and backlink do "
                        "not agree\nLinked portals are disabled\a\n", i, p);
               return;
            }
         }
         else if(link || backlink)
         {
            C_Printf(FC_ERROR "Portal group %i references group %i without a "
                     "backlink.\nLinked portals are disabled\a\n", i, p);
            return;
         }
      }
   }

   // That first loop has to complete before this can be run!
   for(i = 0; i < groupcount; ++i)
      P_GatherLinks(i, 0, 0, 0, R_NOGROUP);

   // SoM: one last step. Find all map architecture with a group id of -1 and assign it to
   // group 0
   for(i = 0; i < numsectors; i++)
   {
      if(sectors[i].groupid == R_NOGROUP)
         sectors[i].groupid = 0;
   }

   // Everything checks out... let's run the portals
   useportalgroups = true;
}



//
// P_LinkRejectTable
//
// Currently just clears each group for every other group.
//
void P_LinkRejectTable(void)
{
   int i, s, p, q;
   sector_t **list, **list2;

   for(i = 0; i < groupcount; i++)
   {
      list = grouplist[i];
      for(s = 0; list[s]; s++)
      {
         int sectorindex1 = list[s] - sectors;

         for(p = 0; p < groupcount; p++)
         {
            if(i == p)
               continue;
            
            list2 = grouplist[p];
            for(q = 0; list2[q]; q++)
            {
               int sectorindex2 = list2[q] - sectors;
               int pnum = (sectorindex1 * numsectors) + sectorindex2;

               rejectmatrix[pnum>>3] &= ~(1 << (pnum&7));
            } // q
         } // p
      } // s
   } // i
}



// -----------------------------------------
// Begin portal teleportation

static boolean PIT_StompThing3D(mobj_t *thing)
{
   fixed_t blockdist;
   
   if(!(thing->flags & MF_SHOOTABLE)) // Can't shoot it? Can't stomp it!
   {
      return true;
   }
   
   blockdist = thing->radius + tm->thing->radius;
   
   if(D_abs(thing->x - tm->x) >= blockdist ||
      D_abs(thing->y - tm->y) >= blockdist)
      return true; // didn't hit it
   
   // don't clip against self
   if(thing == tm->thing)
      return true;

   if(tm->thing->z >= thing->z + thing->height ||
      thing->z >= tm->thing->z + tm->thing->height)
      return true;

   P_DamageMobj(thing, tm->thing, tm->thing, 10000, MOD_TELEFRAG); // Stomp!
   
   return true;
}



boolean P_PortalTeleportMove(mobj_t *thing, fixed_t x, fixed_t y)
{
   int xl, xh, yl, yh, bx, by;
   subsector_t *newsubsec;

   // kill anything occupying the position
   
   tm->thing = thing;
   tm->flags = thing->flags;
   
   tm->x = x;
   tm->y = y;
   
   tm->bbox[BOXTOP] = y + tm->thing->radius;
   tm->bbox[BOXBOTTOM] = y - tm->thing->radius;
   tm->bbox[BOXRIGHT] = x + tm->thing->radius;
   tm->bbox[BOXLEFT] = x - tm->thing->radius;
   
   newsubsec = R_PointInSubsector (x,y);
   tm->ceilingline = NULL;
   
   // The base floor/ceiling is from the subsector
   // that contains the point.
   // Any contacted lines the step closer together
   // will adjust them.
   
#ifdef R_LINKEDPORTALS
   if(demo_version >= 333 && R_LinkedFloorActive(newsubsec->sector) &&
      !(tm->thing->flags & MF_NOCLIP))
   {
      if(!P_CheckPortalHeight(tm->thing, x, y, newsubsec->sector, prtl_floor))
         return false;
   }
   else
#endif
      tm->floorz = newsubsec->sector->floorheight;

#ifdef R_LINKEDPORTALS
   if(demo_version >= 333 && R_LinkedCeilingActive(newsubsec->sector) &&
      !(tm->thing->flags & MF_NOCLIP))
   {
      if(!P_CheckPortalHeight(tm->thing, x, y, newsubsec->sector, prtl_ceiling))
         return false;
   }
   else
#endif
      tm->ceilingz = newsubsec->sector->ceilingheight;

   tm->secfloorz = tm->stepupfloorz = tm->passfloorz = tm->dropoffz = tm->floorz;
   tm->secceilz = tm->passceilz = tm->ceilingz;

   // haleyjd
   tm->floorpic = newsubsec->sector->floorpic;
   
   // SoM 09/07/02: 3dsides monster fix
   tm->touch3dside = 0;
   
   validcount++;
   tm->numspechit = 0;
   
   // stomp on any things contacted
   
   xl = (tm->bbox[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
   xh = (tm->bbox[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
   yl = (tm->bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
   yh = (tm->bbox[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;

   for(bx=xl ; bx<=xh ; bx++)
   {
      for(by=yl ; by<=yh ; by++)
      {
         if(!P_BlockThingsIterator(bx,by,PIT_StompThing3D))
            return false;
      }
   }


   // the move is ok,
   // so unlink from the old position & link into the new position
   
   P_UnsetThingPosition(thing);
   
   thing->floorz = tm->floorz;
   thing->ceilingz = tm->ceilingz;
   thing->dropoffz = tm->dropoffz;        // killough 11/98

   thing->passfloorz = tm->passfloorz;
   thing->passceilz = tm->passceilz;
   thing->secfloorz = tm->secfloorz;
   thing->secceilz = tm->secceilz;
   
   thing->x = x;
   thing->y = y;
   
   P_SetThingPosition(thing);
   
   return true;
}





//
// EV_PortalTeleport
//
boolean EV_PortalTeleport(mobj_t *mo, linkoffset_t *link)
{
   fixed_t moz = mo->z;
   fixed_t momx = mo->momx;
   fixed_t momy = mo->momy;
   fixed_t momz = mo->momz;
   fixed_t vh = mo->player ? mo->player->viewheight : 0;

   if(!mo || !link)
      return 0;
   if(!P_PortalTeleportMove(mo, mo->x - link->x, mo->y - link->y))
      return 0;

   mo->z = moz - link->z;

   mo->momx = momx;
   mo->momy = momy;
   mo->momz = momz;

   // Adjust a player's view, in case there has been a height change
   if(mo->player)
   {
      if(mo->player == players + displayplayer)
          P_ResetChasecam();

      mo->player->viewz = mo->z + vh;
   }

   P_AdjustFloorClip(mo);
   
   return 1;
}



// ----------------------------------------------------------------------------
// SoM: Begin dummy mobj code


#if 0
void P_DummyMobjThinker(mobj_t *mobj)
{
   
}




void P_CreateDummy(mobj_t *owner)
{
   if(owner->portaldummy)
      return;

}
#endif

#endif

// EOF

