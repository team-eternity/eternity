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

#ifndef B_ANALYSIS_H_
#define B_ANALYSIS_H_

#include <queue>
#include <unordered_set>
#include "../info.h"
#include "../m_collection.h"

typedef std::unordered_set<statenum_t> StateSet;
typedef std::queue<statenum_t> StateQue;

// external dependencies
extern void A_BossDeath(actionargs_t *actionargs);
extern void A_HticBossDeath(actionargs_t *actionargs);
extern void A_KeenDie(actionargs_t *actionargs);
extern void A_LineEffect(actionargs_t *actionargs);

//! List of pointers to sector-affecting states
struct SectorAffectingStates
{
   const state_t *bossDeath;
   const state_t *hticBossDeath;
   const state_t *keenDie;
   PODCollection<const state_t *> lineEffects;

   void clear()
   {
      bossDeath = hticBossDeath = keenDie = nullptr;
      lineEffects.makeEmpty();
   }
};

void B_UpdateMobjInfoSet(int numthingsalloc);
void B_UpdateStateInfoSet(int numstates);

bool B_StateEncounters(statenum_t firstState, const Mobj *mo, const mobjinfo_t *info,
                       bool(*statecase)(statenum_t, const mobjinfo_t&, void* miscData),
                       bool avoidPainStates = false, void* miscData = nullptr);

bool B_IsMobjSolidDecor(const Mobj &mo);
bool B_MobjHasMissileAttack(const Mobj& mo);
bool B_IsMobjHitscanner(const Mobj& mo);
int B_MobjDeathExplosion(const Mobj& mo);
bool B_MonsterIsInPreAttack(const Mobj& mo);
bool B_MobjUsesCodepointer(const Mobj& mo, void(*action)(actionargs_t *args));
void B_GetMobjSectorTargetActions(const Mobj& mo, SectorAffectingStates &table);
bool B_MobjIsSummoner(const Mobj &mo);

#endif 

// EOF

