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
//   C/S server routines.
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include <json/json.h>

#include "acs_intr.h"
#include "am_map.h"
#include "c_io.h"
#include "c_net.h"
#include "d_event.h"
#include "d_gi.h"
#include "d_player.h"
#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "e_player.h"
#include "e_states.h"
#include "e_things.h"
#include "g_dmflag.h"
#include "g_game.h"
#include "i_system.h"
#include "info.h"
#include "m_fixed.h"
#include "m_random.h"
#include "mn_engin.h"
#include "p_hubs.h"
#include "p_info.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_partcl.h"
#include "p_saveg.h"
#include "p_skin.h"
#include "p_spec.h"
#include "p_user.h"
#include "psnprntf.h"
#include "r_state.h"
#include "s_sound.h"
#include "v_misc.h"
#include "version.h"

#include "cs_config.h"
#include "cs_hud.h"
#include "cs_main.h"
#include "cs_master.h"
#include "cs_netid.h"
#include "cs_position.h"
#include "cs_team.h"
#include "cs_demo.h"
#include "cs_vote.h"
#include "cs_wad.h"
#include "sv_bans.h"
#include "sv_cmd.h"
#include "sv_main.h"
#include "sv_queue.h"
#include "sv_spec.h"
#include "sv_vote.h"

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <conio.h>
#endif

#define SHOULD_SEND_PACKET_TO(i, mt) ( \
   (((mt) == nm_initialstate) || \
    ((mt) == nm_currentstate) || \
    ((mt) == nm_authresult)   || \
    ((mt) == nm_mapstarted))  || \
   ((playeringame[(i)]) && \
    (server_clients[(i)].auth_level >= cs_auth_spectator) && \
    (server_clients[(i)].received_game_state)) \
)

extern int levelTimeLimit;
extern int levelFragLimit;
extern int levelScoreLimit;
extern unsigned int rngseed;
extern char gamemapname[9];
extern bool d_fastrefresh;

unsigned int sv_world_index = 0;
unsigned long sv_public_address = 0;
bool sv_should_send_new_map = false;
server_client_t server_clients[MAXPLAYERS];
int sv_randomize_maps = false;
bool sv_buffer_commands = false;
unsigned int sv_join_time_limit = 0;

const char *sv_spectator_password = NULL;
const char *sv_player_password = NULL;
const char *sv_moderator_password = NULL;
const char *sv_administrator_password = NULL;
const char *sv_access_list_filename = NULL;
const char *sv_default_access_list_filename = NULL;

static char *sv_user_agent = NULL;

static void send_any_packet(int playernum, void *data, size_t data_size,
                            uint8_t flags, uint8_t channel_id)
{
   uint32_t message_type = *((uint32_t *)data);
   ENetPeer *peer = SV_GetPlayerPeer(playernum);
   unsigned int server_index = sv_world_index;

   // [CG] Can't send packets to non-existent peers.
   if(peer == NULL)
      return;

   if(!SHOULD_SEND_PACKET_TO(playernum, message_type))
      return;

   switch(message_type)
   {
   case nm_initialstate:
      ((nm_initialstate_t *)(data))->player_number = playernum;
      break;
   case nm_currentstate:
   case nm_sync:
   case nm_mapstarted:
   case nm_mapcompleted:
   case nm_clientinit:
   case nm_authresult:
   case nm_clientstatus:
   case nm_playerposition:
   case nm_playerspawned:
   case nm_playerinfoupdated:
   case nm_playerweaponstate:
   case nm_playerremoved:
   case nm_playertouchedspecial:
   case nm_servermessage:
   case nm_playermessage:
   case nm_puffspawned:
   case nm_bloodspawned:
   case nm_actorspawned:
   case nm_actorposition:
   case nm_actormiscstate:
   case nm_actortarget:
   case nm_actorstate:
   case nm_actordamaged:
   case nm_actorkilled:
   case nm_actorremoved:
   case nm_lineactivated:
   case nm_monsteractive:
   case nm_monsterawakened:
   case nm_missilespawned:
   case nm_missileexploded:
   case nm_cubespawned:
#if 0
   case nm_specialspawned:
   case nm_specialstatus:
   case nm_specialremoved:
#endif
   case nm_sectorposition:
   case nm_announcerevent:
   case nm_vote:
   case nm_voteresult:
   case nm_ticfinished:
      break;
   case nm_clientrequest:
   case nm_playercommand:
   case nm_max_messages:
   default:
      I_Error(
         "Server sent a packet with unknown type %d, exiting.\n", message_type
      );
   }

   // [CG] The weird style here is so it's easy to omit message transmissions
   //      that you don't want printed out.
   if(
         message_type == nm_initialstate
      || message_type == nm_currentstate
      || message_type == nm_sync
      || message_type == nm_mapstarted
      || message_type == nm_mapcompleted
      || message_type == nm_authresult
      || message_type == nm_clientinit
      // || message_type == nm_clientstatus
      // || message_type == nm_playerposition
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
#if 0
      || message_type == nm_specialspawned
      // || message_type == nm_specialstatus
      || message_type == nm_specialremoved
#endif
      // || message_type == nm_sectorposition
      || message_type == nm_announcerevent
      || message_type == nm_vote
      || message_type == nm_voteresult
      // || message_type == nm_ticfinished
      || LOG_ALL_NETWORK_MESSAGES
      )
   {
      printf(
         "Sending [%s message] (%u) to %u at %u.\n",
         network_message_names[message_type],
         message_type,
         playernum,
         server_index
      );
   }
   enet_peer_send(
      peer, channel_id, enet_packet_create(data, data_size, flags)
   );
}

static void send_packet(int playernum, void *data, size_t data_size)
{
   send_any_packet(
      playernum, data, data_size, ENET_PACKET_FLAG_RELIABLE, RELIABLE_CHANNEL
   );
}

static void send_unreliable_packet(int playernum, void *data, size_t data_size)
{
   send_any_packet(
      playernum,
      data,
      data_size,
      ENET_PACKET_FLAG_UNSEQUENCED,
      UNRELIABLE_CHANNEL
   );
}

static void send_packet_to_team(int playernum, void *data, size_t data_size)
{
   int i;

   for(i = 1; i < MAX_CLIENTS; i++)
      if(i != playernum && clients[i].team == clients[playernum].team)
         send_packet(playernum, data, data_size);
}

static void broadcast_packet(void *data, size_t data_size)
{
   int i;

   for(i = 1; i < MAX_CLIENTS; i++)
      send_packet(i, data, data_size);
}

static void broadcast_unreliable_packet(void *data, size_t data_size)
{
   int i;

   for(i = 1; i < MAX_CLIENTS; i++)
      send_unreliable_packet(i, data, data_size);
}

static void broadcast_packet_excluding(int playernum, void *data,
                                       size_t data_size)
{
   int i;

   for(i = 1; i < MAX_CLIENTS; i++)
      if(i != playernum)
         send_packet(i, data, data_size);
}

static nm_servermessage_t* build_message(bool hud_msg, bool prepend_name,
                                         const char *fmt, va_list args)
{
   size_t total_size =
      sizeof(nm_servermessage_t) + (MAX_STRING_SIZE * sizeof(char));
   char *buffer = ecalloc(char *, 1, total_size);
   nm_servermessage_t *server_message = (nm_servermessage_t *)buffer;
   char *message = (char *)(buffer + sizeof(nm_servermessage_t));
   int message_length =
      pvsnprintf(message, MAX_STRING_SIZE - 1, fmt, args) + 1;

   server_message->message_type = nm_servermessage;
   server_message->world_index = sv_world_index;
   server_message->is_hud_message = hud_msg;
   server_message->prepend_name = prepend_name;
   server_message->length = message_length;

   printf("%s\n", message);
   return server_message;
}

static void send_message(int playernum, bool hud_msg, bool prepend_name,
                         const char *fmt, va_list args)
{
   send_packet(
      playernum,
      build_message(hud_msg, prepend_name, fmt, args),
      sizeof(nm_servermessage_t) + (MAX_STRING_SIZE * sizeof(char))
   );
}

static void broadcast_message(bool hud_msg, bool prepend_name,
                              const char *fmt, va_list args)
{
   broadcast_packet(
      build_message(hud_msg, prepend_name, fmt, args),
      sizeof(nm_servermessage_t) + (MAX_STRING_SIZE * sizeof(char))
   );
}

void SV_Init(void)
{
   char *address;
   unsigned int i;

   if(enet_initialize() != 0)
      I_Error("Could not initialize networking.\n");

   SV_AddCommands();
   SV_LoadConfig();

   net_host = enet_host_create(
      server_address, MAX_CLIENTS, MAX_CHANNELS, 0, 0
   );

   if(net_host == NULL)
      I_Error("Could not initialize server.\n");

   start_time = enet_time_get();

   address = CS_IPToString(server_address->host);
   printf(
      "SV_Init: Server listening on %s:%d.\n", address, server_address->port
   );
   efree(address);

   enet_socket_set_option(net_host->socket, ENET_SOCKOPT_NONBLOCK, 1);
   enet_host_compress_with_range_coder(net_host);

   for(i = 0; i < MAXPLAYERS; i++)
      M_QueueInit(&server_clients[i].commands);

   CS_ZeroClients();

   // [CG] Initialize sector positions.
   CS_InitSectorPositions();

   // [CG] Initialize CURL multi handle.
   SV_MultiInit();
   atexit(SV_CleanUp);

   strncpy(players[0].name, SERVER_NAME, strlen(SERVER_NAME) + 1);
   efree(default_name);
   default_name = ecalloc(char *, 7, sizeof(char));
   strncpy(default_name, "Player", 7);

   sv_access_list = new AccessList();
}

void SV_InitAnnouncer()
{
   CS_InitAnnouncer();
   CS_SetAnnouncer(s_announcer_type_names[s_announcer_type]);
}

// [CG] For atexit().
void SV_CleanUp(void)
{
   SV_MasterCleanup();
   enet_deinitialize();
}

char* SV_GetUserAgent(void)
{
   if(!sv_user_agent)
   {
      qstring buffer;
      buffer.Printf(28, "emp-server/%u.%u.%u-%u", version / 100,
                                                  version % 100,
                                                  (uint32_t)subversion,
                                                  cs_protocol_version);
      sv_user_agent = estrdup(buffer.constPtr());
   }
   return sv_user_agent;
}

ENetPeer* SV_GetPlayerPeer(int playernum)
{
   size_t i;
   server_client_t *server_client = &server_clients[playernum];

   for(i = 0; i < net_host->peerCount; i++)
      if(net_host->peers[i].connectID == server_client->connect_id)
         return &net_host->peers[i];

   return NULL;
}

unsigned int SV_GetPlayerNumberFromPeer(ENetPeer *peer)
{
   unsigned int i;

   for(i = 1; i < MAXPLAYERS; i++)
      if(peer->connectID == server_clients[i].connect_id)
         return i;

   return 0;
}

