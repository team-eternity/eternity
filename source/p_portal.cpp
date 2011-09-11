// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
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

#include "z_zone.h"
#include "i_system.h"
#include "c_io.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_bsp.h"
#include "r_things.h"
#include "p_setup.h"
#include "p_map.h"
#include "p_portal.h"

// [CG] Added.
#include "cl_spec.h"

// SoM: Linked portals
// This list is allocated PU_LEVEL and is nullified in P_InitPortals. When the 
// table is built it is allocated as a linear buffer of groupcount * groupcount
// entries. The entries are arranged much like pixels in a screen buffer where 
// the offset is located in linktable[startgroup * groupcount + targetgroup]
linkoffset_t **linktable = NULL;

// The group list is allocated PU_STATIC because it isn't level specific, however,
// each element is allocated PU_LEVEL. P_InitPortals clears the list and sets the 
// count back to 0
typedef struct 
{
   // List of sectors contained in the group
   sector_t **seclist;
   
   // Size of the list
   int listsize;
} pgroup_t;

static pgroup_t **groups = NULL;
static int      groupcount = 0;
static int      grouplimit = 0;

// This flag is a big deal. Heh, if this is true a whole lot of code will 
// operate differently. This flag is cleared on P_PortalInit and is ONLY to be
// set true by P_BuildLinkTable.
boolean useportalgroups = false;

//
// P_InitPortals
//
// Called before map processing. Simply inits some module variables
void P_InitPortals(void)
{
   int i;
   linktable = NULL;

   groupcount = 0;
   for(i = 0; i < grouplimit; ++i)
      groups[i] = NULL;

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
   int i;

   sector->groupid = groupid;

   // SoM: soundorg ids need to be set too
   sector->soundorg.groupid  = groupid;
   sector->csoundorg.groupid = groupid;

   for(mo = sector->thinglist; mo; mo = mo->snext)
      mo->groupid = groupid;

   // haleyjd 04/19/09: propagate to line sound origins
   for(i = 0; i < sector->linecount; ++i)
      sector->lines[i]->soundorg.groupid = groupid;
}

//
// P_CreatePortalGroup
//
// This function creates a new portal group using the given sector as the 
// starting point for the group. P_GatherSectors is then called to gather 
// sectors into the group, and the newly created id is returned.
//
int P_CreatePortalGroup(sector_t *from)
{
   int       groupid = groupcount;
   pgroup_t  *group;
   
   if(from->groupid != R_NOGROUP)
      return from->groupid;
      
   if(groupcount == grouplimit)
   {
      grouplimit = grouplimit ? (grouplimit << 1) : 8;
      groups = 
         (pgroup_t **)(realloc(groups, sizeof(pgroup_t **) * grouplimit));
   }
   groupcount++;   
   
   
   group = groups[groupid] = (pgroup_t *)(Z_Malloc(sizeof(pgroup_t), PU_LEVEL, 0));
      
   group->seclist = NULL;
   group->listsize = 0;
  
   P_GatherSectors(from, groupid);
   return groupid;
}

//
// P_GatherSectors
//
// The function will run through the sector's lines list, and add 
// attached sectors to the group's sector list. As each sector is added the 
// currently forming group's id is assigned to that sector. This will continue
// until every attached sector has been added to the list, thus defining a 
// closed subspace of the map.
//
void P_GatherSectors(sector_t *from, int groupid)
{
   static sector_t   **list = NULL;
   static int        listmax = 0;

   sector_t  *sec2;
   pgroup_t  *group;
   line_t    *line;
   int       count = 0;
   int       i, sec, p;
   
   if(groupid < 0 || groupid >= groupcount)
   {
      // I have no idea if/when this would ever happen, but at any rate, it
      // would translate to EE itself doing something wrong.
      I_Error("P_GatherSectors: groupid invalid!");
   }

   group = groups[groupid];
   
   // Sector already has a group
   if(from->groupid != R_NOGROUP)
      return;

   // SoM: Just check for a null list here.
   if(!list || listmax <= numsectors)
   {
      listmax = numsectors + 1;
      list = (sector_t **)(realloc(list, sizeof(sector_t *) * listmax));
   }

   R_SetSectorGroupID(from, groupid);
   list[count++] = from;
   
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
               R_SetSectorGroupID(sec2, groupid);
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
               R_SetSectorGroupID(sec2, groupid);
            }
         }
      }
   }

   // Ok, so expand the group list
   group->seclist = (sector_t **)(realloc(group->seclist, sizeof(sector_t *) * (group->listsize + count)));
   
   memcpy(group->seclist + group->listsize, list, count * sizeof(sector_t *));
   group->listsize += count;
}

