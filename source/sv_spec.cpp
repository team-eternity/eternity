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
   {
      return;
   }

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
      {
         SV_BroadcastSectorPosition(i);
#if 0
#if _SECTOR_PRED_DEBUG
         if(i == _DEBUG_SECTOR && playeringame[1])
         {
            printf(
               "SV_SaveSectorPositions (%u): Changed position of sector %u "
               "from %d/%d/%d to %d/%d/%d.\n",
               sv_world_index,
               i,
               old_position->index,
               old_position->ceiling_height >> FRACBITS,
               old_position->floor_height >> FRACBITS,
               new_position->index,
               new_position->ceiling_height >> FRACBITS,
               new_position->floor_height >> FRACBITS
            );
         }
#endif
#endif
      }
   }
}

void SV_BroadcastMapSpecialStatuses(void)
{
#if 0
   ceiling_netid_t *ceiling_netid = NULL;
   door_netid_t *door_netid = NULL;
   floor_netid_t *floor_netid = NULL;
   elevator_netid_t *elevator_netid = NULL;
   pillar_netid_t *pillar_netid = NULL;
   floorwaggle_netid_t *floorwaggle_netid = NULL;
   platform_netid_t *platform_netid = NULL;

   // [CG] Special types that don't actually change the C type of the special
   //      (i.e. ms_door_tagged and ms_door_manual are both
   //      VerticalDoorThinker) are inconsequential here, so in those cases
   //      I've just used the top value that corresponds with the actual C++
   //      type.

   while((ceiling_netid = CS_IterateCeilings(ceiling_netid)) != NULL)
   {
      SV_BroadcastMapSpecialStatus(ceiling_netid->ceiling, ms_ceiling);
   }

   while((door_netid = CS_IterateDoors(door_netid)) != NULL)
   {
      SV_BroadcastMapSpecialStatus(door_netid->door, ms_door_tagged);
#if _SECTOR_PRED_DEBUG
      // P_PrintDoor(door_netid->door);
#endif
   }

   while((floor_netid = CS_IterateFloors(floor_netid)) != NULL)
   {
      SV_BroadcastMapSpecialStatus(floor_netid->floor, ms_floor);
   }

   while((elevator_netid = CS_IterateElevators(elevator_netid)) != NULL)
   {
      SV_BroadcastMapSpecialStatus(elevator_netid->elevator, ms_elevator);
   }

   while((pillar_netid = CS_IteratePillars(pillar_netid)) != NULL)
   {
      SV_BroadcastMapSpecialStatus(pillar_netid->pillar, ms_pillar_build);
   }

   while((floorwaggle_netid = CS_IterateFloorWaggles(floorwaggle_netid))
         != NULL)
   {
      SV_BroadcastMapSpecialStatus(
         floorwaggle_netid->floorwaggle, ms_floorwaggle
      );
   }

   while((platform_netid = CS_IteratePlatforms(platform_netid)) != NULL)
   {
      SV_BroadcastMapSpecialStatus(platform_netid->platform, ms_platform);
#if _SECTOR_PRED_DEBUG
      // P_PrintPlatform(platform_netid->platform);
#endif
   }
#endif
}

