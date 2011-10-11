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

#include <stdlib.h>
#include <string.h>

#include "a_common.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "d_gi.h"
#include "doomstat.h"
#include "e_sound.h"
#include "e_things.h"
#include "g_game.h"
#include "hu_frags.h"
#include "hu_stuff.h"
#include "i_system.h"
#include "m_file.h"
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
#include "cs_wad.h"
#include "cl_buf.h"
#include "cl_cmd.h"
#include "cl_main.h"
#include "cl_pred.h"
#include "cl_spec.h"

extern unsigned int rngseed;
extern int s_announcer_type;

static void send_packet(void *data, size_t data_size)
{
   ENetPacket *packet;

   if(!net_peer)
      return;
   
   packet = enet_packet_create(data, data_size, ENET_PACKET_FLAG_RELIABLE);
   enet_peer_send(net_peer, SEQUENCED_CHANNEL, packet);
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

   buffer = (char *)(calloc(
      1, sizeof(nm_playermessage_t) + player_message.length
   ));

   memcpy(buffer, &player_message, sizeof(nm_playermessage_t));
   memcpy(
      buffer + sizeof(nm_playermessage_t),
      message,
      player_message.length - 1
   );

   if(recipient_type == mr_auth)
      doom_printf("Authorizing...");
   else if(recipient_type == mr_rcon)
      doom_printf("RCON: %s", message);
   else
   {
      S_StartSound(NULL, GameModeInfo->c_ChatSound);
      doom_printf("%s: %s", players[consoleplayer].name, message);
   }

   send_packet(buffer, sizeof(nm_playermessage_t) + player_message.length);
   free(buffer);
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
   nm_playercommand_t command_message;
   cs_cmd_t *command = CL_GetCurrentCommand();
   // static unsigned int commands_sent = 0;

   if(CS_DEMO)
      I_Error("Error: made a command during demo playback.\n");

   command_message.message_type = nm_playercommand;
   command->world_index = cl_current_world_index;

   if(consoleactive)
      memset(&command->ticcmd, 0, sizeof(ticcmd_t));
   else
      G_BuildTiccmd(&command->ticcmd);

   CS_CopyCommand(&command_message.command, command);

#if _CMD_DEBUG
   printf(
      "CL_SendCommand (%3u/%3u): Sending command %u: ",
      cl_current_world_index,
      cl_latest_world_index,
      command->world_index
   );
   CS_PrintTiccmd(&command->ticcmd);
#endif

   send_packet(&command_message, sizeof(nm_playercommand_t));

   // [CG] Save all sent commands in the demo if recording.
   if(cs_demo_recording)
   {
      if(!CS_WritePlayerCommandToDemo(command))
      {
         doom_printf(
            "Demo error, recording aborted: %s", CS_GetDemoErrorMessage()
         );
         if(!CS_StopDemo())
            doom_printf("Error stopping demo: %s.", CS_GetDemoErrorMessage());
      }
   }

#if _UNLAG_DEBUG
   if(command->ticcmd.buttons & BT_ATTACK)
   {
      printf(
         "CL_SendCommand (%3u/%3u): Sending command %u: ",
         cl_current_world_index,
         cl_latest_world_index,
         command->world_index
      );
      CS_PrintTiccmd(&command->ticcmd);
      if(consoleplayer == 1 && playeringame[2])
      {
         printf("CL_SendCommand: Position of 2: ");
         CS_PrintPlayerPosition(2, 0);
      }
      else if(consoleplayer == 2 && playeringame[1])
      {
         printf("CL_SendCommand: Position of 1: ");
         CS_PrintPlayerPosition(1, 0);
      }
   }
#endif
}

void CL_SendPlayerStringInfo(client_info_e info_type)
{
   nm_playerinfoupdated_t *update_message;
   size_t buffer_size = CS_BuildPlayerStringInfoPacket(
      &update_message, consoleplayer, info_type
   );

   send_packet(update_message, buffer_size);
   free(update_message);
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

void CL_SendTeamRequest(teamcolor_t team)
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

   message.message_type = nm_clientrequest;
   message.request_type = scr_current_state;

   send_packet(&message, sizeof(nm_clientrequest_t));
}

void CL_SendSyncRequest(void)
{
   nm_clientrequest_t message;

   message.message_type = nm_clientrequest;
   message.request_type = scr_sync;

   send_packet(&message, sizeof(nm_clientrequest_t));
}

