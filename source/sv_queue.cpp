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
//   Serverside player queue routines.
//
//----------------------------------------------------------------------------

#include "z_zone.h"
#include "doomstat.h"
#include "cs_main.h"
#include "sv_main.h"
#include "sv_queue.h"

void SV_UpdateQueueLevels()
{
   unsigned int i;
   client_t *client;
   server_client_t *sc;
   cs_queue_level_e new_queue_level;

   for(i = 1; i < MAX_CLIENTS; i++)
   {
      if(!playeringame[i] || clients[i].queue_level == ql_none)
         continue;

      client = &clients[i];
      sc = &server_clients[i];

      if(client->queue_position == 0)
      {
         sc->finished_waiting_in_queue_tic = gametic;
         new_queue_level = ql_can_join;
      }
      else
         new_queue_level = ql_waiting;

      if(client->queue_level != new_queue_level)
      {
         client->queue_level = new_queue_level;
         SV_BroadcastPlayerScalarInfo(i, ci_queue_level);
      }
   }
}

unsigned int SV_GetNewQueuePosition()
{
   unsigned int i;
   unsigned int playing_players = 0;
   unsigned int max_queue_position = 0;
   unsigned int tic_limit = sv_queue_wait_seconds * TICRATE;
   client_t *client;

   // [CG] First, ensure queue levels are as current as possible.
   SV_UpdateQueueLevels();

   // [CG] Next, count playing players.  If there is still room within
   //      max_players then the player doesn't need to queue.
   for(i = 1; i < MAX_CLIENTS; i++)
   {
      if(playeringame[i])
      {
         client = &clients[i];
         server_client_t *sc = &server_clients[i];
         unsigned int tics_waiting =
            gametic - sc->finished_waiting_in_queue_tic;

         if((client->queue_level == ql_playing) ||
            (client->queue_level == ql_can_join && tics_waiting <= tic_limit))
         {
            if(client->queue_level == ql_playing)
               printf("Client %s is playing.\n", players[i].name);
            else
            {
               printf(
                  "Client %s is under the limit (%d/%d).\n",
                  players[i].name,
                  tics_waiting,
                  tic_limit
               );
            }
            playing_players++;
         }
      }
   }

   printf(
      "Playing players/Max players: %d/%d.\n",
      playing_players,
      cs_settings->max_players
   );

   if(playing_players < cs_settings->max_players)
      return 0;

   // [CG] At this point there are no more opening slots, so the player must
   //      enter the queue.  Find the queue position at which they'll enter.
   for(i = 1; i < MAX_CLIENTS; i++)
   {
      client = &clients[i];
      if(playeringame[i] && client->queue_position >= max_queue_position)
         max_queue_position = client->queue_position + 1;
   }

   return max_queue_position;
}

void SV_AssignQueuePosition(int playernum, unsigned int position)
{
   clients[playernum].queue_position = position;
   SV_BroadcastPlayerScalarInfo(playernum, ci_queue_position);

   if(clients[playernum].queue_level > 0)
      clients[playernum].queue_level = ql_waiting;
   else
      clients[playernum].queue_level = ql_can_join;

   SV_BroadcastPlayerScalarInfo(playernum, ci_queue_level);

   printf("Assigned position %d to %d.\n", position, playernum);
}

void SV_AdvanceQueue(unsigned int clientnum)
{
   unsigned int i;

   if(!playeringame[clientnum])
      return;

   if(clients[clientnum].queue_level == ql_none)
      return;

   for(i = 1; i < MAX_CLIENTS; i++)
   {
      if(!playeringame[i] || i == clientnum)
         continue;

      // [CG] Advance all clients behind this player in the queue.
      if(clients[i].queue_position > clients[clientnum].queue_position)
         SV_AssignQueuePosition(i, clients[i].queue_position - 1);
   }
}

void SV_PutPlayerInQueue(int playernum)
{
   SV_AssignQueuePosition(playernum, SV_GetNewQueuePosition());
   SV_UpdateQueueLevels();
}

void SV_PutPlayerAtQueueEnd(int playernum)
{
   SV_RemovePlayerFromQueue(playernum);
   SV_PutPlayerInQueue(playernum);
}

void SV_RemovePlayerFromQueue(int playernum)
{
   SV_AdvanceQueue(playernum);
   clients[playernum].queue_level = ql_none;
   SV_BroadcastPlayerScalarInfo(playernum, ci_queue_level);
   SV_UpdateQueueLevels();
}

void SV_MarkQueuePlayersAFK()
{
   unsigned int i, tics_waiting;
   unsigned int tic_limit = sv_queue_wait_seconds * TICRATE;

   for(i = 1; i < MAX_CLIENTS; i++)
   {
      if(!playeringame[i])
         continue;

      if(clients[i].queue_position != ql_can_join)
         continue;

      tics_waiting = gametic - server_clients[i].finished_waiting_in_queue_tic;

      if((tics_waiting > tic_limit) && clients[i].afk)
      {
         clients[i].afk = true;
         SV_BroadcastPlayerScalarInfo(i, ci_afk);
      }
   }
}

