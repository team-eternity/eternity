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

#ifndef P_PORTAL_H__
#define P_PORTAL_H__


#ifdef R_LINKEDPORTALS
extern boolean useportalgroups;

#ifndef R_NOGROUP
// No link group. I know this means there is a signed limit on portal groups but
// do you think anyone is going to make a level with 2147483647 groups that 
// doesn't have NUTS in the wad name? I didn't think so either.
#define R_NOGROUP -1
#endif

// P_CreatePortalGroup
// Defines a grouping of sectors in the map as its own seperate subspace of the 
// map. This subspace can be related to other subspaces in the map by offset 
// values, thus allowing things like sound travel and monster AI to work through
// portrals. 
int P_CreatePortalGroup(sector_t *from);

// R_BuildLinkTable
// Builds the link table. This should only be called after all the portals for
// the level have been created.
void P_BuildLinkTable(void);

//
// P_LinkRejectTable
// Currently just clears each group for every other group.
void P_LinkRejectTable(void);
#include "linkoffs.h"


boolean EV_PortalTeleport(mobj_t *mo, linkoffset_t *link);

#endif

#endif

// EOF

