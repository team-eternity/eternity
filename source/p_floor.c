// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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
//  General plane mover and floor mover action routines
//  Floor motion, pure changer types, raising stairs. donuts, elevators
//
//-----------------------------------------------------------------------------

#include "doomstat.h"
#include "c_io.h"
#include "r_main.h"
#include "p_info.h"
#include "p_map.h"
#include "p_spec.h"
#include "p_tick.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "sounds.h"
#include "m_argv.h"

boolean P_ChangeSector(sector_t *, int);

//
// P_FloorSequence
//
// haleyjd 10/02/06: Starts a sound sequence for a floor action.
//
void P_FloorSequence(sector_t *s)
{
   if(silentmove(s))
      return;

   if(s->sndSeqID >= 0)
      S_StartSectorSequence(s, SEQ_FLOOR);
   else
      S_StartSectorSequenceName(s, "EEFloor", false);
}

///////////////////////////////////////////////////////////////////////
// 
// Plane (floor or ceiling), Floor motion and Elevator action routines
//
///////////////////////////////////////////////////////////////////////

//
// T_MovePlane()
//
// Move a plane (floor or ceiling) and check for crushing. Called
// every tick by all actions that move floors or ceilings.
//
// Passed the sector to move a plane in, the speed to move it at, 
// the dest height it is to achieve, whether it crushes obstacles,
// whether it moves a floor or ceiling, and the direction up or down 
// to move.
//
// Returns a result_e: 
//  ok - plane moved normally, has not achieved destination yet
//  pastdest - plane moved normally and is now at destination height
//  crushed - plane encountered an obstacle, is holding until removed
//
result_e T_MovePlane
( sector_t*     sector,
  fixed_t       speed,
  fixed_t       dest,
  int           crush,
  int           floorOrCeiling,
  int           direction )
{
   boolean     flag;
   fixed_t     lastpos;     
   fixed_t     destheight;  //jff 02/04/98 used to keep floors/ceilings
                            // from moving thru each other
   boolean     move3dsides; // SoM: If set, check for and move 3d sides.
   boolean     moveattached; // SoM: if set, check for and move attached sector surfaces.


   switch(floorOrCeiling)
   {
   case 0:
      move3dsides  = (sector->f_attached  && demo_version >= 331);
      moveattached = (sector->f_asurfaces && demo_version >= 331);
      
      // Moving a floor
      switch(direction)
      {
      case plat_down:
         // Moving a floor down
         if(sector->floorheight - speed < dest)
         {
            lastpos = sector->floorheight;

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
               if(!P_MoveAttached(sector, false, dest - lastpos, crush))
               {
                  P_MoveAttached(sector, false, lastpos - dest, crush);
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
                  P_MoveAttached(sector, false, lastpos - dest, crush);
               
               // Note from SoM: Shouldn't we return crushed if the
               // last move was rejected?
            }
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
               if(!P_MoveAttached(sector, false, -speed, crush))
               {
                  P_MoveAttached(sector, false, speed, crush);
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
                  P_MoveAttached(sector, false, speed, crush);

               return crushed;
            }
         }
         break;

      case plat_up:
         // Moving a floor up
         // jff 02/04/98 keep floor from moving thru ceilings
         // jff 2/22/98 weaken check to demo_compatibility
         destheight = (demo_version < 203 || comp[comp_floors] ||
                       dest<sector->ceilingheight)? // killough 10/98
                       dest : sector->ceilingheight;
         if(sector->floorheight + speed > destheight)
         {
            lastpos = sector->floorheight;

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
               if(!P_MoveAttached(sector, false, destheight-lastpos, crush))
               {
                  P_MoveAttached(sector, false, lastpos-destheight, crush);
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
                  P_MoveAttached(sector, false, lastpos-destheight, crush);
            }
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
               if(!P_MoveAttached(sector, false, speed, crush))
               {
                  P_MoveAttached(sector, false, -speed, crush);
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
               if(demo_version < 203 || comp[comp_floors]) // killough 10/98
               {
                  if(crush > 0) //jff 1/25/98 fix floor crusher
                     return crushed;
               }
               P_SetFloorHeight(sector, lastpos);
               P_CheckSector(sector,crush,-speed,0); //jff 3/19/98 use faster chk
               if(move3dsides)
                  P_Scroll3DSides(sector, false, -speed, crush);
               if(moveattached)
                  P_MoveAttached(sector, false, -speed, crush);
               
               return crushed;
            }
         }
         break;
      }
      break;
                                                                        
   case 1:
      move3dsides = sector->c_attached && demo_version >= 331;
      moveattached = (sector->c_asurfaces && demo_version >= 331);

      // moving a ceiling
      switch(direction)
      {
      case plat_down:
         // moving a ceiling down
         // jff 02/04/98 keep ceiling from moving thru floors
         // jff 2/22/98 weaken check to demo_compatibility
         destheight = (comp[comp_floors] || dest>sector->floorheight)?
                       dest : sector->floorheight; // killough 10/98: add comp flag
         if(sector->ceilingheight - speed < destheight)
         {
            lastpos = sector->ceilingheight;

            // SoM 9/19/02: If we are go, move 3d sides first.
            if(move3dsides)
            {
               flag = P_Scroll3DSides(sector, true, lastpos-destheight, crush);
               if(!flag)
               {
                  P_Scroll3DSides(sector, true, destheight-lastpos, crush);
                  return crushed;
               }
            }
            if(moveattached)
            {
               if(!P_MoveAttached(sector, true, destheight-lastpos, crush))
               {
                  P_MoveAttached(sector, true, lastpos-destheight, crush);
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
                  P_MoveAttached(sector, true, lastpos-destheight, crush);
            }
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
               if(!P_MoveAttached(sector, true, -speed, crush))
               {
                  P_MoveAttached(sector, true, speed, crush);
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
               if(crush > 0)
                  return crushed;

               P_SetCeilingHeight(sector, lastpos);
               P_CheckSector(sector,crush,speed,1);      //jff 3/19/98 use faster chk
               
               if(move3dsides)
                  P_Scroll3DSides(sector, true, speed, crush);
               if(moveattached)
                  P_MoveAttached(sector, true, speed, crush);
               return crushed;
            }
         }
         break;
         
      case plat_up:
         // moving a ceiling up
         if(sector->ceilingheight + speed > dest)
         {
            lastpos = sector->ceilingheight;

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
               if(!P_MoveAttached(sector, true, dest - lastpos, crush))
               {
                  P_MoveAttached(sector, true, lastpos - dest, crush);
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
                  P_MoveAttached(sector, true, lastpos - dest, crush);
            }
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
               if(!P_MoveAttached(sector, true, speed, crush))
               {
                  P_MoveAttached(sector, true, -speed, crush);
                  return crushed;
               }
            }

            
            lastpos = sector->ceilingheight;
            P_SetCeilingHeight(sector, sector->ceilingheight + speed);
            P_CheckSector(sector,crush,speed,1); //jff 3/19/98 use faster chk
         }
         break;
      }
      break;
   }
   return ok;
}

