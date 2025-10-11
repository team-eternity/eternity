//
// The Eternity Engine
// Copyright (C) 2025 Stephen McGranahan et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Linked portals.
// Authors: Stephen McGranahan, James Haley, Ioan Chera, Max Waine
//

#ifndef P_PORTAL_H__
#define P_PORTAL_H__

#include "r_defs.h"
#include "r_portal.h"

struct polyobj_t;

extern bool useportalgroups;

// ioanch 20160109: true if sector portals are in map
extern bool              gMapHasSectorPortals;
extern bool              gMapHasLinePortals; // ioanch 20160131: also check line portals
extern bool             *gGroupVisit;        // ioanch 20160121: a global helper array
extern const polyobj_t **gGroupPolyobject;   // ioanch 20160227

#ifndef R_NOGROUP
// No link group. I know this means there is a signed limit on portal groups but
// do you think anyone is going to make a level with 2147483647 groups that
// doesn't have NUTS in the wad name? I didn't think so either.
#define R_NOGROUP -1
#endif

struct linkdata_t;
struct portal_t;
struct rendersector_t;
struct sector_t;

//
// Sector_SetPortal arguments
//
enum
{
    paramPortal_argType  = 1,
    paramPortal_argPlane = 2,
    paramPortal_argMisc  = 3,

    paramPortal_planeFloor   = 0,
    paramPortal_planeCeiling = 1,
    paramPortal_planeBoth    = 2,

    paramPortal_normal   = 0,
    paramPortal_copied   = 1,
    paramPortal_skybox   = 2,
    paramPortal_pplane   = 3,
    paramPortal_horizon  = 4,
    paramPortal_copyline = 5,
    paramPortal_linked   = 6,
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

void P_FindPolyobjectSectorCouples(); // called in P_SpawnSpecials

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
// Checks the state of the floor/ceiling portal in the given sector and updates
// the state flags accordingly.
//
void P_CheckRenderSectorPortalState(rendersector_t &sector, surf_e type);
void P_CheckSectorPortalState(sector_t &sector, surf_e type);

//
// Checks the state of the portal in the given line and updates
// the state flags accordingly.
//
void P_CheckLPortalState(line_t *line);

void P_SetSectorHeight(sector_t &sec, surf_e surf, fixed_t h);
void P_SetRenderSectorHeight(rendersector_t &sec, surf_e surf, fixed_t h);

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

void P_MoveGroupCluster(int outgroup, int ingroup, bool *groupvisit, fixed_t dx, fixed_t dy, bool setpolyref,
                        const polyobj_t *po);
void P_ForEachClusterGroup(int outgroup, int ingroup, bool *groupvisit, bool (*func)(int groupid, void *context),
                           void *context);

fixed_t               P_PortalZ(const surface_t &surface, fixed_t x, fixed_t y);
inline static fixed_t P_PortalZ(const surface_t &surface, v2fixed_t v)
{
    return P_PortalZ(surface, v.x, v.y);
}
fixed_t               P_PortalZ(const surface_t &surface);
inline static fixed_t P_PortalZ(surf_e surf, const rendersector_t &sector)
{
    return P_PortalZ(sector.srf[surf]);
}

// Group mappings
void       P_BuildSectorGroupMappings();
const int *P_GetSectorsWithGroupId(int groupid, int *count);
bool       P_PortalLayersByPoly(int groupid1, int groupid2);

const int *P_GetSectorPortalNeighbors(const sector_t &sector, surf_e surf, int *count);

//
// True if it's a passable portal with overlay
//
inline static bool P_IsLiquidOverlaylinkedPortal(const surface_t &surface)
{
    return (surface.pflags & (PS_PASSABLE | PS_OVERLAY)) == (PS_PASSABLE | PS_OVERLAY);
}

#endif

// EOF

