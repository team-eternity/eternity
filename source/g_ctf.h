// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2012 Charles Gunyon
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
//----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Capture the flag game type.
//
//----------------------------------------------------------------------------

// [CG] Notably, CTF was largely written by Toke for Odamex, and although that
//      implementation differs significantly from this one, it's still the same
//      fundamental design.  Thanks :) .

#ifndef G_CTF_H__
#define G_CTF_H__

#include "cs_team.h"

// Map ID for flags
#define BLUE_FLAG_ID 5130
#define RED_FLAG_ID  5131

// Reserve for maintaining the DOOM CTF standard.
//#define ID_NEUTRAL_FLAG 5132
//#define ID_TEAM3_FLAG 5133
//#define ID_TEAM4_FLAG 5134

class Mobj;
struct patch_t;
struct player_t;

// flags can only be in one of these states
typedef enum
{
   flag_home,
   flag_dropped,
   flag_carried,
   NUMFLAGSTATES
} flag_state_t;

typedef struct flag_stand_s
{
   uint32_t net_id;
   fixed_t x;
   fixed_t y;
   fixed_t z;
} flag_stand_t;

// [CG] Flags are sent over the wire and therefore must be packed and use
//      exact-width integer types.

#pragma pack(push, 1)

// data associated with a flag
typedef struct flag_s
{
   // Net ID of this flag's actor when being carried by a player, follows
   // player
   uint32_t net_id;
   // Integer representation of WHO has each flag (player id)
   int32_t carrier;
   // [CG] Time at which the flag was picked up.
   uint32_t pickup_time;
   // Flag Timeout Counters
   uint32_t timeout;
   // True when a flag has been dropped
   int32_t state; // [CG] flag_state_t.
} flag_t;

#pragma pack(pop)

class CTFGameType : public DeathmatchGameType
{
public:
   CTFGameType(const char *new_name);
   int getStrengthOfVictory(float low_score, float high_score);
   bool usesFragsAsScore();
   bool usesFlagsAsScore();
   bool shouldExitLevel();
   void handleActorKilled(Mobj *source, Mobj *target, emod_t *mod);
   void handleActorRemoved(Mobj *actor);
   void handleLoadLevel();
   bool handleActorTouchedSpecial(Mobj *special, Mobj *toucher);
   void handleClientSpectated(int clientnum);
   void handleClientChangedTeam(int clientnum);
   void handleClientDisconnected(int clientnum);
   void tick();
};

extern flag_stand_t cs_flag_stands[team_color_max];
extern flag_t cs_flags[team_color_max];

void     SV_SpawnFlag(flag_t *flag);
void     CS_SnapCarriedFlagToCarrier(Mobj *flag_actor);
void     CS_CheckCarriedFlagPosition(int playernum);
bool     CS_ActorIsCarriedFlag(Mobj *actor);
patch_t* CS_GetFlagPatch(int flag_color);

#endif

