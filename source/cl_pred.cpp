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

/*
#include "doomdef.h"
#include "info.h"
#include "e_states.h"
#include "p_info.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_map3d.h"
#include "p_mobj.h"
#include "p_tick.h"
#include "p_user.h"
#include "r_state.h"
#include "e_ttypes.h"

#include "cs_cmd.h"
#include "cs_position.h"
#include "cs_netid.h"
#include "cs_main.h"
#include "cl_main.h"
#include "cl_spec.h"
#include "cl_pred.h"
*/

#include <stdlib.h>
#include <string.h>

#include "doomstat.h"
#include "r_state.h"
#include "tables.h"
#include "p_user.h"

#include "cs_cmd.h"
#include "cs_main.h"
#include "cs_position.h"
#include "cl_main.h"
#include "cl_spec.h"

bool cl_predicting = false;

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

   P_PlayerThink(&players[consoleplayer]);
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

void CL_Predict(unsigned int start_index, unsigned int end_index,
                bool repredicting)
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

