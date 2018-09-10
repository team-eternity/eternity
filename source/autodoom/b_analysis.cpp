// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 Ioan Chera
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
//      Thing analysis. State walker.
//
//-----------------------------------------------------------------------------

#include "../z_zone.h"

#include "../a_common.h"
#include "b_analysis.h"
#include "b_util.h"
#include "../e_states.h"
#include "../p_mobj.h"

enum Trait
{
   Trait_hostile = 1,
   Trait_hitscan = 2,
   Trait_explosiveDeath = 4,
   Trait_summoner = 8,
};

// Set of traits for each mobjinfo index. Limited to 32 per entry.
static uint32_t* g_traitSet;

//
// Clears cache for things when reloading wad
//
void B_UpdateMobjInfoSet(int numthingsalloc)
{
   g_traitSet = erealloc(decltype(g_traitSet), g_traitSet, numthingsalloc * sizeof(*g_traitSet));
   memset(g_traitSet, 0, numthingsalloc * sizeof(*g_traitSet));
}

typedef void(*ActionFunction)(actionargs_t*);

static std::unordered_map<long, StateSet> preAttackStates;

//
// Clears cache for states when reloading wad
//
void B_UpdateStateInfoSet(int numstates)
{
   preAttackStates.clear();
}

////////////////////////////////////////////////////////////////////////////////
// State walking
////////////////////////////////////////////////////////////////////////////////


void A_PosAttack(actionargs_t *);
void A_SPosAttack(actionargs_t *);
void A_VileChase(actionargs_t *);
void A_VileAttack(actionargs_t *);
void A_SkelFist(actionargs_t *);
void A_SkelMissile(actionargs_t *);
void A_FatAttack1(actionargs_t *);
void A_FatAttack2(actionargs_t *);
void A_FatAttack3(actionargs_t *);
void A_CPosAttack(actionargs_t *);
void A_CPosRefire(actionargs_t *actionargs);
void A_TroopAttack(actionargs_t *);
void A_SargAttack(actionargs_t *);
void A_HeadAttack(actionargs_t *);
void A_BruisAttack(actionargs_t *);
void A_SkullAttack(actionargs_t *actionargs);
void A_SpidRefire(actionargs_t *);
void A_BspiAttack(actionargs_t *);
void A_CyberAttack(actionargs_t *);
void A_PainAttack(actionargs_t *);
void A_PainDie(actionargs_t *);
void A_KeenDie(actionargs_t *);
void A_BrainSpit(actionargs_t *);
void A_SpawnFly(actionargs_t *);
void A_BrainExplode(actionargs_t *);
void A_Detonate(actionargs_t *);        // killough 8/9/98
void A_DetonateEx(actionargs_t *);
void A_Mushroom(actionargs_t *);        // killough 10/98
void A_Spawn(actionargs_t *);           // killough 11/98
void A_Scratch(actionargs_t *);         // killough 11/98
void A_RandomJump(actionargs_t *);      // killough 11/98
void A_Nailbomb(actionargs_t *);

void A_SpawnAbove(actionargs_t *);
void A_BetaSkullAttack(actionargs_t *);

void A_SetFlags(actionargs_t *);
void A_UnSetFlags(actionargs_t *);
void A_GenRefire(actionargs_t *);
void A_KeepChasing(actionargs_t *);

void A_HealthJump(actionargs_t *);
void A_CounterJump(actionargs_t *);
void A_CounterSwitch(actionargs_t *);

void A_RandomWalk(actionargs_t *);
void A_TargetJump(actionargs_t *);
void A_CasingThrust(actionargs_t *);
void A_HticDrop(actionargs_t *);

void A_GenWizard(actionargs_t *);
void A_Sor2DthLoop(actionargs_t *);
void A_Srcr1Attack(actionargs_t *);
void A_Srcr2Decide(actionargs_t *);

void A_SorcererRise(actionargs_t *);

void A_MinotaurDecide(actionargs_t *);
void A_MinotaurAtk3(actionargs_t *);
void A_MinotaurCharge(actionargs_t *);

void A_WhirlwindSeek(actionargs_t *);
void A_LichFireGrow(actionargs_t *);
void A_ImpChargeAtk(actionargs_t *);

