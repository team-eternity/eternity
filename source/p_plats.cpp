// Emacs style mode select   -*- C++ -*- vi:sw=3 ts=3:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2006 James Haley
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
//  Plats (i.e. elevator platforms) code, raising/lowering.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "doomstat.h"
#include "m_random.h"
#include "r_main.h"
#include "p_info.h"
#include "p_spec.h"
#include "p_tick.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "sounds.h"
#include "d_gi.h"

// [CG] Added.
#include "cs_netid.h"
#include "cs_spec.h"
#include "cs_main.h"
#include "cl_main.h"
#include "sv_main.h"

platlist_t *activeplats;       // killough 2/14/98: made global again

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
      S_StartSectorSequenceName(s, seqname, false);
}

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
void CPlat::Think()
{
   result_e      res;

   // handle plat moving, up, down, waiting, or in stasis,
   switch(this->status)
   {
   case up: // plat moving up
      res = T_MovePlane(this->sector,this->speed,this->high,this->crush,0,1);

      // if a pure raise type, make the plat moving sound
      // haleyjd: now handled through sound sequences
      
      // if encountered an obstacle, and not a crush type, reverse direction
      if(res == crushed && this->crush <= 0)
      {
         this->count = this->wait;
         this->status = down;
         P_PlatSequence(this->sector, "EEPlatNormal"); // haleyjd
      }
      else  // else handle reaching end of up stroke
      {
         if(res == pastdest) // end of stroke
         {
            S_StopSectorSequence(this->sector, false); // haleyjd

            // if not an instant toggle type, wait, make plat stop sound
            if(this->type != toggleUpDn)
            {
               this->count = this->wait;
               this->status = waiting;
            }
            else // else go into stasis awaiting next toggle activation
            {
               this->oldstatus = this->status;//jff 3/14/98 after action wait  
               this->status = in_stasis;      //for reactivation of toggle
            }
                                        
            // lift types and pure raise types are done at end of up stroke
            // only the perpetual type waits then goes back up
            switch(this->type)
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
               // [CG] Just set the platform as inactive if we're a client.
               if(CS_CLIENT)
                  inactive = cl_current_world_index;
               else
                  P_RemoveActivePlat(this);     // killough
            default:
               break;
            }
         }
      }
      break;
        
   case down: // plat moving down
      res = T_MovePlane(this->sector, this->speed, this->low, -1, 0, -1);

      // handle reaching end of down stroke
      // SoM: attached sectors means the plat can crush when heading down too
      // if encountered an obstacle, and not a crush type, reverse direction
      if(demo_version >= 333 && res == crushed && this->crush <= 0)
      {
         this->count = this->wait;
         this->status = up;
         P_PlatSequence(this->sector, "EEPlatNormal"); // haleyjd
      }
      else if(res == pastdest)
      {
         S_StopSectorSequence(this->sector, false); // haleyjd

         // if not an instant toggle, start waiting, make plat stop sound
         if(this->type!=toggleUpDn) //jff 3/14/98 toggle up down
         {                           // is silent, instant, no waiting
            this->count = this->wait;
            this->status = waiting;
         }
         else // instant toggles go into stasis awaiting next activation
         {
            this->oldstatus = this->status;//jff 3/14/98 after action wait  
            this->status = in_stasis;      //for reactivation of toggle
         }

         //jff 1/26/98 remove the plat if it bounced so it can be tried again
         //only affects plats that raise and bounce
         //killough 1/31/98: relax compatibility to demo_compatibility
         
         // remove the plat if its a pure raise type
         if(demo_version < 203 ? !demo_compatibility : !comp[comp_floors])
         {
            switch(this->type)
            {
            case raiseAndChange:
            case raiseToNearestAndChange:
               // [CG] Just set the platform as inactive if we're a client.
               if(CS_CLIENT)
                  inactive = cl_current_world_index;
               else
                  P_RemoveActivePlat(this);
            default:
               break;
            }
         }
      }
      break;

   case waiting: // plat is waiting
      if(!--this->count)  // downcount and check for delay elapsed
      {
         if(this->sector->floorheight == this->low)
            this->status = up;     // if at bottom, start up
         else
            this->status = down;   // if at top, start down
         
         // make plat start sound
         // haleyjd: changed for sound sequences
         if(this->type == toggleUpDn)
            P_PlatSequence(this->sector, "EEPlatSilent");
         else
            P_PlatSequence(this->sector, "EEPlatNormal");
      }
      break; //jff 1/27/98 don't pickup code added later to in_stasis

   case in_stasis: // do nothing if in stasis
      break;
   }
}

