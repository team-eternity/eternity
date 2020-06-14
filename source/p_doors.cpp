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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//   Door animation code (opening/closing)
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "d_deh.h"    // Ty 03/27/98 - externalized
#include "d_dehtbl.h"
#include "d_gi.h"
#include "doomstat.h"
#include "dstrings.h"
#include "e_inventory.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "p_info.h"
#include "p_saveg.h"
#include "p_skin.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_main.h"
#include "r_state.h"
#include "s_sndseq.h"
#include "s_sound.h"
#include "sounds.h"
#include "t_plane.h"

//
// P_DoorSequence
//
// haleyjd 09/24/06: Plays the appropriate sound sequence for a door.
//
void P_DoorSequence(bool raise, bool turbo, bool bounced, sector_t *s)
{
   // haleyjd 09/25/06: apparently fraggle forgot silentmove for doors
   if(silentmove(s))
      return;

   if(s->sndSeqID >= 0)
   {
      if(bounced) // if the door bounced, replace the sequence
         S_ReplaceSectorSequence(s, SEQ_DOOR);
      else
         S_StartSectorSequence(s, SEQ_DOOR);
   }
   else
   {
      const char *seqName;
      if(raise)
         seqName = turbo ? "EEDoorOpenBlazing" : "EEDoorOpenNormal";
      else
      {
         if(turbo)
         {
            if(GameModeInfo->type == Game_DOOM && comp[comp_blazing])
               seqName = "EEDoorCloseBlazingComp";
            else
               seqName = "EEDoorCloseBlazing";
         }
         else
            seqName = "EEDoorCloseNormal";
      }

      if(bounced)
         S_ReplaceSectorSequenceName(s, seqName, SEQ_ORIGIN_SECTOR_C);
      else
         S_StartSectorSequenceName(s, seqName, SEQ_ORIGIN_SECTOR_C);
   }
}

//=============================================================================
//
// Door action routines, called once per tick
//

IMPLEMENT_THINKER_TYPE(VerticalDoorThinker)

//
// T_VerticalDoor
//
// Passed a door structure containing all info about the door.
// See P_SPEC.H for fields.
// Returns nothing.
//
// jff 02/08/98 all cases with labels beginning with gen added to support
// generalized line type behaviors.
//
void VerticalDoorThinker::Think()
{
   result_e  res;
   
   // Is the door waiting, going up, or going down?
   switch(direction)
   {
   case plat_stop:
      // Door is waiting
      if(!--topcountdown)  // downcount and check
      {
         switch(type)
         {
         case doorNormal:
         case paramCloseIn: // haleyjd 03/01/05
         case blazeRaise:
            direction = plat_down; // time to go back down
            P_DoorSequence(false, turbo, false, sector); // haleyjd
            break;

         case closeThenOpen:
            direction = plat_up;  // time to go back up
            P_DoorSequence(true, turbo, false, sector); // haleyjd
            break;

         default:
            break;
         }
      }
      break;

   case plat_special: // haleyjd: changed from 2
      // Special case for sector type door that opens in 5 mins
      if(!--topcountdown)  // 5 minutes up?
      {
         switch(type)
         {
         case doorRaiseIn:
            direction = plat_up; // time to raise then
            if(turbo)
               type = blazeRaise; // act like a blaze raise door
            else
               type = doorNormal; // door acts just like normal 1 DR door now
            P_DoorSequence(true, turbo, false, sector); // haleyjd
            break;
            
         default:
            break;
         }
      }
      break;

   case plat_down:
      // Door is moving down
      res = T_MoveCeilingDown(sector, speed, sector->srf.floor.height, -1);

      // killough 10/98: implement gradual lighting effects
      if(lighttag && topheight - sector->srf.floor.height)
         EV_LightTurnOnPartway(lighttag,
                               FixedDiv(sector->srf.ceiling.height - sector->srf.floor.height,
                                        topheight - sector->srf.floor.height));

      // handle door reaching bottom
      if(res == pastdest)
      {
         S_StopSectorSequence(sector, SEQ_ORIGIN_SECTOR_C);

         switch(type)
         {
         // regular raise and close doors are all done, remove them
         case doorNormal:
         case doorClose:
         case blazeRaise:
         case blazeClose:
         case paramCloseIn:      // haleyjd 03/01/05
            sector->srf.ceiling.data = NULL;  //jff 2/22/98
            this->remove();  // unlink and free
            // killough 4/15/98: remove double-closing sound of blazing doors
            // haleyjd 10/06/06: behavior is determined via sound sequence now
            break;

            // close then open doors start waiting
         case closeThenOpen:
            direction    = plat_stop;
            topcountdown = topwait; // jff 5/8/98 insert delay
            break;

         default:
            break;
         }
      }

      //jff 1/31/98 turn lighting off in tagged sectors of manual doors
      // killough 10/98: replaced with gradual lighting code

      else if(res == crushed)
      {
         // handle door meeting obstruction on way down
         switch(type)
         {
         case paramCloseIn:      // haleyjd 03/01/05
         case blazeClose:
         case doorClose:      // Close types do not bounce, merely wait
            break;
            
         default:             // other types bounce off the obstruction
            direction = plat_up;
            P_DoorSequence(true, false, true, sector); // haleyjd
            break;
         }
      }
      break;

   case plat_up:
      // Door is moving up
      res = T_MoveCeilingUp(sector, speed, topheight, -1);

      // killough 10/98: implement gradual lighting effects
      if(lighttag && topheight - sector->srf.floor.height)
         EV_LightTurnOnPartway(lighttag,
                               FixedDiv(sector->srf.ceiling.height - sector->srf.floor.height,
                                        topheight - sector->srf.floor.height));

      // handle door reaching the top
      if(res == pastdest)
      {
         switch(type)
         {
         case blazeRaise:       // regular open/close doors start waiting
         case doorNormal:
            direction    = plat_stop; // wait at top with delay
            topcountdown = topwait;
            break;
            
         case closeThenOpen:  // close and close/open doors are done
         case blazeOpen:
         case doorOpen:
            S_StopSectorSequence(sector, SEQ_ORIGIN_SECTOR_C);
            sector->srf.ceiling.data = NULL; //jff 2/22/98
            this->remove(); // unlink and free
            break;
            
         default:
            break;
         }
      }

      // SoM: With attached sectors, doors can now encounter crushed events while opening
      else if(demo_version >= 333 && res == crushed)
      {
         // handle door meeting obstruction on attached surface moving down
         switch(type)
         {
         case doorOpen:      // Open types do not bounce, merely wait
         case blazeOpen:
            break;
            
         default:             // other types bounce off the obstruction
            direction = plat_down;
            P_DoorSequence(false, false, true, sector); // haleyjd
            break;
         }
      }

      //jff 1/31/98 turn lighting on in tagged sectors of manual doors
      // killough 10/98: replaced with gradual lighting code
      break;
   }
}

