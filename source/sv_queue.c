// Emacs style mode select -*- C -*- vi:sw=3 ts=3:
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

#include "doomstat.h"

#include "cs_main.h"
#include "sv_main.h"
#include "sv_queue.h"

unsigned int SV_GetNewQueuePosition(void)
{
   unsigned int i;
   unsigned int playing_players = 0;
   unsigned int max_queue_position = 0;

   // [CG] First, ensure queue levels are as current as possible.
   SV_UpdateQueueLevels();

   // [CG] Next, count playing players.  If there is still room within
   //      max_players then the player doesn't need to queue.
   for(i = 1; i < MAX_CLIENTS; i++)
   {
      if(playeringame[i] && clients[i].queue_level == ql_playing)
      {
         playing_players++;
      }
   }

   if(playing_players < cs_settings->max_players)
   {
      printf("SV_GetNewQueuePosition: Open slots, returning 0.\n");
      return 0;
   }

   // [CG] At this point there are no more opening slots, so the player must
   //      enter the queue.  Find the queue position at which they'll enter.
   for(i = 1; i < MAX_CLIENTS; i++)
   {
      if(playeringame[i] && clients[i].queue_position >= max_queue_position)
      {
         max_queue_position = clients[i].queue_position + 1;
      }
   }

   printf(
      "SV_GetNewQueuePosition: No open slots, returning %d.\n",
      max_queue_position
   );
   return max_queue_position;
}

void SV_AssignQueuePosition(int playernum, unsigned int queue_position)
{
   clients[playernum].queue_position = queue_position;
   SV_BroadcastPlayerScalarInfo(playernum, ci_queue_position);
   if(clients[playernum].queue_level > 0)
   {
      clients[playernum].queue_level = ql_waiting;
   }
   else
   {
      clients[playernum].queue_level = ql_playing;
   }

   printf(
      "SV_AssignQueuePosition: Assigned position %u to %d.\n",
      queue_position,
      playernum
   );

   SV_BroadcastPlayerScalarInfo(playernum, ci_queue_level);
}

void SV_AdvanceQueue(unsigned int queue_position)
{
   unsigned int i;
   client_t *client;

   for(i = 1; i < MAX_CLIENTS; i++)
   {
      if(!playeringame[i])
      {
         continue;
      }
      client = &clients[i];
      if(client->queue_position > queue_position)
      {
         // [CG] Advance all clients behind this player in the queue.
         SV_AssignQueuePosition(i, client->queue_position - 1);
      }
   }
}

void SV_PutPlayerInQueue(int playernum)
{
   SV_AssignQueuePosition(playernum, SV_GetNewQueuePosition());
   SV_UpdateQueueLevels();
}

void SV_PutPlayerAtQueueEnd(int playernum)
{
   if(clients[playernum].queue_level != ql_none)
   {
      SV_AdvanceQueue(clients[playernum].queue_position);
   }
   SV_AssignQueuePosition(playernum, SV_GetNewQueuePosition());
   SV_UpdateQueueLevels();
}

void SV_UpdateQueueLevels(void)
{
   unsigned int i;
   client_t *client;
   cs_queue_level_t new_queue_level;

   for(i = 1; i < MAX_CLIENTS; i++)
   {
      if(!playeringame[i] || clients[i].queue_level == ql_none)
      {
         continue;
      }

      client = &clients[i];

      if(client->queue_position > 0)
      {
         new_queue_level = ql_waiting;
      }
      else
      {
         new_queue_level = ql_playing;
      }

      if(client->queue_level != new_queue_level)
      {
         client->queue_level = new_queue_level;
         SV_BroadcastPlayerScalarInfo(i, ci_queue_level);
      }
   }
}

