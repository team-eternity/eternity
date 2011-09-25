// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2003 James Haley
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
// Deathmatch Flags
//
// By James Haley
//
//---------------------------------------------------------------------------

#ifndef G_DMFLAG_H__
#define G_DMFLAG_H__

// [CG] 09/13/11 I've changed basically everything in here, because there are
//      many more options.  As far as DMFLAGS/DMFLAGS2 goes, I put the more
//      conservative, traditional, Doom-ish options in DMFLAGS, and saved
//      DMFLAGS2 for the new-school stuff like 2-way wallrun, etc.  Finally,
//      the real reason for the split is that an int can only hold so many
//      flags (30), so I tried to impose some meaning on the split as opposed
//      to some arbitrary technical limitation.
//
//      All that said, I had to keep the previous DMFLAGS where they were, so
//      the first 6 flags may not fit this paradigm exactly.

enum {
   dmf_respawn_items            = 2 << 0, // [CG] Do not change.
   dmf_leave_weapons            = 2 << 1, // [CG] Do not change.
   dmf_respawn_barrels          = 2 << 2, // [CG] Do not change.
   dmf_players_drop_everything  = 2 << 3, // [CG] Do not change.
   dmf_respawn_super_items      = 2 << 4, // [CG] Do not change.
   dmf_instagib                 = 2 << 5, // [CG] Do not change.
   dmf_spawn_armor              = 2 << 6,
   dmf_spawn_super_items        = 2 << 7,
   dmf_respawn_health           = 2 << 8,
   dmf_respawn_armor            = 2 << 9,
   dmf_spawn_monsters           = 2 << 10,
   dmf_fast_monsters            = 2 << 11,
   dmf_strong_monsters          = 2 << 12,
   dmf_powerful_monsters        = 2 << 13,
   dmf_respawn_monsters         = 2 << 14,
   dmf_allow_exit               = 2 << 15,
   dmf_kill_on_exit             = 2 << 16,
   dmf_exit_to_same_level       = 2 << 17,
   dmf_spawn_in_same_spot       = 2 << 18,
   dmf_leave_keys               = 2 << 19,
   dmf_players_drop_items       = 2 << 20,
   dmf_players_drop_weapons     = 2 << 21,
   dmf_keep_items_on_exit       = 2 << 22,
   dmf_keep_keys_on_exit        = 2 << 23,
};

enum {
   dmf_allow_jump                       = 2 << 0,
   dmf_allow_freelook                   = 2 << 1,
   dmf_allow_crosshair                  = 2 << 2,
   dmf_allow_target_names               = 2 << 3,
   dmf_allow_movebob_change             = 2 << 4,
   dmf_use_oldschool_sound_cutoff       = 2 << 5,
   dmf_allow_two_way_wallrun            = 2 << 6,
   dmf_allow_no_weapon_switch_on_pickup = 2 << 7,
   dmf_allow_preferred_weapon_order     = 2 << 8,
   dmf_silent_weapon_pickup             = 2 << 9,
   dmf_allow_weapon_speed_change        = 2 << 10,
   dmf_teleport_missiles                = 2 << 11,
   dmf_allow_chasecam                   = 2 << 12,
   dmf_follow_fragger_on_death          = 2 << 13,
   dmf_spawn_farthest                   = 2 << 14,
   dmf_infinite_ammo                    = 2 << 15,
   dmf_enable_variable_friction         = 2 << 16,
   dmf_enable_boom_push_effects         = 2 << 17,
   dmf_enable_nukage                    = 2 << 18,
   dmf_allow_damage_screen_change       = 2 << 19,
};

enum
{
   DM_ITEMRESPAWN   = dmf_respawn_items,
   DM_WEAPONSTAY    = dmf_leave_weapons,
   DM_BARRELRESPAWN = dmf_respawn_barrels,
   DM_PLAYERDROP    = dmf_players_drop_everything,
   DM_RESPAWNSUPER  = dmf_respawn_super_items,
   DM_INSTAGIB      = dmf_instagib
};

// #define DM_PLAYERDROP    (dmf_drop_items | dmf_drop_weapons)

// default dmflags for certain game modes

#define DMD_SINGLE      (0)
#define DMD_COOP        (DM_WEAPONSTAY)
#define DMD_DEATHMATCH  (DM_WEAPONSTAY)
#define DMD_DEATHMATCH2 (DM_ITEMRESPAWN)
#define DMD_DEATHMATCH3 (DM_ITEMRESPAWN|DM_WEAPONSTAY|DM_BARRELRESPAWN|DM_PLAYERDROP)

extern unsigned int dmflags;
extern unsigned int dmflags2;
extern unsigned int default_dmflags;
extern unsigned int default_dmflags2;

void G_SetDefaultDMFlags(int dmtype, bool setdefault);

#endif

// EOF