//
// T_MoveFloor()
//
// Move a floor to it's destination (up or down). 
// Called once per tick for each moving floor.
//
// Passed a floormove_t structure that contains all pertinent info about the
// move. See P_SPEC.H for fields.
// No return value.
//
// jff 02/08/98 all cases with labels beginning with gen added to support 
// generalized line type behaviors.

void T_MoveFloor(floormove_t* floor)
{
   result_e      res;

   // haleyjd 10/13/05: resetting stairs
   if(floor->type == genBuildStair || floor->type == genWaitStair ||
      floor->type == genDelayStair)
   {
      if(floor->resetTime && !--floor->resetTime)
      {
         // reset timer has expired, so reverse direction, set
         // destination to original floorheight, and change type
         floor->direction = 
            (floor->direction == plat_up) ? plat_down : plat_up;
         floor->floordestheight = floor->resetHeight;
         floor->type = genResetStair;
         
         P_FloorSequence(floor->sector);
      }

      // While stair is building, the delay timer is counting down
      // the amount of time needed to move up one step; once it 
      // expires, we need to wait for the specified delay time.
      //
      // While the stair is delayed, the delay timer is counting
      // down the specified delay time; once it expires, we need to
      // again wait for the amount of time needed to move up one step.
      //
      // Stairs waiting to reset never do normal thinking

      switch(floor->type)
      {
      case genBuildStair:
         if(floor->delayTimer && !--floor->delayTimer)
         {
            floor->type = genDelayStair;
            floor->delayTimer = floor->delayTime;
            S_StopSectorSequence(floor->sector, false);
         }
         break;
      case genDelayStair:
         if(floor->delayTimer && !--floor->delayTimer)
         {
            floor->type = genBuildStair;
            floor->delayTimer = floor->stepRaiseTime;
            
            if(floor->sector->floorheight != floor->floordestheight)
               P_FloorSequence(floor->sector);
         }
         return;
      case genWaitStair:
         return;
      default:
         break;
      }
   }
   
   res = T_MovePlane       // move the floor
   (
     floor->sector,
     floor->speed,
     floor->floordestheight,
     floor->crush,
     0,
     floor->direction
   );

   // sf: added silentmove
   // haleyjd: moving sound handled by sound sequences now
    
   if(res == pastdest)    // if destination height is reached
   {
      S_StopSectorSequence(floor->sector, false);

      // haleyjd 10/13/05: stairs that wish to reset must wait
      // until their reset timer expires.
      if(floor->type == genBuildStair && floor->resetTime > 0)
      {
         floor->type = genWaitStair;
         return;
      }

      if(floor->direction == plat_up)       // going up
      {
         switch(floor->type) // handle texture/type changes
         {
         case donutRaise:
            P_TransferSectorSpecial(floor->sector, &(floor->special));
            floor->sector->floorpic = floor->texture;
            break;
         case genFloorChgT:
         case genFloorChg0:
            //jff add to fix bug in special transfers from changes
            P_TransferSectorSpecial(floor->sector, &(floor->special));
            //fall thru
         case genFloorChg:
            floor->sector->floorpic = floor->texture;
            break;
         default:
            break;
         }
      }
      else if (floor->direction == plat_down) // going down
      {
         switch(floor->type) // handle texture/type changes
         {
         case lowerAndChange:
            //jff add to fix bug in special transfers from changes
            P_TransferSectorSpecial(floor->sector, &(floor->special));
            floor->sector->floorpic = floor->texture;
            break;
         case genFloorChgT:
         case genFloorChg0:
            //jff add to fix bug in special transfers from changes
            P_TransferSectorSpecial(floor->sector, &(floor->special));
            //fall thru
         case genFloorChg:
            floor->sector->floorpic = floor->texture;
            break;
         default:
            break;
         }
      }
      
      floor->sector->floordata = NULL; //jff 2/22/98
      P_RemoveThinker(&floor->thinker);//remove this floor from list of movers

      //jff 2/26/98 implement stair retrigger lockout while still building
      // note this only applies to the retriggerable generalized stairs
      
      if(floor->sector->stairlock == -2) // if this sector is stairlocked
      {
         sector_t *sec = floor->sector;
         sec->stairlock = -1;             // thinker done, promote lock to -1

         while(sec->prevsec != -1 &&
               sectors[sec->prevsec].stairlock != -2)
            sec = &sectors[sec->prevsec]; // search for a non-done thinker
         if(sec->prevsec == -1)           // if all thinkers previous are done
         {
            sec = floor->sector;          // search forward
            while(sec->nextsec != -1 &&
                  sectors[sec->nextsec].stairlock != -2) 
               sec = &sectors[sec->nextsec];
            if(sec->nextsec == -1)        // if all thinkers ahead are done too
            {
               while(sec->prevsec != -1)  // clear all locks
               {
                  sec->stairlock = 0;
                  sec = &sectors[sec->prevsec];
               }
               sec->stairlock = 0;
            }
         }
      }

      // make floor stop sound
      // haleyjd: handled via sound sequences
   }
}