void CL_SendAuthMessage(const char *password)
{
   size_t password_length = strlen(password);

   // [CG] We want to remember what password we sent so that if the server
   //      responds with "authorization successful" we can store the password
   //      for this server's URL as valid.

   if(cs_server_password != NULL)
      free(cs_server_password);

   cs_server_password = (char *)(calloc(password_length + 1, sizeof(char)));
   memcpy(cs_server_password, password, password_length);

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
   int mn = message->map_number;

   // [CG] consoleplayer and displayplayer will both be zero at this point, so
   //      copy clients[0] and players[0] to "new_num" in their respective
   //      arrays.
   if(message->player_number != 0)
   {
      CS_InitPlayer(new_num);
      CS_ZeroClient(new_num);
      CS_SetPlayerName(&players[new_num], players[consoleplayer].name);
      CS_SetPlayerName(&players[0], SERVER_NAME);
      consoleplayer = displayplayer = message->player_number;
      clients[consoleplayer].team = team_color_red;
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
   }

   if(mn > cs_map_count)
      I_Error("CL_HandleInitialStateMessage: Invalid map number %d.\n", mn);

   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      if(!th)
         break;

      if((mo = thinker_cast<Mobj *>(th)))
         CL_RemoveMobj(mo);
   }

   cs_current_map_number = message->map_number;
   rngseed = message->rngseed;
   CS_ReloadDefaults();
   memcpy(cs_settings, &message->settings, sizeof(clientserver_settings_t));
   CS_ApplyConfigSettings();
   CS_LoadMap();

   CL_SendCurrentStateRequest();
}

void CL_HandleCurrentStateMessage(nm_currentstate_t *message)
{
   unsigned int i;

   for(i = 0; i < MAXPLAYERS; i++)
      playeringame[i] = message->playeringame[i];

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

   memcpy(cs_flags, message->flags, sizeof(flag_t) * team_color_max);
   memcpy(team_scores, message->team_scores, sizeof(int32_t) * team_color_max);

   M_WriteFile(
      cs_state_file_path,
      ((char *)message)+ sizeof(nm_currentstate_t),
      message->state_size
   );

   cl_setting_sector_positions = true;
   P_LoadGame(cs_state_file_path);
   cl_setting_sector_positions = false;

   CL_SendSyncRequest();
   cl_flush_packet_buffer = true;
}

void CL_HandleSyncMessage(nm_sync_t *message)
{
   gametic          = message->gametic;
   basetic          = message->basetic;
   leveltime        = message->leveltime;
   levelstarttic    = message->levelstarttic;
   cl_current_world_index = cl_latest_world_index = message->world_index;

   cl_flush_packet_buffer = true;
   cl_received_sync = true;
   CS_UpdateQueueMessage();
}

void CL_HandleMapCompletedMessage(nm_mapcompleted_t *message)
{
   if(message->new_map_number > cs_map_count)
   {
      doom_printf(
         "Received map completed message containing invalid new map lump "
         "number %u, ignoring.\n",
         message->new_map_number
      );
   }

   cs_current_map_number = message->new_map_number;
   // G_SetGameMapName(cs_maps[message->new_map_number].name);
   G_DoCompleted(message->enter_intermission);
   cl_received_sync = false;
}

void CL_HandleMapStartedMessage(nm_mapstarted_t *message)
{
   unsigned int i;
   static unsigned int demo_map_number = 0;

   printf("Handling map started message.\n");

   if(cs_demo_playback)
   {
      if(demo_map_number == cs_current_demo_map)
      {
         if(!CS_LoadNextDemoMap())
         {
            doom_printf(
               "Error during demo playback: %s.\n", CS_GetDemoErrorMessage()
            );
            CS_StopDemo();
         }
         return;
      }
      else if(gamestate == GS_LEVEL)
         G_DoCompleted(false);
   }

   demo_map_number = cs_current_demo_map;

   CS_ReloadDefaults();
   memcpy(cs_settings, &message->settings, sizeof(clientserver_settings_t));
   CS_ApplyConfigSettings();

   /*
   if(cs_demo_recording)
   {
      // [CG] Add a demo for the new map, then write the map started message to
      //      it.
      if(!CS_AddNewMapToDemo() ||
         !CS_WriteNetworkMessageToDemo(message, sizeof(nm_mapstarted_t), 0))
      {
         doom_printf(
            "Demo error, recording aborted: %s\n", CS_GetDemoErrorMessage()
         );
         CS_StopDemo();
      }
   }
   */

   CS_DoWorldDone();

   CL_SendCurrentStateRequest();
}