unsigned int SV_GetClientNumberFromAddress(ENetAddress *address)
{
   int i;

   for(i = 1; i < MAX_CLIENTS; i++)
   {
      if(address->host == server_clients[i].address.host &&
         address->port == server_clients[i].address.port)
      {
         return i;
      }
   }
   return 0;
}

mapthing_t* SV_GetCoopSpawnPoint(int playernum)
{
   int i;
   fixed_t x = 0;
   fixed_t y = 0;
   bool remove_solid = false;
   bool remove_tele_stomp = false;

   // [CG] FIXME: Apparently sometimes player->mo is NULL.
   if((players[playernum].mo->flags & MF_SOLID) == 0)
   {
      remove_solid = true;
      players[playernum].mo->flags |= MF_SOLID;
   }

   for(i = 0; i < MAXPLAYERS; i++)
   {
      x = playerstarts[i].x << FRACBITS;
      y = playerstarts[i].x << FRACBITS;

      if(P_CheckPosition(players[playernum].mo, x, y))
         break;
   }

   if(remove_solid)
      players[playernum].mo->flags &= ~MF_SOLID;

   if(GameType != gt_dm)
   {
      // [CG] TODO: When in coop mode, just telefrag the things on your spawn.
      //            This needs some work so you don't drop or lose all your
      //            stuff.
      if((players[playernum].mo->flags3 & MF3_TELESTOMP) == 0)
      {
         remove_tele_stomp = true;
         players[playernum].mo->flags3 |= MF3_TELESTOMP;
      }

      P_TeleportMove(players[playernum].mo, x, y, false);

      if(remove_tele_stomp)
         players[playernum].mo->flags3 &= ~MF3_TELESTOMP;
   }
   return &playerstarts[i];
}

mapthing_t* SV_GetDeathMatchSpawnPoint(int playernum)
{
   int i, j;
   bool remove_solid = false;
   int deathmatch_start_count = deathmatch_p - deathmatchstarts;

   if(deathmatch_start_count == 0)
   {
      doom_printf("No deathmatch starts, falling back to player starts.\n");
      return SV_GetCoopSpawnPoint(playernum);
   }

   if((players[playernum].mo->flags & MF_SOLID) == 0)
   {
      remove_solid = true;
      players[playernum].mo->flags |= MF_SOLID;
   }

   // [CG] Try 20 times to find an unoccupied spawn point for the player.
   for(i = 0; i < 20; i++)
   {
      j = P_Random(pr_dmspawn) % deathmatch_start_count;
      if(P_CheckPosition(players[playernum].mo,
                         deathmatchstarts[j].x << FRACBITS,
                         deathmatchstarts[j].y << FRACBITS))
         break;
   }

   if(remove_solid)
      players[playernum].mo->flags &= ~MF_SOLID;

   // [CG] Even if there was no good deathmatch spawn, we still use the 20th
   //      one tried... even if it means the player will probably be stuck.
   return &deathmatchstarts[j];
}

mapthing_t* SV_GetTeamSpawnPoint(int playernum)
{
   int i, j;
   fixed_t x, y;
   int color;
   mapthing_t *spawn_point;
   bool remove_solid = false;

   if(team_start_counts_by_team[clients[playernum].team] == 0)
   {
      doom_printf("No team starts, falling back to deathmatch starts.\n");
      return SV_GetDeathMatchSpawnPoint(playernum);
   }

   if((players[playernum].mo->flags & MF_SOLID) == 0)
   {
      remove_solid = true;
      players[playernum].mo->flags |= MF_SOLID;
   }

   color = clients[playernum].team;

   for(i = 0; i < 20; i++)
   {
      j = P_Random(pr_dmspawn) % team_start_counts_by_team[color];
      spawn_point = &team_starts_by_team[color][j];
      x = spawn_point->x;
      y = spawn_point->y;
      if(P_CheckPosition(players[playernum].mo, x, y))
         break;
   }

   if(remove_solid)
      players[playernum].mo->flags &= ~MF_SOLID;

   // [CG] Even if there was no good team spawn, we still use the 20th one
   //      tried... even if it means the player will probably be stuck.
   return spawn_point;
}

unsigned int SV_GetPlayingPlayerCount()
{
   int i;
   unsigned int playing_players = 0;

   for(i = 1; i < MAX_CLIENTS; i++)
   {
      if(playeringame[i] && clients[i].queue_level == ql_playing)
         playing_players++;
   }

   return playing_players;
}

bool SV_RoomInGame(int clientnum)
{
   int i;
   int tic_limit = sv_join_time_limit * TICRATE;
   unsigned int playing_players = 0;

   for(i = 1; i < MAX_CLIENTS; i++)
   {
      if(!playeringame[i])
         continue;

      if(i == clientnum)
         continue;

      if(clients[i].queue_level < ql_can_join)
         continue;

      if(clients[i].afk)
         continue;

      if(clients[i].queue_level == ql_playing)
         playing_players++;
      else if(server_clients[i].finished_waiting_in_queue_tic <= tic_limit)
         playing_players++;
   }

   if(playing_players < cs_settings->max_players)
      return true;

   return false;
}

// [CG] Both versions of SV_ConsoleTicker are from Odamex, licensed under the
//      GPL.

//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 2006-2010 by The Odamex Team.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//-----------------------------------------------------------------------------


