// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 Raven Software, James Haley, et al.
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
// Description:
//
// Mobj-related line specials. Mostly from Hexen.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "doomstat.h"
#include "d_gi.h"
#include "ev_specials.h"
#include "p_mobj.h"
#include "p_map3d.h"
#include "p_saveg.h"
#include "p_setup.h" // ioanch 20160104: for level format
#include "p_things.h"
#include "a_small.h"
#include "acs_intr.h"
#include "e_states.h"
#include "e_things.h"
#include "p_info.h"
#include "s_sound.h"

//
// EV_ThingProjectile
//
// Implements Thing_Projectile(tid, type, angle, speed, vspeed)
//
int EV_ThingProjectile(const int *args, bool gravity)
{
   int tid;
   angle_t angle;
   int fineAngle;
   fixed_t speed;
   fixed_t vspeed;
   mobjtype_t moType;
   Mobj *mobj = NULL, *newMobj;
   mobjinfo_t *mi;
   bool success = false;

   tid    = args[0];
   
   if(args[1] >= 0 && args[1] < ACS_NUM_THINGTYPES)
      moType = ACS_thingtypes[args[1]];
   else
      moType = UnknownThingType;

   mi = mobjinfo[moType];
 
   // Don't spawn monsters if -nomonsters
   if(nomonsters && (mi->flags & MF_COUNTKILL || mi->flags3 & MF3_KILLABLE))
      return false;

   angle     = (angle_t)args[2] << 24;
   fineAngle = angle >> ANGLETOFINESHIFT;
   speed     = (fixed_t)args[3] << 13;
   vspeed    = (fixed_t)args[4] << 13;

   while((mobj = P_FindMobjFromTID(tid, mobj, NULL)))
   {
      newMobj = P_SpawnMobj(mobj->x, mobj->y, mobj->z, moType);
      if(newMobj->info->seesound)
         S_StartSound(newMobj, newMobj->info->seesound);
      P_SetTarget<Mobj>(&newMobj->target, mobj);
      newMobj->angle   = angle;
      newMobj->momx    = FixedMul(speed, finecosine[fineAngle]);
      newMobj->momy    = FixedMul(speed, finesine[fineAngle]);
      newMobj->momz    = vspeed;
      // HEXEN TODO: ???
      //newMobj->flags2 |= MF2_DROPPED; 
      if(gravity)
      {
         newMobj->flags &= ~MF_NOGRAVITY;
         newMobj->flags2 |= MF2_LOGRAV;
      }
      if(P_CheckMissileSpawn(newMobj))
         success = true;
   }

   return success;
}

//
// EV_ThingSpawn
//
// Implements Thing_Spawn(tid, type, angle, newtid)
//
int EV_ThingSpawn(const int *args, bool fog)
{
   int tid;
   angle_t angle;
   Mobj *mobj = NULL, *newMobj, *fogMobj;
   mobjtype_t moType;
   mobjinfo_t *mi;
   bool success = false;
   fixed_t z;
   
   tid = args[0];
   
   if(args[1] >= 0 && args[1] < ACS_NUM_THINGTYPES)
      moType = ACS_thingtypes[args[1]];
   else
      moType = UnknownThingType;

   mi = mobjinfo[moType];
   
   // Don't spawn monsters if -nomonsters 
   if(nomonsters && (mi->flags & MF_COUNTKILL || mi->flags3 & MF3_KILLABLE))
      return false;

   angle = (angle_t)args[2] << 24;
   
   while((mobj = P_FindMobjFromTID(tid, mobj, NULL)))
   {
      z = mobj->z;

      newMobj = P_SpawnMobj(mobj->x, mobj->y, z, moType);
      
      if(!P_CheckPositionExt(newMobj, newMobj->x, newMobj->y)) // Didn't fit?
         newMobj->removeThinker();
      else
      {
         newMobj->angle = angle;

         if(fog)
         {
            fogMobj = 
               P_SpawnMobj(mobj->x, mobj->y,
                           mobj->z + GameModeInfo->teleFogHeight, 
                           E_SafeThingName(GameModeInfo->teleFogType));
            S_StartSound(fogMobj, GameModeInfo->teleSound);
         }
         
         // don't item-respawn
         newMobj->flags3 |= MF3_NOITEMRESP;

         success = true;
      }
   }

   return success;
}

// 
// EV_ThingActivate
//
// Implements Thing_Activate(tid)
//
int EV_ThingActivate(int tid)
{
   Mobj *mobj = NULL;
   int success = 0;

   while((mobj = P_FindMobjFromTID(tid, mobj, NULL)))
   {
      if((mobj->flags & MF_COUNTKILL) || (mobj->flags3 & MF3_KILLABLE))
      {
         if(mobj->flags2 & MF2_DORMANT)
         {
            mobj->flags2 &= ~MF2_DORMANT;
            mobj->tics = 1;
            success = 1;
         }
      }

      // HEXEN TODO: crazy special-case behaviors for spikes and bells...

      if(mobj->info->activestate != NullStateNum)
         P_SetMobjState(mobj, mobj->info->activestate);
      S_StartSound(mobj, mobj->info->activatesound);

      if(mobj->info->activestate || mobj->info->activatesound)
         success = 1; // successful if anything actually happened
   }

   return success;
}

