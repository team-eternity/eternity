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

// [CG] Necessary for JSON parsing routines.
#include <string>

#include "z_zone.h"

#include <sys/stat.h>

#include <json/json.h>

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
#include "e_player.h"
#include "e_sound.h"
#include "e_states.h"
#include "e_things.h"
#include "e_ttypes.h"
#include "g_bind.h"
#include "g_game.h"
#include "hu_frags.h"
#include "hu_stuff.h"
#include "i_system.h"
#include "i_thread.h"
#include "m_argv.h"
#include "m_file.h"
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

#include "cs_announcer.h"
#include "cs_config.h"
#include "cs_demo.h"
#include "cs_hud.h"
#include "cs_main.h"
#include "cs_spec.h"
#include "cs_netid.h"
#include "cs_position.h"
#include "cs_wad.h"
#include "cl_buf.h"
#include "cl_cmd.h"
#include "cl_main.h"
#include "cl_net.h"
#include "cl_pred.h"
#include "cl_spec.h"

char *cs_server_url = NULL;
char *cs_server_password = NULL;
char *cs_client_password_file = NULL;
Json::Value cs_client_password_json;

unsigned int cl_current_world_index = 0;
unsigned int cl_latest_world_index  = 0;
unsigned int cl_commands_sent = 0;

bool cl_initial_spawn = true;
bool cl_received_first_sync = false;
bool cl_spawning_actor_from_message = false;
bool cl_removing_actor_from_message = false;
bool cl_setting_player_weapon_sprites = false;
bool cl_handling_damaged_actor = false;
bool cl_handling_damaged_actor_and_justhit = false;
bool cl_setting_actor_state = false;

cl_ghost_t cl_unlagged_ghosts[MAXPLAYERS];

extern int levelTimeLimit;
extern int levelFragLimit;
extern int levelScoreLimit;

extern bool d_fastrefresh;

extern unsigned int rngseed;

extern char gamemapname[9];
extern wfileadd_t *wadfiles;

struct seenstate_t;
extern seenstate_t *seenstates;

void CL_RunWorldUpdate(void)
{
   cl_packet_buffer.processPacketsForIndex(++cl_current_world_index);

   // [CG] If a player we're spectating spectates, reset our display.
   if(displayplayer != consoleplayer && clients[displayplayer].spectating)
      CS_SetDisplayPlayer(consoleplayer);

   if(gamestate == GS_LEVEL && cl_packet_buffer.synchronized() && !CS_DEMOPLAY)
      CL_SendCommand();

   G_Ticker();
   gametic++;
}

void CL_RunAllWorldUpdates(void)
{
   unsigned int old_world_index = cl_current_world_index;
   unsigned int new_world_index = cl_latest_world_index - 2;
   unsigned int old_commands_sent = cl_commands_sent;

   new_world_index -= cl_packet_buffer.capacity();

   while(cl_current_world_index < new_world_index)
      CL_RunWorldUpdate();

   if(new_world_index > old_world_index)
   {
      CL_PredictFrom(
         old_commands_sent,
         old_commands_sent + (new_world_index - old_world_index)
      );
   }
}