void A_ImpDeath(actionargs_t *);
void A_ImpXDeath1(actionargs_t *);
void A_ImpXDeath2(actionargs_t *);
void A_ImpExplode(actionargs_t *);

void A_JumpIfTargetInLOS(actionargs_t *);
void A_CheckPlayerDone(actionargs_t *);
void A_Jump(actionargs_t *);

void A_MissileAttack(actionargs_t *);
void A_MissileSpread(actionargs_t *);
void A_BulletAttack(actionargs_t *);
void A_ThingSummon(actionargs_t *);

void A_SnakeAttack(actionargs_t *);
void A_SnakeAttack2(actionargs_t *);

void A_SargAttack12(actionargs_t *actionargs);
void A_MummyAttack(actionargs_t *);
void A_MummyAttack2(actionargs_t *);
void A_ClinkAttack(actionargs_t *);
void A_WizardAtk3(actionargs_t *);
void A_Srcr2Attack(actionargs_t *);
void A_GenWizard(actionargs_t *);
void A_HticExplode(actionargs_t *);
void A_KnightAttack(actionargs_t *);
void A_BeastAttack(actionargs_t *);
void A_SnakeAttack(actionargs_t *);
void A_SnakeAttack2(actionargs_t *);
void A_Srcr1Attack(actionargs_t *);
void A_VolcanoBlast(actionargs_t *);
void A_MinotaurAtk1(actionargs_t *);
void A_MinotaurAtk2(actionargs_t *);
void A_MinotaurAtk3(actionargs_t *);
void A_MinotaurCharge(actionargs_t *);
void A_LichFire(actionargs_t *);
void A_LichWhirlwind(actionargs_t *);
void A_LichAttack(actionargs_t *);
void A_ImpChargeAtk(actionargs_t *);
void A_ImpMeleeAtk(actionargs_t *);
void A_ImpMissileAtk(actionargs_t *);
void A_AlertMonsters(actionargs_t *);

void A_BFG11KHit(actionargs_t *);
void A_BouncingBFG(actionargs_t *);
void A_BFGBurst(actionargs_t *);
void A_BFGSpray(actionargs_t *);

