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

static void SV_updateQueueLevels()
{
   unsigned int i;
   client_t *client;
   server_client_t *sc;

   for(i = 1; i < MAX_CLIENTS; i++)
   {
      if(!playeringame[i])
         continue;

      client = &clients[i];
      sc = &server_clients[i];

      if(client->queue_level != ql_waiting)
         continue;

      if(client->queue_position != 0)
         continue;

      SV_QueueSetClientCanJoin(i);
   }
}

static void SV_setQueueLevelAndPosition(int clientnum, cs_queue_level_e level,
                                        unsigned int position)
{
   client_t *client = &clients[clientnum];
   bool should_update = false;

   if(client->queue_level != level)
   {
      clients[clientnum].queue_level = level;
      SV_BroadcastPlayerScalarInfo(clientnum, ci_queue_level);
      should_update = true;
   }

   if(client->queue_position != position)
   {
      clients[clientnum].queue_position = position;
      SV_BroadcastPlayerScalarInfo(clientnum, ci_queue_position);
      should_update = true;
   }

   if(should_update)
      SV_updateQueueLevels();
}

static unsigned int SV_getNewQueuePosition()
{
   unsigned int i;
   unsigned int playing_players = 0;
   unsigned int max_queue_position = 0;
   unsigned int tic_limit = sv_join_time_limit * TICRATE;
   client_t *client;

   // [CG] First, ensure queue levels are as current as possible.
   SV_updateQueueLevels();

   // [CG] Check to see if there is room inside max_players.
   for(i = 1; i < MAX_CLIENTS; i++)
   {
      if(!playeringame[i])
         continue;

      client = &clients[i];

      // [CG] Keep track of the maximum queue position to avoid looping twice
      //      in the worst case.
      if(client->queue_position >= max_queue_position)
         max_queue_position = client->queue_position + 1;

      if(client->queue_level < ql_can_join)
         continue;

      if(clients[i].afk)
         continue;

      if(client->queue_level == ql_playing)
         playing_players++;
      else if(server_clients[i].finished_waiting_in_queue_tic <= tic_limit)
         playing_players++;
   }

   // [CG] If there was room inside max_players, then the player doesn't need
   //      to wait.  Otherwise they enter at the back of the queue.
   if(playing_players < cs_settings->max_players)
      return 0;

   return max_queue_position;
}

static void SV_advanceQueue(unsigned int clientnum)
{
   unsigned int i, queue_position;

   if(clientnum)
   {
      if(!playeringame[clientnum])
         return;

      // [CG] Client wasn't in the queue, so there's no one waiting after them.
      if(clients[clientnum].queue_level == ql_none)
         return;
   }

   if(clientnum)
      queue_position = clients[clientnum].queue_position;
   else
      queue_position = 0;

   for(i = 1; i < MAX_CLIENTS; i++)
   {
      if(!playeringame[i])
         continue;
      if(i == clientnum)
         continue;
      if(clients[i].queue_level != ql_waiting)
         continue;
      if(clients[i].queue_position <= queue_position)
         continue;

      if(!(--clients[i].queue_position))
         SV_QueueSetClientDoneWaiting(i);
      else
         SV_BroadcastPlayerScalarInfo(i, ci_queue_position);
   }
}

void SV_QueueSetClientRemoved(int clientnum)
{
   SV_advanceQueue(clientnum);
   SV_setQueueLevelAndPosition(clientnum, ql_none, 0);
}

void SV_QueueSetClientWaiting(int clientnum, unsigned int position)
{
   SV_setQueueLevelAndPosition(clientnum, ql_waiting, position);
}

void SV_QueueSetClientDoneWaiting(int clientnum)
{
   SV_QueueSetClientCanJoin(clientnum);
   server_clients[clientnum].finished_waiting_in_queue_tic = gametic;
}

void SV_QueueSetClientCanJoin(int clientnum)
{
   SV_setQueueLevelAndPosition(clientnum, ql_can_join, 0);
}

void SV_QueueSetClientNotPlaying(int clientnum)
{
   SV_QueueSetClientDoneWaiting(clientnum);
   SV_advanceQueue(0);
}

void SV_QueueSetClientPlaying(int clientnum)
{
   unsigned int new_wait = gametic - ((sv_join_time_limit * TICRATE) + 1);

   SV_setQueueLevelAndPosition(clientnum, ql_playing, 0);
   server_clients[clientnum].finished_waiting_in_queue_tic = new_wait;
}

void SV_PutClientInQueue(int clientnum)
{
   SV_QueueSetClientWaiting(clientnum, SV_getNewQueuePosition());
   SV_updateQueueLevels();
}

void SV_MarkQueueClientsAFK()
{
   unsigned int i, tics_waiting;
   unsigned int tic_limit = sv_join_time_limit * TICRATE;

   for(i = 1; i < MAX_CLIENTS; i++)
   {
      if(!playeringame[i])
         continue;

      tics_waiting = gametic - server_clients[i].finished_waiting_in_queue_tic;

      if(clients[i].queue_level != ql_can_join)
         continue;

      if((tics_waiting > tic_limit) && (!clients[i].afk))
      {
         clients[i].afk = true;
         SV_BroadcastPlayerScalarInfo(i, ci_afk);
      }
   }
}