//
// P_GetLinkOffset
//
// This function returns a linkoffset_t object which contains the map-space
// offset to get from the startgroup to the targetgroup.
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
// Returns 0 if the link offset was added successfully, 1 if the start group is
// out of bounds, and 2 of the target group is out of bounds.
//
int P_AddLinkOffset(int startgroup, int targetgroup, 
                    fixed_t x, fixed_t y, fixed_t z)
{
   linkoffset_t *link;

#ifdef RANGECHECK
   if(!linktable)
      I_Error("P_AddLinkOffset: no linktable allocated.\n");
#endif

   if(startgroup < 0 || startgroup >= groupcount)
      return 1; 
      //I_Error("P_AddLinkOffset: start groupid %d out of bounds.\n", startgroup);

   if(targetgroup < 0 || targetgroup >= groupcount)
      return 2; 
      //I_Error("P_AddLinkOffset: target groupid %d out of bounds.\n", targetgroup);

   if(startgroup == targetgroup)
      return 0;

   link = (linkoffset_t *)(Z_Malloc(sizeof(linkoffset_t), PU_LEVEL, 0));
   linktable[startgroup * groupcount + targetgroup] = link;
   
   link->x = x;
   link->y = y;
   link->z = z;

   return 0;
}

//
// P_CheckLinkedPortal
//
// This function performs various consistency and validation checks.
static boolean P_CheckLinkedPortal(portal_t *portal, sector_t *sec)
{
   int i = sec - sectors;
   linkoffset_t *link;

   if(!portal || !sec)
      return true;
   if(portal->type != R_LINKED)
      return true;

   if(portal->data.link.toid == sec->groupid)
   {
      C_Printf(FC_ERROR "P_BuildLinkTable: sector %i portal references the "
               "portal group to which it belongs.\n"
               "Linked portals are disabled.\a\n", i);
      return false;
   }

   if(portal->data.link.fromid < 0 || 
      portal->data.link.fromid >= groupcount ||
      portal->data.link.toid < 0 || 
      portal->data.link.toid >= groupcount)
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
   
   if(sec->groupid != portal->data.link.fromid)
   {
      C_Printf(FC_ERROR "P_BuildLinkTable: sector %i does not belong to the "
               "the portal's fromid\nLinked portals are disabled.\a\n", i);
      return false;
   }


   // We've found a linked portal so add the entry to the table
   if(!(link = P_GetLinkOffset(sec->groupid, portal->data.link.toid)))
   {
      int ret = P_AddLinkOffset(sec->groupid, portal->data.link.toid,
                                portal->data.link.deltax, 
                                portal->data.link.deltay, 
                                portal->data.link.deltaz);
      if(ret)
         return false;
   }
   else
   {
      // Check for consistency
      if(link->x != portal->data.link.deltax ||
         link->y != portal->data.link.deltay ||
         link->z != portal->data.link.deltaz)
      {
         C_Printf(FC_ERROR "P_BuildLinkTable: sector %i in group %i contains "
                  "inconsistent reference to group %i.\n"
                  "Linked portals are disabled.\a\n", 
                  i, sec->groupid, portal->data.link.toid);
         return false;
      }
   }

   return true;
}

//
// P_GatherLinks
//
// This function generates linkoffset_t objects for every group to every other 
// group, that is, if group A has a link to B, and B has a link to C, a link
// can be found to go from A to C.
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

static void P_GlobalPortalStateCheck()
{
   sector_t *sec;
   line_t   *line;
   int      i;
   
   for(i = 0; i < numsectors; i++)
   {
      sec = sectors + i;
      
      if(sec->c_portal)
         P_CheckCPortalState(sec);
      if(sec->f_portal)
         P_CheckFPortalState(sec);
   }
   
   for(i = 0; i < numlines; i++)
   {
      line = lines + i;
      
      if(line->portal)
         P_CheckLPortalState(line);
   }
}