//
// B_getBranchingStateSeq
//
// Puts any secondary state sequence into the queue if action says something
//
static void B_getBranchingStateSeq(statenum_t sn,
                            StateQue &alterQueue,
                            const StateSet &stateSet,
                            const Mobj *mo, const mobjinfo_t *mi)
{
   const state_t &st = *states[sn];
   PODCollection<statenum_t> dests(17);
   
   if(mo && !mi)
      mi = mo->info;
   
   if(sn == NullStateNum)
      return;  // do nothing
      
   if(st.action == A_Look || st.action == A_CPosRefire
      || st.action == A_SpidRefire)
   {
      dests.add(mi->seestate);
   }
   else if(st.action == A_Chase || st.action == A_VileChase)
   {
      dests.add(mi->spawnstate);
      if(mi->meleestate != NullStateNum)
         dests.add(mi->meleestate);
      if(mi->missilestate != NullStateNum)
         dests.add(mi->missilestate);
      if(st.action == A_VileChase)
         dests.add(E_SafeState(S_VILE_HEAL1));
   }
   else if(st.action == A_SkullAttack)
      dests.add(mi->spawnstate);
   else if(st.action == A_RandomJump && st.misc2 > 0)
   {
      int rezstate = E_StateNumForDEHNum(st.misc1);
      if (rezstate >= 0)
         dests.add(rezstate);
   }
   else if(st.action == A_GenRefire)
   {
      if (E_ArgAsInt(st.args, 1, 0) > 0 ||
          (mo && mo->flags & MF_FRIEND || !mo && mi->flags & MF_FRIEND))
         dests.add(E_ArgAsStateNum(st.args, 0, mi, &st));
   }
   else if(st.action == A_HealthJump &&
           (mo && mo->flags & MF_SHOOTABLE || !mo && mi->flags & MF_SHOOTABLE) &&
           !(mo && mo->flags2 & MF2_INVULNERABLE || !mo && mi->flags2 & MF2_INVULNERABLE))
   {
      int statenum = E_ArgAsStateNumNI(st.args, 0, mi, &st);
      int checkhealth = E_ArgAsInt(st.args, 2, 0);
      
      if(statenum >= 0 && checkhealth < NUMMOBJCOUNTERS && checkhealth >= 0)
         dests.add(statenum);
   }
   else if(st.action == A_CounterJump)
   {
      // TODO: check if cnum has been touched or will be touched in such a way
      // to reach comparison with value. Only then accept the jump possibility
      int statenum  = E_ArgAsStateNumNI(st.args, 0, mi, &st);
      int cnum      = E_ArgAsInt(st.args, 3, 0);
      if(statenum >= 0 && cnum >= 0 && cnum < NUMMOBJCOUNTERS)
         dests.add(statenum);
   }
   else if(st.action == A_CounterSwitch)
   {
      int cnum       = E_ArgAsInt       (st.args, 0,  0);
      int startstate = E_ArgAsStateNumNI(st.args, 1, mi, &st);
      int numstates  = E_ArgAsInt       (st.args, 2,  0) - 1;

      if (startstate >= 0 && startstate + numstates < NUMSTATES && cnum >= 0 &&
          cnum < NUMMOBJCOUNTERS)
      {
         for (int i = 0; i < numstates; ++i)
            dests.add(startstate + i);
      }
   }
   else if(st.action == A_TargetJump)
   {
      int statenum = E_ArgAsStateNumNI(st.args, 0, mi, &st);
      if(statenum >= 0)
         dests.add(statenum);
   }
   else if(st.action == A_GenWizard)
   {
      dests.add(NullStateNum);
   }
   else if(st.action == A_Sor2DthLoop)
   {
      dests.add(E_SafeState(S_SOR2_DIE4));
   }
   else if(st.action == A_Srcr1Attack)
   {
      dests.add(E_SafeState(S_SRCR1_ATK4));
   }
   else if(st.action == A_Srcr2Decide)
   {
      dests.add(E_SafeState(S_SOR2_TELE1));
   }
   else if(st.action == A_MinotaurDecide)
   {
      dests.add(E_SafeState(S_MNTR_ATK4_1));
      dests.add(E_SafeState(S_MNTR_ATK3_1));
   }
   else if(st.action == A_MinotaurAtk3)
   {
      dests.add(E_SafeState(S_MNTR_ATK3_4));
   }
   else if(st.action == A_MinotaurCharge)
   {
      dests.add(mi->seestate);
      dests.add(mi->spawnstate);
   }
   else if(st.action == A_WhirlwindSeek)
   {
      dests.add(mi->deathstate);
   }
   else if(st.action == A_LichFireGrow)
   {
      dests.add(E_SafeState(S_LICHFX3_4));
   }
   else if(st.action == A_ImpChargeAtk)
   {
      dests.add(mi->seestate);
   }
   else if(st.action == A_ImpDeath || st.action == A_ImpXDeath2)
   {
      dests.add(mi->crashstate);
   }
   else if(st.action == A_ImpExplode)
   {
      dests.add(E_SafeState(S_IMP_XCRASH1));
   }
   else if(st.action == A_JumpIfTargetInLOS || st.action == A_CheckPlayerDone)
   {
      int statenum = E_ArgAsStateNumNI(st.args, 0, mi, &st);
      if(statenum >= 0)
         dests.add(statenum);
   }
   else if(st.action == A_Jump)
   {
      int chance = E_ArgAsInt(st.args, 0, 0);

      // FIXME: mobj is needed here
      if(mo && chance && st.args && st.args->numargs >= 2)
      {
         const state_t *state;
         for(int i = 0; i < st.args->numargs; ++i)
         {
            state = E_ArgAsStateLabel(mi, &st, st.args, i);
            dests.add(state->index);
         }
      }
   }
   else if(st.action == A_MissileAttack || st.action == A_MissileSpread)
   {
      int statenum = E_ArgAsStateNumG0(st.args, 4, mi, &st);
      if(statenum >= 0 && statenum < NUMSTATES)
         dests.add(statenum);
   }
   else if(st.action == A_SnakeAttack || st.action == A_SnakeAttack2)
   {
      dests.add(mi->spawnstate);
   }
   
   // add the destinations
   for (auto it = dests.begin(); it != dests.end(); ++it)
   {
      if(!stateSet.count(*it))
         alterQueue.push(*it);
   }
}