#ifdef WIN32
void SV_ConsoleTicker(void)
{
   char ch;
   size_t len;
   static char *buffer = NULL;
   static bool wrote_prompt = false;

   // [CG] In general this is a little less safe than I would be, but it's
   //      faster than memset'ing all the time (which is what I would do).

   // [CG] I think that this buffer should be external, so that when other
   //      messages are printed to the console the buffer can be re-printed.

   if(buffer == NULL)
      buffer = ecalloc(char *, CONSOLE_INPUT_LENGTH, sizeof(char));

   if(!wrote_prompt)
   {
      printf("SERVER> ");
      wrote_prompt = true;
   }

   len = strlen(buffer);

   while((_kbhit() > 0) && (len < CONSOLE_INPUT_LENGTH - 1))
   {
      ch = (char)getch();

      // [CG] Handle character input, backspaces first
      if(ch == '\b' && len > 0)
      {
         buffer[--len] = 0;
         // john - backspace hack
         fwrite(&ch, 1, 1, stdout);
         ch = ' ';
         fwrite(&ch, 1, 1, stdout);
         ch = '\b';
      }
      else
      {
         buffer[len++] = ch;
      }
      buffer[len] = 0;

      // recalculate length
      len = strlen(buffer);

      // echo character back to user
      fwrite(&ch, 1, 1, stdout);
      fflush(stdout);
   }

   if(len && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
   {
      // echo newline back to user
      // ch = '\n';
      fwrite("\n", 1, 1, stdout);

      // [CG] Run the command.
      buffer[len - 1] = 0; // rip off the \n (or \r) and terminate
      C_RunTextCmd(buffer);
      fwrite("SERVER> ", 8, 1, stdout);
      fflush(stdout);
      buffer[0] = 0;
      len = 0;
   }
}

#else

void SV_ConsoleTicker(void)
{
   fd_set fdr;
   struct timeval tv;
   size_t buf_size, last_char;
   static char *buffer = NULL;
   unsigned int max_buf_size = CONSOLE_INPUT_LENGTH - 2;

   if(buffer == NULL)
      buffer = ecalloc(char *, CONSOLE_INPUT_LENGTH, sizeof(char));

   FD_ZERO(&fdr);
   FD_SET(0, &fdr);
   tv.tv_sec = 0;
   tv.tv_usec = 0;

   if(!select(1, &fdr, NULL, NULL, &tv))
      return;

   buf_size = strlen(buffer);

   // [CG] This now only writes a maximum of (CONSOLE_INPUT_LENGTH - 2)
   //      characters into the buffer; it leaves a spot for a '\n' or '\r' and
   //      a trailing '\0'.  If we really want to read on and on until a
   //      linebreak is entered, we have to implement a re-allocating buffer.
   //      Can't just keep writing into whatever junk memory comes after
   //      buffer.
   // [CG] read() returns the amount of data that was read, so if nothing was
   //      read we just return.
   if(read(0, buffer + buf_size, max_buf_size - (buf_size)) < 1)
      return;

   last_char = strlen(buffer) - 1;

   if(buffer[last_char] == '\n' || buffer[last_char] == '\r')
   {
      buffer[last_char] = 0; // rip off the /n and terminate
      C_RunTextCmd(buffer);
      memset(buffer, 0, CONSOLE_INPUT_LENGTH);
   }
}
#endif

void SV_LoadPlayerPositionAt(int playernum, unsigned int index)
{
   CS_SetPlayerPosition(
      playernum, &server_clients[playernum].positions[index % MAX_POSITIONS]
   );
}

void SV_LoadPlayerMiscStateAt(int playernum, unsigned int index)
{
   Mobj *actor = players[playernum].mo;

   CS_SetActorMiscState(
      actor, &server_clients[playernum].misc_states[index % MAX_POSITIONS]
   );
}

void SV_LoadCurrentPlayerPosition(int playernum)
{
   SV_LoadPlayerPositionAt(playernum, sv_world_index);
}

void SV_LoadCurrentPlayerMiscState(int playernum)
{
   SV_LoadPlayerMiscStateAt(playernum, sv_world_index);
}

// [CG] Currently this just does players and sectors.  We certainly don't
//      store positions for *ALL* actors - that would take up tons of memory
//      on huge maps.
void SV_StartUnlag(int playernum)
{
   int i;
   int j;
   unsigned int command_index = server_clients[playernum].command_world_index;
   unsigned int current_index = sv_world_index - 1;

   // [CG] Don't run for the server's spectator actor.
   if(playernum == 0)
      return;

   for(i = 1; i < MAX_CLIENTS; i++)
   {
      // [CG] At first, skipping the shooting player might not seem to make
      //      sense.  However there are two good reasons for this:
      //      1. New positions resulting from received player commands are
      //         applied in the server's most recent world.  So even if a
      //         command received at world 16 is sent for world 14, the
      //         player's resulting position will be for world 16.
      //      2. The player is shooting from a (clientside) predicted position,
      //         so moving her back in time is actually incorrect; her position
      //         when she shoots is not "lagged", the positions of her targets
      //         are.
      //      It's easier to think of this as reconstructing the world the
      //      player was seeing as precisely as possible.
      if(playeringame[i] && i != playernum)
      {
         Mobj *a = players[i].mo;
         server_client_t *sc = &server_clients[i];

         CS_SavePlayerPosition(&sc->saved_position, i, current_index);
         CS_SaveActorMiscState(&sc->saved_misc_state, a, current_index);
         SV_LoadPlayerPositionAt(i, command_index);
         SV_LoadPlayerMiscStateAt(i, command_index);
         // [CG] If the target player was dead during this index, don't let
         //      let them take damage for the old actor at the old position.
         // [CG] FIXME: playerstate really shouldn't be in positions.
         if(sc->player_states[command_index % MAX_POSITIONS] != PST_LIVE)
            a->flags4 |= MF4_NODAMAGE;
      }
   }

   for(j = 0; j < numsectors; j++)
      SV_LoadSectorPositionAt(j, command_index);
}

void SV_EndUnlag(int playernum)
{
   int i;
   int j;
   server_client_t *server_client = &server_clients[playernum];
   unsigned int command_index = server_client->command_world_index;
   unsigned int current_index = sv_world_index - 1;

   if(playernum == 0)
      return;

   for(i = 1; i < MAX_CLIENTS; i++)
   {
      if(playeringame[i] && i != playernum)
      {
         fixed_t added_momx, added_momy;
         Mobj *a = players[i].mo;
         server_client_t *sc = &server_clients[i];
         cs_player_position_t *old_position =
            &sc->positions[command_index % MAX_POSITIONS];

         // [CG] Check to see if thrust due to damage was applied to the player
         //      during unlagged.
         added_momx = a->momx - old_position->momx;
         added_momy = a->momy - old_position->momy;

         CS_SetPlayerPosition(i, &sc->saved_position);
         CS_SetActorMiscState(a, &sc->saved_misc_state);

         // [CG] If thrust was applied during unlagged, apply it to the
         //      ultimate position.
         players[i].mo->momx += added_momx;
         players[i].mo->momy += added_momy;
      }
   }

   for(j = 0; j < numsectors; j++)
      SV_LoadSectorPositionAt(j, current_index);
}

int SV_HandleClientConnection(ENetPeer *peer)
{
   int i;
   Json::Value *ban;
   server_client_t *server_client;
   char *address = CS_IPToString(peer->address.host);

   for(i = 1; i < MAX_CLIENTS; i++)
      if(!playeringame[i] && !server_clients[i].connecting)
         break;

   // [CG] No more client spots.
   if(i == MAX_CLIENTS)
   {
      efree(address);
      return 0;
   }

   CS_ZeroClient(i);

   server_client = &server_clients[i];

   server_client->connect_id = peer->connectID;
   memcpy(&server_client->address, &peer->address, sizeof(ENetAddress));

   if(sv_spectator_password == NULL)
   {
      if(sv_player_password == NULL)
         server_client->auth_level = cs_auth_player;
      else
         server_client->auth_level = cs_auth_spectator;
   }

   if((ban = sv_access_list->getBan(address)))
   {
      if(AccessList::banIsTemporary(ban))
      {
         SV_SendMessage(
            i,
            "Temporarily banned for %d minutes: %s (%s): %s",
            (*ban)["duration"].asInt(),
            (*ban)["name"].asCString(),
            address,
            (*ban)["reason"].asCString()
         );
      }
      else
      {
         SV_SendMessage(
            i,
            "Banned: %s (%s): %s",
            (*ban)["name"].asCString(),
            address,
            (*ban)["reason"].asCString()
         );
      }
      enet_host_flush(net_host);
      CS_ZeroClient(i);
      efree(address);
      return 0;
   }

   address = CS_IPToString(peer->address.host);
   doom_printf(
      "Player %d has connected (%u, %s:%u).",
      i,
      peer->connectID,
      address,
      peer->address.port
   );
   efree(address);

   server_clients[i].connecting = true;
   server_clients[i].current_request = scr_initial_state;

   return i;
}

bool SV_ServerEmpty()
{
   int i;

   for(i = 1; i < MAX_CLIENTS; i++)
      if(playeringame[i] || server_clients[i].connecting)
         return false;

   return true;
}

void SV_SendClientInfo(int playernum, int clientnum)
{
   nm_clientinit_t client_init_message;

   client_init_message.message_type = nm_clientinit;
   client_init_message.world_index = sv_world_index;
   client_init_message.client_number = clientnum;
   memcpy(&client_init_message.client, &clients[clientnum], sizeof(client_t));

   send_packet(
      playernum, &client_init_message, sizeof(nm_clientinit_t)
   );
}

void SV_SpawnPlayer(int playernum, bool as_spectator)
{
   Mobj *fog;
   mapthing_t *spawn_point = CS_SpawnPlayerCorrectly(playernum, as_spectator);

   if((!as_spectator) && (clients[playernum].queue_level != ql_playing))
      SV_QueueSetClientPlaying(playernum);

   SV_BroadcastPlayerSpawned(spawn_point, playernum);

   // [CG] Only create spawn fog for non-spectators.
   if(!as_spectator)
   {
      fog = G_SpawnFog(
         spawn_point->x << FRACBITS,
         spawn_point->y << FRACBITS,
         spawn_point->angle
      );
      SV_BroadcastActorSpawned(fog);
      S_StartSound(fog, GameModeInfo->teleSound);
   }
}

void SV_BroadcastNewClient(int clientnum)
{
   nm_clientinit_t client_init_message;

   client_init_message.message_type = nm_clientinit;
   client_init_message.world_index = sv_world_index;
   client_init_message.client_number = clientnum;
   memcpy(&client_init_message.client, &clients[clientnum], sizeof(client_t));

   broadcast_packet_excluding(
      clientnum, &client_init_message, sizeof(nm_clientinit_t)
   );
}

void SV_SendCurrentState(int playernum)
{
   int i;
   nm_playerspawned_t spawn_message;
   mapthing_t *spawn_point;
   byte *buffer;
   size_t message_size;

   printf(
      "SV_SendCurrentState (%u): New player %d.\n", sv_world_index, playernum
   );

   clients[playernum].join_tic = gametic;

   spawn_point = CS_SpawnPlayerCorrectly(playernum, true);
   spawn_message.message_type = nm_playerspawned;
   spawn_message.world_index = sv_world_index;
   spawn_message.player_number = playernum;
   spawn_message.net_id = players[playernum].mo->net_id;
   spawn_message.as_spectator = true;
   spawn_message.x = spawn_point->x;
   spawn_message.y = spawn_point->y;
   spawn_message.z = ONFLOORZ;
   spawn_message.angle = spawn_point->angle;

   // [CG] Tell all the other clients to spawn a new spectator actor for this
   //      player.  Exclude the new client so that it doesn't spawn itself
   //      twice.
   broadcast_packet_excluding(
      playernum, &spawn_message, sizeof(nm_playerspawned_t)
   );

   // [CG] Now that we're ready for the client, send it the game's state.
   playeringame[playernum] = true;
   message_size = CS_BuildGameState(playernum, &buffer);
   send_packet(playernum, buffer, message_size);
   efree(buffer);

   server_clients[playernum].received_game_state = true;

   // [CG] Send client initialization info for all connected clients to the new
   //      client.
   for(i = 1; i < MAX_CLIENTS; i++)
   {
      if(playeringame[i] && i != playernum)
         SV_SendClientInfo(playernum, i);
   }

   if(CS_TEAMS_ENABLED)
      clients[playernum].team = team_color_none;

   // [CG] Send info on the new client to all the other clients.
   SV_BroadcastNewClient(playernum);
}

void SV_SendInitialState(int playernum)
{
   nm_initialstate_t message;

   message.message_type = nm_initialstate;
   message.world_index = sv_world_index;
   message.map_number = cs_current_map_number;
   message.rngseed = rngseed;
   message.player_number = playernum;
   memcpy(&message.settings, cs_settings, sizeof(clientserver_settings_t));

   send_packet(playernum, &message, sizeof(nm_initialstate_t));
}

void SV_DisconnectPlayer(int playernum, disconnection_reason_e reason)
{
   Mobj *flash;
   player_t *player = &players[playernum];
   ENetPeer *peer = SV_GetPlayerPeer(playernum);

   SV_QueueSetClientRemoved(playernum);

   if(reason > dr_max_reasons)
      I_Error("Invalid disconnection reason index %d.\n", reason);
   else if(reason == dr_no_reason)
      doom_printf("Player %d disconnected.", playernum);
   else
   {
      doom_printf(
         "Disconnecting player %d: %s.",
         playernum,
         disconnection_strings[reason]
      );
   }

   SV_BroadcastPlayerRemoved(playernum, reason);

   CS_DropFlag(playernum);

   if(gamestate == GS_LEVEL && player->mo != NULL)
   {
      if(!clients[playernum].spectating)
      {
         flash = P_SpawnMobj(
            player->mo->x,
            player->mo->y,
            player->mo->z + GameModeInfo->teleFogHeight,
            GameModeInfo->teleFogType
         );

         flash->momx = player->mo->momx;
         flash->momy = player->mo->momy;

         SV_BroadcastActorSpawned(flash);

         if(drawparticles)
         {
            flash->flags2 |= MF2_DONTDRAW;
            P_DisconnectEffect(player->mo);
         }
      }

      SV_BroadcastActorRemoved(player->mo);
      player->mo->removeThinker();
      player->mo = NULL;
   }

   CS_ZeroClient(playernum);
   playeringame[playernum] = false;
   CS_InitPlayer(playernum);
   CS_DisconnectPeer(peer, reason);
}

void SV_SendMessage(int playernum, const char *fmt, ...)
{
   va_list args;

   va_start(args, fmt);
   send_message(playernum, false, false, fmt, args);
   va_end(args);
}

void SV_SendHUDMessage(int playernum, const char *fmt, ...)
{
   va_list args;

   va_start(args, fmt);
   send_message(playernum, true, false, fmt, args);
   va_end(args);
}

void SV_BroadcastMessage(const char *fmt, ...)
{
   va_list args;

   va_start(args, fmt);
   broadcast_message(false, false, fmt, args);
   va_end(args);
}

void SV_BroadcastHUDMessage(const char *fmt, ...)
{
   va_list args;

   va_start(args, fmt);
   broadcast_message(true, false, fmt, args);
   va_end(args);
}

void SV_Say(const char *fmt, ...)
{
   va_list args;

   va_start(args, fmt);
   broadcast_message(false, true, fmt, args);
   va_end(args);
}

void SV_SayToPlayer(int playernum, const char *fmt, ...)
{
   va_list args;

   va_start(args, fmt);
   send_message(playernum, false, true, fmt, args);
   va_end(args);
}

void SV_SendSync(int playernum)
{
   nm_sync_t message;

   server_clients[playernum].connecting = false;

   message.message_type = nm_sync;
   message.world_index = sv_world_index;
   message.gametic = gametic;
   message.levelstarttic = levelstarttic;
   message.basetic = basetic;
   message.leveltime = leveltime;
   send_packet(playernum, &message, sizeof(nm_sync_t));
}

void SV_BroadcastMapStarted(void)
{
   nm_mapstarted_t message;

   message.message_type = nm_mapstarted;
   message.world_index = sv_world_index;
   memcpy(&message.settings, cs_settings, sizeof(clientserver_settings_t));

   broadcast_packet(&message, sizeof(nm_mapstarted_t));
}

void SV_BroadcastMapCompleted(bool enter_intermission)
{
   nm_mapcompleted_t message;

   message.message_type = nm_mapcompleted;
   message.world_index = sv_world_index;
   message.new_map_number = cs_current_map_number;
   message.enter_intermission = enter_intermission;

   broadcast_packet(&message, sizeof(nm_mapcompleted_t));
}

void SV_SendAuthorizationResult(int playernum,
                                bool authorization_successful,
                                cs_auth_level_e authorization_level)
{
   nm_authresult_t auth_result;

   auth_result.message_type = nm_authresult;
   auth_result.world_index = sv_world_index;
   auth_result.authorization_successful = authorization_successful;
   auth_result.authorization_level = authorization_level;

   send_packet(playernum, &auth_result, sizeof(nm_authresult_t));
}

bool SV_AuthorizeClient(int playernum, const char *password)
{
   size_t length;
   server_client_t *server_client = &server_clients[playernum];
   cs_auth_level_e auth_level = cs_auth_none;

   if(server_client->last_auth_attempt + TICRATE > gametic)
   {
      // [CG] Only allow one authorization attempt per second to hamper brute
      //      forcing.  Maybe it would be good to keep track of attempts made
      //      during this ignorance period, and disconnect the peer after a
      //      certain threshold.
#if _AUTH_DEBUG
      printf(
         "SV_AuthorizeClient: Too many auth attempts from client %d, "
         "ignoring (%u/%u).\n",
         playernum,
         server_client->last_auth_attempt,
         gametic
      );
#endif
      return false;
   }

   server_client->last_auth_attempt = gametic;

   if(sv_spectator_password == NULL)
   {
#if _AUTH_DEBUG
      printf("Spectator password was NULL, escalating to spectator.\n");
#endif
      auth_level = cs_auth_spectator;
   }
   else
   {
      length = strlen(sv_spectator_password);
      if(strncmp(sv_spectator_password, password, length) == 0)
      {
#if _AUTH_DEBUG
         printf("Client authorization: spectator.\n");
#endif
         auth_level = cs_auth_spectator;
      }
#if _AUTH_DEBUG
      else
      {
         printf("Client failed spectator authorization.\n");
      }
#endif
   }

   if(sv_player_password == NULL)
   {
#if _AUTH_DEBUG
      printf("Player password was NULL, escalating to player.\n");
#endif
      auth_level = cs_auth_player;
   }
   else
   {
      length = strlen(sv_player_password);
      if(strncmp(sv_player_password, password, length) == 0)
      {
#if _AUTH_DEBUG
         printf("Client authorization: player.\n");
#endif
         auth_level = cs_auth_player;
      }
#if _AUTH_DEBUG
      else
      {
         printf("Client failed player authorization.\n");
      }
#endif
   }

   length = strlen(sv_moderator_password);
   if(strncmp(sv_moderator_password, password, length) == 0)
   {
#if _AUTH_DEBUG
      printf("Client authorization: moderator.\n");
#endif
      auth_level = cs_auth_moderator;
   }
#if _AUTH_DEBUG
   else
   {
      printf("Client failed moderator authorization.\n");
   }
#endif

   length = strlen(sv_administrator_password);
   if(strncmp(sv_administrator_password, password, length) == 0)
   {
#if _AUTH_DEBUG
      printf("Client authorization: administrator.\n");
#endif
      auth_level = cs_auth_administrator;
   }
#if _AUTH_DEBUG
   else
   {
      printf("Client failed administrator authorization.\n");
   }
#endif

   // [CG] Bad password attempts don't strip you of your current authorization
   //      level.
   if(auth_level > server_client->auth_level)
   {
#if _AUTH_DEBUG
      printf(
         "Escalating client authorization level from %d to %d.\n",
         server_client->auth_level,
         auth_level
      );
#endif
      server_client->auth_level = auth_level;
      return true;
   }
#if _AUTH_DEBUG
   printf(
      "Leaving client authorization at %d.\n", server_client->auth_level
   );
#endif
   return false;
}

void SV_UpdateClientStatuses(void)
{
   int i;

   ENetPeer *peer;
   client_t *client;

   for(i = 1; i < MAX_CLIENTS; i++)
   {
      peer = SV_GetPlayerPeer(i);
      if(peer == NULL)
         return;

      client = &clients[i];

      client->transit_lag = peer->roundTripTime;
      client->packet_loss =
         (int)((peer->packetLoss / (float)ENET_PEER_PACKET_LOSS_SCALE) * 100);

      if(client->packet_loss > 100)
         client->packet_loss = 100;
   }
}

void SV_BroadcastPlayerPositions(void)
{
   int i;
   nm_playerposition_t message;
   cs_player_position_t *pos;
   cs_misc_state_t *ms;
   client_t *client;
   player_t *player;
   server_client_t *sc;

   for(i = 1; i < MAX_CLIENTS; i++)
   {
      client = &clients[i];
      player = &players[i];
      sc = &server_clients[i];

      if(!playeringame[i] || player->mo == NULL)
         continue;

      // [CG] Save position & misc state for unlagged.
      pos = &sc->positions[sv_world_index % MAX_POSITIONS];
      ms = &sc->misc_states[sv_world_index % MAX_POSITIONS];
      CS_SavePlayerPosition(pos, i, sv_world_index);
      CS_SaveActorMiscState(ms, player->mo, sv_world_index);
      sc->player_states[sv_world_index % MAX_POSITIONS] = player->playerstate;

      message.message_type = nm_playerposition;
      message.world_index = sv_world_index;
      message.player_number = i;
      CS_SavePlayerPosition(&message.position, i, sv_world_index);
      message.floor_status = client->floor_status;
      message.last_index_run = sc->last_command_run_index;
      message.last_world_index_run = sc->last_command_run_world_index;

      if(sc->buffering)
         broadcast_packet(&message, sizeof(nm_playerposition_t));
      else
         broadcast_unreliable_packet(&message, sizeof(nm_playerposition_t));
      client->floor_status = cs_fs_none;
   }
}

void SV_BroadcastUpdatedActorPositionsAndMiscState(void)
{
   Mobj *mobj;
   Thinker *think;

   for(think = thinkercap.next; think != &thinkercap; think = think->next)
   {
      if(!(mobj = thinker_cast<Mobj *>(think)))
         continue;

      // [CG] Don't save/broadcast carried flag actor positions, they're always
      //      stuck to their carrier.
      if(CS_ActorIsCarriedFlag(mobj))
         continue;

      if(mobj->player == NULL && CS_ActorPositionChanged(mobj))
      {
         CS_SaveActorPosition(&mobj->old_position, mobj, sv_world_index);

         // [CG] Let clients move missiles on their own.
         if((mobj->flags & MF_MISSILE) == 0)
            SV_BroadcastActorPosition(mobj, sv_world_index);
      }

      if(CS_ActorMiscStateChanged(mobj))
      {
         CS_SaveActorMiscState(&mobj->old_misc_state, mobj, sv_world_index);
         SV_BroadcastActorMiscState(mobj, sv_world_index);
      }
   }
}

void SV_SetPlayerTeam(int playernum, int team)
{
   client_t *client = &clients[playernum];

   if(client->team != team)
   {
      client->team = team;
      SV_BroadcastPlayerScalarInfo(playernum, ci_team);
      SV_SpawnPlayer(playernum, true);
   }
}

void SV_SetWeaponPreference(int playernum, int slot, weapontype_t weapon)
{
   int i;
   server_client_t *server_client = &server_clients[playernum];

   for(i = 0; i < NUMWEAPONS; i++)
   {
      if(server_client->weapon_preferences[i] == weapon)
         break;
   }
   server_client->weapon_preferences[i] =
      server_client->weapon_preferences[slot];
   server_client->weapon_preferences[slot] = weapon;
}

bool SV_HandleJoinRequest(int playernum)
{
   client_t *client = &clients[playernum];
   server_client_t *server_client = &server_clients[playernum];
   player_t *player = &players[playernum];
   unsigned int team_player_count;

   if(client->afk)
   {
      client->afk = false;
      SV_BroadcastPlayerScalarInfo(playernum, ci_afk);
   }

   if(server_client->auth_level < cs_auth_player)
   {
      // [CG] Client has insufficient authorization.
      SV_SendHUDMessage(playernum, "Unauthorized.");
      return false;
   }

   // [CG] If the client isn't yet in the queue, put them there (assign them a 
   //      queue position and the attendant queue level).
   if(client->queue_level == ql_none)
      SV_PutClientInQueue(playernum);

   // [CG] If the client is still in line, they can't join the game yet.
   if(client->queue_level == ql_waiting || !SV_RoomInGame(playernum))
   {
      SV_SendHUDMessage(playernum, "No open slots.");
      return false;
   }

   // [CG] If the client has gone through the queue (or there wasn't anyone
   //      else in line), then let them join the game.
   if(!CS_TEAMS_ENABLED)
   {
      SV_BroadcastMessage("%s has entered the game!", player->name);
      return true;
   }

   // [CG] For team modes we need to check that they can actually join the game
   //      on this team.

   // [CG] Can't join the game if not yet on a team.
   if(client->team == team_color_none)
   {
      SV_SendHUDMessage(playernum, "Pick a team first.");
      return false;
   }

   team_player_count = CS_GetTeamPlayerCount(client->team);

   // [CG] Check if there is room on the team.
   if(team_player_count > cs_settings->max_players_per_team)
   {
      SV_SendHUDMessage(playernum, "Team is full.");
      return false;
   }

   // [CG] Joining the game on this team is a go.
   SV_BroadcastMessage(
      "%s has entered the game on the %s team!",
      player->name,
      team_color_names[client->team]
   );
   return true;
}

void SV_HandlePlayerMessage(char *data, size_t data_length, int playernum)
{
   player_t *player;
   client_t *client;
   server_client_t *server_client;
   bool authorization_successful = false;
   nm_playermessage_t *player_message = (nm_playermessage_t *)data;
   char *message = NULL;

   // [CG] If the client still awaits authorization, then the message won't
   //      have a valid sender_number because it's never been assigned one.  So
   //      this check is only run for non-auth messages.
   if(player_message->recipient_type != mr_auth &&
      player_message->sender_number  != playernum)
   {
      SV_DisconnectPlayer(playernum, dr_invalid_message);
      return;
   }

   if(player_message->sender_number > MAXPLAYERS ||
      !playeringame[player_message->sender_number] ||
      !playeringame[player_message->recipient_number])
   {
      SV_SendMessage(playernum, "Invalid message.\n");
      return;
   }

   message = CS_ExtractMessage(data, data_length);
   if(message == NULL)
   {
      SV_DisconnectPlayer(playernum, dr_invalid_message);
      return;
   }

   player_message->world_index = sv_world_index;

   player = &players[playernum];
   client = &clients[playernum];
   server_client = &server_clients[playernum];

   switch(player_message->recipient_type)
   {
   case mr_vote:
      if(!strncasecmp(message, "yea", 3))
      {
         if(SV_CastBallot(playernum, Ballot::yea_vote))
         {
            doom_printf("%s voted yes.", player->name);
            broadcast_packet(data, data_length);
         }
      }
      else if(!strncasecmp(message, "nay", 3))
      {
         if(SV_CastBallot(playernum, Ballot::nay_vote))
         {
            doom_printf("%s voted no.", player->name);
            broadcast_packet(data, data_length);
         }
      }
      break;
   case mr_auth:
      doom_printf(
         "Received authorization request from player %d.\n", playernum
      );
      authorization_successful = SV_AuthorizeClient(playernum, message);
      SV_SendAuthorizationResult(
         playernum, authorization_successful, server_client->auth_level
      );
      break;
   case mr_rcon:
      doom_printf("Received RCON message from %s.\n", players[playernum].name);
      C_RunRCONTextCmd(data + sizeof(nm_playermessage_t), playernum);
      break;
   case mr_server:
      doom_printf(
         "%s: %s",
         players[player_message->sender_number].name,
         data + sizeof(nm_playermessage_t)
      );
      break;
   case mr_player:
      // [CG] Forwards the message to the receiving player.
      send_packet(player_message->recipient_number, data, data_length);
      break;
   case mr_team:
      // [CG] Forwards the message to the player's teammates.
      send_packet_to_team(player_message->recipient_number, data, data_length);
      break;
   case mr_all:
      // [CG] Forwards the message to all other players.
      doom_printf(
         "%s: %s",
         players[player_message->sender_number].name,
         data + sizeof(nm_playermessage_t)
      );
      broadcast_packet_excluding(
         player_message->recipient_number, data, data_length
      );
      break;
   default:
      doom_printf("Received message with unknown recipient type.");
      break;
   }
   efree(message);
}

void SV_HandleUpdatePlayerInfoMessage(char *data, size_t data_length,
                                      int playernum)
{
   // [CG] This ensures that players can only send info updates for themselves.
   nm_playerinfoupdated_t *info_message = (nm_playerinfoupdated_t *)data;

   if(info_message->player_number != playernum)
   {
      SV_SendMessage(
         playernum, "You cannot send info updates for other players!\n"
      );
   }
   else
   {
      CS_HandleUpdatePlayerInfoMessage(info_message);
   }
}

static void SV_findNextShuffledMap(void)
{
   static unsigned int count = 1;
   unsigned int i;

   if(++count > cs_map_count)
   {
      count = 1;
      for(i = 0; i < cs_map_count; i++)
         cs_maps[i].used = false;
   }
   else
      cs_maps[cs_current_map_index].used = true;

   do
   {
      cs_current_map_index = (M_Random() % cs_map_count);
   } while (cs_maps[cs_current_map_index].used);
}

void SV_AdvanceMapList(void)
{
   if(dmflags & dmf_exit_to_same_level)
      return;

   if(sv_randomize_maps == map_randomization_random)
      cs_current_map_index = (M_Random() % cs_map_count);
   else if(sv_randomize_maps == map_randomization_shuffle)
      SV_findNextShuffledMap();
   else if(++cs_current_map_index >= cs_map_count)
      cs_current_map_index = 0;

   cs_current_map_number = cs_current_map_index;
}

void SV_LoadClientOptions(int playernum)
{
   server_client_t *server_client = &server_clients[playernum];

   player_bobbing      = server_client->options.player_bobbing;
   doom_weapon_toggles = server_client->options.doom_weapon_toggles;
   autoaim             = server_client->options.autoaim;
   weapon_speed        = server_client->options.weapon_speed;
}

void SV_RestoreServerOptions(void)
{
   server_client_t *server_client = &server_clients[consoleplayer];

   player_bobbing      = server_client->options.player_bobbing;
   doom_weapon_toggles = server_client->options.doom_weapon_toggles;
   autoaim             = server_client->options.autoaim;
   weapon_speed        = server_client->options.weapon_speed;
}

#if 0
void SV_BroadcastGameState(void)
{
   byte *buffer;
   size_t message_size = CS_BuildGameState(0, &buffer);

   broadcast_packet(buffer, message_size);
   efree(buffer);
}
#endif

void SV_BroadcastPlayerSpawned(mapthing_t *spawn_point, int playernum)
{
   nm_playerspawned_t spawn_message;

   spawn_message.message_type = nm_playerspawned;
   spawn_message.world_index = sv_world_index;
   spawn_message.player_number = playernum;
   spawn_message.net_id = players[playernum].mo->net_id;
   spawn_message.as_spectator = clients[playernum].spectating;
   spawn_message.x = spawn_point->x;
   spawn_message.y = spawn_point->y;
   spawn_message.z = ONFLOORZ;
   spawn_message.angle = spawn_point->angle;

   broadcast_packet(&spawn_message, sizeof(nm_playerspawned_t));
}

void SV_BroadcastPlayerStringInfo(int playernum, client_info_e info_type)
{
   nm_playerinfoupdated_t *update_message;
   size_t buffer_size = CS_BuildPlayerStringInfoPacket(
      &update_message, playernum, info_type
   );

   update_message->world_index = sv_world_index;

   broadcast_packet(update_message, buffer_size);
   efree(update_message);
}

void SV_BroadcastPlayerArrayInfo(int playernum, client_info_e info_type,
                                 int array_index)
{
   nm_playerinfoupdated_t update_message;

   CS_BuildPlayerArrayInfoPacket(
      &update_message, playernum, info_type, array_index
   );

   update_message.world_index = sv_world_index;

   broadcast_packet(&update_message, sizeof(nm_playerinfoupdated_t));
}

void SV_BroadcastPlayerScalarInfo(int playernum, client_info_e info_type)
{
   nm_playerinfoupdated_t update_message;

   CS_BuildPlayerScalarInfoPacket(&update_message, playernum, info_type);

   update_message.world_index = sv_world_index;

   broadcast_packet(&update_message, sizeof(nm_playerinfoupdated_t));
}

unsigned int SV_ClientCommandBufferSize(int playernum)
{
   client_t *client = &clients[playernum];

   if(!sv_buffer_commands)
      return 0;

   if(!client->packet_loss)
      return 0;

   return (client->packet_loss / 2) + (client->transit_lag / 99) + 1;
}

void SV_RunPlayerCommand(int playernum, cs_buffered_command_t *bufcmd)
{
   server_client_t *server_client = &server_clients[playernum];
   ticcmd_t ticcmd;

   SV_LoadClientOptions(playernum);
   server_client->command_world_index = bufcmd->command.world_index;
   CS_CopyCommandToTiccmd(&ticcmd, &bufcmd->command);
   CS_RunPlayerCommand(playernum, &ticcmd, true);
   server_client->last_command_run_index = bufcmd->command.index;
   server_client->last_command_run_world_index = bufcmd->command.world_index;
   SV_RestoreServerOptions();
}

void SV_RunPlayerCommands(int playernum)
{
   cs_buffered_command_t *bufcmd;
   server_client_t *sc = &server_clients[playernum];
   uint32_t command_buffer_size = SV_ClientCommandBufferSize(playernum);
   uint32_t commands_run = 0;

   if(M_QueueIsEmpty(&sc->commands))
      return;

   if(!sc->command_buffer_filled)
   {
      if(sc->commands.size >= command_buffer_size)
         sc->command_buffer_filled = true;
      else
         return;
   }

   // [CG] Run at least 1 command.  If the buffer's overflowed, run all the
   //      extra commands until we get back to the normal size.
   do
   {
      bufcmd = (cs_buffered_command_t *)M_QueuePop(&sc->commands);
      SV_RunPlayerCommand(playernum, bufcmd);
      efree(bufcmd);
      commands_run++;
   } while((sc->commands.size > command_buffer_size) && (commands_run <= 2));
}

void SV_HandlePlayerCommandMessage(char *data, size_t data_length,
                                   int playernum)
{
   nm_playercommand_t *message = (nm_playercommand_t *)data;
   cs_cmd_t *commands = (cs_cmd_t *)(data + sizeof(nm_playercommand_t));
   server_client_t *server_client = &server_clients[playernum];
   uint32_t last_index = server_client->last_command_received_index;
   cs_buffered_command_t *bc;
   cs_cmd_t *command;

   // [CG] Don't accept commands if we're not in GS_LEVEL.
   if(gamestate != GS_LEVEL)
      return;

   server_client->received_command_for_current_map = true;

   for(command = commands;
       (command - commands) < message->command_count;
       command++)
   {
      // [CG] Skip messages we've seen before.
      if(command->index <= last_index)
         continue;

      // [CG] Queue this command to be run later.
      bc = ecalloc(cs_buffered_command_t *, 1, sizeof(cs_buffered_command_t));
      CS_CopyCommand(&bc->command, command);
      M_QueueInsert((mqueueitem_t *)bc, &server_client->commands);
      server_client->last_command_received_index = command->index;
   }
}

void SV_HandleClientRequestMessage(char *data, size_t data_length,
                                   int playernum)
{
   nm_clientrequest_t *message = (nm_clientrequest_t *)data;
   cs_client_request_e request = (cs_client_request_e)message->request_type;

   server_clients[playernum].current_request = request;
}

void SV_HandleVoteRequestMessage(char *data, size_t data_length, int playernum)
{
   nm_voterequest_t *message = (nm_voterequest_t *)data;
   char *command = data + sizeof(nm_voterequest_t);
   size_t command_size;

   if(message->command_size > 32)
   {
      SV_SendMessage(playernum, "Vote command too long.");
      return;
   }

   command_size = pstrnlen(command, message->command_size);

   if(command_size != message->command_size)
   {
      SV_SendMessage(playernum, "Error: corrupt vote command.");
      return;
   }

   if((command_size + sizeof(nm_voterequest_t) + 1) != data_length)
   {
      SV_SendMessage(playernum, "Error: corrupt vote command.");
      return;
   }

   if(clients[playernum].spectating)
   {
      SV_SendMessage(playernum, "Spectators cannot call votes.");
      return;
   }

   if(SV_GetCurrentVote())
   {
      SV_SendMessage(playernum, "Vote already in progress.");
      return;
   }

   if(!SV_NewVote(command))
   {
      SV_SendMessage(playernum, "No such vote command.");
      return;
   }

   SV_BroadcastVote();
}

void SV_BroadcastPlayerTouchedSpecial(int playernum, int thing_net_id)
{
   nm_playertouchedspecial_t touch_message;

   touch_message.message_type = nm_playertouchedspecial;
   touch_message.world_index = sv_world_index;
   touch_message.player_number = playernum;
   touch_message.thing_net_id = thing_net_id;

   broadcast_packet(&touch_message, sizeof(nm_playertouchedspecial_t));
}

void SV_BroadcastPlayerWeaponState(int playernum, int position,
                                   statenum_t stnum)
{
   nm_playerweaponstate_t state_message;

   state_message.message_type = nm_playerweaponstate;
   state_message.world_index = sv_world_index;
   state_message.player_number = playernum;
   state_message.psprite_position = position;
   state_message.weapon_state = stnum;

   broadcast_packet(&state_message, sizeof(nm_playerweaponstate_t));
}

void SV_BroadcastPlayerRemoved(int playernum, disconnection_reason_e reason)
{
   nm_playerremoved_t remove_player_message;

   remove_player_message.message_type = nm_playerremoved;
   remove_player_message.world_index = sv_world_index;
   remove_player_message.player_number = playernum;
   remove_player_message.reason = reason;

   broadcast_packet(&remove_player_message, sizeof(nm_playerremoved_t));
}

void SV_BroadcastPuffSpawned(Mobj *puff, Mobj *shooter, int updown,
                             bool ptcl)
{
   nm_puffspawned_t puff_message;

   puff_message.message_type = nm_puffspawned;
   puff_message.world_index = sv_world_index;
   puff_message.puff_net_id = puff->net_id;
   puff_message.shooter_net_id = shooter->net_id;
   puff_message.x = puff->x;
   puff_message.y = puff->y;
   puff_message.z = puff->z;
   puff_message.angle = puff->angle;
   puff_message.updown = updown;
   puff_message.ptcl = ptcl;

   broadcast_packet(&puff_message, sizeof(nm_puffspawned_t));
}

void SV_BroadcastBloodSpawned(Mobj *blood, Mobj *shooter, int damage,
                              Mobj *target)
{
   nm_bloodspawned_t blood_message;

   blood_message.message_type = nm_bloodspawned;
   blood_message.world_index = sv_world_index;
   blood_message.blood_net_id = blood->net_id;
   blood_message.shooter_net_id = shooter->net_id;
   blood_message.target_net_id = target->net_id;
   blood_message.x = blood->x;
   blood_message.y = blood->y;
   blood_message.z = blood->z;
   blood_message.angle = blood->angle;
   blood_message.damage = damage;

   broadcast_packet(&blood_message, sizeof(nm_bloodspawned_t));
}

void SV_BroadcastActorSpawned(Mobj *actor)
{
   nm_actorspawned_t spawn_message;

   spawn_message.message_type = nm_actorspawned;
   spawn_message.world_index = sv_world_index;
   spawn_message.net_id = actor->net_id;
   spawn_message.x = actor->x;
   spawn_message.y = actor->y;
   spawn_message.z = actor->z;
   spawn_message.momx = actor->momx;
   spawn_message.momy = actor->momy;
   spawn_message.momz = actor->momz;
   spawn_message.angle = actor->angle;
   spawn_message.flags = actor->flags;
   spawn_message.type = actor->type;

   broadcast_packet(&spawn_message, sizeof(nm_actorspawned_t));
}

void SV_BroadcastActorPosition(Mobj *actor, int tic)
{
   int blood = E_SafeThingType(MT_BLOOD);
   int puff  = E_SafeThingType(MT_PUFF);
   int fog   = GameModeInfo->teleFogType;
   nm_actorposition_t position_message;

   // [CG] Information about actors with NET ID 0 never gets sent to clients.
   if(actor->net_id == 0)
      return;

   // [CG] Clients control this stuff after it's spawned.
   if(actor->type == blood || actor->type == puff || actor->type == fog)
      return;

   position_message.message_type = nm_actorposition;
   position_message.world_index = sv_world_index;
   position_message.actor_net_id = actor->net_id;
   CS_SaveActorPosition(&position_message.position, actor, tic);

   broadcast_packet(&position_message, sizeof(nm_actorposition_t));
}

void SV_BroadcastActorMiscState(Mobj *actor, int tic)
{
   int blood = E_SafeThingType(MT_BLOOD);
   int puff  = E_SafeThingType(MT_PUFF);
   int fog   = GameModeInfo->teleFogType;
   nm_actormiscstate_t misc_state_message;

   // [CG] Information about actors with NET ID 0 never gets sent to clients.
   if(actor->net_id == 0)
      return;

   // [CG] Clients control this stuff after it's spawned.
   if(actor->type == blood || actor->type == puff || actor->type == fog)
      return;

   misc_state_message.message_type = nm_actormiscstate;
   misc_state_message.world_index = sv_world_index;
   misc_state_message.actor_net_id = actor->net_id;
   CS_SaveActorMiscState(&misc_state_message.misc_state, actor, tic);

   broadcast_packet(&misc_state_message, sizeof(nm_actorposition_t));
}

void SV_BroadcastSectorPosition(size_t sector_number)
{
   nm_sectorposition_t position_message;

   position_message.message_type = nm_sectorposition;
   position_message.world_index = sv_world_index;
   position_message.sector_number = sector_number;
   CS_SaveSectorPosition(
      &position_message.sector_position, &sectors[sector_number]
   );
   position_message.sector_position.world_index = sv_world_index;

#if _SECTOR_PRED_DEBUG
   if(playeringame[1] && sector_number == _DEBUG_SECTOR)
   {
      printf(
         "Position for sector %d: %d - %d/%d.\n",
         sector_number,
         sv_world_index,
         position_message.sector_position.ceiling_height >> FRACBITS,
         position_message.sector_position.floor_height >> FRACBITS
      );
   }
#endif

   broadcast_packet(&position_message, sizeof(nm_sectorposition_t));
}

void SV_BroadcastMapSpecialSpawned(void *special, void *data, line_t *line,
                                   sector_t *sector,
                                   map_special_e special_type)
{
#if 0
   size_t msg_size = sizeof(nm_specialspawned_t);
   size_t special_size, total_size;
   char *buffer, *status_buffer;
   nm_specialspawned_t *spawn_message;

   switch(special_type)
   {
   case ms_ceiling:
   case ms_ceiling_param:
      special_size = sizeof(CeilingThinker::status_t);
      break;
   case ms_door_tagged:
   case ms_door_manual:
   case ms_door_closein30:
   case ms_door_raisein300:
   case ms_door_param:
      special_size = sizeof(VerticalDoorThinker::status_t);
      break;
   case ms_floor:
   case ms_stairs:
   case ms_donut:
   case ms_donut_hole:
   case ms_floor_param:
      special_size = sizeof(FloorMoveThinker::status_t);
      break;
   case ms_elevator:
      special_size = sizeof(ElevatorThinker::status_t);
      break;
   case ms_pillar_build:
   case ms_pillar_open:
      special_size = sizeof(PillarThinker::status_t);
      break;
   case ms_floorwaggle:
      special_size = sizeof(FloorWaggleThinker::status_t);
      break;
   case ms_platform:
   case ms_platform_gen:
      special_size = sizeof(PlatThinker::status_t);
      break;
   default:
      I_Error(
         "SV_BroadcastMapSpecialSpawned: Invalid map special type %d.\n",
         special_type
      );
      return;
   }

   total_size = msg_size + special_size;
   switch(special_type)
   {
   case ms_ceiling_param:
      total_size += sizeof(cs_ceilingdata_t);
      break;
   case ms_door_param:
      total_size += sizeof(cs_doordata_t);
      break;
   case ms_floor_param:
      total_size += sizeof(cs_floordata_t);
      break;
   default:
      break;
   }

   buffer = emalloc(char *, total_size));
   status_buffer = (buffer + sizeof(nm_specialspawned_t));
   spawn_message = (nm_specialspawned_t *)buffer;
   spawn_message->message_type = nm_specialspawned;
   spawn_message->world_index = sv_world_index;
   spawn_message->special_type = special_type;

   if(line == NULL)
      spawn_message->line_number = 0;
   else
      spawn_message->line_number = line - lines;

   if(sector == NULL)
      spawn_message->sector_number = 0;
   else
      spawn_message->sector_number = sector - sectors;

   switch(special_type)
   {
   case ms_ceiling:
   case ms_ceiling_param:
   {
      CeilingThinker *ct = reinterpret_cast<CeilingThinker *>(special);
      ct->getStatus((CeilingThinker::status_t *)status_buffer);
      break;
   }
   case ms_door_tagged:
   case ms_door_manual:
   case ms_door_closein30:
   case ms_door_raisein300:
   case ms_door_param:
   {
      VerticalDoorThinker *vdt =
         reinterpret_cast<VerticalDoorThinker *>(special);
      vdt->getStatus((VerticalDoorThinker::status_t *)status_buffer);
      break;
   }
   case ms_floor:
   case ms_stairs:
   case ms_donut:
   case ms_donut_hole:
   case ms_floor_param:
   {
      FloorMoveThinker *fmt = reinterpret_cast<FloorMoveThinker *>(special);
      fmt->getStatus((FloorMoveThinker::status_t *)status_buffer);
      break;
   }
   case ms_elevator:
   {
      ElevatorThinker *et = reinterpret_cast<ElevatorThinker *>(special);
      et->getStatus((ElevatorThinker::status_t *)status_buffer);
      break;
   }
   case ms_pillar_build:
   case ms_pillar_open:
   {
      PillarThinker *pt = reinterpret_cast<PillarThinker *>(special);
      pt->getStatus((PillarThinker::status_t *)status_buffer);
      break;
   }
   case ms_floorwaggle:
   {
      FloorWaggleThinker *flt =
         reinterpret_cast<FloorWaggleThinker *>(special);
      flt->getStatus((FloorWaggleThinker::status_t *)status_buffer);
      break;
   }
   case ms_platform:
   case ms_platform_gen:
   {
      PlatThinker *plt = reinterpret_cast<PlatThinker *>(special);
      plt->getStatus((PlatThinker::status_t *)status_buffer);
      break;
   }
   default:
      return;
   }

   if(special_type == ms_ceiling_param)
   {
      ceilingdata_t *ceiling_data = reinterpret_cast<ceilingdata_t *>(data);
      cs_ceilingdata_t *ceiling_data_buffer = (cs_ceilingdata_t *)(
         status_buffer + sizeof(CeilingThinker::status_t)
      );
      CS_SaveCeilingData(ceiling_data_buffer, ceiling_data);
   }
   else if(special_type == ms_door_param)
   {
      doordata_t *door_data = reinterpret_cast<doordata_t *>(data);
      cs_doordata_t *door_data_buffer = (cs_doordata_t *)(
         status_buffer + sizeof(VerticalDoorThinker::status_t)
      );
      CS_SaveDoorData(door_data_buffer, door_data);
   }
   else if(special_type == ms_floor_param)
   {
      floordata_t *floor_data = reinterpret_cast<floordata_t *>(data);
      cs_floordata_t *floor_data_buffer = (cs_floordata_t *)(
         status_buffer + sizeof(FloorMoveThinker::status_t)
      );
      CS_SaveFloorData(floor_data_buffer, floor_data);
   }

   broadcast_packet(buffer, total_size);

   efree(buffer);
#endif
}

