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
//   Prediction routines for clientside and sector prediction
//
//----------------------------------------------------------------------------

#include "doomdef.h"
#include "info.h"
#include "e_states.h"
#include "p_info.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_map3d.h"
#include "p_mobj.h"
#include "p_tick.h"
#include "r_state.h"
#include "e_ttypes.h"

#include "cs_cmd.h"
#include "cs_position.h"
#include "cs_netid.h"
#include "cs_main.h"
#include "cl_main.h"
#include "cl_spec.h"
#include "cl_pred.h"

boolean cl_predicting = false;

static cs_cmd_t local_commands[MAX_POSITIONS];

void CL_InitPrediction(void)
{
   cl_current_world_index = 0;
   memset(local_commands, 0, MAX_POSITIONS * sizeof(cs_cmd_t));
}

cs_cmd_t* CL_GetCurrentCommand(void)
{
   return &local_commands[cl_current_world_index % MAX_POSITIONS];
}

void CL_PredictPlayerPosition(unsigned int index)
{
   CS_SetPlayerCommand(consoleplayer, &local_commands[index % MAX_POSITIONS]);
#if _PRED_DEBUG
   printf(
      "CL_PredictPlayerPosition (%3u/%3u): Predicting...\n  %u: ",
      cl_current_world_index,
      cl_latest_world_index,
      index
   );
   CS_PrintTiccmd(&players[consoleplayer].cmd);
#endif

   CS_PlayerThink(consoleplayer);
#if _PRED_DEBUG
   printf("  ");
   CS_PrintPlayerPosition(consoleplayer, index);
#endif

#if _SECTOR_PRED_DEBUG
   printf(
      "CL_PredictPlayerPosition: Sector %2u: %3u/%3u\n",
      _DEBUG_SECTOR,
      sectors[_DEBUG_SECTOR].ceilingheight >> FRACBITS,
      sectors[_DEBUG_SECTOR].floorheight >> FRACBITS
   );
#endif
#if _PRED_DEBUG || _SECTOR_PRED_DEBUG
   // printf("CL_PredictPlayerPosition: Done.\n");
#endif
}

void CL_PredictSectorPositions(unsigned int index)
{
   ceiling_netid_t *ceiling_netid = NULL;
   door_netid_t *door_netid = NULL;
   floor_netid_t *floor_netid = NULL;
   elevator_netid_t *elevator_netid = NULL;
   pillar_netid_t *pillar_netid = NULL;
   floorwaggle_netid_t *waggle_netid = NULL;
   platform_netid_t *platform_netid = NULL;

#if _SECTOR_PRED_DEBUG
   printf(
      "CL_PredictSectorPositions (%3u/%3u): Predicting for index %3u.\n",
      cl_current_world_index,
      cl_latest_world_index,
      index
   );
#endif

   cl_predicting_sectors = true;

   while((ceiling_netid = CS_IterateCeilings(ceiling_netid)) != NULL)
   {
      if(ceiling_netid->ceiling->inactive == 0 ||
         index <= ceiling_netid->ceiling->inactive)
      {
         ceiling_netid->ceiling->thinker.function(ceiling_netid->ceiling);
      }
   }
   while((door_netid = CS_IterateDoors(door_netid)) != NULL)
   {
      if(door_netid->door->inactive == 0 ||
         index <= door_netid->door->inactive)
      {
         door_netid->door->thinker.function(door_netid->door);
      }
   }
   while((floor_netid = CS_IterateFloors(floor_netid)) != NULL)
   {
      if(floor_netid->floor->inactive == 0 ||
         index <= floor_netid->floor->inactive)
      {
         floor_netid->floor->thinker.function(floor_netid->floor);
      }
   }
   while((elevator_netid = CS_IterateElevators(elevator_netid)) != NULL)
   {
      if(elevator_netid->elevator->inactive == 0 ||
         index <= elevator_netid->elevator->inactive)
      {
         continue;
      }
      elevator_netid->elevator->thinker.function(elevator_netid->elevator);
   }
   while((pillar_netid = CS_IteratePillars(pillar_netid)) != NULL)
   {
      if(pillar_netid->pillar->inactive == 0 ||
         index <= pillar_netid->pillar->inactive)
      {
         continue;
      }
      pillar_netid->pillar->thinker.function(pillar_netid->pillar);
   }
   while((waggle_netid = CS_IterateFloorWaggles(waggle_netid)) != NULL)
   {
      if(waggle_netid->floorwaggle->inactive == 0 ||
         index <= waggle_netid->floorwaggle->inactive)
      {
         continue;
      }
      waggle_netid->floorwaggle->thinker.function(
         waggle_netid->floorwaggle
      );
   }
   while((platform_netid = CS_IteratePlatforms(platform_netid)) != NULL)
   {
      if(platform_netid->platform->inactive == 0 ||
         index <= platform_netid->platform->inactive)
      {
         platform_netid->platform->thinker.function(
            platform_netid->platform
         );
      }
   }

   cl_predicting_sectors = false;
#if _SECTOR_PRED_DEBUG
   printf("CL_PredictSectorPositions: Done.\n");
#endif
}

void CL_Predict(unsigned int start_index, unsigned int end_index,
                boolean repredicting)
{
   unsigned int i;

#if _PRED_DEBUG
   printf(
      "CL_Predict: Predicting from %u to %u.\n",
      start_index,
      end_index
   );
#endif
   cl_predicting = repredicting;
   for(i = start_index; i < end_index; i++)
   {
      if(!repredicting)
         CL_LoadSectorPositions(i);

      CL_PredictPlayerPosition(i);
   }
   cl_predicting = !repredicting;
   CL_LoadSectorState(cl_current_world_index);
   CL_LoadSectorPositions(cl_current_world_index);
#if _PRED_DEBUG
   printf("CL_Predict: Done.\n");
#endif
}