//
// VerticalDoorThinker::serialize
//
// Saves/loads VerticalDoorThinker thinkers.
//
void VerticalDoorThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << type << topheight << speed << direction << topwait
       << topcountdown << lighttag << turbo;
}

//
// VerticalDoorThinker::reTriggerVerticalDoor
//
// haleyjd 10/13/2011: extracted logic from EV_VerticalDoor for retriggering
// and reversal of door thinkers that are in motion.
//
bool VerticalDoorThinker::reTriggerVerticalDoor(bool player)
{
   if(direction == plat_down)
      direction = plat_up;  // go back up
   else
   {
      if(!player)
         return false;           // JDC: bad guys never close doors

      direction = plat_down; // start going down immediately
   }

   // haleyjd: squash the sector's sound sequence when a door reversal
   // occurs, otherwise you get a doubled sound at the next downstroke.
   S_SquashSectorSequence(sector, SEQ_ORIGIN_SECTOR_C);
   return true;
}

///////////////////////////////////////////////////////////////
//
// Door linedef handlers
//
///////////////////////////////////////////////////////////////

//
// EV_DoDoor
//
// Handle opening a tagged door
//
// Passed the line activating the door and the type of door
// Returns true if a thinker created
//
int EV_DoDoor(const line_t *line, vldoor_e type)
{
   int secnum = -1, rtn = 0;
   sector_t *sec;
   VerticalDoorThinker *door;

   // open all doors with the same tag as the activating line
   while((secnum = P_FindSectorFromLineArg0(line, secnum)) >= 0)
   {
      sec = &sectors[secnum];
      // if the ceiling already moving, don't start the door action
      if(P_SectorActive(ceiling_special, sec)) //jff 2/22/98
         continue;

      // new door thinker
      rtn = 1;
      door = new VerticalDoorThinker;
      door->addThinker();
      sec->srf.ceiling.data = door; //jff 2/22/98

      door->sector    = sec;
      door->type      = type;
      door->topwait   = VDOORWAIT;
      door->speed     = VDOORSPEED;
      door->lighttag  = 0;     // killough 10/98: no light effects with tagged doors

      // setup door parameters according to type of door
      switch(type)
      {
      case blazeClose:
         door->topheight = P_FindLowestCeilingSurrounding(sec);
         door->topheight -= 4*FRACUNIT;
         door->direction = plat_down;
         door->speed     = VDOORSPEED * 4;
         door->turbo     = true;
         P_DoorSequence(false, true, false, door->sector); // haleyjd
         break;

      case doorClose:
         door->topheight = P_FindLowestCeilingSurrounding(sec);
         door->topheight -= 4*FRACUNIT;
         door->direction = plat_down;
         door->turbo     = false;
         P_DoorSequence(false, false, false, door->sector); // haleyjd
         break;

      case closeThenOpen:
         door->topheight = sec->srf.ceiling.height;
         door->direction = plat_down;
         door->topwait   = 30 * TICRATE;                    // haleyjd 01/16/12: set here
         door->turbo     = false;
         P_DoorSequence(false, false, false, door->sector); // haleyjd
         break;

      case blazeRaise:
      case blazeOpen:
         door->direction = plat_up;
         door->topheight = P_FindLowestCeilingSurrounding(sec);
         door->topheight -= 4*FRACUNIT;
         door->speed     = VDOORSPEED * 4;
         door->turbo     = true;
         if(door->topheight != sec->srf.ceiling.height)
            P_DoorSequence(true, true, false, door->sector); // haleyjd
         break;

      case doorNormal:
      case doorOpen:
         door->direction = plat_up;
         door->topheight = P_FindLowestCeilingSurrounding(sec);
         door->topheight -= 4*FRACUNIT;
         door->turbo     = false;
         if(door->topheight != sec->srf.ceiling.height)
            P_DoorSequence(true, false, false, door->sector); // haleyjd
         break;
         
      default:
         break;
      }
   }
   return rtn;
}