//
// EV_ThingDeactivate
//
// Implements Thing_Deactivate(tid)
//
int EV_ThingDeactivate(int tid)
{
   Mobj *mobj = NULL;
   int success = 0;

   while((mobj = P_FindMobjFromTID(tid, mobj, NULL)))
   {
      if((mobj->flags & MF_COUNTKILL) || (mobj->flags3 & MF3_KILLABLE))
      {
         if(!(mobj->flags2 & MF2_DORMANT))
         {
            mobj->flags2 |= MF2_DORMANT;
            mobj->tics = -1;
            success = 1;
         }
      }

      // HEXEN TODO: crazy special-case behavior for spikes...

      if(mobj->info->inactivestate != NullStateNum)
         P_SetMobjState(mobj, mobj->info->inactivestate);
      S_StartSound(mobj, mobj->info->deactivatesound);

      if(mobj->info->inactivestate || mobj->info->deactivatesound)
         success = 1; // successful if anything actually happened
   }

   return success;
}

//
// EV_ThrustThing
//
// Implements ThrustThing(angle, speed, reserved, tid)
//
int EV_ThrustThing(Mobj *activator, const int *args, int side)
{
   // Hexen format maps cannot have ExtraData, and in Hexen activation is only
   // done from the front side, so keep compatible with that. Otherwise, in
   // ExtraData-possible maps and eventually UDMF, the author can choose.
   bool compat = LevelInfo.mapFormat == LEVEL_FORMAT_HEXEN;

   // args[2] is reserved. In the ZDoom equivalent, it has something to do with
   // setting huge speeds (> 30.0)

   int success = 0;

   // Either thrust if not compatible (i.e. modern) or only if the front
   // side is touched (in Hexen). Back side will have no effect in plain Hexen
   // maps.
   if(!compat || !side)
   {
      Mobj *mobj = nullptr;

      angle_t angle = args[0] * (ANG90 / 64);
      fixed_t speed = args[1] << FRACBITS;

      // tid argument is also in ZDoom
      int tid = args[3];

      while((mobj = P_FindMobjFromTID(tid, mobj, activator)))
      {
         P_ThrustMobj(mobj, angle, speed);
         success = 1;
      }
   }
   return success;
}

//=============================================================================
//
// LevelActionThinker
//
// Allows execution of arbitrary line actions when monsters die on a level.
//

IMPLEMENT_THINKER_TYPE(LevelActionThinker)

//
// LevelActionThinker::Think
//
// Check the thinker list for things of the type specified. If any are living,
// return. Otherwise, if a player is alive, activate the action on that player's
// behalf and remove self.
//
void LevelActionThinker::Think()
{
   player_t *thePlayer = nullptr;

   // Check for first living player
   for(int i = 0; i < MAXPLAYERS; i++)
   {
      if(playeringame[i] && players[i].health > 0)
      {
         thePlayer = &players[i];
         break;
      }
   }
   if(!thePlayer)
      return;

   // Check if all monsters dead
   Thinker *cap = &thinkerclasscap[th_enemies];
   for(Thinker *th = cap->cnext; th != cap; th = th->cnext)
   {
      Mobj *mo;

      if((mo = thinker_cast<Mobj *>(th)))
      {
         if(mo->type == mobjtype && mo->health > 0)
            return;
      }
   }

   // Execute special
   ev_action_t *action = EV_HexenActionForSpecial(special);
   if(action && EV_ActivateAction(action, args, thePlayer->mo))
      removeThinker();
}

//
// LevelActionThinker::serialize
//
// Save or load the LevelActionThinker from a save game.
//
void LevelActionThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);
   arc << special;
   arc << mobjtype;
   P_ArchiveArray(arc, args, NUMLINEARGS);
}

//
// LevelActionThinker::Spawn
//
// Spawn a LevelActionThinker.
//
void LevelActionThinker::Spawn(int pSpecial, int *pArgs, int pMobjType)
{
   auto lva = new LevelActionThinker();

   lva->mobjtype = pMobjType;
   lva->special  = pSpecial;
   memcpy(lva->args, pArgs, sizeof(lva->args));
 
   lva->addThinker();
}

//
// P_SpawnLevelActions
//
// Spawn all the levelactions specified in LevelInfo as LevelActionThinker 
// instances.
//
void P_SpawnLevelActions()
{
   for(auto action = LevelInfo.actions; action; action = action->next)
      LevelActionThinker::Spawn(action->special, action->args, action->mobjtype);
}

// EOF

