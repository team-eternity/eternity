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
//   Client/Server routines for both clients and servers.
//
//----------------------------------------------------------------------------

#ifndef CS_MAIN_H__
#define CS_MAIN_H__

#include "d_player.h"
#include "doomdata.h"
#include "m_queue.h"
#include "p_skin.h"
#include "r_defs.h"

#include <enet/enet.h>

#include "cs_cmd.h"
#include "cs_spec.h"
#include "cs_team.h"
#include "cs_position.h"
#include "cs_config.h"

struct event_t;

// [CG] Some debugging defines, setting them to 1 will dump debugging output
//      to STDOUT.
#define _CS_DEBUG 0
#define _CMD_DEBUG  0
#define _PRED_DEBUG 0
#define _PLAYER_DEBUG 0
#define _AUTH_DEBUG 0
#define _UNLAG_DEBUG 1
#define _SPECIAL_DEBUG 0
#define _SECTOR_PRED_DEBUG 0

#define _DEBUG_SECTOR 2

#define LOG_ALL_NETWORK_MESSAGES false

// [CG] The maximum amount of latency we can tolerate in any situation.  For
//      the sake of unlagged, this is the maximum amount of latency between any
//      given pair of peers.  Note that this isn't fluid; if a peer's latency
//      exceeds half of this it will be disconnected, even if none of the other
//      connected peers have latencies <= (MAX_LATENCY / 2).  This is in
//      seconds.
#define MAX_LATENCY 3

// [CG] The number of commands bundled together by the client to mitigate the
//      effects of packet loss.
// [CG] TODO: 56k can really only handle 14 command bundles TOTAL, so this
//            should maybe be configurable... except that lowering it means you
//            will skip more and be more difficult to hit... so maybe not?
//            Requires testing.
// [CG] Try 10 instead of 35 (TICRATE) here.
#define COMMAND_BUNDLE_SIZE 10

// [CG] The default port the server listens on if none is given.
#define DEFAULT_PORT 10666

// [CG] ENet's timeout (milliseconds)
#define CONNECTION_TIMEOUT (MAX_LATENCY * 1000)

// [CG] Default server name.
#define SERVER_NAME "[SERVER]"

// [CG] Convenience macro to get a player's weapon-switch-on-pickup
//      preference.
#define GET_WSOP(c) (CS_SERVER ? server_clients[(c)].weapon_switch_on_pickup :\
                     weapon_switch_on_pickup)

// [CG] Convenience macro to get a player's ammo-switch-on-pickup preference.
#define GET_ASOP(c) (CS_SERVER ? server_clients[(c)].ammo_switch_on_pickup :\
                     ammo_switch_on_pickup)

// [CG] Number of positions we save for each player.
#define MAX_POSITIONS (TICRATE * MAX_LATENCY)

// [CG] Maximum size of a transmitted string.  Does not include trailing \0.
#define MAX_STRING_SIZE 255

// [CG] Maximum size of a player's name.  Includes the trailing \0.
#define MAX_NAME_SIZE 20

// [CG] Convenience macro to return the functional maximum number of clients.
#define MAX_CLIENTS ((signed int)(cs_original_settings->max_clients))

// [CG] Used in p_trace to determine whether or not puffs/blood should be
//      spawned.

#define CL_SHOULD_PREDICT_SHOT(shooter) (clientside && ( \
   cl_predict_shots && \
   ((shooter)->net_id == players[consoleplayer].mo->net_id) \
))

#define CS_SPAWN_ACTOR_OK (\
   serverside || \
   cl_spawning_actor_from_message || \
   gamestate != GS_LEVEL || \
   !cl_packet_buffer.synchronized() \
)

#define CS_REMOVE_ACTOR_OK (\
   serverside || \
   cl_removing_actor_from_message || \
   gamestate != GS_LEVEL || \
   !cl_packet_buffer.synchronized() \
)

typedef enum
{
   WEAPON_SWITCH_ALWAYS,
   WEAPON_SWITCH_USE_PWO,
   WEAPON_SWITCH_NEVER
} weapon_switch_value_e;

typedef enum
{
   AMMO_SWITCH_VANILLA,
   AMMO_SWITCH_USE_PWO,
   AMMO_SWITCH_DISABLED
} ammo_switch_value_e;

enum
{
    RELIABLE_CHANNEL,
    UNRELIABLE_CHANNEL,
    MAX_CHANNELS
};

typedef enum
{
   CS_AT_TARGET,
   CS_AT_TRACER,
   CS_AT_LASTENEMY
} actor_target_e;

