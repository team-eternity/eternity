// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//-----------------------------------------------------------------------------
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//   Clientside network packet handlers.
//
//-----------------------------------------------------------------------------

#include <list>

#include "z_zone.h"
#include "a_common.h"
#include "c_io.h"
#include "c_net.h"
#include "c_runcmd.h"
#include "d_gi.h"
#include "d_main.h"
#include "doomstat.h"
#include "e_sound.h"
#include "e_things.h"
#include "g_dmatch.h"
#include "g_ctf.h"
#include "g_game.h"
#include "hu_frags.h"
#include "hu_stuff.h"
#include "i_system.h"
#include "i_thread.h"
#include "m_file.h"
#include "m_fixed.h"
#include "p_inter.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_mobj.h"
#include "p_saveg.h"
#include "p_tick.h"
#include "r_state.h"
#include "s_sound.h"

#include "cs_announcer.h"
#include "cs_cmd.h"
#include "cs_demo.h"
#include "cs_hud.h"
#include "cs_main.h"
#include "cs_netid.h"
#include "cs_position.h"
#include "cs_vote.h"
#include "cs_wad.h"
#include "cl_buf.h"
#include "cl_cmd.h"
#include "cl_main.h"
#include "cl_pred.h"
#include "cl_spec.h"
#include "cl_vote.h"

extern unsigned int rngseed;
extern int s_announcer_type;

static void send_any_packet(void *data, size_t data_size, uint8_t flags,
                            uint8_t channel_id)
{
   ENetPacket *packet;

   if(!net_peer)
      return;

   if(LOG_ALL_NETWORK_MESSAGES)
   {
      printf(
         "Sending [%s message].\n", network_message_names[*((int *)data)]
      );
   }

   packet = enet_packet_create(data, data_size, flags);
   enet_peer_send(net_peer, channel_id, packet);
}

static void send_packet(void *data, size_t data_size)
{
   send_any_packet(
      data, data_size, ENET_PACKET_FLAG_RELIABLE, RELIABLE_CHANNEL
   );
}

static void send_unreliable_packet(void *data, size_t data_size)
{
   send_any_packet(
      data, data_size, ENET_PACKET_FLAG_UNSEQUENCED, UNRELIABLE_CHANNEL
   );
}

static void send_message(message_recipient_e recipient_type,
                         unsigned int recipient_number, const char *message)
{
   nm_playermessage_t player_message;
   char *buffer;

   player_message.message_type = nm_playermessage;
   player_message.world_index = cl_current_world_index;
   player_message.recipient_type = recipient_type;
   player_message.sender_number = consoleplayer;
   player_message.recipient_number = recipient_number;
   player_message.length = strlen(message) + 1;

   buffer = ecalloc(
      char *, 1, sizeof(nm_playermessage_t) + (size_t)player_message.length
   );

   memcpy(buffer, &player_message, sizeof(nm_playermessage_t));
   memcpy(
      buffer + sizeof(nm_playermessage_t),
      message,
      ((size_t)player_message.length) - 1
   );

   if(recipient_type == mr_auth)
      doom_printf("Authorizing...");
   else if(recipient_type == mr_rcon)
      doom_printf("RCON: %s", message);
   else if(recipient_type != mr_vote)
   {
      S_StartSound(NULL, GameModeInfo->c_ChatSound);
      doom_printf("%s: %s", players[consoleplayer].name, message);
   }

   send_packet(
      buffer, sizeof(nm_playermessage_t) + (size_t)player_message.length
   );
   efree(buffer);
}

static char* CL_extractServerMessage(nm_servermessage_t *message)
{
   char *server_message = ((char *)message) + sizeof(nm_servermessage_t);

   if(strlen(server_message) != (message->length - 1))
      return NULL;

   return server_message;
}

static char* CL_extractPlayerMessage(nm_playermessage_t *message)
{
   char *player_message = ((char *)message) + sizeof(nm_playermessage_t);

   if(message->sender_number > MAXPLAYERS ||
      !playeringame[message->sender_number])
      return NULL;

   if(strlen(player_message) != (message->length - 1))
      return NULL;

   return player_message;
}

void CL_SendCommand(void)
{
   char *buffer;
   nm_playercommand_t *command_message;
   cs_cmd_t *command, *dest_command;
   ticcmd_t ticcmd;
   size_t buffer_size;
   uint32_t i, command_count, world_index;

   // [CG] Ensure no commands are made during demo playback.
   if(CS_DEMOPLAY)
      I_Error("Error: made a command during demo playback.\n");

   // I_StartTic();
   // D_ProcessEvents();
   command = CL_GetCommandAtIndex(cl_commands_sent);
   world_index = cl_latest_world_index;

   if(consoleactive)
   {
      memset(command, 0, sizeof(cs_cmd_t));
      command->index = cl_commands_sent;
      command->world_index = world_index;
   }
   else
   {
      G_BuildTiccmd(&ticcmd);
      CS_CopyTiccmdToCommand(command, &ticcmd, cl_commands_sent, world_index);
   }

   // [CG] Save all sent commands in the demo if recording.
   if(CS_DEMORECORD)
   {
      if(!cs_demo->write(command))
      {
         doom_printf(
            "Error writing player command to demo: %s", cs_demo->getError()
         );
         if(!cs_demo->stop())
            doom_printf("Error stopping demo: %s.", cs_demo->getError());
      }
   }

#if _CMD_DEBUG
   printf(
      "CL_SendCommand (%3u/%3u): Sending command %u: ",
      cl_current_world_index,
      cl_latest_world_index,
      command->world_index
   );
   CS_PrintTiccmd(&ticcmd);
   CS_PrintCommand(command);
#endif

   if(cl_reliable_commands)
      command_count = 1;
   else if(cl_commands_sent < (cl_command_bundle_size - 1))
      command_count = cl_commands_sent + 1;
   else
      command_count = cl_command_bundle_size;

   buffer_size =
      sizeof(nm_playercommand_t) + (sizeof(cs_cmd_t) * command_count);

   buffer = ecalloc(char *, sizeof(char), buffer_size);
   command_message = (nm_playercommand_t *)(buffer);
   dest_command = (cs_cmd_t *)(buffer + sizeof(nm_playercommand_t));

   command_message->message_type = nm_playercommand;
   command_message->command_count = command_count;

   CS_LogSMT(
      "%u/%u: Sending command %u/%u.\n",
      cl_latest_world_index,
      cl_current_world_index,
      cl_commands_sent,
      world_index
   );

   if(cl_reliable_commands)
   {
      CS_CopyCommand(dest_command, command);
      send_packet(buffer, buffer_size);
   }
   else
   {
      for(i = ((cl_commands_sent + 1) - command_count);
          i <= cl_commands_sent;
          i++, dest_command++)
      {
         CS_CopyCommand(dest_command, CL_GetCommandAtIndex(i));
      }
      send_unreliable_packet(buffer, buffer_size);
   }

   CL_CarrySectorPositions();

   cl_commands_sent++;
   efree(buffer);
}