//
// T_MoveElevator()
//
// Move an elevator to it's destination (up or down)
// Called once per tick for each moving floor.
//
// Passed an elevator_t structure that contains all pertinent info about the
// move. See P_SPEC.H for fields.
// No return value.
//
// jff 02/22/98 added to support parallel floor/ceiling motion
//
void T_MoveElevator(elevator_t* elevator)
{
   result_e      res;
   
   if (elevator->direction<0)      // moving down
   {
      res = T_MovePlane    //jff 4/7/98 reverse order of ceiling/floor
      (
         elevator->sector,
         elevator->speed,
         elevator->ceilingdestheight,
         -1,
         1,                          // move floor
         elevator->direction
      );
      if(res==ok || res==pastdest) // jff 4/7/98 don't move ceil if blocked
         T_MovePlane
         (
            elevator->sector,
            elevator->speed,
            elevator->floordestheight,
            -1,
            0,                        // move ceiling
            elevator->direction
          );
   }
   else // up
   {
      res = T_MovePlane    //jff 4/7/98 reverse order of ceiling/floor
      (
         elevator->sector,
         elevator->speed,
         elevator->floordestheight,
         -1,
         0,                          // move ceiling
         elevator->direction
      );
      if(res==ok || res==pastdest) // jff 4/7/98 don't move floor if blocked
         T_MovePlane
         (
            elevator->sector,
            elevator->speed,
            elevator->ceilingdestheight,
            -1,
            1,                        // move floor
            elevator->direction
         );
   }

   // make floor move sound
   // haleyjd: handled through sound sequences
    
   if(res == pastdest)            // if destination height acheived
   {
      S_StopSectorSequence(elevator->sector, false);
      elevator->sector->floordata = NULL;     //jff 2/22/98
      elevator->sector->ceilingdata = NULL;   //jff 2/22/98
      P_RemoveThinker(&elevator->thinker);    // remove elevator from actives
      
      // make floor stop sound
      // haleyjd: handled through sound sequences
   }
}

// haleyjd 10/07/06: Pillars by Joe :)

//
// T_MovePillar
//
// Pillar thinker function
// joek 4/9/06
//
void T_MovePillar(pillar_t *pillar)
{
   boolean result;
   
   // Move floor
   result  = (T_MovePlane(pillar->sector, pillar->floorSpeed, 
                          pillar->floordest, pillar->crush, 0, 
                          pillar->direction) == pastdest);
   
   // Move ceiling
   result &= (T_MovePlane(pillar->sector, pillar->ceilingSpeed, 
                          pillar->ceilingdest, pillar->crush, 1, 
                          pillar->direction * -1) == pastdest);
   
   if(result)
   {
      S_StopSectorSequence(pillar->sector, false);
      pillar->sector->floordata = NULL;
      pillar->sector->ceilingdata = NULL;      
      // TODO: notify scripts
      //P_TagFinished(pillar->sector->tag);
      P_RemoveThinker(&pillar->thinker);
   }
}

///////////////////////////////////////////////////////////////////////
// 
// Floor motion linedef handlers
//
///////////////////////////////////////////////////////////////////////

