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
#include "d_mod.h"
#include "e_inventory.h"
#include "ev_specials.h"
#include "m_cheat.h"
#include "metaapi.h"
#include "p_inter.h"
#include "p_mobj.h"
#include "p_map.h"
#include "p_map3d.h"
#include "p_saveg.h"
#include "p_saveid.h"
#include "p_setup.h"
#include "p_things.h"
#include "a_small.h"
#include "acs_intr.h"
#include "e_player.h"
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
   Mobj *mobj = nullptr, *newMobj;
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

   while((mobj = P_FindMobjFromTID(tid, mobj, nullptr)))
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
   int tid, newtid;
   angle_t angle;
   Mobj *mobj = nullptr, *newMobj, *fogMobj;
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
   newtid = args[3];

   while((mobj = P_FindMobjFromTID(tid, mobj, nullptr)))
   {
      z = mobj->z;

      newMobj = P_SpawnMobj(mobj->x, mobj->y, z, moType);

      if(!P_CheckPositionExt(newMobj, newMobj->x, newMobj->y, newMobj->z)) // Didn't fit?
         newMobj->remove();
      else
      {
         if(newtid)
            P_AddThingTID(newMobj, newtid);
         newMobj->angle = angle;

         if(fog)
         {
            fogMobj = P_SpawnMobj(mobj->x, mobj->y,
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
   Mobj *mobj = nullptr;
   int success = 0;

   while((mobj = P_FindMobjFromTID(tid, mobj, nullptr)))
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
   Mobj *mobj = nullptr;
   int success = 0;

   while((mobj = P_FindMobjFromTID(tid, mobj, nullptr)))
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
// EV_ThingChangeTID
//
// ioanch 20160221 (originally by DavidPH)
// Implements Thing_ChangeTID(oldtid, newtid)
//
int EV_ThingChangeTID(Mobj *actor, int oldtid, int newtid)
{
   Mobj   *mo;
   Mobj   *next   = nullptr;
   bool    found;

   mo    = P_FindMobjFromTID(oldtid, nullptr, actor);
   found = mo != nullptr;
   while(mo)
   {
      // Find next Mobj before changing TID.
      next = P_FindMobjFromTID(oldtid, mo, actor);

      P_RemoveThingTID(mo);
      P_AddThingTID(mo, newtid);
      mo = next;
   }

   return found;
}

//
// EV_ThingRaise
//
// Implements Thing_Raise(tid)
//
int EV_ThingRaise(Mobj *actor, int tid)
{
   Mobj *mobj = nullptr;
   int success = 0;
   while((mobj = P_FindMobjFromTID(tid, mobj, actor)))
   {
      if(!P_ThingIsCorpse(mobj) || !P_CheckCorpseRaiseSpace(mobj))
         continue;
      // no raiser allowed, no friendliness transferred
      P_RaiseCorpse(mobj, nullptr); 
      success = 1;
   }
   return success;
}

//
// EV_ThingStop
//
// Implements Thing_Stop(tid)
//
int EV_ThingStop(Mobj *actor, int tid)
{
   Mobj *mobj = nullptr;
   int success = 0;
   while((mobj = P_FindMobjFromTID(tid, mobj, actor)))
   {
      mobj->momx = mobj->momy = mobj->momz = 0; // same as A_Stop
      success = 1;
   }
   return success;
}

//
// EV_ThrustThing
//
// Implements ThrustThing(angle, speed, reserved, tid)
//
int EV_ThrustThing(Mobj *actor, int side, int byteangle, int ispeed, int tid)
{
   // Hexen format maps cannot have ExtraData, and in Hexen activation is only
   // done from the front side, so keep compatible with that. Otherwise, in
   // ExtraData-possible maps and eventually UDMF, the author can choose.
   
   // args[2] is reserved. In the ZDoom equivalent, it has something to do with
   // setting huge speeds (> 30.0)

   int success = 0;

   // Either thrust if not compatible (i.e. modern) or only if the front
   // side is touched (in Hexen). Back side will have no effect in plain Hexen
   // maps.
   if(!P_LevelIsVanillaHexen() || side == 0)
   {
      Mobj *mobj = nullptr;

      angle_t angle = byteangle * (ANG90 / 64);
      fixed_t speed = ispeed << FRACBITS;

      // tid argument is also in ZDoom

      while((mobj = P_FindMobjFromTID(tid, mobj, actor)))
      {
         P_ThrustMobj(mobj, angle, speed);
         success = 1;
      }
   }
   return success;
}

//
// EV_ThrustThingZ
//
// Implements ThrustThingZ(tid, speed, updown, setadd)
//
int EV_ThrustThingZ(Mobj *actor, int tid, int ispeed, bool upDown, bool setAdd)
{
   fixed_t speed = ispeed << (FRACBITS - 2);
   int sign = upDown ? -1 : 1;

   int success = 0;

   Mobj *mobj = nullptr;
   while((mobj = P_FindMobjFromTID(tid, mobj, actor)))
   {
      mobj->momz = (setAdd ? mobj->momz : 0) + speed * sign;
      success = 1;
   }
   return success;
}

//
// EV_DamageThing
//
// Implements DamageThing(damage, mod)
//
int EV_DamageThing(Mobj *actor, int damage, int mod, int tid)
{
   Mobj *mobj = nullptr;
   int success = 0;
   while((mobj = P_FindMobjFromTID(tid, mobj, actor)))
   {
      P_DamageMobj(mobj, nullptr, nullptr, damage, mod);
      success = 1;
   }
   return success;
}

//
// EV_ThingDestroy
//
// Implements Thing_Destroy(tid, flags, sectortag)
//
int EV_ThingDestroy(int tid, int flags, int sectortag)
{
   Mobj *mobj = nullptr;
   int success = 0;
   if(P_LevelIsVanillaHexen())   // Support vanilla Hexen behavior
   {
      while((mobj = P_FindMobjFromTID(tid, mobj, nullptr)))
      {
         // Also support sector tags because it's harmless
         if(mobj->flags & MF_SHOOTABLE && (!sectortag || mobj->subsector->sector->tag == sectortag))
         {
            P_DamageMobj(mobj, nullptr, nullptr, GOD_BREACH_DAMAGE, MOD_UNKNOWN);
            success = 1;
         }
      }
   }
   else if(!tid && !sectortag)   // zero tagging: kill all monsters
      success = M_NukeMonsters() > 0;
   else
   {
      //
      // Common function to kill iterated thing
      //
      auto killthing = [](Mobj *mobj, int sectortag, bool extreme)
      {
         if(!(mobj->flags & MF_SHOOTABLE) ||
            (sectortag && mobj->subsector->sector->tag != sectortag))
         {
            return false;
         }
         int damage;
         damage = extreme ? GOD_BREACH_DAMAGE : mobj->health;
         P_DamageMobj(mobj, nullptr, nullptr, damage, MOD_UNKNOWN);
         return true;
      };

      bool extreme = !!(flags & 1);
      // Normal, GZDoom-compatible behavior
      if(!tid)
      {
         for(Thinker *th = thinkercap.next; th != &thinkercap; th = th->next)
         {
            if(!(mobj = thinker_cast<Mobj *>(th)) || !killthing(mobj, sectortag, extreme))
               continue;
            success = 1;
         }
      }
      else
      {
         while((mobj = P_FindMobjFromTID(tid, mobj, nullptr)))
         {
            if(killthing(mobj, sectortag, extreme))
               success = 1;
         }
      }
   }
   return success;
}

//
// EV_HealThing
//
// Returns 0 if the health isn't needed at all
//
int EV_HealThing(Mobj *actor, int amount, int maxhealth)
{
   if(!actor || actor->health <= 0)
      return 0;

   if(!maxhealth || !actor->player)
   {
      // If second arg is 0, or the activator isn't a player
      // then set the maxhealth to the activator's spawning health or pclass maxhealth.
      maxhealth = actor->player ? actor->player->pclass->maxhealth :
            actor->getModifiedSpawnHealth();
   }
   else if(maxhealth == 1)
      maxhealth = actor->player->pclass->superhealth;

   // If the activator can be given health then activate the switch
   return EV_DoHealThing(actor, amount, maxhealth) ? 1 : 0;
}

//
// Removes map objects tagged "tid" from the game
//
int EV_ThingRemove(int tid)
{
   Mobj *removed;
   Mobj *mobj = nullptr;
   mobj = P_FindMobjFromTID(tid, mobj, nullptr);
   int rtn = 0;
   while(mobj)
   {
      // don't attempt to remove player object because that would crash the game
      // FIXME: removing voodoo dolls doesn't seem to work anyway
      if(mobj->player && mobj->player->mo == mobj)
      {
         mobj = P_FindMobjFromTID(tid, mobj, nullptr);
         continue;
      }

      // TODO: special handling for Hexen-like bridges

      removed = mobj;
      mobj = P_FindMobjFromTID(tid, mobj, nullptr);

      // clean up as best as we can
      removed->health = 0;
      removed->flags &= ~MF_SHOOTABLE;
      removed->remove();

      rtn = 1;
   }
   return rtn;
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
      remove();
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
   Archive_MobjType(arc, mobjtype);
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
      if(!action->bossonly)   // boss-only actions are handled in UMAPINFO
         LevelActionThinker::Spawn(action->special, action->args, action->mobjtype);
}

// EOF

