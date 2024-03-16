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
//      Enemy thinking, AI.
//      Action Pointer Functions
//      that are associated with states/frames.
//
//-----------------------------------------------------------------------------

#ifndef P_ENEMY_H__
#define P_ENEMY_H__

// Required for pr_class_t, statenum_t
#include "info.h"
#include "m_random.h"

enum 
{
   DI_EAST,
   DI_NORTHEAST,
   DI_NORTH,
   DI_NORTHWEST,
   DI_WEST,
   DI_SOUTHWEST,
   DI_SOUTH,
   DI_SOUTHEAST,
   DI_NODIR,
   NUMDIRS
};
typedef int dirtype_t;

extern fixed_t xspeed[8];
extern fixed_t yspeed[8];

extern int p_lastenemyroar;

bool P_CheckMissileRange(Mobj *actor);
bool P_HelpFriend(Mobj *actor);
bool P_HitFriend(Mobj *actor);
bool P_LookForPlayers(Mobj *actor, int allaround);
bool P_LookForTargets(Mobj *actor, int allaround);
int  P_Move(Mobj *actor, int dropoff); // killough 9/12/98
void P_NewChaseDir(Mobj *actor);
bool P_SmartMove(Mobj *actor);

void P_NoiseAlert (Mobj *target, Mobj *emmiter);
void P_SpawnBrainTargets();     // killough 3/26/98: spawn icon landings
void P_SpawnSorcSpots();        // haleyjd 11/19/02: spawn dsparil spots

extern struct brain_s // killough 3/26/98: global state of boss brain
{
   int easy;
} brain;

bool P_CheckRange(Mobj *actor, fixed_t range);
bool P_CheckMeleeRange(Mobj *actor);

// haleyjd 07/13/03: editable boss brain spawn types
// schepe: removed 11-type limit
extern int NumBossTypes;
extern int *BossSpawnTypes;
extern int *BossSpawnProbs;

enum
{
   BOSSTELE_NONE,
   BOSSTELE_ORIG,
   BOSSTELE_BOTH,
   BOSSTELE_DEST
};

class MobjCollection;

// haleyjd: bossteleport_t
//
// holds a ton of information to allow for the teleportation of
// a thing with various effects
//
struct bossteleport_t
{
   MobjCollection *mc;          // mobj collection to use
   pr_class_t      rngNum;      // rng number to use for selecting spot

   Mobj           *boss;        // boss to teleport
   statenum_t      state;       // number of state to put boss in (-1 to not)

   mobjtype_t      fxtype;      // type of effect object to spawn
   fixed_t         zpamt;       // amount to add of z coordinate of effect
   int             hereThere;   // locations to spawn effects at (0, 1, or 2)
   int             soundNum;    // sound to play at locations
   fixed_t         minDistance; // minimum distance a spot must be from origin
};

void P_BossTeleport(bossteleport_t *bt);

void P_SkullFly(Mobj *actor, fixed_t speed, bool useSeeState);

int P_GetAimShift(Mobj *target, bool missile);

#endif // __P_ENEMY__

//----------------------------------------------------------------------------
//
// $Log: p_enemy.h,v $
// Revision 1.1  1998/05/03  22:29:32  killough
// External declarations formerly in p_local.h
//
//
//----------------------------------------------------------------------------
