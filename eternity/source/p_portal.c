// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2004 Stephen McGranahan
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


#ifdef R_PORTALS

#include "c_io.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_bsp.h"
#include "r_things.h"

#ifdef R_LINKEDPORTALS

// SoM: Linked portals
// this list is allocated PU_LEVEL and is nullified in P_InitPortals. When the table is built
// it is allocated as a linnear buffer of groupcount * groupcount entries. The entries are
// arranged much like pixels in a screen buffer where the offset is located in
// linktable[startgroup * groupcount + targetgroup]
linkoffset_t **linktable = NULL;

// Ok, this is kinda trickey. The group list is a double pointer. The list is allocated static
// and is a expandable list. Each entry in the list is another list of ints which contain
// the sector numbers in that group. This sub-list is allocated PU_LEVEL because it is level
// specific.
static sector_t  **grouplist = NULL;
static int  groupcount = 0, grouplimit = 0;

// This flag is a big deal. Heh, if this is true a whole lot of code will operate differently.
// This flag is cleared on P_PortalInit and is ONLY to be set true by P_BuildLinkTable.
boolean useportalgroups = false;


void R_InitPortals(void);

void P_InitPortals(void)
{
   int i;
   linktable = NULL;

   groupcount = 0;
   for(i = 0; i < grouplimit; i++)
      grouplist[i] = NULL;

   useportalgroups = false;

   R_InitPortals();
}




// SoM: yes this is hackish, I admit :(
// this sets all mobjs inside the sector to have the sector id
void R_SetSectorGroupID(sector_t *sector, int groupid)
{
   mobj_t *mo;

   sector->groupid = groupid;

   for(mo = sector->thinglist; mo; mo = mo->snext)
      mo->groupid = groupid;
}




