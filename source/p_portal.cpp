// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 Stephen McGranahan et al.
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
//      Linked portals
//      SoM created 02/13/06
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "cam_sight.h"
#include "c_io.h"
#include "doomstat.h"
#include "m_bbox.h"        // ioanch 20160107
#include "m_collection.h"  // ioanch 20160106
#include "p_chase.h"
#include "p_map.h"
#include "p_maputl.h"   // ioanch
#include "p_portal.h"
#include "p_setup.h"
#include "p_user.h"
#include "r_bsp.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_pcheck.h"
#include "r_plane.h"
#include "r_portal.h"
#include "r_state.h"
#include "r_things.h"
#include "v_misc.h"

// SoM: Linked portals
// This list is allocated PU_LEVEL and is nullified in P_InitPortals. When the 
// table is built it is allocated as a linear buffer of groupcount * groupcount
// entries. The entries are arranged much like pixels in a screen buffer where 
// the offset is located in linktable[startgroup * groupcount + targetgroup]
linkoffset_t **linktable = NULL;


// This guy is a (0, 0, 0) link offset which is used to populate null links in the 
// linktable and is returned by P_GetLinkOffset for invalid inputs
linkoffset_t zerolink = {0, 0, 0};

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
bool useportalgroups = false;

// ioanch 20160109: needed for sprite projecting
bool gMapHasSectorPortals;
bool gMapHasLinePortals;   // ioanch 20160131: needed for P_UseLines
bool *gGroupVisit;

//
// P_PortalGroupCount
//
int P_PortalGroupCount()
{
   return useportalgroups ? groupcount : 1;
}

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
// FIXME: why is this named R_ instead p_portal?
//
void R_SetSectorGroupID(sector_t *sector, int groupid)
{
   sector->groupid = groupid;

   // SoM: soundorg ids need to be set too
   sector->soundorg.groupid  = groupid;
   sector->csoundorg.groupid = groupid;

   // haleyjd 12/25/13: must scan thinker list, not use sector thinglist.
   for(auto th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      Mobj *mo;
      if((mo = thinker_cast<Mobj *>(th)))
      {
         if(mo->subsector && mo->subsector->sector == sector)
            mo->groupid = groupid;
      }
   }

   // haleyjd 04/19/09: propagate to line sound origins
   for(int i = 0; i < sector->linecount; ++i)
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
      groups = erealloc(pgroup_t **, groups, sizeof(pgroup_t **) * grouplimit);
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
      list = erealloc(sector_t **, list, sizeof(sector_t *) * listmax);
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
   group->seclist = erealloc(sector_t **, group->seclist, 
                             sizeof(sector_t *) * (group->listsize + count));
   
   memcpy(group->seclist + group->listsize, list, count * sizeof(sector_t *));
   group->listsize += count;
}

//
// P_GetLinkOffset
//
// This function returns a linkoffset_t object which contains the map-space
// offset to get from the startgroup to the targetgroup. This will always return 
// a linkoffset_t object. In cases of invalid input or no link the offset will be
// (0, 0, 0)
//
linkoffset_t *P_GetLinkOffset(int startgroup, int targetgroup)
{
   if(!useportalgroups)
      return &zerolink;
      
   if(!linktable)
   {
      C_Printf(FC_ERROR "P_GetLinkOffset: called with no link table.\n");
      return &zerolink;
   }
   
   if(startgroup < 0 || startgroup >= groupcount)
   {
      C_Printf(FC_ERROR "P_GetLinkOffset: called with OoB start groupid %d.\n", startgroup);
      return &zerolink;
   }

   if(targetgroup < 0 || targetgroup >= groupcount)
   {
      C_Printf(FC_ERROR "P_GetLinkOffset: called with OoB target groupid %d.\n", targetgroup);
      return &zerolink;
   }

   auto link = linktable[startgroup * groupcount + targetgroup];
   return link ? link : &zerolink;
}