void CL_SendPlayerStringInfo(client_info_e info_type)
{
   nm_playerinfoupdated_t *update_message;
   size_t buffer_size = CS_BuildPlayerStringInfoPacket(
      &update_message, consoleplayer, info_type
   );

   send_packet(update_message, buffer_size);
   efree(update_message);
}

void CL_SendPlayerArrayInfo(client_info_e info_type, int array_index)
{
   nm_playerinfoupdated_t update_message;

   CS_BuildPlayerArrayInfoPacket(
      &update_message, consoleplayer, info_type, array_index
   );

   send_packet(&update_message, sizeof(nm_playerinfoupdated_t));
}

void CL_SendPlayerScalarInfo(client_info_e info_type)
{
   nm_playerinfoupdated_t update_message;
   CS_BuildPlayerScalarInfoPacket(&update_message, consoleplayer, info_type);
   send_packet(&update_message, sizeof(nm_playerinfoupdated_t));
}

void CL_SendTeamRequest(int team)
{
   nm_playerinfoupdated_t update_message;

   update_message.message_type = nm_playerinfoupdated;
   update_message.player_number = consoleplayer;
   update_message.info_type = ci_team;
   update_message.array_index = 0;
   update_message.boolean_value = false;
   update_message.int_value = team;

   send_packet(&update_message, sizeof(nm_playerinfoupdated_t));
}

void CL_SendCurrentStateRequest(void)
{
   nm_clientrequest_t message;

   doom_printf("Sending request for current game state");

   message.message_type = nm_clientrequest;
   message.request_type = scr_current_state;

   send_packet(&message, sizeof(nm_clientrequest_t));
}

void CL_SendSyncRequest(void)
{
   nm_clientrequest_t message;

   doom_printf("Sending synchronization request");

   message.message_type = nm_clientrequest;
   message.request_type = scr_sync;

   send_packet(&message, sizeof(nm_clientrequest_t));
}

void CL_SendVoteRequest(const char *command)
{
   char *buf;
   nm_voterequest_t *message;
   size_t command_size = strlen(command);
   size_t message_size = sizeof(nm_voterequest_t);
   size_t total_size = message_size + command_size + 1;

   buf = ecalloc(char *, 1, total_size);
   message = (nm_voterequest_t *)buf;
   message->message_type = nm_voterequest;
   message->world_index = cl_latest_world_index;
   message->command_size = command_size;
   memcpy(buf + message_size, command, command_size);

   send_packet(buf, total_size);

   efree(message);
}

void CL_VoteYea()
{
   send_message(mr_vote, 0, "yea");
}

void CL_VoteNay()
{
   send_message(mr_vote, 0, "nay");
}

void CL_SendAuthMessage(const char *password)
{
   size_t password_length = strlen(password);

   // [CG] We want to remember what password we sent so that if the server
   //      responds with "authorization successful" we can store the password
   //      for this server's URL as valid.

   cs_server_password = erealloc(
      char *, cs_server_password, (password_length + 1) * sizeof(char)
   );

   printf("Authenticating...\n");

   memset(cs_server_password, 0, password_length + 1);
   strncpy(cs_server_password, password, password_length);

   send_message(mr_auth, 0, cs_server_password);
}

void CL_SendRCONMessage(const char *command)
{
   send_message(mr_rcon, 0, command);
}

void CL_SendServerMessage(const char *message)
{
   send_message(mr_server, 0, message);
}

void CL_SendPlayerMessage(int playernum, const char *message)
{
   send_message(mr_player, (unsigned int)playernum, message);
}

void CL_SendTeamMessage(const char *message)
{
   send_message(mr_team, 0, message);
}

void CL_BroadcastMessage(const char *message)
{
   send_message(mr_all, 0, message);
}

void CL_Spectate(void)
{
   nm_playerinfoupdated_t update_message;

   update_message.message_type = nm_playerinfoupdated;
   update_message.player_number = consoleplayer;
   update_message.info_type = ci_spectating;
   update_message.array_index = 0;
   update_message.boolean_value = true;

   send_packet(&update_message, sizeof(nm_playerinfoupdated_t));
}

void CL_HandleInitialStateMessage(nm_initialstate_t *message)
{
   Mobj *mo;
   Thinker *th;
   unsigned int i;
   unsigned int new_num = message->player_number;
   unsigned int mi = message->map_index;

   doom_printf("Received initial state");

   // [CG] consoleplayer and displayplayer will both be zero at this point, so
   //      copy clients[0] and players[0] to "new_num" in their respective
   //      arrays.
   if(message->player_number != 0)
   {
      CS_InitPlayer(new_num);
      CS_ZeroClient(new_num);
      CS_SetPlayerName(&players[new_num], default_name);
      CS_SetPlayerName(&players[0], SERVER_NAME);
      consoleplayer = displayplayer = message->player_number;
      CS_SetClientTeam(consoleplayer, team_color_none);
      CL_SendPlayerStringInfo(ci_name);
      CL_SendPlayerScalarInfo(ci_team);
      for(i = 0; i < NUMWEAPONS; i++)
         CL_SendPlayerArrayInfo(ci_pwo, i);
      CL_SendPlayerScalarInfo(ci_wsop);
      CL_SendPlayerScalarInfo(ci_asop);
      CL_SendPlayerScalarInfo(ci_bobbing);
      CL_SendPlayerScalarInfo(ci_weapon_toggle);
      CL_SendPlayerScalarInfo(ci_autoaim);
      CL_SendPlayerScalarInfo(ci_weapon_speed);
      CL_SendPlayerScalarInfo(ci_buffering);
   }

   if(mi > cs_map_count)
      I_Error("CL_HandleInitialStateMessage: Invalid map number %d.\n", mi);

   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      if(!th)
         break;

      if((mo = thinker_cast<Mobj *>(th)))
         CL_RemoveMobj(mo);
   }

   cs_current_map_index = message->map_index;
   rngseed = message->rngseed;
   CS_ReloadDefaults();
   memcpy(cs_settings, &message->settings, sizeof(clientserver_settings_t));
   CS_ApplyConfigSettings();
   CS_LoadMap();

   CL_SendCurrentStateRequest();
}