typedef enum
{
   nm_initialstate,            // (s => c) 0,  Initial game state
   nm_currentstate,            // (s => c) 1,  Current game state
   nm_sync,                    // (s => c) 2,  Game sync
   nm_clientrequest,           // (c => s) 3,  Generic client update request
   nm_mapstarted,              // (s => c) 4,  Map started
   nm_mapcompleted,            // (s => c) 5,  Map completed
   nm_authresult,              // (s => c) 6,  Authorization result
   nm_clientinit,              // (s => c) 7,  New client initialization info
   nm_playercommand,           // (c => s) 8,  Player command
   nm_clientstatus,            // (s => c) 9,  Client status
   nm_playerposition,          // (s => c) 10, Player position
   nm_playerspawned,           // (s => c) 11, Player's actor spawned
   nm_playerinfoupdated,       // ( both ) 12, Player info
   nm_playerweaponstate,       // (s => c) 13, Player weapon state
   nm_playerremoved,           // (s => c) 14, Player removed
   nm_playertouchedspecial,    // (s => c) 15, Player touched special
   nm_servermessage,           // (s => c) 16, Server message
   nm_playermessage,           // ( both ) 17, Player message
   nm_puffspawned,             // (s => c) 18, Puff spawned
   nm_bloodspawned,            // (s => c) 19, Blood spawned
   nm_actorspawned,            // (s => c) 20, Actor spawned
   nm_actorposition,           // (s => c) 21, Actor position
   nm_actormiscstate,          // (s => c) 22, Actor miscellaneous state
   nm_actortarget,             // (s => c) 23, Actor target
   nm_actorstate,              // (s => c) 24, Actor state
   nm_actordamaged,            // (s => c) 25, Actor damaged
   nm_actorkilled,             // (s => c) 26, Actor killed
   nm_actorremoved,            // (s => c) 27, Actor removed
   nm_lineactivated,           // (s => c) 28, Line activated
   nm_monsteractive,           // (s => c) 29, Monster active
   nm_monsterawakened,         // (s => c) 30, Monster awakened
   nm_missilespawned,          // (s => c) 31, Missile spawned
   nm_missileexploded,         // (s => c) 32, Missile exploded
   nm_cubespawned,             // (s => c) 33, Boss brain cube spawned
   nm_sectorposition,          // (s => c) 34, Sector position
   nm_sectorthinkerspawned,    // (s => c) 35, Sector thinker spawned
   nm_sectorthinkerstatus,     // (s => c) 36, Sector thinker status
   nm_sectorthinkerremoved,    // (s => c) 37, Sectorthinkerremoved
   nm_announcerevent,          // (s => c) 38, Announcer event
   nm_voterequest,             // (c => s) 39, Vote request
   nm_vote,                    // ( both ) 40, Client vote
   nm_voteresult,              // (s => c) 41, Vote result
   nm_ticfinished,             // (s => c) 42, TIC is finished
   nm_max_messages
} network_message_e;

typedef enum
{
   dr_no_reason = 0,
   dr_server_is_full,
   dr_invalid_message,
   dr_excessive_latency,
   dr_command_flood,
   dr_kicked,
   dr_banned,
   dr_max_reasons
} disconnection_reason_e;

typedef enum
{
   cs_fs_none,
   cs_fs_hit,
   cs_fs_hit_on_thing
} cs_floor_status_e;

// [CG] TODO: Some of this stuff should only be sent to spectators, like
//            health, armor_points, etc., because it can be used strategically.

typedef enum
{
   ci_team,
   ci_spectating,
   ci_kill_count,        // killcount
   ci_item_count,        // itemcount
   ci_secret_count,      // secretcount
   ci_colormap,
   ci_cheats,
   ci_health,
   ci_armor_points,      // armorpoints
   ci_armor_type,        // armortype
   ci_ready_weapon,      // readyweapon
   ci_pending_weapon,    // pendingweapon
   ci_frags,             // frags[], MAXPLAYERS
   ci_power_enabled,     // powers[], NUMPOWERS
   ci_owns_card,         // cards[], NUMCARDS
   ci_owns_weapon,       // weaponowned[], NUMWEAPONS
   ci_ammo_amount,       // ammo[], NUMAMMO
   ci_max_ammo,          // maxammo[], NUMAMMO
   ci_owns_backpack,     // backpack
   ci_did_secret,        // didsecret
   ci_name,              // name[20]
   ci_skin,              // skin->skinname
   ci_class,             // pclass->mnemonic[33]
   ci_queue_level,
   ci_queue_position,
   ci_pwo,               // server_client->weapon_preferences[], NUMWEAPONS + 1
   ci_wsop,              // server_client->weapon_switch_on_pickup
   ci_asop,              // server_client->ammo_switch_on_pickup
   ci_bobbing,           // player_bobbing
   ci_bobbing_intensity, // player_bobbing
   ci_weapon_toggle,     // doom_weapon_toggles
   ci_autoaim,           // autoaim
   ci_weapon_speed,      // weapon_speed
   ci_buffering,
   ci_afk,
} client_info_e;

