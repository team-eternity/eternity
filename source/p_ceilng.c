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
//   Ceiling aninmation (lowering, crushing, raising)
//
//-----------------------------------------------------------------------------

#include "doomstat.h"
#include "r_main.h"
#include "p_info.h"
#include "p_spec.h"
#include "p_tick.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "sounds.h"

// the list of ceilings moving currently, including crushers
ceilinglist_t *activeceilings;

//
// P_CeilingSequence
//
// haleyjd 09/27/06: Starts the appropriate sound sequence for a ceiling action.
//
void P_CeilingSequence(sector_t *s, int noiseLevel)
{
   if(silentmove(s))
      return;

   if(s->sndSeqID >= 0)
      S_StartSectorSequence(s, SEQ_CEILING);
   else
   {
      switch(noiseLevel)
      {
      case CNOISE_NORMAL:
         S_StartSectorSequenceName(s, "EECeilingNormal", true);
         break;
      case CNOISE_SEMISILENT:
         S_StartSectorSequenceName(s, "EECeilingSemiSilent", true);
         break;
      case CNOISE_SILENT:
         S_StartSectorSequenceName(s, "EECeilingSilent", true);
         break;
      }
   }
}

//
// P_SetSectorCeilingPic
//
// haleyjd 08/30/09: Call this routine to set a sector's ceiling pic.
//
void P_SetSectorCeilingPic(sector_t *sector, int pic)
{
   // clear sky flag
   sector->intflags &= ~SIF_SKY;

   sector->ceilingpic = pic;

   // reset the sky flag
   if(pic == skyflatnum || pic == sky2flatnum)
      sector->intflags |= SIF_SKY;
}

/////////////////////////////////////////////////////////////////
//
// Ceiling action routine and linedef type handler
//
/////////////////////////////////////////////////////////////////

//
// T_MoveCeiling
//
// Action routine that moves ceilings. Called once per tick.
//
// Passed a ceiling_t structure that contains all the info about the move.
// see P_SPEC.H for fields. No return value.
//
// jff 02/08/98 all cases with labels beginning with gen added to support 
// generalized line type behaviors.
//
void T_MoveCeiling(ceiling_t *ceiling)
{
   result_e  res;
   
   switch(ceiling->direction)
   {
   case plat_stop:
      // If ceiling in stasis, do nothing
      break;

   case plat_up:
      // Ceiling is moving up
      res = T_MovePlane
            (
               ceiling->sector,
               ceiling->speed,
               ceiling->topheight,
               -1,
               1,
               ceiling->direction
            );

      // if not a silent crusher, make moving sound
      // haleyjd: now handled through sound sequences

      // handle reaching destination height
      if(res == pastdest)
      {
         switch(ceiling->type)
         {
            // plain movers are just removed
         case raiseToHighest:
         case genCeiling:
            P_RemoveActiveCeiling(ceiling);
            break;

            // movers with texture change, change the texture then get removed
         case genCeilingChgT:
         case genCeilingChg0:
            //jff 3/14/98 transfer old special field as well
            P_TransferSectorSpecial(ceiling->sector, &(ceiling->special));
         case genCeilingChg:
            P_SetSectorCeilingPic(ceiling->sector, ceiling->texture);
            P_RemoveActiveCeiling(ceiling);
            break;

            // crushers reverse direction at the top
         case silentCrushAndRaise:
            // haleyjd: if not playing a looping sequence, start one
            if(!S_CheckSectorSequenceLoop(ceiling->sector, true))
               P_CeilingSequence(ceiling->sector, CNOISE_SEMISILENT);
         case genSilentCrusher:
         case genCrusher:
         case fastCrushAndRaise:
         case crushAndRaise:
            ceiling->direction = plat_down;
            break;
            
         default:
            break;
         }
      }
      break;
  
   case plat_down:
      // Ceiling moving down
      res = T_MovePlane
            (
               ceiling->sector,
               ceiling->speed,
               ceiling->bottomheight,
               ceiling->crush,
               1,
               ceiling->direction
            );

      // if not silent crusher type make moving sound
      // haleyjd: now handled through sound sequences

      // handle reaching destination height
      if(res == pastdest)
      {
         switch(ceiling->type)
         {
            // 02/09/98 jff change slow crushers' speed back to normal
            // start back up
         case genSilentCrusher:
         case genCrusher:
            if(ceiling->oldspeed < CEILSPEED*3)
               ceiling->speed = ceiling->oldspeed;
            ceiling->direction = plat_up; //jff 2/22/98 make it go back up!
            break;
            
            // make platform stop at bottom of all crusher strokes
            // except generalized ones, reset speed, start back up
         case silentCrushAndRaise:
            // haleyjd: if not playing a looping sequence, start one
            if(!S_CheckSectorSequenceLoop(ceiling->sector, true))
               P_CeilingSequence(ceiling->sector, CNOISE_SEMISILENT);
         case crushAndRaise: 
            ceiling->speed = CEILSPEED;
         case fastCrushAndRaise:
            ceiling->direction = plat_up;
            break;
            
            // in the case of ceiling mover/changer, change the texture
            // then remove the active ceiling
         case genCeilingChgT:
         case genCeilingChg0:
            //jff add to fix bug in special transfers from changes
            P_TransferSectorSpecial(ceiling->sector, &(ceiling->special));
         case genCeilingChg:
            P_SetSectorCeilingPic(ceiling->sector, ceiling->texture);
            P_RemoveActiveCeiling(ceiling);
            break;

            // all other case, just remove the active ceiling
         case lowerAndCrush:
         case lowerToFloor:
         case lowerToLowest:
         case lowerToMaxFloor:
         case genCeiling:
            P_RemoveActiveCeiling(ceiling);
            break;
            
         default:
            break;
         }
      }
      else // ( res != pastdest )
      {
         // handle the crusher encountering an obstacle
         if(res == crushed)
         {
            switch(ceiling->type)
            {
               //jff 02/08/98 slow down slow crushers on obstacle
            case genCrusher:  
            case genSilentCrusher:
               if(ceiling->oldspeed < CEILSPEED*3)
                  ceiling->speed = CEILSPEED / 8;
               break;
            case silentCrushAndRaise:
            case crushAndRaise:
            case lowerAndCrush:
               ceiling->speed = CEILSPEED / 8;
               break;
               
            default:
               break;
            }
         }
      } // end else
      break;
   } // end switch
}


