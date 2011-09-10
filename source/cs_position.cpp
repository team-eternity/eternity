// Emacs style mode select -*- C -*- vim:sw=3 ts=3:
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
   // [CG] Actual position members.
   printf(
      "%u: %5d/%5d/%5d %5d/%5d/%5d %3d ",
      position->world_index,
      position->x >> FRACBITS,
      position->y >> FRACBITS,
      position->z >> FRACBITS,
      position->momx >> FRACBITS,
      position->momy >> FRACBITS,
      position->momz >> FRACBITS,
      position->angle / ANGLE_1
   );

   // [CG] Sector-relative information.
   printf(
      "%5d/%5d/%5d ",
      position->ceilingz >> FRACBITS,
      position->floorz >> FRACBITS,
      position->floorclip >> FRACBITS
   );

   // [CG] Flags.
   printf(
      "%d/%d/%d/%d/%d ",
      position->flags,
      position->flags2,
      position->flags3,
      position->flags4,
      position->intflags
   );

   // [CG] Miscellaneous.
   printf(
      "%d/%d/%d/%d",
      position->friction,
      position->movefactor,
      position->alphavelocity,
      position->reactiontime
   );

   // [CG] Bobbing and viewheight; this is below miscellaneous because most of
   //      it is related to the player, and because player-only members in the
   //      position come last.  Also pitch, jumptime and playerstate are tacked
   //      onto the end.
   printf(
      "%d/%d/%d/%d/%d %5d %d %d\n",
      position->floatbob,
      position->bob,
      position->viewz,
      position->viewheight,
      position->deltaviewheight,
      position->pitch >> FRACBITS,
      position->jumptime,
      position->playerstate
   );
}

void CS_PrintActorPosition(mobj_t *actor, unsigned int index)
{
   position_t position;

   CS_SaveActorPosition(&position, actor, index);
   CS_PrintPosition(&position);
}

void CS_PrintPlayerPosition(int playernum, unsigned int index)
{
   CS_PrintActorPosition(players[playernum].mo, index);
}

void CS_SetActorPosition(mobj_t *actor, position_t *position)
{
   P_UnsetThingPosition(actor);
   actor->x             = position->x;
   actor->y             = position->y;
   actor->z             = position->z;
   actor->momx          = position->momx;
   actor->momy          = position->momy;
   actor->momz          = position->momz;
   actor->angle         = position->angle;
   actor->ceilingz      = position->ceilingz;
   actor->floorz        = position->floorz;
   actor->floorclip     = position->floorclip;
   actor->flags         = position->flags;
   actor->flags2        = position->flags2;
   actor->flags3        = position->flags3;
   actor->flags4        = position->flags4;
   actor->intflags      = position->intflags;
   actor->friction      = position->friction;
   actor->movefactor    = position->movefactor;
   actor->alphavelocity = position->alphavelocity;
   actor->reactiontime  = position->reactiontime;
   actor->floatbob      = position->floatbob;
   if(actor->player)
   {
      actor->player->bob             = position->bob;
      actor->player->viewz           = position->viewz;
      actor->player->viewheight      = position->viewheight;
      actor->player->deltaviewheight = position->deltaviewheight;
      actor->player->pitch           = position->pitch;
      actor->player->jumptime        = position->jumptime;
   }
   P_SetThingPosition(actor);
}

void CS_SetPlayerPosition(int playernum, position_t *position)
{
   CS_SetActorPosition(players[playernum].mo, position);
}

boolean CS_ActorPositionEquals(mobj_t *actor, position_t *position)
{
   if(actor->x                       == position->x               &&
      actor->y                       == position->y               &&
      actor->z                       == position->z               &&
      actor->momx                    == position->momx            &&
      actor->momy                    == position->momy            &&
      actor->momz                    == position->momz            &&
      actor->angle                   == position->angle           &&
      actor->ceilingz                == position->ceilingz        &&
      actor->floorz                  == position->floorz          &&
      actor->floorclip               == position->floorclip       &&
      actor->flags                   == position->flags           &&
      actor->flags2                  == position->flags2          &&
      actor->flags3                  == position->flags3          &&
      actor->flags4                  == position->flags4          &&
      actor->intflags                == position->intflags        &&
      actor->friction                == position->friction        &&
      actor->movefactor              == position->movefactor      &&
      actor->alphavelocity           == position->alphavelocity   &&
      actor->reactiontime            == position->reactiontime    &&
      actor->floatbob                == position->floatbob        &&
      (!actor->player || (
      actor->player->bob             == position->bob             &&
      actor->player->viewz           == position->viewz           &&
      actor->player->viewheight      == position->viewheight      &&
      actor->player->deltaviewheight == position->deltaviewheight &&
      actor->player->pitch           == position->pitch           &&
      actor->player->jumptime        == position->jumptime        &&
      actor->player->playerstate     == position->playerstate)))
   {
      return true;
   }
   return false;
}