//
// B_actionRemovesSolid
//
// Utility test if an action removes the solid flag
//
static bool B_actionRemovesSolid(ActionFunction action, statenum_t sn)
{
   if(action == A_Fall)
      return true;
   if(action == A_PainDie)
      return true;
   if(action == A_KeenDie)
      return true;
   if(action == A_HticDrop)
      return true;
   if(action == A_GenWizard)
      return true;
   if(action == A_SorcererRise)
      return true;
   if(action == A_ImpXDeath1)
      return true;
   if (action == A_SetFlags)
   {
      unsigned int *flags = E_ArgAsThingFlags(states[sn]->args, 1);
      if(!flags)
         return false;
      int flagfield = E_ArgAsInt(states[sn]->args, 0, 0);
      
      if (flagfield <= 1 && flags[0] & (MF_NOBLOCKMAP | MF_NOCLIP))
         return true;
      if ((!flagfield || flagfield == 2) && flags[1] & MF2_PUSHABLE)
         return true;
      
      return false;
   }
   if (action == A_UnSetFlags)
   {
      unsigned int *flags = E_ArgAsThingFlags(states[sn]->args, 1);
      if(!flags)
         return false;
      int flagfield = E_ArgAsInt(states[sn]->args, 0, 0);
      switch (flagfield)
      {
         case 0:
         case 1:
            if (flags[0] & (MF_SOLID))
               return true;
         default:
            return false;
      }
      return false;
   }
   return false;
}

//
// B_cantBeSolid
//
// Used as a callback function by mobj-solid-decor tester
//
static bool B_stateCantBeSolidDecor(statenum_t sn, const mobjinfo_t &mi, void* miscData)
{
   if (sn == NullStateNum) // null state: it dissipates
      return true;
   const state_t &st = *states[sn];
   if ((st.action == A_Chase || st.action == A_KeepChasing ||
        st.action == A_RandomWalk) &&
       mi.speed != 0)   // chase state with nonzero walk
   {
      return true;
   }
   if(st.action == A_CasingThrust)
   {
      fixed_t moml, momz;
      
      moml = E_ArgAsInt(st.args, 0, 0) * FRACUNIT / 16;
      momz = E_ArgAsInt(st.args, 1, 0) * FRACUNIT / 16;
      
      if (moml || momz)
         return true;
   }
   
   if (B_actionRemovesSolid(st.action, sn))
      return true;
   
   return false;
}

//
// True if state leads into a goal
//
bool B_StateEncounters(statenum_t firstState, const Mobj *mo, const mobjinfo_t *info,
                       bool(*statecase)(statenum_t, const mobjinfo_t&, void* miscData),
                       bool avoidPainStates, void* miscData)
{
   StateSet stateSet;  // set of visited states
   StateQue alterQueue;        // set of alternate chains
   // (RandomJump, Jump and so on)
   stateSet.rehash(47);

   if(mo && !info)
      info = mo->info;

   statenum_t sn;
   alterQueue.push(firstState);

   // in case we're looking at a thing and it doesn't have SHOOTABLE despite
   // its definition having it, do not add pain and death states
   if((mo && mo->flags & MF_SHOOTABLE || !mo && info->flags & MF_SHOOTABLE)
      && !avoidPainStates)
   {
      if(info->painchance > 0)
         alterQueue.push(info->painstate);
      alterQueue.push(info->deathstate);
      if(info->xdeathstate > 0)
         alterQueue.push(info->xdeathstate);
   }
      
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
         
         if (statecase(sn, *info, miscData))
         {
             // If statecase returns true, it means we reached a goal.
            return true;
         }
         stateSet.insert(sn);
         
         
         B_getBranchingStateSeq(sn, alterQueue, stateSet, mo, info);
         if(states[sn]->tics < 0 || sn == NullStateNum)
            break;   // don't go to next state if current has neg. duration
      }
   }
   
   return false;
}

