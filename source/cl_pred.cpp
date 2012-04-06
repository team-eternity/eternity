// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2012 Charles Gunyon
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
//   Prediction routines for clientside local player and sector prediction
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "d_event.h"
#include "doomstat.h"
#include "i_system.h"
#include "r_state.h"
#include "tables.h"
#include "p_user.h"

#include "cs_cmd.h"
#include "cs_main.h"
#include "cs_netid.h"
#include "cs_position.h"
#include "cl_main.h"
#include "cl_pred.h"
#include "cl_spec.h"

bool cl_predicting = false;

static bool prediction_enabled = true;

static cs_cmd_t local_commands[MAX_POSITIONS];
static cs_player_position_t last_server_position;
static cs_floor_status_e last_floor_status;
static uint32_t last_server_command_command_index;
static uint32_t last_server_command_client_index;
static uint32_t last_server_command_server_index;
static uint32_t latest_sector_position_index;

static void CL_runPlayerCommand(uint32_t index, bool think)
{
   ticcmd_t ticcmd;

   CS_LogSMT(
      "%u/%u: Running player command %u.\n",
      cl_latest_world_index,
      cl_current_world_index,
      index
   );

   CS_CopyCommandToTiccmd(&ticcmd, CL_GetCommandAtIndex(index));
   CS_RunPlayerCommand(consoleplayer, &ticcmd, think);
   // CS_LogPlayerPosition(consoleplayer);
}

static void CL_setSectorThinkerActivationCutoffs(uint32_t command_index,
                                                 uint32_t world_index)
{
   NetIDToObject<SectorThinker> *nito = NULL;

   while((nito = CLNetSectorThinkers.iterate(nito)))
      nito->object->setActivationIndexCutoff(command_index, world_index);
}

static void CL_checkPredictedSectorThinkerActivations()
{
   NetIDToObject<SectorThinker> *nito = NULL;

   while((nito = CLNetSectorThinkers.iterate(nito)))
   {
      nito->object->checkActivationExpired();
      if(nito->object->activationExpired())
      {
         nito->object->Reset();
         nito->object->removeThinker();
      }
   }
}

static void CL_rePredictSectorThinkers(uint32_t index)
{
   NetIDToObject<SectorThinker> *nito = NULL;

   while((nito = NetSectorThinkers.iterate(nito)))
      nito->object->rePredict(index);

   while((nito = CLNetSectorThinkers.iterate(nito)))
      nito->object->rePredict(index);
}

static void CL_predictSectorThinkers(uint32_t index)
{
   NetIDToObject<SectorThinker> *nito = NULL;

   while((nito = NetSectorThinkers.iterate(nito)))
      nito->object->Predict(index);

   while((nito = CLNetSectorThinkers.iterate(nito)))
      nito->object->Predict(index);

   CL_SavePredictedSectorPositions(index);
}

static void CL_removeOldSectorThinkers(uint32_t last_sector_index)
{
   PlatThinker           *platform    = NULL;
   VerticalDoorThinker   *door        = NULL;
   CeilingThinker        *ceiling     = NULL;
   FloorMoveThinker      *floor       = NULL;
   ElevatorThinker       *elevator    = NULL;
   PillarThinker         *pillar      = NULL;
   FloorWaggleThinker    *waggle      = NULL;
   NetIDToObject<SectorThinker> *nito = NULL;

   while((nito = NetSectorThinkers.iterate(nito)))
   {
      if(nito->object->removed && (nito->object->removed < last_sector_index))
      {
         CS_LogSMT(
            "%u/%u: Removing SMT %u (%u < %u).\n",
            cl_latest_world_index,
            cl_current_world_index,
            nito->object->net_id,
            nito->object->removed,
            last_sector_index
         );
      }
      else
         continue;

      if((platform = dynamic_cast<PlatThinker *>(nito->object)))
         P_RemoveActivePlat(platform);
      else if((door = dynamic_cast<VerticalDoorThinker *>(nito->object)))
         P_RemoveDoor(door);
      else if((ceiling = dynamic_cast<CeilingThinker *>(nito->object)))
         P_RemoveActiveCeiling(ceiling);
      else if((floor = dynamic_cast<FloorMoveThinker *>(nito->object)))
         P_RemoveFloor(floor);
      else if((elevator = dynamic_cast<ElevatorThinker *>(nito->object)))
         P_RemoveElevator(elevator);
      else if((pillar = dynamic_cast<PillarThinker *>(nito->object)))
         P_RemovePillar(pillar);
      else if((waggle = dynamic_cast<FloorWaggleThinker *>(nito->object)))
         P_RemoveFloorWaggle(waggle);
      else
      {
         I_Error(
            "CL_RemoveOldSectorThinkers: "
            "can't remove thinker %u: unknown type.\n",
            nito->object->net_id
         );
      }
   }
}

