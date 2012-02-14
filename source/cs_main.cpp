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

// [CG] Needed for JSON serialization routines.
#include <string>
#include <fstream>

#include "z_zone.h"

#include <json/json.h>

#include "acs_intr.h"
#include "am_map.h"
#include "c_io.h"
#include "c_net.h"
#include "c_runcmd.h"
#include "d_gi.h"
#include "d_main.h"
#include "d_player.h"
#include "d_ticcmd.h"
#include "doomdef.h"
#include "doomdata.h"
#include "doomstat.h"
#include "e_player.h"
#include "e_ttypes.h"
#include "g_bind.h"
#include "g_dmflag.h"
#include "g_game.h"
#include "hu_frags.h"
#include "hu_stuff.h"
#include "i_system.h"
#include "i_thread.h"
#include "info.h"
#include "m_argv.h"
#include "m_fixed.h"
#include "m_hash.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "p_chase.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_mobj.h"
#include "p_portal.h"
#include "p_pspr.h"
#include "p_saveg.h"
#include "p_skin.h"
#include "p_user.h"
#include "r_main.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "v_block.h"
#include "v_misc.h"
#include "v_video.h"
#include "version.h"
#include "w_wad.h"

#include "cs_team.h"
#include "cs_config.h"
#include "cs_hud.h"
#include "cs_main.h"
#include "cs_master.h"
#include "cs_position.h"
#include "cs_ctf.h"
#include "cs_demo.h"
#include "cs_wad.h"
#include "cs_netid.h"
#include "cl_buf.h"
#include "cl_cmd.h"
#include "cl_main.h"
#include "cl_pred.h"
#include "cl_spec.h"
#include "sv_main.h"
#include "sv_spec.h"
#include "sv_queue.h"

ENetHost *net_host;
ENetPeer *net_peer;
ENetAddress *server_address;
client_t *clients;
enet_uint32 start_time;
unsigned int cs_shooting_player = 0;
char *cs_state_file_path = NULL;

extern int rngseed;
extern int action_scoreboard;
extern char *default_skin;

const char *disconnection_strings[dr_max_reasons] = {
   "",
   "Server is full",
   "Invalid message received",
   "Latency limit exceeded",
   "Command flood",
   "Kicked",
   "Banned"
};

const char *network_message_names[nm_max_messages] = {
   "initial state",
   "current state",
   "sync",
   "client request",
   "map started",
   "map completed",
   "authorization result",
   "client initialization",
   "player command",
   "client status",
   "player position",
   "player spawned",
   "player info updated",
   "player weapon state",
   "player removed",
   "player touched special",
   "server message",
   "player message",
   "puff spawned",
   "blood spawned",
   "actor spawned",
   "actor position",
   "actor miscellaneous state",
   "actor target",
   "actor state",
   "actor damaged",
   "actor killed",
   "actor removed",
   "line activated",
   "monster active",
   "monster awakened",
   "missile spawned",
   "missile exploded",
   "cube spawned",
   "sector position",
   "announcer event",
   "vote request",
   "vote",
   "vote result",
#if 0
   "special spawned",
   "special status",
   "special removed",
#endif
   "tic finished",
};

const char *frag_level_names[fl_max] = {
   "",
   "on a killing spree",
   "on a rampage",
   "dominating",
   "unstoppable",
   "God like"
};

const char *console_frag_level_names[fl_max] = {
   "",
   "Killing Spree!",
   "Rampage!",
   "Dominating!",
   "Unstoppable!",
   "God Like!"
};

const char *consecutive_frag_level_names[cfl_max] = {
   "",
   "",
   "Double Kill!",
   "Multi Kill!",
   "Ultra Kill!",
   "Monster Kill!"
};

unsigned int respawn_protection_time;
unsigned int death_time_limit;
unsigned int death_time_expired_action;
unsigned int friendly_damage_percentage;

extern bool d_fastrefresh;
extern int weapon_preferences[2][NUMWEAPONS+1];
extern char *weapon_str[NUMWEAPONS];

void Handler_spectate(void);
void Handler_spectate_next(void);
void Handler_spectate_prev(void);

static void build_game_state(nm_currentstate_t *message)
{
   unsigned int i;

   message->message_type = nm_currentstate;
   memcpy(message->flags, cs_flags, sizeof(flag_t) * team_color_max);
   for(i = team_color_none; i < team_color_max; i++)
      message->team_scores[i] = team_scores[i];

   for(i = 0; i < MAXPLAYERS; i++)
      message->playeringame[i] = playeringame[i];
}

static int build_save_buffer(byte **buffer)
{
   P_SaveCurrentLevel(cs_state_file_path, "cs");
   return M_ReadFile(cs_state_file_path, buffer);
}

void CS_Init(void)
{
   cs_state_file_path = ecalloc(
      char *, strlen(userpath) + strlen("/cs.state") + 1, sizeof(char)
   );

   sprintf(cs_state_file_path, "%s/cs.state", userpath);
   M_NormalizeSlashes(cs_state_file_path);

   if(CS_SERVER)
      SV_Init();
   else
      CL_Init(myargv[M_CheckParm("-csjoin") + 1]);
}

bool CS_CheckURI(char *uri)
{
   if((strncmp(uri, "http", 4)   == 0) || // [CG] Also works for "https".
      (strncmp(uri, "file", 4)   == 0) ||
      (strncmp(uri, "ftp", 4)    == 0) ||
      (strncmp(uri, "gopher", 6) == 0) ||
      (strncmp(uri, "scp", 4)    == 0) ||
      (strncmp(uri, "sftp", 4)   == 0) ||
      (strncmp(uri, "tftp", 4)   == 0) ||
      (strncmp(uri, "telnet", 4) == 0) ||
      (strncmp(uri, "dict", 4)   == 0))
   {
      return true;
   }

   return false;
}

void CS_DoWorldDone(void)
{
   unsigned int i;

   idmusnum = -1;
   hub_changelevel = false;
   CS_InitNew();
   AM_clearMarks();
   ACS_RunDeferredScripts();

   for(i = 0; i < MAXPLAYERS; i++)
   {
      clients[i].spectating = true;
      clients[i].join_tic = gametic;

      if(playeringame[i])
      {
         if(CS_CLIENT)
            cl_spawning_actor_from_message = true;

         CS_SpawnPlayerCorrectly(i, true);

         if(CS_CLIENT)
            cl_spawning_actor_from_message = false;
      }
   }

   if(CS_SERVER)
      sv_should_send_new_map = true;
}

