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
//  Plats (i.e. elevator platforms) code, raising/lowering.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "d_gi.h"
#include "doomstat.h"
#include "m_random.h"
#include "p_info.h"
#include "p_saveg.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_main.h"
#include "r_state.h"
#include "s_sndseq.h"
#include "s_sound.h"
#include "sounds.h"
#include "t_plane.h"

// New limit-free plat structure -- killough

struct platlist_t
{
   PlatThinker  *plat; 
   platlist_t   *next;
   platlist_t  **prev;
};

static platlist_t *activeplats;

//
// P_PlatSequence
//
// haleyjd 09/26/06: Starts a sound sequence for a plat.
//
void P_PlatSequence(sector_t *s, const char *seqname)
{
   if(silentmove(s))
      return;

   if(s->sndSeqID >= 0)
      S_StartSectorSequence(s, SEQ_PLAT);
   else
      S_StartSectorSequenceName(s, seqname, SEQ_ORIGIN_SECTOR_F);
}

IMPLEMENT_THINKER_TYPE(PlatThinker)

//
// T_PlatRaise()
//
// Action routine to move a plat up and down
//
// Passed a plat structure containing all pertinent information about the move
// No return value
//
// jff 02/08/98 all cases with labels beginning with gen added to support 
// generalized line type behaviors.
//
void PlatThinker::Think()
{
   result_e res;
   
   // handle plat moving, up, down, waiting, or in stasis
   switch(status)
   {
   case up: // plat moving up
      res = T_MoveFloorUp(sector, speed, high, crush);
                                        
      // if a pure raise type, make the plat moving sound
      // haleyjd: now handled through sound sequences
      
      // if encountered an obstacle, and not a crush type, reverse direction
      if(res == crushed && crush <= 0)
      {
         count  = wait;
         status = down;
         P_PlatSequence(sector, "EEPlatNormal"); // haleyjd
      }
      else  // else handle reaching end of up stroke
      {
         if(res == pastdest) // end of stroke
         {
            S_StopSectorSequence(sector, SEQ_ORIGIN_SECTOR_F); // haleyjd

            // if not an instant toggle type, wait, make plat stop sound
            if(type != toggleUpDn)
            {
               count  = wait;
               status = waiting;
            }
            else // else go into stasis awaiting next toggle activation
            {
               oldstatus = status;    //jff 3/14/98 after action wait  
               status    = in_stasis; //for reactivation of toggle
            }

            // lift types and pure raise types are done at end of up stroke
            // only the perpetual type waits then goes back up
            switch(type)
            {
            case raiseToNearestAndChange:
               // haleyjd 07/16/04: In Heretic, this type of plat goes into 
               // stasis forever, preventing any additional actions
               if(GameModeInfo->type == Game_Heretic)
                  break;
            case blazeDWUS:
            case downWaitUpStay:
            case raiseAndChange:
            case genLift:
               removeActivePlat();     // killough
            default:
               break;
            }
         }
      }
      break;
        
   case down: // plat moving down
      res = T_MoveFloorDown(sector, speed, low, -1);

      // handle reaching end of down stroke
      // SoM: attached sectors means the plat can crush when heading down too
      // if encountered an obstacle, and not a crush type, reverse direction
      if(demo_version >= 333 && res == crushed && crush <= 0)
      {
         count  = wait;
         status = up;
         P_PlatSequence(sector, "EEPlatNormal"); // haleyjd
      }
      else if(res == pastdest)
      {
         S_StopSectorSequence(sector, SEQ_ORIGIN_SECTOR_F); // haleyjd

         // if not an instant toggle, start waiting, make plat stop sound
         if(type != toggleUpDn) //jff 3/14/98 toggle up down
         {                           // is silent, instant, no waiting
            count  = wait;
            status = waiting;
         }
         else // instant toggles go into stasis awaiting next activation
         {
            oldstatus = status;    //jff 3/14/98 after action wait  
            status    = in_stasis; //for reactivation of toggle
         }

         // jff 1/26/98 remove the plat if it bounced so it can be tried again
         //   only affects plats that raise and bounce
         // killough 1/31/98: relax compatibility to demo_compatibility
         // haleyjd 02/18/13: made demo check specific to cases to which it 
         //   applies only, for the benefit of upWaitDownStay plats

         switch(type)
         {
         case raiseAndChange:
         case raiseToNearestAndChange:
            if(demo_version < 203 ? !demo_compatibility : !comp[comp_floors])
               removeActivePlat();
            break;
         case upWaitDownStay: // haleyjd 02/18/13: Hexen/Strife reverse plats
            removeActivePlat();
            break;
         default:
            break;
         }
      }
      break;

   case waiting: // plat is waiting
      if(!--count)  // downcount and check for delay elapsed
      {
         if(sector->floorheight == low)
            status = up;     // if at bottom, start up
         else
            status = down;   // if at top, start down
         
         // make plat start sound
         // haleyjd: changed for sound sequences
         if(type == toggleUpDn)
            P_PlatSequence(sector, "EEPlatSilent");
         else
            P_PlatSequence(sector, "EEPlatNormal");
      }
      break; //jff 1/27/98 don't pickup code added later to in_stasis

   case in_stasis: // do nothing if in stasis
      break;
   }
}