static void CL_loadLatestPlayerPosition()
{
   CS_SetPlayerPosition(consoleplayer, &last_server_position);
   clients[consoleplayer].floor_status = last_floor_status;
}

void CL_InitPrediction()
{
   cl_current_world_index = 0;
   memset(local_commands, 0, MAX_POSITIONS * sizeof(cs_cmd_t));
}

cs_cmd_t* CL_GetCommandAtIndex(uint32_t index)
{
   return &local_commands[index % MAX_POSITIONS];
}

cs_cmd_t* CL_GetCurrentCommand()
{
   return CL_GetCommandAtIndex(cl_commands_sent - 1);
}

void CL_RunPredictedCommand()
{
   CL_runPlayerCommand(cl_commands_sent - 1, false);
}

void CL_Predict()
{
   uint32_t command_index;

   /*
    * [CG]
    *   - Load the latest player position
    *   - Load the sector positions for the PPM's command index
    *   - Load the latest sector status <= the PPM's command index
    *   - For each command sent after the last PPM:
    *     - Load/Predict sector position
    *       - Load stored status changes for the command index if they exist
    *       - If predicting, save sector positions and thinker status (if it
    *         changed)
    *     - Run command
    */

   CL_checkPredictedSectorThinkerActivations();

   if(!prediction_enabled)
      return;

   if(cl_commands_sent <= last_server_command_command_index)
      return;

   CL_removeOldSectorThinkers(last_server_command_command_index);

   if(!clients[consoleplayer].spectating)
      CL_loadLatestPlayerPosition();

   cl_setting_sector_positions = true;
   CL_LoadSectorPositionsAt(last_server_command_command_index);
   cl_setting_sector_positions = false;

   CS_LogSMT(
      "%u/%u: Re-predicting %u TICs %u => %u.\n",
      cl_latest_world_index,
      cl_current_world_index,
      (cl_commands_sent - 2) - last_server_command_command_index,
      last_server_command_command_index + 1,
      cl_commands_sent - 1
   );

   for(command_index = last_server_command_command_index + 1;
       command_index < (cl_commands_sent - 1);
       command_index++)
   {
      CL_rePredictSectorThinkers(command_index);

      if(!clients[consoleplayer].spectating)
      {
         cl_predicting = true;
         CL_runPlayerCommand(command_index, true);
         cl_predicting = false;
      }
   }

   CS_LogSMT(
      "%u/%u: Predicting....\n", cl_latest_world_index, cl_current_world_index
   );

   if(clients[consoleplayer].spectating && (players[consoleplayer].mo))
      players[consoleplayer].mo->Think();

   P_PlayerThink(&players[consoleplayer]);

   CL_predictSectorThinkers(command_index);

   if((!clients[consoleplayer].spectating) && (players[consoleplayer].mo))
      players[consoleplayer].mo->Think();

   CS_LogSMT(
      "%u/%u: Done predicting.\n",
      cl_latest_world_index,
      cl_current_world_index
   );
}