void CS_SaveActorPosition(position_t *position, mobj_t *actor, int index)
{
   position->world_index        = index;
   position->x                  = actor->x;
   position->y                  = actor->y;
   position->z                  = actor->z;
   position->momx               = actor->momx;
   position->momy               = actor->momy;
   position->momz               = actor->momz;
   position->angle              = actor->angle;
   position->ceilingz           = actor->ceilingz;
   position->floorz             = actor->floorz;
   position->floorclip          = actor->floorclip;
   position->flags              = actor->flags;
   position->flags2             = actor->flags2;
   position->flags3             = actor->flags3;
   position->flags4             = actor->flags4;
   position->intflags           = actor->intflags;
   position->friction           = actor->friction;
   position->movefactor         = actor->movefactor;
   position->alphavelocity      = actor->alphavelocity;
   position->reactiontime       = actor->reactiontime;
   position->floatbob           = actor->floatbob;
   if(actor->player)
   {
      position->bob             = actor->player->bob;
      position->viewz           = actor->player->viewz;
      position->viewheight      = actor->player->viewheight;
      position->deltaviewheight = actor->player->deltaviewheight;
      position->pitch           = actor->player->pitch;
      position->jumptime        = actor->player->jumptime;
      position->playerstate     = actor->player->playerstate;
   }
}

boolean CS_ActorPositionChanged(mobj_t *actor)
{
   if(CS_ActorPositionEquals(actor, &actor->old_position))
      return false;
   return true;
}

boolean CS_PositionsEqual(position_t *position_one, position_t *position_two)
{
   if(position_one->x               == position_two->x               &&
      position_one->y               == position_two->y               &&
      position_one->z               == position_two->z               &&
      position_one->momx            == position_two->momx            &&
      position_one->momy            == position_two->momy            &&
      position_one->momz            == position_two->momz            &&
      position_one->angle           == position_two->angle           &&
      position_one->ceilingz        == position_two->ceilingz        &&
      position_one->floorz          == position_two->floorz          &&
      position_one->floorclip       == position_two->floorclip       &&
      position_one->flags           == position_two->flags           &&
      position_one->flags2          == position_two->flags2          &&
      position_one->flags3          == position_two->flags3          &&
      position_one->flags4          == position_two->flags4          &&
      position_one->intflags        == position_two->intflags        &&
      position_one->friction        == position_two->friction        &&
      position_one->movefactor      == position_two->movefactor      &&
      position_one->alphavelocity   == position_two->alphavelocity   &&
      position_one->reactiontime    == position_two->reactiontime    &&
      position_one->floatbob        == position_two->floatbob        &&
      position_one->bob             == position_two->bob             &&
      position_one->viewz           == position_two->viewz           &&
      position_one->viewheight      == position_two->viewheight      &&
      position_one->deltaviewheight == position_two->deltaviewheight &&
      position_one->pitch           == position_two->pitch           &&
      position_one->jumptime        == position_two->jumptime        &&
      position_one->playerstate     == position_two->playerstate)
   {
      return true;
   }
   return false;
}

void CS_CopyPosition(position_t *dest, position_t *src)
{
   dest->x               = src->x;
   dest->y               = src->y;
   dest->z               = src->z;
   dest->momx            = src->momx;
   dest->momy            = src->momy;
   dest->momz            = src->momz;
   dest->angle           = src->angle;
   dest->ceilingz        = src->ceilingz;
   dest->floorz          = src->floorz;
   dest->floorclip       = src->floorclip;
   dest->flags           = src->flags;
   dest->flags2          = src->flags2;
   dest->flags3          = src->flags3;
   dest->flags4          = src->flags4;
   dest->intflags        = src->intflags;
   dest->friction        = src->friction;
   dest->movefactor      = src->movefactor;
   dest->alphavelocity   = src->alphavelocity;
   dest->reactiontime    = src->reactiontime;
   dest->floatbob        = src->floatbob;
   dest->bob             = src->bob;
   dest->viewz           = src->viewz;
   dest->viewheight      = src->viewheight;
   dest->deltaviewheight = src->deltaviewheight;
   dest->pitch           = src->pitch;
   dest->jumptime        = src->jumptime;
   dest->playerstate     = src->playerstate;
}

