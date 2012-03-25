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

#include "m_fixed.h"
#include "p_mobj.h"
#include "p_portal.h"
#include "r_defs.h"
#include "r_state.h"

#include "cs_spec.h"
#include "cs_main.h"
#include "cs_netid.h"
#include "cl_main.h"

sector_position_t **cs_sector_positions = NULL;

static int old_numsectors = 0;

void CS_InitSectorPositions(void)
{
   int i;

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

   for(i = 0; i < numsectors; i++)
   {
      cs_sector_positions[i] = ecalloc(
         sector_position_t *, MAX_POSITIONS, sizeof(sector_position_t)
      );
      CS_SaveSectorPosition(&cs_sector_positions[i][0], &sectors[i]);
   }
   old_numsectors = numsectors;
}

void CS_SetSectorPosition(sector_t *sector, sector_position_t *position)
{
   P_SetCeilingHeight(sector, position->ceiling_height);
   P_SetFloorHeight(sector, position->floor_height);
}

bool CS_SectorPositionsEqual(sector_position_t *position_one,
                                sector_position_t *position_two)
{
   if(position_one->ceiling_height != position_two->ceiling_height)
      return false;

   if(position_one->floor_height != position_two->floor_height)
      return false;

   return true;
}

void CS_SaveSectorPosition(sector_position_t *position, sector_t *sector)
{
   position->ceiling_height = sector->ceilingheight;
   position->floor_height   = sector->floorheight;
}

void CS_CopySectorPosition(sector_position_t *position_one,
                           sector_position_t *position_two)
{
   position_one->world_index    = position_two->world_index;
   position_one->ceiling_height = position_two->ceiling_height;
   position_one->floor_height   = position_two->floor_height;
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

