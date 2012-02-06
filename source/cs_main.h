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

#ifndef __CS_MAIN_H__
#define __CS_MAIN_H__

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
#include "cs_ctf.h"
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

#define _DEBUG_SECTOR 350

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
#define MAX_CLIENTS (cs_original_settings->max_clients)

// [CG] Used in p_trace to determine whether or not puffs/blood should be
//      spawned.

#define CL_SHOULD_PREDICT_SHOT(shooter) (clientside && ( \
   cl_predict_shots && \
   ((shooter)->net_id == players[consoleplayer].mo->net_id) \
))

#define CS_SHOULD_SHOW_SHOT(shooter) ( \
   serverside || (!(shooter)->player) || (CL_SHOULD_PREDICT_SHOT((shooter))) \
)

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
   nm_announcerevent,          // (s => c) 35, Announcer event
   nm_ticfinished,             // (s => c) 36, TIC is finished
   // nm_specialspawned,          // (s => c) 32, Map special spawned
   // nm_specialstatus,           // (s => c) 33, Map special's status
   // nm_specialremoved,          // (s => c) 34, Map special removed
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
   ci_kill_count,     // killcount
   ci_item_count,     // itemcount
   ci_secret_count,   // secretcount
   ci_colormap,
   ci_cheats,
   ci_health,
   ci_armor_points,   // armorpoints
   ci_armor_type,     // armortype
   ci_ready_weapon,   // readyweapon
   ci_pending_weapon, // pendingweapon
   ci_frags,          // frags[], MAXPLAYERS
   ci_power_enabled,  // powers[], NUMPOWERS
   ci_owns_card,      // cards[], NUMCARDS
   ci_owns_weapon,    // weaponowned[], NUMWEAPONS
   ci_ammo_amount,    // ammo[], NUMAMMO
   ci_max_ammo,       // maxammo[], NUMAMMO
   ci_owns_backpack,  // backpack
   ci_did_secret,     // didsecret
   ci_name,           // name[20]
   ci_skin,           // skin->skinname
   ci_class,          // pclass->mnemonic[33]
   ci_queue_level,
   ci_queue_position,
   ci_pwo,            // server_client->weapon_preferences[], NUMWEAPONS + 1
   ci_wsop,           // server_client->weapon_switch_on_pickup
   ci_asop,           // server_client->ammo_switch_on_pickup
   ci_bobbing,        // player_bobbing
   ci_weapon_toggle,  // doom_weapon_toggles
   ci_autoaim,        // autoaim
   ci_weapon_speed,   // weapon_speed
   ci_buffering,
   ci_afk,
} client_info_e;

typedef enum
{
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
   uint8_t doom_weapon_toggles;
   uint8_t autoaim;
   uint32_t weapon_speed;
} client_options_t;

typedef struct
{
   // [CG] The gametic at which the client joined the spectators for the first
   //      time.
   int32_t join_tic;
   // [CG] Whether or not the client is spectating.
   uint8_t spectating;
   // [CG] This the index of the client's current command and position, and
   //      will always point to a valid one of both.  There may be commands or
   //      positions past this index that are also "current" (as in, they have
   //      yet to be run), but this index refers to the client's command and
   //      position for this TIC.
   // uint32_t index;
   // [CG] The client's current team, teamcolor_t.
   int32_t team;
   // [CG] This is the number of TICs between when a command was generated and
   //      when it was received again from the server.  This is only used
   //      clientside.
   uint32_t client_lag;
   // [CG] This is the number of commands currently buffered by the server,
   //      again only used clientside.
   uint32_t server_lag;
   // [CG] The amount of TICs it takes for a client's packet to reach the
   //      server (one way, so this could apply vice-versa as well).
   uint32_t transit_lag;
   // [CG] Percentage of packets (as in, 100 is complete and total packet
   //      loss) that have been lost over time.  This is an integer value, 1 is
   //      1%, 100 is 100%.
   uint8_t packet_loss;
   // [CG] The client's queue status: none, waiting or playing,
   //      cs_queue_level_t.
   int32_t queue_level;
   // [CG] The client's current queue position, only relevant if queue_level is
   //      ql_waiting.
   uint32_t queue_position;
   // [CG] The client's current floor status, whether or not they hit the floor
   //      and if they're now standing on a special surface, cs_floor_status_e.
   int32_t floor_status;
   // [CG] The number of TICs the client's been dead; used for death time
   //      limit.
   uint32_t death_time;
   // [CG] The number of times this client has died.
   uint32_t death_count;
   // [CG] The world index of the latest received position.
   uint32_t latest_position_index;
   // [CG] Whether or not a client is AFK
   uint8_t afk;
} client_t;

#pragma pack(pop)

// [CG] Server clients are never sent over the wire, so they can stay as
//      normal.

