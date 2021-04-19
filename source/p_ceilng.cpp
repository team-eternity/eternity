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
//   Ceiling aninmation (lowering, crushing, raising)
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "doomstat.h"
#include "r_main.h"
#include "p_info.h"
#include "p_saveg.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_data.h"
#include "r_sky.h"
#include "r_state.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "sounds.h"
#include "t_plane.h"

// the list of ceilings moving currently, including crushers
ceilinglist_t *activeceilings;

// ioanch 20160306: vanilla demo compatibility stuff
static const int vanilla_MAXCEILINGS = 30;
static CeilingThinker *vanilla_activeceilings[vanilla_MAXCEILINGS];

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
         S_StartSectorSequenceName(s, "EECeilingNormal", SEQ_ORIGIN_SECTOR_C);
         break;
      case CNOISE_SEMISILENT:
         S_StartSectorSequenceName(s, "EECeilingSemiSilent", SEQ_ORIGIN_SECTOR_C);
         break;
      case CNOISE_SILENT:
         S_StartSectorSequenceName(s, "EECeilingSilent", SEQ_ORIGIN_SECTOR_C);
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

   sector->srf.ceiling.pic = pic;

   // reset the sky flag
   if(R_IsSkyFlat(sector->srf.ceiling.pic))
      sector->intflags |= SIF_SKY;
}

/////////////////////////////////////////////////////////////////
//
// Ceiling action routine and linedef type handler
//
/////////////////////////////////////////////////////////////////

IMPLEMENT_THINKER_TYPE(CeilingThinker)

