// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//  General plane mover routines
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "doomstat.h"
#include "p_map.h"
#include "p_portal.h"
#include "p_sector.h"
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
result_e T_MoveFloorDown(sector_t *sector, fixed_t speed, fixed_t dest, int crush)
{
   fixed_t lastpos;     

   bool flag;
   bool move3dsides;  // SoM: If set, check for and move 3d sides.
   bool moveattached; // SoM: if set, check for and move attached sector surfaces.

   move3dsides  = (sector->f_attached  && demo_version >= 331);
   moveattached = (sector->f_asurfaces && demo_version >= 331);

   // Moving a floor down
   if(sector->floorheight - speed < dest)
   {
      lastpos = sector->floorheight;
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
         if(!P_MoveAttached(sector, false, dest - lastpos, crush, instant))
         {
            P_MoveAttached(sector, false, lastpos - dest, crush, instant);
            return crushed;
         }
      }            

      P_SetFloorHeight(sector, dest);
      flag = P_CheckSector(sector,crush,dest-lastpos,0); //jff 3/19/98 use faster chk
      if(flag == true)                   
      {
         P_SetFloorHeight(sector, lastpos);
         P_CheckSector(sector,crush,lastpos-dest,0); //jff 3/19/98 use faster chk
         // SoM: if the move in the master sector was bad,
         // keep the 3d sides consistant.
         if(move3dsides)
            P_Scroll3DSides(sector, false, lastpos - dest, crush);
         if(moveattached)
            P_MoveAttached(sector, false, lastpos - dest, crush, instant);

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
         if(!P_MoveAttached(sector, false, -speed, crush, false))
         {
            P_MoveAttached(sector, false, speed, crush, false);
            return crushed;
         }
      }            

      lastpos = sector->floorheight;
      P_SetFloorHeight(sector, sector->floorheight - speed);
      flag = P_CheckSector(sector,crush,-speed,0); //jff 3/19/98 use faster chk

      // haleyjd 02/15/01: last of cph's current demo fixes:
      // cph - make more compatible with original Doom, by
      // reintroducing this code. This means floors can't lower
      // if objects are stuck in the ceiling 
      if((flag == true) && demo_compatibility)
      {
         P_SetFloorHeight(sector, lastpos);
         P_ChangeSector(sector, crush);

         if(move3dsides)
            P_Scroll3DSides(sector, false, speed, crush);
         if(moveattached)
            P_MoveAttached(sector, false, speed, crush, false);

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
result_e T_MoveFloorUp(sector_t *sector, fixed_t speed, fixed_t dest, int crush,
                       bool emulateStairCrush)
{
   fixed_t destheight;
   fixed_t lastpos;

   bool flag;
   bool move3dsides;  // SoM: If set, check for and move 3d sides.
   bool moveattached; // SoM: if set, check for and move attached sector surfaces.

   move3dsides  = (sector->f_attached  && demo_version >= 331);
   moveattached = (sector->f_asurfaces && demo_version >= 331);

   // Moving a floor up
   // jff 02/04/98 keep floor from moving thru ceilings
   // jff 2/22/98 weaken check to demo_compatibility
   if(g_opts.comp[comp_floors] || dest < sector->ceilingheight)
      destheight = dest;
   else
      destheight = sector->ceilingheight;
   
   if(sector->floorheight + speed > destheight)
   {
      lastpos = sector->floorheight;
      bool instant = lastpos > destheight;

      // SoM 9/19/02: If we are go, move 3d sides first.
      if(move3dsides)
      {
         flag = P_Scroll3DSides(sector, false, destheight-lastpos, crush);
         if(!flag)
         {
            P_Scroll3DSides(sector, false, lastpos-destheight, crush);
            return crushed;
         }
      }
      if(moveattached)
      {
         if(!P_MoveAttached(sector, false, destheight-lastpos, crush, instant))
         {
            P_MoveAttached(sector, false, lastpos-destheight, crush, instant);
            return crushed;
         }
      }            

      P_SetFloorHeight(sector, destheight);
      flag = P_CheckSector(sector,crush,destheight-lastpos,0); //jff 3/19/98 use faster chk

      if(flag == true)
      {
         P_SetFloorHeight(sector, lastpos);
         P_CheckSector(sector,crush,lastpos-destheight,0); //jff 3/19/98 use faster chk
         if(move3dsides)
            P_Scroll3DSides(sector, false, lastpos-destheight, crush);
         if(moveattached)
            P_MoveAttached(sector, false, lastpos-destheight, crush, instant);
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
         if(!P_MoveAttached(sector, false, speed, crush, false))
         {
            P_MoveAttached(sector, false, -speed, crush, false);
            return crushed;
         }
      }            

      // crushing is possible
      lastpos = sector->floorheight;
      P_SetFloorHeight(sector, sector->floorheight + speed);
      flag = P_CheckSector(sector,crush,speed,0); //jff 3/19/98 use faster chk
      if(flag == true)
      {
         // haleyjd 07/23/05: crush no longer boolean
         // Note: to make crushers that stop at heads, fail
         // to return crushed here even when crush is positive
         if(demo_version < 203 || g_opts.comp[comp_floors]) // killough 10/98
         {
            if(crush > 0 && !emulateStairCrush) //jff 1/25/98 fix floor crusher
               return crushed;
         }
         P_SetFloorHeight(sector, lastpos);
         P_CheckSector(sector,crush,-speed,0); //jff 3/19/98 use faster chk
         if(move3dsides)
            P_Scroll3DSides(sector, false, -speed, crush);
         if(moveattached)
            P_MoveAttached(sector, false, -speed, crush, false);

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
result_e T_MoveCeilingDown(sector_t *sector, fixed_t speed, fixed_t dest,
                           int crush, bool crushrest)
{
   fixed_t destheight;
   fixed_t lastpos;

   bool flag;
   bool move3dsides;  // SoM: If set, check for and move 3d sides.
   bool moveattached; // SoM: if set, check for and move attached sector surfaces.

   move3dsides  = (sector->c_attached  && demo_version >= 331);
   moveattached = (sector->c_asurfaces && demo_version >= 331);

   // moving a ceiling down
   // jff 02/04/98 keep ceiling from moving thru floors
   // jff 2/22/98 weaken check to demo_compatibility
   // killough 10/98: add comp flag
   if(g_opts.comp[comp_floors] || dest > sector->floorheight)
      destheight = dest;
   else
      destheight = sector->floorheight;
   
   if(sector->ceilingheight - speed < destheight)
   {
      lastpos = sector->ceilingheight;
      bool instant = lastpos < destheight;

      // SoM 9/19/02: If we are go, move 3d sides first.
      if(move3dsides)
      {
         flag = P_Scroll3DSides(sector, true, destheight-lastpos, crush);
         if(!flag)
         {
            P_Scroll3DSides(sector, true, lastpos-destheight, crush);
            return crushed;
         }
      }
      if(moveattached)
      {
         if(!P_MoveAttached(sector, true, destheight-lastpos, crush, instant))
         {
            P_MoveAttached(sector, true, lastpos-destheight, crush, instant);
            return crushed;
         }
      }            

      P_SetCeilingHeight(sector, destheight);
      flag = P_CheckSector(sector,crush,lastpos-destheight,1); //jff 3/19/98 use faster chk

      if(flag == true)
      {
         P_SetCeilingHeight(sector, lastpos);
         P_CheckSector(sector,crush,destheight-lastpos,1); //jff 3/19/98 use faster chk

         if(move3dsides)
            P_Scroll3DSides(sector, true, lastpos-destheight, crush);
         if(moveattached)
            P_MoveAttached(sector, true, lastpos-destheight, crush, instant);
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
         if(!P_MoveAttached(sector, true, -speed, crush, false))
         {
            P_MoveAttached(sector, true, speed, crush, false);
            return crushed;
         }
      }            

      // crushing is possible
      lastpos = sector->ceilingheight;
      P_SetCeilingHeight(sector, sector->ceilingheight - speed);
      flag = P_CheckSector(sector,crush,-speed,1); //jff 3/19/98 use faster chk

      if(flag == true)
      {
         // haleyjd 07/23/05: crush no longer boolean
         // Note: to make crushers that stop at heads, fail to
         // return crush here even when crush is positive.
         // ioanch 20160305: do so if crushrest is true
         if(!crushrest && crush > 0)
            return crushed;

         P_SetCeilingHeight(sector, lastpos);
         P_CheckSector(sector,crush,speed,1);      //jff 3/19/98 use faster chk

         if(move3dsides)
            P_Scroll3DSides(sector, true, speed, crush);
         if(moveattached)
            P_MoveAttached(sector, true, speed, crush, false);

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
result_e T_MoveCeilingUp(sector_t *sector, fixed_t speed, fixed_t dest, int crush)
{
   fixed_t lastpos;

   bool flag;
   bool move3dsides;  // SoM: If set, check for and move 3d sides.
   bool moveattached; // SoM: if set, check for and move attached sector surfaces.

   move3dsides  = (sector->c_attached  && demo_version >= 331);
   moveattached = (sector->c_asurfaces && demo_version >= 331);

   // moving a ceiling up
   if(sector->ceilingheight + speed > dest)
   {
      lastpos = sector->ceilingheight;
      bool instant = lastpos > dest;


      // SoM 9/19/02: If we are go, move 3d sides first.
      if(move3dsides)
      {
         flag = P_Scroll3DSides(sector, true, dest-lastpos, crush);
         if(!flag)
         {
            P_Scroll3DSides(sector, true, lastpos-dest, crush);
            return crushed;
         }
      }

      if(moveattached)
      {
         if(!P_MoveAttached(sector, true, dest - lastpos, crush, instant))
         {
            P_MoveAttached(sector, true, lastpos - dest, crush, instant);
            return crushed;
         }
      }            

      P_SetCeilingHeight(sector, dest);
      flag = P_CheckSector(sector,crush,dest-lastpos,1); //jff 3/19/98 use faster chk

      if(flag == true)
      {
         P_SetCeilingHeight(sector, lastpos);
         P_CheckSector(sector,crush,lastpos-dest,1); //jff 3/19/98 use faster chk
         if(move3dsides)
            P_Scroll3DSides(sector, true, lastpos-dest, crush);
         if(moveattached)
            P_MoveAttached(sector, true, lastpos - dest, crush, instant);
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
         if(!P_MoveAttached(sector, true, speed, crush, false))
         {
            P_MoveAttached(sector, true, -speed, crush, false);
            return crushed;
         }
      }

      //lastpos = sector->ceilingheight;
      P_SetCeilingHeight(sector, sector->ceilingheight + speed);
      P_CheckSector(sector,crush,speed,1); //jff 3/19/98 use faster chk
   }

   return ok;
}

//
// T_MoveFloorInDirection
//
// Dispatches to the correct movement routine depending on a thinker's
// floor movement direction.
//
result_e T_MoveFloorInDirection(sector_t *sector, fixed_t speed, fixed_t dest, 
                                int crush, int direction, bool emulateStairCrush)
{
   switch(direction)
   {
   case plat_up:
      return T_MoveFloorUp(sector, speed, dest, crush, emulateStairCrush);
   case plat_down:
      return T_MoveFloorDown(sector, speed, dest, crush);
   default:
      return ok;
   }
}

//
// T_MoveCeilingInDirection
//
// Dispatches to the correct movement routine depending on a thinker's
// ceiling movement direction.
//
result_e T_MoveCeilingInDirection(sector_t *sector, fixed_t speed, fixed_t dest,
                                  int crush, int direction)
{
   switch(direction)
   {
   case plat_up:
      return T_MoveCeilingUp(sector, speed, dest, crush);
   case plat_down:
      return T_MoveCeilingDown(sector, speed, dest, crush);
   default:
      return ok;
   }
}

// EOF