//
// EV_DoCeiling
//
// Move a ceiling up/down or start a crusher
//
// Passed the linedef activating the function and the type of function desired
// returns true if a thinker started
//
int EV_DoCeiling(line_t *line, ceiling_e type)
{
   int       secnum = -1;
   int       rtn = 0;
   int       noise = CNOISE_NORMAL; // haleyjd 09/28/06
   sector_t  *sec;
   ceiling_t *ceiling;
      
   // Reactivate in-stasis ceilings...for certain types.
   // This restarts a crusher after it has been stopped
   switch(type)
   {
   case fastCrushAndRaise:
   case silentCrushAndRaise:
   case crushAndRaise:
      //jff 4/5/98 return if activated
      rtn = P_ActivateInStasisCeiling(line);
   default:
      break;
   }
  
   // affects all sectors with the same tag as the linedef
   while((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
   {
      sec = &sectors[secnum];
      
      // if ceiling already moving, don't start a second function on it
      if(P_SectorActive(ceiling_special, sec))  //jff 2/22/98
         continue;
  
      // create a new ceiling thinker
      rtn = 1;
      ceiling = Z_Calloc(1, sizeof(*ceiling), PU_LEVSPEC, 0);
      P_AddThinker (&ceiling->thinker);
      sec->ceilingdata = ceiling;               //jff 2/22/98
      ceiling->thinker.function = T_MoveCeiling;
      ceiling->sector = sec;
      ceiling->crush = -1;
  
      // setup ceiling structure according to type of function
      switch(type)
      {
      case fastCrushAndRaise:
         ceiling->crush = 10;
         ceiling->topheight = sec->ceilingheight;
         ceiling->bottomheight = sec->floorheight + (8*FRACUNIT);
         ceiling->direction = plat_down;
         ceiling->speed = CEILSPEED * 2;
         break;

      case silentCrushAndRaise:
         noise = CNOISE_SEMISILENT;
      case crushAndRaise:
         ceiling->crush = 10;
         ceiling->topheight = sec->ceilingheight;
      case lowerAndCrush:
      case lowerToFloor:
         ceiling->bottomheight = sec->floorheight;
         if(type != lowerToFloor)
            ceiling->bottomheight += 8*FRACUNIT;
         ceiling->direction = plat_down;
         ceiling->speed = CEILSPEED;
         break;

      case raiseToHighest:
         ceiling->topheight = P_FindHighestCeilingSurrounding(sec);
         ceiling->direction = plat_up;
         ceiling->speed = CEILSPEED;
         break;
         
      case lowerToLowest:
         ceiling->bottomheight = P_FindLowestCeilingSurrounding(sec);
         ceiling->direction = plat_down;
         ceiling->speed = CEILSPEED;
         break;
         
      case lowerToMaxFloor:
         ceiling->bottomheight = P_FindHighestFloorSurrounding(sec);
         ceiling->direction = plat_down;
         ceiling->speed = CEILSPEED;
         break;
         
      default:
         break;
      }
    
      // add the ceiling to the active list
      ceiling->tag = sec->tag;
      ceiling->type = type;
      P_AddActiveCeiling(ceiling);

      // haleyjd 09/28/06: sound sequences
      P_CeilingSequence(ceiling->sector, noise);
   }
   return rtn;
}

//////////////////////////////////////////////////////////////////////
//
// Active ceiling list primitives
//
/////////////////////////////////////////////////////////////////////

// jff 2/22/98 - modified Lee's plat code to work for ceilings
//
// The following were all rewritten by Lee Killough
// to use the new structure which places no limits
// on active ceilings. It also avoids spending as much
// time searching for active ceilings. Previously a 
// fixed-size array was used, with NULL indicating
// empty entries, while now a doubly-linked list
// is used.

//
// P_ActivateInStasisCeiling()
//
// Reactivates all stopped crushers with the right tag
//
// Passed the line reactivating the crusher
// Returns true if a ceiling reactivated
//
//jff 4/5/98 return if activated
//
int P_ActivateInStasisCeiling(line_t *line)
{
   ceilinglist_t *cl;
   int rtn = 0, noise;
   
   for(cl = activeceilings; cl; cl = cl->next)
   {
      ceiling_t *ceiling = cl->ceiling;
      if(ceiling->tag == line->tag && ceiling->direction == 0)
      {
         ceiling->direction = ceiling->olddirection;
         ceiling->thinker.function = T_MoveCeiling;

         // haleyjd: restart sound sequence
         switch(ceiling->type)
         {
         case silentCrushAndRaise:
            noise = CNOISE_SEMISILENT;
            break;
         case genSilentCrusher:
            noise = CNOISE_SILENT;
            break;
         default:
            noise = CNOISE_NORMAL;
            break;
         }
         P_CeilingSequence(ceiling->sector, noise);

         //jff 4/5/98 return if activated
         rtn = 1;
      }
   }
   return rtn;
}

//
// EV_CeilingCrushStop()
//
// Stops all active ceilings with the right tag
//
// Passed the linedef stopping the ceilings
// Returns true if a ceiling put in stasis
//
int EV_CeilingCrushStop(line_t* line)
{
   int rtn = 0;
   
   ceilinglist_t *cl;
   for(cl = activeceilings; cl; cl = cl->next)
   {
      ceiling_t *ceiling = cl->ceiling;
      if(ceiling->direction != plat_stop && ceiling->tag == line->tag)
      {
         ceiling->olddirection = ceiling->direction;
         ceiling->direction = plat_stop;
         ceiling->thinker.function = NULL;
         S_StopSectorSequence(ceiling->sector, true); // haleyjd 09/28/06
         rtn = 1;
      }
   }
   return rtn;
}

//
// P_AddActiveCeiling()
//
// Adds a ceiling to the head of the list of active ceilings
//
// Passed the ceiling motion structure
// Returns nothing
//
void P_AddActiveCeiling(ceiling_t *ceiling)
{
   ceilinglist_t *list = malloc(sizeof *list);
   list->ceiling = ceiling;
   ceiling->list = list;
   if((list->next = activeceilings))
      list->next->prev = &list->next;
   list->prev = &activeceilings;
   activeceilings = list;
}

//
// P_RemoveActiveCeiling()
//
// Removes a ceiling from the list of active ceilings
//
// Passed the ceiling motion structure
// Returns nothing
//
void P_RemoveActiveCeiling(ceiling_t* ceiling)
{
   ceilinglist_t *list = ceiling->list;
   ceiling->sector->ceilingdata = NULL;   //jff 2/22/98
   S_StopSectorSequence(ceiling->sector, true); // haleyjd 09/28/06
   P_RemoveThinker(&ceiling->thinker);
   if((*list->prev = list->next))
      list->next->prev = list->prev;
   free(list);
}

//
// P_RemoveAllActiveCeilings()
//
// Removes all ceilings from the active ceiling list
//
// Passed nothing, returns nothing
//
void P_RemoveAllActiveCeilings(void)
{
   while(activeceilings)
   {  
      ceilinglist_t *next = activeceilings->next;
      free(activeceilings);
      activeceilings = next;
   }
}

//
// P_ChangeCeilingTex
//
// Changes the ceiling flat of all tagged sectors.
//
void P_ChangeCeilingTex(const char *name, int tag)
{
   int flatnum;
   int secnum = -1;

   if((flatnum = R_CheckForFlat(name)) == -1)
      return;

   while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
      P_SetSectorCeilingPic(&sectors[secnum], flatnum);
}

//----------------------------------------------------------------------------
//
// $Log: p_ceilng.c,v $
// Revision 1.14  1998/05/09  10:58:10  jim
// formatted/documented p_ceilng
//
// Revision 1.13  1998/05/03  23:07:43  killough
// Fix #includes at the top, nothing else
//
// Revision 1.12  1998/04/05  13:54:17  jim
// fixed switch change on second activation
//
// Revision 1.11  1998/03/15  14:40:26  jim
// added pure texture change linedefs & generalized sector types
//
// Revision 1.10  1998/02/23  23:46:35  jim
// Compatibility flagged multiple thinker support
//
// Revision 1.9  1998/02/23  00:41:31  jim
// Implemented elevators
//
// Revision 1.7  1998/02/13  03:28:22  jim
// Fixed W1,G1 linedefs clearing untriggered special, cosmetic changes
//
// Revision 1.5  1998/02/08  05:35:18  jim
// Added generalized linedef types
//
// Revision 1.4  1998/01/30  14:44:12  jim
// Added gun exits, right scrolling walls and ceiling mover specials
//
// Revision 1.2  1998/01/26  19:23:56  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:58  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------