void CS_FormatTime(char *formatted_time, unsigned int seconds)
{
   unsigned int hours, minutes;

   hours = seconds / 3600;
   seconds = seconds % 3600;
   minutes = seconds / 60;
   seconds = seconds % 60;

   if(hours > 0)
      sprintf(formatted_time, "%d:%.2d:%.2d", hours, minutes, seconds);
   else
      sprintf(formatted_time, "%.2d:%.2d", minutes, seconds);
}

void CS_FormatTicsAsTime(char *formatted_time, unsigned int tics)
{
   CS_FormatTime(formatted_time, tics / TICRATE);
}

void CS_SetDisplayPlayer(int playernum)
{
   int old_displayplayer = displayplayer;

   displayplayer = playernum;

   if(displayplayer != old_displayplayer)
   {
     ST_Start();
     HU_Start();
     S_UpdateSounds(players[displayplayer].mo);
     P_ResetChasecam();
   }

   P_FollowCamOff();

   if(camera == &followcam)
     camera = NULL;

   // use chasecam when player is dead.
   if(players[displayplayer].health <= 0)
      P_ChaseStart();
   else
      P_ChaseEnd();
}

char* CS_IPToString(uint32_t ip_address)
{
   qstring buffer;

   buffer.Printf(16, "%u.%u.%u.%u", ((ip_address       ) & 0xff),
                                    ((ip_address  >>  8) & 0xff),
                                    ((ip_address  >> 16) & 0xff),
                                    ((ip_address  >> 24) & 0xff));
   return estrdup(buffer.constPtr());
}

char* CS_VersionString(void)
{
   qstring buffer;

   buffer.Printf(12, "%u.%u.%u", version / 100, version % 100, subversion);

   return estrdup(buffer.constPtr());
}

char* CS_GetSHA1HashFile(char *path)
{
   HashData localHash = HashData(HashData::SHA1);
   size_t bytes_read = 0;
   size_t total_bytes_read = 0;
   unsigned int chunk_size = 512;
   unsigned char chunk[512];
   FILE *f = fopen(path, "rb");

   if(f == NULL)
   {
      I_Error(
         "CS_GetSHA1HashFile: Error opening %s: %s.\n", path, strerror(errno)
      );
   }

   while(1)
   {
      if((bytes_read = fread((void *)chunk, sizeof(char), chunk_size, f)))
         localHash.addData((const uint8_t *)chunk, (uint32_t)bytes_read);

      total_bytes_read += bytes_read;

      if(feof(f))
         break;

      if(ferror(f))
      {
         I_Error(
            "CS_GetSHA1HashFile: Error computing SHA-1 hash for %s: %s.\n",
            path,
            strerror(errno)
         );
      }
   }

   localHash.wrapUp();
   return localHash.digestToString();
}

char* CS_GetSHA1Hash(const char *input, size_t input_size)
{
   return HashData(
      HashData::SHA1, (const uint8_t *)input, (uint32_t)input_size
   ).digestToString();
}

void CS_SetPlayerName(player_t *player, const char *name)
{
   bool initializing_name = false;

   if(strlen(player->name) == 0)
      initializing_name = true;

   if(strlen(name) >= MAX_NAME_SIZE)
   {
      if(initializing_name)
         I_Error("Name size exceeds limit (%d).\n", MAX_NAME_SIZE);

      doom_printf("Name size exceeds limit (%d).\n", MAX_NAME_SIZE);
      return;
   }

   strncpy(player->name, name, MAX_NAME_SIZE);
}

void CS_InitPlayer(int playernum)
{
   memset(&players[playernum], 0, sizeof(player_t));

   players[playernum].colormap = default_colour;
   players[playernum].pclass =
      E_PlayerClassForName(GameModeInfo->defPClassName);
   players[playernum].skin = P_GetDefaultSkin(&players[playernum]);

   if(playernum == consoleplayer)
   {
      if(CS_SERVER)
         CS_SetPlayerName(&players[playernum], SERVER_NAME);
      else
         CS_SetPlayerName(&players[playernum], default_name);
   }
   else
      CS_SetPlayerName(&players[playernum], "");
}

void CS_InitPlayers(void)
{
   int i;

   for(i = 0; i < MAXPLAYERS; i++)
      CS_InitPlayer(i);
}

void CS_ZeroClient(int clientnum)
{
   client_t *client = &clients[clientnum];
   server_client_t *sc;

   memset(client, 0, sizeof(client_t));
   client->spectating = true;

   if(CS_SERVER && server_clients != NULL)
   {
      // [CG] Clear the server client too.
      sc = &server_clients[clientnum];
      sc->connect_id = 0;
      memset(&sc->address, 0, sizeof(ENetAddress));
      sc->auth_level = cs_auth_none;
      sc->current_request = scr_none;
      sc->received_game_state = false;
      sc->command_buffer_filled = false;
      sc->last_auth_attempt = 0;
      sc->commands_dropped = 0;
      sc->last_command_run_index = 0;
      sc->last_command_run_world_index = 0;
      sc->last_command_received_index = 0;
      sc->command_world_index = 0;
      sc->received_command_for_current_map = 0;
      M_QueueFree(&sc->commands);
      memset(sc->positions, 0, MAX_POSITIONS * sizeof(cs_player_position_t));
      memset(sc->misc_states, 0, MAX_POSITIONS * sizeof(cs_misc_state_t));
      memset(sc->player_states, 0, MAX_POSITIONS * sizeof(playerstate_t));
      memcpy(
         sc->weapon_preferences,
         weapon_preferences[1],
         (NUMWEAPONS + 1) * sizeof(int)
      );
      sc->weapon_switch_on_pickup = false;
      sc->ammo_switch_on_pickup = false;
      sc->options.player_bobbing = false;
      sc->options.doom_weapon_toggles = false;
      sc->options.autoaim = false;
      sc->options.autoaim = 0;
      memset(&sc->saved_position, 0, sizeof(cs_player_position_t));
      memset(&sc->saved_misc_state, 0, sizeof(cs_misc_state_t));
      sc->buffering = false;
      sc->finished_waiting_in_queue_tic = 0;
   }
}