void CL_HandleCurrentStateMessage(nm_currentstate_t *message)
{
   if(!M_WriteFile(cs_state_file_path,
                   ((char *)message)+ sizeof(nm_currentstate_t),
                   (size_t)message->state_size))
   {
      C_Printf("Error saving game state to disk, disconnecting.\n");
      C_Printf("Error: %s.\n", M_GetFileSystemErrorMessage());
      CL_Disconnect();
      return;
   }

   doom_printf("Received current game state");

   CL_LoadGame(cs_state_file_path);

   // [CG] Have to be "in-game" at this point.  This should be sent over
   //      in the game state message, so error out if not.
   if(!playeringame[consoleplayer])
   {
      C_Printf(
         "This player was not in game according to game state message, "
         "disconnecting.\n"
      );
      CL_Disconnect();
      return;
   }

   if(clients[consoleplayer].team != team_color_none)
      CL_SendPlayerScalarInfo(ci_team);

   CL_SendSyncRequest();
}

void CL_HandleSyncMessage(nm_sync_t *message)
{
   gametic            = message->gametic;
   basetic            = message->basetic;
   leveltime          = message->leveltime;
   levelstarttic      = message->levelstarttic;
   rng.seed[pr_plats] = message->plat_seed;
   rng.platrndindex   = message->platrndindex;

   cl_current_world_index = cl_latest_world_index = message->world_index;

   cl_packet_buffer.setSynchronized(true);
   cl_packet_buffer.setCapacity(cl_packet_buffer_size);
   if(cl_packet_buffer_enabled)
      cl_packet_buffer.enable();

   CS_UpdateQueueMessage();
   if(cl_commands_sent)
      CL_StoreLatestSectorStatusIndex(cl_commands_sent - 1);
   else
      CL_StoreLatestSectorStatusIndex(0);

   doom_printf("Synchronized!");

   if(!cl_received_first_sync)
      cl_received_first_sync = true;
   else if(GameType != gt_coop)
      CS_Announce(ae_round_started, NULL);
}

void CL_HandleMapCompletedMessage(nm_mapcompleted_t *message)
{
   if(message->new_map_index > cs_map_count)
   {
      doom_printf(
         "Received map completed message containing invalid new map lump "
         "number %u, ignoring.\n",
         message->new_map_index
      );
   }

   cs_current_map_index = message->new_map_index;

   if(message->enter_intermission)
      G_DoCompleted(true);
   else
      G_DoCompleted(false);

   gameaction = ga_worlddone;

   cl_packet_buffer.disable();
}

void CL_HandleMapStartedMessage(nm_mapstarted_t *message)
{
   cl_commands_sent = 0;
   cl_latest_world_index = cl_current_world_index = 0;
   CL_InitPrediction();
   cl_packet_buffer.setSynchronized(false);
   CS_ReloadDefaults();
   memcpy(cs_settings, &message->settings, sizeof(clientserver_settings_t));
   CS_ApplyConfigSettings();
   CS_DoWorldDone();

   if(CS_DEMORECORD)
   {
      // [CG] Add a demo for the new map, then write the map started message to
      //      it.
      if(!cs_demo->addNewMap())
      {
         doom_printf(
            "Error adding new map to demo: %s.", cs_demo->getError()
         );
         if(!cs_demo->stop())
            doom_printf("Error stopping demo: %s.", cs_demo->getError());
      }
   }

   CL_SendCurrentStateRequest();
}

void CL_HandleAuthResultMessage(nm_authresult_t *message)
{
   if(message->authorization_successful)
   {
      if(!CS_DEMOPLAY)
         CL_SaveServerPassword();

      doom_printf("Authorization succeeded.");
   }

   switch(message->authorization_level)
   {
   case cs_auth_spectator:
      doom_printf("Authorization level: spectator.");
      break;
   case cs_auth_player:
      doom_printf("Authorization level: player.");
      break;
   case cs_auth_moderator:
      doom_printf("Authorization level: moderator.");
      break;
   case cs_auth_administrator:
      doom_printf("Authorization level: administrator.");
      break;
   case cs_auth_none:
   default:
      I_Error(
         "Received invalid authorization level %d from server, exiting.\n",
         message->authorization_level
      );
      break;
   }

}

void CL_HandleClientInitMessage(nm_clientinit_t *message)
{
   int i = message->client_number;

   if(i >= MAX_CLIENTS || i >= MAXPLAYERS)
   {
     doom_printf("Received a client init message for invalid client %u.\n", i);
     return;
   }

   CS_ZeroClient(i);
   memcpy(&clients[i], &message->client, sizeof(client_t));
   HU_FragsUpdate();
}

void CL_HandleClientStatusMessage(nm_clientstatus_t *message)
{
   client_t *client;
   int playernum = message->client_number;

   if(playernum > MAX_CLIENTS || !playeringame[playernum])
      return;

   client = &clients[playernum];

   client->stats.client_lag = message->client_lag;
   client->stats.server_lag = message->server_lag;
   client->stats.transit_lag = message->transit_lag;
   if((!CS_DEMOPLAY) && (playernum == consoleplayer))
   {
      client->stats.packet_loss = (uint8_t)(
         (net_peer->packetLoss / (float)ENET_PEER_PACKET_LOSS_SCALE) * 100
      );

      if(client->stats.packet_loss > 100)
         client->stats.packet_loss = 100;
   }
   else
      client->stats.packet_loss = message->packet_loss;
}