typedef enum
{
   mr_vote,
   mr_auth,
   mr_rcon,
   mr_server,
   mr_player,
   mr_team,
   mr_all,
} message_recipient_e;

typedef enum
{
   at_used,
   at_crossed,
   at_shot,
   at_max,
} activation_type_e;

typedef enum
{
   cs_auth_none,
   cs_auth_spectator,
   cs_auth_player,
   cs_auth_moderator,
   cs_auth_administrator,
} cs_auth_level_e;

typedef enum
{
   ql_none,
   ql_waiting,
   ql_can_join,
   ql_playing
} cs_queue_level_e;

// [CG] These structs are sent over the wire, so they must be packed & use
//      exact-width integer types.

#pragma pack(push, 1)

typedef struct
{
   uint8_t player_bobbing;
   double  bobbing_intensity;
   uint8_t doom_weapon_toggles;
   uint8_t autoaim;
   uint32_t weapon_speed;
} client_options_t;

typedef struct
{
   int32_t  join_tic;
   uint32_t client_lag;
   uint32_t server_lag;
   uint32_t transit_lag;
   uint8_t  packet_loss;
   int32_t  score; // [CG] So we don't have to do funny things with frags.
   uint32_t environment_deaths;
   uint32_t monster_deaths;
   uint32_t player_deaths;
   uint32_t suicides;
   uint32_t total_deaths;
   uint32_t monster_kills;
   uint32_t player_kills;
   uint32_t team_kills;
   uint32_t total_kills;
   uint32_t flag_touches;
   uint32_t flag_captures;
   uint32_t flag_picks;
   uint32_t flag_carriers_fragged;
   uint16_t average_damage;
} client_stats_t;

typedef struct
{
   // [CG] Whether or not the client is spectating.
   uint8_t spectating;
   // [CG] The client's current team.
   int32_t team;
   // [CG] The client's queue status: none, waiting or playing,
   //      cs_queue_level_t.
   int32_t queue_level;
   // [CG] The client's current queue position, only relevant if queue_level is
   //      ql_waiting.
   int32_t queue_position;
   // [CG] The client's current floor status, whether or not they hit the floor
   //      and if they're now standing on a special surface, cs_floor_status_e.
   int32_t floor_status;
   // [CG] The number of TICs the client's been dead; used for death time
   //      limit.
   uint32_t death_time;
   // [CG] The world index of the latest received position.
   uint32_t latest_position_index;
   // [CG] Whether or not a client is AFK
   uint8_t afk;
   // [CG] Frags this life.
   uint16_t frags_this_life;
   // [CG] The index at which the client last fragged.
   int32_t last_frag_index;
   // [CG] The client's current frag level.
   uint8_t frag_level;
   // [CG] The client's current consecutive frag level.
   uint8_t consecutive_frag_level;
   // [CG] Various client statistics.
   client_stats_t stats;
} client_t;

#pragma pack(pop)

typedef enum
{
   scr_none,
   scr_initial_state,
   scr_current_state,
   scr_sync,
} cs_client_request_e;

enum
{
   fl_none,
   fl_killing_spree,
   fl_rampage,
   fl_dominating,
   fl_unstoppable,
   fl_god_like,
   fl_wicked_sick,
   fl_max
};

enum
{
   cfl_none,
   cfl_single_kill,
   cfl_double_kill,
   cfl_multi_kill,
   cfl_ultra_kill,
   cfl_monster_kill,
   cfl_max
};