void CS_ZeroClients(void)
{
   int i;
   server_client_t *server_client;

   for(i = 0; i < MAXPLAYERS; i++)
      CS_ZeroClient(i);

   if(CS_SERVER)
   {
      // [CG] Copy our configuration information into our own server client so
      //      it can be restored later.
      server_client = &server_clients[consoleplayer];
      server_client->options.player_bobbing = player_bobbing;
      server_client->options.doom_weapon_toggles = doom_weapon_toggles;
      server_client->options.autoaim = autoaim;
      server_client->options.weapon_speed = weapon_speed;
   }
}

size_t CS_BuildGameState(int playernum, byte **buffer)
{
   int state_size;
   byte *savebuffer;
   nm_currentstate_t message;

   build_game_state(&message);
   state_size = build_save_buffer(&savebuffer);

   message.state_size = state_size;

   *buffer = emalloc(byte *, sizeof(nm_currentstate_t) + state_size);
   memcpy(*buffer, &message, sizeof(nm_currentstate_t));
   memcpy(*buffer + sizeof(nm_currentstate_t), savebuffer, state_size);

   efree(savebuffer);

   return sizeof(nm_currentstate_t) + state_size;
}

bool CS_WeaponPreferred(int playernum, weapontype_t weapon_one,
                                       weapontype_t weapon_two)
{
   unsigned int p1, p2;
   server_client_t *server_client;

   weapon_one++;
   weapon_two++;

   if(CS_SERVER)
   {
      server_client = &server_clients[playernum];

      for(p1 = 0; p1 < NUMWEAPONS; p1++)
         if(server_client->weapon_preferences[p1] == weapon_one)
            break;

      for(p2 = 0; p2 < NUMWEAPONS; p2++)
         if(server_client->weapon_preferences[p2] == weapon_two)
            break;
   }
   else
   {
      for(p1 = 0; p1 < NUMWEAPONS; p1++)
         if(weapon_preferences[0][p1] == weapon_one)
            break;

      for(p2 = 0; p2 < NUMWEAPONS; p2++)
         if(weapon_preferences[0][p2] == weapon_two)
            break;
   }

   if(p1 < p2)
      return true;

   return false;
}

void CS_ReadJSON(Json::Value &json, const char *filename)
{
   byte *buffer = NULL;
   std::string data;
   Json::Reader reader;

   if(M_ReadFile(filename, &buffer) == -1)
   {
      I_Error(
         "CS_ReadJSON: Error reading file %s: %s.\n", filename, strerror(errno)
      );
   }

   data = (char *)buffer;

   if(!reader.parse(data, json))
   {
      I_Error(
         "CS_ReadJSON: Error parsing JSON: %s.\n",
         reader.getFormattedErrorMessages().c_str()
      );
   }
}

void CS_WriteJSON(const char *filename, Json::Value &value, bool styled)
{
   std::ofstream out_file;
   Json::FastWriter fast_writer;
   Json::StyledWriter styled_writer;

   out_file.open(filename);

   if(styled)
      out_file << styled_writer.write(value);
   else
      out_file << fast_writer.write(value);
}

void CS_HandleSpectateKey(event_t *ev)
{
   if(!clients[consoleplayer].spectating)
      if(CS_CLIENT && ev->type == ev_keydown)
         Handler_spectate();
}

void CS_HandleSpectatePrevKey(event_t *ev)
{
   if(ev->type != ev_keydown)
      return;

   if(!clientserver || clients[consoleplayer].spectating)
      Handler_spectate_prev();
}

void CS_HandleSpectateNextKey(event_t *ev)
{
   if(ev->type != ev_keydown)
      return;

   if(!clientserver || clients[consoleplayer].spectating)
      Handler_spectate_next();
}

void CS_HandleFlushPacketBufferKey(event_t *ev)
{
   if(CS_CLIENT && net_peer != NULL && ev->type == ev_keydown)
      cl_packet_buffer.setNeedsFlushing(true);
}