void CL_HandlePlayerPositionMessage(nm_playerposition_t *message)
{
   int playernum = message->player_number;
   client_t *client = &clients[playernum];
   player_t *player = &players[playernum];

   if(playernum > MAX_CLIENTS || !playeringame[playernum])
      return;

   // [CG] Out-of-order position message.
   if(message->world_index <= client->latest_position_index)
      return;

   client->latest_position_index = message->world_index;

   if(playernum == consoleplayer)
   {
      CL_StoreLastServerPosition(message);
      return;
   }

   if((!client->spectating) && (player->mo))
   {
      CS_SetPlayerPosition(playernum, &message->position);
      client->floor_status = message->floor_status;
   }
}

void CL_HandlePlayerSpawnedMessage(nm_playerspawned_t *message)
{
   player_t *player;
   client_t *client;
   bool was_spectating, as_spectator;

   if(message->player_number > MAX_CLIENTS)
   {
      doom_printf("Received invalid player spawned message, ignoring.");
      return;
   }

   player = &players[message->player_number];
   client = &clients[message->player_number];
   was_spectating = client->spectating;

   // [CG] Clear the spree-related variables for this client.
   client->frags_this_life = 0;
   client->last_frag_index = 0;
   client->frag_level = fl_none;
   client->consecutive_frag_level = cfl_none;

   playeringame[message->player_number] = true;
   player->playerstate = PST_REBORN;

   if(message->as_spectator)
      as_spectator = true;
   else
      as_spectator = false;

   CL_SpawnPlayer(
      message->player_number,
      message->net_id,
      message->x << FRACBITS,
      message->y << FRACBITS,
      message->z,
      message->angle,
      as_spectator
   );

   if(message->player_number == consoleplayer)
   {
      CS_UpdateQueueMessage();

      if(was_spectating && !as_spectator)
      {
         if(CS_TEAMS_ENABLED)
         {
            int other_team = CS_GetOtherTeam(client->team);

            if(team_scores[client->team] > team_scores[other_team])
               CS_Announce(ae_lead_gained, NULL);
            else if(team_scores[client->team] == team_scores[other_team])
               CS_Announce(ae_lead_tied, NULL);
            else if(team_scores[client->team] < team_scores[other_team])
               CS_Announce(ae_lead_lost, NULL);
         }
         else
         {
            int i;
            bool tie = false;

            for(i = 1; i < MAX_CLIENTS; i++)
            {
               if(i == consoleplayer)
                  continue;

               if(players[i].totalfrags > player->totalfrags)
               {
                  CS_Announce(ae_lead_lost, NULL);
                  return;
               }

               if(players[i].totalfrags == player->totalfrags)
                  tie = true;
            }

            if(tie)
               CS_Announce(ae_lead_tied, NULL);
            else
               CS_Announce(ae_lead_gained, NULL);
         }
      }
   }
}

void CL_HandlePlayerWeaponStateMessage(nm_playerweaponstate_t *message)
{
   if(message->player_number > MAXPLAYERS || message->player_number == 0)
   {
      doom_printf(
         "Received player weapon state message for invalid player "
         "%d, ignoring.\n",
         message->player_number
      );
      return;
   }

   // [CG] During spawning, a player's weapon sprites will be initialized and
   //      this process sends a message serverside.  The client will receive
   //      the message before they receive nm_playerspawned, so we have to
   //      silently ignore weapon state messages for players who aren't ingame.
   if(!playeringame[message->player_number])
      return;

   if(message->player_number != consoleplayer)
   {
      cl_setting_player_weapon_sprites = true;
      P_SetPsprite(
         &players[message->player_number],
         message->psprite_position,
         message->weapon_state
      );
      cl_setting_player_weapon_sprites = false;
   }
}

void CL_HandlePlayerRemovedMessage(nm_playerremoved_t *message)
{
   if(message->player_number > MAXPLAYERS ||
      !playeringame[message->player_number])
   {
     doom_printf(
       "Received player_removed message for invalid player %d, ignoring.\n",
       message->player_number
     );
     return;
   }

   if(message->reason == dr_no_reason || message->reason > dr_max_reasons)
   {
      doom_printf(
         "%s disconnected.",
         players[message->player_number].name
      );
   }
   else
   {
      doom_printf(
         "%s disconnected: %s.",
         players[message->player_number].name,
         disconnection_strings[message->reason]
      );
   }
   CL_RemovePlayer(message->player_number);
}

void CL_HandlePlayerTouchedSpecialMessage(nm_playertouchedspecial_t *message)
{
   Mobj *mo;

   mo = NetActors.lookup(message->thing_net_id);

   if(mo == NULL)
   {
     doom_printf(
       "Received touch special message for non-existent thing %u, ignoring.\n",
       message->thing_net_id
     );
     return;
   }

   if(message->player_number > MAXPLAYERS ||
      !playeringame[message->player_number] ||
      clients[message->player_number].spectating)
   {
     doom_printf(
       "Received touch special message for invalid player %d, ignoring.\n",
       message->player_number
     );
     return;
   }

   P_TouchSpecialThing(mo, players[message->player_number].mo);

}

void CL_HandleServerMessage(nm_servermessage_t *message)
{
   char *server_message = CL_extractServerMessage(message);

   if(server_message == NULL)
   {
      doom_printf("Received invalid server message, ignoring.\n");
      return;
   }

   if(message->prepend_name)
   {
      S_StartSound(NULL, GameModeInfo->c_ChatSound);
      doom_printf("%s: %s", SERVER_NAME, server_message);
   }
   else if(message->is_hud_message)
      HU_CenterMessage(server_message);
   else
      doom_printf("%s", server_message);
}

