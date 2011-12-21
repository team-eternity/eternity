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

#include <stdio.h>

#include "doomstat.h"
#include "g_game.h"
#include "p_maputl.h"
#include "p_mobj.h"

#include "cs_main.h"
#include "cs_position.h"

// [CG] For debugging, really.
void CS_PrintPosition(position_t *position)
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
      position->pitch >> FRACBITS,
   );
}

void CS_PrintActorPosition(Mobj *actor, unsigned int index)
{
   position_t position;

   CS_SaveActorPosition(&position, actor, index);
   CS_PrintPosition(&position);
}

void CS_PrintPlayerPosition(int playernum, unsigned int index)
{
   CS_PrintActorPosition(players[playernum].mo, index);
}

void CS_SetActorPosition(Mobj *actor, position_t *position)
{
   P_UnsetThingPosition(actor);
   actor->x     = position->x;
   actor->y     = position->y;
   actor->z     = position->z;
   actor->momx  = position->momx;
   actor->momy  = position->momy;
   actor->momz  = position->momz;
   actor->angle = position->angle;
   if(actor->player)
      actor->player->pitch = position->pitch;
   P_SetThingPosition(actor);
}

void CS_SetPlayerPosition(int playernum, position_t *position)
{
   CS_SetActorPosition(players[playernum].mo, position);
}

bool CS_ActorPositionEquals(Mobj *actor, position_t *position)
{
   if(actor->x                                 == position->x     &&
      actor->y                                 == position->y     &&
      actor->z                                 == position->z     &&
      actor->momx                              == position->momx  &&
      actor->momy                              == position->momy  &&
      actor->momz                              == position->momz  &&
      actor->angle                             == position->angle &&
      (!actor->player || (actor->player->pitch == position->pitch)))
   {
      return true;
   }
   return false;
}

void CS_SaveActorPosition(position_t *position, Mobj *actor, int index)
{
   position->world_index = index;
   position->x           = actor->x;
   position->y           = actor->y;
   position->z           = actor->z;
   position->momx        = actor->momx;
   position->momy        = actor->momy;
   position->momz        = actor->momz;
   position->angle       = actor->angle;
   if(actor->player)
      position->pitch = actor->player->pitch;
}

bool CS_ActorPositionChanged(Mobj *actor)
{
   if(CS_ActorPositionEquals(actor, &actor->old_position))
      return false;
   return true;
}

bool CS_PositionsEqual(position_t *position_one, position_t *position_two)
{
   if(position_one->x     == position_two->x     &&
      position_one->y     == position_two->y     &&
      position_one->z     == position_two->z     &&
      position_one->momx  == position_two->momx  &&
      position_one->momy  == position_two->momy  &&
      position_one->momz  == position_two->momz  &&
      position_one->angle == position_two->angle &&
      position_one->pitch == position_two->pitch)
   {
      return true;
   }
   return false;
}

void CS_CopyPosition(position_t *dest, position_t *src)
{
   dest->x     = src->x;
   dest->y     = src->y;
   dest->z     = src->z;
   dest->momx  = src->momx;
   dest->momy  = src->momy;
   dest->momz  = src->momz;
   dest->angle = src->angle;
   dest->pitch = src->pitch;
}

// [CG] For debugging, really.
void CS_PrintMiscState(misc_state_t *state)
{
   printf(
      "%u: %d/%d/%d/%d/%d %d/%d %d %d/%d %d/%d/%d %d %d",
      state->world_index,
      state->flags,
      state->flags2,
      state->flags3,
      state->flags4,
      state->intflags,
      state->friction,
      state->movefactor,
      state->reactiontime,
      state->floatbob,
      state->bob,
      state->viewz,
      state->viewheight,
      state->deltaviewheight,
      state->jumptime,
      state->playerstate
   );
}

void CS_PrintActorMiscState(Mobj *actor, unsigned int index)
{
   position_t position;

   CS_SaveActorMiscState(&position, actor, index);
   CS_PrintMiscState(&position);
}

void CS_PrintPlayerMiscState(int playernum, unsigned int index)
{
   CS_PrintActorMiscState(players[playernum].mo, index);
}