void CS_HandleUpdatePlayerInfoMessage(nm_playerinfoupdated_t *message)
{
   char *buffer;
   server_client_t *server_client = NULL;
   bool respawn_player = false;
   int playernum = message->player_number;
   player_t *player = &players[playernum];
   client_t *client = &clients[playernum];

   if(CS_SERVER)
      server_client = &server_clients[playernum];

   // [CG] Now dealing with client string info.
   if(message->info_type == ci_name ||
      message->info_type == ci_skin ||
      message->info_type == ci_class)
   {
      if(message->string_size > MAX_STRING_SIZE)
      {
         doom_printf(
            "CS_HUPIM: string size exceeds limit (%d)\n", MAX_STRING_SIZE
         );
         return;
      }
      buffer = ((char *)message) + sizeof(nm_playerinfoupdated_t);
      if(message->info_type == ci_name)
      {
         if(strlen(buffer) >= MAX_NAME_SIZE)
         {
            doom_printf("Name update message exceeds maximum name length.\n");
            return;
         }

         if(!strlen(buffer))
         {
            if(CS_SERVER)
               SV_SendMessage(playernum, "Cannot blank your name.\n");

            return;
         }

         if(playernum != consoleplayer &&
            strncmp(player->name, buffer, MAX_NAME_SIZE) == 0)
         {
            // [CG] If the name is unchanged, don't do anything.
            return;
         }

         if(strlen(player->name) > 0)
         {
            // [CG] Only print out a notice if the original name wasn't blank.
            if(playernum == consoleplayer)
               doom_printf("You are now known as %s.", buffer);
            else
               doom_printf("%s is now known as %s.", player->name, buffer);
         }
         else if(playernum == consoleplayer)
            doom_printf("Connected as %s.", buffer);
         else
            doom_printf("%s connected.", buffer);
         CS_SetPlayerName(player, buffer);
      }
      else if(message->info_type == ci_skin)
         CS_SetSkin(buffer, playernum);
      else if(message->info_type == ci_class)
      {
         // [CG] I'm not even sure you're supposed to be able to do this,
         //      but let's give it a try.
         player->pclass = E_PlayerClassForName(buffer);
      }

      if(CS_SERVER)
      {
         SV_BroadcastPlayerStringInfo(
            playernum, (client_info_e)message->info_type
         );
      }

      return;
   }

   // [CG] Now dealing with client array info.
   if(message->info_type == ci_frags         ||
      message->info_type == ci_power_enabled ||
      message->info_type == ci_owns_card     ||
      message->info_type == ci_owns_weapon   ||
      message->info_type == ci_ammo_amount   ||
      message->info_type == ci_max_ammo      ||
      message->info_type == ci_pwo)
   {
      if(message->info_type == ci_frags)
      {
         if(CS_CLIENT)
         {
            player->frags[message->array_index] = message->int_value;
            HU_FragsUpdate();
         }
      }
      else if(message->info_type == ci_power_enabled)
      {
         if(CS_CLIENT)
            player->powers[message->array_index] = message->int_value;
      }
      else if(message->info_type == ci_owns_card)
      {
         if(CS_CLIENT)
         {
            if(message->boolean_value)
               player->cards[message->array_index] = true;
            else
               player->cards[message->array_index] = false;
         }
      }
      else if(message->info_type == ci_owns_weapon)
      {
         if(CS_CLIENT)
         {
            if(message->boolean_value)
               player->weaponowned[message->array_index] = true;
            else
               player->weaponowned[message->array_index] = false;
         }
      }
      else if(message->info_type == ci_ammo_amount)
      {
         if(CS_CLIENT)
            player->ammo[message->array_index] = message->int_value;
      }
      else if(message->info_type == ci_max_ammo)
      {
         if(CS_CLIENT)
            player->maxammo[message->array_index] = message->int_value;
      }
      else if(message->info_type == ci_pwo)
      {
         if(CS_SERVER)
         {
            SV_SetWeaponPreference(
               playernum, message->array_index, message->int_value
            );
         }
      }
      return;
   }

   // [CG] Now dealing with scalars.
   if(message->info_type == ci_team)
   {
      // [CG] Don't do anything if the value hasn't actually changed.
      if(client->team == message->int_value)
         return;

      // [CG] If this isn't a team game, then we don't care about client teams.
      if(!CS_TEAMS_ENABLED)
         return;

      if(message->int_value < team_color_none ||
         message->int_value > team_color_max)
      {
         if(CS_SERVER)
         {
            SV_SendMessage(
               playernum, "Invalid team %d.\n", message->int_value
            );
         }
         else if(CS_CLIENT)
         {
            // [CG] TODO: Really if we somehow get an invalid team from the
            //            server it means something else is broken, so this
            //            should probably be handled with more than just an
            //            error message.
            doom_printf("Invalid team %d.\n", message->int_value);
         }
         return;
      }

      // [CG] If the player is holding a flag, they must drop it.
      CS_DropFlag(playernum);

      client->team = message->int_value;

      if(CS_CLIENT && CS_TEAMS_ENABLED && playernum == consoleplayer)
      {
         // [CG] Set the announcer type to Unreal if the client isn't on a
         //      team because it uses "red/blue team" and the Quake announcer
         //      uses "your/enemy team", and "your/enemy" won't apply if you're
         //      not on a team.
         if(client->team == team_color_none)
            CS_SetAnnouncer(s_announcer_type_names[S_ANNOUNCER_UNREAL]);
         else
            CS_SetAnnouncer(s_announcer_type_names[s_announcer_type]);
         CS_UpdateTeamSpecificAnnouncers(client->team);
      }

      if(CS_SERVER)
         SV_QueueSetClientNotPlaying(playernum);

      if(client->queue_level == ql_none)
      {
         if(playernum == consoleplayer)
         {
            doom_printf(
               "You are now watching on the %s team.",
               team_color_names[client->team]
            );
         }
         else
         {
            doom_printf(
               "%s is now watching on the %s team.",
               player->name,
               team_color_names[client->team]
            );
         }
      }
      else if(client->queue_level == ql_waiting)
      {
         if(playernum == consoleplayer)
         {
            doom_printf(
               "You are now waiting on the %s team.",
               team_color_names[client->team]
            );
         }
         else
         {
            doom_printf(
               "%s is now waiting on the %s team.",
               player->name,
               team_color_names[client->team]
            );
         }
      }
      else
      {
         if(playernum == consoleplayer)
         {
            doom_printf(
               "You are now on the %s team.",
               team_color_names[client->team]
            );
         }
         else
         {
            doom_printf(
               "%s is now on the %s team.",
               player->name,
               team_color_names[client->team]
            );
         }

         if(CS_SERVER)
            respawn_player = true;
      }
   }
   else if(message->info_type == ci_spectating)
   {
      // [CG] Don't do anything if the value hasn't actually changed.
      if(client->spectating == message->boolean_value)
         return;

      if(!message->boolean_value)
      {
         // [CG] The case where the player is attempting to join the game is
         //      handled in P_RunPlayerCommand serverside and
         //      CL_HandlePlayerSpawned clientside.  Receiving a false update
         //      for ci_spectating means the sending client is bugged somehow.
         if(CS_SERVER)
            SV_SendMessage(playernum, "Invalid join request.\n");

         doom_printf(
            "Received invalid spectating value from %d.\n", playernum
         );
         return;
      }

      // [CG] At this point, the player is attempting to spectate.

      // [CG] If the player is holding a flag, they must drop it.
      CS_DropFlag(playernum);

      if(CS_SERVER)
      {
         client->spectating = true;
         player->frags[playernum]++; // [CG] Spectating costs a frag.
         HU_FragsUpdate();
         SV_BroadcastPlayerArrayInfo(playernum, ci_frags, playernum);
         SV_QueueSetClientNotPlaying(playernum);
         respawn_player = true;
      }
      else
      {
         if(playernum == consoleplayer)
         {
            doom_printf("You are now spectating.");
            CS_UpdateQueueMessage();
         }
         else
            doom_printf("%s is now spectating.\n", player->name);
      }
   }
   else if(message->info_type == ci_ready_weapon)
   {
      // [CG] We always know what our own ready and pending weapons are.
      if(CS_CLIENT && playernum != consoleplayer)
         player->readyweapon = message->int_value;
   }
   else if(message->info_type == ci_pending_weapon)
   {
      // [CG] We always know what our own ready and pending weapons are.
      if(CS_CLIENT && playernum != consoleplayer)
         player->pendingweapon = message->int_value;
   }
   else if(message->info_type == ci_colormap)
      player->colormap = message->int_value;
   else if(message->info_type == ci_kill_count)
   {
      if(CS_CLIENT)
         player->killcount = message->int_value;
   }
   else if(message->info_type == ci_item_count)
   {
      if(CS_CLIENT)
         player->itemcount = message->int_value;
   }
   else if(message->info_type == ci_secret_count)
   {
      if(CS_CLIENT)
         player->secretcount = message->int_value;
   }
   else if(message->info_type == ci_cheats)
   {
      if(CS_CLIENT)
         player->cheats = message->int_value;
   }
   else if(message->info_type == ci_health)
   {
      if(CS_CLIENT)
         player->health = message->int_value;
   }
   else if(message->info_type == ci_armor_points)
   {
      if(CS_CLIENT)
         player->armorpoints = message->int_value;
   }
   else if(message->info_type == ci_armor_type)
   {
      if(CS_CLIENT)
         player->armortype = message->int_value;
   }
   else if(message->info_type == ci_owns_backpack)
   {
      if(CS_CLIENT)
      {
         if(message->boolean_value)
            player->backpack = true;
         else
            player->backpack = false;
      }
   }
   else if(message->info_type == ci_did_secret)
   {
      if(CS_CLIENT)
      {
         if(message->boolean_value)
            player->didsecret = true;
         else
            player->didsecret = false;
      }
   }
   else if(message->info_type == ci_queue_level)
   {
      if(CS_CLIENT)
      {
         client->queue_level = message->int_value;
         if(playernum == consoleplayer)
            CS_UpdateQueueMessage();
      }
   }
   else if(message->info_type == ci_queue_position)
   {
      if(CS_CLIENT)
      {
         client->queue_position = message->int_value;
         if(playernum == consoleplayer)
            CS_UpdateQueueMessage();
      }
   }
   else if(message->info_type == ci_wsop)
   {
      if(CS_SERVER)
         server_client->weapon_switch_on_pickup = message->int_value;
   }
   else if(message->info_type == ci_asop)
   {
      if(CS_SERVER)
         server_client->ammo_switch_on_pickup = message->int_value;
   }
   else if(message->info_type == ci_bobbing)
   {
      if(CS_SERVER)
         server_client->options.player_bobbing = message->boolean_value;
   }
   else if(message->info_type == ci_weapon_toggle)
   {
      if(CS_SERVER)
         server_client->options.doom_weapon_toggles = message->boolean_value;
   }
   else if(message->info_type == ci_autoaim)
   {
      if(CS_SERVER)
         server_client->options.autoaim = message->boolean_value;
   }
   else if(message->info_type == ci_weapon_speed)
   {
      if(CS_SERVER)
         server_client->options.weapon_speed = message->int_value;
   }
   else if(message->info_type == ci_buffering)
   {
      if(CS_SERVER)
         server_client->buffering = message->boolean_value;
   }
   else if(message->info_type == ci_afk)
   {
      if(message->boolean_value)
      {
         client->afk = true;
         doom_printf("%s is AFK.", (player->name));
      }
      else
         client->afk = false;
   }
   else
   {
      doom_printf(
         "CS_HUPIM: received unknown info type %d\n", message->info_type
      );
      return;
   }

   if(CS_SERVER)
   {
      SV_BroadcastPlayerScalarInfo(
         playernum, (client_info_e)message->info_type
      );

      if(respawn_player)
         SV_SpawnPlayer(playernum, true);
   }
}