void CL_Init(char *url)
{
   int p;
   size_t url_length;
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
   cs_server_url = ecalloc(char *, url_length + 1, sizeof(char));
   if(strncmp(url, "eternity://", 11) == 0)
      sprintf(cs_server_url, "http://%s", url + 11);
   else
      memcpy(cs_server_url, url, url_length);

   // [CG] cURL supports millions of protocols, nonetheless, make sure that
   //      the passed URI is supported (and actually a URI).
   if(!CS_CheckURI(cs_server_url))
      I_Error("Invalid URI [%s], exiting.\n", cs_server_url);

   cs_client_password_file = ecalloc(
      char *, strlen(userpath) + strlen(PASSWORD_FILENAME) + 2, sizeof(char)
   );
   sprintf(cs_client_password_file, "%s/%s", userpath, PASSWORD_FILENAME);
   M_NormalizeSlashes(cs_client_password_file);

   if(M_PathExists((const char *)cs_client_password_file))
   {
      byte *buffer;
      std::string json_string;

      if(!M_IsFile((const char *)cs_client_password_file))
      {
         I_Error(
            "CL_Init: Password file %s exists but is not a file.\n",
            cs_client_password_file
         );
      }

      M_ReadFile(cs_client_password_file, &buffer);
      json_string += (const char *)buffer;
      if(!reader.parse(json_string, cs_client_password_json))
      {
         I_Error(
            "CL_Init: Error parsing password file %s: %s",
            cs_client_password_file,
            reader.getFormattedErrorMessages().c_str()
         );
      }
   }

   if((p = M_CheckParm("-csplaydemo")))
   {
      if(p >= (myargc - 2))
         I_Error("CL_Init: No demo file specified.\n");

      if(!cs_demo->play(myargv[p + 1]))
      {
         I_Error(
            "Error playing demo %s: %s.\n", myargv[p + 1], cs_demo->getError()
         );
      }
   }
   else if((M_CheckParm("-record")))
   {
      printf("CL_Init: Recording client/server demo.\n");

      if(!cs_demo->record())
         I_Error("Error recording demo: %s\n", cs_demo->getError());
   }
   else
   {
      CL_InitPrediction();
      CS_ZeroClients();
      CL_LoadConfig();
      CS_ClearTempWADDownloads();
      CS_InitSectorPositions();
   }
}

void CL_InitAnnouncer()
{
   CS_InitAnnouncer();
   CS_SetAnnouncer(s_announcer_type_names[s_announcer_type]);

   if(CS_TEAMS_ENABLED)
      CS_UpdateTeamSpecificAnnouncers(clients[consoleplayer].team);
}

void CL_InitPlayDemoMode(void)
{
   CL_InitPrediction();
   CS_ZeroClients();
   CS_InitSectorPositions();
}

char* CL_GetUserAgent(void)
{
   qstring buffer;
   buffer.Printf(28, "emp-client/%u.%u.%u-%u", version / 100,
                                               version % 100,
                                               (uint32_t)subversion,
                                               cs_protocol_version);
   return estrdup(buffer.constPtr());
}

void CL_Reset(void)
{
   cl_current_world_index = 0;
   cl_latest_world_index = 0;
   cl_commands_sent = 0;
   cl_initial_spawn = false;
   cl_received_first_sync = false;
   cl_packet_buffer.setSynchronized(false);
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
   efree(address);

   cl_packet_buffer.setSynchronized(false);

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
            CL_SendAuthMessage(
               cs_client_password_json[(const char *)cs_server_url].asCString()
            );
         }
      }
      else
         doom_printf("Connected!");

      if(!cs_client_password_json[(const char *)cs_server_url].empty())
      {
         CL_SendAuthMessage(
            cs_client_password_json[(const char *)cs_server_url].asCString()
         );
      }

      if(CS_DEMORECORD)
      {
         if(!cs_demo->addNewMap())
         {
            doom_printf(
               "Error adding new map to demo: %s.", cs_demo->getError()
            );
            if(!cs_demo->stop())
               doom_printf("Error stopping demo: %s.", cs_demo->getError());
         }
         else
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

   if(CS_DEMORECORD && !cs_demo->stop())
      printf("Error saving demo: %s\n", cs_demo->getError());

   CL_Reset();
   C_SetConsole();
   doom_printf("Disconnected.");
}

void CL_SaveServerPassword(void)
{
   cs_client_password_json[(const char *)cs_server_url] = cs_server_password;
   CS_WriteJSON(cs_client_password_file, cs_client_password_json, true);
}

void CL_MessageTeam(event_t *ev)
{
   CS_SetMessageMode(MESSAGE_MODE_TEAM);
}

void CL_MessageServer(event_t *ev)
{
   CS_SetMessageMode(MESSAGE_MODE_SERVER);
}