typedef enum
{
   scr_none,
   scr_initial_state,
   scr_current_state,
   scr_sync,
} cs_client_request_e;

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
   uint8_t buffering;
   // [CG] The TIC at which the client was able to join the game.
   uint32_t finished_waiting_in_queue_tic;
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
   uint32_t map_number;
   uint32_t rngseed;
   uint32_t player_number;
   clientserver_settings_t settings;
} nm_initialstate_t;

// [CG] A save-game-ish blob of data is at the end of this message.
typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint64_t state_size;
   flag_t flags[team_color_max];
   int32_t team_scores[team_color_max];
   uint8_t playeringame[MAXPLAYERS];
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
   uint32_t new_map_number;
   uint8_t enter_intermission;
} nm_mapcompleted_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t client_number;
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
   uint32_t sender_number;
   // [CG] If recipient is a player, this is that player's number.  Otherwise
   //      this is garbage.
   uint32_t recipient_number;
   uint64_t length;
} nm_playermessage_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t net_id;
   uint32_t player_number;
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
   uint32_t player_number;
   int32_t info_type; // [CG] client_info_e.
   int32_t array_index;
   union {
      int32_t int_value;
      uint64_t string_size;
      uint8_t boolean_value;
   };
} nm_playerinfoupdated_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t player_number;
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
   uint32_t client_number;
   uint32_t client_lag;
   uint32_t server_lag;
   uint32_t transit_lag;
   uint8_t packet_loss;
} nm_clientstatus_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t player_number;
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
   uint32_t player_number;
   uint32_t thing_net_id;
} nm_playertouchedspecial_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t player_number;
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
   fixed_t actor_x;
   fixed_t actor_y;
   fixed_t actor_z;
   angle_t actor_angle;
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
   size_t sector_number;
   sector_position_t sector_position;
} nm_sectorposition_t;

#if 0
// [CG] A map special's status is appended to the end of this message, the type
//      of which is indicated by the value of the special_type member.
typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   int32_t special_type; // [CG] map_special_e.
   size_t line_number;
   size_t sector_number;
} nm_specialspawned_t;

// [CG] A map special's status is appended to the end of this message, the type
//      of which is indicated by the value of the special_type member.
typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   int32_t special_type; // [CG] map_special_e.
} nm_specialstatus_t;

typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   int32_t special_type; // [CG] map_special_e.
   uint32_t net_id;
} nm_specialremoved_t;
#endif

// [CG] The name of the event is appended to the end of this message.
typedef struct
{
   int32_t message_type;
   uint32_t world_index;
   uint32_t source_net_id;
   uint32_t event_index;
} nm_announcerevent_t;

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
extern unsigned int world_index;
extern unsigned int cs_shooting_player;
extern char *cs_state_file_path;

void CS_Init(void);
void CS_DoWorldDone(void);
void CS_PrintTime(void);
char* CS_IPToString(uint32_t ip_address);
bool CS_CheckURI(char *uri);
float CS_VersionFloat(void);
char* CS_VersionString(void);
char* CS_GetSHA1Hash(const char *input, size_t input_size);
char* CS_GetSHA1HashFile(char *path);
char* CS_GetPlayerName(player_t *player);
void CS_SetPlayerName(player_t *player, char *name);
void CS_SetSkin(const char *skin_name, int playernum);
void CS_InitPlayers(void);
void CS_DisconnectPeer(ENetPeer *peer, enet_uint32 reason);
void CS_ZeroClient(int clientnum);
void CS_ZeroClients(void);
void CS_InitPlayer(int playernum);
void CS_FormatTime(char *formatted_time, unsigned int seconds);
void CS_FormatTicsAsTime(char *formatted_time, unsigned int tics);
void CS_SetDisplayPlayer(int playernum);
size_t CS_BuildGameState(int playernum, byte **buffer);
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

void CS_SetSpectator(int playernum, bool spectating);
void CS_SpawnPlayer(int playernum, fixed_t x, fixed_t y, fixed_t z,
                    angle_t angle, bool as_spectator);
mapthing_t* CS_SpawnPlayerCorrectly(int playernum, bool as_spectator);

void CS_SpawnPuff(Mobj *shooter, fixed_t x, fixed_t y, fixed_t z,
                  angle_t angle, int updown, bool ptcl);
void CS_SpawnBlood(Mobj *shooter, fixed_t x, fixed_t y, fixed_t z,
                   angle_t angle, int damage, Mobj *target);
char* CS_ExtractMessage(char *data, size_t data_length);
void CS_FlushConnection(void);
void CS_ServiceNetwork(void);
void CS_ReadFromNetwork(unsigned int timeout);
void CS_TryRunTics(void);
void CS_AddCommands(void);

#endif

