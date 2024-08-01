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
//  General plane mover and floor mover action routines
//  Floor motion, pure changer types, raising stairs. donuts, elevators
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "c_io.h"
#include "doomstat.h"
#include "m_argv.h"
#include "m_compare.h"
#include "p_info.h"
#include "p_map.h"
#include "p_portal.h"
#include "p_saveg.h"
#include "p_saveid.h"
#include "p_slopes.h"
#include "p_spec.h"
#include "p_tick.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "sounds.h"
#include "r_data.h"
#include "r_main.h"
#include "r_sky.h"
#include "r_state.h"
#include "t_plane.h"

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
      S_StartSectorSequenceName(s, "EEFloor", SEQ_ORIGIN_SECTOR_F);
}

//
// Some games have a dedicated stair sequence
//
void P_StairSequence(sector_t *s)
{
   if(silentmove(s))
      return;

   if(s->sndSeqID >= 0)
      S_StartSectorSequence(s, SEQ_FLOOR);
   else if(E_SequenceForName("EEFloorStair"))
      S_StartSectorSequenceName(s, "EEFloorStair", SEQ_ORIGIN_SECTOR_F);
   else
      S_StartSectorSequenceName(s, "EEFloor", SEQ_ORIGIN_SECTOR_F);
}


IMPLEMENT_THINKER_TYPE(FloorMoveThinker)