// [CG] Server clients are never sent over the wire, so they can stay normal.
typedef struct
{
   enet_uint32 connect_id;
   ENetAddress address;
   cs_auth_level_e auth_level;
   cs_client_request_e current_request;
   bool received_game_state;
   bool command_buffer_filled;
   int last_auth_attempt;
   unsigned int commands_dropped;
   // [CG] These are used to keep track of the player's most recently run
   //      command.
   unsigned int last_command_run_index;
   unsigned int last_command_run_world_index;
   // [CG] This is used to check validity of the nm_playercommand message.
   unsigned int last_command_received_index;
   // [CG] This is so that unlagged knows which positions to load.
   unsigned int command_world_index;
   // [CG] So the server knows that a client's loaded the map.
   bool received_command_for_current_map;
   // [CG] The serverside command buffer.
   mqueue_t commands;
   // [CG] The server stores player positions here for unlagged.
   cs_player_position_t positions[MAX_POSITIONS];
   // [CG] The server stores miscellaneous player states here for unlagged.
   cs_misc_state_t misc_states[MAX_POSITIONS];
   // [CG] The server stores player states here for unlagged.
   playerstate_t player_states[MAX_POSITIONS];
   // [CG] Per-client preferred weapon order.
   weapontype_t weapon_preferences[NUMWEAPONS + 1];
   // [CG] Per-client weapon switch on weapon pickup preference.
   unsigned int weapon_switch_on_pickup;
   // [CG] Per-client weapon switch on ammo pickup preference.
   unsigned int ammo_switch_on_pickup;
   // [CG] Sync-critical configuration options.
   client_options_t options;
   // [CG] Last position, used for unlagged.
   cs_player_position_t saved_position;
   // [CG] Last misc state, used for unlagged.
   cs_misc_state_t saved_misc_state;
   // [CG] Whether or not a client is buffering incoming server messages.
   bool buffering;
   // [CG] The TIC at which the client was able to join the game.
   int finished_waiting_in_queue_tic;
   // [CG] True if the client is currently connecting.
   bool connecting;
   // [CG] > 0 if the client is currently firing.
   int firing;
} server_client_t;

// [CG] Below are all the network message structure definitions.  Each struct
//      must be packed (as in, it must be defined within the #pragma
//      directives) and must use exact-width integer types (such as uint32_t)
//      in order to ensure that machines of different architectures can still
//      communicate properly.

#pragma pack(push, 1)

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t map_index;
   uint32_t rngseed;
   int32_t player_number;
   clientserver_settings_t settings;
} nm_initialstate_t;

// [CG] A save-game-ish blob of data is at the end of this message.
typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint64_t state_size;
} nm_currentstate_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   int32_t gametic;
   int32_t levelstarttic;
   int32_t basetic;
   int32_t leveltime;
} nm_sync_t;

typedef struct
{
   int32_t message_type;
   int32_t request_type; // [CG] cs_client_request_e
} nm_clientrequest_t;

typedef struct
{
   int32_t message_type;
} nm_clientready_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   clientserver_settings_t settings;
} nm_mapstarted_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t new_map_index;
   uint8_t enter_intermission;
} nm_mapcompleted_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   int32_t client_number;
   client_t client;
} nm_clientinit_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint8_t authorization_successful;
   int32_t authorization_level; // [CG] cs_auth_level_e.
} nm_authresult_t;

// [CG] The contents of the message are at the end of this message.
typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint8_t is_hud_message;
   uint8_t prepend_name;
   uint64_t length;
} nm_servermessage_t;

// [CG] The contents of the message are at the end of this message.
typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   int32_t recipient_type; // [CG] message_recipient_e.
   int32_t sender_number;
   // [CG] If recipient is a player, this is that player's number.  Otherwise
   //      this is garbage.
   int32_t recipient_number;
   uint64_t length;
} nm_playermessage_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t net_id;
   int32_t player_number;
   uint8_t as_spectator;
   fixed_t x;
   fixed_t y;
   fixed_t z;
   angle_t angle;
} nm_playerspawned_t;

// [CG] If the info_type is a string, then there is a string buffer at the end
//      of this message (which is of variable length).
typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   int32_t player_number;
   int32_t info_type; // [CG] client_info_e.
   int32_t array_index;
   union {
      int32_t  int_value;
      uint64_t string_size;
      uint8_t  boolean_value;
      double   float_value;
   };
} nm_playerinfoupdated_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   int32_t player_number;
   int32_t psprite_position;
   int32_t weapon_state; // [CG] statenum_t.
} nm_playerweaponstate_t;

