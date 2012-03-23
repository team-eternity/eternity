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
//   Client/Server configuration loading and parsing.
//
//----------------------------------------------------------------------------

#ifndef CS_CONFIG_H__
#define CS_CONFIG_H__

#include <json/json-forwards.h>

#include "doomtype.h"

#define DEFAULT_CONFIG_FILENAME "server.json"

// [CG] These names aren't as nice as the DMFLAGS' names, but I don't care.
typedef enum
{
   cmf_comp_god        = 2 << 0,
   cmf_comp_infcheat   = 2 << 1,
   cmf_comp_skymap     = 2 << 2,
   cmf_comp_zombie     = 2 << 3,
   cmf_comp_vile       = 2 << 4,
   cmf_comp_pain       = 2 << 5,
   cmf_comp_skull      = 2 << 6,
   cmf_comp_soul       = 2 << 7,
   cmf_comp_staylift   = 2 << 8,
   cmf_comp_doorstuck  = 2 << 9,
   cmf_comp_pursuit    = 2 << 10,
   cmf_comp_dropoff    = 2 << 11,
   cmf_comp_falloff    = 2 << 12,
   cmf_comp_telefrag   = 2 << 13,
   cmf_comp_respawnfix = 2 << 14,
   cmf_comp_terrain    = 2 << 15,
   cmf_comp_theights   = 2 << 16,
   cmf_comp_planeshoot = 2 << 17,
   cmf_comp_blazing    = 2 << 18,
   cmf_comp_doorlight  = 2 << 19,
   cmf_comp_stairs     = 2 << 20,
   cmf_comp_floors     = 2 << 21,
   cmf_comp_model      = 2 << 22,
   cmf_comp_zerotags   = 2 << 23,
   cmf_comp_special    = 2 << 24,
   cmf_comp_fallingdmg = 2 << 25,
   cmf_comp_overunder  = 2 << 26,
   cmf_comp_ninja      = 2 << 27,
   cmf_comp_mouselook  = 2 << 28,
   cmf_comp_2dradatk   = 2 << 29,
} compatflags_t;

typedef enum
{
   cmf_monsters_remember     = 2 << 0,
   cmf_monster_infighting    = 2 << 1,
   cmf_monster_backing       = 2 << 2,
   cmf_monster_avoid_hazards = 2 << 3,
   cmf_monster_friction      = 2 << 4,
   cmf_monkeys               = 2 << 5,
   cmf_help_friends          = 2 << 6,
   cmf_dog_jumping           = 2 << 7,
} compatflags2_t;

enum
{
   map_randomization_none,
   map_randomization_random,
   map_randomization_shuffle
};

// [CG] C/S settings are sent over the wire and stored in demos, and therefore
//      must be packed and use exact-width integer types.

#pragma pack(push, 1)

typedef struct
{
   int32_t  skill;
   int32_t  game_type;
   uint8_t  deathmatch;
   uint8_t  teams;
   uint32_t max_clients;
   uint32_t max_players;
   uint32_t max_players_per_team;
   uint32_t max_lives;
   uint32_t frag_limit;
   uint32_t time_limit;
   uint32_t score_limit;
   uint32_t dogs;
   uint32_t friend_distance;
   int32_t  bfg_type;
   uint32_t friendly_damage_percentage;
   uint32_t spectator_time_limit;
   uint32_t death_time_limit;
   uint32_t death_time_expired_action;
   uint32_t respawn_protection_time;
   double   radial_attack_damage;
   double   radial_attack_self_damage;
   double   radial_attack_lift;
   double   radial_attack_self_lift;
   uint8_t  build_blockmap;
   uint32_t dmflags;
   uint32_t dmflags2;
   uint32_t compatflags;
   uint32_t compatflags2;
   uint8_t  requires_spectator_password;
   uint8_t  requires_player_password;
   uint8_t  use_zdoom_gravity;
   uint8_t  use_zdoom_air_control;
   uint8_t  use_zdoom_player_physics;
   uint8_t  use_zdoom_sound_attenuation;
   int32_t  zdoom_gravity;
   fixed_t  zdoom_air_control;
   fixed_t  zdoom_air_friction;
} clientserver_settings_t;

#pragma pack(pop)

extern Json::Value cs_json;
extern clientserver_settings_t *cs_original_settings;
extern clientserver_settings_t *cs_settings;

void CS_ValidateOptions(Json::Value &options);
void SV_LoadConfig(void);
bool CS_AddIWAD(const char *resource_name);
bool CS_AddWAD(const char *resource_name);
bool CS_AddDeHackEdFile(const char *resource_name);
void CS_HandleMastersSection();
const char* CS_GetIWADResourceBasename();
void CS_FindIWADResource();
void CS_HandleResourcesSection();
void CS_HandleServerSection();
void CS_HandleOptionsSection();
void CS_HandleMapsSection();
void CS_LoadConfig(void);
void CL_LoadConfig(void);
void CS_LoadWADs(void);
void CS_LoadMapOverrides(unsigned int map_index);
void CS_ReloadDefaults(void);
void CS_ApplyConfigSettings(void);

#endif