void CS_DisconnectPeer(ENetPeer *peer, enet_uint32 reason)
{
   // [CG] The peer's already disconnected, so don't worry about this stuff.
   if(peer == NULL)
      return;

   enet_peer_disconnect(peer, reason);
   enet_peer_reset(peer);
}

// [CG] A c/s version of P_SetSkin from p_skin.c.
void CS_SetSkin(const char *skin_name, int playernum)
{
   skin_t *skin;
   player_t *player = &players[playernum];

   if(!playeringame[playernum])
      return;

   skin = P_SkinForName(skin_name);
   // [CG] FIXME: this should be better all around
   if(skin == NULL)
      return;

   player->skin = skin;

   if(gamestate == GS_LEVEL)
   {
      player->mo->skin = skin;
      if(player->mo->state &&
         player->mo->state->sprite == player->mo->info->defsprite)
      {
         player->mo->sprite = skin->sprite;
      }
   }

   if(CS_CLIENT && playernum == consoleplayer)
      default_skin = skin->skinname;
}

size_t CS_BuildPlayerStringInfoPacket(nm_playerinfoupdated_t **update_message,
                                      int playernum, client_info_e info_type)
{
   char *buffer;
   char *player_info;
   size_t info_size;
   player_t *player = &players[playernum];

   if(info_type == ci_name)
      player_info = player->name;
   else if(info_type == ci_skin)
      player_info = player->skin->skinname;
   else if(info_type == ci_class)
      player_info = player->pclass->mnemonic;
   else
   {
      I_Error(
         "CS_GetPlayerStringInfo: Unsupported info type %d\n",
         info_type
      );
   }

   info_size = strlen(player_info) + 1;

   buffer = ecalloc(char *, 1, sizeof(nm_playerinfoupdated_t) + info_size);

   *update_message = (nm_playerinfoupdated_t *)buffer;
   (*update_message)->string_size = info_size;
   (*update_message)->message_type = nm_playerinfoupdated;
   (*update_message)->player_number = playernum;
   (*update_message)->info_type = info_type;
   (*update_message)->array_index = 0;

   memcpy(buffer + sizeof(nm_playerinfoupdated_t), player_info, info_size);

   return sizeof(nm_playerinfoupdated_t) + info_size;
}