//
// PlatThinker::serialize
//
// Saves/loads PlatThinker thinkers.
//
void PlatThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << speed << low << high << wait << count << status << oldstatus
       << crush << tag << type;

   // Reattach to active plats list
   if(arc.isLoading())
      addActivePlat();
}

//
// PlatThinker::reTriggerVerticalDoor
//
// haleyjd 10/13/2011: emulate vanilla behavior when a PlatThinker is treated as
// a VerticalDoorThinker
//
bool PlatThinker::reTriggerVerticalDoor(bool player)
{
   if(!demo_compatibility)
      return false;

   if(wait == plat_down)
      wait = plat_up;
   else
   {
      if(!player)
         return false;

      wait = plat_down;
   }

   return true;
}


//
// EV_DoPlat
//
// Handle Plat linedef types
//
// Passed the linedef that activated the plat, the type of plat action,
// and for some plat types, an amount to raise
// Returns true if a thinker is started, or restarted from stasis
//
bool EV_DoPlat(const line_t *line, plattype_e type, int amount )
{
   PlatThinker *plat;
   int          secnum;
   bool         rtn;
   sector_t    *sec;
   
   secnum = -1;
   rtn    = false;

   // Activate all <type> plats that are in_stasis
   switch(type)
   {
   case perpetualRaise:
      PlatThinker::ActivateInStasis(line->tag);
      break;
      
   case toggleUpDn:
      PlatThinker::ActivateInStasis(line->tag);
      rtn = true;
      break;
      
   default:
      break;
   }
      
   // act on all sectors tagged the same as the activating linedef
   while((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
   {
      sec = &sectors[secnum];
      
      // don't start a second floor function if already moving
      if(P_SectorActive(floor_special,sec)) //jff 2/23/98 multiple thinkers
         continue;
      
      // Create a thinker
      rtn = true;
      plat = new PlatThinker;
      plat->addThinker();
      
      plat->type   = type;
      plat->crush  = -1;
      plat->tag    = line->tag;
      plat->sector = sec;
      plat->sector->floordata = plat; //jff 2/23/98 multiple thinkers

      //jff 1/26/98 Avoid raise plat bouncing a head off a ceiling and then
      //going down forever -- default low to plat height when triggered
      plat->low = sec->floorheight;
      
      // set up plat according to type  
      switch(type)
      {
      case raiseToNearestAndChange:
         plat->speed   = PLATSPEED/2;
         sec->floorpic = sides[line->sidenum[0]].sector->floorpic;
         plat->high    = P_FindNextHighestFloor(sec,sec->floorheight);
         plat->wait    = 0;
         plat->status  = PlatThinker::up;
         //jff 3/14/98 clear old field as well
         P_ZeroSectorSpecial(sec);
         P_PlatSequence(plat->sector, "EEPlatRaise"); // haleyjd
         break;
          
      case raiseAndChange:
         plat->speed   = PLATSPEED/2;
         sec->floorpic = sides[line->sidenum[0]].sector->floorpic;
         plat->high    = sec->floorheight + amount*FRACUNIT;
         plat->wait    = 0;
         plat->status  = PlatThinker::up;
         
         P_PlatSequence(plat->sector, "EEPlatRaise"); // haleyjd
         break;
          
      case downWaitUpStay:
         plat->speed = PLATSPEED * 4;
         plat->low   = P_FindLowestFloorSurrounding(sec);
         
         if(plat->low > sec->floorheight)
            plat->low = sec->floorheight;
         
         plat->high   = sec->floorheight;
         plat->wait   = 35*PLATWAIT;
         plat->status = PlatThinker::down;
         P_PlatSequence(plat->sector, "EEPlatNormal"); // haleyjd
         break;
          
      case blazeDWUS:
         plat->speed = PLATSPEED * 8;
         plat->low   = P_FindLowestFloorSurrounding(sec);
         
         if(plat->low > sec->floorheight)
            plat->low = sec->floorheight;
         
         plat->high   = sec->floorheight;
         plat->wait   = 35*PLATWAIT;
         plat->status = PlatThinker::down;
         P_PlatSequence(plat->sector, "EEPlatNormal"); // haleyjd
         break;
          
      case perpetualRaise:
         plat->speed = PLATSPEED;
         plat->low   = P_FindLowestFloorSurrounding(sec);
         
         if(plat->low > sec->floorheight)
            plat->low = sec->floorheight;
         
         plat->high = P_FindHighestFloorSurrounding(sec);
         
         if(plat->high < sec->floorheight)
            plat->high = sec->floorheight;
         
         plat->wait   = 35*PLATWAIT;
         plat->status = (P_Random(pr_plats) & 1) ? PlatThinker::down : PlatThinker::up;
         
         P_PlatSequence(plat->sector, "EEPlatNormal"); // haleyjd
         break;

      case toggleUpDn: //jff 3/14/98 add new type to support instant toggle
         plat->speed = PLATSPEED;   //not used
         plat->wait  = 35*PLATWAIT; //not used
         plat->crush = 10;          //jff 3/14/98 crush anything in the way
         
         // set up toggling between ceiling, floor inclusive
         plat->low    = sec->ceilingheight;
         plat->high   = sec->floorheight;
         plat->status = PlatThinker::down;

         P_PlatSequence(plat->sector, "EEPlatSilent");
         break;
         
      default:
         break;
      }

      plat->addActivePlat();  // add plat to list of active plats
   }

   return rtn;
}

//
// EV_DoParamPlat
//
// haleyjd 02/18/13: Implements parameterized plat types for Hexen
// support.
//
bool EV_DoParamPlat(const line_t *line, const int *args, paramplattype_e type)
{
   sector_t *sec    = NULL;
   int       secnum = -1;
   bool      manual = false;
   bool      rtn    = false;

   if(!args[0])
   {
      if(!line || !line->backsector)
         return false;
      sec    = line->backsector;
      secnum = static_cast<int>(sec - sectors);
      manual = true;
      goto manual_plat;
   }

   while((secnum = P_FindSectorFromTag(args[0], secnum)) >= 0)
   {
      sec = &sectors[secnum];

manual_plat:
      if(P_SectorActive(floor_special, sec))
      {
         if(manual)
            return false;
         continue;
      }

      rtn = true;
      PlatThinker *plat = new PlatThinker;
      plat->addThinker();

      plat->crush  = -1;
      plat->tag    = args[0];
      plat->speed  = args[1] * FRACUNIT / 8;
      plat->wait   = args[2];
      plat->sector = sec;
      plat->sector->floordata = plat;

      switch(type)
      {
      case paramDownWaitUpStay:
         plat->type   = downWaitUpStay;
         plat->status = PlatThinker::down;
         plat->high   = sec->floorheight;
         plat->low    = P_FindLowestFloorSurrounding(sec) + 8 * FRACUNIT;
         if(plat->low > sec->floorheight)
            plat->low = sec->floorheight;
         break;

      case paramDownByValueWaitUpStay:
         plat->type   = downWaitUpStay;
         plat->status = PlatThinker::down;
         plat->high   = sec->floorheight;
         plat->low    = sec->floorheight - args[3] * 8 * FRACUNIT;
         if(plat->low > sec->floorheight)
            plat->low = sec->floorheight;
         break;
      
      case paramUpWaitDownStay:
         plat->type   = upWaitDownStay;
         plat->status = PlatThinker::up;
         plat->low    = sec->floorheight;
         plat->high   = P_FindHighestFloorSurrounding(sec);
         if(plat->high < sec->floorheight)
            plat->high = sec->floorheight;
         break;
      
      case paramUpByValueWaitDownStay:
         plat->type   = upWaitDownStay;
         plat->status = PlatThinker::up;
         plat->low    = sec->floorheight;
         plat->high   = sec->floorheight + args[3] * 8 * FRACUNIT;
         if(plat->high < sec->floorheight)
            plat->high = sec->floorheight;
         break;
      
      case paramPerpetualRaise:
         plat->type   = perpetualRaise;
         plat->status = (P_Random(pr_plats) & 1) ? PlatThinker::down : PlatThinker::up;
         plat->low    = P_FindLowestFloorSurrounding(sec) + 8 * FRACUNIT;
         plat->high   = P_FindHighestFloorSurrounding(sec);
         if(plat->low > sec->floorheight)
            plat->low = sec->floorheight;
         if(plat->high < sec->floorheight)
            plat->high = sec->floorheight;
         break;

      default:
         break;
      }

      plat->addActivePlat();
      P_PlatSequence(sec, "EEPlatNormal");

      if(manual)
         return rtn;
   }

   return rtn;
}

// The following were all rewritten by Lee Killough to use the new structure 
// which places no limits on active plats. It also avoids spending as much
// time searching for active plats. Previously a fixed-size array was used,
// with NULL indicating empty entries, while now a doubly-linked list is used.

//
// P_ActivateInStasis()
//
// Activate a plat that has been put in stasis 
// (stopped perpetual floor, instant floor/ceil toggle)
//
// Passed the tag of the plat that should be reactivated
// Returns nothing
//
void PlatThinker::ActivateInStasis(int tag)
{
   // search the active plats
   for(platlist_t *pl = activeplats; pl; pl = pl->next)
   {
      PlatThinker *plat = pl->plat;   // for one in stasis with right tag
      if(plat->tag == tag && plat->status == in_stasis) 
      {
         if(plat->type == toggleUpDn) //jff 3/14/98 reactivate toggle type
            plat->status = plat->oldstatus == up ? down : up;
         else
            plat->status = plat->oldstatus;
      }
   }
}

//
// EV_StopPlatByTag
//
// Handler for "stop perpetual floor" linedef type
//
// Passed the linedef that stopped the plat
// Returns true if a plat was put in stasis
//
// jff 2/12/98 added int return value, fixed return
//
bool EV_StopPlatByTag(int tag)
{
   // search the active plats
   for(platlist_t *pl = activeplats; pl; pl = pl->next)
   {
      PlatThinker *plat = pl->plat;      // for one with the tag not in stasis
      if(plat->status != PlatThinker::in_stasis && plat->tag == tag)
      {
         // put it in stasis
         plat->oldstatus = plat->status; 
         plat->status    = PlatThinker::in_stasis;
      }
   }

   return true;
}

//
// EV_StopPlat
//
// haleyjd 02/18/13: Refactored to call common implementation above.
//
bool EV_StopPlat(const line_t *line)
{
   return EV_StopPlatByTag(line->tag);
}

//
// P_AddActivePlat
//
// Add a plat to the head of the active plat list
//
// Passed a pointer to the plat to add
// Returns nothing
//
void PlatThinker::addActivePlat()
{
   list = estructalloc(platlist_t, 1);
   list->plat = this;
   if((list->next = activeplats))
      list->next->prev = &list->next;
   list->prev = &activeplats;
   activeplats = list;
}

//
// P_RemoveActivePlat
//
// Remove a plat from the active plat list
//
// Passed a pointer to the plat to remove
// Returns nothing
//
void PlatThinker::removeActivePlat()
{
   sector->floordata = NULL; //jff 2/23/98 multiple thinkers
   removeThinker();
   if((*list->prev = list->next))
      list->next->prev = list->prev;
   efree(list);
}

//
// P_RemoveAllActivePlats
//
// Remove all plats from the active plat list
//
// Passed nothing, returns nothing
//
void PlatThinker::RemoveAllActivePlats()
{
   while(activeplats)
   {  
      platlist_t *next = activeplats->next;
      efree(activeplats);
      activeplats = next;
   }
}

//----------------------------------------------------------------------------
//
// $Log: p_plats.c,v $
// Revision 1.16  1998/05/08  17:44:18  jim
// formatted/documented p_plats
//
// Revision 1.15  1998/05/03  23:11:15  killough
// Fix #includes at the top, nothing else
//
// Revision 1.14  1998/03/29  21:45:45  jim
// Fixed lack of switch action on second instant toggle activation
//
// Revision 1.13  1998/03/15  14:40:06  jim
// added pure texture change linedefs & generalized sector types
//
// Revision 1.12  1998/03/14  17:19:22  jim
// Added instant toggle floor type
//
// Revision 1.11  1998/02/23  23:46:59  jim
// Compatibility flagged multiple thinker support
//
// Revision 1.9  1998/02/17  06:06:01  killough
// Make activeplats global for savegame, change RNG calls
//
// Revision 1.8  1998/02/13  03:28:45  jim
// Fixed W1,G1 linedefs clearing untriggered special, cosmetic changes
//
// Revision 1.6  1998/02/02  13:39:58  killough
// Progam beautification
//
// Revision 1.5  1998/01/30  14:44:08  jim
// Added gun exits, right scrolling walls and ceiling mover specials
//
// Revision 1.3  1998/01/27  16:20:42  jim
// Fixed failure to set plat->low when a raise reversed direction
//
// Revision 1.2  1998/01/26  19:24:17  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:00  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------

