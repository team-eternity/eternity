//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
// Purpose: General plane mover routines.
// Authors: James Haley, Ioan Chera
//

#include "z_zone.h"

#include "doomstat.h"
#include "p_map.h"
#include "p_portal.h"
#include "p_sector.h"
#include "p_slopes.h"
#include "p_spec.h"
#include "r_defs.h"
#include "r_state.h"
#include "t_plane.h"

//=============================================================================
//
// T_Move Functions
//
// Move a plane (floor or ceiling) and check for crushing. Called
// every tick by all actions that move floors or ceilings.
//
// Passed the sector to move a plane in, the speed to move it at,
// the dest height it is to achieve, and whether it crushes obstacles.
//
// Returns a result_e:
//  ok       - plane moved normally, has not achieved destination yet
//  pastdest - plane moved normally and is now at destination height
//  crushed  - plane encountered an obstacle, is holding until removed
//
// These were all originally one gigantic function which shared virtually
// none of its code between cases, called T_MovePlane.
//

//
// T_MoveFloorDown
//
// Handle a floor plane moving downward.
//
result_e T_MoveFloorDown(sector_t *const sector, fixed_t speed, fixed_t dest, int crush)
{
    fixed_t lastpos;

    bool flag;
    bool move3dsides;  // SoM: If set, check for and move 3d sides.
    bool moveattached; // SoM: if set, check for and move attached sector surfaces.

    move3dsides  = (sector->srf.floor.attached && demo_version >= 331);
    moveattached = (sector->srf.floor.asurfaces && demo_version >= 331);

    // Moving a floor down
    if(sector->srf.floor.height - speed < dest)
    {
        lastpos      = sector->srf.floor.height;
        bool instant = lastpos < dest;

        // SoM 9/19/02: If we are go, move 3d sides first.
        if(move3dsides)
        {
            flag = P_Scroll3DSides(sector, false, dest - lastpos, crush);
            if(!flag)
            {
                P_Scroll3DSides(sector, false, lastpos - dest, crush);
                return crushed;
            }
        }
        if(moveattached)
        {
            if(!P_MoveAttached(sector, surf_floor, dest - lastpos, crush, instant))
            {
                P_MoveAttached(sector, surf_floor, lastpos - dest, crush, instant);
                return crushed;
            }
        }

        P_SetSectorHeight(*sector, surf_floor, dest);
        // jff 3/19/98 use faster chk
        flag = P_CheckSector(sector, crush, dest - lastpos, CheckSectorPlane::floor);
        if(flag == true)
        {
            P_SetSectorHeight(*sector, surf_floor, lastpos);

            // jff 3/19/98 use faster chk
            P_CheckSector(sector, crush, lastpos - dest, CheckSectorPlane::floor);
            // SoM: if the move in the master sector was bad,
            // keep the 3d sides consistant.
            if(move3dsides)
                P_Scroll3DSides(sector, false, lastpos - dest, crush);
            if(moveattached)
                P_MoveAttached(sector, surf_floor, lastpos - dest, crush, instant);

            // Note from SoM: Shouldn't we return crushed if the
            // last move was rejected?
        }
        if(instant)
            P_SaveSectorPosition(*sector, ssurf_floor);
        return pastdest;
    }
    else
    {
        // SoM 9/19/02: If we are go, move 3d sides first.
        if(move3dsides)
        {
            flag = P_Scroll3DSides(sector, false, -speed, crush);
            if(!flag)
            {
                P_Scroll3DSides(sector, false, speed, crush);
                return crushed;
            }
        }
        if(moveattached)
        {
            if(!P_MoveAttached(sector, surf_floor, -speed, crush, false))
            {
                P_MoveAttached(sector, surf_floor, speed, crush, false);
                return crushed;
            }
        }

        lastpos = sector->srf.floor.height;
        P_SetSectorHeight(*sector, surf_floor, sector->srf.floor.height - speed);
        // jff 3/19/98 use faster chk
        flag = P_CheckSector(sector, crush, -speed, CheckSectorPlane::floor);

        // haleyjd 02/15/01: last of cph's current demo fixes:
        // cph - make more compatible with original Doom, by
        // reintroducing this code. This means floors can't lower
        // if objects are stuck in the ceiling
        if((flag == true) && demo_compatibility)
        {
            P_SetSectorHeight(*sector, surf_floor, lastpos);
            P_ChangeSector(sector, crush);

            if(move3dsides)
                P_Scroll3DSides(sector, false, speed, crush);
            if(moveattached)
                P_MoveAttached(sector, surf_floor, speed, crush, false);

            return crushed;
        }
    }

    return ok;
}