//
// B_IsMobjSolidDecor
//
// Checks if mobj is a permanent solid decoration (needed by the bot map)
//
bool B_IsMobjSolidDecor(const Mobj &mo)
{
   if (!(mo.flags & MF_SOLID))
      return false;
   if (mo.flags & MF_SHOOTABLE && !(mo.flags2 & MF2_INVULNERABLE))
      return false;
   if (mo.flags & (MF_NOBLOCKMAP | MF_NOCLIP))
      return false;
   if (mo.flags2 & MF2_PUSHABLE)
      return false;
   
   const mobjinfo_t &mi = *mo.info;
   
   if (mi.spawnstate == NullStateNum)
      return false;  // has null start frame, invalid
   if (B_StateEncounters(mi.spawnstate, &mo, nullptr, B_stateCantBeSolidDecor))
      return false;  // state goes to null or disables solidity or moves
   
   return true;
}

//
// B_stateAttacks
//
// True if this state is used for attacking
//
static bool B_stateAttacks(statenum_t sn, const mobjinfo_t &mi, void* miscData)
{
    const state_t &st = *states[sn];

    static const std::unordered_set<ActionFunction> attacks = 
    {
        A_Explode, 
        A_PosAttack,
        A_SPosAttack,
        A_VileAttack,
        A_SkelFist,
        A_SkelMissile,
        A_FatAttack1,
        A_FatAttack2,
        A_FatAttack3,
        A_CPosAttack,
        A_TroopAttack,
        A_SargAttack,
        A_HeadAttack,
        A_BruisAttack,
        A_SkullAttack,
        A_BspiAttack,
        A_CyberAttack,
        A_PainAttack,
        A_PainDie,
        A_BrainSpit,
        A_SpawnFly,
        A_BrainExplode,
        A_Detonate,
        A_Mushroom,
        A_Spawn,
        A_Scratch,
        A_Nailbomb,
        A_SpawnAbove,
        A_BetaSkullAttack,
        A_MissileAttack,
        A_MissileSpread,
        A_BulletAttack,
        A_ThingSummon,
        A_SargAttack12,
        A_MummyAttack,
        A_MummyAttack2,
        A_ClinkAttack,
        A_WizardAtk3,
        A_Srcr2Attack,
        A_GenWizard,
        A_HticExplode,
        A_KnightAttack,
        A_BeastAttack,
        A_SnakeAttack,
        A_Srcr1Attack,
        A_VolcanoBlast,
        A_MinotaurAtk1,
        A_MinotaurAtk2,
        A_MinotaurAtk3,
        A_MinotaurCharge,
        A_LichFire,
        A_ImpChargeAtk,
        A_ImpMeleeAtk,
        A_ImpMissileAtk,
        A_AlertMonsters,
    };

    if (attacks.count(st.action))
        return true;

    return false;
}

//
// B_stateHitscans
//
// True if this state has an instant-hit avoidable attack
//
static bool B_stateHitscans(statenum_t sn, const mobjinfo_t& mi, void* miscData)
{
    const state_t &st = *states[sn];
    static const std::unordered_set<ActionFunction> attacks =
    {
        A_PosAttack,
        A_SPosAttack,
        A_VileAttack,
        A_CPosAttack,
        A_Nailbomb,
        A_BulletAttack,
    };
    if (attacks.count(st.action))
        return true;
    return false;
}

//
// Checks if mobj is going to attack player
//
bool B_MobjHasMissileAttack(const Mobj& mo)
{
   auto d = (long)(mo.info - mobjinfo[0]);
   if(g_traitSet[d] & Trait_hostile)
   {
      return true;
   }
    const mobjinfo_t& mi = *mo.info;

    if (mi.spawnstate == NullStateNum || mi.missilestate == NullStateNum)
        return false;
    if (B_StateEncounters(mi.missilestate, &mo, nullptr, B_stateAttacks, true))
    {
       g_traitSet[d] |= Trait_hostile;
        return true;
    }

    return false;
}

