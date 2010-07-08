// Emacs style mode select   -*- C -*- 
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

#ifndef P_PORTAL_H__
#define P_PORTAL_H__


extern boolean useportalgroups;

#ifndef R_NOGROUP
// No link group. I know this means there is a signed limit on portal groups but
// do you think anyone is going to make a level with 2147483647 groups that 
// doesn't have NUTS in the wad name? I didn't think so either.
#define R_NOGROUP -1
#endif


// P_CreatePortalGroup
// This creates an portal group, and returns the id of that group
int P_CreatePortalGroup(sector_t *from);


// P_GatherSectors
// This function gathers adjacent sectors, starting with the supplied sector,
// and adds them to the specified group.
void P_GatherSectors(sector_t *from, int groupid);


// R_BuildLinkTable
// Builds the link table. This should only be called after all the portals for
// the level have been created.
boolean P_BuildLinkTable(void);

//
// P_LinkRejectTable
// Currently just clears each group for every other group.
void P_LinkRejectTable(void);
#include "linkoffs.h"

void P_InitPortals(void);

boolean EV_PortalTeleport(mobj_t *mo, linkoffset_t *link);

void R_SetSectorGroupID(sector_t *sector, int groupid);



//
// P_CheckCPortalState
// 
// Checks the state of the ceiling portal in the given sector and updates
// the state flags accordingly.
void P_CheckCPortalState(sector_t *sec);



//
// P_CheckFPortalState
// 
// Checks the state of the floor portal in the given sector and updates
// the state flags accordingly.
void P_CheckFPortalState(sector_t *sec);



//
// P_CheckLPortalState
// 
// Checks the state of the portal in the given line and updates
// the state flags accordingly.
void P_CheckLPortalState(line_t *line);



// P_SetFloorHeight
// This function will set the floor height, and update
// the float version of the floor height as well.
void P_SetFloorHeight(sector_t *sec, fixed_t h);


// P_SetCeilingHeight
// This function will set the ceiling height, and update
// the float version of the ceiling height as well.
void P_SetCeilingHeight(sector_t *sec, fixed_t h);


#endif

// EOF