void CL_HandlePlayerMessage(nm_playermessage_t *message)
{
   player_t *player;
   char *player_message = CL_extractPlayerMessage(message);

   if(player_message == NULL)
   {
      doom_printf("Received invalid player message, ignoring.\n");
      return;
   }

   player = &players[message->sender_number];

   if(message->recipient_type == mr_vote)
   {
      if(!strncasecmp(player_message, "yea", 3))
      {
         if(CL_CastBallot(message->sender_number, Ballot::yea_vote))
            doom_printf("%s voted yes.", player->name);
      }
      else if(!strncasecmp(player_message, "nay", 3))
      {
         if(CL_CastBallot(message->sender_number, Ballot::nay_vote))
            doom_printf("%s voted no.", player->name);
      }
   }
   else if(message->sender_number != consoleplayer)
   {
      // [CG] Don't print our own messages.  When a client sends a message,
      //      it's automatically printed to the screen (because delivery is
      //      guaranteed) so printing it here would be redundant.
      S_StartSound(NULL, GameModeInfo->c_ChatSound);
      doom_printf("%s: %s", player->name, player_message);
   }
}

void CL_HandlePuffSpawnedMessage(nm_puffspawned_t *message)
{
   Mobj *puff, *shooter;
   bool ptcl;

   if((shooter = NetActors.lookup(message->shooter_net_id)) == NULL)
   {
      doom_printf(
         "Received spawn puff message for invalid shooter %u, ignoring.\n",
         message->shooter_net_id
      );
      return;
   }

   if(message->ptcl)
      ptcl = true;
   else
      ptcl = false;

   puff = CL_SpawnPuff(
      message->puff_net_id,
      message->x,
      message->y,
      message->z,
      message->angle,
      message->updown,
      ptcl
   );

   if(CL_SHOULD_PREDICT_SHOT(shooter))
      puff->flags2 |= MF2_DONTDRAW;
}

void CL_HandleBloodSpawnedMessage(nm_bloodspawned_t *message)
{
   Mobj *blood, *shooter, *target;

   if((shooter = NetActors.lookup(message->shooter_net_id)) == NULL)
   {
      doom_printf(
         "Received spawn blood message for invalid shooter %u, ignoring.\n",
         message->shooter_net_id
      );
      return;
   }

   if((target = NetActors.lookup(message->target_net_id)) == NULL)
   {
      doom_printf(
         "Received spawn blood message for invalid target %u, ignoring.\n",
         message->target_net_id
      );
      return;
   }

   blood = CL_SpawnBlood(
      message->blood_net_id,
      message->x,
      message->y,
      message->z,
      message->angle,
      message->damage,
      target
   );

   if(CL_SHOULD_PREDICT_SHOT(shooter))
      blood->flags2 |= MF2_DONTDRAW;

}

void CL_HandleActorSpawnedMessage(nm_actorspawned_t *message)
{
   Mobj *actor;
   int safe_item_fog_type          = E_SafeThingType(MT_IFOG);
   int safe_tele_fog_type          = E_SafeThingType(GameModeInfo->teleFogType);
   int safe_bfg_type               = E_SafeThingType(MT_BFG);
   int safe_spawn_fire_type        = E_SafeThingType(MT_SPAWNFIRE);
   int safe_sorcerer_teleport_type = E_SafeThingType(MT_SOR2TELEFADE);
   int safe_red_flag_type          = E_SafeThingName("RedFlag");
   int safe_blue_flag_type         = E_SafeThingName("BlueFlag");
   int safe_dropped_red_flag_type  = E_SafeThingName("DroppedRedFlag");
   int safe_dropped_blue_flag_type = E_SafeThingName("DroppedBlueFlag");
   int safe_carried_red_flag_type  = E_SafeThingName("CarriedRedFlag");
   int safe_carried_blue_flag_type = E_SafeThingName("CarriedBlueFlag");

   actor = CL_SpawnMobj(
      message->net_id, message->x, message->y, message->z, message->type
   );

   actor->momx = message->momx;
   actor->momy = message->momy;
   actor->momz = message->momz;
   actor->angle = message->angle;
   actor->flags = message->flags;

   // [CG] Some actors only make sounds when they spawn, but because spawning
   //      is handled serverside and therefore clients don't get the initial
   //      opportunity, it must be done here.  Probably this will grow large
   //      enough to require a hash.

   if(message->type == safe_item_fog_type)
      S_StartSound(actor, sfx_itmbk);
   else if(message->type == safe_tele_fog_type)
      S_StartSound(actor, GameModeInfo->teleSound);
   else if(message->type == safe_bfg_type && bfgtype == bfg_bouncing)
      S_StartSound(actor, actor->info->seesound);
   else if(message->type == safe_spawn_fire_type)
      S_StartSound(actor, sfx_telept);
   else if(message->type == safe_sorcerer_teleport_type)
       S_StartSound(actor, sfx_htelept);
   else if(message->type == safe_red_flag_type          ||
           message->type == safe_blue_flag_type         ||
           message->type == safe_dropped_red_flag_type  ||
           message->type == safe_dropped_blue_flag_type ||
           message->type == safe_carried_red_flag_type  ||
           message->type == safe_carried_blue_flag_type)
   {
      int color = team_color_none;

      if(message->type == safe_red_flag_type         ||
         message->type == safe_dropped_red_flag_type ||
         message->type == safe_carried_red_flag_type)
      {
         color = team_color_red;
      }
      else if(message->type == safe_blue_flag_type         ||
              message->type == safe_dropped_blue_flag_type ||
              message->type == safe_carried_blue_flag_type)
      {
         color = team_color_blue;
      }

      cs_flags[color].net_id = actor->net_id;
   }
}

void CL_HandleActorPositionMessage(nm_actorposition_t *message)
{
   Mobj *actor;

   actor = NetActors.lookup(message->actor_net_id);

   if(actor == NULL)
   {
      doom_printf(
         "Received position update for invalid actor %u, ignoring.",
         message->actor_net_id
      );
      return;
   }

   CS_SetActorPosition(actor, &message->position);
}

void CL_HandleActorMiscStateMessage(nm_actormiscstate_t *message)
{
   Mobj *actor;

   actor = NetActors.lookup(message->actor_net_id);

   if(actor == NULL)
   {
      doom_printf(
         "Received misc state update for invalid actor %u, ignoring.",
         message->actor_net_id
      );
      return;
   }

   CS_SetActorMiscState(actor, &message->misc_state);
}

