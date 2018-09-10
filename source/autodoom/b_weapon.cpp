// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2018 Ioan Chera
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Weapon analysis
//
//-----------------------------------------------------------------------------

#include "../z_zone.h"
#include "b_analysis.h"
#include "b_weapon.h"
#include "../a_common.h"
#include "../e_player.h"
#include "../e_states.h"
#include "../e_things.h"
#include "../e_weapons.h"
#include "../p_map.h"

// actors

void A_BFG11KHit(actionargs_t *);
void A_BFGBurst(actionargs_t *);
void A_BFGSpray(actionargs_t *);
void A_BouncingBFG(actionargs_t *);
void A_Detonate(actionargs_t *);
void A_DetonateEx(actionargs_t *);
void A_Explode(actionargs_t *);
void A_HticExplode(actionargs_t *);
void A_Mushroom(actionargs_t *);
void A_Nailbomb(actionargs_t *);

// weapons

void A_FireBFG(actionargs_t *);
void A_FireCGun(actionargs_t *);
void A_FireCustomBullets(actionargs_t *);
void A_FireOldBFG(actionargs_t *);
void A_FirePistol(actionargs_t *);
void A_FirePlasma(actionargs_t *);
void A_FirePlayerMissile(actionargs_t *);
void A_FireMissile(actionargs_t *);
void A_FireShotgun(actionargs_t *);
void A_FireShotgun2(actionargs_t *);

void A_ReFire(actionargs_t *);
void A_CloseShotgun2(actionargs_t *);
void A_WeaponReady(actionargs_t *);

void A_CheckReload(actionargs_t *);
void A_CheckReloadEx(actionargs_t *);
void A_JumpIfNoAmmo(actionargs_t *);

void A_Raise(actionargs_t *);
void A_GunFlash(actionargs_t *);

void A_WeaponCtrJump(actionargs_t *);
void A_WeaponCtrSwitch(actionargs_t *);

void A_Punch(actionargs_t *);
void A_Saw(actionargs_t *);
void A_CustomPlayerMelee(actionargs_t *);

//==============================================================================
//
// Weapon analysis
//

std::unordered_map<const playerclass_t *, std::unordered_map<const weaponinfo_t *, BotWeaponInfo>>
g_botweapons;

static void B_weaponGetBranchingStateSeq(statenum_t sn,
                                         StateQue &alterQueue,
                                         const StateSet &stateSet,
                                         const weaponinfo_t &wi)
{
   const state_t &st = *states[sn];
   PODCollection<statenum_t> dests(17);

   statenum_t fireState = wi.flashstate;

   if(sn == NullStateNum)
      return;  // do nothing

   if(st.action == A_FireCGun)
   {
      if(st.index >= E_StateNumForDEHNum(S_CHAIN1) &&
         st.index < E_StateNumForDEHNum(S_CHAIN3))
      {
         dests.add(st.index - states[E_SafeState(S_CHAIN1)]->index);
      }
      else
         dests.add(fireState);
   }
   else if(st.action == A_FireCustomBullets)
   {
      arglist_t *args = st.args;
      int flashint = E_ArgAsInt(args, 5, 0);
      int flashstate = E_ArgAsStateNum(args, 5, &wi, &st);
      if(flashint >= 0 && flashstate != NullStateNum)
         dests.add(flashstate);
      else
         dests.add(fireState);
   }
   else if(st.action == A_FirePistol || st.action == A_FireShotgun ||
           st.action == A_FireShotgun2 || st.action == A_GunFlash)
   {
      dests.add(fireState);
   }
   else if(st.action == A_FirePlasma)
   {
      dests.add(fireState);
      dests.add(fireState + 1);
   }
   else if(st.action == A_CheckReload)
      dests.add(wi.downstate);
   else if(st.action == A_CheckReloadEx || st.action == A_JumpIfNoAmmo)
   {
      int statenum = E_ArgAsStateNumNI(st.args, 0, &wi, &st);
      if(statenum >= 0)
         dests.add(statenum);
   }
   else if(st.action == A_Raise)
      dests.add(wi.readystate);


   // TODO: A_PlayerThunk
   else if(st.action == A_WeaponCtrJump)
   {
      // TODO: check if cnum has been touched or will be touched in such a way
      // to reach comparison with value. Only then accept the jump possibility
      int statenum  = E_ArgAsStateNumNI(st.args, 0, &wi, &st);
      int cnum      = E_ArgAsInt(st.args, 3, 0);
      if(statenum >= 0 && cnum >= 0 && cnum < 3)
         dests.add(statenum);
   }
   else if(st.action == A_WeaponCtrSwitch)
   {
      int cnum       = E_ArgAsInt       (st.args, 0,  0);
      int startstate = E_ArgAsStateNumNI(st.args, 1,  &wi, &st);
      int numstates  = E_ArgAsInt       (st.args, 2,  0) - 1;

      if (startstate >= 0 && startstate + numstates < NUMSTATES && cnum >= 0 &&
          cnum < 3)
      {
         for (int i = 0; i < numstates; ++i)
            dests.add(startstate + i);
      }
   }

   // add the destinations
   for (auto it = dests.begin(); it != dests.end(); ++it)
   {
      if(*it == 0 || *it == 1)
         continue;
      if(!stateSet.count(*it))
         alterQueue.push(*it);
   }
}