//
// T_MoveFloorUp
//
// Handle a floor plane moving upward.
//
result_e T_MoveFloorUp(sector_t *const sector, fixed_t speed, fixed_t dest, int crush, bool emulateStairCrush)
{
    fixed_t destheight;
    fixed_t lastpos;

    bool flag;
    bool move3dsides;  // SoM: If set, check for and move 3d sides.
    bool moveattached; // SoM: if set, check for and move attached sector surfaces.

    move3dsides  = (sector->srf.floor.attached && demo_version >= 331);
    moveattached = (sector->srf.floor.asurfaces && demo_version >= 331);

    // Moving a floor up
    // jff 02/04/98 keep floor from moving thru ceilings
    // jff 2/22/98 weaken check to demo_compatibility
    fixed_t ceilinglimit = sector->srf.ceiling.height - pSlopeHeights[sector - sectors].touchheight;
    if(getComp(comp_floors) || dest < ceilinglimit)
        destheight = dest;
    else
        destheight = ceilinglimit;

    if(sector->srf.floor.height + speed > destheight)
    {
        lastpos      = sector->srf.floor.height;
        bool instant = lastpos > destheight;

        // SoM 9/19/02: If we are go, move 3d sides first.
        if(move3dsides)
        {
            flag = P_Scroll3DSides(sector, false, destheight - lastpos, crush);
            if(!flag)
            {
                P_Scroll3DSides(sector, false, lastpos - destheight, crush);
                return crushed;
            }
        }
        if(moveattached)
        {
            if(!P_MoveAttached(sector, surf_floor, destheight - lastpos, crush, instant))
            {
                P_MoveAttached(sector, surf_floor, lastpos - destheight, crush, instant);
                return crushed;
            }
        }

        P_SetSectorHeight(*sector, surf_floor, destheight);
        // jff 3/19/98 use faster chk
        flag = P_CheckSector(sector, crush, destheight - lastpos, CheckSectorPlane::floor);

        if(flag == true)
        {
            P_SetSectorHeight(*sector, surf_floor, lastpos);
            // jff 3/19/98 use faster chk
            P_CheckSector(sector, crush, lastpos - destheight, CheckSectorPlane::floor);
            if(move3dsides)
                P_Scroll3DSides(sector, false, lastpos - destheight, crush);
            if(moveattached)
                P_MoveAttached(sector, surf_floor, lastpos - destheight, crush, instant);
        }
        if(instant)
            P_SaveSectorPosition(*sector, ssurf_floor);
        return pastdest;
    }
    else
    {
        // SoM 9/19/02: If we are go, move 3d sides first.
        if(move3dsides)
        {
            flag = P_Scroll3DSides(sector, false, speed, crush);
            if(!flag)
            {
                P_Scroll3DSides(sector, false, -speed, crush);
                return crushed;
            }
        }
        if(moveattached)
        {
            if(!P_MoveAttached(sector, surf_floor, speed, crush, false))
            {
                P_MoveAttached(sector, surf_floor, -speed, crush, false);
                return crushed;
            }
        }

        // crushing is possible
        lastpos = sector->srf.floor.height;
        P_SetSectorHeight(*sector, surf_floor, sector->srf.floor.height + speed);
        // jff 3/19/98 use faster chk
        flag = P_CheckSector(sector, crush, speed, CheckSectorPlane::floor);
        if(flag == true)
        {
            // haleyjd 07/23/05: crush no longer boolean
            // Note: to make crushers that stop at heads, fail
            // to return crushed here even when crush is positive
            if(demo_version < 203 || getComp(comp_floors)) // killough 10/98
            {
                if(crush > 0 && !emulateStairCrush) // jff 1/25/98 fix floor crusher
                    return crushed;
            }
            P_SetSectorHeight(*sector, surf_floor, lastpos);
            // jff 3/19/98 use faster chk
            P_CheckSector(sector, crush, -speed, CheckSectorPlane::floor);
            if(move3dsides)
                P_Scroll3DSides(sector, false, -speed, crush);
            if(moveattached)
                P_MoveAttached(sector, surf_floor, -speed, crush, false);

            return crushed;
        }
    }

    return ok;
}