//
// B_IsMobjHitscanner
//
// Checks if mobjinfo belongs to a hitscanner. Done simple enough because srsly
//
bool B_IsMobjHitscanner(const Mobj& mo)
{
   long d = (long)(mo.info - mobjinfo[0]);
   if(g_traitSet[d] & Trait_hitscan)
   {
      return true;
   }

    const mobjinfo_t& mi = *mo.info;
    if (mi.spawnstate == NullStateNum || mi.missilestate == NullStateNum)
        return false;
    if (B_StateEncounters(mi.missilestate, &mo, nullptr, B_stateHitscans, true))
    {
       g_traitSet[d] |= Trait_hitscan;
        return true;
    }

    return false;
}

//
// Checks if the encountered state causes an explosion. MiscData points to an
// int that may increase if the current frame explodes.
//
static bool B_stateExplodes(statenum_t sn, const mobjinfo_t &mi, void *miscData)
{
   int *damage = static_cast<int *>(miscData);
   const state_t &st = *states[sn];

   static auto increase = [damage](int value) {
      if(value > *damage)
         *damage = value;
   };

   // TODO: potential frames which spawn other explosives may count here!
   if(st.action == A_Explode || st.action == A_Mushroom || st.action == A_Nailbomb)
      increase(kExplosionRadius);
   else if(st.action == A_Detonate)
      increase(mi.damage);
   else if(st.action == A_DetonateEx)
   {
      int damage = E_ArgAsInt(st.args, 0, 128);
      if(damage > 0)
         increase(damage);
   }
   else if(st.action == A_HticExplode)
   {
      switch(E_ArgAsKwd(st.args, 0, &hticexpkwds, 0))
      {
         case kHticExplodeDsparilBSpark:
            increase(111);
            break;
         case kHticExplodeFloorFire:
            increase(24);
            break;
         default:
            increase(kExplosionRadius);
      }
   }

   return false;
}

//
// True if mobj's death contains explosions
//
int B_MobjDeathExplosion(const Mobj& mo)
{
   auto d = (long)(mo.info - mobjinfo[0]);
   if(g_traitSet[d] & Trait_explosiveDeath)
   {
      return true;
   }

    const mobjinfo_t& mi = *mo.info;
   if (mi.spawnstate == NullStateNum || mi.deathstate == NullStateNum)
      return false;
   int result = 0;
   B_StateEncounters(mi.deathstate, &mo, nullptr, B_stateExplodes, false, &result);
   return result;
}

//
// B_getPreAttackStates
//
// Returns the states happening right before first attack of monster.
// Useful for the bot to retreat before the monster starts attacking.
//
// May return nullptr if monster has no missile state.
//
static const StateSet *B_getPreAttackStates(const Mobj& mo)
{
    // Cache the results

    auto index = (long)(mo.info - mobjinfo[0]);
    auto found = preAttackStates.find(index);
    if (found != preAttackStates.end())
        return &found->second;   // found in cache

    const mobjinfo_t& mi = *mo.info;
    if (mi.spawnstate == NullStateNum || mi.missilestate == NullStateNum)
        return nullptr;

    B_StateEncounters(mi.missilestate, &mo, nullptr, [](statenum_t sn, const mobjinfo_t& mi, void* miscData) -> bool {

        auto& preAttackStates = *(StateSet*)miscData;
        static const std::unordered_set<ActionFunction> attacks =
        {
            A_Explode,
            A_PosAttack,
            A_SPosAttack,
            A_VileAttack,
            A_SkelFist,
            A_SkelMissile,
            A_FatAttack1,
            A_FatAttack2,
            A_FatAttack3,
            A_CPosAttack,
            A_TroopAttack,
            A_SargAttack,
            A_HeadAttack,
            A_BruisAttack,
            A_SkullAttack,
            A_BspiAttack,
            A_CyberAttack,
            A_PainAttack,
            A_PainDie,
            A_Detonate,
            A_Mushroom,
            A_Scratch,
            A_Nailbomb,
            // A_BetaSkullAttack is unavoidable
            A_MissileAttack,
            A_MissileSpread,
            A_BulletAttack,
            // A_ThingSummon might not be used for evil
            A_DetonateEx,
            A_SargAttack12,
            A_MummyAttack,
            A_MummyAttack2,
            A_ClinkAttack,
            A_WizardAtk3,
            A_Srcr2Attack,
            A_HticExplode,
            A_KnightAttack,
            A_BeastAttack,
            A_SnakeAttack,
            A_SnakeAttack2,
            A_VolcanoBlast, // maybe not useful
            A_MinotaurAtk1,
            A_MinotaurAtk2,
            A_MinotaurAtk3,
            A_MinotaurCharge,
            A_LichFire,
            A_LichWhirlwind,
            A_LichAttack,
            A_ImpChargeAtk, // hardly harmful
            A_ImpMeleeAtk,
            A_ImpMissileAtk,
        };

        const state_t &state = *::states[sn];
        if (attacks.count(state.action))
            return true;    // exit if encountered an attack state

        // Not encountered a state yet? Put it in the set.
        // Hopefully I won't place the walking state.
        preAttackStates.insert(sn);
        B_Log("For %d added state %d", (int)(&mi - mobjinfo[0]), (int)sn);

        return false;

    }, true, &preAttackStates[index]);

    return &preAttackStates[index];
}