static bool B_weaponStateEncounters(statenum_t firstState,
                                    const weaponinfo_t &wi,
                                    bool(*statecase)(statenum_t sn,
                                                     void* miscData),
                                    void* miscData = nullptr)
{
   StateSet stateSet;  // set of visited states
   StateQue alterQueue;        // set of alternate chains
   // (RandomJump, Jump and so on)
   stateSet.rehash(47);

   statenum_t sn;
   alterQueue.push(firstState);

   while (alterQueue.size() > 0)
   {
      for(sn = alterQueue.front(), alterQueue.pop();
          ;
          sn = states[sn]->nextstate)
      {
         if (stateSet.count(sn))
         {
            // found a cycle
            break;
         }

         if (statecase(sn, miscData))
         {
            // If statecase returns true, it means we reached a goal.
            return true;
         }
         stateSet.insert(sn);

         B_weaponGetBranchingStateSeq(sn, alterQueue, stateSet, wi);
         if(states[sn]->tics < 0 || sn == NullStateNum)
            break;   // don't go to next state if current has neg. duration
      }
   }

   return false;
}

// Calculates the hitscan damage one may give to a target
int BotWeaponInfo::calcHitscanDamage(fixed_t dist, fixed_t radius, fixed_t height, bool berserk, bool first) const
{
   if(!dist)   // if dist is 0, just get total damage per attack
   {
      // FIXME: bfg not counted
      return meleeDamage + berserkDamage + alwaysDamage + firstDamage + neverDamage + monsterDamage + ssgDamage + projectileDamage + explosionDamage;
   }
   int damage = 0;
   dist -= radius;   // reduce the target radius now
   if(dist < 0)
      dist = 0;

   if(dist < MELEERANGE)
      damage += berserk ? berserkDamage : meleeDamage;

   if(dist < MISSILERANGE)
   {
      damage += alwaysDamage;
      if(first)
         damage += firstDamage;
      // aimshift 18. P_SubRandom returns between -255 to 255.
      // That's -2^8 to 2^8. Shift by 18 and that becomes -2^26 to 2^26.
      // 2^26/2^32 * 360 is 360/64 which is 5.625 degrees each side.

      // Assume the aim is centred
      if(neverDamage || firstDamage && !first)
      {
         int added = neverDamage + (!first ? firstDamage : 0);
         fixed_t spread = FixedMul(12909, dist);
         if(!spread || spread < radius * 2)   // also treat the zero case
            damage += added;
         else
         {
            // Area is spread * spread / 4
            // e = spread / 2 - radius is excluded
            // excluded area on one side is e * e
            // Result is: spread * spread / 4 - (spread / 2 - radius) ^ 2
            // spread * spread / 4 - (spread * spread / 4 - spread * radius + radius * radius) =
            // spread * radius - radius * radius = radius * (spread - radius)
            // 4 * radius / spread - 4 * (radius / spread) ^ 2 =
            // 4 * radius / spread * (1 - 4 * radius / spread)
            // Multiplier formula: 4 * r * (S - r) / S / S
            damage += FixedMul(4 * FixedMul(FixedDiv(radius, spread), FixedDiv(spread - radius, spread)), added);

         }
      }
      if(monsterDamage)
      {
         fixed_t spread = FixedMul(54292, dist);
         if(!spread || spread < radius * 2)
            damage += monsterDamage;
         else
         {
            damage += FixedMul(4 * FixedMul(FixedDiv(radius, spread), FixedDiv(spread - radius, spread)), monsterDamage);
         }
      }
      if(ssgDamage)
      {
         fixed_t xspread = FixedMul(18421, dist);
         fixed_t zspread = FixedMul(16384, dist);
         if(xspread && xspread < radius * 2)
            xspread = radius * 2;
         if(zspread && zspread < height)
            zspread = height;

         int interdmg = xspread ? FixedMul(4 * FixedMul(FixedDiv(radius, xspread), FixedDiv(xspread - radius, xspread)), ssgDamage) : ssgDamage;
         damage += zspread ? FixedMul(4 * FixedMul(FixedDiv(height / 2, zspread), FixedDiv(zspread - height / 2, zspread)), interdmg) : interdmg;
      }
   }

   damage += projectileDamage + explosionDamage;
   return damage;
}

