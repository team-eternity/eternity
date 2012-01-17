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
//   Door animation code (opening/closing)
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "d_deh.h"    // Ty 03/27/98 - externalized
#include "d_dehtbl.h"
#include "d_gi.h"
#include "doomstat.h"
#include "dstrings.h"
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

//
// P_DoorSequence
//
// haleyjd 09/24/06: Plays the appropriate sound sequence for a door.
//
void P_DoorSequence(bool raise, bool turbo, bool bounced, sector_t *s)
{
   const char *seqName;

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
         S_ReplaceSectorSequenceName(s, seqName, true);
      else
         S_StartSectorSequenceName(s, seqName, true);
   }
}

///////////////////////////////////////////////////////////////
//
// Door action routines, called once per tick
//
///////////////////////////////////////////////////////////////

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
         case genRaise:
         case paramCloseIn: // haleyjd 03/01/05
         case blazeRaise:
         case genBlazeRaise:
         case paramBlazeCloseIn: // haleyjd 03/01/05
            direction = plat_down; // time to go back down
            P_DoorSequence(false, turbo, false, sector); // haleyjd
            break;

         case close30ThenOpen:
         case genCdO:
         case genBlazeCdO:
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
         case raiseIn5Mins:
         case paramRaiseIn:      // haleyjd 03/01/05: new param type
         case paramBlazeRaiseIn: // haleyjd 03/01/05: new param type
            direction = plat_up; // time to raise then
            if(turbo)
               type = genBlazeRaise; // act like a blaze raise door
            else
               type = doorNormal;    // door acts just like normal 1 DR door now
            P_DoorSequence(true, turbo, false, sector); // haleyjd
            break;
            
         default:
            break;
         }
      }
      break;

   case plat_down:
      // Door is moving down
      res = T_MovePlane(sector, speed, sector->floorheight, -1, 1, direction);

      // killough 10/98: implement gradual lighting effects
      if(lighttag && topheight - sector->floorheight)
         EV_LightTurnOnPartway(lighttag,
                               FixedDiv(sector->ceilingheight - sector->floorheight,
                                        topheight - sector->floorheight));

      // handle door reaching bottom
      if(res == pastdest)
      {
         S_StopSectorSequence(sector, true);

         switch(type)
         {
         // regular raise and close doors are all done, remove them
         case doorNormal:
         case doorClose:
         case blazeRaise:
         case blazeClose:
         case genRaise:
         case genClose:
         case genBlazeRaise:
         case genBlazeClose:
         case paramCloseIn:      // haleyjd 03/01/05
         case paramBlazeCloseIn: // haleyjd 03/01/05
            sector->ceilingdata = NULL;  //jff 2/22/98
            this->removeThinker();  // unlink and free
            // killough 4/15/98: remove double-closing sound of blazing doors
            // haleyjd 10/06/06: behavior is determined via sound sequence now
            break;

            // close then open doors start waiting
         case close30ThenOpen:
         case genCdO:
         case genBlazeCdO:
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
         case paramBlazeCloseIn:
         case genClose:
         case genBlazeClose:
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
      res = T_MovePlane(sector, speed, topheight, -1, 1, direction);

      // killough 10/98: implement gradual lighting effects
      if(lighttag && topheight - sector->floorheight)
         EV_LightTurnOnPartway(lighttag,
                               FixedDiv(sector->ceilingheight - sector->floorheight,
                                        topheight - sector->floorheight));

      // handle door reaching the top
      if(res == pastdest)
      {
         switch(type)
         {
         case blazeRaise:       // regular open/close doors start waiting
         case doorNormal:
         case genRaise:
         case genBlazeRaise:
            direction    = plat_stop; // wait at top with delay
            topcountdown = topwait;
            break;
            
         case close30ThenOpen:  // close and close/open doors are done
         case blazeOpen:
         case doorOpen:
         case genBlazeOpen:
         case genOpen:
         case genCdO:
         case genBlazeCdO:
            S_StopSectorSequence(sector, true);
            sector->ceilingdata = NULL; //jff 2/22/98
            this->removeThinker(); // unlink and free
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
         case genOpen:
         case genBlazeOpen:
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
       << topcountdown << line << lighttag << turbo;
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
   S_SquashSectorSequence(sector, true);
   return true;
}

///////////////////////////////////////////////////////////////
//
// Door linedef handlers
//
///////////////////////////////////////////////////////////////

//
// EV_DoLockedDoor
//
// Handle opening a tagged locked door
//
// Passed the line activating the door, the type of door,
// and the thing that activated the line
// Returns true if a thinker created
//
int EV_DoLockedDoor(line_t *line, vldoor_e type, Mobj *thing)
{
   player_t *p = thing->player;
   
   if(!p)          // only players can open locked doors
      return 0;

   // check type of linedef, and if key is possessed to open it
   switch(line->special)
   {
   case 99:  // Blue Lock
   case 133:
      if(!p->cards[it_bluecard] && !p->cards[it_blueskull])
      {
         // Ty 03/27/98 - externalized
         player_printf(p, "%s", DEH_String("PD_BLUEO"));
         S_StartSound(p->mo, GameModeInfo->playerSounds[sk_oof]); // killough 3/20/98
         return 0;
      }
      break;

   case 134: // Red Lock
   case 135:
      if(!p->cards[it_redcard] && !p->cards[it_redskull])
      {
         const char *msg = (GameModeInfo->type == Game_Heretic) 
                           ? DEH_String("HPD_GREENO") : DEH_String("PD_REDO");
         player_printf(p, "%s", msg);  // Ty 03/27/98 - externalized
         S_StartSound(p->mo, GameModeInfo->playerSounds[sk_oof]); // killough 3/20/98
         return 0;
      }
      break;

   case 136: // Yellow Lock
   case 137:
      if (!p->cards[it_yellowcard] && !p->cards[it_yellowskull])
      {
         // Ty 03/27/98 - externalized
         player_printf(p, "%s", DEH_String("PD_YELLOWO"));
         S_StartSound(p->mo, GameModeInfo->playerSounds[sk_oof]); // killough 3/20/98
         return 0;
      }
      break;
   }

   // got the key, so open the door
   return EV_DoDoor(line,type);
}

//
// EV_DoDoor
//
// Handle opening a tagged door
//
// Passed the line activating the door and the type of door
// Returns true if a thinker created
//
int EV_DoDoor(line_t *line, vldoor_e type)
{
   int secnum = -1, rtn = 0;
   sector_t *sec;
   VerticalDoorThinker *door;

   // open all doors with the same tag as the activating line
   while ((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
   {
      sec = &sectors[secnum];
      // if the ceiling already moving, don't start the door action
      if(P_SectorActive(ceiling_special,sec)) //jff 2/22/98
         continue;

      // new door thinker
      rtn = 1;
      door = new VerticalDoorThinker;
      door->addThinker();
      sec->ceilingdata = door; //jff 2/22/98

      door->sector   = sec;
      door->type     = type;
      door->topwait  = VDOORWAIT;
      door->speed    = VDOORSPEED;
      door->line     = line;  // jff 1/31/98 remember line that triggered us
      door->lighttag = 0; // killough 10/98: no light effects with tagged doors

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

      case close30ThenOpen:
         door->topheight = sec->ceilingheight;
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
         if(door->topheight != sec->ceilingheight)
            P_DoorSequence(true, true, false, door->sector); // haleyjd
         break;

      case doorNormal:
      case doorOpen:
         door->direction = plat_up;
         door->topheight = P_FindLowestCeilingSurrounding(sec);
         door->topheight -= 4*FRACUNIT;
         door->turbo     = false;
         if(door->topheight != sec->ceilingheight)
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
int EV_VerticalDoor(line_t *line, Mobj *thing)
{
   player_t *player;
   sector_t *sec;
   VerticalDoorThinker *door;
   SectorThinker       *secThinker;
   
   //  Check for locks
   player = thing->player;
   
   switch(line->special)
   {
   case 26: // Blue Lock
   case 32:
      if(!player)
         return 0;
      if(!player->cards[it_bluecard] && !player->cards[it_blueskull])
      {
         // Ty 03/27/98 - externalized
         player_printf(player, "%s", DEH_String("PD_BLUEK"));
         S_StartSound(player->mo, GameModeInfo->playerSounds[sk_oof]); // killough 3/20/98
         return 0;
      }
      break;

   case 27: // Yellow Lock
   case 34:
      if( !player )
         return 0;
      if(!player->cards[it_yellowcard] && !player->cards[it_yellowskull])
      {
         // Ty 03/27/98 - externalized
         player_printf(player, "%s", DEH_String("PD_YELLOWK"));
         S_StartSound(player->mo, GameModeInfo->playerSounds[sk_oof]); // killough 3/20/98
         return 0;
      }
      break;

   case 28: // Red Lock
   case 33:
      if( !player )
         return 0;
      if(!player->cards[it_redcard] && !player->cards[it_redskull])
      {
         const char *msg = (GameModeInfo->type == Game_Heretic)
                           ? DEH_String("HPD_GREENK") : DEH_String("PD_REDK");
         player_printf(player, "%s", msg);  // Ty 03/27/98 - externalized
         S_StartSound(player->mo, GameModeInfo->playerSounds[sk_oof]); // killough 3/20/98
         return 0;
      }
      break;

   default:
      break;
   }

   // if the wrong side of door is pushed, give oof sound
   if(line->sidenum[1] == -1)                      // killough
   {
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

   secThinker = thinker_cast<SectorThinker *>(sec->ceilingdata);

   // exactly only one at most of these pointers is valid during demo_compatibility
   if(demo_compatibility)
   {
      if(!secThinker)
         secThinker = thinker_cast<SectorThinker *>(sec->floordata);
      if(!secThinker)
         secThinker = thinker_cast<SectorThinker *>(sec->lightingdata);
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
         return secThinker->reTriggerVerticalDoor(!!thing->player);
      }
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
   
   sec->ceilingdata = door; //jff 2/22/98
   
   door->sector    = sec;
   door->direction = plat_up;
   door->speed     = VDOORSPEED;
   door->turbo     = false;
   door->topwait   = VDOORWAIT;
   door->line      = line; // jff 1/31/98 remember line that triggered us

   // killough 10/98: use gradual lighting changes if nonzero tag given
   door->lighttag = comp[comp_doorlight] ? 0 : line->tag; // killough 10/98
   
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
      door->type = doorOpen;
      line->special = 0;
      break;

   case 117: // blazing door raise
      door->type = blazeRaise;
      door->speed = VDOORSPEED*4;
      door->turbo = true;
      break;

   case 118: // blazing door open
      door->type = blazeOpen;
      line->special = 0;
      door->speed = VDOORSPEED*4;
      door->turbo = true;
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


///////////////////////////////////////////////////////////////
//
// Sector type door spawners
//
///////////////////////////////////////////////////////////////

//
// P_SpawnDoorCloseIn30()
//
// Spawn a door that closes after 30 seconds (called at level init)
//
// Passed the sector of the door, whose type specified the door action
// Returns nothing

void P_SpawnDoorCloseIn30 (sector_t* sec)
{
   VerticalDoorThinker *door = new VerticalDoorThinker;
   
   door->addThinker();
   
   sec->ceilingdata = door; //jff 2/22/98
   sec->special = 0;
   
   door->sector       = sec;
   door->direction    = plat_stop;
   door->type         = doorNormal;
   door->speed        = VDOORSPEED;
   door->turbo        = false;
   door->topcountdown = 30 * 35;
   door->line         = NULL; // jff 1/31/98 remember line that triggered us
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

void P_SpawnDoorRaiseIn5Mins(sector_t *sec, int secnum)
{
   VerticalDoorThinker *door = new VerticalDoorThinker;
   
   door->addThinker();
   
   sec->ceilingdata = door; //jff 2/22/98
   sec->special = 0;
   
   door->sector       = sec;
   door->direction    = plat_special; // haleyjd: changed from 2
   door->type         = raiseIn5Mins;
   door->speed        = VDOORSPEED;
   door->turbo        = false;
   door->topheight    = P_FindLowestCeilingSurrounding(sec);
   door->topheight   -= 4*FRACUNIT;
   door->topwait      = VDOORWAIT;
   door->topcountdown = 5 * 60 * 35;
   door->line         = NULL; // jff 1/31/98 remember line that triggered us
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

