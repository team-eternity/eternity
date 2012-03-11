// Emacs style mode select   -*- C++ -*- vi:sw=3 ts=3:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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
// struct sector_t;
class  Mobj;

// haleyjd 10/31/02: moved to header
// Index of the special effects (INVUL inverse) map.

#define INVERSECOLORMAP 32

void P_IncrementPlayerEnvironmentDeaths(int playernum);
void P_IncrementPlayerMonsterDeaths(int playernum);
void P_IncrementPlayerMonsterKills(int playernum);
void P_IncrementPlayerPlayerKills(int sourcenum, int targetnum);
void P_Thrust(player_t *player, angle_t angle, fixed_t move);

// bool P_SectorIsSpecial(sector_t *sector);
void P_HereticCurrent(player_t *player);

void P_RunPlayerCommand(int playernum);
void P_CheckPlayerButtons(int playernum);
void P_PlayerThink(player_t *player);
void P_CalcHeight(player_t *player);
void P_DeathThink(player_t *player);
void P_MovePlayer(player_t *player);
void P_SetPlayerAttacker(player_t *player, Mobj *attacker);
void P_SetDisplayPlayer(int new_displayplayer);

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
