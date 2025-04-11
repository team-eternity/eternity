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
//      Player related stuff.
//      Bobbing POV/weapon, movement.
//      Pending weapon.
//
//-----------------------------------------------------------------------------

#ifndef P_USER_H__
#define P_USER_H__

struct player_t;
class  Mobj;

// haleyjd 10/31/02: moved to header
// Index of the special effects (INVUL inverse) map.

#define INVERSECOLORMAP 32

void P_PlayerThink(player_t &player);
void P_CalcHeight(player_t &player);
void P_DeathThink(player_t &player);
void P_MovePlayer(player_t &player);
void P_Thrust(player_t &player, angle_t angle, angle_t pitch, fixed_t move);
void P_SetPlayerAttacker(player_t &player, Mobj *attacker);
void P_SetDisplayPlayer(int new_displayplayer);
void P_PlayerStartFlight(player_t &player, bool thrustup);
void P_PlayerStopFlight(player_t &player);
void P_GiveRebornInventory(player_t& player);
bool P_UnmorphPlayer(player_t& player, bool onexit);

extern bool pitchedflight;
extern bool default_pitchedflight;

#endif // P_USER_H__

//----------------------------------------------------------------------------
//
// $Log: p_user.h,v $
// Revision 1.2  1998/05/10  23:38:38  killough
// Add more prototypes
//
// Revision 1.1  1998/05/03  23:19:24  killough
// Move from obsolete p_local.h
//
//----------------------------------------------------------------------------
