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
//   Client/Server client routines.
//
//----------------------------------------------------------------------------

#include <math.h>
#include <string.h>
#include <sys/stat.h>

#include "acs_intr.h"
#include "a_common.h"
#include "c_io.h"
#include "c_net.h"
#include "c_runcmd.h"
#include "d_dehtbl.h"
#include "d_gi.h"
#include "d_iwad.h"
#include "d_main.h"
#include "d_ticcmd.h"
#include "doomdata.h"
#include "e_edf.h"
#include "e_sound.h"
#include "e_states.h"
#include "e_things.h"
#include "g_game.h"
#include "hu_frags.h"
#include "hu_stuff.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_qstr.h"
#include "p_inter.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_partcl.h"
#include "p_portal.h"
#include "p_saveg.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_user.h"
#include "r_data.h"
#include "r_draw.h"
#include "r_main.h"
#include "s_sound.h"
#include "w_wad.h"

#include "cs_hud.h"
#include "cs_spec.h"
#include "cs_main.h"
#include "cs_config.h"
#include "cs_netid.h"
#include "cs_position.h"
#include "cs_demo.h"
#include "cs_wad.h"
#include "cl_cmd.h"
#include "cl_main.h"
#include "cl_pred.h"
#include "cl_spec.h"

#include <json/json.h>

#define SHOULD_BUFFER_MESSAGES \
   ((cl_received_sync) && (cl_packet_buffer_size != 1))
#define SHOULD_HANDLE_OLD_MESSAGES (cl_constant_prediction == 1)
#define MESSAGE_IS_OLD(i) ((i) <= (cl_current_world_index))
/*
#define SHOULD_HANDLE_MESSAGE_NOW(i) (\
   (!(SHOULD_BUFFER_MESSAGES)) && \
   ((!(MESSAGE_IS_OLD((i)))) || (SHOULD_HANDLE_OLD_MESSAGES))\
)
*/
#define SHOULD_HANDLE_MESSAGE_NOW(i) (\
   ((!(MESSAGE_IS_OLD((i)))) || (SHOULD_HANDLE_OLD_MESSAGES))\
)

char *cs_server_url = NULL;
char *cs_server_password = NULL;
char *cs_client_password_file = NULL;
json_object *cs_client_password_json = NULL;

unsigned int cl_current_world_index = 0;
unsigned int cl_latest_world_index  = 0;

boolean cl_received_sync = false;
boolean cl_initial_spawn = true;
boolean cl_spawning_actor_from_message = false;
boolean cl_removing_actor_from_message = false;

extern int levelTimeLimit;
extern int levelFragLimit;
extern int levelScoreLimit;

extern unsigned int rngseed;

extern char gamemapname[9];
extern wfileadd_t *wadfiles;

static ehash_t *network_message_hash = NULL;

static const void* NMHashFunc(void *object)
{
   return &((nm_buffer_node_t *)object)->world_index;
}

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

   buffer = calloc(
      1, sizeof(nm_playermessage_t) + player_message.length
   );

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

static void run_world(void)
{
   cl_current_world_index++;

   CL_ProcessNetworkMessages();

   // [CG] If a player we're spectating spectates, reset our display.
   if(displayplayer != consoleplayer && clients[displayplayer].spectating)
      CS_SetDisplayPlayer(consoleplayer);

   if(gamestate == GS_LEVEL && cl_received_sync && !cs_demo_playback)
      CL_SendCommand();

   G_Ticker();
   gametic++;
}

void CL_Init(char *url)
{
   size_t url_length;
   struct stat sbuf;

   ENetCallbacks callbacks = { Z_SysMalloc, Z_SysFree, abort };

   if(enet_initialize_with_callbacks(ENET_VERSION, &callbacks) != 0)
     I_Error("Could not initialize networking.\n");

   atexit(enet_deinitialize);

   net_host = enet_host_create(NULL, 1, MAX_CHANNELS, 0, 0);
   if(net_host == NULL)
     I_Error("Could not initialize client.\n");

   enet_socket_set_option(net_host->socket, ENET_SOCKOPT_NONBLOCK, 1);
   enet_host_compress_with_range_coder(net_host);

   url_length = strlen(url);
   cs_server_url = calloc(url_length + 1, sizeof(char));
   if(strncmp(url, "eternity://", 11) == 0)
      sprintf(cs_server_url, "http://%s", url + 11);
   else
      memcpy(cs_server_url, url, url_length);

   // [CG] cURL supports millions of protocols, nonetheless, make sure that
   //      the passed URI is supported (and actually a URI).
   if(!CS_CheckURI(cs_server_url))
      I_Error("Invalid URI [%s], exiting.\n", cs_server_url);

   cs_client_password_file = calloc(
      strlen(basepath) + strlen(PASSWORD_FILENAME) + 2, sizeof(char)
   );
   sprintf(cs_client_password_file, "%s/%s", basepath, PASSWORD_FILENAME);
   M_NormalizeSlashes(cs_client_password_file);

   if(!stat(cs_client_password_file, &sbuf))
   {
      if(!S_ISREG(sbuf.st_mode))
      {
         I_Error(
            "CL_Init: Password file %s exists but is not a file.\n",
            cs_client_password_file
         );
      }
      cs_client_password_json = json_object_from_file(cs_client_password_file);
   }
   else
   {
      cs_client_password_json = json_object_new_object();
      CS_PrintJSONToFile(
         cs_client_password_file,
         json_object_get_string(cs_client_password_json)
      );
   }

   if(is_error(cs_client_password_json))
   {
      I_Error(
         "CL_Init: Password file parse error:\n\t%s.\n",
         json_tokener_errors[(unsigned long)cs_client_password_json]
      );
   }

   CL_InitPrediction();
   CS_ZeroClients();
   CL_LoadConfig();
   CL_SpecInit();
   CL_InitNetworkMessageHash();
}

void CL_InitPlayDemoMode(void)
{
   CL_InitPrediction();
   CS_ZeroClients();
   CL_SpecInit();
   CL_InitNetworkMessageHash();
}

void CL_InitNetworkMessageHash(void)
{
   nm_buffer_node_t *node = NULL;

   if(network_message_hash == NULL)
   {
      network_message_hash = calloc(1, sizeof(ehash_t));
      E_UintHashInit(network_message_hash, 8192, NMHashFunc, NULL);
   }
   else
   {
      while((node = (nm_buffer_node_t *)E_HashTableIterator(
             network_message_hash, node)) != NULL)
      {
         E_HashRemoveObject(network_message_hash, node);
         free(node->message);
         free(node);
         node = NULL;
      }
   }
}

char* CL_GetUserAgent(void)
{
   char *version = CS_VersionString();
   char *user_agent = calloc(35, sizeof(char)); // [CG] 35 is probably good...

   sprintf(user_agent, "emp-client/%s", version);
   free(version);

   return user_agent;
}

void CL_Reset(void)
{
   cl_current_world_index = 0;
   cl_latest_world_index = 0;
   cl_received_sync = false;
   consoleplayer = displayplayer = 0;
   CS_InitPlayers();
   CS_ZeroClients();
   memset(playeringame, 0, MAXPLAYERS * sizeof(boolean));
   playeringame[0] = true;
   CL_InitNetworkMessageHash();
}