//
// T_MoveCeilingDown
//
// Handle a ceiling plane moving downward.
// ioanch 20160305: added crushrest parameter
//
result_e T_MoveCeilingDown(sector_t *const sector, fixed_t speed, fixed_t dest, int crush, bool crushrest)
{
    fixed_t destheight;
    fixed_t lastpos;

    bool flag;
    bool move3dsides;  // SoM: If set, check for and move 3d sides.
    bool moveattached; // SoM: if set, check for and move attached sector surfaces.

    move3dsides  = (sector->srf.ceiling.attached && demo_version >= 331);
    moveattached = (sector->srf.ceiling.asurfaces && demo_version >= 331);

    // moving a ceiling down
    // jff 02/04/98 keep ceiling from moving thru floors
    // jff 2/22/98 weaken check to demo_compatibility
    // killough 10/98: add comp flag
    fixed_t floorlimit = sector->srf.floor.height + pSlopeHeights[sector - sectors].touchheight;
    if(getComp(comp_floors) || dest > floorlimit)
        destheight = dest;
    else
        destheight = floorlimit;

    if(sector->srf.ceiling.height - speed < destheight)
    {
        lastpos      = sector->srf.ceiling.height;
        bool instant = lastpos < destheight;

        // SoM 9/19/02: If we are go, move 3d sides first.
        if(move3dsides)
        {
            flag = P_Scroll3DSides(sector, true, destheight - lastpos, crush);
            if(!flag)
            {
                P_Scroll3DSides(sector, true, lastpos - destheight, crush);
                return crushed;
            }
        }
        if(moveattached)
        {
            if(!P_MoveAttached(sector, surf_ceil, destheight - lastpos, crush, instant))
            {
                P_MoveAttached(sector, surf_ceil, lastpos - destheight, crush, instant);
                return crushed;
            }
        }

        P_SetSectorHeight(*sector, surf_ceil, destheight);
        // jff 3/19/98 use faster chk
        flag = P_CheckSector(sector, crush, lastpos - destheight, CheckSectorPlane::ceiling);

        if(flag == true)
        {
            P_SetSectorHeight(*sector, surf_ceil, lastpos);
            // jff 3/19/98 use faster chk
            P_CheckSector(sector, crush, destheight - lastpos, CheckSectorPlane::ceiling);

            if(move3dsides)
                P_Scroll3DSides(sector, true, lastpos - destheight, crush);
            if(moveattached)
                P_MoveAttached(sector, surf_ceil, lastpos - destheight, crush, instant);
        }

        if(instant)
            P_SaveSectorPosition(*sector, ssurf_ceiling);
        return pastdest;
    }
    else
    {
        // SoM 9/19/02: If we are go, move 3d sides first.
        if(move3dsides)
        {
            flag = P_Scroll3DSides(sector, true, -speed, crush);
            if(!flag)
            {
                P_Scroll3DSides(sector, true, speed, crush);
                return crushed;
            }
        }
        if(moveattached)
        {
            if(!P_MoveAttached(sector, surf_ceil, -speed, crush, false))
            {
                P_MoveAttached(sector, surf_ceil, speed, crush, false);
                return crushed;
            }
        }

        // crushing is possible
        lastpos = sector->srf.ceiling.height;
        P_SetSectorHeight(*sector, surf_ceil, sector->srf.ceiling.height - speed);
        // jff 3/19/98 use faster chk
        flag = P_CheckSector(sector, crush, -speed, CheckSectorPlane::ceiling);

        if(flag == true)
        {
            // haleyjd 07/23/05: crush no longer boolean
            // Note: to make crushers that stop at heads, fail to
            // return crush here even when crush is positive.
            // ioanch 20160305: do so if crushrest is true
            if(!crushrest && crush > 0)
                return crushed;

            P_SetSectorHeight(*sector, surf_ceil, lastpos);
            // jff 3/19/98 use faster chk
            P_CheckSector(sector, crush, speed, CheckSectorPlane::ceiling);

            if(move3dsides)
                P_Scroll3DSides(sector, true, speed, crush);
            if(moveattached)
                P_MoveAttached(sector, surf_ceil, speed, crush, false);

            return crushed;
        }
    }

    return ok;
}

