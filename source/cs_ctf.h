// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright (C) 2006-2010 by The Odamex Team.
// Copyright(C) 2011 Charles Gunyon
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
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
//    CTF Implementation
//
//---------------------------------------------------------------------------

// [CG] Notably, CTF was largely written by Toke for Odamex, and although that
//      implementation varies significantly from this one, it's still the same
//      fundamental design.  Thanks :) .

#ifndef CS_CTF_H__
#define CS_CTF_H__

#include "doomtype.h"
#include "doomstat.h"
#include "d_player.h"
#include "m_fixed.h"
#include "p_mobj.h"

#include "cs_team.h"

// Map ID for flags
#define BLUE_FLAG_ID 5130
#define RED_FLAG_ID  5131

// Reserve for maintaining the DOOM CTF standard.
//#define ID_NEUTRAL_FLAG 5132
//#define ID_TEAM3_FLAG 5133
//#define ID_TEAM4_FLAG 5134

struct patch_t;

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

extern flag_stand_t cs_flag_stands[team_color_max];
extern flag_t cs_flags[team_color_max];

void     CL_SpawnFlag(flag_t *flag, uint32_t net_id);
void     SV_SpawnFlag(flag_t *flag);
void     CS_PlayFlagSound(int flag_color);
void     CS_RemoveFlagActor(flag_t *flag);
void     CS_ReturnFlag(flag_t *flag);
void     CS_GiveFlag(int playernum, flag_t *flag);
void     CS_HandleFlagTouch(player_t *player, int color);
flag_t*  CS_GetFlagCarriedByPlayer(int playernum);
void     CS_DropFlag(int playernum);
void     CS_CTFTicker(void);
void     CS_SnapCarriedFlagToCarrier(Mobj *flag_actor);
void     CS_CheckCarriedFlagPosition(int playernum);
bool     CS_ActorIsCarriedFlag(Mobj *actor);
Mobj*    CS_GetFlagStandActorForFlag(flag_t *flag);
patch_t* CS_GetFlagPatch(int flag_color);

#endif

