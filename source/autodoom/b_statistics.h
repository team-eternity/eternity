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
//      Various statistics, used for learning
//
//-----------------------------------------------------------------------------

#ifndef __EternityEngine__b_statistics__
#define __EternityEngine__b_statistics__

// As called from P_DamageMobj, register statistics
struct mobjinfo_t;

void B_AddMonsterDeath(const mobjinfo_t* mi);
void B_AddToPlayerDamage(const mobjinfo_t* mi, int amount);

void B_LoadMonsterStats();
void B_StoreMonsterStats();

#endif /* defined(__EternityEngine__b_statistics__) */
