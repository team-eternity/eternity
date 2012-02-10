// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2011 Charles Gunyon
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
//   Position functions and data structure.
//
//----------------------------------------------------------------------------

#ifndef CS_POSITION_H__
#define CS_POSITION_H__

#include "tables.h"

// [CG] A position really is everything that's critical to an actor's or
//      player's position at any given TIC.  When prediction is incorrect, the
//      correct position/state (provided by the server) is loaded and
//      subsequent positions/states are repredicted.  Note that this is really
//      only for players; even though actors have a ".old_position" member,
//      this is only used by the server to avoid sending positions for every
//      actor each TIC when possible.

// [CG] Positions are sent over the network and stored in demos, and therefore
//      must be packed and use exact-width integer types.

#pragma pack(push, 1)

typedef struct
{
   uint32_t world_index;
   fixed_t x;
   fixed_t y;
   fixed_t z;
   fixed_t momx;
   fixed_t momy;
   fixed_t momz;
   angle_t angle;
   int32_t floatbob;
} cs_actor_position_t;

// [CG] Same with player positions.
typedef struct 
{
   uint32_t world_index;
   fixed_t x;
   fixed_t y;
   fixed_t z;
   fixed_t momx;
   fixed_t momy;
   fixed_t momz;
   angle_t angle;
   int32_t floatbob;
   fixed_t pitch;
   int32_t bob;
   int32_t viewz;
   int32_t viewheight;
   int32_t deltaviewheight;
   int32_t jumptime;
} cs_player_position_t;

// [CG] Same with miscellaneous state.
typedef struct
{
   uint32_t world_index;
   uint32_t flags;
   uint32_t flags2;
   uint32_t flags3;
   uint32_t flags4;
   int32_t intflags;
   int32_t friction;
   int32_t movefactor;
   int32_t reactiontime;
} cs_misc_state_t;

#pragma pack(pop)

void CS_PrintActorPosition(cs_actor_position_t *position);
void CS_PrintPositionForActor(Mobj *actor, uint32_t index);
void CS_SetActorPosition(Mobj *actor, cs_actor_position_t *position);
bool CS_ActorPositionEquals(Mobj *actor, cs_actor_position_t *position);
void CS_SaveActorPosition(cs_actor_position_t *position, Mobj *actor,
                          uint32_t index);
bool CS_ActorPositionChanged(Mobj *actor);
bool CS_ActorPositionsEqual(cs_actor_position_t *position_one,
                            cs_actor_position_t *position_two);
void CS_CopyActorPosition(cs_actor_position_t *dest, cs_actor_position_t *src);

void CS_PrintMiscState(cs_misc_state_t *state);
void CS_SetActorMiscState(Mobj *actor, cs_misc_state_t *state);
bool CS_ActorMiscStateEquals(Mobj *actor, cs_misc_state_t *state);
void CS_SaveActorMiscState(cs_misc_state_t *state, Mobj *actor,
                           uint32_t index);
bool CS_ActorMiscStateChanged(Mobj *actor);
bool CS_MiscStatesEqual(cs_misc_state_t *state_one,
                        cs_misc_state_t *state_two);
void CS_CopyMiscState(cs_misc_state_t *dest, cs_misc_state_t *src);

void CS_PrintPlayerPosition(cs_player_position_t *position);
void CS_PrintPositionForPlayer(int playernum, uint32_t index);
void CS_SetPlayerPosition(int playernum, cs_player_position_t *position);
bool CS_PlayerPositionEquals(int playernum, cs_player_position_t *position);
void CS_SavePlayerPosition(cs_player_position_t *position, int playernum,
                           uint32_t index);
bool CS_PlayerPositionChanged(int playernum);
bool CS_PlayerPositionsEqual(cs_player_position_t *position_one,
                             cs_player_position_t *position_two);
void CS_CopyPlayerPosition(cs_player_position_t *dest,
                           cs_player_position_t *src);

#endif

