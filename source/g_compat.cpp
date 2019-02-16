//
// The Eternity Engine
// Copyright (C) 2019 James Haley, Ioan Chera, et al.
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
// Purpose: Compatibility flags from Boom, MBF and Eternity
//

#include "z_zone.h"
#include "g_compat.h"

int  allow_pushers = 1;      // PUSH Things              // phares 3/10/98
int  default_allow_pushers;  // killough 3/1/98: make local to each game

// killough 7/19/98: classic Doom BFG
bfg_t bfgtype, default_bfgtype;

int comp[COMP_TOTAL], default_comp[COMP_TOTAL];    // killough 10/98

// killough 8/8/98: distance friends tend to move towards players
int distfriend = 128, default_distfriend = 128;

int dog_jumping, default_dog_jumping;   // killough 10/98

// killough 9/9/98: whether monsters help friends
int help_friends, default_help_friends;

int monkeys, default_monkeys;

// killough 9/9/98: whether monsters are able to avoid hazards (e.g. crushers)
int monster_avoid_hazards, default_monster_avoid_hazards;

// killough 9/8/98: whether monsters are allowed to strafe or retreat
int monster_backing, default_monster_backing;

int monster_friction=1;       // killough 10/98: monsters affected by friction
int default_monster_friction=1;

int monster_infighting=1;       // killough 7/19/98: monster<=>monster attacks
int default_monster_infighting=1;

int monsters_remember=1;        // killough 3/1/98
int default_monsters_remember=1;

int  variable_friction = 1;      // ice & mud               // phares 3/10/98
int  default_variable_friction;  // killough 3/1/98: make local to each game

int  weapon_recoil;              // weapon recoil                   // phares
int  default_weapon_recoil;      // killough 3/1/98: make local to each game

// EOF