//
// EV_VerticalDoor
//
// Handle opening a door manually, no tag value
//
// Passed the line activating the door and the thing activating it
// Returns true if a thinker created
//
// jff 2/12/98 added int return value, fixed all returns
//
int EV_VerticalDoor(line_t *line, const Mobj *thing, int lockID)
{
   player_t *player = thing ? thing->player : nullptr;
   sector_t *sec;
   VerticalDoorThinker *door;
   SectorThinker       *secThinker;
   
   // Check for locks
   if(lockID)
   {
      if(!player)
         return 0;
      if(!E_PlayerCanUnlock(player, lockID, false))
         return 0;
   }

   // if the wrong side of door is pushed, give oof sound
   if(line->sidenum[1] == -1)                      // killough
   {
      if(player)
         S_StartSound(player->mo, GameModeInfo->playerSounds[sk_oof]); // killough 3/20/98
      return 0;
   }

   // get the sector on the second side of activating linedef
   sec = sides[line->sidenum[1]].sector;

   // haleyjd: adapted cph's prboom fix for demo compatibility and
   //          corruption of thinkers
   // Two bugs: 
   // 1. DOOM used any thinker that was on a door
   // 2. DOOM assumed the thinker was a T_VerticalDoor thinker, and 
   //    this bug was even still in Eternity -- fixed when not in 
   //    demo_compatibility (only VerticalDoorThinker::reTriggerVerticalDoor
   //    will actually do anything outside of demo_compatibility mode)

   secThinker = thinker_cast<SectorThinker *>(sec->srf.ceiling.data);

   // exactly only one at most of these pointers is valid during demo_compatibility
   if(demo_compatibility)
   {
      if(!secThinker)
         secThinker = thinker_cast<SectorThinker *>(sec->srf.floor.data);
   }
   
   // if door already has a thinker, use it
   if(secThinker)
   {
      switch(line->special)
      {
      case  1: // only for "raise" doors, not "open"s
      case  26:
      case  27:
      case  28:
      case  117:
         return secThinker->reTriggerVerticalDoor(thing && thing->player);
      }
      
      // haleyjd 01/22/12: never start multiple thinkers on a door sector.
      // This bug has been here since the beginning, and got especially bad
      // in Strife. In Vanilla DOOM, door types 31-34 and 118 can fall through
      // here and cause the sector to become jammed as multiple thinkers fight 
      // for control.
      if(full_demo_version >= make_full_version(340, 22))
         return 0; // sector is busy.
   }

   // emit proper sound
   switch(line->special)
   {
   case 117: // blazing door raise
   case 118: // blazing door open
      P_DoorSequence(true, true, false, sec); // haleyjd
      break;

   default:  // normal door sound
      P_DoorSequence(true, false, false, sec); // haleyjd
      break;
   }

   // new door thinker
   door = new VerticalDoorThinker;
   door->addThinker();
   
   sec->srf.ceiling.data = door; //jff 2/22/98
   
   door->sector    = sec;
   door->direction = plat_up;
   door->speed     = VDOORSPEED;
   door->turbo     = false;
   door->topwait   = VDOORWAIT;

   // killough 10/98: use gradual lighting changes if nonzero tag given
   door->lighttag = comp[comp_doorlight] ? 0 : line->args[0]; // killough 10/98
   
   // set the type of door from the activating linedef type
   switch(line->special)
   {
   case 1:
   case 26:
   case 27:
   case 28:
      door->type = doorNormal;
      break;

   case 31:
   case 32:
   case 33:
   case 34:
      door->type    = doorOpen;
      line->special = 0;
      break;

   case 117: // blazing door raise
      door->type  = blazeRaise;
      door->speed = VDOORSPEED*4;
      door->turbo = true;
      break;

   case 118: // blazing door open
      door->type    = blazeOpen;
      line->special = 0;
      door->speed   = VDOORSPEED*4;
      door->turbo   = true;
      break;

   default:
      door->lighttag = 0;   // killough 10/98
      break;
   }
   
   // find the top and bottom of the movement range
   door->topheight = P_FindLowestCeilingSurrounding(sec);
   door->topheight -= 4*FRACUNIT;
   return 1;
}

