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
//   Clientside routines for handling sectors and map specials.
//
//----------------------------------------------------------------------------

#include "doomtype.h"
#include "m_fixed.h"
#include "p_spec.h"
#include "r_defs.h"
#include "r_state.h"

#include "cs_netid.h"
#include "cs_spec.h"
#include "cs_main.h"
#include "cl_main.h"
#include "cl_pred.h"
#include "cl_spec.h"

bool cl_predicting_sectors = false;
bool cl_setting_sector_positions = false;

static int old_numsectors = 0;
static uint32_t *cl_latest_sector_position_indices = NULL;

void CL_InitSectorPositions()
{
   int i;
   uint32_t j;

   if(numsectors != old_numsectors)
   {
      cl_latest_sector_position_indices = erealloc(
         uint32_t *,
         cl_latest_sector_position_indices,
         numsectors * sizeof(uint32_t)
      );

      old_numsectors = numsectors;
   }

   for(i = 0; i < numsectors; i++)
   {
      cl_latest_sector_position_indices[i] = cl_latest_world_index;
      for(j = 0; j < MAX_POSITIONS; j++)
         CS_SaveSectorPosition(cl_latest_world_index + j, i);
   }
}

void CL_CarrySectorPositions(uint32_t old_index)
{
   int i;
   uint32_t j;
   sector_position_t *old_pos, *new_pos;

   CS_LogSMT("\n");

   for(i = 0; i < numsectors; i++)
   {
      old_pos = CS_GetSectorPosition(i, old_index);

      for(j = old_index + 1; j <= cl_latest_world_index; j++)
      {
         new_pos = CS_GetSectorPosition(i, j);

         if(new_pos->world_index != j)
         {
            new_pos->world_index = j;
            new_pos->ceiling_height = old_pos->ceiling_height;
            new_pos->floor_height = old_pos->floor_height;
         }
      }
   }

   CL_StoreLatestSectorPositionIndex();
}

void CL_SaveSectorPosition(uint32_t index, uint32_t sector_number,
                           sector_position_t *new_position)
{
   sector_position_t *pos = CS_GetSectorPosition(sector_number, index);

   pos->world_index    = new_position->world_index;
   pos->ceiling_height = new_position->ceiling_height;
   pos->floor_height   = new_position->floor_height;

   if(sector_number == _DEBUG_SECTOR)
   {
      CS_LogSMT(
         "%u/%u: SPM: %u (%d/%d).\n",
         cl_latest_world_index,
         cl_current_world_index,
         pos->world_index,
         pos->ceiling_height >> FRACBITS,
         pos->floor_height >> FRACBITS
      );
   }
   cl_latest_sector_position_indices[sector_number] = index;
}

void CL_LoadLatestSectorPositions()
{
   for(int i = 0; i < numsectors; i++)
      CS_SetSectorPosition(i, cl_latest_sector_position_indices[i]);
}

void CL_LoadSectorPositionsAt(uint32_t index)
{
   for(int i = 0; i < numsectors; i++)
      CS_SetSectorPosition(i, index);
}

void CL_ActivateAllSectorMovementThinkers()
{
   NetIDToObject<SectorMovementThinker> *nito = NULL;

   while((nito = NetSectorThinkers.iterate(nito)))
   {
      nito->object->inactive = 0;
      nito->object->storePreRePredictionStatus();
      // nito->object->loadStoredStatusUpdate();
   }
}

void CL_ResetAllSectorMovementThinkers()
{
   NetIDToObject<SectorMovementThinker> *nito = NULL;

   while((nito = NetSectorThinkers.iterate(nito)))
      nito->object->loadPreRePredictionStatus();
}

void CL_SaveAllSectorMovementThinkerStatuses()
{
   NetIDToObject<SectorMovementThinker> *nito = NULL;

   while((nito = NetSectorThinkers.iterate(nito)))
      nito->object->storePreRePredictionStatus();
}

void CL_SpawnPlatform(sector_t *sector, cs_sector_thinker_data_t *data,
                      cs_sector_thinker_spawn_data_t *spawn_data)
{
   static char seqname[15];
   PlatThinker *platform = new PlatThinker;

   platform->addThinker();
   platform->sector = sector;
   platform->netUpdate(cl_latest_world_index, data);
   platform->storePreRePredictionStatus();

   P_AddActivePlat(platform, NULL);
   strncpy(
      seqname, spawn_data->platform_spawn_data.seqname, sizeof(seqname) - 1
   );
   seqname[sizeof(seqname) - 1] = '\0';
   P_PlatSequence(sector, (const char *)seqname);
}

void CL_SpawnVerticalDoor(sector_t *sector, cs_sector_thinker_data_t *data,
                          cs_sector_thinker_spawn_data_t *spawn_data)
{
   VerticalDoorThinker *door = new VerticalDoorThinker;

   door->addThinker();
   door->sector = sector;
   door->netUpdate(cl_latest_world_index, data);
   door->storePreRePredictionStatus();

   NetSectorThinkers.add(door);

   if(spawn_data->door_spawn_data.make_sound)
   {
      P_DoorSequence(
         spawn_data->door_spawn_data.raise,
         spawn_data->door_spawn_data.turbo,
         spawn_data->door_spawn_data.bounce,
         sector
      );
   }
}

void CL_SpawnCeiling(sector_t *sector, cs_sector_thinker_data_t *data,
                     cs_sector_thinker_spawn_data_t *spawn_data)
{
   CeilingThinker *ceiling = new CeilingThinker;

   ceiling->addThinker();
   ceiling->sector = sector;
   ceiling->netUpdate(cl_latest_world_index, data);
   ceiling->storePreRePredictionStatus();

   P_AddActiveCeiling(ceiling);
   P_CeilingSequence(sector, spawn_data->ceiling_spawn_data.noise);
}

