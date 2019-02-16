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

#ifndef G_COMPAT_H_
#define G_COMPAT_H_

typedef enum
{
   bfg_normal,
   bfg_classic,
   bfg_11k,
   bfg_bouncing, // haleyjd
   bfg_burst,    // haleyjd
} bfg_t;

// -------------------------------------------
// killough 10/98: compatibility vector

// IMPORTANT: when searching for usage in the code, do NOT include the comp_
// prefix. Just search for e.g. "telefrag" or "dropoff".

enum {
   comp_telefrag,
   comp_dropoff,
   comp_vile,
   comp_pain,
   comp_skull,
   comp_blazing,
   comp_doorlight,
   comp_model,
   comp_god,
   comp_falloff,
   comp_floors,
   comp_skymap,
   comp_pursuit,
   comp_doorstuck,
   comp_staylift,
   comp_zombie,
   comp_stairs,
   comp_infcheat,
   comp_zerotags,
   comp_terrain,     // haleyjd 07/04/99: TerrainTypes toggle (#19)
   comp_respawnfix,  // haleyjd 08/09/00: compat. option for nm respawn fix
   comp_fallingdmg,  //         08/09/00: falling damage
   comp_soul,        //         03/23/03: lost soul bounce
   comp_theights,    //         07/06/05: thing heights fix
   comp_overunder,   //         10/19/02: thing z clipping
   comp_planeshoot,  //         09/22/07: ability to shoot floor/ceiling
   comp_special,     //         08/29/09: special failure behavior
   comp_ninja,       //         04/18/10: ninja spawn in G_CheckSpot
   comp_aircontrol,  // Disable air control
   COMP_NUM_USED,    // counts the used comps. MUST BE LAST ONE + 1.
   COMP_TOTAL=32  // Some extra room for additional variables
};

extern int allow_pushers;         // PUSH Things    // phares 3/10/98
extern int default_allow_pushers;

// killough 7/19/98: Classic Pre-Beta BFG
extern bfg_t bfgtype, default_bfgtype;

extern int comp[COMP_TOTAL], default_comp[COMP_TOTAL];

// killough 8/8/98: distance friendly monsters tend to stay from player
extern int distfriend, default_distfriend;

extern int dog_jumping, default_dog_jumping;   // killough 10/98

// killough 9/9/98: whether monsters help friends
extern int help_friends, default_help_friends;

extern int monkeys, default_monkeys;

// killough 9/9/98: whether monsters intelligently avoid hazards
extern int monster_avoid_hazards, default_monster_avoid_hazards;

// killough 9/8/98: whether monsters are allowed to strafe or retreat
extern int monster_backing, default_monster_backing;

// killough 10/98: whether monsters are affected by friction
extern int monster_friction, default_monster_friction;

// killough 7/19/98: whether monsters should fight against each other
extern int monster_infighting, default_monster_infighting;

extern int monsters_remember;                          // killough 3/1/98
extern int default_monsters_remember;

extern int variable_friction;  // ice & mud            // phares 3/10/98
extern int default_variable_friction;

extern int weapon_recoil;          // weapon recoil    // phares
extern int default_weapon_recoil;

#endif 

// EOF