//
// P_CreatePortalGroup
//
// This function creates a portal group using the given sector as a starting point.
// The function will run through the sector's lines list, and add attached sectors to the group's
// sector list. As each sector is added the currently forming group's id is assigned to that
// sector. This will continue until every attached sector has been added to the list, thus
// defining a closed subspace of the map.

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

   // 
   if(listmax <= numsectors)
   {
      listmax = numsectors + 1;
      list = (sector_t **)Z_Realloc(list, sizeof(sector_t *) * listmax, PU_STATIC, NULL);
   }

   if(groupcount == grouplimit)
   {
      grouplimit = grouplimit ? (grouplimit << 1) : 8;
      grouplist = (sector_t **)Z_Realloc(grouplist, sizeof(sector_t *) * grouplimit, PU_STATIC, NULL);
   }

   list[count++] = from;
   R_SetSectorGroupID(from, groupcount);
   for(sec = 0; sec < count; sec++)
   {
      from = list[sec];

      for(i = 0; i < from->linecount; i++)
      {
         // add any sectors to the list which aren't already there.
         line = from->lines[i];
         if(sec2 = line->frontsector)
         {
            for(p = 0; p < count; p++)
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

   
         if(sec2 = line->backsector)
         {
            for(p = 0; p < count; p++)
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

   grouplist[groupcount] = (sector_t *)Z_Malloc(count * sizeof(sector_t *), PU_LEVEL, 0);
   memcpy(grouplist[groupcount], list, count * sizeof(sector_t *));

   return groupcount++;
}





linkoffset_t *P_GetLinkOffset(int startgroup, int targetgroup)
{
   if(!linktable)
   {
      I_Error("P_GetLinkOffset called with no linktable allocated.\n");
      return NULL;
   }

   if(startgroup < 0 || startgroup >= groupcount)
   {
      I_Error("P_GetLinkOffset called with start groupid out of bounds.\n");
      return NULL;
   }
   if(targetgroup < 0 || targetgroup >= groupcount)
   {
      I_Error("P_GetLinkOffset called with target groupid out of bounds.\n");
      return NULL;
   }


   return linktable[startgroup * groupcount + targetgroup];
}


void P_AddLinkOffset(int startgroup, int targetgroup, fixed_t x, fixed_t y, fixed_t z)
{
   linkoffset_t *link;

   if(!linktable)
   {
      I_Error("P_AddLinkOffset called with no linktable allocated.\n");
      return;
   }

   if(startgroup < 0 || startgroup >= groupcount)
   {
      I_Error("P_AddLinkOffset called with start groupid out of bounds.\n");
      return;
   }
   if(targetgroup < 0 || targetgroup >= groupcount)
   {
      I_Error("P_AddLinkOffset called with target groupid out of bounds.\n");
      return;
   }

   if(startgroup == targetgroup)
      return;


   link = (linkoffset_t *)Z_Malloc(sizeof(linkoffset_t), PU_LEVEL, 0);
   linktable[startgroup * groupcount + targetgroup] = link;
   
   link->x = x;
   link->y = y;
   link->z = z;
}






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
      C_Printf(FC_ERROR"P_BuildLinkTable: sector %i portal references the portal group which it belongs to.\nLinked portals are disabled.\n", i);
      return false;
   }
   if(portal->data.camera.groupid < 0 || portal->data.camera.groupid >= groupcount)
   {
      C_Printf(FC_ERROR"P_BuildLinkTable: sector %i portal has a groupid out of range.\nLinked portals are disabled.\n", i);
      return false;
   }

   // We've found a linked portal so add the entry to the table
   if(!(link = P_GetLinkOffset(sec->groupid, portal->data.camera.groupid)))
   {
      P_AddLinkOffset(sec->groupid, portal->data.camera.groupid, portal->data.camera.deltax,
                      portal->data.camera.deltay, portal->data.camera.deltaz);
   }
   else
   {
      // Check for consistency
      if(link->x != portal->data.camera.deltax ||
         link->y != portal->data.camera.deltay ||
         link->z != portal->data.camera.deltaz)
      {
         C_Printf(FC_ERROR"P_BuildLinkTable: sector %i in group %i contains inconsistent reference to group %i.\nLinked portals are disabled.\n", i, sec->groupid, sec->c_portal->data.camera.groupid);
         return false;
      }
   }

   return true;
}



static void P_GatherLinks(int group, fixed_t dx, fixed_t dy, fixed_t dz, int from)
{
   int i, p;
   linkoffset_t *link, **grouplist, **fromlist;

   // The main group has an indrect link with every group that links to a group it has a direct
   // link to it, or any group that has a link to a group the main group has an indirect link to.
   // huh.

   // First step: run through the list of groups this group has direct links to
   // from there, run the function again with each direct link.
   if(from == R_NOGROUP)
   {
      grouplist = linktable + group * groupcount;

      for(i = 0; i < groupcount; i++)
      {
         if(i == group)
            continue;

         if(link = grouplist[i])
            P_GatherLinks(group, link->x, link->y, link->z, i);
      }

      return;
   }


   grouplist = linktable + group * groupcount;
   fromlist = linktable + from * groupcount;

   // Second step run through the linked group's link list. Ignore any groups the main group is
   // already linked to. Add the deltas and add the entries, then call this function for groups
   // the linked group links to.
   for(p = 0; p < groupcount; p++)
   {
      if(p == group || p == from)
         continue;

      if(!(link = fromlist[p]) || grouplist[p])
         continue;

      P_AddLinkOffset(group, p, dx + link->x, dy + link->y, dz + link->z);
      P_GatherLinks(group, dx + link->x, dy + link->y, dz + link->z, p);
   }
}


void P_BuildLinkTable()
{
   int i, p;
   sector_t *sec;
   linkoffset_t *link, *backlink;

   if(!groupcount)
      return;

   // SoM: the last line of the table (starting at groupcount * groupcount) is used as a temporary
   // list for gathering links.
   linktable = (linkoffset_t **)Z_Malloc(sizeof(linkoffset_t *) * groupcount * groupcount, PU_LEVEL, 0);
   memset(linktable, 0, sizeof(linkoffset_t *) * groupcount * groupcount);

   // Run through the sectors check for invalid portal references.
   for(i = 0; i < numsectors; i++)
   {
      sec = sectors + i;
      // Make sure there are no groups that reference themselves or invalid group id numbers.
      if(sec->groupid < R_NOGROUP || sec->groupid >= groupcount)
      {
         C_Printf(FC_ERROR"P_BuildLinkTable: sector %i has a groupid out of range.\nLinked portals are disabled.\n", i);
         return;
      }

      if(!P_CheckLinkedPortal(sec->c_portal, sec))
         return;

      if(!P_CheckLinkedPortal(sec->f_portal, sec))
         return;

      for(p = 0; p < sec->linecount; p++)
      {
         if(!P_CheckLinkedPortal(sec->lines[p]->portal, sec))
            return;
      }
      // Sector checks out...
   }

   // Now the fun begins! Checking the actual groups for correct backlinks.
   // this needs to be done before the indirect link information is gathered to make sure
   // every link is two-way.
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
               C_Printf(FC_ERROR"Portal groups %i and %i link and backlink do not agree\nLinked portals are disabled\n", i, p);
               return;
            }
         }
         else if(link || backlink)
         {
            C_Printf(FC_ERROR"Portal group %i references group %i without a backlink.\nLinked portals are disabled\n", i, p);
            return;
         }
      }
   }

   // That first loop has to complete before this can be run!
   for(i = 0; i < groupcount; i++)
   {
      P_GatherLinks(i, 0, 0, 0, R_NOGROUP);
   }

   // Everything checks out... let's run the portals
   useportalgroups = true;
}
#endif
#endif

// EOF