// [CG] An array of cs_cmd_t structures is appended to this message.
typedef struct
{
   int32_t message_type;
   uint8_t command_count;
} nm_playercommand_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   int32_t client_number;
   uint32_t client_lag;
   uint32_t server_lag;
   uint32_t transit_lag;
   uint8_t packet_loss;
} nm_clientstatus_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   int32_t player_number;
   cs_player_position_t position;
   int32_t floor_status; // [CG] cs_floor_status_e.
   // [CG] Last client command index run.
   uint32_t last_index_run;
   // [CG] World index at which the last command was run.
   uint32_t last_world_index_run;
} nm_playerposition_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   int32_t player_number;
   uint32_t thing_net_id;
} nm_playertouchedspecial_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   int32_t player_number;
   int32_t reason; // [CG] disconnection_reason_t.
} nm_playerremoved_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t puff_net_id;
   uint32_t shooter_net_id;
   fixed_t x;
   fixed_t y;
   fixed_t z;
   angle_t angle;
   int32_t updown;
   uint8_t ptcl;
} nm_puffspawned_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t blood_net_id;
   uint32_t shooter_net_id;
   uint32_t target_net_id;
   fixed_t x;
   fixed_t y;
   fixed_t z;
   angle_t angle;
   int32_t damage;
} nm_bloodspawned_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t net_id;
   fixed_t x;
   fixed_t y;
   fixed_t z;
   fixed_t momx;
   fixed_t momy;
   fixed_t momz;
   angle_t angle;
   int32_t flags;
   int32_t type; // [CG] mobjtype_t.
} nm_actorspawned_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t actor_net_id;
   cs_actor_position_t position;
} nm_actorposition_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t actor_net_id;
   cs_misc_state_t misc_state;
} nm_actormiscstate_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   int32_t target_type;
   uint32_t actor_net_id;
   uint32_t target_net_id;
} nm_actortarget_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t actor_net_id;
   int32_t state_number; // [CG] statenum_t.
   int32_t actor_type;   // [CG] mobjtype_t.
} nm_actorstate_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t target_net_id;
   uint32_t inflictor_net_id;
   uint32_t source_net_id;
   int32_t health_damage;
   int32_t armor_damage;
   int32_t mod;
   uint8_t damage_was_fatal;
   uint8_t just_hit;
#if _UNLAG_DEBUG
   fixed_t x;
   fixed_t y;
   fixed_t z;
   angle_t angle;
#endif
} nm_actordamaged_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t target_net_id;
   uint32_t inflictor_net_id;
   uint32_t source_net_id;
   int32_t damage;
   int32_t mod;
} nm_actorkilled_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t actor_net_id;
} nm_actorremoved_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   int32_t activation_type; // [CG] activation_type_t.
   uint32_t actor_net_id;
   int32_t line_number;
   int32_t side;
} nm_lineactivated_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t monster_net_id;
} nm_monsteractive_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t monster_net_id;
} nm_monsterawakened_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t net_id;
   uint32_t source_net_id;
   int32_t type; // [CG] mobjtype_t.
   fixed_t x;
   fixed_t y;
   fixed_t z;
   fixed_t momx;
   fixed_t momy;
   fixed_t momz;
   angle_t angle;
} nm_missilespawned_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t missile_net_id;
   uint32_t tics;
} nm_missileexploded_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t net_id;
   uint32_t target_net_id;
   int32_t flags;
} nm_cubespawned_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t sector_number;
   sector_position_t sector_position;
} nm_sectorposition_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint8_t type;
   uint32_t sector_number;
   cs_sector_thinker_data_t data;
   cs_sector_thinker_spawn_data_t spawn_data;
} nm_sectorthinkerspawned_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint8_t type;
   cs_sector_thinker_data_t data;
} nm_sectorthinkerstatus_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint8_t type;
   uint32_t net_id;
} nm_sectorthinkerremoved_t;

// [CG] The name of the event is appended to the end of this message.
typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t source_net_id;
   uint32_t event_index;
} nm_announcerevent_t;

// [CG] The command is appended to the end of this message.
typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint64_t command_size;
} nm_voterequest_t;

// [CG] The command is appended to the end of this message.
typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint64_t command_size;
   uint32_t duration;
   double threshold;
   uint32_t max_votes;
} nm_vote_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint8_t passed;
} nm_voteresult_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
} nm_ticfinished_t;

#pragma pack(pop)

extern bool display_target_names;
extern bool default_display_target_names;

extern unsigned int weapon_switch_on_pickup;
extern unsigned int default_weapon_switch_on_pickup;
extern unsigned int ammo_switch_on_pickup;
extern unsigned int default_ammo_switch_on_pickup;

