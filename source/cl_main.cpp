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

#include <sstream>
#include <iostream>
#include <fstream>

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
#include "e_ttypes.h"
#include "g_game.h"
#include "hu_frags.h"
#include "hu_stuff.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "mn_engin.h"
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
#include "v_misc.h"
#include "version.h"
#include "w_wad.h"

#include "cs_hud.h"
#include "cs_spec.h"
#include "cs_main.h"
#include "cs_config.h"
#include "cs_netid.h"
#include "cs_position.h"
#include "cs_demo.h"
#include "cs_wad.h"
#include "cl_buf.h"
#include "cl_cmd.h"
#include "cl_main.h"
#include "cl_net.h"
#include "cl_pred.h"
#include "cl_spec.h"

#include <json/json.h>

char *cs_server_url = NULL;
char *cs_server_password = NULL;
char *cs_client_password_file = NULL;
Json::Value cs_client_password_json;

unsigned int cl_current_world_index = 0;
unsigned int cl_latest_world_index  = 0;

bool cl_received_sync = false;
bool cl_initial_spawn = true;
bool cl_spawning_actor_from_message = false;
bool cl_removing_actor_from_message = false;
bool cl_setting_player_weapon_sprites = false;
bool cl_handling_damaged_actor = false;
bool cl_handling_damaged_actor_and_justhit = false;
bool cl_setting_actor_state = false;

extern int levelTimeLimit;
extern int levelFragLimit;
extern int levelScoreLimit;

extern bool d_fastrefresh;

extern unsigned int rngseed;

extern char gamemapname[9];
extern wfileadd_t *wadfiles;

struct seenstate_t;
extern seenstate_t *seenstates;

static void run_world(void)
{
   cl_packet_buffer.processPacketsForIndex(++cl_current_world_index);

   // [CG] If a player we're spectating spectates, reset our display.
   if(displayplayer != consoleplayer && clients[displayplayer].spectating)
      CS_SetDisplayPlayer(consoleplayer);

   if(gamestate == GS_LEVEL && cl_received_sync && !cs_demo_playback)
      CL_SendCommand();

   G_Ticker();
   gametic++;
}

static void flush_packet_buffer(void)
{
   unsigned int old_world_index = cl_current_world_index;
   while(cl_current_world_index < (cl_latest_world_index - 2))
      run_world();
   CL_PredictFrom(old_world_index, cl_latest_world_index - 2);
   cl_flush_packet_buffer = false;
}

void CL_Init(char *url)
{
   size_t url_length;
   struct stat sbuf;
   Json::Reader reader;

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
   cs_server_url = (char *)(calloc(url_length + 1, sizeof(char)));
   if(strncmp(url, "eternity://", 11) == 0)
      sprintf(cs_server_url, "http://%s", url + 11);
   else
      memcpy(cs_server_url, url, url_length);

   // [CG] cURL supports millions of protocols, nonetheless, make sure that
   //      the passed URI is supported (and actually a URI).
   if(!CS_CheckURI(cs_server_url))
      I_Error("Invalid URI [%s], exiting.\n", cs_server_url);

   cs_client_password_file = (char *)(calloc(
      strlen(basepath) + strlen(PASSWORD_FILENAME) + 2, sizeof(char)
   ));
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

      if(!reader.parse(cs_client_password_file, cs_client_password_json))
      {
         I_Error(
            "CL_Init: Error parsing password file: %s",
            reader.getFormattedErrorMessages().c_str()
         );
      }
   }

   CL_InitPrediction();
   CS_ZeroClients();
   CL_LoadConfig();
   CS_InitSectorPositions();
}

void CL_InitPlayDemoMode(void)
{
   CL_InitPrediction();
   CS_ZeroClients();
   CS_InitSectorPositions();
}

char* CL_GetUserAgent(void)
{
   std::stringstream user_agent_stream;

   user_agent_stream << "emp-client"           << "/"
                     << (version / 100)        << "."
                     << (version % 100)        << "."
                     << ((uint32_t)subversion) << "-"
                     << (cs_protocol_version);

   return strdup(user_agent_stream.str().c_str());
}

