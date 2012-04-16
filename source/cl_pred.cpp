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

static bool should_repredict_sectors = false;
static cs_cmd_t local_commands[MAX_POSITIONS];
static cs_player_position_t latest_server_position;
static cs_floor_status_e latest_floor_status;
static uint32_t latest_server_command_command_index;
static uint32_t latest_server_command_client_index;
static uint32_t latest_server_command_server_index;
static uint32_t latest_sector_status_index;
static uint32_t last_sector_status_index_predicted;

static uint32_t smtcount = 0;
static SectorThinker **deadsmts = NULL;

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

static void CL_clearOldSectorThinkerUpdates(uint32_t index)
{
   NetIDToObject<SectorThinker> *nito = NULL;

   while((nito = NetSectorThinkers.iterate(nito)))
      nito->object->clearOldUpdates(index);

   while((nito = CLNetSectorThinkers.iterate(nito)))
      nito->object->clearOldUpdates(index);
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
}

static void CL_removeOldSectorThinkers(uint32_t last_sector_index)
{
   uint32_t i, j, size;
   NetIDToObject<SectorThinker> *nito = NULL;

   size = NetSectorThinkers.getSize();

   if(!size)
      return;

   if(smtcount != size)
   {
      smtcount = size;
      deadsmts = erealloc(
         SectorThinker **, deadsmts, sizeof(SectorThinker *) * smtcount
      );
   }

   i = 0;

   while((nito = NetSectorThinkers.iterate(nito)))
   {
      if((nito->object->removal_index) &&
         (nito->object->removal_index < last_sector_index))
      {
         deadsmts[i] = nito->object;
         i++;
      }
   }

   for(j = 0; j < i; j++)
   {
      CS_LogSMT(
         "%u/%u: Removing SMT %u (%u < %u).\n",
         cl_latest_world_index,
         cl_current_world_index,
         deadsmts[j]->net_id,
         deadsmts[j]->removal_index,
         last_sector_index
      );
      deadsmts[j]->Remove();
   }
}

static void CL_loadLatestPlayerPosition()
{
   CS_SetPlayerPosition(consoleplayer, &latest_server_position);
   clients[consoleplayer].floor_status = latest_floor_status;
}

void CL_InitPrediction()
{
   should_repredict_sectors = false;
   latest_floor_status = cs_fs_none;
   latest_server_command_command_index = 0;
   latest_server_command_client_index = 0;
   latest_server_command_server_index = 0;
   latest_sector_status_index = 0;
   last_sector_status_index_predicted = 0;

   memset(local_commands, 0, MAX_POSITIONS * sizeof(cs_cmd_t));
   memset(&latest_server_position, 0, sizeof(cs_player_position_t));
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
   uint32_t command_index, sector_status_index;

   CL_checkPredictedSectorThinkerActivations();

   if(!prediction_enabled)
      return;

   if(cl_commands_sent <= latest_server_command_command_index)
      return;

   CL_removeOldSectorThinkers(latest_server_command_command_index);

   sector_status_index = latest_sector_status_index + 1;
   command_index = latest_server_command_command_index + 1;

   /*
   if(last_sector_status_index_predicted < latest_sector_status_index)
   {
      CS_LogSMT(
         "%u/%u: Re-predicting SMTs for %u TICs (%u => %u).\n",
         cl_latest_world_index,
         cl_current_world_index,
         (command_index - sector_status_index),
         sector_status_index,
         command_index
      );

      cl_setting_sector_positions = true;
      CL_LoadSectorPositionsAt(latest_sector_status_index);
      cl_setting_sector_positions = false;

      for(; sector_status_index < command_index; sector_status_index++)
         CL_rePredictSectorThinkers(sector_status_index);

      last_sector_status_index_predicted = latest_sector_status_index;
   }
   */

   if(should_repredict_sectors)
   {
      CS_LogSMT(
         "%u/%u: Re-predicting SMTs for %u TICs (%u => %u).\n",
         cl_latest_world_index,
         cl_current_world_index,
         (command_index - sector_status_index),
         sector_status_index,
         command_index
      );

      cl_setting_sector_positions = true;
      CL_LoadSectorPositionsAt(latest_sector_status_index);
      cl_setting_sector_positions = false;

      for(; sector_status_index < command_index; sector_status_index++)
         CL_rePredictSectorThinkers(sector_status_index);

      should_repredict_sectors = false;
   }

   if(command_index > 2)
      CL_clearOldSectorThinkerUpdates(command_index - 1);

   if(!clients[consoleplayer].spectating)
      CL_loadLatestPlayerPosition();

   cl_setting_sector_positions = true;
   CL_LoadSectorPositionsAt(latest_server_command_command_index);
   cl_setting_sector_positions = false;

   CS_LogSMT(
      "%u/%u: Re-predicting %u TICs %u => %u.\n",
      cl_latest_world_index,
      cl_current_world_index,
      (cl_commands_sent - 2) - latest_server_command_command_index,
      command_index,
      cl_commands_sent - 1
   );

   if((cl_commands_sent) &&
      ((cl_commands_sent - 1) > command_index) &&
      (((cl_commands_sent - 1) - command_index) > MAX_POSITIONS))
   {
      doom_printf(
         "Lost connection to the server, last local position received was > "
         "%u TICs ago (%u/%u).\n",
         MAX_POSITIONS,
         command_index,
         cl_commands_sent
      );
      CL_DeferDisconnect();
      return;
   }

   for(; command_index < (cl_commands_sent - 1); command_index++)
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
   CL_SavePredictedSectorPositions(command_index);

   if((!clients[consoleplayer].spectating) && (players[consoleplayer].mo))
      players[consoleplayer].mo->Think();

   CS_LogSMT(
      "%u/%u: Done predicting.\n",
      cl_latest_world_index,
      cl_current_world_index
   );
}

void CL_StoreLatestSectorStatusIndex(uint32_t index)
{
   CS_LogSMT(
      "%u/%u: Last SSI: %u.\n",
      cl_latest_world_index,
      cl_current_world_index,
      index
   );
   latest_sector_status_index = index;
   should_repredict_sectors = true;
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
   CS_CopyPlayerPosition(&latest_server_position, &message->position);
   latest_server_command_command_index = message->last_index_run;
   latest_server_command_client_index = message->last_world_index_run;
   latest_server_command_server_index = message->world_index;
   latest_server_position.world_index = message->world_index;
   latest_floor_status = (cs_floor_status_e)message->floor_status;
   CL_setSectorThinkerActivationCutoffs(
      latest_server_command_command_index, latest_server_command_server_index
   );
}

cs_player_position_t* CL_GetLastServerPosition()
{
   return &latest_server_position;
}