void P_CopyPlatform(CPlat *dest, CPlat *src)
{
   dest->speed     = src->speed;
   dest->low       = src->low;
   dest->high      = src->high;
   dest->wait      = src->wait;
   dest->count     = src->count;
   dest->status    = src->status;
   dest->oldstatus = src->oldstatus;
   dest->crush     = src->crush;
   dest->tag       = src->tag;
   dest->type      = src->type;
   dest->net_id    = src->net_id;
}

boolean P_PlatformsEqual(CPlat *platform_one, CPlat *platform_two)
{
   if(platform_one->speed     == platform_two->speed     &&
      platform_one->low       == platform_two->low       &&
      platform_one->high      == platform_two->high      &&
      platform_one->wait      == platform_two->wait      &&
      platform_one->count     == platform_two->count     &&
      platform_one->status    == platform_two->status    &&
      platform_one->oldstatus == platform_two->oldstatus &&
      platform_one->crush     == platform_two->crush     &&
      platform_one->tag       == platform_two->tag       &&
      platform_one->type      == platform_two->type)
   {
      return true;
   }
   return false;
}

void P_PrintPlatform(CPlat *platform)
{
   unsigned int index;

   if(CS_SERVER && playeringame[1])
   {
      index = server_clients[1].last_command_run_index;
   }
   else if(CS_CLIENT)
   {
      index = cl_current_world_index;
   }
   else if(CS_SERVER)
   {
      index = sv_world_index;
   }

   printf(
      "Platform %2u (%3u): %5u %5u %5u %3d %3d %2d %2d %2d %2d %2d %1d | ",
      platform->net_id,
      index,
      platform->speed >> FRACBITS,
      platform->low >> FRACBITS,
      platform->high >> FRACBITS,
      platform->wait,
      platform->count,
      platform->status,
      platform->oldstatus,
      platform->crush,
      platform->tag,
      platform->type,
      platform->inactive
   );

   if(platform->sector && ((platform->sector - sectors) < numsectors))
   {
      printf(
         "Sector %2u: %5u %5u.\n",
         platform->sector - sectors,
         platform->sector->ceilingheight >> FRACBITS,
         platform->sector->floorheight   >> FRACBITS
      );
   }
   else
   {
      printf("Sector XX (%5u): XXXXX XXXXX.\n", index);
   }
}