void CL_HandleActorTargetMessage(nm_actortarget_t *message)
{
   Mobj *actor;
   Mobj *target = NULL;

   actor = NetActors.lookup(message->actor_net_id);

   if(actor == NULL)
   {
      doom_printf(
         "Received actor target message for invalid actor %u, ignoring.\n",
         message->actor_net_id
      );
      return;
   }

   if(message->target_net_id != 0)
   {
      target = NetActors.lookup(message->target_net_id);
      if(target == NULL)
      {
         doom_printf(
            "Received actor target message for invalid target %u, ignoring.\n",
            message->target_net_id
         );
         return;
      }
   }

   switch(message->target_type)
   {
   case CS_AT_TARGET:
      P_SetTarget(&actor->target, target);
      break;
   case CS_AT_TRACER:
      P_SetTarget(&actor->tracer, target);
      break;
   case CS_AT_LASTENEMY:
      P_SetTarget(&actor->lastenemy, target);
      break;
   default:
      I_Error(
         "SV_BroadcastActorTarget: Invalid target type %d.\n",
         message->target_type
      );
      break;
   }
}

void CL_HandleActorStateMessage(nm_actorstate_t *message)
{
   Mobj *actor = NetActors.lookup(message->actor_net_id);

   if(actor == NULL)
   {
      doom_printf(
         "Received actor state message for invalid "
         "actor %u, type %d, state %d, ignoring.",
         message->actor_net_id,
         message->actor_type,
         message->state_number
      );
      return;
   }

   // [CG] Don't accept state messages for ourselves because we're predicting.
   if(actor != players[consoleplayer].mo)
      CL_SetMobjState(actor, message->state_number);
}

void CL_HandleActorDamagedMessage(nm_actordamaged_t *message)
{
   emod_t *emod;
   player_t *player;
   Mobj *target;
   Mobj *inflictor = NULL;
   Mobj *source = NULL;

   emod = E_DamageTypeForNum(message->mod);
   target = NetActors.lookup(message->target_net_id);

   if(target == NULL)
   {
      doom_printf(
         "Received actor damaged message for invalid target %u.",
         message->target_net_id
      );
      return;
   }

   if(message->source_net_id != 0)
   {
      source = NetActors.lookup(message->source_net_id);
      if(source == NULL)
      {
         doom_printf(
            "Received actor damaged message for invalid target %u.",
            message->target_net_id
         );
         return;
      }
   }

   if(message->inflictor_net_id != 0)
   {
      inflictor = NetActors.lookup(message->inflictor_net_id);
      if(inflictor == NULL)
      {
         doom_printf(
            "Received actor damaged message for invalid inflictor %u.",
            message->inflictor_net_id
         );
         return;
      }
   }

#if _UNLAG_DEBUG
   if(cl_debug_unlagged)
   {
      cs_player_position_t *last_server_position = CL_GetLastServerPosition();

      CL_SpawnRemoteGhost(
         message->target_net_id,
         message->x,
         message->y,
         message->z,
         message->angle,
         message->world_index
      );
      CL_SpawnRemoteGhost(
         players[displayplayer].mo->net_id,
         last_server_position->x,
         last_server_position->y,
         last_server_position->z,
         last_server_position->angle,
         last_server_position->world_index
      );
   }
#endif

   current_game_type->handleActorDamaged(
      target, inflictor, source, message->health_damage, message->mod
   );

   // a dormant thing being destroyed gets restored to normal first
   if(target->flags2 & MF2_DORMANT)
   {
      target->flags2 &= ~MF2_DORMANT;
      target->tics = 1;
   }

   if(target->flags & MF_SKULLFLY)
      target->momx = target->momy = target->momz = 0;

   if(target->player)
   {
      player = target->player;
      player->armorpoints -= message->armor_damage;

      if(player->armorpoints <= 0)
      {
         player->armortype = 0;
         player->hereticarmor = false;
      }

      player->health -= message->health_damage;

      if(player->health < 0)
         player->health = 0;

      player->attacker = source;
      player->damagecount += message->health_damage;

      if(player->damagecount > 100)
         player->damagecount = 100; // teleport stomp does 10k points...

      // haleyjd 10/14/09: we do not allow negative damagecount
      if(player->damagecount < 0)
         player->damagecount = 0;

   }
   else
      player = NULL;

   target->health -= message->health_damage;

   if(!message->damage_was_fatal)
   {
      cl_handling_damaged_actor = true;
      if(message->just_hit)
         cl_handling_damaged_actor_and_justhit = true;
      else
         cl_handling_damaged_actor_and_justhit = false;
      P_HandleDamagedActor(
         target, source, message->health_damage, message->mod, emod
      );
      cl_handling_damaged_actor = false;
      cl_handling_damaged_actor_and_justhit = false;
   }
}

void CL_HandleActorKilledMessage(nm_actorkilled_t *message)
{
   emod_t *emod;
   Mobj *target, *inflictor, *source;

   target = NetActors.lookup(message->target_net_id);
   if(target == NULL)
   {
      doom_printf(
         "Received actor killed message for invalid target %u.",
         message->target_net_id
      );
      return;
   }

   if(message->inflictor_net_id != 0)
   {
      inflictor = NetActors.lookup(message->inflictor_net_id);
      if(inflictor == NULL)
      {
         doom_printf(
            "Received actor killed message for invalid inflictor %u.",
            message->inflictor_net_id
         );
         return;
      }
   }
   else
      inflictor = NULL;

   if(message->source_net_id != 0)
   {
      source = NetActors.lookup(message->source_net_id);
      if(source == NULL)
      {
         doom_printf(
            "Received actor killed message for invalid source %u.",
            message->source_net_id
         );
         return;
      }
   }
   else
      source = NULL;

   // [CG] TODO: Ensure this is valid.
   emod = E_DamageTypeForNum(message->mod);

   if(target->player)
      P_DeathMessage(source, target, inflictor, emod);

   if(message->damage <= 10)
      target->intflags |= MIF_WIMPYDEATH;

   P_KillMobj(source, target, emod);
}