//
// EV_DoFloor()
//
// Handle regular and extended floor types
//
// Passed the line that activated the floor and the type of floor motion
// Returns true if a thinker was created.
//
int EV_DoFloor(line_t *line, floor_e floortype )
{
   int           secnum;
   int           rtn;
   int           i;
   sector_t*     sec;
   floormove_t*  floor;

   secnum = -1;
   rtn = 0;
   // move all floors with the same tag as the linedef
   while((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
   {
      sec = &sectors[secnum];
      
      // Don't start a second thinker on the same floor
      if(P_SectorActive(floor_special,sec)) //jff 2/23/98
         continue;
      
      // new floor thinker
      rtn = 1;
      floor = Z_Calloc(1, sizeof(*floor), PU_LEVSPEC, 0);
      P_AddThinker (&floor->thinker);
      sec->floordata = floor; //jff 2/22/98
      floor->thinker.function = T_MoveFloor;
      floor->type = floortype;
      floor->crush = -1;

      // setup the thinker according to the linedef type
      switch(floortype)
      {
      case lowerFloor:
         floor->direction = plat_down;
         floor->sector = sec;
         floor->speed = FLOORSPEED;
         floor->floordestheight = P_FindHighestFloorSurrounding(sec);
         break;

         //jff 02/03/30 support lowering floor by 24 absolute
      case lowerFloor24:
         floor->direction = plat_down;
         floor->sector = sec;
         floor->speed = FLOORSPEED;
         floor->floordestheight = 
            floor->sector->floorheight + 24 * FRACUNIT;
         break;

         //jff 02/03/30 support lowering floor by 32 absolute (fast)
      case lowerFloor32Turbo:
         floor->direction = plat_down;
         floor->sector = sec;
         floor->speed = FLOORSPEED*4;
         floor->floordestheight =
            floor->sector->floorheight + 32 * FRACUNIT;
         break;

      case lowerFloorToLowest:
         floor->direction = plat_down;
         floor->sector = sec;
         floor->speed = FLOORSPEED;
         floor->floordestheight = P_FindLowestFloorSurrounding(sec);
         break;

         //jff 02/03/30 support lowering floor to next lowest floor
      case lowerFloorToNearest:
         floor->direction = plat_down;
         floor->sector = sec;
         floor->speed = FLOORSPEED;
         floor->floordestheight =
            P_FindNextLowestFloor(sec,floor->sector->floorheight);
         break;

      case turboLower:
         floor->direction = plat_down;
         floor->sector = sec;
         floor->speed = FLOORSPEED * 4;
         floor->floordestheight = P_FindHighestFloorSurrounding(sec);
         if (floor->floordestheight != sec->floorheight)
            floor->floordestheight += 8*FRACUNIT;
         break;

      case raiseFloorCrush:
         floor->crush = 10;
      case raiseFloor:
         floor->direction = plat_up;
         floor->sector = sec;
         floor->speed = FLOORSPEED;
         floor->floordestheight = P_FindLowestCeilingSurrounding(sec);
         if(floor->floordestheight > sec->ceilingheight)
            floor->floordestheight = sec->ceilingheight;
         floor->floordestheight -=
            (8*FRACUNIT)*(floortype == raiseFloorCrush);
         break;

      case raiseFloorTurbo:
         floor->direction = plat_up;
         floor->sector = sec;
         floor->speed = FLOORSPEED*4;
         floor->floordestheight =
            P_FindNextHighestFloor(sec,sec->floorheight);
         break;

      case raiseFloorToNearest:
         floor->direction = plat_up;
         floor->sector = sec;
         floor->speed = FLOORSPEED;
         floor->floordestheight =
            P_FindNextHighestFloor(sec,sec->floorheight);
         break;

      case raiseFloor24:
         floor->direction = plat_up;
         floor->sector = sec;
         floor->speed = FLOORSPEED;
         floor->floordestheight =
            floor->sector->floorheight + 24 * FRACUNIT;
         break;

         // jff 2/03/30 support straight raise by 32 (fast)
      case raiseFloor32Turbo:
         floor->direction = plat_up;
         floor->sector = sec;
         floor->speed = FLOORSPEED*4;
         floor->floordestheight =
            floor->sector->floorheight + 32 * FRACUNIT;
         break;

      case raiseFloor512:
         floor->direction = plat_up;
         floor->sector = sec;
         floor->speed = FLOORSPEED;
         floor->floordestheight =
            floor->sector->floorheight + 512 * FRACUNIT;
         break;

      case raiseFloor24AndChange:
         floor->direction = plat_up;
         floor->sector = sec;
         floor->speed = FLOORSPEED;
         floor->floordestheight =
            floor->sector->floorheight + 24 * FRACUNIT;
         sec->floorpic = line->frontsector->floorpic;
         //jff 3/14/98 transfer both old and new special
         P_DirectTransferSectorSpecial(line->frontsector, sec);
         break;

      case raiseToTexture:
         {
            int minsize = D_MAXINT;
            side_t*     side;
                      
            if(!comp[comp_model])  // killough 10/98
               minsize = 32000<<FRACBITS; //jff 3/13/98 no ovf
            floor->direction = plat_up;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
            for (i = 0; i < sec->linecount; i++)
            {
               if(twoSided (secnum, i) )
               {
                  side = getSide(secnum,i,0);
                  if(side->bottomtexture >= 0      //killough 10/98
                     && (side->bottomtexture || comp[comp_model]))
                  {
                     if(textures[side->bottomtexture]->heightfrac < minsize)
                        minsize = textures[side->bottomtexture]->heightfrac;
                  }
                  side = getSide(secnum,i,1);
                  if(side->bottomtexture >= 0      //killough 10/98
                     && (side->bottomtexture || comp[comp_model]))
                  {
                     if(textures[side->bottomtexture]->heightfrac < minsize)
                        minsize = textures[side->bottomtexture]->heightfrac;
                  }
               }
            }
            if(comp[comp_model])
            {
               floor->floordestheight =
                  floor->sector->floorheight + minsize;
            }
            else
            {
               floor->floordestheight =
                  (floor->sector->floorheight>>FRACBITS) + 
                  (minsize>>FRACBITS);
               if(floor->floordestheight>32000)
                  floor->floordestheight = 32000;   //jff 3/13/98 do not
               floor->floordestheight<<=FRACBITS;   // allow height overflow
            }
         }                             
         break;
        
      case lowerAndChange:
         floor->direction = plat_down;
         floor->sector = sec;
         floor->speed = FLOORSPEED;
         floor->floordestheight = P_FindLowestFloorSurrounding(sec);
         floor->texture = sec->floorpic;

         // jff 1/24/98 make sure floor->newspecial gets initialized
         // in case no surrounding sector is at floordestheight
         // --> should not affect compatibility <--
         //jff 3/14/98 transfer both old and new special
         P_SetupSpecialTransfer(sec, &(floor->special));

         //jff 5/23/98 use model subroutine to unify fixes and handling
         sec = P_FindModelFloorSector(floor->floordestheight,
                                      sec-sectors);
         if(sec)
         {
            floor->texture = sec->floorpic;
            //jff 3/14/98 transfer both old and new special
            P_SetupSpecialTransfer(sec, &(floor->special));
         }
         break;
      default:
         break;
      }

      P_FloorSequence(floor->sector);
   }
   return rtn;
}

//
// EV_DoChange()
//
// Handle pure change types. These change floor texture and sector type
// by trigger or numeric model without moving the floor.
//
// The linedef causing the change and the type of change is passed
// Returns true if any sector changes
//
// jff 3/15/98 added to better support generalized sector types
//
int EV_DoChange(line_t *line, change_e changetype)
{
   int                   secnum;
   int                   rtn;
   sector_t*             sec;
   sector_t*             secm;

   secnum = -1;
   rtn = 0;
   // change all sectors with the same tag as the linedef
   while((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
   {
      sec = &sectors[secnum];
      
      rtn = 1;

      // handle trigger or numeric change type
      switch(changetype)
      {
      case trigChangeOnly:
         sec->floorpic = line->frontsector->floorpic;
         P_DirectTransferSectorSpecial(line->frontsector, sec);
         break;
      case numChangeOnly:
         secm = P_FindModelFloorSector(sec->floorheight,secnum);
         if(secm) // if no model, no change
         {
            sec->floorpic = secm->floorpic;
            P_DirectTransferSectorSpecial(secm, sec);
         }
         break;
      default:
         break;
      }
   }
   return rtn;
}


//
// P_FindSectorFromLineTagWithLowerBound
//
// haleyjd 07/15/04: this is from prboom, and is needed to repair 
// the stair-building logic bugs introduced in our various ancestral 
// ports.
// BTW, I'm pretty sure this is the longest function name in the
// source code! ^_^
//
static int P_FindSectorFromLineTagWithLowerBound(line_t *l, int start,
                                                 int min)
{
   // Emulate original Doom's linear lower-bounded 
   // P_FindSectorFromLineTag as needed

   do
   {
      start = P_FindSectorFromLineTag(l, start);
   }
   while(start >= 0 && start <= min);

   return start;
}

//
// EV_BuildStairs()
//
// Handles staircase building. A sequence of sectors chosen by algorithm
// rise at a speed indicated to a height that increases by the stepsize
// each step.
//
// Passed the linedef triggering the stairs and the type of stair rise
// Returns true if any thinkers are created
//
// cph 2001/09/21 - compatibility nightmares again
// There are three different ways this function has, during its history, 
// stepped through all the stairs to be triggered by the single switch:
//
// * original Doom used a linear P_FindSectorFromLineTag, but failed to 
//   preserve the index of the previous sector found, so instead it 
//   would restart its linear search from the last sector of the 
//   previous staircase
//
// * MBF/PrBoom with comp_stairs fail to emulate this, because their
//   P_FindSectorFromLineTag is a chained hash table implementation. 
//   Instead they start following the hash chain from the last sector 
//   of the previous staircase, which will (probably) have the wrong tag,
//   so they miss any further stairs
//
// * Boom fixed the bug, and MBF/PrBoom without comp_stairs work right
//
int EV_BuildStairs(line_t *line, stair_e type)
{
   // cph 2001/09/22 - cleaned up this function to save my sanity. 
   // A separate outer loop index makes the logic much cleared, and 
   // local variables moved into the inner blocks helps too
   int                   ssec = -1;
   int                   minssec = -1;
   int                   rtn = 0;

   // start a stair at each sector tagged the same as the linedef
   while((ssec = P_FindSectorFromLineTagWithLowerBound(line,ssec,minssec)) >= 0)
   {
      int           secnum = ssec;
      sector_t*     sec = &sectors[secnum];
      
      // don't start a stair if the first step's floor is already moving
      if(!P_SectorActive(floor_special,sec)) //jff 2/22/98
      { 
         floormove_t*  floor;
         int           texture, height;
         fixed_t       stairsize;
         fixed_t       speed;
         int           ok;

         // create new floor thinker for first step
         rtn = 1;
         floor = Z_Calloc(1, sizeof(*floor), PU_LEVSPEC, 0);
         P_AddThinker(&floor->thinker);
         sec->floordata = floor;
         floor->thinker.function = T_MoveFloor;
         floor->direction = 1;
         floor->sector = sec;
         floor->type = buildStair;   //jff 3/31/98 do not leave uninited

         // set up the speed and stepsize according to the stairs type
         switch(type)
         {
         default: // killough -- prevent compiler warning
         case build8:
            speed = FLOORSPEED/4;
            stairsize = 8*FRACUNIT;
            if(!demo_compatibility)
               floor->crush = -1; //jff 2/27/98 fix uninitialized crush field
            break;
         case turbo16:
            speed = FLOORSPEED*4;
            stairsize = 16*FRACUNIT;
            if(!demo_compatibility)
               floor->crush = 10;  //jff 2/27/98 fix uninitialized crush field
            break;
         }

         floor->speed = speed;
         height = sec->floorheight + stairsize;
         floor->floordestheight = height;
         
         texture = sec->floorpic;

         P_FloorSequence(floor->sector);
         
         // Find next sector to raise
         //   1. Find 2-sided line with same sector side[0] (lowest numbered)
         //   2. Other side is the next sector to raise
         //   3. Unless already moving, or different texture, then stop building
         do
         {
            int i;
            ok = 0;
            
            for(i = 0; i < sec->linecount; ++i)
            {
               sector_t* tsec = (sec->lines[i])->frontsector;
               int newsecnum;
               if(!((sec->lines[i])->flags & ML_TWOSIDED))
                  continue;

               newsecnum = tsec-sectors;
               
               if(secnum != newsecnum)
                  continue;
               
               tsec = (sec->lines[i])->backsector;
               if(!tsec) continue;     //jff 5/7/98 if no backside, continue
               newsecnum = tsec - sectors;

               // if sector's floor is different texture, look for another
               if(tsec->floorpic != texture)
                  continue;

                 /* jff 6/19/98 prevent double stepsize
                  * killough 10/98: intentionally left this way [MBF comment]
                  * cph 2001/02/06: stair bug fix should be controlled by comp_stairs,
                  *  except if we're emulating MBF which perversly reverted the fix
                  */
               if(comp[comp_stairs] || demo_version == 203)
                  height += stairsize; // jff 6/28/98 change demo compatibility

               // if sector's floor already moving, look for another
               if(P_SectorActive(floor_special,tsec)) //jff 2/22/98
                  continue;

               /* cph - see comment above - do this iff we didn't do so above */
               if(!comp[comp_stairs] && demo_version != 203)
                  height += stairsize;

               sec = tsec;
               secnum = newsecnum;

               // create and initialize a thinker for the next step
               floor = Z_Calloc(1, sizeof(*floor), PU_LEVSPEC, 0);
               P_AddThinker(&floor->thinker);

               sec->floordata = floor; //jff 2/22/98
               floor->thinker.function = T_MoveFloor;
               floor->direction = 1;
               floor->sector = sec;
               floor->speed = speed;
               floor->floordestheight = height;
               floor->type = buildStair; //jff 3/31/98 do not leave uninited
               //jff 2/27/98 fix uninitialized crush field
               if(!demo_compatibility)
                  floor->crush = (type == build8 ? -1 : 10);
               P_FloorSequence(floor->sector);
               ok = 1;
               break;
            } // end for

         } while(ok);      // continue until no next step is found

      } // end if(!P_SectorActive())

      /* killough 10/98: compatibility option */
      if(comp[comp_stairs])
      {
         // cph 2001/09/22 - emulate buggy MBF comp_stairs for demos, 
         // with logic reversed since we now have a separate outer loop 
         // index.
         // DEMOSYNC - what about boom_compatibility_compatibility?
         if(demo_version >= 203 && demo_version < 331) 
            ssec = secnum; /* Trash outer loop index */
         else
         {
            // cph 2001/09/22 - now the correct comp_stairs - Doom used 
            // a linear search from the last secnum, so we set that as a 
            // minimum value and do a fresh tag search
            ssec = -1; 
            minssec = secnum;
         }
      } // end if
   } // end for

   return rtn;
}

boolean donut_emulation;

//
// DonutOverflow
//
// haleyjd 10/16/09: enables emulation of undefined behavior by donut actions
// when no proper reference sector is found outside the pool (ie, line is 
// marked as two-sided but is really one-sided).
//
// Thanks to entryway for discovering and coding a fix to this.
//
static boolean DonutOverflow(fixed_t *pfloorheight, int16_t *pfloorpic)
{
   static boolean firsttime = true;
   static boolean donutparm = false;
   static int floorpic      = 0x16;
   static int floorheight   = 0;

   if(firsttime)
   {
      int p;

      if((p = M_CheckParm("-donut")) && p < myargc - 2)
      {
         floorheight = (int)strtol(myargv[p + 1], NULL, 0);
         floorpic    = (int)strtol(myargv[p + 2], NULL, 0);

         // bounds-check floorpic
         if(floorpic <= numwalls || floorpic >= texturecount)
            floorpic = numwalls + 0x16;

         donutparm = true;
      }

      firsttime = false;
   }
   else
      floorpic = numwalls + 0x16;

   // if -donut used, always emulate
   if(!donutparm)
   {
      // otherwise, only in demos and only when so requested
      if(!(demo_compatibility && donut_emulation))
         return false;
   }

   *pfloorheight = (fixed_t)floorheight;
   *pfloorpic    = (int16_t)floorpic;

   return true;
}

//
// EV_DoDonut()
//
// Handle donut function: lower pillar, raise surrounding pool, both to height,
// texture and type of the sector surrounding the pool.
//
// Passed the linedef that triggered the donut
// Returns whether a thinker was created
//
int EV_DoDonut(line_t *line)
{
   sector_t    *s1, *s2, *s3;
   int          secnum;
   int          rtn;
   int          i;
   floormove_t *floor;
   fixed_t      s3_floorheight;
   int16_t      s3_floorpic;

   secnum = -1;
   rtn = 0;
   // do function on all sectors with same tag as linedef
   while((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
   {
      s1 = &sectors[secnum];                // s1 is pillar's sector
              
      // do not start the donut if the pillar is already moving
      if(P_SectorActive(floor_special, s1)) //jff 2/22/98
         continue;
                      
      s2 = getNextSector(s1->lines[0],s1); // s2 is pool's sector
      if(!s2) continue;           // note lowest numbered line around
                                  // pillar must be two-sided 

      // do not start the donut if the pool is already moving
      if(!comp[comp_floors] && P_SectorActive(floor_special, s2))
         continue;                           //jff 5/7/98
                      
      // find a two sided line around the pool whose other side isn't the pillar
      for (i = 0;i < s2->linecount;i++)
      {
         //jff 3/29/98 use true two-sidedness, not the flag
         if(comp[comp_model])
         {
            if((!s2->lines[i]->flags & ML_TWOSIDED) ||
               (s2->lines[i]->backsector == s1))
               continue;
         }
         else if(!s2->lines[i]->backsector ||
                  s2->lines[i]->backsector == s1)
         {
            continue;
         }

         //jff 1/26/98 no donut action - no switch change on return
         rtn = 1;
         
         // s3 is model sector for changes
         s3 = s2->lines[i]->backsector;

         // haleyjd: donut "overflow" emulation courtesy of entryway,
         // as opposed to just committing an access violation.
         if(!s3)
         {
            if(!DonutOverflow(&s3_floorheight, &s3_floorpic))
               break;
         }
         else
         {
            s3_floorheight = s3->floorheight;
            s3_floorpic    = s3->floorpic;
         }
        
         //  Spawn rising slime
         floor = Z_Calloc(1, sizeof(*floor), PU_LEVSPEC, 0);
         P_AddThinker (&floor->thinker);
         s2->floordata = floor; //jff 2/22/98
         floor->thinker.function = T_MoveFloor;
         floor->type = donutRaise;
         floor->crush = -1;
         floor->direction = plat_up;
         floor->sector = s2;
         floor->speed = FLOORSPEED / 2;
         floor->texture = s3_floorpic;
         P_ZeroSpecialTransfer(&(floor->special));
         floor->floordestheight = s3_floorheight;
         P_FloorSequence(floor->sector);
        
         //  Spawn lowering donut-hole pillar
         floor = Z_Calloc(1, sizeof(*floor), PU_LEVSPEC, 0);
         P_AddThinker (&floor->thinker);
         s1->floordata = floor; //jff 2/22/98
         floor->thinker.function = T_MoveFloor;
         floor->type = lowerFloor;
         floor->crush = -1;
         floor->direction = plat_down;
         floor->sector = s1;
         floor->speed = FLOORSPEED / 2;
         floor->floordestheight = s3_floorheight;
         P_FloorSequence(floor->sector);
         break;
      }
   }
   return rtn;
}

//
// EV_DoElevator
//
// Handle elevator linedef types
//
// Passed the linedef that triggered the elevator and the elevator action
//
// jff 2/22/98 new type to move floor and ceiling in parallel
//
int EV_DoElevator
( line_t*       line,
  elevator_e    elevtype )
{
   int                   secnum;
   int                   rtn;
   sector_t*             sec;
   elevator_t*           elevator;

   secnum = -1;
   rtn = 0;
   // act on all sectors with the same tag as the triggering linedef
   while((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
   {
      sec = &sectors[secnum];
              
      // If either floor or ceiling is already activated, skip it
      if(sec->floordata || sec->ceilingdata) //jff 2/22/98
         continue;
      
      // create and initialize new elevator thinker
      rtn = 1;
      elevator = Z_Calloc(1, sizeof(*elevator), PU_LEVSPEC, 0);
      P_AddThinker (&elevator->thinker);
      sec->floordata = elevator; //jff 2/22/98
      sec->ceilingdata = elevator; //jff 2/22/98
      elevator->thinker.function = T_MoveElevator;
      elevator->type = elevtype;

      // set up the fields according to the type of elevator action
      switch(elevtype)
      {
      // elevator down to next floor
      case elevateDown:
         elevator->direction = plat_down;
         elevator->sector = sec;
         elevator->speed = ELEVATORSPEED;
         elevator->floordestheight =
            P_FindNextLowestFloor(sec,sec->floorheight);
         elevator->ceilingdestheight =
            elevator->floordestheight + sec->ceilingheight - sec->floorheight;
         break;

      // elevator up to next floor
      case elevateUp:
         elevator->direction = plat_up;
         elevator->sector = sec;
         elevator->speed = ELEVATORSPEED;
         elevator->floordestheight =
            P_FindNextHighestFloor(sec,sec->floorheight);
         elevator->ceilingdestheight =
            elevator->floordestheight + sec->ceilingheight - sec->floorheight;
         break;

      // elevator to floor height of activating switch's front sector
      case elevateCurrent:
         elevator->sector = sec;
         elevator->speed = ELEVATORSPEED;
         elevator->floordestheight = line->frontsector->floorheight;
         elevator->ceilingdestheight =
            elevator->floordestheight + sec->ceilingheight - sec->floorheight;
         elevator->direction =
            elevator->floordestheight>sec->floorheight ? plat_up : plat_down;
         break;

      default:
         break;
      }
      P_FloorSequence(elevator->sector);
   }
   return rtn;
}

//
// EV_PillarBuild
//
// Pillar build init function
// joek 4/9/06
//
int EV_PillarBuild(line_t *line, pillardata_t *pd)
{
   pillar_t *pillar;
   sector_t *sector;
   int returnval = 0;
   int sectornum = -1;
   int destheight;
   boolean manual = false;

   // check if a manual trigger, if so do just the sector on the backside
   if(pd->tag == 0)
   {
      if(!line || !(sector = line->backsector))
         return returnval;
      sectornum = sector - sectors;
      manual = true;
      goto manual_pillar;
   }

   while((sectornum = P_FindSectorFromTag(pd->tag, sectornum)) >= 0)
   {
      sector = &sectors[sectornum];

manual_pillar:
      // already being buggered about with, 
      // or ceiling <= floor, therefore closed
      if(sector->floordata || sector->ceilingdata || 
         sector->ceilingheight <= sector->floorheight)
      {
         if(manual)
            return returnval;
         else
            continue;
      }
            
      pillar = Z_Calloc(1, sizeof(pillar_t), PU_LEVSPEC, 0);
      sector->floordata = pillar;
      sector->ceilingdata = pillar;
      P_AddThinker(&pillar->thinker);
      pillar->thinker.function = T_MovePillar;
      pillar->sector = sector;
      
      if(pd->height == 0) // height == 0 so we meet in the middle
      {
         destheight = sector->floorheight + 
                         ((sector->ceilingheight - sector->floorheight) / 2);
      }
      else // else we meet at floorheight + args[2]
         destheight = sector->floorheight + pd->height;

      if(pd->height == 0) // height is 0 so we meet halfway
      {
         pillar->ceilingSpeed = pd->speed;
         pillar->floorSpeed   = pd->speed;
      }      
      else if(destheight-sector->floorheight > sector->ceilingheight-destheight)
      {
         pillar->floorSpeed = pd->speed;
         pillar->ceilingSpeed = FixedDiv(sector->ceilingheight - destheight,
                                   FixedDiv(destheight - sector->floorheight,
                                            pillar->floorSpeed));
      }
      else
      {
         pillar->ceilingSpeed = pd->speed;
         pillar->floorSpeed = FixedDiv(destheight - sector->floorheight,
                                 FixedDiv(sector->ceilingheight - destheight, 
                                          pillar->ceilingSpeed));
      } 
      
      pillar->floordest = destheight;
      pillar->ceilingdest = destheight;
      pillar->direction = 1;
      pillar->crush = pd->crush;
      returnval = 1;
      
      P_FloorSequence(pillar->sector);

      if(manual)
         return returnval;
   }

   return returnval;
}

//
// EV_PillarOpen
//
// Pillar open init function
// joek 4/9/06
//
int EV_PillarOpen(line_t *line, pillardata_t *pd)
{
   pillar_t *pillar;
   sector_t *sector;
   int returnval = 0;
   int sectornum = -1;
   boolean manual = false;

   // check if a manual trigger, if so do just the sector on the backside
   if(pd->tag == 0)
   {
      if(!line || !(sector = line->backsector))
         return returnval;
      sectornum = sector - sectors;
      manual = true;
      goto manual_pillar;
   }

   while((sectornum = P_FindSectorFromTag(pd->tag, sectornum)) >= 0)
   {
      sector = &sectors[sectornum];

manual_pillar:
      // already being buggered about with
      if(sector->floordata || sector->ceilingdata ||
         sector->floorheight != sector->ceilingheight)
      {
         if(manual)
            return returnval;
         else
            continue;
      }

      pillar = Z_Calloc(1, sizeof(pillar_t), PU_LEVSPEC, 0);
      sector->floordata   = pillar;
      sector->ceilingdata = pillar;
      P_AddThinker(&pillar->thinker);
      pillar->thinker.function = T_MovePillar;
      pillar->sector = sector;
      
      if(pd->fdist == 0) // floordist == 0 so we find the next lowest floor
         pillar->floordest = P_FindLowestFloorSurrounding(sector);
      else               // else we meet at floorheight - args[2]
         pillar->floordest = sector->floorheight - pd->fdist;
      
      if(pd->cdist == 0) // ceilingdist = 0 so we find the next highest ceiling
         pillar->ceilingdest = P_FindHighestCeilingSurrounding(sector);
      else               // else we meet at ceilingheight + args[3]
         pillar->ceilingdest = sector->ceilingheight + pd->cdist;
       
      if(pillar->ceilingdest - sector->ceilingheight > 
            sector->floorheight - pillar->floordest)
      {
         pillar->ceilingSpeed = pd->speed;
         pillar->floorSpeed = FixedDiv(sector->floorheight - pillar->floordest, 
                                 FixedDiv(pillar->ceilingdest - sector->ceilingheight,
                                          pillar->ceilingSpeed));
      }
      else
      {
         pillar->floorSpeed = pd->speed;
         pillar->ceilingSpeed = FixedDiv(pillar->ceilingdest - sector->ceilingheight,
                                   FixedDiv(sector->floorheight - pillar->floordest, 
                                            pillar->floorSpeed));
      }

      pillar->direction = -1;
      returnval = 1;
      
      P_FloorSequence(pillar->sector);

      if(manual)
         return returnval;
   }

   return returnval;
}

void P_ChangeFloorTex(const char *name, int tag)
{
   int flatnum;
   int secnum = -1;

   flatnum = R_FindFlat(name);

   while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
      sectors[secnum].floorpic = flatnum;
}

//=============================================================================
//
// Waggle Floors
//
// haleyjd 06/30/09: joe was supposed to do these but he quit instead :P
//

#define WGLSTATE_EXPAND 1
#define WGLSTATE_STABLE 2
#define WGLSTATE_REDUCE 3

//
// T_FloorWaggle
//
// haleyjd: thinker for floor waggle action.
//
void T_FloorWaggle(floorwaggle_t *waggle)
{
   fixed_t destheight;
   fixed_t dist;
   extern fixed_t FloatBobOffsets[64];

   switch(waggle->state)
   {
   case WGLSTATE_EXPAND:
      if((waggle->scale += waggle->scaleDelta) >= waggle->targetScale)
      {
         waggle->scale = waggle->targetScale;
         waggle->state = WGLSTATE_STABLE;
      }
      break;
   case WGLSTATE_REDUCE:
      if((waggle->scale -= waggle->scaleDelta) <= 0)
      { 
         // Remove
         /*
         waggle->sector->floorheight = waggle->originalHeight;
         P_ChangeSector(waggle->sector, 8);
         */
         destheight = waggle->originalHeight;
         dist       = waggle->originalHeight - waggle->sector->floorheight;
         T_MovePlane(waggle->sector, abs(dist), destheight, 8, 0, 
                     destheight >= waggle->sector->floorheight ? plat_down : plat_up);

         waggle->sector->floordata = NULL;
         // HEXEN_TODO: P_TagFinished
         // P_TagFinished(waggle->sector->tag);
         P_RemoveThinker(&waggle->thinker);
         return;
      }
      break;
   case WGLSTATE_STABLE:
      if(waggle->ticker != -1)
      {
         if(!--waggle->ticker)
            waggle->state = WGLSTATE_REDUCE;
      }
      break;
   }

   waggle->accumulator += waggle->accDelta;
   
   destheight = 
      waggle->originalHeight + 
         FixedMul(FloatBobOffsets[(waggle->accumulator >> FRACBITS) & 63],
                  waggle->scale);
   dist = destheight - waggle->sector->floorheight;

   T_MovePlane(waggle->sector, abs(dist), destheight, 8, 0, 
               destheight >= waggle->sector->floorheight ? plat_up : plat_down);
      
}

//
// EV_StartFloorWaggle
//
int EV_StartFloorWaggle(line_t *line, int tag, int height, int speed, 
                        int offset, int timer)
{
   int           sectorIndex = -1;
   int           retCode = 0;
   boolean       manual = false;
   sector_t      *sector;
   floorwaggle_t *waggle;

   if(tag == 0)
   {
      if(!line || !(sector = line->backsector))
         return retCode;
      sectorIndex = sector - sectors;
      manual = true;
      goto manual_waggle;
   }
   
   while((sectorIndex = P_FindSectorFromTag(tag, sectorIndex)) >= 0)
   {
      sector = &sectors[sectorIndex];

manual_waggle:
      // Already busy with another thinker
      if(sector->floordata)
      {
         if(manual)
            return retCode;
         else
            continue;
      }

      retCode = 1;
      waggle = Z_Calloc(1, sizeof(*waggle), PU_LEVSPEC, 0);
      sector->floordata = waggle;      
      waggle->thinker.function = T_FloorWaggle;
      P_AddThinker(&waggle->thinker);

      waggle->sector         = sector;
      waggle->originalHeight = sector->floorheight;
      waggle->accumulator    = offset * FRACUNIT;
      waggle->accDelta       = speed << 10;
      waggle->scale          = 0;
      waggle->targetScale    = height << 10;
      waggle->scaleDelta     = waggle->targetScale / (35+((3*35)*height)/255);
      waggle->ticker         = timer ? timer * 35 : -1;
      waggle->state          = WGLSTATE_EXPAND;

      if(manual)
         return retCode;
   }

   return retCode;
}

//----------------------------------------------------------------------------
//
// $Log: p_floor.c,v $
// Revision 1.23  1998/05/23  10:23:16  jim
// Fix numeric changer loop corruption
//
// Revision 1.22  1998/05/07  17:01:25  jim
// documented/formatted p_floor
//
// Revision 1.21  1998/05/04  02:21:58  jim
// formatted p_specs, moved a coupla routines to p_floor
//
// Revision 1.20  1998/05/03  23:08:04  killough
// Fix #includes at the top, nothing else
//
// Revision 1.19  1998/04/07  11:55:08  jim
// fixed elevators to block properly
//
// Revision 1.18  1998/03/31  16:52:03  jim
// Fixed uninited type field in stair builders
//
// Revision 1.17  1998/03/20  02:10:30  jim
// Improved crusher code with new mobj data structures
//
// Revision 1.16  1998/03/15  14:40:20  jim
// added pure texture change linedefs & generalized sector types
//
// Revision 1.15  1998/03/13  14:06:03  jim
// Fixed arith overflow in some linedef types
//
// Revision 1.14  1998/03/04  11:56:25  jim
// Fix multiple sector stair raise
//
// Revision 1.13  1998/02/27  11:50:54  jim
// Fixes for stairs
//
// Revision 1.12  1998/02/23  23:46:45  jim
// Compatibility flagged multiple thinker support
//
// Revision 1.11  1998/02/23  00:41:41  jim
// Implemented elevators
//
// Revision 1.10  1998/02/13  03:28:31  jim
// Fixed W1,G1 linedefs clearing untriggered special, cosmetic changes
//
// Revision 1.9  1998/02/08  05:35:28  jim
// Added generalized linedef types
//
// Revision 1.6  1998/02/02  13:42:11  killough
// Program beautification
//
// Revision 1.5  1998/01/30  14:44:16  jim
// Added gun exits, right scrolling walls and ceiling mover specials
//
// Revision 1.3  1998/01/26  19:24:03  phares
// First rev with no ^Ms
//
// Revision 1.2  1998/01/25  20:24:42  jim
// Fixed crusher floor, lowerandChange floor types, and unknown sector special error
//
// Revision 1.1.1.1  1998/01/19  14:02:59  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------