void SV_BroadcastMapSpecialStatus(void *special, map_special_e special_type)
{
#if 0
   size_t msg_size = sizeof(nm_specialstatus_t);
   size_t special_size, total_size;
   char *buffer, *status_buffer;
   nm_specialstatus_t *status_message;

   switch(special_type)
   {
   case ms_ceiling:
   case ms_ceiling_param:
      special_size = sizeof(CeilingThinker::status_t);
      break;
   case ms_door_tagged:
   case ms_door_manual:
   case ms_door_closein30:
   case ms_door_raisein300:
   case ms_door_param:
      special_size = sizeof(VerticalDoorThinker::status_t);
      break;
   case ms_floor:
   case ms_stairs:
   case ms_donut:
   case ms_donut_hole:
   case ms_floor_param:
      special_size = sizeof(FloorMoveThinker::status_t);
      break;
   case ms_elevator:
      special_size = sizeof(ElevatorThinker::status_t);
      break;
   case ms_pillar_build:
   case ms_pillar_open:
      special_size = sizeof(PillarThinker::status_t);
      break;
   case ms_floorwaggle:
      special_size = sizeof(FloorWaggleThinker::status_t);
      break;
   case ms_platform:
   case ms_platform_gen:
      special_size = sizeof(PlatThinker::status_t);
      break;
   default:
      I_Error(
         "SV_BroadcastMapSpecialStatus: Invalid map special type %d.\n",
         special_type
      );
      return;
   }

   total_size = msg_size + special_size;
   buffer = emalloc(char *, total_size));
   status_buffer = (buffer + sizeof(nm_specialspawned_t));

   status_message = (nm_specialstatus_t *)buffer;
   status_message->message_type = nm_specialstatus;
   status_message->world_index = sv_world_index;
   status_message->special_type = special_type;

   switch(special_type)
   {
   case ms_ceiling:
   case ms_ceiling_param:
   {
      CeilingThinker *ct = reinterpret_cast<CeilingThinker *>(special);
      ct->getStatus((CeilingThinker::status_t *)status_buffer);
      break;
   }
   case ms_door_tagged:
   case ms_door_manual:
   case ms_door_closein30:
   case ms_door_raisein300:
   case ms_door_param:
   {
      VerticalDoorThinker *vdt =
         reinterpret_cast<VerticalDoorThinker *>(special);
      vdt->getStatus((VerticalDoorThinker::status_t *)status_buffer);
      break;
   }
   case ms_floor:
   case ms_stairs:
   case ms_donut:
   case ms_donut_hole:
   case ms_floor_param:
   {
      FloorMoveThinker *fmt = reinterpret_cast<FloorMoveThinker *>(special);
      fmt->getStatus((FloorMoveThinker::status_t *)status_buffer);
      break;
   }
   case ms_elevator:
   {
      ElevatorThinker *et = reinterpret_cast<ElevatorThinker *>(special);
      et->getStatus((ElevatorThinker::status_t *)status_buffer);
      break;
   }
   case ms_pillar_build:
   case ms_pillar_open:
   {
      PillarThinker *pt = reinterpret_cast<PillarThinker *>(special);
      pt->getStatus((PillarThinker::status_t *)status_buffer);
      break;
   }
   case ms_floorwaggle:
   {
      FloorWaggleThinker *flt =
         reinterpret_cast<FloorWaggleThinker *>(special);
      flt->getStatus((FloorWaggleThinker::status_t *)status_buffer);
      break;
   }
   case ms_platform:
   case ms_platform_gen:
   {
      PlatThinker *plt = reinterpret_cast<PlatThinker *>(special);
      plt->getStatus((PlatThinker::status_t *)status_buffer);
      break;
   }
   default:
      return;
   }

   broadcast_packet(buffer, total_size);
   efree(buffer);