extern ENetHost *net_host;
extern ENetPeer *net_peer;
extern ENetAddress *server_address;
extern client_t *clients;
extern enet_uint32 start_time;
extern const char *disconnection_strings[dr_max_reasons];
extern const char *network_message_names[nm_max_messages];
extern const char *frag_level_names[fl_max];
extern const char *consecutive_frag_level_names[cfl_max];
extern unsigned int world_index;
extern unsigned int cs_shooting_player;
extern char *cs_state_file_path;

void CS_Init(void);
void CS_DoWorldDone(void);
void CS_PrintTime(void);
char* CS_IPToString(uint32_t ip_address);
bool CS_CheckURI(const char *uri);
float CS_VersionFloat(void);
char* CS_VersionString(void);
char* CS_GetSHA1Hash(const char *input, size_t input_size);
char* CS_GetSHA1HashFile(const char *path);
char* CS_GetPlayerName(player_t *player);
void CS_SetPlayerName(player_t *player, const char *name);
void CS_SetSkin(const char *skin_name, int playernum);
void CS_InitPlayers(void);
void CS_DisconnectPeer(ENetPeer *peer, enet_uint32 reason);
void CS_ClearClientStats(int clientnum);
void CS_ZeroClient(int clientnum);
void CS_ZeroClients(void);
void CS_SetClientTeam(int clientnum, int new_team_color);
void CS_InitPlayer(int playernum);
void CS_FormatTime(char *formatted_time, unsigned int seconds);
void CS_FormatTicsAsTime(char *formatted_time, unsigned int tics);
void CS_SetDisplayPlayer(int playernum);
void CS_IncrementClientScore(int clientnum, bool increment_team_score);
void CS_DecrementClientScore(int clientnum, bool decrement_team_score);
void CS_SetClientScore(int clientnum, int new_score, bool set_team_score);
void CS_CheckClientSprees(int clientnum);
fixed_t CS_GetExtraZDoomGravity();
fixed_t CS_GetZDoomGravity();
fixed_t CS_ZDoomAirControlToAirFriction(fixed_t air_control);
size_t CS_BuildGameState(int playernum, byte **buffer);
void CS_ResetClientSprees(int clientnum);
void CS_PlayerThink(int playernum);
void CS_ApplyCommandButtons(ticcmd_t *cmd);
void CS_PlayerTicker(int playernum);
bool CS_WeaponPreferred(int playernum, weapontype_t weapon_one,
                                       weapontype_t weapon_two);
void CS_ReadJSON(Json::Value &json, const char *filename);
void CS_WriteJSON(const char *filename, Json::Value &value, bool styled);
void CS_HandleSpectateKey(event_t *ev);
void CS_HandleSpectatePrevKey(event_t *ev);
void CS_HandleSpectateNextKey(event_t *ev);
void CS_HandleFlushPacketBufferKey(event_t *ev);
void CS_HandleUpdatePlayerInfoMessage(nm_playerinfoupdated_t *message);
void CS_HandlePlayerCommand(nm_playercommand_t *command_message);
size_t CS_BuildPlayerStringInfoPacket(nm_playerinfoupdated_t **update_message,
                                      int playernum, client_info_e info_type);
void CS_BuildPlayerArrayInfoPacket(nm_playerinfoupdated_t *update_message,
                                   int playernum, client_info_e info_type,
                                   int array_index);
void CS_BuildPlayerScalarInfoPacket(nm_playerinfoupdated_t *update_message,
                                    int playernum, client_info_e info_type);
void CS_CheckSprees(int sourcenum, int targetnum, bool suicide, bool team_kill);
void CS_SetSpectator(int playernum, bool spectating);
void CS_SpawnPlayer(int playernum, fixed_t x, fixed_t y, fixed_t z,
                    angle_t angle, bool as_spectator);
mapthing_t* CS_SpawnPlayerCorrectly(int playernum, bool as_spectator);

void CS_SpawnPuff(Mobj *shooter, fixed_t x, fixed_t y, fixed_t z,
                  angle_t angle, int updown, bool ptcl);
void CS_SpawnBlood(Mobj *shooter, fixed_t x, fixed_t y, fixed_t z,
                   angle_t angle, int damage, Mobj *target);
void CS_ArchiveSettings(SaveArchive& arc);
void CS_ArchiveTeams(SaveArchive& arc);
void CS_ArchiveClients(SaveArchive& arc);
char* CS_ExtractMessage(char *data, size_t data_length);
void CS_FlushConnection(void);
void CS_ServiceNetwork(void);
void CS_ReadFromNetwork(unsigned int timeout);
void CS_TryRunTics(void);
void CS_MessageAll(event_t *ev);

#endif