void CS_SetActorMiscState(Mobj *actor, misc_state_t *state)
{
   actor->flags        = state->flags;
   actor->flags2       = state->flags2;
   actor->flags3       = state->flags3;
   actor->flags4       = state->flags4;
   actor->intflags     = state->intflags;
   actor->friction     = state->friction;
   actor->movefactor   = state->movefactor;
   actor->reactiontime = state->reactiontime;
   actor->floatbob     = state->floatbob;
   if(actor->player)
   {
      actor->player->bob             = state->bob;
      actor->player->viewz           = state->viewz;
      actor->player->viewheight      = state->viewheight;
      actor->player->deltaviewheight = state->deltaviewheight;
      actor->player->jumptime        = state->jumptime;
      actor->player->playerstate     = state->playerstate;
   }
}

void CS_SetPlayerMiscState(int playernum, misc_state_t *state)
{
   CS_SetActorPosition(players[playernum].mo, state);
}

bool CS_ActorMiscStateEquals(Mobj *actor, misc_state_t *state)
{
   if(actor->flags        == state->flags        &&
      actor->flags2       == state->flags2       &&
      actor->flags3       == state->flags3       &&
      actor->flags4       == state->flags4       &&
      actor->intflags     == state->intflags     &&
      actor->friction     == state->friction     &&
      actor->movefactor   == state->movefactor   && 
      actor->reactiontime == state->reactiontime &&
      actor->floatbob     == state->floatbob     &&
      (!actor->player || (
      actor->player->bob             == state->bob             &&
      actor->player->viewz           == state->viewz           &&
      actor->player->viewheight      == state->viewheight      &&
      actor->player->deltaviewheight == state->deltaviewheight &&
      actor->player->jumptime        == state->jumptime        &&
      actor->player->playerstate     == state->playerstate
      )))
   {
      return true;
   }
   return false;
}

void CS_SaveActorMiscState(misc_state_t *state, Mobj *actor, int index)
{
   state->world_index  = index;
   state->flags        = actor->flags;
   state->flags2       = actor->flags2;
   state->flags3       = actor->flags3;
   state->flags4       = actor->flags4;
   state->intflag      = actor->intflags;
   state->friction     = actor->friction;
   state->movefactor   = actor->movefactor;
   state->reactiontime = actor->reactiontime;
   state->floatbob     = actor->floatbob;
   if(actor->player)
   {
      state->bob             = actor->player->bob;
      state->viewz           = actor->player->viewz;
      state->viewheight      = actor->player->viewheight;
      state->deltaviewheight = actor->player->deltaviewheight;
      state->jumptime        = actor->player->jumptime;
      state->playerstate     = actor->player->playerstate;
   }
}

bool CS_ActorMiscStateChanged(Mobj *actor)
{
   if(CS_ActorMiscStateEquals(actor, &actor->old_misc_state))
      return false;
   return true;
}

bool CS_MiscStatesEqual(misc_state_t *state_one, misc_state_t *state_two)
{
   if(state_one->flags           == state_two->flags           &&
      state_one->flags2          == state_two->flags2          &&
      state_one->flags3          == state_two->flags3          &&
      state_one->flags4          == state_two->flags4          &&
      state_one->intflags        == state_two->intflags        &&
      state_one->friction        == state_two->friction        &&
      state_one->movefactor      == state_two->movefactor      &&
      state_one->reactiontime    == state_two->reactiontime    &&
      state_one->floatbob        == state_two->floatbob        &&
      state_one->bob             == state_two->bob             &&
      state_one->viewz           == state_two->viewz           &&
      state_one->viewheight      == state_two->viewheight      &&
      state_one->deltaviewheight == state_two->deltaviewheight &&
      state_one->jumptime        == state_two->jumptime        &&
      state_one->playerstate     == state_two->playerstate)
   {
      return true;
   }
   return false;
}

void CS_CopyMiscState(misc_state_t *dest, misc_state_t *src)
{
   state_one->flags           = state_two->flags;
   state_one->flags2          = state_two->flags2;
   state_one->flags3          = state_two->flags3;
   state_one->flags4          = state_two->flags4;
   state_one->intflags        = state_two->intflags;
   state_one->friction        = state_two->friction;
   state_one->movefactor      = state_two->movefactor;
   state_one->reactiontime    = state_two->reactiontime;
   state_one->floatbob        = state_two->floatbob;
   state_one->bob             = state_two->bob;
   state_one->viewz           = state_two->viewz;
   state_one->viewheight      = state_two->viewheight;
   state_one->deltaviewheight = state_two->deltaviewheight;
   state_one->jumptime        = state_two->jumptime;
   state_one->playerstate     = state_two->playerstate;
}

