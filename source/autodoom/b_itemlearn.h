// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2014 Ioan Chera
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Learn item effects by trying them.
//
//-----------------------------------------------------------------------------

#ifndef __EternityEngine__b_itemlearn__
#define __EternityEngine__b_itemlearn__

#include "../doomdef.h"
#include "../p_inter.h"

bool B_CheckArmour(const player_t *pl, const itemeffect_t *effect);
bool B_CheckBody(const player_t *pl, const itemeffect_t *effect);
bool B_CheckCard(const player_t *pl, const char *effectname);
bool B_CheckPowerForItem(const player_t *pl, const itemeffect_t *power);
bool B_CheckInventoryItem(const player_t *pl, const itemeffect_t *artifact,
                          int amount = -1);
bool B_CheckAmmoPickup(const player_t *pl, const itemeffect_t *effect,
                       bool dropped, int dropamount);
bool B_CheckBackpack(const player_t *pl);
bool B_CheckWeapon(const player_t *pl, const itemeffect_t *giver, bool dropped,
                   const Mobj *special);

#endif /* defined(__EternityEngine__b_itemlearn__) */