#endif
}

void SV_BroadcastMapSpecialRemoved(unsigned int net_id,
                                   map_special_e special_type)
{
#if 0
   nm_specialremoved_t removal_message;

   removal_message.message_type = nm_specialremoved;
   removal_message.world_index = sv_world_index;
   removal_message.special_type = special_type;
   removal_message.net_id = net_id;

   broadcast_packet(&removal_message, sizeof(nm_specialremoved_t));
#endif
}

void SV_BroadcastAnnouncerEvent(announcer_event_type_e event, Mobj *source)
{
   nm_announcerevent_t message;

   message.message_type = nm_announcerevent;
   message.world_index = sv_world_index;
   if(source)
      message.source_net_id = source->net_id;
   else
      message.source_net_id = 0;
   message.event_index = event;

   broadcast_packet(&message, sizeof(nm_announcerevent_t));
}

void SV_BroadcastTICFinished(void)
{
   nm_ticfinished_t tic_message;

   tic_message.message_type = nm_ticfinished;
   tic_message.world_index = sv_world_index;

   broadcast_packet(&tic_message, sizeof(nm_ticfinished_t));
}

void SV_BroadcastActorTarget(Mobj *actor, actor_target_e target_type)
{
   Mobj *target;
   nm_actortarget_t target_message;

   target_message.message_type = nm_actortarget;
   target_message.world_index = sv_world_index;
   target_message.target_type = target_type;
   target_message.actor_net_id = actor->net_id;

   switch(target_type)
   {
   case CS_AT_TARGET:
      target = actor->target;
      break;
   case CS_AT_TRACER:
      target = actor->tracer;
      break;
   case CS_AT_LASTENEMY:
      target = actor->lastenemy;
      break;
   default:
      I_Error(
         "SV_BroadcastActorTarget: Invalid target type %d.\n", target_type
      );
      break;
   }

   if(actor->target == NULL)
      target_message.target_net_id = 0;
   else
      target_message.target_net_id = target->net_id;

   broadcast_packet(&target_message, sizeof(nm_actortarget_t));
}