//
// T_MoveCeiling
//
// Action routine that moves ceilings. Called once per tick.
//
// Passed a CeilingThinker structure that contains all the info about the move.
// see P_SPEC.H for fields. No return value.
//
// jff 02/08/98 all cases with labels beginning with gen added to support 
// generalized line type behaviors.
//
void CeilingThinker::Think()
{
   result_e  res;

   if(inStasis)
      return;

   switch(direction)
   {
   case plat_stop:
      // If ceiling in stasis, do nothing
      break;

   case plat_up:
      // Ceiling is moving up
      res = T_MoveCeilingUp(sector, speed, topheight, -1);

      // if not a silent crusher, make moving sound
      // haleyjd: now handled through sound sequences

      // handle reaching destination height
      if(res == pastdest)
      {
         switch(type)
         {
            // plain movers are just removed
         case raiseToHighest:
         case genCeiling:
         case paramHexenCrushRaiseStay:   // ioanch 20160306
         case paramHexenLowerCrush:
            P_RemoveActiveCeiling(this);
            break;

            // movers with texture change, change the texture then get removed
         case genCeilingChgT:
         case genCeilingChg0:
            //jff 3/14/98 transfer old special field as well
            P_TransferSectorSpecial(sector, &special);
         case genCeilingChg:
            P_SetSectorCeilingPic(sector, texture);
            P_RemoveActiveCeiling(this);
            break;

            // crushers reverse direction at the top
         case silentCrushAndRaise:
            // haleyjd: if not playing a looping sequence, start one
            if(!S_CheckSectorSequenceLoop(sector, SEQ_ORIGIN_SECTOR_C))
               P_CeilingSequence(sector, CNOISE_SEMISILENT);
         case genSilentCrusher:
         case genCrusher:
            // ioanch 20160314: Generic_Crusher support
            if(type != silentCrushAndRaise)
               speed = oldspeed;
         case fastCrushAndRaise:
         case crushAndRaise:
            direction = plat_down;
            break;

         // ioanch 20160305
         case paramHexenCrush:
            // preserve the weird Hexen behaviour where the crusher becomes mute
            // after any pastdest.
            if(P_LevelIsVanillaHexen())
               S_StopSectorSequence(sector, SEQ_ORIGIN_SECTOR_C);
            else if(crushflags & crushSilent &&
               !S_CheckSectorSequenceLoop(sector, SEQ_ORIGIN_SECTOR_C))
            {
               P_CeilingSequence(sector, CNOISE_SEMISILENT);
            }
            direction = plat_down;
            speed = oldspeed;   // restore the speed to the designated DOWN one
            break;
            
         default:
            break;
         }
      }
      break;
  
   case plat_down:
      // Ceiling moving down
      // ioanch 20160305: allow resting
      res = T_MoveCeilingDown(sector, speed, bottomheight, crush, 
         !!(crushflags & crushRest));

      // if not silent crusher type make moving sound
      // haleyjd: now handled through sound sequences

      // handle reaching destination height
      if(res == pastdest)
      {
         switch(this->type)
         {
            // 02/09/98 jff change slow crushers' speed back to normal
            // start back up
         case genSilentCrusher:
         case genCrusher:
            if(oldspeed < CEILSPEED*3)
               speed = this->upspeed;  // ioanch 20160314: use up speed
            direction = plat_up; //jff 2/22/98 make it go back up!
            break;
            
            // make platform stop at bottom of all crusher strokes
            // except generalized ones, reset speed, start back up
         case silentCrushAndRaise:
            // haleyjd: if not playing a looping sequence, start one
            if(!S_CheckSectorSequenceLoop(sector, SEQ_ORIGIN_SECTOR_C))
               P_CeilingSequence(sector, CNOISE_SEMISILENT);
         case crushAndRaise: 
            speed = CEILSPEED;
         case fastCrushAndRaise:
            direction = plat_up;
            break;
            
            // in the case of ceiling mover/changer, change the texture
            // then remove the active ceiling
         case genCeilingChgT:
         case genCeilingChg0:
            //jff add to fix bug in special transfers from changes
            P_TransferSectorSpecial(sector, &special);
         case genCeilingChg:
            P_SetSectorCeilingPic(sector, texture);
            P_RemoveActiveCeiling(this);
            break;

            // all other case, just remove the active ceiling
         case lowerAndCrush:
         case lowerToFloor:
         case lowerToLowest:
         case lowerToMaxFloor:
         case genCeiling:
         case paramHexenLowerCrush:
            P_RemoveActiveCeiling(this);
            break;
         // ioanch 20160305
         case paramHexenCrush:
         case paramHexenCrushRaiseStay:
            // preserve the weird Hexen behaviour where the crusher becomes mute
            // after any pastdest (only in maps for vanilla Hexen).
            if(P_LevelIsVanillaHexen())
               S_StopSectorSequence(sector, SEQ_ORIGIN_SECTOR_C);
            else if(crushflags & crushSilent &&
               !S_CheckSectorSequenceLoop(sector, SEQ_ORIGIN_SECTOR_C))
            {
               P_CeilingSequence(sector, CNOISE_SEMISILENT);
            }

            direction = plat_up;
            // keep old speed in case it was decreased by crushing like Doom.
            speed = upspeed;  // set to the different up speed
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
            switch(type)
            {
               //jff 02/08/98 slow down slow crushers on obstacle
            case genCrusher:  
            case genSilentCrusher:
               if(oldspeed < CEILSPEED*3)
                  speed = CEILSPEED / 8;
               break;
            case silentCrushAndRaise:
            case crushAndRaise:
            case lowerAndCrush:
               speed = CEILSPEED / 8;
               break;
            case paramHexenCrush:
            case paramHexenCrushRaiseStay:
            case paramHexenLowerCrush:
            case genCeiling:  // ioanch: also emulate Doom ceiling fake crush
                              // slowdown
               // if crusher doesn't rest on victims:
               // this is like ZDoom: if a ceiling speed is set exactly to 8,
               // then apply the Doom crusher slowdown. Otherwise, keep speed
               // constant. This may not apply to all crushing specials in
               // ZDoom, but for simplicity it has been applied generally here.
               if(!(crushflags & crushRest) && (crushflags & crushParamSlow))
                  speed = CEILSPEED / 8;
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
// CeilingThinker::serialize
//
// Saves and loads CeilingThinker thinkers.
//
void CeilingThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << type << bottomheight << topheight << speed << oldspeed
       << crush << special << texture << direction << inStasis << tag 
       << olddirection;

   if((arc.saveVersion() >= 3))
      arc << upspeed << crushflags;

   // Reattach to active ceilings list
   if(arc.isLoading())
      P_AddActiveCeiling(this);
}

//
// CeilingThinker::reTriggerVerticalDoor
//
// haleyjd 10/13/2011: emulate vanilla behavior when a CeilingThinker is treated
// as a VerticalDoorThinker
//
bool CeilingThinker::reTriggerVerticalDoor(bool player)
{
   if(!demo_compatibility)
      return false;

   if(speed == plat_down)
      speed = plat_up;
   else
   {
      if(!player)
         return false;

      speed = plat_down;
   }

   return true;
}

//
// EV_DoCeiling
//
// Move a ceiling up/down or start a crusher
//
// Passed the linedef activating the function and the type of function desired
// returns true if a thinker started
//
int EV_DoCeiling(const line_t *line, ceiling_e type)
{
   int       secnum = -1;
   int       rtn = 0;
   int       noise = CNOISE_NORMAL; // haleyjd 09/28/06
   sector_t  *sec;
   CeilingThinker *ceiling;
      
   // Reactivate in-stasis ceilings...for certain types.
   // This restarts a crusher after it has been stopped
   switch(type)
   {
   case fastCrushAndRaise:
   case silentCrushAndRaise:
   case crushAndRaise:
      //jff 4/5/98 return if activated
      rtn = P_ActivateInStasisCeiling(line, line->args[0]);
   default:
      break;
   }
  
   // affects all sectors with the same tag as the linedef
   while((secnum = P_FindSectorFromLineArg0(line,secnum)) >= 0)
   {
      sec = &sectors[secnum];
      
      // if ceiling already moving, don't start a second function on it
      if(P_SectorActive(ceiling_special, sec))  //jff 2/22/98
         continue;
  
      // create a new ceiling thinker
      rtn = 1;
      ceiling = new CeilingThinker;
      ceiling->addThinker();
      sec->srf.ceiling.data = ceiling;               //jff 2/22/98
      ceiling->sector = sec;
      ceiling->crush = -1;
      ceiling->crushflags = 0;   // ioanch 20160305
  
      // setup ceiling structure according to type of function
      switch(type)
      {
      case fastCrushAndRaise:
         ceiling->crush = 10;
         ceiling->topheight = sec->srf.ceiling.height;
         ceiling->bottomheight = sec->srf.floor.height + (8*FRACUNIT);
         ceiling->direction = plat_down;
         ceiling->speed = CEILSPEED * 2;
         break;

      case silentCrushAndRaise:
         noise = CNOISE_SEMISILENT;
      case crushAndRaise:
         ceiling->crush = 10;
         ceiling->topheight = sec->srf.ceiling.height;
      case lowerAndCrush:
      case lowerToFloor:
         ceiling->bottomheight = sec->srf.floor.height;
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
// fixed-size array was used, with nullptr indicating
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
//ioanch 20160305: added manual parameter, for backside access
//
int P_ActivateInStasisCeiling(const line_t *line, int tag, bool manual)
{
   int rtn = 0;
   
   // ioanch 20160314: avoid code duplication
   auto resumeceiling = [&rtn](CeilingThinker *ceiling)
   {
      int noise;

      ceiling->direction = ceiling->olddirection;
      ceiling->inStasis = false;

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
         // ioanch 20160314: mind the semi-silent variants
         noise = ceiling->crushflags & CeilingThinker::crushSilent ?
                     CNOISE_SEMISILENT : CNOISE_NORMAL;
         break;
      }
      P_CeilingSequence(ceiling->sector, noise);

      //jff 4/5/98 return if activated
      rtn = 1;
   };

   // ioanch 20160306: restore old vanilla bug only for demos
   if(demo_compatibility || P_LevelIsVanillaHexen())
   {
      for(int i = 0; i < vanilla_MAXCEILINGS; ++i)
      {
         CeilingThinker *ceiling = vanilla_activeceilings[i];
         if(ceiling && ceiling->tag == tag && ceiling->direction == plat_stop)
         {
            resumeceiling(ceiling);
         }
      }
      return rtn;
   }

   // ioanch: normal setup
   ceilinglist_t *cl;
   
   for(cl = activeceilings; cl; cl = cl->next)
   {
      CeilingThinker *ceiling = cl->ceiling;
      if(((manual && line->backsector == ceiling->sector) ||
         (!manual && ceiling->tag == tag)) && ceiling->direction == 0)
      {
         resumeceiling(ceiling);
      }
   }
   return rtn;
}

//
// Activates stasis for a ceiling thinker
//
static void P_putThinkerInStasis(CeilingThinker *ceiling)
{
   ceiling->olddirection = ceiling->direction;
   ceiling->direction = plat_stop;
   ceiling->inStasis = true;
   // ioanch 20160314: like in vanilla, do not make click sound when stopping
   // these types
   if(ceiling->type == silentCrushAndRaise ||
      ceiling->crushflags & CeilingThinker::crushSilent)
   {
      S_SquashSectorSequence(ceiling->sector, SEQ_ORIGIN_SECTOR_C);
   }
   else
      S_StopSectorSequence(ceiling->sector, SEQ_ORIGIN_SECTOR_C); // haleyjd 09/28/06
}

//
// EV_CeilingCrushStop()
//
// Stops all active ceilings with the right tag
//
// Passed the linedef stopping the ceilings
// Returns true if a ceiling put in stasis
//
int EV_CeilingCrushStop(int tag, bool removeThinker)
{
   int rtn = 0;
   
   // ioanch 20160306
   if(demo_compatibility || P_LevelIsVanillaHexen())
   {
      for(int i = 0; i < vanilla_MAXCEILINGS; ++i)
      {
         CeilingThinker *ceiling = vanilla_activeceilings[i];
         if(removeThinker)
         {
            if(ceiling && ceiling->tag == tag)
            {
               // in Hexen, just kill the crusher thinker
               rtn = 1;
               P_RemoveActiveCeiling(ceiling);
               break;   // get out after killing a single crusher
            }
         }
         else if(ceiling && ceiling->tag == tag && 
            ceiling->direction != plat_stop)
         {
            P_putThinkerInStasis(ceiling);
            rtn = 1;
         }
      }
      return rtn;
   }

   // ioanch: normal setup
   ceilinglist_t *cl;
   for(cl = activeceilings; cl; cl = cl->next)
   {
      CeilingThinker *ceiling = cl->ceiling;
      if(removeThinker)
      {
         if(ceiling->tag == tag)
         {
            P_RemoveActiveCeiling(ceiling);
            rtn = 1;
         }
      }
      else if(ceiling->direction != plat_stop && ceiling->tag == tag)
      {
         P_putThinkerInStasis(ceiling);
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
void P_AddActiveCeiling(CeilingThinker *ceiling)
{
   // ioanch 20160306
   if(demo_compatibility || P_LevelIsVanillaHexen())
   {
      for(int i = 0; i < vanilla_MAXCEILINGS; ++i)
      {
         if(!vanilla_activeceilings[i])
         {
            vanilla_activeceilings[i] = ceiling;
            break;
         }
      }
      return;
   }

   // ioanch: normal setup
   ceilinglist_t *list = estructalloc(ceilinglist_t, 1);
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
void P_RemoveActiveCeiling(CeilingThinker* ceiling)
{
   // ioanch 20160306
   if(demo_compatibility || P_LevelIsVanillaHexen())
   {
      for(int i = 0; i < vanilla_MAXCEILINGS; ++i)
      {
         if(vanilla_activeceilings[i] == ceiling)
         {
            ceiling->sector->srf.ceiling.data = nullptr;
            S_StopSectorSequence(ceiling->sector, SEQ_ORIGIN_SECTOR_C);
            ceiling->remove();
            vanilla_activeceilings[i] = nullptr;
            break;
         }
      }
      return;
   }

   // ioanch: normal setup
   ceilinglist_t *list = ceiling->list;
   ceiling->sector->srf.ceiling.data = nullptr;   //jff 2/22/98
   S_StopSectorSequence(ceiling->sector, SEQ_ORIGIN_SECTOR_C); // haleyjd 09/28/06
   ceiling->remove();
   if((*list->prev = list->next))
      list->next->prev = list->prev;
   efree(list);
}

//
// P_RemoveAllActiveCeilings()
//
// Removes all ceilings from the active ceiling list
//
// Passed nothing, returns nothing
//
void P_RemoveAllActiveCeilings()
{
   // ioanch 20160306
   if(demo_compatibility || P_LevelIsVanillaHexen())
   {
      memset(vanilla_activeceilings, 0, sizeof(vanilla_activeceilings));
      return;
   }

   // normal setup

   while(activeceilings)
   {  
      ceilinglist_t *next = activeceilings->next;
      efree(activeceilings);
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

//=============================================================================
//
// Waggle Ceilings
//

IMPLEMENT_THINKER_TYPE(CeilingWaggleThinker)

#define WGLSTATE_EXPAND 1
#define WGLSTATE_STABLE 2
#define WGLSTATE_REDUCE 3

//
// Thinker for ceiling waggle action.
//
void CeilingWaggleThinker::Think()
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
         dist       = originalHeight - sector->srf.ceiling.height;

         T_MoveCeilingInDirection(sector, abs(dist),
            destheight, 8, destheight >= sector->srf.ceiling.height ? plat_up : plat_down);

         sector->srf.ceiling.data = nullptr;
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

   destheight = originalHeight +
                FixedMul(FloatBobOffsets[(accumulator >> FRACBITS) & 63], scale);
   dist = destheight - sector->srf.ceiling.height;

   T_MoveCeilingInDirection(sector, abs(dist), destheight, 8,
      destheight >= sector->srf.ceiling.height ? plat_up : plat_down);
}

//
// Saves/loads a CeilingWaggleThinker thinker.
//
void CeilingWaggleThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << originalHeight << accumulator << accDelta << targetScale
       << scale << scaleDelta << ticker << state;
}

int EV_StartCeilingWaggle(const line_t *line, int tag, int height, int speed,
                         int offset, int timer)
{
   int       sectorIndex = -1;
   int       retCode = 0;
   bool      manual = false;
   sector_t *sector;
   CeilingWaggleThinker *waggle;

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
      if(sector->srf.ceiling.data)
      {
         if(manual)
            return retCode;
         else
            continue;
      }

      retCode = 1;
      waggle = new CeilingWaggleThinker;
      sector->srf.ceiling.data = waggle;
      waggle->addThinker();

      waggle->sector         = sector;
      waggle->originalHeight = sector->srf.ceiling.height;
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