void CL_HandleActorRemovedMessage(nm_actorremoved_t *message)
{
   Mobj *actor;

   if((actor = NetActors.lookup(message->actor_net_id)) == NULL)
   {
      doom_printf(
         "Received an actor removed message for an invalid actor %u.",
         message->actor_net_id
      );
      return;
   }

   CL_RemoveMobj(actor);
}

void CL_HandleLineActivatedMessage(nm_lineactivated_t *message)
{
   Mobj *actor;
   line_t *line;

   actor = NetActors.lookup(message->actor_net_id);
   if(actor == NULL)
   {
      doom_printf(
         "Received an activate line message for an invalid actor %u.",
         message->actor_net_id
      );
      return;
   }

   if(message->line_number > (numlines - 1))
   {
      doom_printf(
         "Received an activate line message for an invalid line %u.",
         message->line_number
      );
      return;
   }
   line = lines + message->line_number;

   if(message->side > 1)
   {
      doom_printf(
         "Received an activate line message for an invalid side %u.",
         message->side
      );
      return;
   }

   if((actor->player) && ((actor->player - players) == consoleplayer))
   {
      CL_HandleLineActivation(message->line_number, message->command_index);
      return;
   }

   cl_activating_line_from_message = true;
   if(message->activation_type == at_crossed)
      P_CrossSpecialLine(line, message->side, actor);
   else if(message->activation_type == at_shot)
      P_ShootSpecialLine(actor, line, message->side);
   else if(message->activation_type == at_used)
   {
      if(P_UseSpecialLine(actor, line, message->side))
      {
         current_game_type->handleActorUsedSpecialLine(
            actor, line, message->side
         );
      }
   }
   else
   {
      doom_printf(
         "Received a line activated message with an invalid activation "
         "type.\n"
      );
   }
   cl_activating_line_from_message = false;
}

void CL_HandleMonsterActiveMessage(nm_monsteractive_t *message)
{
   Mobj *monster;

   monster = NetActors.lookup(message->monster_net_id);
   if(monster == NULL)
   {
      doom_printf(
         "Received a monster active message for an invalid monster %u.",
         message->monster_net_id
      );
      return;
   }

   P_MakeActiveSound(monster);
}

void CL_HandleMonsterAwakenedMessage(nm_monsterawakened_t *message)
{
   int sound;
   Mobj *monster = NetActors.lookup(message->monster_net_id);

   if(monster == NULL)
   {
      doom_printf(
         "Received monster awakened message for invalid actor %u, ignoring.",
         message->monster_net_id
      );
      return;
   }

   // EDF_FIXME: needs to be generalized like in zdoom
   switch (monster->info->seesound)
   {
      case sfx_posit1:
      case sfx_posit2:
      case sfx_posit3:
         sound = sfx_posit1 + P_Random(pr_see) % 3;
         break;
      case sfx_bgsit1:
      case sfx_bgsit2:
         sound = sfx_bgsit1 + P_Random(pr_see) % 2;
         break;
      default:
         sound = monster->info->seesound;
         break;
   }
   // haleyjd: generalize to all bosses
   if(monster->flags2 & MF2_BOSS)
      S_StartSound(NULL, sound);        // full volume
   else
      S_StartSound(monster, sound);
}

void CL_HandleMissileSpawnedMessage(nm_missilespawned_t *message)
{
   int player_number;
   player_t *player;
   Mobj *source, *missile;

   source = NetActors.lookup(message->source_net_id);
   if(source == NULL)
   {
      doom_printf(
         "Received a missile spawned message for an invalid actor %u.",
         message->source_net_id
      );
      return;
   }

   if(source->player)
   {
      player = source->player;
      player_number = player - players;
      if(!playeringame[player_number] || clients[player_number].spectating)
      {
         doom_printf(
            "Received spawn missile message for invalid player %d.",
            player_number
         );
         return;
      }
   }

   missile = CL_SpawnMobj(
      message->net_id, message->x, message->y, message->z, message->type
   );

   P_SetTarget(&missile->target, source);

   missile->momx = message->momx;
   missile->momy = message->momy;
   missile->momz = message->momz;
   missile->angle = message->angle;

   if(source->player && source->player->powers[pw_silencer] &&
      P_GetReadyWeapon(source->player)->flags & WPF_SILENCER)
   {
      S_StartSoundAtVolume(
         missile,
         missile->info->seesound,
         WEAPON_VOLUME_SILENCED,
         ATTN_NORMAL,
         CHAN_AUTO
      );
   }
   else
      S_StartSound(missile, missile->info->seesound);

   // [CG] This normally uses #define BFGBOUNCE 16, but that's in p_pspr.c and
   //      consequently inaccessible.
   if(message->type == MT_BFG)
      missile->extradata.bfgcount = 16;
   else if(message->type == E_SafeThingType(MT_MNTRFX2))
      S_StartSound(missile, sfx_minat1);
}

void CL_HandleMissileExplodedMessage(nm_missileexploded_t *message)
{
   Mobj *missile;

   missile = NetActors.lookup(message->missile_net_id);
   if(missile == NULL)
   {
      doom_printf(
         "Received an explode missile message for an invalid missile %u.",
         message->missile_net_id
      );
      return;
   }

   missile->momx = missile->momy = missile->momz = 0;

   P_SetMobjState(missile, missile->info->deathstate);
   missile->tics = message->tics;
   missile->flags &= ~MF_MISSILE;
   S_StartSound(missile, missile->info->deathsound);
   // haleyjd: disable any particle effects
   missile->effects = 0;
   missile->tics = message->tics;
}

void CL_HandleCubeSpawnedMessage(nm_cubespawned_t *message)
{
#if 0
   Mobj *cube, *target;

   cube = NetActors.lookup(message->net_id);
   if(cube == NULL)
   {
      doom_printf(
         "Received a cube spawned message for an invalid cube %u.\n",
         message->net_id
      );
      return;
   }

   target = NetActors.lookup(message->target_net_id);
   if(target == NULL)
   {
      doom_printf(
         "Received a cube spawned message with invalid target %u.\n",
         message->target_net_id
      );
      return;
   }

   P_SetTarget(&cube->target, target);
   P_UpdateThinker(&cube->thinker);
#endif
}