//
// T_MoveCeilingUp
//
// Handle a ceiling plane moving upward.
//
result_e T_MoveCeilingUp(sector_t *const sector, fixed_t speed, fixed_t dest, int crush)
{
    fixed_t lastpos;

    bool flag;
    bool move3dsides;  // SoM: If set, check for and move 3d sides.
    bool moveattached; // SoM: if set, check for and move attached sector surfaces.

    move3dsides  = (sector->srf.ceiling.attached && demo_version >= 331);
    moveattached = (sector->srf.ceiling.asurfaces && demo_version >= 331);

    // moving a ceiling up
    if(sector->srf.ceiling.height + speed > dest)
    {
        lastpos      = sector->srf.ceiling.height;
        bool instant = lastpos > dest;

        // SoM 9/19/02: If we are go, move 3d sides first.
        if(move3dsides)
        {
            flag = P_Scroll3DSides(sector, true, dest - lastpos, crush);
            if(!flag)
            {
                P_Scroll3DSides(sector, true, lastpos - dest, crush);
                return crushed;
            }
        }

        if(moveattached)
        {
            if(!P_MoveAttached(sector, surf_ceil, dest - lastpos, crush, instant))
            {
                P_MoveAttached(sector, surf_ceil, lastpos - dest, crush, instant);
                return crushed;
            }
        }

        P_SetSectorHeight(*sector, surf_ceil, dest);
        // jff 3/19/98 use faster chk
        flag = P_CheckSector(sector, crush, dest - lastpos, CheckSectorPlane::ceiling);

        if(flag == true)
        {
            P_SetSectorHeight(*sector, surf_ceil, lastpos);
            // jff 3/19/98 use faster chk
            P_CheckSector(sector, crush, lastpos - dest, CheckSectorPlane::ceiling);
            if(move3dsides)
                P_Scroll3DSides(sector, true, lastpos - dest, crush);
            if(moveattached)
                P_MoveAttached(sector, surf_ceil, lastpos - dest, crush, instant);
        }
        if(instant)
            P_SaveSectorPosition(*sector, ssurf_ceiling);
        return pastdest;
    }
    else
    {
        if(move3dsides)
        {
            flag = P_Scroll3DSides(sector, true, speed, crush);
            if(!flag)
            {
                P_Scroll3DSides(sector, true, -speed, crush);
                return crushed;
            }
        }

        if(moveattached)
        {
            if(!P_MoveAttached(sector, surf_ceil, speed, crush, false))
            {
                P_MoveAttached(sector, surf_ceil, -speed, crush, false);
                return crushed;
            }
        }

        // lastpos = sector->ceilingheight;
        P_SetSectorHeight(*sector, surf_ceil, sector->srf.ceiling.height + speed);
        // jff 3/19/98 use faster chk
        P_CheckSector(sector, crush, speed, CheckSectorPlane::ceiling);
    }

    return ok;
}

//
// T_MoveFloorInDirection
//
// Dispatches to the correct movement routine depending on a thinker's
// floor movement direction.
//
result_e T_MoveFloorInDirection(sector_t *sector, fixed_t speed, fixed_t dest, int crush, int direction,
                                bool emulateStairCrush)
{
    switch(direction)
    {
    case plat_up:   return T_MoveFloorUp(sector, speed, dest, crush, emulateStairCrush);
    case plat_down: return T_MoveFloorDown(sector, speed, dest, crush);
    default:        return ok;
    }
}

//
// T_MoveCeilingInDirection
//
// Dispatches to the correct movement routine depending on a thinker's
// ceiling movement direction.
//
result_e T_MoveCeilingInDirection(sector_t *sector, fixed_t speed, fixed_t dest, int crush, int direction)
{
    switch(direction)
    {
    case plat_up:   return T_MoveCeilingUp(sector, speed, dest, crush);
    case plat_down: return T_MoveCeilingDown(sector, speed, dest, crush);
    default:        return ok;
    }
}

// EOF