//
// T_MoveFloor()
//
// Move a floor to it's destination (up or down). 
// Called once per tick for each moving floor.
//
// Passed a FloorMoveThinker structure that contains all pertinent info about the
// move. See P_SPEC.H for fields.
// No return value.
//
// jff 02/08/98 all cases with labels beginning with gen added to support 
// generalized line type behaviors.
//
void FloorMoveThinker::Think()
{
   result_e      res;

   // haleyjd 10/13/05: resetting stairs
   if(type == genBuildStair || type == genWaitStair || type == genDelayStair)
   {
      if(resetTime && !--resetTime)
      {
         // reset timer has expired, so reverse direction, set
         // destination to original floorheight, and change type
         direction = (direction == plat_up) ? plat_down : plat_up;
         floordestheight = resetHeight;
         type = genResetStair;
         
         P_StairSequence(sector);
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

      switch(type)
      {
      case genBuildStair:
         if(delayTimer && !--delayTimer)
         {
            type = genDelayStair;
            delayTimer = delayTime;
            S_StopSectorSequence(sector, SEQ_ORIGIN_SECTOR_F);
         }
         break;
      case genDelayStair:
         if(delayTimer && !--delayTimer)
         {
            type = genBuildStair;
            delayTimer = stepRaiseTime;

            if(sector->srf.floor.height != floordestheight)
               P_StairSequence(sector);
         }
         return;
      case genWaitStair:
         return;
      default:
         break;
      }
   }

   // move the floor
   res = T_MoveFloorInDirection(sector, speed, floordestheight, crush, direction,
                                emulateStairCrush);

   // sf: added silentmove
   // haleyjd: moving sound handled by sound sequences now

   if(res == pastdest)    // if destination height is reached
   {
      S_StopSectorSequence(sector, SEQ_ORIGIN_SECTOR_F);

      // haleyjd 10/13/05: stairs that wish to reset must wait
      // until their reset timer expires.
      if(type == genBuildStair && resetTime > 0)
      {
         type = genWaitStair;
         return;
      }

      if(direction == plat_up)       // going up
      {
         switch(type) // handle texture/type changes
         {
         case donutRaise:
            P_TransferSectorSpecial(sector, &special);
            R_CacheTexture(texture);
            R_CacheIfSkyTexture(sector->srf.floor.pic, texture);
            sector->srf.floor.pic = texture;
            break;
         case genFloorChgT:
         case genFloorChg0:
            //jff add to fix bug in special transfers from changes
            P_TransferSectorSpecial(sector, &special);
            //fall thru
         case genFloorChg:
            R_CacheTexture(texture);
            R_CacheIfSkyTexture(sector->srf.floor.pic, texture);
            sector->srf.floor.pic = texture;
            break;
         default:
            break;
         }
      }
      else if (direction == plat_down) // going down
      {
         switch(type) // handle texture/type changes
         {
         case lowerAndChange:
            //jff add to fix bug in special transfers from changes
            P_TransferSectorSpecial(sector, &special);
            R_CacheTexture(texture);
            R_CacheIfSkyTexture(sector->srf.floor.pic, texture);
            sector->srf.floor.pic = texture;
            break;
         case genFloorChgT:
         case genFloorChg0:
            //jff add to fix bug in special transfers from changes
            P_TransferSectorSpecial(sector, &special);
            //fall thru
         case genFloorChg:
            R_CacheTexture(texture);
            R_CacheIfSkyTexture(sector->srf.floor.pic, texture);
            sector->srf.floor.pic = texture;
            break;
         default:
            break;
         }
      }
      
      sector->srf.floor.data = nullptr; //jff 2/22/98
      this->remove(); //remove this floor from list of movers

      //jff 2/26/98 implement stair retrigger lockout while still building
      // note this only applies to the retriggerable generalized stairs
      
      if(sector->stairlock == -2) // if this sector is stairlocked
      {
         sector_t *sec = sector;
         sec->stairlock = -1;             // thinker done, promote lock to -1

         while(sec->prevsec != -1 &&
               sectors[sec->prevsec].stairlock != -2)
            sec = &sectors[sec->prevsec]; // search for a non-done thinker
         if(sec->prevsec == -1)           // if all thinkers previous are done
         {
            sec = sector;          // search forward
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
// FloorMoveThinker::serialize
//
// Saves/loads FloorMoveThinker thinkers.
//
void FloorMoveThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << type << crush << direction << special;
   Archive_Flat(arc, texture);
   arc << floordestheight << speed << resetTime << resetHeight
       << stepRaiseTime << delayTime << delayTimer;
}

//
// FloorMoveThinker::reTriggerVerticalDoor
//
// haleyjd 10/13/2011: emulate vanilla behavior when a FloorMoveThinker is
// treated as a VerticalDoorThinker
//
bool FloorMoveThinker::reTriggerVerticalDoor(bool player)
{
   // FIXME/TODO: may not have same effects as in vanilla due to special
   // transfer semantics... verify and fix if so
   // (need a spectransfer-for-special-number function)

   if(!demo_compatibility)
      return false;

   if(special.newspecial == plat_down)
      special.newspecial = plat_up;
   else
   {
      if(!player)
         return false;

      special.newspecial = plat_down;
   }

   return true;
}


IMPLEMENT_THINKER_TYPE(ElevatorThinker)

//
// T_MoveElevator()
//
// Move an elevator to it's destination (up or down)
// Called once per tick for each moving floor.
//
// Passed an ElevatorThinker structure that contains all pertinent info about the
// move. See P_SPEC.H for fields.
// No return value.
//
// jff 02/22/98 added to support parallel floor/ceiling motion
//
void ElevatorThinker::Think()
{
   result_e      res;
   
   if(direction < 0)      // moving down
   {
      //jff 4/7/98 reverse order of ceiling/floor
      res = T_MoveCeilingDown(sector, speed, ceilingdestheight, -1);

      if(res == ok || res == pastdest) // jff 4/7/98 don't move floor if blocked
         T_MoveFloorDown(sector, speed, floordestheight, -1);
   }
   else // up
   {
      //jff 4/7/98 reverse order of ceiling/floor
      res = T_MoveFloorUp(sector, speed, floordestheight, -1, false);

      if(res == ok || res == pastdest) // jff 4/7/98 don't move ceiling if blocked
         T_MoveCeilingUp(sector, speed, ceilingdestheight, -1); 
   }

   // make floor move sound
   // haleyjd: handled through sound sequences
    
   if(res == pastdest)            // if destination height acheived
   {
      S_StopSectorSequence(sector, SEQ_ORIGIN_SECTOR_F);
      sector->srf.floor.data = nullptr;     //jff 2/22/98
      sector->srf.ceiling.data = nullptr;   //jff 2/22/98
      this->remove();               // remove elevator from actives
      
      // make floor stop sound
      // haleyjd: handled through sound sequences
   }
}

//
// ElevatorThinker::serialize
//
// Saves/loads a ElevatorThinker thinker
//
void ElevatorThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << type << direction << floordestheight << ceilingdestheight 
       << speed;
}

// haleyjd 10/07/06: Pillars by Joe :)

IMPLEMENT_THINKER_TYPE(PillarThinker)

//
// T_MovePillar
//
// Pillar thinker function
// joek 4/9/06
//
void PillarThinker::Think()
{
   result_e resf, resc;
   
   resf = T_MoveFloorInDirection  (sector, floorSpeed,   floordest,   crush,  direction, false);
   resc = T_MoveCeilingInDirection(sector, ceilingSpeed, ceilingdest, crush, -direction);
   
   if(resf == pastdest && resc == pastdest)
   {
      S_StopSectorSequence(sector, SEQ_ORIGIN_SECTOR_F);
      sector->srf.floor.data = nullptr;
      sector->srf.ceiling.data = nullptr;
      this->remove();
   }
}

//
// PillarThinker::serialize
//
// Saves/loads a PillarThinker thinker.
//
void PillarThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << ceilingSpeed << floorSpeed << floordest 
       << ceilingdest << direction << crush;
}

///////////////////////////////////////////////////////////////////////
// 
// Floor motion linedef handlers
//
///////////////////////////////////////////////////////////////////////

//
// Just for Hexen compatibility
//
int EV_FloorCrushStop(const line_t *line, int tag)
{
   // This will just stop all crushing floors with given tag
   int rtn = 0;
   for(Thinker *th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      auto fmt = thinker_cast<FloorMoveThinker *>(th);
      if(!fmt || fmt->crush <= 0)
      {
         continue;
      }
      // Odd: in Hexen this affects ALL sectors
      if(P_LevelIsVanillaHexen() || fmt->sector->tag == tag)
      {
         rtn = 1;
         fmt->sector->srf.floor.data = nullptr;
         S_StopSectorSequence(fmt->sector, SEQ_ORIGIN_SECTOR_F);
         fmt->remove();
      }
   }
   return rtn;
}

//
// EV_DoFloor()
//
// Handle regular and extended floor types
//
// Passed the line that activated the floor and the type of floor motion
// Returns true if a thinker was created.
//
int EV_DoFloor(const line_t *line, int tag, floor_e floortype )
{
   int           secnum;
   int           rtn;
   int           i;
   sector_t*     sec;
   FloorMoveThinker*  floor;

   secnum = -1;
   rtn = 0;
   // move all floors with the same tag as the linedef
   while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
   {
      sec = &sectors[secnum];
      
      // Don't start a second thinker on the same floor
      if(P_SectorActive(floor_special,sec)) //jff 2/23/98
         continue;
      
      // new floor thinker
      rtn = 1;
      floor = new FloorMoveThinker;
      floor->addThinker();
      sec->srf.floor.data = floor; //jff 2/22/98
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
            floor->sector->srf.floor.height + 24 * FRACUNIT;
         break;

         //jff 02/03/30 support lowering floor by 32 absolute (fast)
      case lowerFloor32Turbo:
         floor->direction = plat_down;
         floor->sector = sec;
         floor->speed = FLOORSPEED*4;
         floor->floordestheight =
            floor->sector->srf.floor.height + 32 * FRACUNIT;
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
            P_FindNextLowestFloor(sec,floor->sector->srf.floor.height);
         break;

      case turboLower:
         floor->direction = plat_down;
         floor->sector = sec;
         floor->speed = FLOORSPEED * 4;
         floor->floordestheight = P_FindHighestFloorSurrounding(sec);
         if(floor->floordestheight != sec->srf.floor.height)
            floor->floordestheight += 8*FRACUNIT;
         break;

      case turboLowerA:
         // haleyjd 02/09/13: Heretic has different behavior for turbo lower
         // floors. Thanks to Gez for pointing this out.
         floor->direction = plat_down;
         floor->sector    = sec;
         floor->speed     = FLOORSPEED * 4;
         floor->floordestheight = 
            P_FindHighestFloorSurrounding(sec) + 8 * FRACUNIT;
         break;

      case raiseFloorCrush:
         floor->crush = 10;
      case raiseFloor:
         floor->direction = plat_up;
         floor->sector = sec;
         floor->speed = FLOORSPEED;
         floor->floordestheight = P_FindLowestCeilingSurrounding(sec);
         if(floor->floordestheight > sec->srf.ceiling.height)
            floor->floordestheight = sec->srf.ceiling.height;
         floor->floordestheight -=
            (8*FRACUNIT)*(floortype == raiseFloorCrush);
         break;

      case raiseFloorTurbo:
         floor->direction = plat_up;
         floor->sector = sec;
         floor->speed = FLOORSPEED*4;
         floor->floordestheight =
            P_FindNextHighestFloor(sec,sec->srf.floor.height);
         break;

      case raiseFloorToNearest:
         floor->direction = plat_up;
         floor->sector = sec;
         floor->speed = FLOORSPEED;
         floor->floordestheight =
            P_FindNextHighestFloor(sec,sec->srf.floor.height);
         break;

      case raiseFloor24:
         floor->direction = plat_up;
         floor->sector = sec;
         floor->speed = FLOORSPEED;
         floor->floordestheight =
            floor->sector->srf.floor.height + 24 * FRACUNIT;
         break;

         // jff 2/03/30 support straight raise by 32 (fast)
      case raiseFloor32Turbo:
         floor->direction = plat_up;
         floor->sector = sec;
         floor->speed = FLOORSPEED*4;
         floor->floordestheight =
            floor->sector->srf.floor.height + 32 * FRACUNIT;
         break;

      case raiseFloor512:
         floor->direction = plat_up;
         floor->sector = sec;
         floor->speed = FLOORSPEED;
         floor->floordestheight =
            floor->sector->srf.floor.height + 512 * FRACUNIT;
         break;

      case raiseFloor24AndChange:
         floor->direction = plat_up;
         floor->sector = sec;
         floor->speed = FLOORSPEED;
         floor->floordestheight =
            floor->sector->srf.floor.height + 24 * FRACUNIT;
         if(line)
         {
            R_CacheIfSkyTexture(sec->srf.floor.pic, line->frontsector->srf.floor.pic);
            sec->srf.floor.pic = line->frontsector->srf.floor.pic;
            //jff 3/14/98 transfer both old and new special
            P_DirectTransferSectorSpecial(line->frontsector, sec);
         }
         break;

      case raiseToTexture:
         {
            int minsize = D_MAXINT;
            side_t*     side;
                      
            if(!getComp(comp_model))  // killough 10/98
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
                     && (side->bottomtexture || getComp(comp_model)))
                  {
                     if(textures[side->bottomtexture]->heightfrac < minsize)
                        minsize = textures[side->bottomtexture]->heightfrac;
                  }
                  side = getSide(secnum,i,1);
                  if(side->bottomtexture >= 0      //killough 10/98
                     && (side->bottomtexture || getComp(comp_model)))
                  {
                     if(textures[side->bottomtexture]->heightfrac < minsize)
                        minsize = textures[side->bottomtexture]->heightfrac;
                  }
               }
            }
            if(getComp(comp_model))
            {
               floor->floordestheight =
                  floor->sector->srf.floor.height + minsize;
            }
            else
            {
               floor->floordestheight =
                  (floor->sector->srf.floor.height >>FRACBITS) +
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
         floor->texture = sec->srf.floor.pic;

         // jff 1/24/98 make sure floor->newspecial gets initialized
         // in case no surrounding sector is at floordestheight
         // --> should not affect compatibility <--
         //jff 3/14/98 transfer both old and new special
         P_SetupSpecialTransfer(sec, &(floor->special));

         //jff 5/23/98 use model subroutine to unify fixes and handling
         sec = P_FindModelFloorSector(floor->floordestheight,
                                      eindex(sec-sectors));
         if(sec)
         {
            floor->texture = sec->srf.floor.pic;
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
// Floor and ceiling mover. Mainly for Hexen compatibility
//
int EV_DoFloorAndCeiling(const line_t *line, int tag, const floordata_t &fd,
                         const ceilingdata_t &cd)
{
   int floor = EV_DoParamFloor(line, tag, &fd);
   if(P_LevelIsVanillaHexen())
   {
      // Clear ceilingdata to emulate Hexen bug
      int secnum = -1;
      while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
      {
         sectors[secnum].srf.ceiling.data = nullptr;
      }
   }
   int ceiling = EV_DoParamCeiling(line, tag, &cd);
   return floor || ceiling ? 1 : 0;
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
int EV_DoChange(const line_t *line, int tag, change_e changetype, bool isParam)
{
   int                   secnum;
   int                   rtn;
   sector_t*             sec;
   sector_t*             secm;

   if(changetype == trigChangeOnly && !line)
      return 0;   // ioanch: sanity check

   rtn = 0;
   bool manual = false;
   if(isParam && !tag)
   {
      if(!line || !(sec = line->backsector))
         return rtn;
      manual = true;
      secnum = static_cast<int>(sec - sectors);
      goto manualChange;
   }

   secnum = -1;
   // change all sectors with the same tag as the linedef
   while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
   {
      sec = &sectors[secnum];
   manualChange:
      
      rtn = 1;

      // handle trigger or numeric change type
      switch(changetype)
      {
      case trigChangeOnly:
         R_CacheIfSkyTexture(sec->srf.floor.pic, line->frontsector->srf.floor.pic);
         sec->srf.floor.pic = line->frontsector->srf.floor.pic;
         P_DirectTransferSectorSpecial(line->frontsector, sec);
         break;
      case numChangeOnly:
         secm = P_FindModelFloorSector(sec->srf.floor.height,secnum);
         if(secm) // if no model, no change
         {
            R_CacheIfSkyTexture(sec->srf.floor.pic, secm->srf.floor.pic);
            sec->srf.floor.pic = secm->srf.floor.pic;
            P_DirectTransferSectorSpecial(secm, sec);
         }
         break;
      default:
         break;
      }
      if(manual)
         return rtn;
   }
   return rtn;
}

//
// Change the friction values of sector
//
void EV_SetFriction(const int tag, int amount)
{
   amount = eclamp(amount, 1, 255); // Valid range allegedly 1 to 255

   for(int s = -1; (s = P_FindSectorFromTag(tag, s)) >= 0;)
   {
      sector_t &sector = sectors[s];

      if(amount == 100)
      {
         sector.flags     &= ~SECF_FRICTION;
         sector.friction   = ORIG_FRICTION;
         sector.movefactor = ORIG_FRICTION_FACTOR;
      }
      else
      {
         int friction, movefactor;
         P_CalcFriction(amount, friction, movefactor);

         sector.flags     |= SECF_FRICTION;
         sector.friction   = friction;
         sector.movefactor = movefactor;
      }
   }
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
// ioanch 20211030: renamed
//
static int P_FindSectorFromTagWithLowerBound(int tag, int start, int min)
{
   // Emulate original Doom's linear lower-bounded 
   // P_FindSectorFromLineArg0 as needed

   do
   {
      start = P_FindSectorFromTag(tag, start);
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
int EV_BuildStairs(int tag, stair_e type)
{
   // cph 2001/09/22 - cleaned up this function to save my sanity. 
   // A separate outer loop index makes the logic much cleared, and 
   // local variables moved into the inner blocks helps too
   int                   ssec = -1;
   int                   minssec = -1;
   int                   rtn = 0;

   // start a stair at each sector tagged the same as the linedef
   while((ssec = P_FindSectorFromTagWithLowerBound(tag, ssec, minssec)) >= 0)
   {
      int           secnum = ssec;
      sector_t*     sec = &sectors[secnum];
      
      // don't start a stair if the first step's floor is already moving
      if(!P_SectorActive(floor_special,sec)) //jff 2/22/98
      { 
         FloorMoveThinker*  floor;
         int           texture, height;
         fixed_t       stairsize;
         fixed_t       speed;
         int           ok;

         // create new floor thinker for first step
         rtn = 1;
         floor = new FloorMoveThinker;
         floor->addThinker();
         sec->srf.floor.data = floor;
         floor->direction = 1;
         floor->sector = sec;
         floor->type = buildStair;   //jff 3/31/98 do not leave uninited

         // ioanch: vanilla Doom stairs crushing behaviour is undefined! But in
         // practice the behaviour is similar to Hexen crushers because of how
         // the undefined values are set. But it's really depending on chance.

         // set up the speed and stepsize according to the stairs type
         switch(type)
         {
         default: // killough -- prevent compiler warning
         case build8:
            speed = FLOORSPEED/4;
            stairsize = 8*FRACUNIT;

            if(!demo_compatibility)
               floor->crush = -1; //jff 2/27/98 fix uninitialized crush field
            else
            {
               floor->crush = 10;
               floor->emulateStairCrush = true;
            }
            break;
         case turbo16:
            speed = FLOORSPEED*4;
            stairsize = 16*FRACUNIT;
            // ioanch: also allow crushing if demo_compatibility is true
            floor->crush = 10;  //jff 2/27/98 fix uninitialized crush field
            if(demo_compatibility)
               floor->emulateStairCrush = true;
            break;
         }

         floor->speed = speed;
         height = sec->srf.floor.height + stairsize;
         floor->floordestheight = height;
         
         texture = sec->srf.floor.pic;

         P_StairSequence(floor->sector);
         
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

               newsecnum = eindex(tsec-sectors);
               
               if(secnum != newsecnum)
                  continue;
               
               tsec = (sec->lines[i])->backsector;
               if(!tsec) continue;     //jff 5/7/98 if no backside, continue
               newsecnum = eindex(tsec - sectors);

               // if sector's floor is different texture, look for another
               if(tsec->srf.floor.pic != texture)
                  continue;

                 /* jff 6/19/98 prevent double stepsize
                  * killough 10/98: intentionally left this way [MBF comment]
                  * cph 2001/02/06: stair bug fix should be controlled by comp_stairs,
                  *  except if we're emulating MBF which perversly reverted the fix
                  */
               if(getComp(comp_stairs) || demo_version == 203)
                  height += stairsize; // jff 6/28/98 change demo compatibility

               // if sector's floor already moving, look for another
               if(P_SectorActive(floor_special,tsec)) //jff 2/22/98
                  continue;

               /* cph - see comment above - do this iff we didn't do so above */
               if(!getComp(comp_stairs) && demo_version != 203)
                  height += stairsize;

               sec = tsec;
               secnum = newsecnum;

               // create and initialize a thinker for the next step
               floor = new FloorMoveThinker;
               floor->addThinker();

               sec->srf.floor.data = floor; //jff 2/22/98
               floor->direction = 1;
               floor->sector = sec;
               floor->speed = speed;
               floor->floordestheight = height;
               floor->type = buildStair; //jff 3/31/98 do not leave uninited
               //jff 2/27/98 fix uninitialized crush field
               if(!demo_compatibility)
                  floor->crush = (type == build8 ? -1 : 10);
               else
               {
                  floor->crush = 10;
                  floor->emulateStairCrush = true;
               }
               P_StairSequence(floor->sector);
               ok = 1;
               break;
            } // end for

         } while(ok);      // continue until no next step is found

      } // end if(!P_SectorActive())

      /* killough 10/98: compatibility option */
      if(getComp(comp_stairs))
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

bool donut_emulation;

//
// DonutOverflow
//
// haleyjd 10/16/09: enables emulation of undefined behavior by donut actions
// when no proper reference sector is found outside the pool (ie, line is 
// marked as two-sided but is really one-sided).
//
// Thanks to entryway for discovering and coding a fix to this.
//
static bool DonutOverflow(fixed_t *pfloorheight, int16_t *pfloorpic)
{
   static bool firsttime  = true;
   static bool donutparm  = false;
   static int floorpic    = 0x16;
   static int floorheight = 0;

   if(firsttime)
   {
      int p;

      if((p = M_CheckParm("-donut")) && p < myargc - 2)
      {
         floorheight = (int)strtol(myargv[p + 1], nullptr, 0);
         floorpic    = (int)strtol(myargv[p + 2], nullptr, 0);

         // bounds-check floorpic
         if(floorpic <= 0 || floorpic >= numflats)
            floorpic = 0x16;

         donutparm = true;
      }

      firsttime = false;
   }

   // if -donut used, always emulate
   if(!donutparm)
   {
      // otherwise, only in demos and only when so requested
      if(!(demo_compatibility && donut_emulation))
         return false;
   }

   // SoM: Add the offset for the flats after all the calculations are done to 
   // preserve the old behavior in all cases.
   *pfloorheight = (fixed_t)floorheight;
   *pfloorpic    = (int16_t)floorpic + flatstart;

   return true;
}

//
// EV_DoParamDonut()
//
// Handle donut function: lower pillar, raise surrounding pool, both to height,
// texture and type of the sector surrounding the pool.
//
// Passed the linedef that triggered the donut
// Returns whether a thinker was created
//
// ioanch 20160305: made parameterized
//
int EV_DoParamDonut(const line_t *line, int tag, bool havespac,
                    fixed_t pspeed, fixed_t sspeed)
{
   sector_t    *s1, *s2, *s3;
   int          secnum;
   int          rtn;
   int          i;
   FloorMoveThinker *floor;
   fixed_t      s3_floorheight;
   int16_t      s3_floorpic;

   secnum = -1;
   rtn = 0;

   // ioanch: param tag0 support
   bool manual = false;
   if(havespac && !tag)
   {
      if(!line || !(s1 = line->backsector))
         return rtn;
      secnum = eindex(s1 - sectors);
      manual = true;
      goto manual_donut;
   }
   // do function on all sectors with same tag as linedef
   while(!manual && (secnum = P_FindSectorFromTag(tag,secnum)) >= 0)
   {
      s1 = &sectors[secnum];                // s1 is pillar's sector
   manual_donut:  // ioanch
      // do not start the donut if the pillar is already moving
      if(P_SectorActive(floor_special, s1)) //jff 2/22/98
      {
         if(manual)
            return rtn;
         continue;
      }
                      
      s2 = getNextSector(s1->lines[0], s1); // s2 is pool's sector
      if(!s2) continue;           // note lowest numbered line around
                                  // pillar must be two-sided 

      // do not start the donut if the pool is already moving
      if(!getComp(comp_floors) && P_SectorActive(floor_special, s2))
         continue;                           //jff 5/7/98
                      
      // find a two sided line around the pool whose other side isn't the pillar
      for (i = 0;i < s2->linecount;i++)
      {
         //jff 3/29/98 use true two-sidedness, not the flag
         if(getComp(comp_model))
         {
            // haleyjd 10/12/10: The first check here, which has a typo that
            // goes all the way back to vanilla DOOM, is inconsequential 
            // because it always evaluates to false. Since this is causing
            // a warning in GCC 4, I have commented it out.
            if(/*(!s2->lines[i]->flags & ML_TWOSIDED) ||*/
               s2->lines[i]->backsector == s1)
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
            // Give some slope a chance, but it may be wrong
            s3_floorheight = P_ExtremeHeightOnLine(*s3, *s2->lines[i], surf_floor, emin);
            s3_floorpic    = s3->srf.floor.pic;
         }
        
         //  Spawn rising slime
         floor = new FloorMoveThinker;
         floor->addThinker();
         s2->srf.floor.data = floor; //jff 2/22/98
         floor->type      = donutRaise;
         floor->crush     = -1;
         floor->direction = plat_up;
         floor->sector    = s2;
         floor->speed     = sspeed;
         floor->texture   = s3_floorpic;
         P_ZeroSpecialTransfer(&(floor->special));
         floor->floordestheight = s3_floorheight;
         P_FloorSequence(floor->sector);
        
         //  Spawn lowering donut-hole pillar
         floor = new FloorMoveThinker;
         floor->addThinker();
         s1->srf.floor.data = floor; //jff 2/22/98
         floor->type      = lowerFloor;
         floor->crush     = -1;
         floor->direction = plat_down;
         floor->sector    = s1;
         floor->speed     = pspeed;
         floor->floordestheight = s3_floorheight;
         P_FloorSequence(floor->sector);
         break;
      }
      if(manual)
         return rtn;
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
int EV_DoElevator(const line_t *line, const Mobj *mo, const polyobj_t *po, int tag,
                  elevator_e    elevtype, fixed_t speed, fixed_t amount, bool isParam)
{
   int                   secnum;
   int                   rtn;
   sector_t*             sec;
   ElevatorThinker*           elevator;

   secnum = -1;
   rtn = 0;
   bool manual = false;
   if(isParam && !tag)
   {
      if(!line || !(sec = line->backsector))
         return rtn;
      manual = true;
      goto manualElevator;
   }
   // act on all sectors with the same tag as the triggering linedef
   while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
   {
      sec = &sectors[secnum];
              
   manualElevator:
      // If either floor or ceiling is already activated, skip it
      if(sec->srf.floor.data || sec->srf.ceiling.data) //jff 2/22/98
      {
         if(manual)
            return rtn; // ioanch: also take care of manual activation
         continue;
      }
      
      // create and initialize new elevator thinker
      rtn = 1;
      elevator = new ElevatorThinker;
      elevator->addThinker();
      sec->srf.floor.data = elevator; //jff 2/22/98
      sec->srf.ceiling.data = elevator; //jff 2/22/98
      elevator->type = elevtype;

      elevator->speed = speed;
      elevator->sector = sec;

      // set up the fields according to the type of elevator action
      switch(elevtype)
      {
      // elevator down to next floor
      case elevateDown:
         elevator->direction = plat_down;
         elevator->floordestheight =
            P_FindNextLowestFloor(sec,sec->srf.floor.height);
         elevator->ceilingdestheight =
            elevator->floordestheight + sec->srf.ceiling.height - sec->srf.floor.height;
         break;

      // elevator up to next floor
      case elevateUp:
         elevator->direction = plat_up;
         elevator->floordestheight =
            P_FindNextHighestFloor(sec,sec->srf.floor.height);
         elevator->ceilingdestheight =
            elevator->floordestheight + sec->srf.ceiling.height - sec->srf.floor.height;
         break;

      // elevator to floor height of activating switch's front sector
      case elevateCurrent:
         // Fall back to different references if linedef is missing
         if(line)
            elevator->floordestheight = line->frontsector->srf.floor.height;
         else if(mo)
            elevator->floordestheight = mo->zref.floor;  // where it rests
         else if(po)
         {
            elevator->floordestheight = R_PointInSubsector(po->centerPt.x,
                                                           po->centerPt.y)
                  ->sector->srf.floor.height;   // the sector of the trigger poly
         }
         elevator->ceilingdestheight =
            elevator->floordestheight + sec->srf.ceiling.height - sec->srf.floor.height;
         elevator->direction =
            elevator->floordestheight>sec->srf.floor.height ? plat_up : plat_down;
         break;
      case elevateByValue:
         elevator->floordestheight = sec->srf.floor.height + amount;
         elevator->ceilingdestheight =
            elevator->floordestheight + sec->srf.ceiling.height - sec->srf.floor.height;
         elevator->direction = amount > 0 ? plat_up : plat_down;
         break;
      default:
         break;
      }
      P_FloorSequence(elevator->sector);
      if(manual)
         return rtn;
   }
   return rtn;
}

//
// EV_PillarBuild
//
// Pillar build init function
// joek 4/9/06
//
int EV_PillarBuild(const line_t *line, const pillardata_t *pd)
{
   PillarThinker *pillar;
   sector_t *sector;
   int returnval = 0;
   int sectornum = -1;
   fixed_t destfloorheight, destceilheight;
   bool manual = false;

   // check if a manual trigger, if so do just the sector on the backside
   if(pd->tag == 0)
   {
      if(!line || !(sector = line->backsector))
         return returnval;
      sectornum = eindex(sector - sectors);
      manual = true;
      goto manual_pillar;
   }

   while((sectornum = P_FindSectorFromTag(pd->tag, sectornum)) >= 0)
   {
      sector = &sectors[sectornum];

manual_pillar:
      const slopeheight_t &sh = pSlopeHeights[sectornum];
      // already being buggered about with,
      // or ceiling <= floor, therefore closed
      if(sector->srf.floor.data || sector->srf.ceiling.data ||
         sector->srf.ceiling.height - sector->srf.floor.height <= sh.touchheight)
      {
         if(manual)
            return returnval;
         else
            continue;
      }

      pillar = new PillarThinker;
      sector->srf.floor.data = pillar;
      sector->srf.ceiling.data = pillar;
      pillar->addThinker();
      pillar->sector = sector;

      if(pd->height == 0) // height == 0 so we meet in the middle
      {
         fixed_t gap = sector->srf.ceiling.height - sector->srf.floor.height - sh.touchheight;
         destfloorheight = sector->srf.floor.height + gap / 2;
         // Mind the round-off errors for compatibility
         destceilheight = sector->srf.ceiling.height - (gap - gap / 2);
      }
      else // else we meet at floorheight + args[2]
      {
         destfloorheight = sector->srf.floor.height + pd->height;
         destceilheight = destfloorheight + sh.touchheight;
      }

      if(pd->height == 0) // height is 0 so we meet halfway
      {
         pillar->ceilingSpeed = pd->speed;
         pillar->floorSpeed   = pd->speed;
      }
      else if(destfloorheight - sector->srf.floor.height > sector->srf.ceiling.height - destceilheight)
      {
         pillar->floorSpeed = pd->speed;
         pillar->ceilingSpeed = FixedDiv(sector->srf.ceiling.height - destceilheight,
                                   FixedDiv(destfloorheight - sector->srf.floor.height,
                                            pillar->floorSpeed));
      }
      else
      {
         pillar->ceilingSpeed = pd->speed;
         pillar->floorSpeed = FixedDiv(destfloorheight - sector->srf.floor.height,
                                 FixedDiv(sector->srf.ceiling.height - destceilheight,
                                          pillar->ceilingSpeed));
      }

      pillar->floordest = destfloorheight;
      pillar->ceilingdest = destceilheight;
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
int EV_PillarOpen(const line_t *line, const pillardata_t *pd)
{
   PillarThinker *pillar;
   sector_t *sector;
   int returnval = 0;
   int sectornum = -1;
   bool manual = false;

   // check if a manual trigger, if so do just the sector on the backside
   if(pd->tag == 0)
   {
      if(!line || !(sector = line->backsector))
         return returnval;
      sectornum = eindex(sector - sectors);
      manual = true;
      goto manual_pillar;
   }

   while((sectornum = P_FindSectorFromTag(pd->tag, sectornum)) >= 0)
   {
      sector = &sectors[sectornum];

manual_pillar:
      // already being buggered about with
      if(sector->srf.floor.data || sector->srf.ceiling.data ||
         sector->srf.floor.height + pSlopeHeights[sectornum].touchheight != sector->srf.ceiling.height)
      {
         if(manual)
            return returnval;
         else
            continue;
      }

      pillar = new PillarThinker;
      sector->srf.floor.data = pillar;
      sector->srf.ceiling.data = pillar;
      pillar->addThinker();
      pillar->sector = sector;

      if(pd->fdist == 0) // floordist == 0 so we find the next lowest floor
         pillar->floordest = P_FindLowestFloorSurrounding(sector);
      else               // else we meet at floorheight - args[2]
         pillar->floordest = sector->srf.floor.height - pd->fdist;

      if(pd->cdist == 0) // ceilingdist = 0 so we find the next highest ceiling
         pillar->ceilingdest = P_FindHighestCeilingSurrounding(sector);
      else               // else we meet at ceilingheight + args[3]
         pillar->ceilingdest = sector->srf.ceiling.height + pd->cdist;

      if(pillar->ceilingdest - sector->srf.ceiling.height >
            sector->srf.floor.height - pillar->floordest)
      {
         pillar->ceilingSpeed = pd->speed;
         pillar->floorSpeed = FixedDiv(sector->srf.floor.height - pillar->floordest,
                                 FixedDiv(pillar->ceilingdest - sector->srf.ceiling.height,
                                          pillar->ceilingSpeed));
      }
      else
      {
         pillar->floorSpeed = pd->speed;
         pillar->ceilingSpeed = FixedDiv(pillar->ceilingdest - sector->srf.ceiling.height,
                                   FixedDiv(sector->srf.floor.height - pillar->floordest,
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

   R_CacheTexture(flatnum);

   while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
   {
      R_CacheIfSkyTexture(sectors[secnum].srf.floor.pic, flatnum);
      sectors[secnum].srf.floor.pic = flatnum;
   }
}

//=============================================================================
//
// Waggle Floors
//
// haleyjd 06/30/09: joe was supposed to do these but he quit instead :P
//

IMPLEMENT_THINKER_TYPE(FloorWaggleThinker)

#define WGLSTATE_EXPAND 1
#define WGLSTATE_STABLE 2
#define WGLSTATE_REDUCE 3

//
// T_FloorWaggle
//
// haleyjd: thinker for floor waggle action.
//
void FloorWaggleThinker::Think()
{
   fixed_t destheight;
   fixed_t dist;
   extern fixed_t FloatBobOffsets[64];

   switch(state)
   {
   case WGLSTATE_EXPAND:
      if((scale += scaleDelta) >= targetScale)
      {
         scale = targetScale;
         state = WGLSTATE_STABLE;
      }
      break;

   case WGLSTATE_REDUCE:
      if((scale -= scaleDelta) <= 0)
      {
         // Remove
         destheight = originalHeight;
         dist       = originalHeight - sector->srf.floor.height;

         T_MoveFloorInDirection(sector, abs(dist), destheight, 8,
            destheight >= sector->srf.floor.height ? plat_down : plat_up, false);

         sector->srf.floor.data = nullptr;
         remove();
         return;
      }
      break;

   case WGLSTATE_STABLE:
      if(ticker != -1)
      {
         if(!--ticker)
            state = WGLSTATE_REDUCE;
      }
      break;
   }

   accumulator += accDelta;
   
   destheight = 
      originalHeight + 
         FixedMul(FloatBobOffsets[(accumulator >> FRACBITS) & 63], scale);
   dist = destheight - sector->srf.floor.height;

   T_MoveFloorInDirection(sector, abs(dist), destheight, 8,
      destheight >= sector->srf.floor.height ? plat_up : plat_down, false);
}

//
// FloorWaggleThinker::serialize
//
// Saves/loads a FloorWaggleThinker thinker.
//
void FloorWaggleThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << originalHeight << accumulator << accDelta << targetScale
       << scale << scaleDelta << ticker << state;
}

//
// EV_StartFloorWaggle
//
int EV_StartFloorWaggle(const line_t *line, int tag, int height, int speed,
                        int offset, int timer)
{
   int       sectorIndex = -1;
   int       retCode = 0;
   bool      manual = false;
   sector_t *sector;
   FloorWaggleThinker *waggle;

   if(tag == 0)
   {
      if(!line || !(sector = line->backsector))
         return retCode;
      sectorIndex = eindex(sector - sectors);
      manual = true;
      goto manual_waggle;
   }
   
   while((sectorIndex = P_FindSectorFromTag(tag, sectorIndex)) >= 0)
   {
      sector = &sectors[sectorIndex];

manual_waggle:
      // Already busy with another thinker
      if(sector->srf.floor.data)
      {
         if(manual)
            return retCode;
         else
            continue;
      }

      retCode = 1;
      waggle = new FloorWaggleThinker;
      sector->srf.floor.data = waggle;
      waggle->addThinker();

      waggle->sector         = sector;
      waggle->originalHeight = sector->srf.floor.height;
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