void CL_HandleSectorThinkerSpawnedMessage(nm_sectorthinkerspawned_t *message)
{
   sector_t *sector = NULL;
   line_t *line = NULL;

   if((message->sector_number >= (unsigned int)numsectors))
   {
      doom_printf(
         "Received a map special spawned message containing invalid sector "
         "%d, ignoring.",
         message->sector_number
      );
      return;
   }

   if(message->line_number >= 0)
   {
      if((message->line_number >= numlines))
      {
         doom_printf(
            "Received a map special spawned message containing invalid sector "
            "%d, ignoring.",
            message->sector_number
         );
         return;
      }
      line = &lines[message->line_number];
   }

   CS_LogSMT(
      "%u/%u: Sector thinker spawned (%u).\n",
      cl_latest_world_index,
      cl_current_world_index,
      message->command_index
   );

   sector = &sectors[message->sector_number];
   if(message->line_number >= 0)
      line = &lines[message->line_number];

   cl_spawning_sector_thinker_from_message = true;
   switch(message->type)
   {
      case st_platform:
         CL_SpawnPlatform(message);
         break;
      case st_door:
         CL_SpawnVerticalDoor(message);
         break;
      case st_ceiling:
         CL_SpawnCeiling(message);
         break;
      case st_floor:
         CL_SpawnFloor(message);
         break;
      case st_elevator:
         CL_SpawnElevator(message);
         break;
      case st_pillar:
         CL_SpawnPillar(message);
         break;
      case st_floorwaggle:
         CL_SpawnFloorWaggle(message);
         break;
      default:
         doom_printf(
            "CL_HandleSectorThinkerSpawnedMessage: Received a sector thinker "
            "spawned message with invalid type %d, ignoring.",
            message->type
         );
         break;
   }
   cl_spawning_sector_thinker_from_message = false;
   CL_StoreLatestSectorStatusIndex(message->command_index);
}

void CL_HandleSectorThinkerStatusMessage(nm_sectorthinkerstatus_t *message)
{
   CS_LogSMT(
      "%u/%u: Sector thinker status update (%u).\n",
      cl_latest_world_index,
      cl_current_world_index,
      message->command_index
   );

   switch(message->type)
   {
   case st_platform:
      CL_UpdatePlatform(message->command_index, &message->data);
      break;
   case st_door:
      CL_UpdateVerticalDoor(message->command_index, &message->data);
      break;
   case st_ceiling:
      CL_UpdateCeiling(message->command_index, &message->data);
      break;
   case st_floor:
      CL_UpdateFloor(message->command_index, &message->data);
      break;
   case st_elevator:
      CL_UpdateElevator(message->command_index, &message->data);
      break;
   case st_pillar:
      CL_UpdatePillar(message->command_index, &message->data);
      break;
   case st_floorwaggle:
      CL_UpdateFloorWaggle(message->command_index, &message->data);
      break;
   default:
      doom_printf(
         "CL_HandleSectorThinkerStatusMessage: Received a sector thinker "
         "status message with invalid type %u, ignoring.",
         message->type
      );
      return;
   }

   CL_StoreLatestSectorStatusIndex(message->command_index);
}

void CL_HandleSectorThinkerRemovedMessage(nm_sectorthinkerremoved_t *message)
{
   SectorThinker *thinker  = NULL;
   
   if(!(thinker = NetSectorThinkers.lookup(message->net_id)))
   {
      doom_printf(
         "CL_HandleSectorThinkerRemovedMessage: Received a sector thinker "
         "removed message for invalid thinker %u, ignoring.",
         message->net_id
      );
      return;
   }

   CS_LogSMT(
      "%u/%u: SMT %u removed at %u/%u (%u).\n",
      cl_latest_world_index,
      cl_current_world_index,
      message->net_id,
      message->world_index,
      message->command_index,
      thinker->net_id
   );

   thinker->removed = message->command_index;
}

void CL_HandleSectorPositionMessage(nm_sectorposition_t *message)
{
   int32_t sector_number = message->sector_number;

   if(sector_number >= numsectors)
   {
      doom_printf(
         "Received a map special spawned message containing invalid sector "
         "%d, ignoring.",
         message->sector_number
      );
      return;
   }

   if(sector_number == _DEBUG_SECTOR)
   {
      CS_LogSMT(
         "%u/%u: SPM: %u/%u (%d/%d).\n",
         cl_latest_world_index,
         cl_current_world_index,
         message->world_index,
         message->command_index,
         message->sector_position.ceiling_height >> FRACBITS,
         message->sector_position.floor_height >> FRACBITS
      );
   }
}

void CL_HandleAnnouncerEventMessage(nm_announcerevent_t *message)
{
   Mobj *source;

   if(!CS_AnnouncerEnabled())
      return;

   // [CG] Ignore announcer events if we haven't received sync yet.
   if(!cl_packet_buffer.synchronized())
      return;

   if(message->source_net_id)
   {
      if(!(source = NetActors.lookup(message->source_net_id)))
      {
         doom_printf(
            "Received a announcer event message for an invalid source %u.\n",
            message->source_net_id
         );
         return;
      }
   }
   else
      source = NULL;

   CS_Announce(message->event_index, source);
}

void CL_HandleVoteMessage(nm_vote_t *message)
{
   char *command = ((char *)message) + sizeof(nm_vote_t);
   unsigned int duration  = message->duration;
   double threshold       = message->threshold;
   unsigned int max_votes = message->max_votes;

   CL_CloseVote();
   CL_NewVote(command, duration, threshold, max_votes);
}

void CL_HandleVoteResultMessage(nm_voteresult_t *message)
{
   if(message->passed)
      doom_printf("Vote passed!");
   else
      doom_printf("Vote failed!");

   CL_CloseVote();
}

void CL_HandleRNGSyncMessage(nm_rngsync_t *message)
{
   rng.seed[pr_plats] = message->plat_seed;
   rng.platrndindex = message->platrndindex;
}