void CS_BuildPlayerArrayInfoPacket(nm_playerinfoupdated_t *update_message,
                                   int playernum, client_info_e info_type,
                                   int array_index)
{
   player_t *player = &players[playernum];

   update_message->message_type = nm_playerinfoupdated;
   update_message->player_number = playernum;
   update_message->info_type = info_type;
   update_message->array_index = array_index;

   if(info_type == ci_frags)
      update_message->int_value =  player->frags[array_index];
   else if(info_type == ci_owns_card)
      update_message->boolean_value = player->cards[array_index];
   else if(info_type == ci_owns_weapon)
      update_message->boolean_value = player->killcount;
   else if(info_type == ci_power_enabled)
      update_message->int_value = player->powers[array_index];
   else if(info_type == ci_ammo_amount)
      update_message->int_value = player->ammo[array_index];
   else if(info_type == ci_max_ammo)
      update_message->int_value = player->maxammo[array_index];
   else if(info_type == ci_pwo)
      update_message->int_value = weapon_preferences[0][array_index];
   else
   {
      I_Error(
         "CS_GetPlayerArrayInfo: Unsupported info type %d\n",
         info_type
      );
   }
}

void CS_BuildPlayerScalarInfoPacket(nm_playerinfoupdated_t *update_message,
                                    int playernum, client_info_e info_type)
{
   client_t *client = &clients[playernum];
   player_t *player = &players[playernum];
   server_client_t *server_client = NULL;

   if(CS_SERVER)
      server_client = &server_clients[playernum];

   update_message->message_type = nm_playerinfoupdated;
   update_message->player_number = playernum;
   update_message->info_type = info_type;
   update_message->array_index = 0;

   if(info_type == ci_team)
      update_message->int_value = client->team;
   else if(info_type == ci_spectating)
      update_message->boolean_value = client->spectating;
   else if(info_type == ci_kill_count)
      update_message->int_value = player->killcount;
   else if(info_type == ci_item_count)
      update_message->int_value = player->itemcount;
   else if(info_type == ci_secret_count)
      update_message->int_value = player->secretcount;
   else if(info_type == ci_colormap)
      update_message->int_value = player->colormap;
   else if(info_type == ci_cheats)
      update_message->int_value = player->cheats;
   else if(info_type == ci_health)
      update_message->int_value = player->health;
   else if(info_type == ci_armor_points)
      update_message->int_value = player->armorpoints;
   else if(info_type == ci_armor_type)
      update_message->int_value = player->armortype;
   else if(info_type == ci_ready_weapon)
      update_message->int_value = player->readyweapon;
   else if(info_type == ci_pending_weapon)
      update_message->int_value = player->pendingweapon;
   else if(info_type == ci_owns_backpack)  // backpack
      update_message->boolean_value = player->backpack;
   else if(info_type == ci_did_secret)    // didsecret
      update_message->boolean_value = player->didsecret;
   else if(info_type == ci_queue_level)
      update_message->int_value = client->queue_level;
   else if(info_type == ci_queue_position)
      update_message->int_value = client->queue_position;
   else if(info_type == ci_wsop)
      update_message->int_value = GET_WSOP(playernum);
   else if(info_type == ci_asop)
      update_message->int_value = GET_ASOP(playernum);
   else if(info_type == ci_afk)
      update_message->boolean_value = client->afk;
   else if(info_type == ci_bobbing)
   {
      if(CS_CLIENT)
         update_message->boolean_value = player_bobbing;
      else if(CS_SERVER)
         update_message->boolean_value = server_client->options.player_bobbing;
   }
   else if(info_type == ci_weapon_toggle)
   {
      if(CS_CLIENT)
         update_message->boolean_value = doom_weapon_toggles;
      else if(CS_SERVER)
      {
         update_message->boolean_value =
            server_client->options.doom_weapon_toggles;
      }
   }
   else if(info_type == ci_autoaim)
   {
      if(CS_CLIENT)
         update_message->boolean_value = autoaim;
      else if(CS_SERVER)
         update_message->boolean_value = server_client->options.autoaim;
   }
   else if(info_type == ci_weapon_speed)
   {
      if(CS_CLIENT)
         update_message->int_value = weapon_speed;
      else if(CS_SERVER)
         update_message->int_value = server_client->options.weapon_speed;
   }
   else if(info_type == ci_buffering)
   {
      if(CS_CLIENT)
         update_message->boolean_value = cl_packet_buffer.enabled();
      else if(CS_SERVER)
         update_message->boolean_value = server_client->buffering;
   }
   else
   {
      I_Error(
         "CS_GetPlayerScalarInfo: Unsupported info type %d\n",
         info_type
      );
   }
}

void CS_CheckSprees(int sourcenum, int targetnum, bool suicide, bool team_kill)
{
   client_t *source_client = &clients[sourcenum];
   player_t *source_player = &players[sourcenum];
   unsigned int latest_index;

   if(CS_CLIENT)
      latest_index = cl_latest_world_index;
   else if(CS_SERVER)
      latest_index = sv_world_index;
   else
      return;

   if(CS_CLIENT || !CS_HEADLESS)
   {
      if(suicide && (sourcenum == consoleplayer))
         CS_Announce(ae_suicide_death, NULL);
      else if(team_kill)
         CS_Announce(ae_team_kill, NULL);
   }

   if(suicide || team_kill)
   {
      // [CG] If you suicide or team kill, all your sprees are over.
      source_client->frags_this_life = 0;
      source_client->frag_level = fl_none;
      source_client->consecutive_frag_level = 0;
   }
   else
   {
      source_client->frags_this_life++;

      if(source_client->frag_level < (fl_max - 1))
      {
         unsigned int new_fl = source_client->frags_this_life / 5;

         if((new_fl < fl_max) && (source_client->frag_level != new_fl))
         {
            source_client->frag_level = new_fl;

            doom_printf(
               "%s is %s!",
               source_player->name,
               frag_level_names[source_client->frag_level]
            );

            if((sourcenum == consoleplayer) && cl_show_sprees)
            {
               HU_CenterMessage(console_frag_level_names[
                  source_client->frag_level
               ]);
            }
         }
      }

      if((latest_index - source_client->last_frag_tic) <= (3 * TICRATE))
      {
         if(source_client->consecutive_frag_level < (cfl_max - 1))
            source_client->consecutive_frag_level++;

         if((sourcenum == consoleplayer) &&
            cl_show_sprees &&
            source_client->consecutive_frag_level > cfl_none)
         {
            HU_CenterMessage(consecutive_frag_level_names[
               source_client->consecutive_frag_level
            ]);
         }
      }

      source_client->last_frag_tic = latest_index;
   }
}