CPlat* P_SpawnPlatform(line_t *line, sector_t *sec, int amount,
                        plattype_e type)
{
   CPlat *plat = new CPlat;

   plat->addThinker();
   plat->type = type;
   plat->sector = sec;
   plat->sector->floordata = plat; //jff 2/23/98 multiple thinkers
   plat->crush = -1;
   plat->tag = line->tag;

   //jff 1/26/98 Avoid raise plat bouncing a head off a ceiling and then
   //going down forever -- default low to plat height when triggered
   plat->low = sec->floorheight;

   // set up plat according to type
   switch(type)
   {
   case raiseToNearestAndChange:
      plat->speed = PLATSPEED/2;
      sec->floorpic = sides[line->sidenum[0]].sector->floorpic;
      plat->high = P_FindNextHighestFloor(sec,sec->floorheight);
      plat->wait = 0;
      plat->status = up;
      //jff 3/14/98 clear old field as well
      P_ZeroSectorSpecial(sec);
      P_PlatSequence(plat->sector, "EEPlatRaise"); // haleyjd
      break;

   case raiseAndChange:
      plat->speed = PLATSPEED / 2;
      sec->floorpic = sides[line->sidenum[0]].sector->floorpic;
      plat->high = sec->floorheight + amount * FRACUNIT;
      plat->wait = 0;
      plat->status = up;
      
      P_PlatSequence(plat->sector, "EEPlatRaise"); // haleyjd
      break;

   case downWaitUpStay:
      plat->speed = PLATSPEED * 4;
      plat->low = P_FindLowestFloorSurrounding(sec);

      if(plat->low > sec->floorheight)
         plat->low = sec->floorheight;

      plat->high = sec->floorheight;
      plat->wait = 35 * PLATWAIT;
      plat->status = down;
      P_PlatSequence(plat->sector, "EEPlatNormal"); // haleyjd
      break;

   case blazeDWUS:
      plat->speed = PLATSPEED * 8;
      plat->low = P_FindLowestFloorSurrounding(sec);

      if(plat->low > sec->floorheight)
         plat->low = sec->floorheight;

      plat->high = sec->floorheight;
      plat->wait = 35 * PLATWAIT;
      plat->status = down;
      P_PlatSequence(plat->sector, "EEPlatNormal"); // haleyjd
      break;

   case perpetualRaise:
      plat->speed = PLATSPEED;
      plat->low = P_FindLowestFloorSurrounding(sec);

      if(plat->low > sec->floorheight)
         plat->low = sec->floorheight;

      plat->high = P_FindHighestFloorSurrounding(sec);

      if(plat->high < sec->floorheight)
         plat->high = sec->floorheight;

      plat->wait = 35 * PLATWAIT;
      plat->status = P_Random(pr_plats) & 1;

      P_PlatSequence(plat->sector, "EEPlatNormal"); // haleyjd

      break;

   case toggleUpDn: //jff 3/14/98 add new type to support instant toggle
      plat->speed = PLATSPEED;   //not used
      plat->wait  = 35 * PLATWAIT; //not used
      plat->crush = 10;          //jff 3/14/98 crush anything in the way

      // set up toggling between ceiling, floor inclusive
      plat->low    = sec->ceilingheight;
      plat->high   = sec->floorheight;
      plat->status = down;

      P_PlatSequence(plat->sector, "EEPlatSilent");
      break;

   default:
      break;
   }

   if(serverside)
   {
      P_AddActivePlat(plat);  // add plat to list of active plats
      if(CS_SERVER)
         SV_BroadcastMapSpecialSpawned(plat, NULL, line, ms_platform);
   }
   return plat;
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
int EV_DoPlat(line_t *line, plattype_e type, int amount)
{
   CPlat   *plat;
   sector_t *sec;
   int rtn = 0;
   int secnum = -1;

   // Activate all <type> plats that are in_stasis
   switch(type)
   {
   case perpetualRaise:
      P_ActivateInStasis(line->tag);
      break;

   case toggleUpDn:
      P_ActivateInStasis(line->tag);
      rtn = 1;
      if(CS_CLIENT)
         return rtn;
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
      rtn = 1;

      // [CG] Clients don't spawn platforms themselves when lines are
      //      activated, but we have to inform the caller whether or not a
      //      thinker would have been created regardless.
      if(CS_CLIENT)
         return rtn;
      plat = P_SpawnPlatform(line, sec, amount, type);
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
void P_ActivateInStasis(int tag)
{
   platlist_t *pl;
   for(pl = activeplats; pl; pl = pl->next)   // search the active plats
   {
      CPlat *plat = pl->plat;              // for one in stasis with right tag
      if(plat->tag == tag && plat->status == in_stasis) 
      {
         if(plat->type==toggleUpDn) //jff 3/14/98 reactivate toggle type
            plat->status = plat->oldstatus==up? down : up;
         else
            plat->status = plat->oldstatus;
      }
   }
}

//
// EV_StopPlat()
//
// Handler for "stop perpetual floor" linedef type
//
// Passed the linedef that stopped the plat
// Returns true if a plat was put in stasis
//
// jff 2/12/98 added int return value, fixed return
//
int EV_StopPlat(line_t *line)
{
   platlist_t *pl;
   for(pl = activeplats; pl; pl = pl->next)  // search the active plats
   {
      CPlat *plat = pl->plat;             // for one with the tag not in stasis
      if(plat->status != in_stasis && plat->tag == line->tag)
      {
         plat->oldstatus = plat->status;    // put it in stasis
         plat->status = in_stasis;
      }
   }
   return 1;
}

//
// P_AddActivePlat()
//
// Add a plat to the head of the active plat list
//
// Passed a pointer to the plat to add
// Returns nothing
//
void P_AddActivePlat(CPlat *plat)
{
   CS_ObtainPlatformNetID(plat);
}

void oldP_AddActivePlat(CPlat *plat)
{
   platlist_t *list = malloc(sizeof *list);
   list->plat = plat;
   plat->list = list;
   if((list->next = activeplats))
      list->next->prev = &list->next;
   list->prev = &activeplats;
   activeplats = list;
}

//
// P_RemoveActivePlat()
//
// Remove a plat from the active plat list
//
// Passed a pointer to the plat to remove
// Returns nothing
//
void P_RemoveActivePlat(CPlat *plat)
{
   plat->sector->floordata = NULL; //jff 2/23/98 multiple thinkers
   P_RemoveThinker(&plat->thinker);
   if(CS_SERVER)
      SV_BroadcastMapSpecialRemoved(plat->net_id, ms_platform);
   CS_ReleasePlatformNetID(plat);
}

void oldP_RemoveActivePlat(CPlat *plat)
{
   platlist_t *list = plat->list;
   plat->sector->floordata = NULL; //jff 2/23/98 multiple thinkers
   plat->removeThinker();
   if((*list->prev = list->next))
      list->next->prev = list->prev;
   free(list);
}

//
// P_RemoveAllActivePlats()
//
// Remove all plats from the active plat list
//
// Passed nothing, returns nothing
//
void P_RemoveAllActivePlats(void)
{
   CS_ResetPlatformNetIDs();
}

void oldP_RemoveAllActivePlats(void)
{
   while(activeplats)
   {  
      platlist_t *next = activeplats->next;
      free(activeplats);
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