void CL_Connect(void)
{
   ENetEvent event;
   json_object *password_entry, *server_section, *spectator_password;
   char *address = CS_IPToString(server_address->host);

   printf("Connecting to %s:%d\n", address, server_address->port);
   free(address);
   // enet_address_set_host(&address, address_string);

   net_peer = enet_host_connect(net_host, server_address, MAX_CHANNELS, 0);
   if(net_peer == NULL)
      doom_printf("Unknown error creating a new connection.\n");

   start_time = enet_time_get();

   if(enet_host_service(net_host, &event, CONNECTION_TIMEOUT) > 0 &&
      event.type == ENET_EVENT_TYPE_CONNECT)
   {
      CS_ResetNetIDs(); // [CG] Clear the Net ID hashes.

      server_section = json_object_object_get(cs_json, "server");
      spectator_password =
         json_object_object_get(server_section, "requires_spectator_password");
      password_entry = json_object_object_get(
         cs_client_password_json, cs_server_url
      );

      // [CG] Automatically authenticate if possible.  If this server requires
      //      a password in order to connect as a spectator and we don't have a
      //      password stored for it, inform the client they need to enter one.
      if(password_entry == NULL)
      {
         if(json_object_get_boolean(spectator_password))
         {
            doom_printf(
               "This server requires a password in order to connect.  Use the "
               "'password' command to gain authorization.\n"
            );
         }
         else
            doom_printf("Connected!");
      }
      else
      {
         doom_printf("Connected!");
         CL_AuthMessage(json_object_get_string(password_entry));
      }

      if(cs_demo_recording)
      {
         CS_AddNewMapToDemo();
         doom_printf("Recording demo.");
      }
   }
   else
   {
      if(net_peer)
         enet_peer_reset(net_peer);

      doom_printf("Could not connect to server.\nConnection failed!\n");
      net_peer = NULL;
   }

}

