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

#ifndef P_PORTAL_H__
#define P_PORTAL_H__

struct polyobj_t;

extern bool useportalgroups;

// ioanch 20160109: true if sector portals are in map
extern bool gMapHasSectorPortals;
extern bool gMapHasLinePortals;  // ioanch 20160131: also check line portals
extern bool *gGroupVisit;  // ioanch 20160121: a global helper array
extern const polyobj_t **gGroupPolyobject; // ioanch 20160227

#ifndef R_NOGROUP
// No link group. I know this means there is a signed limit on portal groups but
// do you think anyone is going to make a level with 2147483647 groups that 
// doesn't have NUTS in the wad name? I didn't think so either.
#define R_NOGROUP -1
#endif

struct linkdata_t;
struct portal_t;
struct sector_t;

//
// Sector_SetPortal arguments
//
enum
{
   paramPortal_argType = 1,
   paramPortal_argPlane = 2,
   paramPortal_argMisc = 3,

   paramPortal_planeFloor = 0,
   paramPortal_planeCeiling = 1,
   paramPortal_planeBoth = 2,

   paramPortal_normal = 0,
   paramPortal_copied = 1,
   paramPortal_skybox = 2,
   paramPortal_pplane = 3,
   paramPortal_horizon = 4,
   paramPortal_copyline = 5,
   paramPortal_linked = 6,
};

//
// P_PortalGroupCount
//
int P_PortalGroupCount();

//
// P_CreatePortalGroup
// This creates an portal group, and returns the id of that group
//
int P_CreatePortalGroup(sector_t *from);

//
// P_GatherSectors
// This function gathers adjacent sectors, starting with the supplied sector,
// and adds them to the specified group.
//
void P_GatherSectors(sector_t *from, int groupid);

void P_FindPolyobjectSectorCouples();  // called in P_SpawnSpecials

//
// R_BuildLinkTable
// Builds the link table. This should only be called after all the portals for
// the level have been created.
//
bool P_BuildLinkTable();
void P_MarkPortalClusters();
void P_MarkPolyobjPortalLinks();

void P_InitPortals();

bool EV_SectorPortalTeleport(Mobj *mo, const linkdata_t &ldata);
void P_PortalDidTeleport(Mobj *mo, fixed_t dx, fixed_t dy, fixed_t dz, int fromid, int toid);

void R_SetSectorGroupID(sector_t *sector, int groupid);

void P_FitLinkOffsetsToPortal(const linkdata_t &ldata);

//
// P_CheckCPortalState
// 
// Checks the state of the ceiling portal in the given sector and updates
// the state flags accordingly.
//
void P_CheckCPortalState(sector_t *sec);


// P_CheckFPortalState
// 
// Checks the state of the floor portal in the given sector and updates
// the state flags accordingly.
//
void P_CheckFPortalState(sector_t *sec);

//
// P_CheckLPortalState
// 
// Checks the state of the portal in the given line and updates
// the state flags accordingly.
//
void P_CheckLPortalState(line_t *line);

//
// P_SetFloorHeight
// This function will set the floor height, and update
// the float version of the floor height as well.
//
void P_SetFloorHeight(sector_t *sec, fixed_t h);

//
// P_SetCeilingHeight
// This function will set the ceiling height, and update
// the float version of the ceiling height as well.
//
void P_SetCeilingHeight(sector_t *sec, fixed_t h);

//
// P_SetPortalBehavior
//
// Sets the behavior flags for a portal. This function then iterates through all
// the sectors and lines in the currently loaded map and updates the state flags
// as such, this function shouldn't really be used a lot.
//
void P_SetPortalBehavior(portal_t *portal, int newbehavior);

//
// P_SetFPortalBehavior
//
// This function sets the behavior flags for the floor portal of a given sector
// and updates the state flags for the surface.
//
void P_SetFPortalBehavior(sector_t *sec, int newbehavior);

//
// P_SetCPortalBehavior
//
// This function sets the behavior flags for the ceiling portal of a given 
// sector and updates the state flags for the surface.
//
void P_SetCPortalBehavior(sector_t *sec, int newbehavior);

//
// P_SetLPortalBehavior
//
// This function sets the behavior flags for the portal of a given line
// and updates the state flags for the surface.
//
void P_SetLPortalBehavior(line_t *line, int newbehavior);

void P_MoveGroupCluster(int outgroup, int ingroup, bool *groupvisit, fixed_t dx,
                        fixed_t dy, bool setpolyref, const polyobj_t *po);
void P_ForEachClusterGroup(int outgroup, int ingroup, bool *groupvisit,
                           bool (*func)(int groupid, void *context), void *context);

fixed_t P_CeilingPortalZ(const sector_t &sector);
fixed_t P_FloorPortalZ(const sector_t &sector);

// Group mappings
void P_BuildSectorGroupMappings();
sector_t **P_GetSectorsWithGroupId(int groupid, int *count);
bool P_PortalLayersByPoly(int groupid1, int groupid2);

#endif

// EOF