void CL_SpawnFloor(sector_t *sector, cs_sector_thinker_data_t *data,
                   cs_sector_thinker_spawn_data_t *spawn_data)
{
   FloorMoveThinker *floor = new FloorMoveThinker;

   floor->addThinker();
   floor->sector = sector;
   floor->netUpdate(cl_latest_world_index, data);
   floor->storePreRePredictionStatus();

   NetSectorThinkers.add(floor);

   if(spawn_data->floor_spawn_data.make_sound)
      P_FloorSequence(sector);
}

void CL_SpawnElevator(sector_t *sector, cs_sector_thinker_data_t *data,
                      cs_sector_thinker_spawn_data_t *spawn_data)
{
   ElevatorThinker *elevator = new ElevatorThinker;

   elevator->addThinker();
   elevator->sector = sector;
   elevator->netUpdate(cl_latest_world_index, data);
   elevator->storePreRePredictionStatus();

   NetSectorThinkers.add(elevator);

   if(spawn_data->floor_spawn_data.make_sound)
      P_FloorSequence(sector);
}

void CL_SpawnPillar(sector_t *sector, cs_sector_thinker_data_t *data,
                    cs_sector_thinker_spawn_data_t *spawn_data)
{
   PillarThinker *pillar = new PillarThinker;

   pillar->addThinker();
   pillar->sector = sector;
   pillar->netUpdate(cl_latest_world_index, data);
   pillar->storePreRePredictionStatus();

   NetSectorThinkers.add(pillar);

   if(spawn_data->floor_spawn_data.make_sound)
      P_FloorSequence(sector);
}

void CL_SpawnFloorWaggle(sector_t *sector, cs_sector_thinker_data_t *data,
                         cs_sector_thinker_spawn_data_t *spawn_data)
{
   FloorWaggleThinker *floor_waggle = new FloorWaggleThinker;

   floor_waggle->addThinker();
   floor_waggle->sector = sector;
   floor_waggle->netUpdate(cl_latest_world_index, data);
   floor_waggle->storePreRePredictionStatus();

   NetSectorThinkers.add(floor_waggle);

   if(spawn_data->floor_spawn_data.make_sound)
      P_FloorSequence(sector);
}

void CL_UpdatePlatform(cs_sector_thinker_data_t *data)
{
   PlatThinker *platform = NULL;
   SectorThinker *thinker = NULL;
   cs_platform_data_t *dt = &data->platform_data;

   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such platform %u.", dt->net_id);
   else if(!(platform = dynamic_cast<PlatThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a platform.", dt->net_id);
   else
      platform->netUpdate(cl_latest_world_index, data);
}

void CL_UpdateVerticalDoor(cs_sector_thinker_data_t *data)
{
   VerticalDoorThinker *door = NULL;
   SectorThinker *thinker = NULL;
   cs_door_data_t *dt = &data->door_data;

   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such door %u.", dt->net_id);
   else if(!(door = dynamic_cast<VerticalDoorThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a door.", dt->net_id);
   else
      door->netUpdate(cl_latest_world_index, data);
}

void CL_UpdateCeiling(cs_sector_thinker_data_t *data)
{
   CeilingThinker *ceiling = NULL;
   SectorThinker *thinker = NULL;
   cs_ceiling_data_t *dt = &data->ceiling_data;

   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such ceiling %u.", dt->net_id);
   else if(!(ceiling = dynamic_cast<CeilingThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a ceiling.", dt->net_id);
   else
      ceiling->netUpdate(cl_latest_world_index, data);
}

void CL_UpdateFloor(cs_sector_thinker_data_t *data)
{
   FloorMoveThinker *floor = NULL;
   SectorThinker *thinker = NULL;
   cs_floor_data_t *dt = &data->floor_data;

   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such floor %u.", dt->net_id);
   else if(!(floor = dynamic_cast<FloorMoveThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a floor.", dt->net_id);
   else
      floor->netUpdate(cl_latest_world_index, data);
}

void CL_UpdateElevator(cs_sector_thinker_data_t *data)
{
   ElevatorThinker *elevator = NULL;
   SectorThinker *thinker = NULL;
   cs_elevator_data_t *dt = &data->elevator_data;

   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such elevator %u.", dt->net_id);
   else if(!(elevator = dynamic_cast<ElevatorThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a elevator.", dt->net_id);
   else
      elevator->netUpdate(cl_latest_world_index, data);
}

void CL_UpdatePillar(cs_sector_thinker_data_t *data)
{
   PillarThinker *pillar = NULL;
   SectorThinker *thinker = NULL;
   cs_pillar_data_t *dt = &data->pillar_data;

   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such pillar %u.", dt->net_id);
   else if(!(pillar = dynamic_cast<PillarThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a pillar.", dt->net_id);
   else
      pillar->netUpdate(cl_latest_world_index, data);
}

void CL_UpdateFloorWaggle(cs_sector_thinker_data_t *data)
{
   FloorWaggleThinker *floorwaggle = NULL;
   SectorThinker *thinker = NULL;
   cs_floorwaggle_data_t *dt = &data->floorwaggle_data;

   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such floorwaggle %u.", dt->net_id);
   else if(!(floorwaggle = dynamic_cast<FloorWaggleThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a floor waggle.", dt->net_id);
   else
      floorwaggle->netUpdate(cl_latest_world_index, data);
}