void CS_SetSpectator(int playernum, bool spectating)
{
   client_t *client = &clients[playernum];
   player_t *player = &players[playernum];

   if(spectating)
   {
      player->weaponowned[wp_pistol] = false;
      player->ammo[am_clip] = 0;
      player->readyweapon = player->pendingweapon = wp_fist;

      player->mo->flags &= ~MF_SOLID;
      player->mo->flags &= ~MF_SHOOTABLE;
      player->mo->flags &= ~MF_PICKUP;
      player->mo->flags |=  MF_NOCLIP;
      player->mo->flags |=  MF_SLIDE;

      player->mo->flags2 &= ~MF2_FOOTCLIP;
      player->mo->flags2 |=  MF2_NOCROSS;
      player->mo->flags2 |=  MF2_DONTDRAW;
      player->mo->flags2 |=  MF2_INVULNERABLE;
      player->mo->flags2 |=  MF2_NOSPLASH;

      player->mo->flags3 &= ~MF3_TELESTOMP;
      player->mo->flags3 &= ~MF3_WINDTHRUST;
      player->cheats |= CF_NOCLIP;
   }
   else
   {
      player->weaponowned[wp_pistol] = true;
      player->ammo[am_clip] = initial_bullets;
      player->readyweapon = wp_pistol;
      player->mo->flags &= ~MF_NOCLIP;
      player->cheats &= ~CF_NOCLIP;
   }

   client->spectating = spectating;
}

void CS_SpawnPlayer(int playernum, fixed_t x, fixed_t y, fixed_t z,
                    angle_t angle, bool as_spectator)
{
   int i;
   player_t *player = &players[playernum];
   client_t *client = &clients[playernum];

   client->death_time = 0;

   playeringame[playernum] = true;

   if(CS_SERVER)
   {
      if(player->mo != NULL)
      {
         SV_BroadcastActorRemoved(player->mo);
         player->mo->removeThinker();
         player->mo = NULL;
      }
   }

   G_PlayerReborn(playernum);

   // [CG] Stop perpetually displaying the scoreboard in c/s mode once we
   //      respawn.
   if(!CS_HEADLESS || playernum == consoleplayer)
      action_scoreboard = 0;

   // [CG] Only set non-spectators' colormaps.
   if((!as_spectator) && CS_TEAMS_ENABLED && (client->team != team_color_none))
      player->colormap = team_colormaps[client->team];

   player->mo = P_SpawnMobj(x, y, z, player->pclass->type);

   player->mo->colour = player->colormap;
   player->mo->angle = R_WadToAngle(angle);
   player->mo->player = player;
   player->mo->health = player->health;

   if(!player->skin)
      I_Error("CS_SpawnPlayer: player skin undefined!\n");

   player->mo->skin = player->skin;
   player->mo->sprite = player->skin->sprite;

   player->playerstate = PST_LIVE;
   player->refire = 0;
   player->damagecount = 0;
   player->bonuscount = 0;
   player->extralight = 0;
   player->fixedcolormap = 0;
   player->viewheight = VIEWHEIGHT;
   player->viewz = player->mo->z + VIEWHEIGHT;

   player->momx = player->momy = 0;

   P_SetupPsprites(player);

   if(DEATHMATCH)
   {
      for(i = 0; i < NUMCARDS; i++)
         player->cards[i] = true;
   }

   if(clientside && playernum == consoleplayer)
      ST_Start();

   if(clientside && playernum == displayplayer)
      P_ResetChasecam();

   CS_SetSpectator(playernum, as_spectator);
}

mapthing_t* CS_SpawnPlayerCorrectly(int playernum, bool as_spectator)
{
   mapthing_t *spawn_point;

   if(!as_spectator)
   {
      // [CG] In order for clipping routines to work properly, the player can't
      //      be a spectator when we search for a spawn point.
      CS_SetSpectator(playernum, false);
      if(DEATHMATCH)
      {
         if(CS_TEAMS_ENABLED)
            spawn_point = SV_GetTeamSpawnPoint(playernum);
         else
            spawn_point = SV_GetDeathMatchSpawnPoint(playernum);
      }
      else
         spawn_point = SV_GetCoopSpawnPoint(playernum);
      CS_SetSpectator(playernum, true);
   }
   else
      spawn_point = &playerstarts[0];

   CS_SpawnPlayer(
      playernum,
      spawn_point->x << FRACBITS,
      spawn_point->y << FRACBITS,
      ONFLOORZ,
      spawn_point->angle,
      as_spectator
   );

   return spawn_point;
}

void CS_SpawnPuff(Mobj *shooter, fixed_t x, fixed_t y, fixed_t z,
                  angle_t angle, int updown, bool ptcl)
{
   Mobj *puff = NULL;

   if(serverside)
   {
      puff = P_SpawnPuff(x, y, z, angle, updown, ptcl);
      if(CS_SERVER)
         SV_BroadcastPuffSpawned(puff, shooter, updown, ptcl);
   }
   else if(shooter->player && CL_SHOULD_PREDICT_SHOT(shooter))
   {
      puff = CL_SpawnPuff(0, x, y, z, angle, updown, ptcl);
   }
}

void CS_SpawnBlood(Mobj *shooter, fixed_t x, fixed_t y, fixed_t z,
                   angle_t angle, int damage, Mobj *target)
{
   Mobj *blood = NULL;

   if(serverside)
   {
      blood = P_SpawnBlood(x, y, z, angle, damage, target);
      if(CS_SERVER)
         SV_BroadcastBloodSpawned(blood, shooter, damage, target);
   }
   else if(shooter->player && CL_SHOULD_PREDICT_SHOT(shooter))
   {
      blood = CL_SpawnBlood(0, x, y, z, angle, damage, target);
   }
}