//
// B_MonsterIsInPreAttack
//
// Returns true if monster is in a state just before the attack state, i.e.
// a state between the missilestate and the first firing state.
//
bool B_MonsterIsInPreAttack(const Mobj& mo)
{
   const StateSet* set = B_getPreAttackStates(mo);
   if(!set)
   {
      return false;
   }
   return set->count(mo.state->index) != 0;
}

//! True if this state's action is set
inline static bool B_stateUsesCodepointer(statenum_t sn, const mobjinfo_t &mi, void* miscData)
{
   return states[sn]->action == miscData;
}

//! Returns true if any of a monster's frames reaches an action pointer
bool B_MobjUsesCodepointer(const Mobj& mo, void(*action)(actionargs_t *args))
{
   const mobjinfo_t &mi = *mo.info;

   if (mi.spawnstate == NullStateNum)
      return false;  // has null start frame, invalid
   return B_StateEncounters(mi.spawnstate, &mo, nullptr, B_stateUsesCodepointer, false, reinterpret_cast<void *>(action));
}

//! Sets the SectorAffectingStates struct
static bool B_stateSetSectorState(statenum_t sn, const mobjinfo_t &mi, void *miscData)
{
   auto sas = static_cast<SectorAffectingStates *>(miscData);
   const state_t *state = states[sn];
   if (!sas->bossDeath && state->action == A_BossDeath)
      sas->bossDeath = state;
   else if (!sas->hticBossDeath && state->action == A_HticBossDeath)
      sas->hticBossDeath = state;
   else if (!sas->keenDie && state->action == A_KeenDie)
      sas->keenDie = state;
   else if (state->action == A_LineEffect)
      sas->lineEffects.add(state);
   return false;
}

//! Populates a collection with A_BossDeath, A_HticBossDeath, A_KeenDie or A_LineEffect-using
//! states
void B_GetMobjSectorTargetActions(const Mobj& mo, SectorAffectingStates &table)
{
   const mobjinfo_t &mi = *mo.info;

   if (mi.spawnstate == NullStateNum)
      return;  // has null start frame, invalid
   B_StateEncounters(mi.spawnstate, &mo, nullptr, B_stateSetSectorState, false, &table);
}

//
// True if monster calls PainAttack or ThingSummon or is an archvile
//
bool B_MobjIsSummoner(const Mobj &mo)
{
   // Cache the results here
   if(g_traitSet[mo.type] & Trait_summoner)
      return true;

   const mobjinfo_t &mi = *mo.info;
   if(mi.spawnstate ==  NullStateNum)
      return false;
   bool result = B_StateEncounters(mi.spawnstate, &mo, nullptr, [](statenum_t sn, const mobjinfo_t &mi, void *miscData) -> bool {

      return states[sn]->action == A_PainAttack ||
      states[sn]->action == A_ThingSummon ||
      states[sn]->action == A_VileChase;
   });
   if(result)
      g_traitSet[mo.type] |= Trait_summoner;
   return result;
}

// EOF