void CL_HandleAuthResultMessage(nm_authresult_t *message)
{
   if(!message->authorization_successful)
   {
      doom_printf("Authorization failed.");
      return;
   }

   if(!cs_demo_playback)
      CL_SaveServerPassword();

   doom_printf("Authorization succeeded.");

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
   unsigned int i = message->client_number;

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
   fixed_t oldbob;
   fixed_t oldviewz;
   fixed_t oldviewheight;
   fixed_t olddeltaviewheight;
   client_t *client;
   unsigned int playernum = message->client_number;

#if _PRED_DEBUG || _CMD_DEBUG || _UNLAG_DEBUG
   printf(
      "CL_HandleClientStatusMessage (%3u/%3u): %3u/%3u/%3u.\n",
      cl_current_world_index,
      cl_latest_world_index,
      message->world_index,
      message->last_command_run,
      message->position.world_index
   );
   printf("  ");
   CS_PrintPlayerPosition(playernum, cl_current_world_index);
   printf("  ");
   CS_PrintPosition(&message->position);
#endif

   if(playernum > MAX_CLIENTS || !playeringame[playernum])
      return;

   client = &clients[playernum];

   if(playernum == consoleplayer && !cl_enable_prediction)
      client->client_lag = cl_current_world_index - message->world_index;

   if(message->world_index > message->last_command_run)
      client->client_lag = message->world_index - message->last_command_run;
   else
      client->client_lag = 0;

   client->server_lag = message->server_lag;
   client->transit_lag = message->transit_lag;
   if(playernum == consoleplayer)
   {
      client->packet_loss =
         (net_peer->packetLoss / (float)ENET_PEER_PACKET_LOSS_SCALE) * 100;
   }
   else
      client->packet_loss = message->packet_loss;

   if(client->packet_loss > 100)
      client->packet_loss = 100;

   if(client->spectating)
      return;

   if(playernum == consoleplayer && players[playernum].playerstate == PST_LIVE)
   {
      oldbob = players[playernum].bob;
      oldviewz = players[playernum].viewz;
      oldviewheight = players[playernum].viewheight;
      olddeltaviewheight = players[playernum].deltaviewheight;
   }

   CS_SetPlayerPosition(playernum, &message->position);
   clients[playernum].floor_status = message->floor_status;

   if(playernum != consoleplayer)
      return;

   // [CG] Re-predicts from the position just received from the server on.
   if(cl_enable_prediction && message->last_command_run > 0)
      CL_RePredict(message->last_command_run + 1, cl_current_world_index + 1);

   if(players[playernum].playerstate == PST_LIVE)
   {
      players[playernum].bob = oldbob;
      players[playernum].viewz = oldviewz;
      players[playernum].viewheight = oldviewheight;
      players[playernum].deltaviewheight = olddeltaviewheight;
   }
}

void CL_HandlePlayerSpawnedMessage(nm_playerspawned_t *message)
{
   player_t *player = &players[message->player_number];

   if(message->player_number > MAX_CLIENTS)
   {
      printf("Received invalid player spawned message, ignoring.");
      return;
   }

   playeringame[message->player_number] = true;
   player->playerstate = PST_REBORN;

   CL_SpawnPlayer(
      message->player_number,
      message->net_id,
      message->x << FRACBITS,
      message->y << FRACBITS,
      message->z,
      message->angle,
      message->as_spectator
   );

   if(message->player_number == consoleplayer)
      CS_UpdateQueueMessage();
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

   if(message->player_number != consoleplayer || !cl_enable_prediction)
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
   char *player_message = CL_extractPlayerMessage(message);

   if(player_message == NULL)
   {
      doom_printf("Received invalid player message, ignoring.\n");
      return;
   }

   if(message->sender_number != consoleplayer)
   {
      // [CG] Don't print our own messages.  When a client sends a message,
      //      it's automatically printed to the screen (because delivery is
      //      guaranteed) so printing it here would be redundant.
      S_StartSound(NULL, GameModeInfo->c_ChatSound);
      doom_printf(
         "%s: %s",
         players[message->sender_number].name,
         player_message
      );
   }
}