static bool B_analyzeProjectile(statenum_t sn, const mobjinfo_t &mi, void *ctx)
{

   auto bwi = static_cast<BotWeaponInfo *>(ctx);
   const state_t &st = *states[sn];

   static auto increase = [bwi](int value) {
      if(value > bwi->explosionRadius)
         bwi->explosionRadius = value;
   };


   // TODO: potential frames which spawn other explosives may count here!
   if(st.action == A_Explode || st.action == A_Mushroom || st.action == A_Nailbomb)
   {
      increase(kExplosionRadius);
      bwi->explosionDamage += kExplosionRadius;
      bwi->unsafeExplosion = true;
   }
   else if(st.action == A_Detonate)
   {
      increase(mi.damage);
      bwi->explosionDamage += mi.damage;
      bwi->unsafeExplosion = true;
   }
   else if(st.action == A_DetonateEx)
   {
      int damage = E_ArgAsInt(st.args, 0, 128);
      int radius = E_ArgAsInt(st.args, 1, 128);
      bool hurtself = E_ArgAsInt(st.args, 2, 1) == 1;
      increase(radius);
      bwi->explosionDamage += damage;
      bwi->unsafeExplosion = bwi->unsafeExplosion || hurtself;
   }
   else if(st.action == A_HticExplode)
   {
      bwi->unsafeExplosion = true;
      switch(E_ArgAsKwd(st.args, 0, &hticexpkwds, 0))
      {
         case kHticExplodeDsparilBSpark:
            increase(111);
            bwi->explosionDamage += 111;
            break;
         case kHticExplodeFloorFire:
            increase(24);
            bwi->explosionDamage += 24;
            break;
         default:
            increase(kExplosionRadius);
            bwi->explosionDamage += kExplosionRadius;
      }
   }
   else if(st.action == A_BFG11KHit || st.action == A_BouncingBFG ||
           st.action == A_BFGBurst || st.action == A_BFGSpray)
   {
      ++bwi->bfgCount;
   }

   return false;
}