void CL_RCONMessage(event_t *ev)
{
   CS_SetMessageMode(MESSAGE_MODE_RCON);
}

void CL_SpawnLocalGhost(Mobj *actor)
{
   size_t i;
   mobjtype_t ptype = E_PlayerClassForName(GameModeInfo->defPClassName)->type;
   player_t *player = actor->player;

   if(!player)
   {
      doom_printf("No player for actor %u, not spawning ghost.", actor->net_id);
      return;
   }

   i = player - players;

   if(cl_unlagged_ghosts[i].local_ghost)
   {
      CL_RemoveMobj(cl_unlagged_ghosts[i].local_ghost);
      cl_unlagged_ghosts[i].local_ghost = NULL;
   }

   cl_unlagged_ghosts[i].local_ghost = CL_SpawnMobj(
      0, actor->x, actor->y, actor->z, ptype
   );
   cl_unlagged_ghosts[i].local_ghost->angle = actor->angle;
   cl_unlagged_ghosts[i].local_ghost->flags |= MF_NOCLIP;
   cl_unlagged_ghosts[i].local_ghost->flags |= MF_TRANSLUCENT;
   cl_unlagged_ghosts[i].local_ghost->flags &= ~MF_SHOOTABLE;
   if(actor == players[displayplayer].mo)
      cl_unlagged_ghosts[i].local_ghost->colour = 13;
   else
      cl_unlagged_ghosts[i].local_ghost->colour = 3;

   printf(
      "Local: (%u) %d/%d/%d/%d.\n",
      cl_current_world_index,
      actor->x >> FRACBITS,
      actor->y >> FRACBITS,
      actor->z >> FRACBITS,
      actor->angle / ANGLE_1
   );
}

void CL_SpawnRemoteGhost(unsigned int net_id, fixed_t x, fixed_t y, fixed_t z,
                         angle_t angle, unsigned int world_index)
{
   unsigned int i;
   Mobj *actor = NetActors.lookup(net_id);
   mobjtype_t ptype = E_PlayerClassForName(GameModeInfo->defPClassName)->type;

   if(!actor)
   {
      doom_printf("No actor for Net ID %u, not spawning ghost.", net_id);
      return;
   }

   if(!actor->player)
   {
      doom_printf("No player for actor %u, not spawning ghost.", net_id);
      return;
   }

   i = actor->player - players;

   if(cl_unlagged_ghosts[i].remote_ghost)
   {
      CL_RemoveMobj(cl_unlagged_ghosts[i].remote_ghost);
      cl_unlagged_ghosts[i].remote_ghost = NULL;
   }

   cl_unlagged_ghosts[i].remote_ghost = CL_SpawnMobj(0, x, y, z, ptype);
   cl_unlagged_ghosts[i].remote_ghost->angle = actor->angle;
   cl_unlagged_ghosts[i].remote_ghost->flags |= MF_NOCLIP;
   cl_unlagged_ghosts[i].remote_ghost->flags |= MF_TRANSLUCENT;
   cl_unlagged_ghosts[i].remote_ghost->flags &= ~MF_SHOOTABLE;
   if(actor == players[displayplayer].mo)
      cl_unlagged_ghosts[i].remote_ghost->colour = 1;
   else
      cl_unlagged_ghosts[i].remote_ghost->colour = 8;
   printf(
      "Remote: (%u) %d/%d/%d/%d.\n",
      world_index,
      x >> FRACBITS,
      y >> FRACBITS,
      z >> FRACBITS,
      angle / ANGLE_1
   );
}