//
// P_BuildLinkTable
//
boolean P_BuildLinkTable(void)
{
   int i, p;
   sector_t *sec;
   linkoffset_t *link, *backlink;

   if(!groupcount)
      return true;

   // SoM: the last line of the table (starting at groupcount * groupcount) is
   // used as a temporary list for gathering links.
   linktable = 
      (linkoffset_t **)(Z_Calloc(1, sizeof(linkoffset_t *)*groupcount*groupcount,
                                PU_LEVEL, 0));

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
         return false;
      }

      if(!P_CheckLinkedPortal(sec->c_portal, sec))
         return false;

      if(!P_CheckLinkedPortal(sec->f_portal, sec))
         return false;

      for(p = 0; p < sec->linecount; ++p)
      {
         if(!P_CheckLinkedPortal(sec->lines[p]->portal, sec))
           return false;
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
               return false;
            }
         }
         else if(link || backlink)
         {
            C_Printf(FC_ERROR "Portal group %i references group %i without a "
                     "backlink.\nLinked portals are disabled\a\n", i, p);
            return false;
         }
      }
   }

   // That first loop has to complete before this can be run!
   for(i = 0; i < groupcount; ++i)
      P_GatherLinks(i, 0, 0, 0, R_NOGROUP);

   // SoM: one last step. Find all map architecture with a group id of -1 and 
   // assign it to group 0
   for(i = 0; i < numsectors; i++)
   {
      if(sectors[i].groupid == R_NOGROUP)
         R_SetSectorGroupID(sectors + i, 0);
   }

   // Everything checks out... let's run the portals
   useportalgroups = true;
   P_GlobalPortalStateCheck();
   
   return true;
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
      list = groups[i]->seclist;
      for(s = 0; list[s]; s++)
      {
         int sectorindex1 = list[s] - sectors;

         for(p = 0; p < groupcount; p++)
         {
            if(i == p)
               continue;
            
            list2 = groups[p]->seclist;
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

//=============================================================================
//
// SoM: Utility functions
//

//
// P_GetPortalState
//
// Returns the combined state flags for the given portal based on various
// behavior flags
static int P_GetPortalState(portal_t *portal, int sflags, boolean obscured)
{
   boolean   active;
   int       ret = sflags & (PF_FLAGMASK | PS_OVERLAYFLAGS | PO_OPACITYMASK);
   
   if(!portal)
      return 0;
      
   active = !obscured && !(portal->flags & PF_DISABLED) && !(sflags & PF_DISABLED);
   
   if(active && !(portal->flags & PF_NORENDER) && !(sflags & PF_NORENDER))
      ret |= PS_VISIBLE;
      
   // Next two flags are for linked portals only
   active = active && portal->type == R_LINKED && useportalgroups == true;
      
   if(active && !(portal->flags & PF_NOPASS) && !(sflags & PF_NOPASS))
      ret |= PS_PASSABLE;
   
   if(active && !(portal->flags & PF_BLOCKSOUND) && !(sflags & PF_BLOCKSOUND))
      ret |= PS_PASSSOUND;
      
   return ret;
}

void P_CheckCPortalState(sector_t *sec)
{
   boolean     obscured;
   
   if(!sec->c_portal)
   {
      sec->c_pflags = 0;
      return;
   }
   
   obscured = (sec->c_portal->type == R_LINKED && 
               sec->ceilingheight < sec->c_portal->data.link.planez);
               
   sec->c_pflags = P_GetPortalState(sec->c_portal, sec->c_pflags, obscured);
}

void P_CheckFPortalState(sector_t *sec)
{
   boolean     obscured;
   
   if(!sec->f_portal)
   {
      sec->f_pflags = 0;
      return;
   }

   obscured = (sec->f_portal->type == R_LINKED && 
               sec->floorheight > sec->f_portal->data.link.planez);
               
   sec->f_pflags = P_GetPortalState(sec->f_portal, sec->f_pflags, obscured);
}

void P_CheckLPortalState(line_t *line)
{
   if(!line->portal)
   {
      line->pflags = 0;
      return;
   }
   
   line->pflags = P_GetPortalState(line->portal, line->pflags, false);
}

//
// P_SetFloorHeight
//
// This function will set the floor height, and update
// the float version of the floor height as well.
//
void P_SetFloorHeight(sector_t *sec, fixed_t h)
{
   // [CG] Clients don't move sectors during normal thinking, either they
   //      deliberately run the sector thinkers (prediction) or they set sector
   //      positions to whatever was received from the server.
   if(serverside || cl_predicting_sectors || cl_setting_sector_positions)
   {
      // TODO: Support for diabling a linked portal on a surface
      sec->floorheight = h;
      sec->floorheightf = M_FixedToFloat(sec->floorheight);
   }

   P_CheckFPortalState(sec);
}

//
// P_SetCeilingHeight
//
// This function will set the ceiling height, and update
// the float version of the ceiling height as well.
//
void P_SetCeilingHeight(sector_t *sec, fixed_t h)
{
   // [CG] Clients don't move sectors during normal thinking, either they
   //      deliberately run the sector thinkers (prediction) or they set sector
   //      positions to whatever was received from the server.
   if(serverside || cl_predicting_sectors || cl_setting_sector_positions)
   {
      // TODO: Support for diabling a linked portal on a surface
      sec->ceilingheight = h;
      sec->ceilingheightf = M_FixedToFloat(sec->ceilingheight);
   }

   P_CheckCPortalState(sec);
}

void P_SetPortalBehavior(portal_t *portal, int newbehavior)
{
   int   i;
   
   portal->flags = newbehavior & PF_FLAGMASK;
   for(i = 0; i < numsectors; i++)
   {
      sector_t *sec = sectors + i;
      
      if(sec->c_portal == portal)
         P_CheckCPortalState(sec);
      if(sec->f_portal == portal)
         P_CheckFPortalState(sec);
   }
   
   
   for(i = 0; i < numlines; i++)
   {
      if(lines[i].portal == portal)
         P_CheckLPortalState(lines + i);
   }
}

void P_SetFPortalBehavior(sector_t *sec, int newbehavior)
{
   if(!sec->f_portal)
      return;
      
   sec->f_pflags = newbehavior;
   P_CheckFPortalState(sec);
}

void P_SetCPortalBehavior(sector_t *sec, int newbehavior)
{
   if(!sec->c_portal)
      return;
      
   sec->c_pflags = newbehavior;
   P_CheckCPortalState(sec);
}

void P_SetLPortalBehavior(line_t *line, int newbehavior)
{
   if(!line->portal)
      return;
      
   line->pflags = newbehavior;
   P_CheckLPortalState(line);
}

//=============================================================================
//
// SoM: Begin dummy mobj code
//

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

// EOF

