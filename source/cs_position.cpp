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
//   Position functions and data structures.
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include <stdio.h>

#include "doomstat.h"
#include "g_game.h"
#include "p_maputl.h"
#include "p_mobj.h"

#include "cs_main.h"
#include "cs_position.h"

// [CG] Actor positions.

void CS_PrintActorPosition(cs_actor_position_t *position)
{
   printf(
      "%u: %5d/%5d/%5d %5d/%5d/%5d %3d %3d",
      position->world_index,
      position->x >> FRACBITS,
      position->y >> FRACBITS,
      position->z >> FRACBITS,
      position->momx >> FRACBITS,
      position->momy >> FRACBITS,
      position->momz >> FRACBITS,
      position->angle / ANGLE_1,
      position->floatbob
   );
}

void CS_PrintPositionForActor(Mobj *actor, uint32_t index)
{
   cs_actor_position_t position;

   CS_SaveActorPosition(&position, actor, index);
   CS_PrintActorPosition(&position);
}

void CS_SetActorPosition(Mobj *actor, cs_actor_position_t *position)
{
   P_UnsetThingPosition(actor);
   actor->x        = position->x;
   actor->y        = position->y;
   actor->z        = position->z;
   actor->momx     = position->momx;
   actor->momy     = position->momy;
   actor->momz     = position->momz;
   actor->angle    = position->angle;
   actor->floatbob = position->floatbob;
   P_SetThingPosition(actor);
}

bool CS_ActorPositionEquals(Mobj *actor, cs_actor_position_t *position)
{
   if(actor->x        == position->x     &&
      actor->y        == position->y     &&
      actor->z        == position->z     &&
      actor->momx     == position->momx  &&
      actor->momy     == position->momy  &&
      actor->momz     == position->momz  &&
      actor->angle    == position->angle &&
      actor->floatbob == position->floatbob)
   {
      return true;
   }
   return false;
}

void CS_SaveActorPosition(cs_actor_position_t *position, Mobj *actor,
                          uint32_t index)
{
   position->world_index = index;
   position->x           = actor->x;
   position->y           = actor->y;
   position->z           = actor->z;
   position->momx        = actor->momx;
   position->momy        = actor->momy;
   position->momz        = actor->momz;
   position->angle       = actor->angle;
   position->floatbob    = actor->floatbob;
}

bool CS_ActorPositionChanged(Mobj *actor)
{
   if(CS_ActorPositionEquals(actor, &actor->old_position))
      return false;
   return true;
}

bool CS_ActorPositionsEqual(cs_actor_position_t *position_one,
                            cs_actor_position_t *position_two)
{
   if(position_one->x        == position_two->x     &&
      position_one->y        == position_two->y     &&
      position_one->z        == position_two->z     &&
      position_one->momx     == position_two->momx  &&
      position_one->momy     == position_two->momy  &&
      position_one->momz     == position_two->momz  &&
      position_one->angle    == position_two->angle &&
      position_one->floatbob == position_two->floatbob)
   {
      return true;
   }
   return false;
}

void CS_CopyActorPosition(cs_actor_position_t *dest, cs_actor_position_t *src)
{
   dest->x        = src->x;
   dest->y        = src->y;
   dest->z        = src->z;
   dest->momx     = src->momx;
   dest->momy     = src->momy;
   dest->momz     = src->momz;
   dest->angle    = src->angle;
   dest->floatbob = src->floatbob;
}

// [CG] Miscellaneous state.

void CS_PrintMiscState(cs_misc_state_t *state)
{
   printf(
      "%u: %d/%d/%d/%d/%d %d/%d %d",
      state->world_index,
      state->flags,
      state->flags2,
      state->flags3,
      state->flags4,
      state->intflags,
      state->friction,
      state->movefactor,
      state->reactiontime
   );
}

void CS_SetActorMiscState(Mobj *actor, cs_misc_state_t *state)
{
   actor->flags        = state->flags;
   actor->flags2       = state->flags2;
   actor->flags3       = state->flags3;
   actor->flags4       = state->flags4;
   actor->intflags     = state->intflags;
   actor->friction     = state->friction;
   actor->movefactor   = state->movefactor;
   actor->reactiontime = state->reactiontime;
}

bool CS_ActorMiscStateEquals(Mobj *actor, cs_misc_state_t *state)
{
   if(actor->flags        == state->flags        &&
      actor->flags2       == state->flags2       &&
      actor->flags3       == state->flags3       &&
      actor->flags4       == state->flags4       &&
      actor->intflags     == state->intflags     &&
      actor->friction     == state->friction     &&
      actor->movefactor   == state->movefactor   && 
      actor->reactiontime == state->reactiontime)
   {
      return true;
   }
   return false;
}

void CS_SaveActorMiscState(cs_misc_state_t *state, Mobj *actor, uint32_t index)
{
   state->world_index  = index;
   state->flags        = actor->flags;
   state->flags2       = actor->flags2;
   state->flags3       = actor->flags3;
   state->flags4       = actor->flags4;
   state->intflags     = actor->intflags;
   state->friction     = actor->friction;
   state->movefactor   = actor->movefactor;
   state->reactiontime = actor->reactiontime;
}

bool CS_ActorMiscStateChanged(Mobj *actor)
{
   if(CS_ActorMiscStateEquals(actor, &actor->old_misc_state))
      return false;
   return true;
}