Mobj* CL_SpawnMobj(uint32_t net_id, fixed_t x, fixed_t y, fixed_t z,
                   mobjtype_t type)
{
   Mobj *actor;

   cl_spawning_actor_from_message = true;
   actor = P_SpawnMobj(x, y, z, type);
   cl_spawning_actor_from_message = false;

   actor->net_id = net_id;

   if(net_id != 0)
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

Mobj* CL_SpawnPuff(uint32_t net_id, fixed_t x, fixed_t y, fixed_t z,
                   angle_t angle, int updown, bool ptcl)
{
   Mobj *puff;

   cl_spawning_actor_from_message = true;
   puff = P_SpawnPuff(x, y, z, angle, updown, ptcl);
   if(net_id)
   {
      puff->net_id = net_id;
      NetActors.add(puff);
   }
   cl_spawning_actor_from_message = false;

   return puff;
}

Mobj* CL_SpawnBlood(uint32_t net_id, fixed_t x, fixed_t y, fixed_t z,
                    angle_t angle, int damage, Mobj *target)
{
   Mobj *blood;

   cl_spawning_actor_from_message = true;
   blood = P_SpawnBlood(x, y, z, angle, damage, target);
   if(net_id)
   {
      blood->net_id = net_id;
      NetActors.add(blood);
   }
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
   cl_latest_world_index = index;

   // [CG] This means we haven't even completed a single TIC yet, so ignore the
   //      piled up messages we're receiving.
   if(cl_current_world_index == 0)
      return;

   // [CG] Can't flush during independent buffering.
   if(cl_packet_buffer.bufferingIndependently())
      return;

   // [CG] Can't flush if there's no TICs to run.
   if(cl_latest_world_index <= cl_current_world_index)
      return;

   if(cl_packet_buffer.overflowed())
      CL_RunAllWorldUpdates();
}

void CL_RunDemoTics(void)
{
   int current_tic;
   static int new_tic;
   static bool displayed_message = false;
   unsigned int new_index;

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

   if(cs_demo->isFinished())
   {
      if(!displayed_message)
      {
         HU_CenterMessage("Demo playback complete.\n");
         displayed_message = true;
         if(!cs_demo->stop())
            doom_printf("Error stopping demo: %s.", cs_demo->getError());
      }
      HU_Ticker();
   }
   else
   {
      displayed_message = false;

      new_index = cl_current_world_index;
      
      if(cs_demo_speed > cs_demo_speed_normal)
         new_index += cs_demo_speed_rates[cs_demo_speed];

      while(cl_latest_world_index <= new_index)
      {
         if(!cs_demo->readPacket())
         {
            bool is_finished = cs_demo->isFinished();

            if(is_finished)
               break;

            doom_printf("Error reading demo: %s.\n", cs_demo->getError());
            if(!cs_demo->stop())
               doom_printf("Error stopping demo: %s.", cs_demo->getError());

            return;
         }
      }

      while(cl_current_world_index < cl_latest_world_index)
         CL_RunWorldUpdate();

      if(cs_demo_speed < cs_demo_speed_normal)
         current_tic += cs_demo_speed_rates[cs_demo_speed];
   }

   do
   {
      I_StartTic();
      D_ProcessEvents();
   } while((new_tic = I_GetTime()) <= current_tic);
}

void CL_TryRunTics(void)
{
   int current_tic;
   bool received_sync = cl_packet_buffer.synchronized();
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

   if((!net_host) || (!net_peer))
   {
      while((!d_fastrefresh) && ((new_tic = I_GetTime()) == current_tic))
         I_Sleep(1);
      return;
   }

   CS_ChatTicker();
   CS_ReadFromNetwork(1);

   // [CG] The buffer absolutely cannot be flushed if we've not yet received
   //      sync, so only honor cl_packet_buffer.needsFlushing() if sync's been
   //      received.  Also always process all packets if the packet buffer's
   //      size is 1.
   if(cl_packet_buffer.synchronized() && !cl_packet_buffer.needsFilling())
   {
      if(cl_packet_buffer.needsFlushing() || !cl_packet_buffer.enabled())
      {
         CL_RunAllWorldUpdates();
         cl_packet_buffer.setNeedsFlushing(false);
      }

      CL_RunWorldUpdate();
   }

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

   if(cl_packet_buffer.synchronized() && received_sync)
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

