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
//   Serverside routines for handling sectors and map specials.
//
//----------------------------------------------------------------------------

#include "r_defs.h"
#include "r_state.h"

#include "cs_spec.h"
#include "cs_netid.h"
#include "cs_main.h"
#include "sv_main.h"
#include "sv_spec.h"

void SV_LoadSectorPositionAt(unsigned int sector_number, unsigned int index)
{
   sector_t *sector = &sectors[sector_number];
   sector_position_t *sector_position =
      &cs_sector_positions[sector_number][index % MAX_POSITIONS];

   CS_SetSectorPosition(sector, sector_position);
}

void SV_LoadCurrentSectorPosition(unsigned int sector_number)
{
   SV_LoadSectorPositionAt(sector_number, sv_world_index);
}

void SV_SaveSectorPositions(void)
{
   size_t i;
   sector_position_t *old_position, *new_position;

   if(sv_world_index == 0)
      return;

   for(i = 0; i < (unsigned int)numsectors; i++)
   {
      old_position =
         &cs_sector_positions[i][(sv_world_index - 1) % MAX_POSITIONS];
      new_position =
         &cs_sector_positions[i][sv_world_index % MAX_POSITIONS];
      CS_SaveSectorPosition(new_position, &sectors[i]);
      new_position->world_index = sv_world_index;
#if _SECTOR_PRED_DEBUG
      if(i == _DEBUG_SECTOR)
      {
         printf(
            "SV_SaveSectorPositions (%u): Sector %u: %d/%d.\n",
            sv_world_index,
            i,
            sectors[i].ceilingheight >> FRACBITS,
            sectors[i].floorheight >> FRACBITS
         );
      }
#endif
      if(!CS_SectorPositionsEqual(old_position, new_position))
         SV_BroadcastSectorPosition(i);
   }
}

void SV_BroadcastSectorThinkerStatuses(void)
{
   NetIDToObject<SectorThinker> *nito = NULL;

   while((nito = NetSectorThinkers.iterate(nito)))
      SV_BroadcastSectorThinkerStatus(nito->object);
}