void CL_Disconnect(void)
{
   if(net_peer == NULL)
   {
      doom_printf("Not connected.");
      return;
   }

   CS_DisconnectPeer(net_peer, dr_no_reason);
   net_peer = NULL;

   if(cs_demo_recording && !CS_StopDemo())
      printf("Error saving demo: %s\n", CS_GetDemoErrorMessage());

   CL_Reset();
   C_SetConsole();
   doom_printf("Disconnected.");
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

void CL_SendSyncRequest(void)
{
   nm_syncrequest_t message;

   message.message_type = nm_syncrequest;

   printf("CL_SendSyncRequest: Sending sync request.\n");

   send_packet(&message, sizeof(nm_syncrequest_t));
}

void CL_SendSyncReceived(void)
{
   nm_syncreceived_t message;

   message.message_type = nm_syncreceived;

   printf("CL_SendSyncReceived: Sending sync received.\n");

   send_packet(&message, sizeof(nm_syncreceived_t));
}

void CL_ServerMessage(const char *message)
{
   send_message(mr_server, 0, message);
}

void CL_PlayerMessage(int playernum, const char *message)
{
   send_message(mr_player, (unsigned int)playernum, message);
}

void CL_TeamMessage(const char *message)
{
   send_message(mr_team, 0, message);
}

void CL_BroadcastMessage(const char *message)
{
   send_message(mr_all, 0, message);
}

void CL_AuthMessage(const char *password)
{
   size_t password_length = strlen(password);

   // [CG] We want to remember what password we sent so that if the server
   //      responds with "authorization successful" we can store the password
   //      for this server's URL as valid.

   if(cs_server_password != NULL)
      free(cs_server_password);

   printf("CL_AuthMessage: Sending authorization request..\n");

   cs_server_password = calloc(password_length + 1, sizeof(char));
   memcpy(cs_server_password, password, password_length);

   send_message(mr_auth, 0, cs_server_password);
}

void CL_RCONMessage(const char *command)
{
   send_message(mr_rcon, 0, command);
}

void CL_SaveServerPassword(void)
{
   if(json_object_object_get(cs_client_password_json, cs_server_url))
      json_object_object_del(cs_client_password_json, cs_server_url);

   json_object_object_add(
      cs_client_password_json,
      cs_server_url,
      json_object_new_string(cs_server_password)
   );

   CS_PrintJSONToFile(
      cs_client_password_file,
      json_object_get_string(cs_client_password_json)
   );
}

mobj_t* CL_SpawnMobj(uint32_t net_id, fixed_t x, fixed_t y, fixed_t z,
                     mobjtype_t type)
{
   mobj_t *actor;

   cl_spawning_actor_from_message = true;
   actor = P_SpawnMobj(x, y, z, type);
   cl_spawning_actor_from_message = false;

   if(actor->net_id != 0)
      CS_ReleaseActorNetID(actor);

   actor->net_id = net_id;
   CS_RegisterActorNetID(actor);

   return actor;
}

void CL_RemoveMobj(mobj_t *actor)
{
   cl_removing_actor_from_message = true;
   P_RemoveMobj(actor);
   cl_removing_actor_from_message = false;
}

void CL_SpawnPlayer(int playernum, uint32_t net_id, fixed_t x, fixed_t y,
                    fixed_t z, angle_t angle, boolean as_spectator)
{
   if(players[playernum].mo != NULL)
      CL_RemoveMobj(players[(playernum)].mo);

   cl_spawning_actor_from_message = true;
   CS_SpawnPlayer(playernum, x, y, z, angle, as_spectator);
   cl_spawning_actor_from_message = false;

   CS_ReleaseActorNetID(players[playernum].mo);
   players[playernum].mo->net_id = net_id;
   CS_RegisterActorNetID(players[playernum].mo);
}

void CL_RemovePlayer(int playernum)
{
   player_t *player = &players[playernum];

   if((!clients[playernum].spectating) && drawparticles)
      P_DisconnectEffect(player->mo);

   player->mo = NULL;
   CS_ZeroClient(playernum);
   playeringame[playernum] = false;
   CS_InitPlayer(playernum);
}

mobj_t* CL_SpawnPuff(fixed_t x, fixed_t y, fixed_t z, angle_t angle,
                     int updown, boolean ptcl)
{
   mobj_t *puff;

   cl_spawning_actor_from_message = true;
   puff = P_SpawnPuff(x, y, z, angle, updown, ptcl);
   cl_spawning_actor_from_message = false;

   return puff;
}

mobj_t* CL_SpawnBlood(fixed_t x, fixed_t y, fixed_t z, angle_t angle,
                      int damage, mobj_t *target)
{
   mobj_t *blood;

   cl_spawning_actor_from_message = true;
   blood = P_SpawnBlood(x, y, z, angle, damage, target);
   cl_spawning_actor_from_message = false;

   return blood;
}

boolean CL_SetMobjState(mobj_t* mobj, statenum_t state)
{
   state_t *st;
   seenstate_t *seenstates = NULL;
   boolean ret = true;

   do
   {
      if(state == NullStateNum)
      {
         mobj->state = NULL;
         ret = false;
         break;
      }

      st = states[state];
      mobj->state = st;
      mobj->tics = st->tics;

      if(mobj->skin && st->sprite == mobj->info->defsprite)
         mobj->sprite = mobj->skin->sprite;
      else
         mobj->sprite = st->sprite;

      mobj->frame = st->frame;

      if(st->action)
         st->action(mobj);

      if(st->particle_evt)
         P_RunEvent(mobj);

      P_AddSeenState(state, &seenstates);
      state = st->nextstate;
   } while(!mobj->tics && !P_CheckSeenState(state, seenstates));

   if(ret && !mobj->tics)
      printf("Warning: State Cycle Detected");

   if(seenstates)
      P_FreeSeenStates(seenstates);

   return ret;
}

char* CL_ExtractServerMessage(nm_servermessage_t *message)
{
   char *server_message = ((char *)message) + sizeof(nm_servermessage_t);

   if(strlen(server_message) != (message->length - 1))
      return NULL;

   return server_message;
}

char* CL_ExtractPlayerMessage(nm_playermessage_t *message)
{
   char *player_message = ((char *)message) + sizeof(nm_playermessage_t);

   if(message->sender_number > MAXPLAYERS ||
      !playeringame[message->sender_number])
      return NULL;

   if(strlen(player_message) != (message->length - 1))
      return NULL;

   return player_message;
}

// [CG] Copied & pasted from p_pspr.c, this is just without the initial check
//      so that it can be used clientside for other clients.
void CL_SetPsprite(player_t *player, int position, statenum_t stnum)
{
   pspdef_t *psp = &player->psprites[position];

   // haleyjd 04/05/07: codepointer rewrite -- use same prototype for
   // all codepointers by getting player and psp from mo->player. This
   // requires stashing the "position" parameter in player_t, however.

   player->curpsprite = position;

   do
   {
      state_t *state;
      
      if(!stnum)
      {
         // object removed itself
         psp->state = NULL;
         break;
      }

      // WEAPON_FIXME: pre-beta bfg must become an alternate weapon under EDF

      // killough 7/19/98: Pre-Beta BFG
      if(stnum == E_StateNumForDEHNum(S_BFG1) && bfgtype == bfg_classic)
         stnum = E_SafeState(S_OLDBFG1); // Skip to alternative weapon frame

      state = states[stnum];
      psp->state = state;
      psp->tics = state->tics;        // could be 0

      if(state->misc1)
      {
         // coordinate set
         psp->sx = state->misc1 << FRACBITS;
         psp->sy = state->misc2 << FRACBITS;
      }

      // Call action routine.
      // Modified handling.
      if(state->action)
      {
         P_SetupPlayerGunAction(player, psp);

         state->action(player->mo);
         
         P_FinishPlayerGunAction();
         
         if(!psp->state)
            break;
      }
      stnum = psp->state->nextstate;
   } while(!psp->tics);   // an initial state of 0 could cycle through
}

void CL_HandleDamagedMobj(mobj_t *target, mobj_t *source, int damage, int mod,
                          emod_t *emod, boolean justhit)
{
   boolean bossignore;       // haleyjd

   // haleyjd: special death hacks: if we got here, we didn't really die
   target->intflags &= ~(MIF_DIEDFALLING|MIF_WIMPYDEATH);

   // killough 9/7/98: keep track of targets so that friends can help friends
   if(demo_version >= 203)
   {
      // If target is a player, set player's target to source,
      // so that a friend can tell who's hurting a player
      if(target->player)
         P_SetTarget(&target->target, source);

      // killough 9/8/98:
      // If target's health is less than 50%, move it to the front of its list.
      // This will slightly increase the chances that enemies will choose to
      // "finish it off", but its main purpose is to alert friends of danger.

      if(target->health * 2 < target->info->spawnhealth)
      {
         thinker_t *cap =
            &thinkerclasscap[target->flags & MF_FRIEND ?
                             th_friends : th_enemies];
         (target->thinker.cprev->cnext = target->thinker.cnext)->cprev =
            target->thinker.cprev;
         (target->thinker.cnext = cap->cnext)->cprev = &target->thinker;
         (target->thinker.cprev = cap)->cnext = &target->thinker;
      }
   }

   if(justhit)
   {
      // haleyjd 10/06/99: remove pain for zero-damage projectiles
      // FIXME: this needs a comp flag
      if(damage > 0 || demo_version < 329)
      {
         statenum_t st = target->info->painstate;
         state_t *state = NULL;

         // haleyjd  06/05/08: check for special damagetype painstate
         if(mod > 0 && (state = E_StateForMod(target->info, "Pain", emod)))
            st = state->index;

         P_SetMobjState(target, st);
      }
   }

   target->reactiontime = 0;           // we're awake now...

   // killough 9/9/98: cleaned up, made more consistent:
   // haleyjd 11/24/02: added MF3_DMGIGNORED and MF3_BOSSIGNORE flags

   // EDF_FIXME: replace BOSSIGNORE with generalized infighting controls.
   // BOSSIGNORE flag will be made obsolete.

   // haleyjd: set bossignore
   if(source && (source->type != target->type) &&
      (source->flags3 & target->flags3 & MF3_BOSSIGNORE))
   {
      // ignore if friendliness matches
      bossignore = !((source->flags ^ target->flags) & MF_FRIEND);
   }
   else
      bossignore = false;

   // Set target based on the following criteria:
   // * Damage is sourced and source is not self.
   // * Source damage is not ignored via MF3_DMGIGNORED.
   // * Threshold is expired, or target has no threshold via MF3_NOTHRESHOLD.
   // * Things are not friends, or monster infighting is enabled.
   // * Things are not both friends while target is not a SUPERFRIEND

   // TODO: add fine-grained infighting control as metadata

   if(source && source != target                                     // source checks
      && !(source->flags3 & MF3_DMGIGNORED)                          // not ignored?
      && !bossignore                                                 // EDF_FIXME!
      && (!target->threshold || (target->flags3 & MF3_NOTHRESHOLD))  // threshold?
      && ((source->flags ^ target->flags) & MF_FRIEND ||             // friendliness?
           monster_infighting || demo_version < 203)
      && !(source->flags & target->flags & MF_FRIEND &&              // superfriend?
           target->flags3 & MF3_SUPERFRIEND))
   {
      // if not intent on another player, chase after this one
      //
      // killough 2/15/98: remember last enemy, to prevent
      // sleeping early; 2/21/98: Place priority on players
      // killough 9/9/98: cleaned up, made more consistent:

      if(!target->lastenemy || target->lastenemy->health <= 0 ||
         (demo_version < 203 ? !target->lastenemy->player :
          !((target->flags ^ target->lastenemy->flags) & MF_FRIEND) &&
             target->target != source)) // remember last enemy - killough
      {
         P_SetTarget(&target->lastenemy, target->target);
      }

      P_SetTarget(&target->target, source);       // killough 11/98
      target->threshold = BASETHRESHOLD;

      if(target->state == states[target->info->spawnstate] &&
         target->info->seestate != NullStateNum)
      {
         P_SetMobjState(target, target->info->seestate);
      }
   }

   // haleyjd 01/15/06: Fix for demo comp problem introduced in MBF
   // For MBF and above, set MF_JUSTHIT here
   // killough 11/98: Don't attack a friend, unless hit by that friend.
   if(!demo_compatibility && justhit &&
      (target->target == source || !target->target ||
       !(target->flags & target->target->flags & MF_FRIEND)))
   {
      target->flags |= MF_JUSTHIT;    // fight back!
   }
}

void CL_HandleGameStateMessage(nm_gamestate_t *message)
{
   thinker_t *think;
   unsigned int i;
   unsigned int new_num = message->player_number;

   if(message->map_number > cs_map_count)
   {
      I_Error(
         "CL_HandleGameStateMessage: Invalid map number %d.\n",
         message->map_number
      );
   }

   for(think = thinkercap.next; think != &thinkercap; think = think->next)
   {
      if(!think)
         break;

      if(think->function == P_MobjThinker)
         CL_RemoveMobj((mobj_t *)think);
   }

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

   cs_current_map_number = message->map_number;

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

   rngseed = message->rngseed;
   CS_ReloadDefaults();
   memcpy(cs_settings, &message->settings, sizeof(clientserver_settings_t));
   /*
   memcpy(
      cs_original_settings,
      &message->settings,
      sizeof(clientserver_settings_t)
   );
   */
   CS_ApplyConfigSettings();
   CS_InitNew();
   memcpy(cs_flags, message->flags, sizeof(flag_t) * team_color_max);
   memcpy(team_scores, message->team_scores, sizeof(int32_t) * team_color_max);

   save_p = savebuffer = ((byte *)message) + sizeof(nm_gamestate_t);

   ACS_PrepareForLoad();

   cl_setting_sector_positions = true;
   P_UnArchivePlayers();
   P_UnArchiveWorld();
   P_UnArchivePolyObjects();   // haleyjd 03/27/06
   P_UnArchiveThinkers();
   P_UnArchiveSpecials();
   P_UnArchiveRNG();         // killough 1/18/98: load RNG information
   P_UnArchiveMap();         // killough 1/22/98: load automap information
   P_UnArchiveScripts();      // sf: scripting
   P_UnArchiveSoundSequences();
   P_UnArchiveButtons();
   P_FreeObjTable();
   cl_setting_sector_positions = false;

   Z_CheckHeap();
   save_p = savebuffer = NULL;

   R_FillBackScreen();
   ST_Start();
   ACS_RunDeferredScripts();

   CL_SendSyncRequest();
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
   CL_SendSyncReceived();
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

   gametic = message->gametic;
   levelstarttic = message->gametic;
   basetic = message->gametic;
   leveltime = message->gametic;

   for(i = 0; i < MAXPLAYERS; i++)
      playeringame[i] = message->playeringame[i];

   memcpy(cs_settings, &message->settings, sizeof(clientserver_settings_t));

   CS_ResetNetIDs();

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

   CS_DoWorldDone();

   for(i = 0; i < MAXPLAYERS; i++)
   {
      if(!playeringame[i])
         continue;

      if(!players[i].mo)
         continue;

      if(players[i].mo->net_id != 0 &&
         players[i].mo->net_id != message->net_ids[i])
      {
         I_Error(
            "Net ID for %d did not match (%u/%u), exiting.\n",
            i, players[i].mo->net_id, message->net_ids[i]
         );
      }

      players[i].mo->net_id = message->net_ids[i];

      if(players[i].mo->net_id != 0)
         CS_RegisterActorNetID(players[i].mo);
   }

   cl_flush_packet_buffer = true;
   cl_received_sync = true;
   CS_UpdateQueueMessage();
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

   if(!client->spectating)
   {
      if(playernum == consoleplayer &&
         players[playernum].playerstate == PST_LIVE)
      {
         oldbob = players[playernum].bob;
         oldviewz = players[playernum].viewz;
         oldviewheight = players[playernum].viewheight;
         olddeltaviewheight = players[playernum].deltaviewheight;
      }

      CS_SetPlayerPosition(playernum, &message->position);
      clients[playernum].floor_status = message->floor_status;

      // [CG] Re-predicts from the position just received from the server on.
      if(playernum == consoleplayer && cl_enable_prediction)
      {
         if(message->last_command_run > 0)
         {
            CL_Predict(
               message->last_command_run + 1, cl_current_world_index, true
            );
         }
      }

      if(playernum == consoleplayer &&
         players[playernum].playerstate == PST_LIVE)
      {
         players[playernum].bob = oldbob;
         players[playernum].viewz = oldviewz;
         players[playernum].viewheight = oldviewheight;
         players[playernum].deltaviewheight = olddeltaviewheight;
      }
   }

   // [CG] TODO: Put something here that marks network message indices as no
   //            longer needed, so they can be cleared accurately.  Once the
   //            position for a given command has been checked, there's no
   //            reason to keep network messages from that index or before, so
   //            they can be cleared.
}

void CL_HandlePlayerSpawnedMessage(nm_playerspawned_t *message)
{
   player_t *player = &players[message->player_number];

   printf("Handling player spawned message.\n");

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
   if(!playeringame[message->player_number])
   {
      // [CG] During spawning, a player's weapon sprites will be initialized
      //      and this process sends a message serverside.  The client will
      //      receive the message before they receive nm_playerspawned, so we
      //      have to ignore weapon state messages for players who aren't
      //      ingame silently.
      return;
   }

   if(message->player_number != consoleplayer || !cl_enable_prediction)
   {
      CL_SetPsprite(
         &players[message->player_number],
         message->psprite_position,
         message->weapon_state
      );
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
   mobj_t *mo;

   mo = CS_GetActorFromNetID(message->thing_net_id);

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

void CL_HandleServerMessage(nm_servermessage_t *message)
{
   char *server_message = CL_ExtractServerMessage(message);

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
   char *player_message = CL_ExtractPlayerMessage(message);

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
   mobj_t *target;

   if(cl_predict_shots &&
      cl_enable_prediction &&
      message->shooter_net_id == players[consoleplayer].mo->net_id)
   {
      return;
   }

   if((target = CS_GetActorFromNetID(message->target_net_id)) == NULL)
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
   mobj_t *actor;
   int safe_item_fog_type          = E_SafeThingType(MT_IFOG);
   int safe_tele_fog_type          = GameModeInfo->teleFogType;
   int safe_bfg_type               = E_SafeThingType(MT_BFG);
   int safe_spawn_fire_type        = E_SafeThingType(MT_SPAWNFIRE);
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
   mobj_t *actor;

   actor = CS_GetActorFromNetID(message->actor_net_id);

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
   mobj_t *actor;
   mobj_t *target = NULL;

   actor = CS_GetActorFromNetID(message->actor_net_id);

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
      target = CS_GetActorFromNetID(message->target_net_id);
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
   mobj_t *actor = CS_GetActorFromNetID(message->actor_net_id);

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

void CL_HandleMonsterAwakenedMessage(nm_monsterawakened_t *message)
{
   int sound;
   mobj_t *monster = CS_GetActorFromNetID(message->monster_net_id);

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

void CL_HandleActorDamagedMessage(nm_actordamaged_t *message)
{
   emod_t *emod;
   player_t *player;
   mobj_t *target;
   mobj_t *inflictor = NULL;
   mobj_t *source = NULL;

   emod = E_DamageTypeForNum(message->mod);
   target = CS_GetActorFromNetID(message->target_net_id);

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
      source = CS_GetActorFromNetID(message->source_net_id);
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
      inflictor = CS_GetActorFromNetID(message->inflictor_net_id);
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
      CL_HandleDamagedMobj(
         target,
         source,
         message->health_damage,
         message->mod,
         emod,
         message->just_hit
      );
   }
}

void CL_HandleActorKilledMessage(nm_actorkilled_t *message)
{
   emod_t *emod;
   mobj_t *target, *inflictor, *source;

   target = CS_GetActorFromNetID(message->target_net_id);
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
      inflictor = CS_GetActorFromNetID(message->inflictor_net_id);
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
      source = CS_GetActorFromNetID(message->source_net_id);
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
   mobj_t *actor;

   if((actor = CS_GetActorFromNetID(message->actor_net_id)) == NULL)
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
   mobj_t *actor;
   line_t *line;
   position_t position;

   actor = CS_GetActorFromNetID(message->actor_net_id);
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
   CS_SaveActorPosition(&position, actor, gametic);
   // [CG] TODO: Actually move the actor into the position included in the
   //            line activation message (why didn't I add this earlier...?).

   if(message->activation_type == at_used)
      P_UseSpecialLine(actor, line, message->side);
   else if(message->activation_type == at_crossed)
      P_CrossSpecialLine(line, message->side, actor);
   else if(message->activation_type == at_shot)
      P_ShootSpecialLine(actor, line, message->side);

   CS_SetActorPosition(actor, &position);
}

void CL_HandleMonsterActiveMessage(nm_monsteractive_t *message)
{
   mobj_t *monster;

   monster = CS_GetActorFromNetID(message->monster_net_id);
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

void CL_HandleMissileSpawnedMessage(nm_missilespawned_t *message)
{
   int player_number;
   player_t *player;
   mobj_t *source, *missile;

   source = CS_GetActorFromNetID(message->source_net_id);
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
   mobj_t *missile;

   missile = CS_GetActorFromNetID(message->missile_net_id);
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
   mobj_t *cube, *target;

   cube = CS_GetActorFromNetID(message->net_id);
   if(cube == NULL)
   {
      doom_printf(
         "Received a cube spawned message for an invalid cube %u.\n",
         message->net_id
      );
      return;
   }

   target = CS_GetActorFromNetID(message->target_net_id);
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
}

void CL_HandleSoundPlayedMessage(nm_soundplayed_t *message)
{
   mobj_t *source;
   sfxinfo_t *sfx;
   char *sound_name = calloc(message->sound_name_size + 2, sizeof(char));
         
   strncpy(
      sound_name,
      (const char *)message + sizeof(nm_soundplayed_t),
      message->sound_name_size
   );

   // [CG] Ignore server-generated sounds if we haven't received sync yet, or
   //      if a game isn't currently running.
   if(!cl_received_sync || gamestate != GS_LEVEL)
      return;

   if((source = CS_GetActorFromNetID(message->source_net_id)) == NULL)
   {
      doom_printf(
         "Received a sound played message for an invalid source %u.\n",
         message->source_net_id
      );
      return;
   }

   if(!(sfx = E_SoundForName(sound_name)))
   {
      printf(
         "Received a sound played message for an invalid sound %s.\n",
         sound_name
      );
      return;
   }

   S_StartSfxInfo(
      source,
      sfx,
      message->volume,
      message->attenuation,
      message->loop,
      message->channel
   );
}

void CL_SetLatestFinishedIndices(unsigned int index)
{
#if _PRED_DEBUG
   /*
   printf(
      "CL_SetLatestFinishedIndices (%3u/%3u): %u finished.\n",
      cl_current_world_index,
      cl_latest_world_index,
      index
   );
   */
#endif

   cl_latest_world_index = index;

#if _SECTOR_PRED_DEBUG
   // printf(
   //    "CL_SetLatestFinishedIndices: %3u is finished.\n",
   //    cl_latest_world_index
   // );
#endif

   // [CG] This means we haven't even completed a single TIC yet, so ignore the
   //      piled up messages we're receiving.
   if(cl_current_world_index == 0)
      return;

   if((cl_latest_world_index > cl_current_world_index) &&
      ((cl_latest_world_index - cl_current_world_index) > CL_MAX_BUFFER_SIZE))
   {
      /* [CG] Used to disconnect here, now we flush the buffer, even if
       *      flushing all these TICs is extremely disorienting.
      doom_printf(
         "Network message buffer overflowed (%u/%u), disconnecting.",
         cl_current_world_index, cl_latest_world_index
      );
      CL_Disconnect();
      C_Printf("5.\n");
      */
      unsigned int old_world_index = cl_current_world_index;
      while(cl_current_world_index < (cl_latest_world_index - 2))
         run_world();
      CL_Predict(old_world_index, cl_latest_world_index - 2, false);
   }
}

void CL_HandleMessage(char *data, size_t data_length)
{
   int32_t message_type = *(int32_t *)data;
   size_t message_size = 0;
   unsigned int message_index = 0;
   nm_buffer_node_t *node = NULL;
   boolean handled_a_message = false;

   if(message_type < 0 || message_type >= nm_max_messages)
   {
      doom_printf(
         "Received invalid network message %d, ignoring.\n", message_type
      );
   }

   // [CG] The weird style here is so it's easy to omit message receipts that
   //      you don't want printed out.
   if(
      message_type == nm_gamestate
      || message_type == nm_sync
      || message_type == nm_mapstarted
      || message_type == nm_mapcompleted
      || message_type == nm_authresult
      || message_type == nm_clientinit
      // || message_type == nm_clientstatus
      || message_type == nm_playerspawned
      || message_type == nm_playerinfoupdated
      // || message_type == nm_playerweaponstate
      || message_type == nm_playerremoved
      // || message_type == nm_playertouchedspecial
      || message_type == nm_servermessage
      || message_type == nm_playermessage
      || message_type == nm_puffspawned
      || message_type == nm_bloodspawned
      || message_type == nm_actorspawned
      // || message_type == nm_actorposition
      // || message_type == nm_actortarget
      // || message_type == nm_actorstate
      || message_type == nm_actordamaged
      || message_type == nm_actorkilled
      || message_type == nm_actorremoved
      // || message_type == nm_lineactivated
      // || message_type == nm_monsteractive
      // || message_type == nm_monsterawakened
      || message_type == nm_missilespawned
      || message_type == nm_missileexploded
      || message_type == nm_cubespawned
      || message_type == nm_specialspawned
      // || message_type == nm_specialstatus
      || message_type == nm_specialremoved
      // || message_type == nm_sectorposition
      || message_type == nm_soundplayed
      // || message_type == nm_ticfinished
      )
   {
      printf(
         "Received [%s message], %d.\n",
         network_message_names[message_type],
         cl_received_sync
      );
   }

   // [CG] This is the main message handler.  It handles various cases
   //      including before and after the client's received sync, and the
   //      buffer being enabled or disabled.

   // [CG] nm_servermessage and nm_playermessage add an extra byte to
   //      message_size to ensure that the appended message is NULL-terminated.
   //      This allows the use of strlen to check string size in
   //      CL_ExtractServer/PlayerMessage without allocating a totally separate
   //      (but safe) buffer.

   switch(message_type)
   {
   case nm_gamestate:
      message_index = 0; // [CG] Game state messages have no world index.
      message_size  = ((nm_gamestate_t *)data)->state_size +
                      sizeof(nm_gamestate_t);
      CL_HandleGameStateMessage((nm_gamestate_t *)data);
      handled_a_message = true;
      break;
   case nm_sync:
      message_index = ((nm_sync_t *)data)->world_index;
      message_size  = sizeof(nm_sync_t);
      CL_HandleSyncMessage((nm_sync_t *)data);
      handled_a_message = true;
      break;
   case nm_mapstarted:
      message_index = ((nm_mapstarted_t *)data)->world_index;
      message_size  = sizeof(nm_mapstarted_t);
      CL_HandleMapStartedMessage((nm_mapstarted_t *)data);
      handled_a_message = true;
      break;
   case nm_mapcompleted:
      message_index = ((nm_mapcompleted_t *)data)->world_index;
      message_size  = sizeof(nm_mapcompleted_t);
      CL_HandleMapCompletedMessage((nm_mapcompleted_t *)data);
      handled_a_message = true;
      break;
   case nm_clientinit:
      message_index = ((nm_clientinit_t *)data)->world_index;
      message_size  = sizeof(nm_clientinit_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandleClientInitMessage((nm_clientinit_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_authresult:
      message_index = ((nm_authresult_t *)data)->world_index;
      message_size  = sizeof(nm_authresult_t);
      CL_HandleAuthResultMessage((nm_authresult_t *)data);
      handled_a_message = true;
      break;
   case nm_clientstatus:
      message_index = ((nm_clientstatus_t *)data)->world_index;
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandleClientStatusMessage((nm_clientstatus_t *)data);
         handled_a_message = true;
      }
      message_size = sizeof(nm_clientstatus_t);
      break;
   case nm_playerspawned:
      message_index = ((nm_playerspawned_t *)data)->world_index;
      message_size = sizeof(nm_playerspawned_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandlePlayerSpawnedMessage((nm_playerspawned_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_playerinfoupdated:
      message_index = ((nm_playerinfoupdated_t *)data)->world_index;
      message_size = data_length;
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CS_HandleUpdatePlayerInfoMessage((nm_playerinfoupdated_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_playerweaponstate:
      message_index = ((nm_playerweaponstate_t *)data)->world_index;
      message_size = sizeof(nm_playerweaponstate_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandlePlayerWeaponStateMessage((nm_playerweaponstate_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_playerremoved:
      message_index = ((nm_playerremoved_t *)data)->world_index;
      message_size = sizeof(nm_playerremoved_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandlePlayerRemovedMessage((nm_playerremoved_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_playertouchedspecial:
      message_index = ((nm_playertouchedspecial_t *)data)->world_index;
      message_size = sizeof(nm_playertouchedspecial_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandlePlayerTouchedSpecialMessage(
            (nm_playertouchedspecial_t *)data
         );
         handled_a_message = true;
      }
      break;
   case nm_servermessage:
      if(((nm_servermessage_t *)data)->length > (MAX_STRING_SIZE + 1) ||
         data_length > (sizeof(nm_servermessage_t) + MAX_STRING_SIZE + 1))
      {
         doom_printf(
            "Received server message with excessive length, ignoring.\n"
         );
         return;
      }
      message_index = ((nm_servermessage_t *)data)->world_index;
      message_size = sizeof(nm_servermessage_t) +
                     ((nm_servermessage_t *)data)->length + 1;
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandleServerMessage((nm_servermessage_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_playermessage:
      if(((nm_playermessage_t *)data)->length > (MAX_STRING_SIZE + 1) ||
         data_length > (sizeof(nm_playermessage_t) + MAX_STRING_SIZE + 1))
      {
         doom_printf(
            "Received player message with excessive length, ignoring.\n"
         );
         return;
      }
      message_index = ((nm_playermessage_t *)data)->world_index;
      message_size = sizeof(nm_playermessage_t) +
                     ((nm_playermessage_t *)data)->length + 1;
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandlePlayerMessage((nm_playermessage_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_puffspawned:
      message_index = ((nm_puffspawned_t *)data)->world_index;
      message_size = sizeof(nm_puffspawned_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandlePuffSpawnedMessage((nm_puffspawned_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_bloodspawned:
      message_index = ((nm_bloodspawned_t *)data)->world_index;
      message_size = sizeof(nm_bloodspawned_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandleBloodSpawnedMessage((nm_bloodspawned_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_actorspawned:
      message_index = ((nm_actorspawned_t *)data)->world_index;
      message_size = sizeof(nm_actorspawned_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandleActorSpawnedMessage((nm_actorspawned_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_actorposition:
      message_index = ((nm_actorposition_t *)data)->world_index;
      message_size = sizeof(nm_actorposition_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandleActorPositionMessage((nm_actorposition_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_actortarget:
      message_index = ((nm_actortarget_t *)data)->world_index;
      message_size = sizeof(nm_actortarget_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandleActorTargetMessage((nm_actortarget_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_actorstate:
      message_index = ((nm_actorstate_t *)data)->world_index;
      message_size = sizeof(nm_actorstate_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandleActorStateMessage((nm_actorstate_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_actordamaged:
      message_index = ((nm_actordamaged_t *)data)->world_index;
      message_size = sizeof(nm_actordamaged_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandleActorDamagedMessage((nm_actordamaged_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_actorkilled:
      message_index = ((nm_actorkilled_t *)data)->world_index;
      message_size = sizeof(nm_actorkilled_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandleActorKilledMessage((nm_actorkilled_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_actorremoved:
      message_index = ((nm_actorremoved_t *)data)->world_index;
      message_size = sizeof(nm_actorremoved_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandleActorRemovedMessage((nm_actorremoved_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_lineactivated:
      message_index = ((nm_lineactivated_t *)data)->world_index;
      message_size = sizeof(nm_lineactivated_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandleLineActivatedMessage((nm_lineactivated_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_monsteractive:
      message_index = ((nm_monsteractive_t *)data)->world_index;
      message_size = sizeof(nm_monsteractive_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandleMonsterActiveMessage((nm_monsteractive_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_monsterawakened:
      message_index = ((nm_monsterawakened_t *)data)->world_index;
      message_size = sizeof(nm_monsterawakened_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandleMonsterAwakenedMessage((nm_monsterawakened_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_missilespawned:
      message_index = ((nm_missilespawned_t *)data)->world_index;
      message_size = sizeof(nm_missilespawned_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandleMissileSpawnedMessage((nm_missilespawned_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_missileexploded:
      message_index = ((nm_missileexploded_t *)data)->world_index;
      message_size = sizeof(nm_missileexploded_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandleMissileExplodedMessage((nm_missileexploded_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_cubespawned:
      message_index = ((nm_cubespawned_t *)data)->world_index;
      message_size = sizeof(nm_cubespawned_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandleCubeSpawnedMessage((nm_cubespawned_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_specialspawned:
   {
      nm_specialspawned_t *message = (nm_specialspawned_t *)data;

      message_index = message->world_index;

      switch(message->special_type)
      {
      case ms_ceiling:
         message_size = sizeof(nm_specialspawned_t) +
                        sizeof(ceiling_status_t);
         break;
      case ms_ceiling_param:
         message_size = sizeof(nm_specialspawned_t) +
                        sizeof(ceiling_status_t) +
                        sizeof(cs_ceilingdata_t);
         break;
      case ms_door_tagged:
      case ms_door_manual:
      case ms_door_closein30:
      case ms_door_raisein300:
         message_size = sizeof(nm_specialspawned_t) + sizeof(door_status_t);
         break;
      case ms_door_param:
         message_size = sizeof(nm_specialspawned_t) +
                        sizeof(door_status_t) +
                        sizeof(cs_doordata_t);
         break;
      case ms_floor:
      case ms_stairs:
      case ms_donut:
      case ms_donut_hole:
         message_size = sizeof(nm_specialspawned_t) +
                        sizeof(floor_status_t);
         break;
      case ms_floor_param:
         message_size = sizeof(nm_specialspawned_t) +
                        sizeof(floor_status_t) +
                        sizeof(cs_floordata_t);
         break;
      case ms_elevator:
         message_size = sizeof(nm_specialspawned_t) +
                        sizeof(elevator_status_t);
         break;
      case ms_pillar_build:
      case ms_pillar_open:
         message_size = sizeof(nm_specialspawned_t) +
                        sizeof(pillar_status_t);
         break;
      case ms_floorwaggle:
         message_size = sizeof(nm_specialspawned_t) +
                        sizeof(floorwaggle_status_t);
         break;
      case ms_platform:
      case ms_platform_gen:
         message_size = sizeof(nm_specialspawned_t) +
                        sizeof(platform_status_t);
         break;
      default:
         doom_printf(
            "Received map special spawned message with invalid type %d, "
            "ignoring.",
            message->special_type
         );
         return;
      }

      if(message->line_number >= numlines)
      {
         printf(
            "Received a map special spawned message containing invalid line "
            "%d, ignoring.\n",
            message->line_number
         );
         return;
      }

      if(message->sector_number >= numsectors)
      {
         printf(
            "Received a map special spawned message containing invalid sector "
            "%d, ignoring.\n",
            message->sector_number
         );
         return;
      }

      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandleMapSpecialSpawnedMessage(message);
         handled_a_message = true;
      }
      break;
   }
   case nm_specialstatus:
      message_index = ((nm_specialstatus_t *)data)->world_index;
      message_size = data_length;

      CL_BufferMapSpecialStatus(
         (nm_specialstatus_t *)data,
         (void *)(data + sizeof(nm_specialstatus_t))
      );

      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         switch(((nm_specialremoved_t *)data)->special_type)
         {
         case ms_ceiling:
         case ms_ceiling_param:
            CL_ApplyCeilingStatus(
               (ceiling_status_t *)(data + sizeof(nm_specialstatus_t))
            );
            break;
         case ms_door_tagged:
         case ms_door_manual:
         case ms_door_closein30:
         case ms_door_raisein300:
         case ms_door_param:
            CL_ApplyDoorStatus(
               (door_status_t *)(data + sizeof(nm_specialstatus_t))
            );
            break;
         case ms_floor:
         case ms_stairs:
         case ms_donut:
         case ms_donut_hole:
         case ms_floor_param:
            CL_ApplyFloorStatus(
               (floor_status_t *)(data + sizeof(nm_specialstatus_t))
            );
            break;
         case ms_elevator:
            CL_ApplyElevatorStatus(
               (elevator_status_t *)(data + sizeof(nm_specialstatus_t))
            );
            break;
         case ms_pillar_build:
         case ms_pillar_open:
            CL_ApplyPillarStatus(
               (pillar_status_t *)(data + sizeof(nm_specialstatus_t))
            );
            break;
         case ms_floorwaggle:
            CL_ApplyFloorWaggleStatus(
               (floorwaggle_status_t *)(data + sizeof(nm_specialstatus_t))
            );
            break;
         case ms_platform:
         case ms_platform_gen:
            CL_ApplyPlatformStatus(
               (platform_status_t *)(data + sizeof(nm_specialstatus_t))
            );
            break;
         default:
            doom_printf(
               "CL_HandleMessage: Received a map special status message "
               "with invalid type %d, ignoring.",
               ((nm_specialremoved_t *)data)->special_type
            );
         }
      }
      handled_a_message = true;
      break;
   case nm_specialremoved:
      switch(((nm_specialremoved_t *)data)->special_type)
      {
      case ms_ceiling:
      case ms_door_tagged:
      case ms_door_manual:
      case ms_door_closein30:
      case ms_door_raisein300:
      case ms_floor:
      case ms_stairs:
      case ms_donut:
      case ms_donut_hole:
      case ms_elevator:
      case ms_pillar_build:
      case ms_pillar_open:
      case ms_floorwaggle:
      case ms_platform:
         break;
      default:
         doom_printf(
            "Received a map special removed message with invalid type %d, "
            "ignoring.",
            ((nm_specialremoved_t *)data)->special_type
         );
         return;
      }
      message_index = ((nm_specialremoved_t *)data)->world_index;
      message_size = sizeof(nm_specialremoved_t);
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandleMapSpecialRemovedMessage((nm_specialremoved_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_sectorposition:
   {
      nm_sectorposition_t *message = (nm_sectorposition_t *)data;

      message_index = message->world_index;
      message_size = sizeof(nm_sectorposition_t);

      if(message->sector_number >= numsectors)
      {
         printf(
            "Received a sector position message containing invalid sector "
            "%d, ignoring.\n",
            message->sector_number
         );
         return;
      }

      CL_BufferSectorPosition(message);

      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         nm_sectorposition_t *message = (nm_sectorposition_t *)data;

         cl_setting_sector_positions = true;
         CS_SetSectorPosition(
            &sectors[message->sector_number], &message->sector_position
         );
         CS_CopySectorPosition(
            &cs_sector_positions[message->sector_number]
                                [message->world_index % MAX_POSITIONS],
            &message->sector_position
         );
         cl_setting_sector_positions = false;
      }
      handled_a_message = true;
      break;
   }
   case nm_soundplayed:
      message_index = ((nm_soundplayed_t *)data)->world_index;
      message_size = data_length;
      if(SHOULD_HANDLE_MESSAGE_NOW(message_index))
      {
         CL_HandleSoundPlayedMessage((nm_soundplayed_t *)data);
         handled_a_message = true;
      }
      break;
   case nm_ticfinished:
      CL_SetLatestFinishedIndices(
         ((nm_ticfinished_t *)data)->world_index
      );
      handled_a_message = true;
      break;
   case nm_playercommand:
   case nm_max_messages:
   default:
      doom_printf(
         "Received unknown network message %d, ignoring.\n", message_type
      );
      break;
   }

   if(SHOULD_HANDLE_OLD_MESSAGES && MESSAGE_IS_OLD(message_index))
   {
      if(!handled_a_message)
      {
         printf(
            "Dropping [%s] message.\n", network_message_names[message_type]
         );
      }
   }

   if(handled_a_message)
      return;

   if(!SHOULD_BUFFER_MESSAGES)
      return;

   if(SHOULD_HANDLE_OLD_MESSAGES && MESSAGE_IS_OLD(message_index))
      return;

   if(message_index <= cl_current_world_index)
   {
      printf(
         "Received a message that will never be processed, exiting.\n"
         "  Message Type: %s\n"
         "  Received Sync: %d\n"
         "  Buffer Size:   %u\n"
         "  Constant Pred: %d\n"
         "  Message Index: %u\n"
         "  Current Index: %u",
         network_message_names[message_type],
         cl_received_sync,
         cl_packet_buffer_size,
         cl_constant_prediction,
         message_index,
         cl_current_world_index
      );
      CL_Disconnect();
      I_Error("\n");
   }

   //
   // [CG] TODO: Really, a check for maximum message_size right here would be
   //            good.
   //

   node = calloc(1, sizeof(nm_buffer_node_t));
   node->world_index = message_index;
   node->message = calloc(1, message_size);
   memcpy(node->message, data, message_size);

   E_HashAddObject(network_message_hash, node);
}

void CL_ProcessNetworkMessages(void)
{
   uint32_t message_type;
   nm_buffer_node_t *node = NULL;
   nm_buffer_node_t *reordered_nodes = NULL;
   boolean processed_a_message = false;

   CL_ProcessSectorPositions(cl_current_world_index);

   if(cl_received_sync && cl_packet_buffer_size == 1)
   {
      CL_ClearMapSpecialStatuses();
      CL_ClearSectorPositions();
      return;
   }

   // [CG] mdllist stores its nodes in LIFO order like a stack.  This causes
   //      problems because it means network messages received during the same
   //      index are processed in reverse.  To fix this, all messages for the
   //      current index are are pulled out of the buffer and stored in a
   //      normal mdllist, then they're just popped off normally.
   while((node = (nm_buffer_node_t *)E_HashObjectIterator(
            network_message_hash, node, &cl_current_world_index
         )) != NULL)
   {
      message_type = *((uint32_t *)node->message);

      E_HashRemoveObject(network_message_hash, node);
      M_DLListInsert(
         (mdllistitem_t *)node, (mdllistitem_t **)(&reordered_nodes)
      );
      node = NULL;
   }

   while((node = reordered_nodes) != NULL)
   {
      message_type = *((uint32_t *)node->message);

      switch(message_type)
      {
      case nm_gamestate:
         break;
      case nm_sync:
         break;
      case nm_mapstarted:
         break;
      case nm_mapcompleted:
         break;
      case nm_authresult:
         break;
      case nm_clientinit:
         CL_HandleClientInitMessage((nm_clientinit_t *)node->message);
         processed_a_message = true;
         break;
      case nm_clientstatus:
         CL_HandleClientStatusMessage((nm_clientstatus_t *)node->message);
         processed_a_message = true;
         break;
      case nm_playerspawned:
         CL_HandlePlayerSpawnedMessage((nm_playerspawned_t *)node->message);
         processed_a_message = true;
         break;
      case nm_playerinfoupdated:
         CS_HandleUpdatePlayerInfoMessage(
            (nm_playerinfoupdated_t *)node->message
         );
         processed_a_message = true;
         break;
      case nm_playerweaponstate:
         CL_HandlePlayerWeaponStateMessage(
            (nm_playerweaponstate_t *)node->message
         );
         processed_a_message = true;
         break;
      case nm_playerremoved:
         CL_HandlePlayerRemovedMessage((nm_playerremoved_t *)node->message);
         processed_a_message = true;
         break;
      case nm_playertouchedspecial:
         CL_HandlePlayerTouchedSpecialMessage(
            (nm_playertouchedspecial_t *)node->message
         );
         processed_a_message = true;
         break;
      case nm_servermessage:
         CL_HandleServerMessage((nm_servermessage_t *)node->message);
         processed_a_message = true;
         break;
      case nm_playermessage:
         CL_HandlePlayerMessage((nm_playermessage_t *)node->message);
         processed_a_message = true;
         break;
      case nm_puffspawned:
         CL_HandlePuffSpawnedMessage((nm_puffspawned_t *)node->message);
         processed_a_message = true;
         break;
      case nm_bloodspawned:
         CL_HandleBloodSpawnedMessage((nm_bloodspawned_t *)node->message);
         processed_a_message = true;
         break;
      case nm_actorspawned:
         CL_HandleActorSpawnedMessage((nm_actorspawned_t *)node->message);
         processed_a_message = true;
         break;
      case nm_actorposition:
         CL_HandleActorPositionMessage((nm_actorposition_t *)node->message);
         processed_a_message = true;
         break;
      case nm_actortarget:
         CL_HandleActorTargetMessage((nm_actortarget_t *)node->message);
         processed_a_message = true;
         break;
      case nm_actorstate:
         CL_HandleActorStateMessage((nm_actorstate_t *)node->message);
         processed_a_message = true;
         break;
      case nm_actordamaged:
         CL_HandleActorDamagedMessage((nm_actordamaged_t *)node->message);
         processed_a_message = true;
         break;
      case nm_actorkilled:
         CL_HandleActorKilledMessage((nm_actorkilled_t *)node->message);
         processed_a_message = true;
         break;
      case nm_actorremoved:
         CL_HandleActorRemovedMessage((nm_actorremoved_t *)node->message);
         processed_a_message = true;
         break;
      case nm_lineactivated:
         CL_HandleLineActivatedMessage((nm_lineactivated_t *)node->message);
         processed_a_message = true;
         break;
      case nm_monsteractive:
         CL_HandleMonsterActiveMessage(
            (nm_monsteractive_t *)node->message
         );
         processed_a_message = true;
         break;
      case nm_monsterawakened:
         CL_HandleMonsterAwakenedMessage(
            (nm_monsterawakened_t *)node->message
         );
         processed_a_message = true;
         break;
      case nm_missilespawned:
         CL_HandleMissileSpawnedMessage(
            (nm_missilespawned_t *)node->message
         );
         processed_a_message = true;
         break;
      case nm_missileexploded:
         CL_HandleMissileExplodedMessage(
            (nm_missileexploded_t *)node->message
         );
         processed_a_message = true;
         break;
      case nm_cubespawned:
         CL_HandleCubeSpawnedMessage((nm_cubespawned_t *)node->message);
         processed_a_message = true;
         break;
      case nm_specialspawned:
         CL_HandleMapSpecialSpawnedMessage(
            (nm_specialspawned_t *)node->message
         );
         break;
      case nm_specialstatus:
         break;
      case nm_sectorposition:
         break;
      case nm_specialremoved:
         CL_HandleMapSpecialRemovedMessage(
            (nm_specialremoved_t *)node->message
         );
         break;
      case nm_soundplayed:
         CL_HandleSoundPlayedMessage((nm_soundplayed_t *)node->message);
         break;
      case nm_ticfinished:
         break;
      case nm_playercommand:
      case nm_max_messages:
      default:
         doom_printf(
            "Received unknown server message %d, ignoring.\n", message_type
         );
         break;
      }
      M_DLListRemove((mdllistitem_t *)node);
      free(node->message);
      free(node);
   }

   CL_ClearMapSpecialStatuses();
   CL_ClearSectorPositions();
}

void CL_RunDemoTics(void)
{
   int current_tic;
   static int new_tic;
   static boolean displayed_message = false;

   current_tic = new_tic;
   new_tic = I_GetTime();

   I_StartTic();
   D_ProcessEvents();
   MN_Ticker();
   C_Ticker();
   V_FPSTicker();

   // [CG] The general idea is:
   //      - Read demo packets until cl_latest_world_index updates.
   //      - Sleep until next TIC.

   if(CS_DemoFinished())
   {
      if(!displayed_message)
      {
         HU_CenterMessage("Demo playback complete.\n");
         displayed_message = true;
         if(!CS_StopDemo())
         {
            doom_printf("Error stopping demo: %s.", CS_GetDemoErrorMessage());
         }
      }
      HU_Ticker();
   }
   else
   {
      displayed_message = false;

      while(cl_latest_world_index <= cl_current_world_index)
      {
         if(!CS_ReadDemoPacket())
         {
            boolean is_finished = CS_DemoFinished();

            if(is_finished)
               break;

            doom_printf("Error reading demo: %s.\n", CS_GetDemoErrorMessage());

            if(!CS_StopDemo())
            {
               doom_printf(
                  "Error stopping demo: %s.", CS_GetDemoErrorMessage()
               );
            }
            return;
         }
      }

      while(cl_latest_world_index > cl_current_world_index)
         run_world();
   }

   do
   {
      I_StartTic();
      D_ProcessEvents();
   } while((new_tic = I_GetTime()) <= current_tic);
}

void CL_TryRunTics(void)
{
   int current_tic, buffer_size;
   boolean flush_buffer = false;
   boolean received_sync = cl_received_sync;
   client_t *client = &clients[consoleplayer];
   static int new_tic;

   current_tic = new_tic;
   new_tic = I_GetTime();

   I_StartTic();
   D_ProcessEvents();
   MN_Ticker();
   C_Ticker();
   V_FPSTicker();

   // [CG] Because worlds may be skipped in order to keep buffer sizes low,
   //      it's necessary to pull damagecount and bonuscount decrementing out
   //      from the normal game loop.
   if(consoleplayer && players[consoleplayer].damagecount)
      players[consoleplayer].damagecount--;

   if(consoleplayer && players[consoleplayer].bonuscount)
      players[consoleplayer].bonuscount--;

   buffer_size = cl_latest_world_index - cl_current_world_index;
   if(buffer_size < 0)
      buffer_size = 0;

   // [CG] There are a few cases in which we flush the packet buffer and some
   //      precedents.  Hopefully these if statements are clear enough.
   if(clients[consoleplayer].spectating && cl_buffer_packets_while_spectating)
   {
      // [CG] If the client's disabled buffer flushing while spectating, then
      //      abort buffer flushing if it was activated above.
      flush_buffer = false;
   }
   else if(cl_packet_buffer_size < 2)
   {
      // [CG] pbs == 1 disables the buffer, and pbs == 0 is handled specially
      //      below.
      flush_buffer = false;
   }

   // [CG] If we're directed to flush the buffer we must... unless we haven't
   //      yet received sync.
   if(cl_flush_packet_buffer)
      flush_buffer = true;

   // [CG] The buffer absolutely cannot be flushed if we've not yet received
   //      sync; this is a fail-safe here.
   if(!cl_received_sync)
      flush_buffer = false;

   if(flush_buffer)
   {
      unsigned int old_world_index = cl_current_world_index;
      printf("Flushing buffer.\n");
      while(cl_current_world_index < (cl_latest_world_index - 2))
         run_world();
      CL_Predict(old_world_index, cl_latest_world_index - 2, false);
      cl_flush_packet_buffer = false;
   }
   else if(cl_packet_buffer_size == 0)
   {
      // [CG] Adaptive buffer flushing.  Load a single extra world in an
      //      attempt to catch up.
      if(buffer_size > (((float)client->transit_lag / TICRATE) + 3))
         run_world();
   }
   else if(!cl_constant_prediction && buffer_size > cl_packet_buffer_size)
   {
      // [CG] Manual buffer flushing.  Load a single extra world in an attempt
      //      to catch up.
      run_world();
   }

   if(cl_constant_prediction ||
      (cl_current_world_index < (cl_latest_world_index - 1)))
   {
      // [CG] The server finished an index, so process network messages, send a
      //      command, and run G_Ticker.
      run_world();
   }

   // [CG] For the rest of the TIC, try reading from the network.  Even though
   //      things happen "after" this, this functionally serves as the end of
   //      the main game loop.
   if(net_peer)
   {
      do
      {
         // [CG] Without I_StartTic and D_ProcessEvents here, very noticeable
         //      mouse deceleration occurs.
         I_StartTic();
         D_ProcessEvents();
         CS_ReadFromNetwork();
      } while((new_tic = I_GetTime()) <= current_tic);
   }

   if(cl_received_sync && received_sync)
   {
      if(!net_peer)
      {
         CL_Reset();
         C_SetConsole();
         doom_printf("Lost connection to server.");
      }
      else if(((new_tic - current_tic) > (MAX_LATENCY * TICRATE)))
      {
         CL_Disconnect();
         if(received_sync)
            doom_printf("Synced the whole loop.");
         else if(cl_received_sync)
            doom_printf("Received sync just this last loop.");
         else
            doom_printf("Never synchronized.");
      }
   }
}