#if 0
void CL_Predict()
{
   int old_status = 0;
   bool started_predicting_sectors = false;
   uint32_t i, delta, lsindex, command_index;
   uint32_t client_world_index, server_world_index, command_start_index;
   PlatThinker *platform;
   
   CL_checkSectorThinkerActivations();

   if(!prediction_enabled)
      return;

   if(!(platform = (PlatThinker *)NetSectorThinkers.lookup(1)))
      platform = (PlatThinker *)CLNetSectorThinkers.lookup(1);

   lsindex = latest_sector_position_index;

   command_index = last_server_command_command_index + 1;

   if(cl_commands_sent <= (command_index + 1))
      return;

   delta = cl_commands_sent - (command_index + 1);

   if((!delta) || (delta >= MAX_POSITIONS))
      return;

   // client_world_index cl_latest_world_index - delta;
   client_world_index  = last_server_command_client_index + 1;
   server_world_index  = last_server_command_server_index + 1;
   command_start_index = last_server_command_server_index - delta;

   cl_predicting_sectors = true;

   if(platform)
   {
      old_status = platform->status;
      CS_LogSMT(
         "%u/%u: SMT %u before re-prediction: %d/%d.\n",
         cl_latest_world_index,
         cl_current_world_index,
         _DEBUG_SECTOR,
         platform->sector->ceilingheight >> FRACBITS,
         platform->sector->floorheight >> FRACBITS
      );
   }

   if(lsindex < client_world_index)
      CL_removeOldSectorThinkers(lsindex);
   else
      CL_removeOldSectorThinkers(client_world_index);

   if(lsindex < client_world_index)
   {
      CS_LogSMT(
         "%u/%u: Catching sectors up (%u => %u).\n",
         cl_latest_world_index,
         cl_current_world_index,
         lsindex,
         client_world_index
      );

      cl_setting_sector_positions = true;
      CL_LoadSectorPositionsAt(lsindex);
      cl_setting_sector_positions = false;

      CS_LogSMT(
         "%u/%u: Started sector prediction past %u.\n",
         cl_latest_world_index,
         cl_current_world_index,
         lsindex
      );

      lsindex++;
      started_predicting_sectors = true;

      cl_predicting_sectors = true;
      for(; lsindex < client_world_index; lsindex++)
         CL_rePredictSectorThinkers(lsindex);
      cl_predicting_sectors = false;

      if(platform)
      {
         CS_LogSMT(
            "%u/%u: Done catching sectors up (%d/%d).\n",
            cl_latest_world_index,
            cl_current_world_index,
            platform->sector->ceilingheight >> FRACBITS,
            platform->sector->floorheight >> FRACBITS
         );
      }
      else
      {
         CS_LogSMT(
            "%u/%u: Done catching sectors up.\n",
            cl_latest_world_index,
            cl_current_world_index
         );
      }
   }

   CS_LogSMT(
      "%u/%u: Re-predicting %u (%u/%u) TICs (%u/%u/%u).\n",
      cl_latest_world_index,
      cl_current_world_index,
      delta,
      command_index,
      cl_commands_sent - 1,
      lsindex,
      client_world_index,
      server_world_index
   );

   if(client_world_index > 1)
   {
      for(; client_world_index < server_world_index; client_world_index++)
      {
         if(started_predicting_sectors)
         {
            cl_predicting_sectors = true;
            CL_rePredictSectorThinkers(client_world_index);
            cl_predicting_sectors = false;
         }
         else
         {
            if(client_world_index <= lsindex)
            {
               cl_setting_sector_positions = true;
               CL_LoadSectorPositionsAt(client_world_index);
               cl_setting_sector_positions = false;

               if(client_world_index == lsindex)
               {
                  CS_LogSMT(
                     "%u/%u: Started sector prediction past %u.\n",
                     cl_latest_world_index,
                     cl_current_world_index,
                     client_world_index
                  );
                  started_predicting_sectors = true;
               }
            }
         }
      }
   }

   cl_predicting = true;

   CS_LogSMT(
      "%u/%u: Loading last server position.\n",
      cl_latest_world_index,
      cl_current_world_index
   );

   CS_LogPlayerPosition(consoleplayer);
   if(!clients[consoleplayer].spectating)
      CL_loadLatestPlayerPosition();
   CS_LogPlayerPosition(consoleplayer);

   CS_LogSMT(
      "%u/%u: Running commands.\n",
      cl_latest_world_index,
      cl_current_world_index
   );

   for(i = 0; i < delta; i++, client_world_index++)
   {
      if(!clients[consoleplayer].spectating)
         CL_runPlayerCommand(command_index + i, true);

      // if(i < (delta - 1))
      {
         if(started_predicting_sectors)
         {
            cl_predicting_sectors = true;
            CL_predictSectorThinkers(client_world_index);
            cl_predicting_sectors = false;
         }
         else if(client_world_index <= lsindex)
         {
            cl_setting_sector_positions = true;
            CL_LoadSectorPositionsAt(client_world_index);
            cl_setting_sector_positions = false;

            if(client_world_index == lsindex)
            {
               CS_LogSMT(
                  "%u/%u: Started sector prediction past %u.\n",
                  cl_latest_world_index,
                  cl_current_world_index,
                  client_world_index
               );
               started_predicting_sectors = true;
            }
         }
      }
   }

   cl_predicting = false;

   if(platform)
   {
      CS_LogSMT(
         "%u/%u: SMT %u after re-prediction: %d/%d.\n",
         cl_latest_world_index,
         cl_current_world_index,
         _DEBUG_SECTOR,
         platform->sector->ceilingheight >> FRACBITS,
         platform->sector->floorheight >> FRACBITS
      );

      if(platform->status != old_status)
      {
         CS_LogSMT(
            "%u/%u: Status changed: %d => %d.\n",
            cl_latest_world_index,
            cl_current_world_index,
            old_status,
            platform->status
         );
      }
   }

   CS_LogSMT(
      "%u/%u: Done re-predicting.\n",
      cl_latest_world_index,
      cl_current_world_index
   );

   if(clients[consoleplayer].spectating && (players[consoleplayer].mo))
      players[consoleplayer].mo->Think();

   CS_LogSMT(
      "%u/%u: Predicting....\n", cl_latest_world_index, cl_current_world_index
   );

   CS_LogPlayerPosition(consoleplayer);
   P_PlayerThink(&players[consoleplayer]);

   cl_predicting_sectors = true;
   CL_predictSectorThinkers(client_world_index);
   cl_predicting_sectors = false;

   if((!clients[consoleplayer].spectating) && (players[consoleplayer].mo))
      players[consoleplayer].mo->Think();

   CS_LogPlayerPosition(consoleplayer);

   CS_LogSMT(
      "%u/%u: Done predicting.\n",
      cl_latest_world_index,
      cl_current_world_index
   );
}
#endif

void CL_StoreLatestSectorPositionIndex(uint32_t index)
{
   CS_LogSMT(
      "%u/%u: Last SPI: %u.\n",
      cl_latest_world_index,
      cl_current_world_index,
      index
   );
   latest_sector_position_index = index;
}

void CL_StoreLastServerPosition(nm_playerposition_t *message)
{
   CS_LogSMT(
      "%u/%u: PPM %u/%u/%u.\n",
      cl_latest_world_index,
      cl_current_world_index,
      message->last_index_run,
      message->last_world_index_run,
      message->world_index
   );
   CS_LogPlayerPosition(consoleplayer, &message->position);
   CS_CopyPlayerPosition(&last_server_position, &message->position);
   last_server_command_command_index = message->last_index_run;
   last_server_command_client_index = message->last_world_index_run;
   last_server_command_server_index = message->world_index;
   last_server_position.world_index = message->world_index;
   last_floor_status = (cs_floor_status_e)message->floor_status;
   CL_setSectorThinkerActivationCutoffs(
      last_server_command_command_index, last_server_command_server_index
   );
}

cs_player_position_t* CL_GetLastServerPosition()
{
   return &last_server_position;
}