void CL_Reset(void)
{
   cl_current_world_index = 0;
   cl_latest_world_index = 0;
   cl_received_sync = false;
   consoleplayer = displayplayer = 0;
   CS_InitPlayers();
   CS_ZeroClients();
   memset(playeringame, 0, MAXPLAYERS * sizeof(bool));
   playeringame[0] = true;
}

void CL_Connect(void)
{
   ENetEvent event;
   char *address = CS_IPToString(server_address->host);

   printf("Connecting to %s:%d\n", address, server_address->port);
   free(address);

   net_peer = enet_host_connect(net_host, server_address, MAX_CHANNELS, 0);
   if(net_peer == NULL)
      doom_printf("Unknown error creating a new connection.\n");

   start_time = enet_time_get();

   if(enet_host_service(net_host, &event, CONNECTION_TIMEOUT) > 0 &&
      event.type == ENET_EVENT_TYPE_CONNECT)
   {
      CS_ClearNetIDs(); // [CG] Clear the Net ID hashes.

      // [CG] Automatically authenticate if possible.  If this server requires
      //      a password in order to connect as a spectator and we don't have a
      //      password stored for it, inform the client they need to enter one.
      if(cs_json["server"]["requires_spectator_password"].asBool())
      {
         if(cs_client_password_json[(const char *)cs_server_url].empty())
         {
            doom_printf(
               "This server requires a password in order to connect.  Use the "
               "'password' command to gain authorization.\n"
            );
         }
         else
         {
            doom_printf("Connected!");
            CL_SendAuthMessage(cs_client_password_json[
               (const char *)cs_server_url
            ].asCString());
         }
      }
      else
         doom_printf("Connected!");

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

void CL_SaveServerPassword(void)
{
   cs_client_password_json[(const char *)cs_server_url] = cs_server_password;
   CS_WriteJSON(cs_client_password_file, cs_client_password_json, true);
}

Mobj* CL_SpawnMobj(uint32_t net_id, fixed_t x, fixed_t y, fixed_t z,
                     mobjtype_t type)
{
   Mobj *actor;

   cl_spawning_actor_from_message = true;
   actor = P_SpawnMobj(x, y, z, type);
   cl_spawning_actor_from_message = false;

   actor->net_id = net_id;
   NetActors.add(actor);

   return actor;
}

void CL_RemoveMobj(Mobj *actor)
{
   cl_removing_actor_from_message = true;
   actor->removeThinker();
   cl_removing_actor_from_message = false;
}

void CL_SpawnPlayer(int playernum, uint32_t net_id, fixed_t x, fixed_t y,
                    fixed_t z, angle_t angle, bool as_spectator)
{
   if(players[playernum].mo != NULL)
      CL_RemoveMobj(players[(playernum)].mo);

   cl_spawning_actor_from_message = true;
   CS_SpawnPlayer(playernum, x, y, z, angle, as_spectator);
   cl_spawning_actor_from_message = false;

   NetActors.remove(players[playernum].mo);
   players[playernum].mo->net_id = net_id;
   NetActors.add(players[playernum].mo);
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

Mobj* CL_SpawnPuff(fixed_t x, fixed_t y, fixed_t z, angle_t angle,
                     int updown, bool ptcl)
{
   Mobj *puff;

   cl_spawning_actor_from_message = true;
   puff = P_SpawnPuff(x, y, z, angle, updown, ptcl);
   cl_spawning_actor_from_message = false;

   return puff;
}

Mobj* CL_SpawnBlood(fixed_t x, fixed_t y, fixed_t z, angle_t angle,
                      int damage, Mobj *target)
{
   Mobj *blood;

   cl_spawning_actor_from_message = true;
   blood = P_SpawnBlood(x, y, z, angle, damage, target);
   cl_spawning_actor_from_message = false;

   return blood;
}

bool CL_SetMobjState(Mobj* mobj, statenum_t state)
{
   bool out;

   cl_setting_actor_state = true;
   out = P_SetMobjState(mobj, state);
   cl_setting_actor_state = false;
   return out;
}

void CL_PlayerThink(int playernum)
{
   client_t *client = &clients[playernum];
   player_t *player = &players[playernum];
   Mobj     *actor  = player->mo;

   // [CG] Here we do some things clientside that are inconsequential to
   //      the game world, but are necessary so that all players (local
   //      or otherwise) "act" correctly.

   if(client->spectating) // [CG] Don't think on spectators clientside.
      return;

   // [CG] Move the player's weapon sprite.
   P_MovePsprites(player);

   // [CG] These shouldn't wait on server messages because they're
   //      annoying when they linger.
   if(player->damagecount)
      player->damagecount--;

   if(player->bonuscount)
      player->bonuscount--;

   // [CG] Make sounds when a player hits the floor.
   if(client->floor_status)
   {
      if(actor->health > 0)
      {
         if(!comp[comp_fallingdmg])
         {
            // haleyjd: new features -- feet sound for normal hits,
            //          grunt for harder, falling damage for worse
            if(actor->momz < -23 * FRACUNIT)
            {
               if(actor->player->powers[pw_invulnerability] ||
                  (actor->player->cheats & CF_GODMODE))
               {
                  S_StartSound(actor, GameModeInfo->playerSounds[sk_oof]);
               }
            }
            else if(actor->momz < -12 * FRACUNIT)
            {
               S_StartSound(actor, GameModeInfo->playerSounds[sk_oof]);
            }
            else if((client->floor_status == cs_fs_hit_on_thing) ||
                    !E_GetThingFloorType(actor, true)->liquid)
            {
               S_StartSound(actor, GameModeInfo->playerSounds[sk_plfeet]);
            }
         }
         else if((client->floor_status == cs_fs_hit_on_thing) ||
                 !E_GetThingFloorType(actor, true)->liquid)
         {
            S_StartSound(actor, GameModeInfo->playerSounds[sk_oof]);
         }
      }
      client->floor_status = cs_fs_none;
   }
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
      flush_packet_buffer();
   }
}

void CL_RunDemoTics(void)
{
   int current_tic;
   static int new_tic;
   static bool displayed_message = false;

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
            doom_printf("Error stopping demo: %s.", CS_GetDemoErrorMessage());
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
            bool is_finished = CS_DemoFinished();

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
   } while((new_tic = I_GetTime()) == current_tic);
}

void CL_TryRunTics(void)
{
   int current_tic;
   bool received_sync = cl_received_sync;
   static int new_tic;

   current_tic = new_tic;
   new_tic = I_GetTime();

   // [CG] Return early if d_fastrefresh is on and the TIC hasn't changed.
   if(d_fastrefresh && (current_tic == new_tic))
      return;

   // [CG] These go here to avoid super-fast speeds if d_fastrefresh is
   //      enabled.
   I_StartTic();
   D_ProcessEvents();
   MN_Ticker();
   C_Ticker();
   V_FPSTicker();
   CS_ReadFromNetwork(1);

   // [CG] Because worlds may be skipped during buffer flushes, it's necessary
   //      to pull damagecount and bonuscount decrementing out from the normal
   //      game loop.
   if(consoleplayer && players[consoleplayer].damagecount)
      players[consoleplayer].damagecount--;

   if(consoleplayer && players[consoleplayer].bonuscount)
      players[consoleplayer].bonuscount--;

   // [CG] The buffer absolutely cannot be flushed if we've not yet received
   //      sync, so only honor cl_flush_packet_buffer if sync's been received.
   if(cl_received_sync && cl_flush_packet_buffer)
      flush_packet_buffer();

   // [CG] When constant prediction is enabled, 1 world is always run each TIC.
   //      Otherwise run a world if the server finished an index.
   if(cl_constant_prediction)
      run_world();
   else if(cl_current_world_index < (cl_latest_world_index - 1))
      run_world();

   // [CG] For the rest of the TIC, try reading from the network.  Even though
   //      things happen "after" this, this functionally serves as the end of
   //      the main game loop.
   if(net_peer && !d_fastrefresh)
   {
      do
      {
         I_StartTic();
         D_ProcessEvents();
         CS_ReadFromNetwork(1);
      } while((new_tic = I_GetTime()) == current_tic);
   }

   if(cl_received_sync && received_sync)
   {
      if(!net_peer)
      {
         CL_Reset();
         C_SetConsole();
         doom_printf("Lost connection to server.");
      }
      else if((new_tic > current_tic) &&
              ((new_tic - current_tic) > (MAX_LATENCY * TICRATE)))
      {
         CL_Disconnect();
         doom_printf("Timed out: %u TICs expired.\n", new_tic - current_tic);
      }
   }
}