char* CS_ExtractMessage(char *data, size_t data_length)
{
   size_t message_length;
   char *message = NULL;
   nm_playermessage_t *player_message = (nm_playermessage_t *)data;

   if(player_message->length > 256)
   {
      printf("player_message->length > 256.\n");
      return NULL;
   }
   else if(data_length > sizeof(nm_playermessage_t) + 256)
   {
      printf("data_length > sizeof(nm_playermessage_t) + 256.\n");
      return NULL;
   }

   // [CG] So we avoid falling prey to malicious messages that don't have an
   //      ending '\0', we create a string 1 character larger than what the
   //      message should be and zero it.  If strlen is (length - 1), then the
   //      message was malicious and we return NULL.
   //      - length is size of packet data - size of playermessage.
   //      - strlen will be length - 1 (if correct).
   //      - buffer size will be length + 1
   data_length = data_length - sizeof(nm_playermessage_t);
   message = ecalloc(char *, data_length + 1, sizeof(char));
   memcpy(message, data + sizeof(nm_playermessage_t), data_length);
   message_length = strlen(message);

   // [CG] For debugging.
   // printf(
   //    "CS_ExtractMessage: %d, %d, %d, %d, %d.\n",
   //    packet->dataLength,
   //    sizeof(nm_playermessage_t),
   //    player_message->length,
   //    data_length,
   //    strlen(message)
   // );

   if(message_length >= data_length)
   {
      // [CG] The string didn't end in a '\0', instead strlen went all the way
      //      to the end of our zero'd buffer.  Consequently this message is
      //      either malformed, malicious, or both; don't use it.

      // [CG] For debugging.
      // printf("message_length >= data_length.\n");
      return NULL;
   }

   if(message_length != (data_length - 1))
   {
      // [CG] The message's reported length was incorrect, so this message is
      //      either malformed, malicious, or both; don't use it.

      // [CG] For debugging.
      // printf("message_length != (data_length - 1).\n");
      return NULL;
   }

   // [CG] Message is cool, go ahead and return it.

   // [CG] For debugging.
   // printf("CS_ExtractMessage: Message was cool.\n");
   return message;
}

void CS_MessageAll(event_t *ev)
{
   CS_SetMessageMode(MESSAGE_MODE_ALL);
}

void CS_FlushConnection(void)
{
   if(net_host != NULL)
      enet_host_flush(net_host);
}

void CS_ServiceNetwork(void)
{
   if(net_host && net_peer)
      enet_host_service(net_host, NULL, 1);
}

void CS_ReadFromNetwork(unsigned int timeout)
{
   ENetEvent event;
   char *address;
   int playernum = 0;

   while(enet_host_service(net_host, &event, timeout) > 0)
   {
      switch(event.type)
      {
      case ENET_EVENT_TYPE_CONNECT:
         if(CS_CLIENT)
         {
            // [CG] Clients should already have received their connect event
            //      from the server before this loop is ever run, so ignore
            //      further events of this type.
            doom_printf(
               "Somehow received a connection from server %d:%d\n",
               event.peer->address.host,
               event.peer->address.port
            );
         }
         else if(CS_SERVER)
         {
            address = CS_IPToString(event.peer->address.host);
            playernum = SV_GetPlayerNumberFromPeer(event.peer);
            if(playernum == 0)
            {
               doom_printf(
                  "Adding a new client: %s:%d\n",
                  address,
                  event.peer->address.port
               );

               // [CG] Try and find a slot for the new client.  If no slot is
               //      available, disconnect it.
               if(SV_HandleClientConnection(event.peer) == 0)
                  enet_peer_disconnect(event.peer, 1);
            }
            else
            {
               doom_printf(
                  "Received a spurious connection from: %s:%d\n",
                  address,
                  event.peer->address.port
               );
            }
            efree(address);
         }
         break;
      case ENET_EVENT_TYPE_RECEIVE:
         if(CS_SERVER)
            playernum = SV_GetPlayerNumberFromPeer(event.peer);

         // [CG] Save all received network messages in the demo if recording.
         if(cs_demo_recording)
         {
            if(!CS_WriteNetworkMessageToDemo(
                  event.packet->data, event.packet->dataLength, playernum))
            {
               doom_printf(
                  "Demo error, recording aborted: %s\n",
                  CS_GetDemoErrorMessage()
               );
               CS_StopDemo();
            }
         }

         if(CS_CLIENT)
         {
            cl_packet_buffer.add(
               (char *)event.packet->data, (uint32_t)event.packet->dataLength
            );
         }
         else if(CS_SERVER)
         {
            playernum = SV_GetPlayerNumberFromPeer(event.peer);
            if(playernum == 0)
            {
               doom_printf(
                  "Received a message from an unknown client, ignoring.\n"
               );
               return;
            }
            SV_HandleMessage(
               (char *)event.packet->data, event.packet->dataLength, playernum
            );
         }
         event.peer->data = NULL;
         enet_packet_destroy(event.packet);
         break;
      case ENET_EVENT_TYPE_DISCONNECT:
         if(CS_CLIENT)
            CL_Disconnect();

         if(CS_SERVER)
         {
            playernum = SV_GetClientNumberFromAddress(&event.peer->address);
            if(playernum && playernum < (signed int)MAX_CLIENTS
                         && playeringame[playernum])
            {
               SV_DisconnectPlayer(playernum, dr_no_reason);
            }
            else
            {
               address = CS_IPToString(event.peer->address.host);
               doom_printf(
                  "Received disconnect from unknown player %s:%d.\n",
                  address,
                  event.peer->address.port
               );
               efree(address);
            }
         }
         event.peer->data = NULL;
         break;
      case ENET_EVENT_TYPE_NONE:
         break;
      default:
         doom_printf("Received unknown event type: %d\n", event.type);
         break;
      }
   }
}

void CS_TryRunTics(void)
{
   if(CS_CLIENTDEMO)
      CL_RunDemoTics();
   else if(CS_SERVERDEMO)
      SV_RunDemoTics();
   else if(CS_CLIENT)
      CL_TryRunTics();
   else
      SV_TryRunTics();
}

VARIABLE_STRING(cs_demo_folder_path, NULL, 1024);
CONSOLE_VARIABLE(demo_folder_path, cs_demo_folder_path, cf_netonly) {}

void CS_AddCommands(void)
{
   C_AddCommand(demo_folder_path);
}