void SV_BroadcastActorState(Mobj *actor, statenum_t state_number)
{
   nm_actorstate_t state_message;

   // [CG] Don't send state for spectating players.
   if(actor->player && clients[actor->player - players].spectating)
      return;

   // [CG] Don't send state for actors with no Net ID.
   if(actor->net_id == 0)
      return;

   state_message.message_type = nm_actorstate;
   state_message.world_index = sv_world_index;
   state_message.actor_net_id = actor->net_id;
   state_message.state_number = state_number;
   state_message.actor_type = actor->type;

   broadcast_packet(&state_message, sizeof(nm_actorstate_t));
}

void SV_BroadcastActorDamaged(Mobj *target, Mobj *inflictor,
                              Mobj *source, int health_damage,
                              int armor_damage, int mod,
                              bool damage_was_fatal, bool just_hit)
{
   nm_actordamaged_t damage_message;

   damage_message.message_type = nm_actordamaged;
   damage_message.world_index = sv_world_index;
   damage_message.target_net_id = target->net_id;
   damage_message.just_hit = just_hit;

   if(inflictor == NULL)
      damage_message.inflictor_net_id = 0;
   else
      damage_message.inflictor_net_id = inflictor->net_id;

   if(source == NULL)
      damage_message.source_net_id = 0;
   else
      damage_message.source_net_id = source->net_id;

   damage_message.health_damage = health_damage;
   damage_message.armor_damage = armor_damage;
   damage_message.mod = mod;
   damage_message.damage_was_fatal = damage_was_fatal;

#if _UNLAG_DEBUG
   damage_message.x = target->x;
   damage_message.y = target->y;
   damage_message.z = target->z;
   damage_message.angle = target->angle;
#endif

   broadcast_packet(&damage_message, sizeof(nm_actordamaged_t));
}

