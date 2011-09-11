// Emacs style mode select -*- C++ -*- vim:sw=3 ts=3:
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

#ifndef __CS_POSITION_H__
#define __CS_POSITION_H__

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

typedef struct position_s
{
   uint32_t world_index;
   fixed_t x;
   fixed_t y;
   fixed_t z;
   fixed_t momx;
   fixed_t momy;
   fixed_t momz;
   angle_t angle;
   fixed_t ceilingz;
   fixed_t floorz;
   fixed_t floorclip;
   int32_t flags;
   int32_t flags2;
   int32_t flags3;
   int32_t flags4;
   int32_t intflags;
   int32_t friction;
   int32_t movefactor;
   int32_t alphavelocity;
   int16_t reactiontime;
   int32_t floatbob;
   int32_t bob;             // [CG] Players only, 0 otherwise.
   fixed_t viewz;           // [CG] Players only, 0 otherwise.
   int32_t viewheight;      // [CG] Players only, 0 otherwise.
   int32_t deltaviewheight; // [CG] Players only, 0 otherwise.
   fixed_t pitch;           // [CG] Players only, 0 otherwise.
   int32_t jumptime;        // [CG] Players only, 0 otherwise.
   int32_t playerstate;     // [CG] Players only, 0 otherwise.
} position_t;

#pragma pack(pop)

void    CS_PrintPosition(position_t *position);
void    CS_PrintActorPosition(mobj_t *actor, unsigned int index);
void    CS_PrintPlayerPosition(int playernum, unsigned int index);
void    CS_SetActorPosition(mobj_t *actor, position_t *position);
void    CS_SetPlayerPosition(int playernum, position_t *position);
boolean CS_ActorPositionEquals(mobj_t *actor, position_t *position);
void    CS_SaveActorPosition(position_t *position, mobj_t *actor, int index);
boolean CS_ActorPositionChanged(mobj_t *actor);
boolean CS_PositionsEqual(position_t *position_one, position_t *position_two);
void    CS_CopyPosition(position_t *dest, position_t *src);

#endif