//=============================================================================
//
// Sector type door spawners
//

//
// P_SpawnDoorCloseIn30()
//
// Spawn a door that closes after 30 seconds (called at level init)
//
// Passed the sector of the door, whose type specified the door action
// Returns nothing
//
void P_SpawnDoorCloseIn30(sector_t* sec)
{
   VerticalDoorThinker *door = new VerticalDoorThinker;
   
   door->addThinker();
   
   sec->srf.ceiling.data = door; //jff 2/22/98
   P_ZeroSectorSpecial(sec);
   
   door->sector       = sec;
   door->direction    = plat_stop;
   door->type         = doorNormal;
   door->speed        = VDOORSPEED;
   door->turbo        = false;
   door->topcountdown = 30 * 35;
   door->lighttag     = 0;  // killough 10/98: no lighting changes
}

//
// P_SpawnDoorRaiseIn5Mins()
//
// Spawn a door that opens after 5 minutes (called at level init)
//
// Passed the sector of the door, whose type specified the door action
// Returns nothing
//
void P_SpawnDoorRaiseIn5Mins(sector_t *sec)
{
   VerticalDoorThinker *door = new VerticalDoorThinker;
   
   door->addThinker();
   
   sec->srf.ceiling.data = door; //jff 2/22/98
   P_ZeroSectorSpecial(sec);
   
   door->sector       = sec;
   door->direction    = plat_special; // haleyjd: changed from 2
   door->type         = doorRaiseIn;
   door->speed        = VDOORSPEED;
   door->turbo        = false;
   door->topheight    = P_FindLowestCeilingSurrounding(sec);
   door->topheight   -= 4*FRACUNIT;
   door->topwait      = VDOORWAIT;
   door->topcountdown = 5 * 60 * 35;
   door->lighttag     = 0;    // killough 10/98: no lighting changes
}

//----------------------------------------------------------------------------
//
// $Log: p_doors.c,v $
// Revision 1.13  1998/05/09  12:16:29  jim
// formatted/documented p_doors
//
// Revision 1.12  1998/05/03  23:07:16  killough
// Fix #includes at the top, remove #if 0, nothing else
//
// Revision 1.11  1998/04/16  06:28:34  killough
// Remove double-closing sound of blazing doors
//
// Revision 1.10  1998/03/28  05:32:36  jim
// Text enabling changes for DEH
//
// Revision 1.9  1998/03/23  03:24:53  killough
// Make door-opening 'oof' sound have true source
//
// Revision 1.8  1998/03/10  07:08:16  jim
// Extended manual door lighting to generalized doors
//
// Revision 1.7  1998/02/23  23:46:40  jim
// Compatibility flagged multiple thinker support
//
// Revision 1.6  1998/02/23  00:41:36  jim
// Implemented elevators
//
// Revision 1.5  1998/02/13  03:28:25  jim
// Fixed W1,G1 linedefs clearing untriggered special, cosmetic changes
//
// Revision 1.4  1998/02/08  05:35:23  jim
// Added generalized linedef types
//
// Revision 1.2  1998/01/26  19:23:58  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:59  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