void CL_HandlePuffSpawnedMessage(nm_puffspawned_t *message)
{
   if(cl_predict_shots &&
      cl_enable_prediction &&
      message->shooter_net_id == players[consoleplayer].mo->net_id)
   {
      return;
   }

   CL_SpawnPuff(
      message->x,
      message->y,
      message->z,
      message->angle,
      message->updown,
      message->ptcl
   );
}

void CL_HandleBloodSpawnedMessage(nm_bloodspawned_t *message)
{
   Mobj *target;

   if(cl_predict_shots &&
      cl_enable_prediction &&
      message->shooter_net_id == players[consoleplayer].mo->net_id)
   {
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

   CL_SpawnBlood(
      message->x,
      message->y,
      message->z,
      message->angle,
      message->damage,
      target
   );
}

void CL_HandleActorSpawnedMessage(nm_actorspawned_t *message)
{
   Mobj *actor;
   int safe_item_fog_type          = E_SafeThingType(MT_IFOG);
   int safe_tele_fog_type          = GameModeInfo->teleFogType;
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
      teamcolor_t color;

      if(message->type == safe_red_flag_type           ||
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

      // [CG] Don't draw the flag if we're carrying it.  Prevents the flag from
      //      ever blocking the local player's view.
      if(cs_flags[color].carrier == consoleplayer)
         actor->flags2 |= MF2_DONTDRAW;
   }
}

void CL_HandleActorPositionMessage(nm_actorposition_t *message)
{
   Mobj *actor;

   actor = NetActors.lookup(message->actor_net_id);

   if(actor == NULL)
   {
      printf(
         "Received position update for invalid actor %u, ignoring.\n",
         message->actor_net_id
      );
      return;
   }

   CS_SetActorPosition(actor, &message->position);
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
      printf(
         "Received actor state message for invalid "
         "actor %u, type %d, state %d, ignoring.\n",
         message->actor_net_id,
         message->actor_type,
         message->state_number
      );
      return;
   }

   // [CG] Don't accept state messages for ourselves if we're predicting.
   if(!cl_enable_prediction || actor != players[consoleplayer].mo)
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
      printf(
         "Received actor damaged message for invalid target %u.\n",
         message->target_net_id
      );
      return;
   }

   if(message->source_net_id != 0)
   {
      source = NetActors.lookup(message->source_net_id);
      if(source == NULL)
      {
         printf(
            "Received actor damaged message for invalid target %u.\n",
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
         printf(
            "Received actor damaged message for invalid inflictor %u.\n",
            message->inflictor_net_id
         );
         return;
      }
   }

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
      cl_handling_damaged_actor_and_justhit = message->just_hit;
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
      printf(
         "Received actor killed message for invalid target %u.\n",
         message->target_net_id
      );
      return;
   }

   if(message->inflictor_net_id != 0)
   {
      inflictor = NetActors.lookup(message->inflictor_net_id);
      if(inflictor == NULL)
      {
         printf(
            "Received actor killed message for invalid inflictor %u.\n",
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
         printf(
            "Received actor killed message for invalid source %u.\n",
            message->source_net_id
         );
         return;
      }
   }
   else
      source = NULL;

   // printf("Received actor killed message\n");

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
      printf(
         "Received an actor removed message for an invalid actor %u.\n",
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
   position_t saved_position, line_position;

   actor = NetActors.lookup(message->actor_net_id);
   if(actor == NULL)
   {
      printf(
         "Received an activate line message for an invalid actor %u.\n",
         message->actor_net_id
      );
      return;
   }

   if(message->line_number > (numlines - 1))
   {
      printf(
         "Received an activate line message for an invalid line %u.\n",
         message->line_number
      );
      return;
   }
   line = lines + message->line_number;

   if(message->side > 1)
   {
      printf(
         "Received an activate line message for an invalid side %u.\n",
         message->side
      );
      return;
   }

   if(message->activation_type < 0 ||
      message->activation_type >= at_max)
   {
      printf(
         "Received an activate line message with an invalid activation type.\n"
      );
      return;
   }

   // [CG] Because some line activations depend on the position of the actor
   //      at the time of activation, set the actor's position appropriately,
   //      but save the old position so the actor can be returned there
   //      afterwards.  This is sort of like unlagged for line activations.
   CS_SaveActorPosition(&saved_position, actor, gametic);

   CS_CopyPosition(&line_position, &saved_position);
   line_position.x     = message->actor_x;
   line_position.y     = message->actor_y;
   line_position.z     = message->actor_z;
   line_position.angle = message->actor_angle;
   CS_SetActorPosition(actor, &line_position);

   if(message->activation_type == at_used)
      P_UseSpecialLine(actor, line, message->side);
   else if(message->activation_type == at_crossed)
      P_CrossSpecialLine(line, message->side, actor);
   else if(message->activation_type == at_shot)
      P_ShootSpecialLine(actor, line, message->side);

   CS_SetActorPosition(actor, &saved_position);
}

void CL_HandleMonsterActiveMessage(nm_monsteractive_t *message)
{
   Mobj *monster;

   monster = NetActors.lookup(message->monster_net_id);
   if(monster == NULL)
   {
      printf(
         "Received a monster active message for an invalid monster %u.\n",
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
      printf(
         "Received monster awakened message for invalid actor %u, ignoring.\n",
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
      printf(
         "Received a missile spawned message for an invalid actor %u.\n",
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
         printf(
            "Received spawn missile message for invalid player %d.\n",
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
      printf(
         "Received an explode missile message for an invalid missile %u.\n",
         message->missile_net_id
      );
      return;
   }

   missile->momx = missile->momy = missile->momz = 0;

   P_SetMobjState(missile, mobjinfo[missile->type].deathstate);
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

void CL_HandleMapSpecialSpawnedMessage(nm_specialspawned_t *message)
{
   line_t *line;
   sector_t *sector;
   void *blob = (void *)(((char *)message) + sizeof(nm_specialspawned_t));

   if(message->line_number >= numlines)
   {
      doom_printf(
         "Received a map special spawned message containing invalid line %d, "
         "ignoring.",
         message->line_number
      );
      return;
   }

   if(message->sector_number >= numsectors)
   {
      doom_printf(
         "Received a map special spawned message containing invalid sector "
         "%d, ignoring.",
         message->sector_number
      );
      return;
   }

   line = &lines[message->line_number];
   sector = &sectors[message->sector_number];

   switch(message->special_type)
   {
   case ms_ceiling_param:
      CL_SpawnParamCeilingFromBlob(line, sector, blob);
      break;
   case ms_door_param:
      CL_SpawnParamDoorFromBlob(line, sector, blob);
      break;
   case ms_floor_param:
      CL_SpawnParamFloorFromBlob(line, sector, blob);
      break;
   case ms_ceiling:
      CL_SpawnCeilingFromStatusBlob(line, sector, blob);
      break;
   case ms_door_tagged:
   case ms_door_manual:
   case ms_door_closein30:
   case ms_door_raisein300:
      CL_SpawnDoorFromStatusBlob(
         line, sector, blob, (map_special_e)message->special_type
      );
      break;
   case ms_floor:
   case ms_stairs:
   case ms_donut:
   case ms_donut_hole:
      CL_SpawnFloorFromStatusBlob(
         line, sector, blob, (map_special_e)message->special_type
      );
      break;
   case ms_elevator:
      CL_SpawnElevatorFromStatusBlob(line, sector, blob);
      break;
   case ms_pillar_build:
   case ms_pillar_open:
      CL_SpawnPillarFromStatusBlob(
         line, sector, blob, (map_special_e)message->special_type
      );
      break;
   case ms_floorwaggle:
      CL_SpawnFloorWaggleFromStatusBlob(line, sector, blob);
      break;
   case ms_platform:
      CL_SpawnPlatformFromStatusBlob(line, sector, blob);
      break;
   case ms_platform_gen:
      CL_SpawnGenPlatformFromStatusBlob(line, sector, blob);
      break;
   default:
      doom_printf(
         "CL_HandleMapSpecialSpawnedMessage: unknown special type %d, "
         "ignoring.\n",
         message->special_type
      );
      break;
   }
}

void CL_HandleMapSpecialStatusMessage(nm_specialstatus_t *message)
{
   void *blob = (void *)(((char *)message) + sizeof(nm_specialstatus_t));

   switch(message->special_type)
   {
   case ms_ceiling:
   case ms_ceiling_param:
      CL_ApplyCeilingStatusFromBlob(blob);
      break;
   case ms_door_tagged:
   case ms_door_manual:
   case ms_door_closein30:
   case ms_door_raisein300:
   case ms_door_param:
      CL_ApplyDoorStatusFromBlob(blob);
      break;
   case ms_floor:
   case ms_stairs:
   case ms_donut:
   case ms_donut_hole:
   case ms_floor_param:
      CL_ApplyFloorStatusFromBlob(blob);
      break;
   case ms_elevator:
      CL_ApplyElevatorStatusFromBlob(blob);
      break;
   case ms_pillar_build:
   case ms_pillar_open:
      CL_ApplyPillarStatusFromBlob(blob);
      break;
   case ms_floorwaggle:
      CL_ApplyFloorWaggleStatusFromBlob(blob);
      break;
   case ms_platform:
   case ms_platform_gen:
      CL_ApplyPlatformStatusFromBlob(blob);
      break;
   default:
      doom_printf(
         "CL_LoadSectorState: unknown node type %d, ignoring.\n",
         message->special_type
      );
      break;
   }
}

void CL_HandleMapSpecialRemovedMessage(nm_specialremoved_t *message)
{
   switch(message->special_type)
   {
   case ms_ceiling:
   case ms_ceiling_param:
      if(CeilingThinker *t = NetCeilings.lookup(message->net_id))
         P_RemoveActiveCeiling(t);
      break;
   case ms_door_tagged:
   case ms_door_manual:
   case ms_door_closein30:
   case ms_door_raisein300:
   case ms_door_param:
      if(VerticalDoorThinker *t = NetDoors.lookup(message->net_id))
         P_RemoveDoor(t);
      break;
   case ms_floor:
   case ms_stairs:
   case ms_donut:
   case ms_donut_hole:
   case ms_floor_param:
      if(FloorMoveThinker *t = NetFloors.lookup(message->net_id))
         P_RemoveFloor(t);
      break;
   case ms_elevator:
      if(ElevatorThinker *t = NetElevators.lookup(message->net_id))
         P_RemoveElevator(t);
      break;
   case ms_pillar_build:
   case ms_pillar_open:
      if(PillarThinker *t = NetPillars.lookup(message->net_id))
         P_RemovePillar(t);
      break;
   case ms_floorwaggle:
      if(FloorWaggleThinker *t = NetFloorWaggles.lookup(message->net_id))
         P_RemoveFloorWaggle(t);
      break;
   case ms_platform:
   case ms_platform_gen:
      if(PlatThinker *t = NetPlatforms.lookup(message->net_id))
         P_RemoveActivePlat(t);
      break;
   default:
      doom_printf(
         "CL_HandleMapSpecialRemovedMessage: unknown special type %d, "
         "ignoring.\n",
         message->special_type
      );
      break;
   }
}

void CL_HandleSectorPositionMessage(nm_sectorposition_t *message)
{
   uint32_t index = message->world_index % MAX_POSITIONS;

   if(message->sector_number >= numsectors)
   {
      doom_printf(
         "Received a map special spawned message containing invalid sector "
         "%d, ignoring.",
         message->sector_number
      );
      return;
   }
   cl_setting_sector_positions = true;
   CS_SetSectorPosition(
      &sectors[message->sector_number],
      &message->sector_position
   );
   CS_CopySectorPosition(
      &cs_sector_positions[message->sector_number][index % MAX_POSITIONS],
      &message->sector_position
   );
   cl_setting_sector_positions = false;
}

void CL_HandleAnnouncerEventMessage(nm_announcerevent_t *message)
{
   Mobj *source;
   sfxinfo_t *sfx;
   announcer_event_t *event;

   if(s_announcer_type == S_ANNOUNCER_NONE)
      return;
   
   // [CG] Ignore announcer events if we haven't received sync yet, or if a
   //      game isn't currently running.
   if(!cl_received_sync || gamestate != GS_LEVEL)
      return;

   if((source = NetActors.lookup(message->source_net_id)) == NULL)
   {
      doom_printf(
         "Received a announcer event message for an invalid source %u.\n",
         message->source_net_id
      );
      return;
   }

   if((event = CS_GetAnnouncerEvent(message->event_index)) == NULL)
   {
      doom_printf(
         "Received an announcer event message for an invalid announcer index "
         "%u.\n",
         message->event_index
      );
      return;
   }

   if((sfx = E_SoundForName(event->sound_name)) && source)
      S_StartSfxInfo(source, sfx, 127, ATTN_NONE, false, CHAN_AUTO);

   if(strlen(event->message))
      HU_CenterMessage(event->message);
}