void SV_BroadcastActorKilled(Mobj *target, Mobj *inflictor, Mobj *source,
                             int damage, int mod)
{
   nm_actorkilled_t kill_message;

   kill_message.message_type = nm_actorkilled;
   kill_message.world_index = sv_world_index;
   kill_message.target_net_id = target->net_id;

   if(inflictor == NULL)
      kill_message.inflictor_net_id = 0;
   else
      kill_message.inflictor_net_id = inflictor->net_id;

   if(source == NULL)
      kill_message.source_net_id = 0;
   else
      kill_message.source_net_id = source->net_id;

   kill_message.damage = damage;
   kill_message.mod = mod;

   broadcast_packet(&kill_message, sizeof(nm_actorkilled_t));
}

void SV_BroadcastActorRemoved(Mobj *mo)
{
   nm_actorremoved_t remove_message;

   remove_message.message_type = nm_actorremoved;
   remove_message.world_index = sv_world_index;
   remove_message.actor_net_id = mo->net_id;

   broadcast_packet(&remove_message, sizeof(nm_actorremoved_t));
}

static void build_line_packet(nm_lineactivated_t *line_message, Mobj *actor,
                              line_t *line, int side,
                              activation_type_e activation_type)
{
   line_message->message_type = nm_lineactivated;
   line_message->world_index = sv_world_index;
   line_message->activation_type = activation_type;
   line_message->actor_net_id = actor->net_id;
   line_message->actor_x = actor->x;
   line_message->actor_y = actor->y;
   line_message->actor_z = actor->z;
   line_message->actor_angle = actor->angle;
   line_message->line_number = (line - lines);
   line_message->side = side;
}

void SV_BroadcastLineCrossed(Mobj *actor, line_t *line, int side)
{
   nm_lineactivated_t line_message;

   build_line_packet(&line_message, actor, line, side, at_crossed),

   broadcast_packet(&line_message, sizeof(nm_lineactivated_t));
}

void SV_BroadcastLineUsed(Mobj *actor, line_t *line, int side)
{
   nm_lineactivated_t line_message;

   build_line_packet(&line_message, actor, line, side, at_used),

   broadcast_packet(&line_message, sizeof(nm_lineactivated_t));
}

void SV_BroadcastLineShot(Mobj *actor, line_t *line, int side)
{
   nm_lineactivated_t line_message;

   build_line_packet(&line_message, actor, line, side, at_shot);

   broadcast_packet(&line_message, sizeof(nm_lineactivated_t));
}

void SV_BroadcastMonsterActive(Mobj *monster)
{
   nm_monsteractive_t monster_message;

   monster_message.message_type = nm_monsteractive;
   monster_message.world_index = sv_world_index;
   monster_message.monster_net_id = monster->net_id;

   broadcast_packet(&monster_message, sizeof(nm_monsteractive_t));
}

void SV_BroadcastMonsterAwakened(Mobj *monster)
{
   nm_monsterawakened_t monster_message;

   monster_message.message_type = nm_monsterawakened;
   monster_message.world_index = sv_world_index;
   monster_message.monster_net_id = monster->net_id;

   broadcast_packet(&monster_message, sizeof(nm_monsterawakened_t));
}

void SV_BroadcastMissileSpawned(Mobj *source, Mobj *missile)
{
   nm_missilespawned_t missile_message;

   missile_message.message_type = nm_missilespawned;
   missile_message.world_index = sv_world_index;
   missile_message.net_id = missile->net_id;
   missile_message.source_net_id = source->net_id;
   missile_message.type = missile->type;
   missile_message.x = missile->x;
   missile_message.y = missile->y;
   missile_message.z = missile->z;
   missile_message.momx = missile->momx;
   missile_message.momy = missile->momy;
   missile_message.momz = missile->momz;
   missile_message.angle = missile->angle;

   broadcast_packet(&missile_message, sizeof(nm_missilespawned_t));
}

