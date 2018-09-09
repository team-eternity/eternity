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

#include "../doomdef.h"
#include "../m_collection.h"
#include "../m_fixed.h"

class Mobj;
struct actionargs_t;
struct mobjinfo_t;
struct player_t;
struct state_t;

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

bool B_IsMobjSolidDecor(const Mobj &mo);
bool B_MobjHasMissileAttack(const Mobj& mo);
bool B_IsMobjHitscanner(const Mobj& mo);
int B_MobjDeathExplosion(const Mobj& mo);
bool B_MonsterIsInPreAttack(const Mobj& mo);
bool B_MobjUsesCodepointer(const Mobj& mo, void(*action)(actionargs_t *args));
void B_GetMobjSectorTargetActions(const Mobj& mo, SectorAffectingStates &table);
bool B_MobjIsSummoner(const Mobj &mo);

//
// WEAPON ANALYSIS
//

enum BotWeaponType
{
   kWeaponFist,
};

// Bot weapon info flags
enum
{
   BWI_MELEE =       0x00000001,
   BWI_BERSERK =     0x00000002,
   BWI_MELEE_ONLY =  0x00000004,

   BWI_HITSCAN =     0x00000008,
   BWI_SNIPE =       0x00000010,
   BWI_TAP_SNIPE =   0x00000020,

   BWI_MISSILE =     0x00000040,
   BWI_DANGEROUS =   0x00000080,
   BWI_ULTIMATE =    0x00000100
};

// There'll be NUMWEAPONS instances of this
struct BotWeaponInfo
{
   uint32_t flags;
    
   int timeToFire;            // time before first fire
   int refireRate;            // time between two attacks
   int oneShotRate;           // duration without holding fire
   int burstRate;             // max time between individual shots

   int meleeDamage;           // total non-berserk melee damage
   int berserkDamage;         // total berserk melee damage

   int alwaysDamage;          // total 100% accurate bullet damage
   int firstDamage;           // total tappable bullet damage
   int neverDamage;           // total "never" accuracy bullet damage
   int monsterDamage;         // total "monster" accuracy bullet damage
   int ssgDamage;             // total "ssg" accuracy bullet damage

   int projectileDamage;      // total projectile impact damage
   fixed_t projectileSpeed;   // maximum projectile speed
   fixed_t projectileRadius;  // maximum projectile radius
   int explosionDamage;       // total projectile explosion damage
   int explosionRadius;       // maximum projectile explosion radius
   bool unsafeExplosion;      // true if explosion hurts shooter
   int bfgCount;              // number of BFG effects in impact
   bool seeking;              // true if it seeks the target

   int calcHitscanDamage(fixed_t dist, fixed_t radius, fixed_t height, bool berserk, bool first) const;  // calculates hitscan damage at given range
};

extern BotWeaponInfo g_botweapons[NUMWEAPONS];

void B_AnalyzeWeapons();

#endif 

// EOF

