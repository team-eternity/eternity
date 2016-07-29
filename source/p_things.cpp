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
#include "metaapi.h"
#include "p_inter.h"
#include "p_mobj.h"
#include "p_map.h"
#include "p_map3d.h"
#include "p_saveg.h"
#include "p_setup.h"
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
// EV_ThingChangeTID
//
// ioanch 20160221 (originally by DavidPH)
// Implements Thing_ChangeTID(oldtid, newtid)
//
int EV_ThingChangeTID(Mobj *actor, int oldtid, int newtid)
{
   Mobj   *mo     = nullptr;
   Mobj   *next   = nullptr;
   bool    found  = false;

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
int EV_ThingRaise(Mobj *actor, int tid, bool keepfriend)
{
   Mobj *mobj = nullptr;
   int success = 0;
   while((mobj = P_FindMobjFromTID(tid, mobj, actor)))
   {
      if(!P_ThingIsCorpse(mobj) || !P_CheckCorpseRaiseSpace(mobj))
         continue;
      // no raiser allowed, no friendliness transferred
      P_RaiseCorpse(mobj, keepfriend ? mobj : nullptr);
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
// Implements Thing_Destroy(tid, reserved, sectortag)
//
int EV_ThingDestroy(int tid, int sectortag)
{
   Mobj *mobj = nullptr;
   int success = 0;
   while((mobj = P_FindMobjFromTID(tid, mobj, nullptr)))
   {
      if(mobj->flags & MF_SHOOTABLE &&
         (!sectortag || mobj->subsector->sector->tag == sectortag))
      {
         P_DamageMobj(mobj, nullptr, nullptr, 10000, MOD_UNKNOWN);
         success = 1;
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
   if(!actor)
      return 0;
   mobjinfo_t *info = mobjinfo[actor->type];

   if(!maxhealth || !actor->player)
   {
      // If second arg is 0, or the activator isn't a player
      // then set the maxhealth to the activator's spawning health.
      maxhealth = info->spawnhealth;
   }
   else if(maxhealth == 1)
   {
      // Otherwise if second arg is 1 and the SoulSphere's effect is present,
      // then set maxhealth to the maximum health provided by a SoulSphere.
      itemeffect_t *soulsphereeffect
      = E_ItemEffectForName(ITEMNAME_SOULSPHERE);

      if(soulsphereeffect)
         maxhealth = soulsphereeffect->getInt("maxamount", 0);
      else
      {
         // FIXME: Handle this with a bit more finesse.
         maxhealth = info->spawnhealth + 100;
      }
   }

   // If the activator can be given health then activate the switch
   if(actor->health < maxhealth)
   {
      actor->health += amount;
      // cap to maxhealth
      if(actor->health > maxhealth)
         actor->health = maxhealth;
      // propagate to Mobj's player if it exists
      if(actor->player)
         actor->player->health = actor->health;

      return 1;
   }
   return 0;
}

//
// Thing_Hate conversion code. From ZDoom.
//
// Ally = won't attack player until shot
// Hostile = hostile to player, like any infighting monster
// Slave = will never attack player even if hurt
//
// Will be gradually implemented
//
enum hateconversion_e
{
   HATECONV_DEFAULT = 0,         // same as 4
   HATECONV_STANDBY_ALLY = 1,    // stands by for target while being an "ally"
                                 // (will become enemy when fired upon)
   HATECONV_PURSUIT_ALLY = 2,    // will immediately pursue
   HATECONV_STANDBY_HOSTILE = 3, // stands by for target OR players
   HATECONV_PURSUIT_HOSTILE = 4, // pursues a target but still hostile
   HATECONV_STANDBY_SLAVE = 5,   // stands by for target while "superfriend"
   HATECONV_PURSUIT_SLAVE = 6,   // pursues while "superfriend"
};

//
// Causes some things to target other things
//
int EV_ThingHate(Mobj *activator, int hater, int hatee, int conversion)
{
   Mobj *mobj = nullptr;
   Mobj *target = nullptr;
   int success = 0;
   // TODO: use conversion
   // TODO: determine if "ALLY" is like MBF's friend (temporary retaliation)
   // or makes the monster totally hostile. Also is it appropriate for "SLAVE"
   // to keep following the player?
   while((mobj = P_FindMobjFromTID(hater, mobj, activator)))
   {
      if(mobj->health < 1 || mobj->player)
         continue;
      success = 1;

      // Try to distribute haters evenly
      target = P_FindMobjFromTID(hatee, target, nullptr);
      if(!target)    // it may have wrapped around
         target = P_FindMobjFromTID(hatee, target, nullptr);   // try again
      while(target && target->health < 1)
         target = P_FindMobjFromTID(hatee, target, nullptr);   // skip dead
      if(!target || target->health < 1)
         return 0;   // no targets found at all, or all dead
      // found a target
      P_SetTarget<Mobj>(&mobj->target, target);
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