//
// Analyzes one weapon, if not set already
//
void B_AnalyzeWeapons(const playerclass_t *pclass)
{
   // Some assumptions are made for now:
   // - all UP, DOWN and idle states are using standard codepointers
   // - the flashing states are purely decorative (but may be hit by the state
   //   walker from the shooting state)
   // - any damage done after ReFire is ignored

   if(!pclass || !pclass->hasslots || g_botweapons.find(pclass) != g_botweapons.end())
      return;

   std::unordered_map<const weaponinfo_t *, BotWeaponInfo> &map = g_botweapons[pclass];

   // First collect all weapons
   for(const weaponslot_t *slot : pclass->weaponslots)
   {
      if(!slot)
         continue;
      for(const BDListItem<weaponslot_t> *weaponslot = E_FirstInSlot(slot);
          !weaponslot->isDummy(); weaponslot = weaponslot->bdNext)
      {
         const weaponinfo_t *wi = weaponslot->bdObject->weapon;
         map[wi] = BotWeaponInfo();
      }
   }

   struct State
   {
      bool reachedFire;
      bool reachedRefire;
      int burstTics;
      BotWeaponInfo *bwi;
   } state;

   // Now setup the information
   for(auto it = map.begin(); it != map.end(); ++it)
   {
      const weaponinfo_t &wi = *it->first;
      BotWeaponInfo &bwi = it->second;
      bwi = BotWeaponInfo();

      memset(&state, 0, sizeof(state));
      state.bwi = &bwi;

      B_weaponStateEncounters(wi.atkstate, wi, [](statenum_t sn, void *ctx) {
         State &state = *static_cast<State *>(ctx);
         BotWeaponInfo &bwi = *state.bwi;
         const state_t &st = *states[sn];

         auto increaseBurst = [&state]() {
            if(state.bwi->burstRate < state.burstTics)
            {
               state.bwi->burstRate = state.burstTics;
               state.burstTics = 0;
            }
         };

         mobjtype_t projectile = -1, projectile2 = -1;

         bwi.oneShotRate += st.tics;

         if(st.action == A_ReFire || st.action == A_CloseShotgun2)
         {
            state.reachedRefire = true;
            state.burstTics += bwi.timeToFire;
            increaseBurst();
         }

         if(!state.reachedRefire)
            bwi.refireRate += st.tics;
         else
            return true;

         if(st.action == A_WeaponReady)   // we're out
            return true;

         if(st.action == A_Punch)
         {
            state.reachedFire = true;
            increaseBurst();

            bwi.meleeDamage += 11;
            bwi.berserkDamage += 110;
         }
         else if(st.action == A_Saw)
         {
            state.reachedFire = true;
            increaseBurst();

            bwi.meleeDamage += 11;
            bwi.berserkDamage += 11;
         }
         else if(st.action == A_CustomPlayerMelee)
         {
            state.reachedFire = true;
            increaseBurst();

            int dmgfactor = E_ArgAsInt(st.args, 0, 0);
            int dmgmod = E_ArgAsInt(st.args, 1, 0);
            if(dmgmod < 1)
               dmgmod = 1;
            else if(dmgmod > 256)
               dmgmod = 256;
            int berserkmul = E_ArgAsInt(st.args, 2, 0);

            int damage = dmgfactor * (1 + dmgmod) / 2;
            bwi.meleeDamage += damage;
            bwi.berserkDamage += damage * berserkmul;
         }
         else if(st.action == A_FirePistol || st.action == A_FireCGun)
         {
            state.reachedFire = true;
            increaseBurst();

            bwi.firstDamage += 10;
         }
         else if(st.action == A_FireShotgun)
         {
            state.reachedFire = true;
            increaseBurst();

            bwi.neverDamage += 70;
         }
         else if(st.action == A_FireShotgun2)
         {
            state.reachedFire = true;
            increaseBurst();

            bwi.ssgDamage += 200;
         }
         else if(st.action == A_FireCustomBullets)
         {
            state.reachedFire = true;
            increaseBurst();

            int accurate = E_ArgAsKwd(st.args, 1, &fcbkwds, 0);
            int numbullets = E_ArgAsInt(st.args, 2, 0);
            int damage = E_ArgAsInt(st.args, 3, 0);
            int dmgmod = E_ArgAsInt(st.args, 4, 0);
            if(!accurate)
               accurate = 1;
            if(dmgmod < 1)
               dmgmod = 1;
            else if(dmgmod > 256)
               dmgmod = 256;

            int calcDamage = numbullets * damage * (1 + dmgmod) / 2;
            switch(accurate)
            {
               case 1:  // always
                  bwi.alwaysDamage += calcDamage;
                  break;
               case 2:  // first
                  bwi.firstDamage += calcDamage;
                  break;
               case 3:  // never
                  bwi.neverDamage += calcDamage;
                  break;
               case 4:  // ssg
                  bwi.ssgDamage += calcDamage;
                  break;
               case 5:  // monster
                  bwi.monsterDamage += calcDamage;
                  break;
            }
         }
         else if(st.action == A_FireMissile)
            projectile = E_SafeThingType(MT_ROCKET);
         else if(st.action == A_FirePlasma)
            projectile = E_SafeThingType(MT_PLASMA);
         else if(st.action == A_FireBFG)
            projectile = E_SafeThingType(MT_BFG);
         else if(st.action == A_FireOldBFG)
         {
            projectile = E_SafeThingType(MT_PLASMA1);
            projectile2 = E_SafeThingType(MT_PLASMA2);
         }
         else if(st.action == A_FirePlayerMissile)
         {
            projectile = E_ArgAsThingNumG0(st.args, 0);
            if(projectile < 0 || projectile == -1)
               projectile = UnknownThingType;
            else
            {
               bool seek = !!E_ArgAsKwd(st.args, 1, &seekkwds, 0);
               bwi.seeking = bwi.seeking || seek;
            }
         }

         if(projectile >= 0 && projectile != UnknownThingType)
         {
            state.reachedFire = true;
            increaseBurst();

            const mobjinfo_t *info = mobjinfo[projectile];
            bwi.projectileDamage += info->damage * 9 / 2;
            if(info->speed > bwi.projectileSpeed)
               bwi.projectileSpeed = info->speed;
            if(info->radius > bwi.projectileRadius)
               bwi.projectileRadius = info->radius;

            B_StateEncounters(info->deathstate, nullptr, info, B_analyzeProjectile, false, &bwi);
         }
         if(projectile2 >= 0 && projectile2 != UnknownThingType)
         {
            state.reachedFire = true;
            increaseBurst();

            const mobjinfo_t *info = mobjinfo[projectile2];
            bwi.projectileDamage += info->damage * 9 / 2;
            if(info->speed > bwi.projectileSpeed)
               bwi.projectileSpeed = info->speed;
            if(info->radius > bwi.projectileRadius)
               bwi.projectileRadius = info->radius;

            B_StateEncounters(info->deathstate, nullptr, info, B_analyzeProjectile, false, &bwi);
         }

         if(!state.reachedFire)
            bwi.timeToFire += st.tics;
         state.burstTics += st.tics;

         return false;
      }, &state);

      // now set flags
      if(bwi.meleeDamage)
         bwi.flags |= BWI_MELEE;
      if(bwi.berserkDamage > bwi.meleeDamage)
         bwi.flags |= BWI_BERSERK;
      if(bwi.alwaysDamage || bwi.firstDamage ||
         bwi.neverDamage || bwi.monsterDamage ||
         bwi.ssgDamage)
      {
         bwi.flags |= BWI_HITSCAN;
         if(bwi.alwaysDamage)
            bwi.flags |= BWI_SNIPE;
         if(bwi.firstDamage)
            bwi.flags |= BWI_TAP_SNIPE;
      }
      if(bwi.projectileDamage || bwi.explosionDamage ||
         bwi.bfgCount)
      {
         bwi.flags |= BWI_MISSILE;
         if(bwi.unsafeExplosion)
            bwi.flags |= BWI_DANGEROUS;
         if(bwi.bfgCount)
            bwi.flags |= BWI_ULTIMATE;
      }
      if(!(bwi.flags & (BWI_HITSCAN | BWI_SNIPE | BWI_TAP_SNIPE |
                        BWI_MISSILE | BWI_DANGEROUS | BWI_ULTIMATE)))
      {
         bwi.flags |= BWI_MELEE_ONLY;
      }
   }
}
