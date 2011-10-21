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

#include <stdlib.h>
#include <string.h>

#include "d_event.h"
#include "doomstat.h"
#include "i_system.h"
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
static position_t last_server_position;
static cs_floor_status_e last_floor_status;

void CL_InitPrediction(void)
{
   cl_current_world_index = 0;
   memset(local_commands, 0, MAX_POSITIONS * sizeof(cs_cmd_t));
}

cs_cmd_t* CL_GetCurrentCommand(void)
{
   return &local_commands[cl_current_world_index % MAX_POSITIONS];
}

void CL_PredictPlayerPosition(unsigned int index, bool think)
{
   CS_RunPlayerCommand(
      consoleplayer, &local_commands[index % MAX_POSITIONS].ticcmd, think
   );
}

void CL_PredictFrom(unsigned int start, unsigned int end)
{
   unsigned int i;

   for(i = start; i < end; i++)
      CL_PredictPlayerPosition(i, true);
}

void CL_RePredict(unsigned int start, unsigned int end)
{
   unsigned int i;

   cl_predicting = true;
   for(i = start; i < end; i++)
   {
      CL_LoadSectorPositions(i);
      CL_PredictPlayerPosition(i, true);
   }
   cl_predicting = false;
   CL_LoadSectorPositions(cl_current_world_index);

}

void CL_StoreLastServerPosition(position_t *new_server_position,
                                uint32_t index,
                                cs_floor_status_e floor_status)
{
   CS_CopyPosition(&last_server_position, new_server_position);
   last_server_position.world_index = index;
   last_floor_status = floor_status;
}

void CL_LoadLastServerPosition(void)
{
   CS_SetPlayerPosition(consoleplayer, &last_server_position);
   clients[consoleplayer].floor_status = last_floor_status;
}

uint32_t CL_GetLastServerPositionIndex(void)
{
   return last_server_position.world_index;
}

