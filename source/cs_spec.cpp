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
//   C/S routines for handling sectors and map specials.
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "doomstat.h"
#include "m_file.h"
#include "m_fixed.h"
#include "p_mobj.h"
#include "p_portal.h"
#include "r_defs.h"
#include "r_state.h"

#include "cs_spec.h"
#include "cs_main.h"
#include "cs_netid.h"
#include "cl_main.h"
#include "cl_spec.h"
#include "sv_main.h"

static int old_numsectors = 0;

#if _SECTOR_PRED_DEBUG
static bool opened_log_file = false;
static FILE *specfd;
#endif

static sector_position_t **cs_sector_positions = NULL;

void CS_LogSMT(const char *fmt, ...)
{
#if _SECTOR_PRED_DEBUG
   va_list args;

   if(CS_CLIENT && opened_log_file)
   {
      va_start(args, fmt);
      vfprintf(specfd, fmt, args);
      va_end(args);
   }
#endif
}

#if _SECTOR_PRED_DEBUG
static void CS_closeSpecLog(void)
{
   if(CS_CLIENT && opened_log_file)
   {
      M_FlushFile(specfd);
      M_CloseFile(specfd);
   }
}
#endif

void CS_InitSectorPositions(void)
{
   int i;
   uint32_t index;

#if _SECTOR_PRED_DEBUG
   if(CS_CLIENT && (!opened_log_file))
   {
      specfd = M_OpenFile("E:/Code/eecs/codeblocks/bin/Debug/spec.log", "wb");
      if(!specfd)
         I_Error("Error opening log: %s.\n", M_GetFileSystemErrorMessage());

      atexit(CS_closeSpecLog);

      opened_log_file = true;
   }
#endif

   if(cs_sector_positions != NULL)
   {
      for(i = 0; i < old_numsectors; i++)
         efree(cs_sector_positions[i]);
   }

   cs_sector_positions = erealloc(
      sector_position_t **,
      cs_sector_positions,
      numsectors * sizeof(sector_position_t *)
   );

   memset(cs_sector_positions, 0, numsectors * sizeof(sector_position_t *));

   if(CS_CLIENT)
      index = cl_latest_world_index;
   else if(CS_SERVER)
      index = sv_world_index;
   else
      index = gametic;

   for(i = 0; i < numsectors; i++)
   {
      cs_sector_positions[i] = emalloc(
         sector_position_t *, MAX_POSITIONS * sizeof(sector_position_t)
      );

      CS_SaveSectorPosition(index, i);
   }

   old_numsectors = numsectors;

   if(CS_CLIENT)
      CL_InitSectorPositions();
}

sector_position_t* CS_GetSectorPosition(uint32_t sector_number, uint32_t index)
{
   return &cs_sector_positions[sector_number][index % MAX_POSITIONS];
}

void CS_SetSectorPosition(uint32_t sector_number, uint32_t index)
{
   sector_t *sector = &sectors[sector_number];
   sector_position_t *pos = CS_GetSectorPosition(sector_number, index);

   if(sector_number == _DEBUG_SECTOR)
   {
      CS_LogSMT(
         "%u/%u: CS_SetSectorPosition (%u) %u/%d/%d.\n",
         cl_latest_world_index,
         cl_current_world_index,
         index,
         pos->world_index,
         pos->ceiling_height >> FRACBITS,
         pos->floor_height >> FRACBITS
      );
   }

   P_SetCeilingHeight(sector, pos->ceiling_height);
   P_SetFloorHeight(sector, pos->floor_height);
}

bool CS_SectorPositionsEqual(uint32_t sector_number, uint32_t index_one,
                             uint32_t index_two)
{
   sector_position_t *one = CS_GetSectorPosition(sector_number, index_one);
   sector_position_t *two = CS_GetSectorPosition(sector_number, index_two);

   if(one->ceiling_height != two->ceiling_height)
      return false;

   if(one->floor_height != two->floor_height)
      return false;

   return true;
}

void CS_SaveSectorPosition(uint32_t index, uint32_t sector_number)
{
   sector_t *sector = &sectors[sector_number];
   sector_position_t *pos = CS_GetSectorPosition(sector_number, index);

   if(sector_number == _DEBUG_SECTOR)
   {
      CS_LogSMT(
         "%u/%u: CS_SaveSectorPosition %u/%d/%d.\n",
         cl_latest_world_index,
         cl_current_world_index,
         index,
         sector->ceilingheight >> FRACBITS,
         sector->floorheight >> FRACBITS
      );
   }

   pos->world_index    = index;
   pos->ceiling_height = sector->ceilingheight;
   pos->floor_height   = sector->floorheight;
}

void CS_PrintPositionForSector(uint32_t sector_number)
{
   sector_t *sector = &sectors[sector_number];

   printf(
      "Sector %u: %5u/%5u.\n",
      sector_number,
      sector->ceilingheight >> FRACBITS,
      sector->floorheight   >> FRACBITS
   );
}

void CS_PrintSectorPosition(size_t sector_number, sector_position_t *position)
{
   printf(
      "Sector %u: %u/%5u/%5u.\n",
      sector_number,
      position->world_index,
      position->ceiling_height >> FRACBITS,
      position->floor_height   >> FRACBITS
   );
}

bool CS_SectorPositionIs(sector_t *sector, sector_position_t *position)
{
   if(sector->ceilingheight != position->ceiling_height)
      return false;

   if(sector->floorheight != position->floor_height)
      return false;

   return true;
}