//
// P_GetLinkIfExists
//
// Returns a link offset to get from 'fromgroup' to 'togroup' if one exists. 
// Returns NULL otherwise
//
linkoffset_t *P_GetLinkIfExists(int fromgroup, int togroup)
{
   if(!useportalgroups)
      return NULL;

   if(!linktable)
   {
      C_Printf(FC_ERROR "P_GetLinkIfExists: called with no link table.\n");
      return NULL;
   }
   
   if(fromgroup < 0 || fromgroup >= groupcount)
   {
      C_Printf(FC_ERROR "P_GetLinkIfExists: called with OoB fromgroup %d.\n", fromgroup);
      return NULL;
   }

   if(togroup < 0 || togroup >= groupcount)
   {
      C_Printf(FC_ERROR "P_GetLinkIfExists: called with OoB togroup %d.\n", togroup);
      return NULL;
   }

   return linktable[fromgroup * groupcount + togroup];
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
//
static bool P_CheckLinkedPortal(portal_t *portal, sector_t *sec)
{
   int i = sec - sectors;

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

   auto link = linktable[sec->groupid * groupcount + portal->data.link.toid];

   // We've found a linked portal so add the entry to the table
   if(!link)
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
//
static void P_GatherLinks(int group, fixed_t dx, fixed_t dy, fixed_t dz, 
                          int via)
{
   int i, p;
   linkoffset_t *link, **linklist, **grouplinks;

   // The main group has an indrect link with every group that links to a group
   // that has a direct link to it, or any group that has a link to a group the 
   // main group has an indirect link to. huh.

   // First step: run through the list of groups this group has direct links to
   // from there, run the function again with each direct link.
   if(via == R_NOGROUP)
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

   linklist = linktable + via * groupcount;
   grouplinks = linktable + group * groupcount;

   // Second step run through the linked group's link list. Ignore any groups 
   // the main group is already linked to. Add the deltas and add the entries,
   // then call this function for groups the linked group links to.
   for(p = 0; p < groupcount; ++p)
   {
      if(p == group || p == via)
         continue;

      if(!(link = linklist[p]) || grouplinks[p])
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
// P_buildPortalMap
//
// haleyjd 05/17/13: Build a blockmap-like array which will instantly tell
// whether or not a given blockmap cell contains linked portals of different
// types and may therefore need to be subject to differing clipping behaviors,
// such as disabling certain short circuit checks.
//
// ioanch 20151228: also flag for floor and ceiling portals
//
static void P_buildPortalMap()
{
   PODCollection<int> curGroups; // ioanch 20160106: keep list of current groups
   size_t pcount = P_PortalGroupCount();
   gGroupVisit = ecalloctag(bool *, sizeof(bool), pcount, PU_LEVEL, nullptr);
   pcount *= sizeof(bool);

   gMapHasSectorPortals = false; // init with false
   gMapHasLinePortals = false;
   
   auto addPortal = [&curGroups](int groupid)
   {
      if(!gGroupVisit[groupid])
      {
         gGroupVisit[groupid] = true;
         curGroups.add(groupid);
      }
   };
   
   int writeOfs;
   for(int y = 0; y < bmapheight; y++)
   {
      for(int x = 0; x < bmapwidth; x++)
      {
         int offset;
         int *list;
         
         curGroups.makeEmpty();
         memset(gGroupVisit, 0, pcount);

         writeOfs = offset = y * bmapwidth + x;
         offset = *(blockmap + offset);
         list = blockmaplump + offset;

         // skip 0 delimiter
         ++list;

         // ioanch: also check sector blockmaps
         int *tmplist = list;
         if(*tmplist == -1)   // empty blockmap
         {
            // ioanch: in case of no lines in blockmap, determine the sector
            // by looking in the centre
            const sector_t *sector = R_PointInSubsector(
               ::bmaporgx + (x << MAPBLOCKSHIFT) + (MAPBLOCKSHIFT / 2),
               ::bmaporgy + (y << MAPBLOCKSHIFT) + (MAPBLOCKSHIFT / 2))->sector;

            if(sector->c_pflags & PS_PASSABLE)
            {
               portalmap[writeOfs] |= PMF_CEILING;
               curGroups.add(sector->c_portal->data.link.toid);
               gMapHasSectorPortals = true;
            }
            if(sector->f_pflags & PS_PASSABLE)
            {
               portalmap[writeOfs] |= PMF_FLOOR;
               curGroups.add(sector->f_portal->data.link.toid);
               gMapHasSectorPortals = true;
            }
         }
         else for(; *tmplist != -1; tmplist++)
         {
            const line_t &li = lines[*tmplist];
            if(li.pflags & PS_PASSABLE)
            {
               portalmap[writeOfs] |= PMF_LINE;
               addPortal(li.portal->data.link.toid);
               gMapHasLinePortals = true;
            }
            if(li.frontsector->c_pflags & PS_PASSABLE)
            {
               portalmap[writeOfs] |= PMF_CEILING;
               addPortal(li.frontsector->c_portal->data.link.toid);
               gMapHasSectorPortals = true;
            }
            if(li.backsector && li.backsector->c_pflags & PS_PASSABLE)
            {
               portalmap[writeOfs] |= PMF_CEILING;
               addPortal(li.backsector->c_portal->data.link.toid);
               gMapHasSectorPortals = true;
            }
            if(li.frontsector->f_pflags & PS_PASSABLE)
            {
               portalmap[writeOfs] |= PMF_FLOOR;
               addPortal(li.frontsector->f_portal->data.link.toid);
               gMapHasSectorPortals = true;
            }
            if(li.backsector && li.backsector->f_pflags & PS_PASSABLE)
            {
               portalmap[writeOfs] |= PMF_FLOOR;
               addPortal(li.backsector->f_portal->data.link.toid);
               gMapHasSectorPortals = true;
            }
         }
         if(gBlockGroups[writeOfs])
            I_Error("P_buildPortalMap: non-null gBlockGroups entry!");
         
         size_t curSize = curGroups.getLength();
         gBlockGroups[writeOfs] = emalloctag(int *, 
            (curSize + 1) * sizeof(int), PU_LEVEL, nullptr);
         gBlockGroups[writeOfs][0] = static_cast<int>(curSize);
         // just copy...
         if(curSize)
         {
            memcpy(gBlockGroups[writeOfs] + 1, &curGroups[0], 
               curSize * sizeof(int));
         }
      }
   }
}

//
// P_BuildLinkTable
//
bool P_BuildLinkTable()
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

      for(p = 0; p < sec->linecount; p++)
      {
         if(!P_CheckLinkedPortal(sec->lines[p]->portal, sec))
           return false;
      }
      // Sector checks out...
   }

   // Now the fun begins! Checking the actual groups for correct backlinks.
   // this needs to be done before the indirect link information is gathered to
   // make sure every link is two-way.
   for(i = 0; i < groupcount; i++)
   {
      for(p = 0; p < groupcount; p++)
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
      }
   }

   // That first loop has to complete before this can be run!
   for(i = 0; i < groupcount; i++)
      P_GatherLinks(i, 0, 0, 0, R_NOGROUP);

   // SoM: one last step. Find all map architecture with a group id of -1 and 
   // assign it to group 0
   for(i = 0; i < numsectors; i++)
   {
      if(sectors[i].groupid == R_NOGROUP)
         R_SetSectorGroupID(sectors + i, 0);
   }
   
   // Last step is to put zerolink in every link that goes from a group to that same group
   for(i = 0; i < groupcount; i++)
   {
      if(!linktable[i * groupcount + i])
         linktable[i * groupcount + i] = &zerolink;
   }

   // Everything checks out... let's run the portals
   useportalgroups = true;
   P_GlobalPortalStateCheck();

   // haleyjd 05/17/13: mark all blockmap cells where portals live.
   P_buildPortalMap();
   
   return true;
}

//
// P_LinkRejectTable
//
// Currently just clears each group for every other group.
//
void P_LinkRejectTable()
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
bool EV_PortalTeleport(Mobj *mo, const linkoffset_t *link)
{
   fixed_t moz = mo->z;
   fixed_t momx = mo->momx;
   fixed_t momy = mo->momy;
   fixed_t momz = mo->momz;
   //fixed_t vh = mo->player ? mo->player->viewheight : 0;

   if(!mo || !link)
      return 0;

   // ioanch 20160113: don't teleport. Just change x and y
   P_UnsetThingPosition(mo);
   mo->x += link->x;
   mo->y += link->y;
   mo->z = moz + link->z;
   // ioanch 20160123: only use interpolation for non-player objects
   // players are exposed to bugs if they interpolate here
   if(mo->player && mo->player->mo == mo)
   {
      mo->backupPosition();
   }
   else
   {
      // translate the interpolated coordinates
      mo->prevpos.x += link->x;
      mo->prevpos.y += link->y;
      mo->prevpos.z += link->z;
   }
   P_SetThingPosition(mo);

   

   mo->momx = momx;
   mo->momy = momy;
   mo->momz = momz;

   // SoM: Boom's code for silent teleports. Fixes view bob jerk.
   // Adjust a player's view, in case there has been a height change
   if(mo->player)
   {
      // Save the current deltaviewheight, used in stepping
      fixed_t deltaviewheight = mo->player->deltaviewheight;

      // Clear deltaviewheight, since we don't want any changes now
      mo->player->deltaviewheight = 0;

      // Set player's view according to the newly set parameters
      P_CalcHeight(mo->player);

      mo->player->prevviewz = mo->player->viewz;

      // Reset the delta to have the same dynamics as before
      mo->player->deltaviewheight = deltaviewheight;

      if(mo->player == players + displayplayer)
          P_ResetChasecam();
   }

   //mo->backupPosition();
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
//
static int P_GetPortalState(portal_t *portal, int sflags, bool obscured)
{
   bool active;
   int     ret = sflags & (PF_FLAGMASK | PS_OVERLAYFLAGS | PO_OPACITYMASK);
   
   if(!portal)
      return 0;
      
   active = !obscured && !(portal->flags & PF_DISABLED) && !(sflags & PF_DISABLED);
   
   if(active && !(portal->flags & PF_NORENDER) && !(sflags & PF_NORENDER))
      ret |= PS_VISIBLE;
      
   // Next two flags are for linked portals only
   active = (active && portal->type == R_LINKED && useportalgroups == true);
      
   if(active && !(portal->flags & PF_NOPASS) && !(sflags & PF_NOPASS))
      ret |= PS_PASSABLE;
   
   if(active && !(portal->flags & PF_BLOCKSOUND) && !(sflags & PF_BLOCKSOUND))
      ret |= PS_PASSSOUND;
      
   return ret;
}

void P_CheckCPortalState(sector_t *sec)
{
   bool     obscured;
   
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
   bool     obscured;
   
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
   // set new value
   sec->floorheight = h;
   sec->floorheightf = M_FixedToFloat(sec->floorheight);

   // check floor portal state
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
   // set new value
   sec->ceilingheight = h;
   sec->ceilingheightf = M_FixedToFloat(sec->ceilingheight);

   // check ceiling portal state
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

//
// P_LinePortalCrossing
//
// ioanch 20160106
// Checks if any line portals are crossed between (x, y) and (x+dx, y+dy),
// updating the target position correctly
//
v2fixed_t P_LinePortalCrossing(fixed_t x, fixed_t y, fixed_t dx, fixed_t dy, int *group)
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
   int recprotection = 100000;
   
   // keep track of source coordinates. If any of them is repeated, then we have 
   // an infinite loop
   PODCollection<v2fixed_t> prevcoords;
   
   do
   {
      for(v2fixed_t prev : prevcoords)
         if(prev == cur)  // exit if any previous one is found
            break;
      prevcoords.add(cur);
      
      res = CAM_PathTraverse(cur, fin, CAM_ADDLINES | CAM_REQUIRELINEPORTALS, 
                             [&cur, &fin, group](const intercept_t *in, 
                                                 const divline_t &trace)
      {

         const line_t *line = in->d.line;

         // must be a portal line
         if(!(line->pflags & PS_PASSABLE))
            return true;

         // rule out invalid cases
         if(in->frac <= 0 || in->frac > FRACUNIT)
            return true;

         // must be a valid portal line
         int fromid = line->portal->data.link.fromid;
         int toid = line->portal->data.link.toid;
         if(fromid == toid)
            return true;

         // must face the user
         if(P_PointOnLineSide(trace.x, trace.y, line) == 1)
            return true;

         // link must be valid
         const linkoffset_t *link = P_GetLinkIfExists(fromid, toid);
         if(!link)
            return true;

         // update the fields
         cur.x += FixedMul(trace.dx, in->frac) + link->x;
         cur.y += FixedMul(trace.dy, in->frac) + link->y;
         fin += *link;
         if(group)
            *group = toid;

         return false;
      });
      --recprotection;
      
      // Continue looking for portals after passing through one, from the new
      // coordinates
   } while (!res && recprotection);

   if(!recprotection)
      C_Printf("Warning: P_PortalCrossing loop");

   return fin;
}

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
         const int *block = gBlockGroups[y * bmapwidth + x];
         for(int i = 1; i <= block[0]; ++i)
         {
#ifdef RANGECHECK
            if(block[i] < 0 || block[i] >= gcount)
               I_Error("P_TransPortalBlockWalker: i (%d) out of range (count %d)", block[i], gcount);
#endif
            // Add to queue and visitlist
            if(!accessedgroupids[block[i]])
            {
               accessedgroupids[block[i]] = true;
               groupqueue[queueback++] = block[i];
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

   bool *groupvisit = ecalloc(bool *, numgroups, sizeof(bool));
   
   auto pflags = ceiling ? &sector_t::c_pflags : &sector_t::f_pflags;
   auto portal = ceiling ? &sector_t::c_portal : &sector_t::f_portal;
   
   int loopprotection = 100000;
      
   while(sector->*pflags & PS_PASSABLE && loopprotection--)
   {
      groupvisit[sector->groupid] = true;
      const linkoffset_t *link =
         P_GetLinkOffset(sector->groupid, (sector->*portal)->data.link.toid);

      if(!link->x && !link->y)   // if link happens to be 0
      {
         efree(groupvisit);
         return sector;
      }
      
      // move into the new sector
      x += link->x;
      y += link->y;
      sector = R_PointInSubsector(x, y)->sector;
      if(groupvisit[sector->groupid])
      {
         break;   // erroneous case, so quit
      }
   }
   
   if(loopprotection < 0)
      C_Printf("Warning: P_ExtremeSectorAtPoint loop");
   
   efree(groupvisit);
   return sector;
}

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
   if(sector->f_pflags & PS_PASSABLE && 
      topz < sector->f_portal->data.link.planez)
   {
      return false;
   }
   if(sector->c_pflags & PS_PASSABLE &&
      mobj->z > sector->c_portal->data.link.planez)
   {
      return false;
   }
   return true;
}

//
// P_ThingReachesGroupVertically
//
// Simple function that just checks if mobj is in a position that vertically
// points to groupid. THis does NOT change gGroupVisit.
//
bool P_PointReachesGroupVertically(fixed_t cx, fixed_t cy, fixed_t cmidz,
                                   int cgroupid, int tgroupid, const sector_t *csector,
                                   fixed_t midzhint)
{
   if(cgroupid == tgroupid)
      return true;

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

   const sector_t *sector;
   int groupid;
   fixed_t x, y;
   
   const linkoffset_t *link;

   for(int i = 0; i < 2; ++i)
   {
      sector = csector;
      groupid = cgroupid;
      x = cx, y = cy;
      
      while(sector->*pflags[i] & PS_PASSABLE)
      {
         link = P_GetLinkOffset(groupid, (sector->*portal[i])->data.link.toid);
         x += link->x;
         y += link->y;
         sector = R_PointInSubsector(x, y)->sector;
         groupid = sector->groupid;
         if(groupid == tgroupid)
            return true;
         if(groupVisit[groupid])
            break;
         groupVisit[groupid] = true;
      }
   }
   return false;
}

// EOF