void SV_BroadcastMissileExploded(Mobj *missile)
{
   nm_missileexploded_t explode_message;

   explode_message.message_type = nm_missileexploded;
   explode_message.world_index = sv_world_index;
   explode_message.missile_net_id = missile->net_id;
   explode_message.tics = missile->tics;

   broadcast_packet(&explode_message, sizeof(nm_missileexploded_t));
}

void SV_BroadcastCubeSpawned(Mobj *cube)
{
   nm_cubespawned_t cube_message;

   cube_message.message_type = nm_cubespawned;
   cube_message.world_index = sv_world_index;
   cube_message.net_id = cube->net_id;
   cube_message.target_net_id = cube->target->net_id;

   broadcast_packet(&cube_message, sizeof(nm_cubespawned_t));
}

void SV_BroadcastClientStatuses(void)
{
   int i;
   nm_clientstatus_t status_message;
   ENetPeer *peer;
   client_t *client;
   server_client_t *server_client;

   for(i = 1; i < MAX_CLIENTS; i++)
   {
      peer = SV_GetPlayerPeer(i);
      client = &clients[i];
      server_client = &server_clients[i];

      // [CG] The player probably disconnected between when we received a
      //      command from them and now; no need to send their client's status
      //      in this case, so return early.
      if(peer == NULL)
         return;

      // [CG] Build most of the status message.
      status_message.message_type = nm_clientstatus;
      status_message.world_index = sv_world_index;
      status_message.client_number = i;
      if(sv_world_index > server_client->last_command_run_world_index)
      {
         status_message.client_lag =
            sv_world_index - server_client->last_command_run_world_index;
      }
      else
         status_message.client_lag = 0;
      status_message.server_lag = server_client->commands.size;
      status_message.transit_lag = client->transit_lag;
      status_message.packet_loss = client->packet_loss;

      broadcast_packet(&status_message, sizeof(nm_clientstatus_t));
   }
}

void SV_BroadcastVote()
{
   char *buf;
   nm_vote_t *message;
   size_t command_size;
   size_t message_size = sizeof(nm_vote_t);
   ServerVote *vote = SV_GetCurrentVote();

   if(!vote)
      return;

   command_size = strlen(vote->getCommand());
   buf = ecalloc(char *, 1, message_size + command_size + 1);

   message = (nm_vote_t *)buf;
   message->message_type = nm_vote;
   message->world_index = sv_world_index;
   message->command_size = command_size;
   message->duration = vote->getDuration();
   message->threshold = vote->getThreshold();
   message->max_votes = vote->getMaxVotes();
   memcpy(buf + message_size, vote->getCommand(), command_size);
   broadcast_packet(buf, message_size + command_size + 1);

   efree(message);
}

void SV_BroadcastVoteResult()
{
   nm_voteresult_t message;

   message.message_type = nm_voteresult;
   message.world_index = sv_world_index;
   if(SV_GetCurrentVote()->passed())
      message.passed = true;
   else
      message.passed = false;

   broadcast_packet(&message, sizeof(nm_voteresult_t));
}

void SV_RunDemoTics(void) {}

void SV_HandleClientRequests(void)
{
   int i;

   for(i = 1; i < MAX_CLIENTS; i++)
   {
      server_client_t *sc = &server_clients[i];

      if(sc->auth_level < cs_auth_spectator)
         continue;

      if(sc->current_request == scr_initial_state)
         SV_SendInitialState(i);
      else if(sc->current_request == scr_current_state)
         SV_SendCurrentState(i);
      else if(sc->current_request == scr_sync)
         SV_SendSync(i);

      sc->current_request = scr_none;
   }
}

void SV_SendNewMap(void)
{
   int color, i;
   Mobj *flag_actor;

   sv_world_index = 0;

   for(i = 1; i < MAXPLAYERS; i++)
   {
      server_client_t *sc;
      
      if(!playeringame[i])
         continue;

      sc = &server_clients[i];
      sc->command_buffer_filled = false;
      sc->commands_dropped = 0;
      sc->last_command_run_index = 0;
      sc->last_command_run_world_index = 0;
      sc->last_command_received_index = 0;
      sc->command_world_index = 0;
      sc->received_game_state = false;
      sc->received_command_for_current_map = false;
      M_QueueFree(&sc->commands);
      memset(sc->positions, 0, MAX_POSITIONS * sizeof(cs_player_position_t));
      memset(sc->misc_states, 0, MAX_POSITIONS * sizeof(cs_misc_state_t));
      memset(sc->player_states, 0, MAX_POSITIONS * sizeof(playerstate_t));
   }

   SV_BroadcastMapStarted();

   for(color = team_color_none; color < team_color_max; color++)
   {
      if(!cs_flag_stands[color].net_id)
         continue;

      if((flag_actor = NetActors.lookup(cs_flags[color].net_id)))
         SV_BroadcastActorSpawned(flag_actor);
   }

   sv_should_send_new_map = false;
}

void SV_TryRunTics(void)
{
   int current_tic;
   uint32_t realtics;
   cs_cmd_t command;
   ServerVote *vote = NULL;
   static int new_tic = 0;
   static bool no_players = true;

   if(CS_HEADLESS)
   {
      if(SV_ServerEmpty())
      {
         if(!no_players)
         {
            printf("Server empty, resetting map.\n");
            G_DoCompleted(false);
            CS_DoWorldDone();
         }
         no_players = true;
      }
      else
         no_players = false;
   }
   else
      no_players = false;

   if(d_fastrefresh)
      d_fastrefresh = false;

   current_tic = new_tic;
   new_tic = I_GetTime();

   I_StartTic();
   D_ProcessEvents();
   C_RunBuffers();
   V_FPSTicker();

   realtics = new_tic - current_tic;

   do
   {
      if(CS_HEADLESS)
         SV_ConsoleTicker();

      if(!CS_HEADLESS)
      {
         MN_Ticker();
         C_Ticker();
         CS_ChatTicker();
      }

      SV_MasterUpdate();

      // [CG] If a player we're spectating becomes a spectator, be sure to
      //      switch displayplayer back.
      if(displayplayer != consoleplayer && clients[displayplayer].spectating)
         CS_SetDisplayPlayer(consoleplayer);

      if(!no_players)
      {
         // [CG] If the console is active, then store/use/send a blank command.
         if(consoleactive)
            memset(&players[consoleplayer].cmd, 0, sizeof(ticcmd_t));

         if(!consoleactive)
         {
            G_BuildTiccmd(&players[consoleplayer].cmd);
            if(cs_demo_recording)
            {
               CS_CopyTiccmdToCommand(
                  &command, &players[consoleplayer].cmd, 0, sv_world_index
               );

               if(!CS_WritePlayerCommandToDemo(&command))
               {
                  doom_printf(
                     "Demo error, recording aborted: %s\n",
                     CS_GetDemoErrorMessage()
                  );
                  CS_StopDemo();
               }

            }
         }

         G_Ticker();

         SV_UpdateClientStatuses();
         if((gametic % TICRATE) == 0)
            SV_BroadcastClientStatuses();

         if(gamestate == GS_LEVEL)
         {
            SV_SaveSectorPositions();
            SV_BroadcastPlayerPositions();
            SV_BroadcastUpdatedActorPositionsAndMiscState();
            // [CG] Because map specials are ephemeral by nature, it's probably
            //      not too much extra bandwidth to send updates even if their
            //      states haven't changed.  If this becomes ridiculous, saving
            //      and checking their state is simple enough.
            SV_BroadcastMapSpecialStatuses();
            SV_MarkQueueClientsAFK();
            // [CG] Now that the game loop has finished, servers can address
            //      new clients and map changes.
            SV_HandleClientRequests();

            if(sv_should_send_new_map)
               SV_SendNewMap();

            if((vote = SV_GetCurrentVote()) && vote->hasResult())
            {
               SV_BroadcastVoteResult();
               if(vote->passed())
                  C_RunTextCmd(vote->getCommand());
               SV_CloseVote();
            }
         }

         SV_BroadcastTICFinished();
         gametic++;
      }
      else if((vote = SV_GetCurrentVote()))
         SV_CloseVote();

      sv_world_index++;

   } while (realtics--);

   do
   {
      if(!CS_HEADLESS)
      {
         I_StartTic();
         D_ProcessEvents();
      }
      if(no_players)
         CS_ReadFromNetwork(1000 / TICRATE);
      else
         CS_ReadFromNetwork(1);
   } while ((new_tic = I_GetTime()) == current_tic);
}

void SV_HandleMessage(char *data, size_t data_length, int playernum)
{
   int32_t message_type = *(int32_t *)data;

   switch(message_type)
   {
   case nm_clientrequest:
      SV_HandleClientRequestMessage(data, data_length, playernum);
      break;
   case nm_playermessage:
      SV_HandlePlayerMessage(data, data_length, playernum);
      break;
   case nm_playerinfoupdated:
      SV_HandleUpdatePlayerInfoMessage(data, data_length, playernum);
      break;
   case nm_playercommand:
      SV_HandlePlayerCommandMessage(data, data_length, playernum);
      break;
   case nm_voterequest:
      SV_HandleVoteRequestMessage(data, data_length, playernum);
      break;
   case nm_currentstate:
   case nm_sync:
   case nm_mapstarted:
   case nm_mapcompleted:
   case nm_authresult:
   case nm_clientinit:
   case nm_clientstatus:
   case nm_playerposition:
   case nm_playerspawned:
   case nm_playerweaponstate:
   case nm_playerremoved:
   case nm_playertouchedspecial:
   case nm_servermessage:
   case nm_puffspawned:
   case nm_bloodspawned:
   case nm_actorspawned:
   case nm_actorposition:
   case nm_actortarget:
   case nm_actorstate:
   case nm_actordamaged:
   case nm_actorkilled:
   case nm_actorremoved:
   case nm_lineactivated:
   case nm_monsteractive:
   case nm_monsterawakened:
   case nm_missilespawned:
   case nm_missileexploded:
   case nm_cubespawned:
#if 0
   case nm_specialspawned:
   case nm_specialstatus:
   case nm_specialremoved:
#endif
   case nm_sectorposition:
   case nm_announcerevent:
   case nm_voteresult:
   case nm_ticfinished:
      doom_printf(
         "Received invalid client message %s (%u)\n",
         network_message_names[message_type],
         message_type
      );
      break;
   case nm_max_messages:
   default:
      doom_printf("Received unknown client message %u\n", message_type);
      break;
   }

   if(LOG_ALL_NETWORK_MESSAGES)
      printf("Received [%s message].\n", network_message_names[message_type]);
}