bool CS_MiscStatesEqual(cs_misc_state_t *state_one, cs_misc_state_t *state_two)
{
   if(state_one->flags           == state_two->flags           &&
      state_one->flags2          == state_two->flags2          &&
      state_one->flags3          == state_two->flags3          &&
      state_one->flags4          == state_two->flags4          &&
      state_one->intflags        == state_two->intflags        &&
      state_one->friction        == state_two->friction        &&
      state_one->movefactor      == state_two->movefactor      &&
      state_one->reactiontime    == state_two->reactiontime)
   {
      return true;
   }
   return false;
}

void CS_CopyMiscState(cs_misc_state_t *dest, cs_misc_state_t *src)
{
   dest->flags           = src->flags;
   dest->flags2          = src->flags2;
   dest->flags3          = src->flags3;
   dest->flags4          = src->flags4;
   dest->intflags        = src->intflags;
   dest->friction        = src->friction;
   dest->movefactor      = src->movefactor;
   dest->reactiontime    = src->reactiontime;
}

// [CG] Player positions.

void CS_PrintPlayerPosition(cs_player_position_t *position)
{
   printf(
      "%u: %5d/%5d/%5d %5d/%5d/%5d %3d %3d",
      position->world_index,
      position->x >> FRACBITS,
      position->y >> FRACBITS,
      position->z >> FRACBITS,
      position->momx >> FRACBITS,
      position->momy >> FRACBITS,
      position->momz >> FRACBITS,
      position->angle / ANGLE_1
   );
}

void CS_PrintPositionForPlayer(int playernum, uint32_t index)
{
   cs_player_position_t position;

   CS_SavePlayerPosition(&position, playernum, index);
   CS_PrintPlayerPosition(&position);
}

void CS_SetPlayerPosition(int playernum, cs_player_position_t *position)
{
   player_t *player = &players[playernum];

   P_UnsetThingPosition(player->mo);
   player->mo->x           = position->x;
   player->mo->y           = position->y;
   player->mo->z           = position->z;
   player->mo->momx        = position->momx;
   player->mo->momy        = position->momy;
   player->mo->momz        = position->momz;
   player->mo->angle       = position->angle;
   player->pitch           = position->pitch;
   player->bob             = position->bob;
   player->viewz           = position->viewz;
   player->viewheight      = position->viewheight;
   player->deltaviewheight = position->deltaviewheight;
   player->jumptime        = position->jumptime;
   player->playerstate     = position->playerstate;
   P_SetThingPosition(player->mo);
}

bool CS_PlayerPositionEquals(int playernum, cs_player_position_t *position)
{
   player_t *player = &players[playernum];

   if(player->mo->x           == position->x               &&
      player->mo->y           == position->y               &&
      player->mo->z           == position->z               &&
      player->mo->momx        == position->momx            &&
      player->mo->momy        == position->momy            &&
      player->mo->momz        == position->momz            &&
      player->mo->angle       == position->angle           &&
      player->pitch           == position->pitch           &&
      player->bob             == position->bob             &&
      player->viewz           == position->viewz           &&
      player->viewheight      == position->viewheight      &&
      player->deltaviewheight == position->deltaviewheight &&
      player->jumptime        == position->jumptime        &&
      player->playerstate     == position->playerstate)
   {
      return true;
   }
   return false;
}

void CS_SavePlayerPosition(cs_player_position_t *position, int playernum,
                           uint32_t index)
{
   player_t *player = &players[playernum];

   position->world_index     = index;
   position->x               = player->mo->x;
   position->y               = player->mo->y;
   position->z               = player->mo->z;
   position->momx            = player->mo->momx;
   position->momy            = player->mo->momy;
   position->momz            = player->mo->momz;
   position->angle           = player->mo->angle;
   position->pitch           = player->pitch;
   position->bob             = player->bob;
   position->viewz           = player->viewz;
   position->viewheight      = player->viewheight;
   position->deltaviewheight = player->deltaviewheight;
   position->jumptime        = player->jumptime;
   position->playerstate     = player->playerstate;
}

bool CS_PlayerPositionChanged(int playernum)
{
   player_t *player = &players[playernum];

   if(CS_PlayerPositionEquals(playernum, &player->old_position))
      return false;
   return true;
}

bool CS_PlayerPositionsEqual(cs_player_position_t *position_one,
                             cs_player_position_t *position_two)
{
   if(position_one->x               == position_two->x               &&
      position_one->y               == position_two->y               &&
      position_one->z               == position_two->z               &&
      position_one->momx            == position_two->momx            &&
      position_one->momy            == position_two->momy            &&
      position_one->momz            == position_two->momz            &&
      position_one->angle           == position_two->angle           &&
      position_one->pitch           == position_two->pitch           &&
      position_one->bob             == position_two->bob             &&
      position_one->viewz           == position_two->viewz           &&
      position_one->viewheight      == position_two->viewheight      &&
      position_one->deltaviewheight == position_two->deltaviewheight &&
      position_one->jumptime        == position_two->jumptime        &&
      position_one->playerstate     == position_two->playerstate)
   {
      return true;
   }
   return false;
}

void CS_CopyPlayerPosition(cs_player_position_t *dest,
                           cs_player_position_t *src)
{
   dest->x               = src->x;
   dest->y               = src->y;
   dest->z               = src->z;
   dest->momx            = src->momx;
   dest->momy            = src->momy;
   dest->momz            = src->momz;
   dest->angle           = src->angle;
   dest->pitch           = src->pitch;
   dest->bob             = src->bob;
   dest->viewz           = src->viewz;
   dest->viewheight      = src->viewheight;
   dest->deltaviewheight = src->deltaviewheight;
   dest->jumptime        = src->jumptime;
   dest->playerstate     = src->playerstate;
}

