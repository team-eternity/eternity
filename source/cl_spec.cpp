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
#include "cl_spec.h"

bool cl_predicting_sectors = false;
bool cl_setting_sector_positions = false;

void CL_LoadSectorPositions(unsigned int index)
{
   unsigned int i;
   sector_position_t *position;

   cl_setting_sector_positions = true;

   for(i = 0; i < numsectors; i++)
   {
      position = &cs_sector_positions[i][index % MAX_POSITIONS];
      if(position->world_index != index)
      {
         continue;
      }
      CS_SetSectorPosition(&sectors[i], position);
#if _SECTOR_PRED_DEBUG
      if(i == _DEBUG_SECTOR)
      {
         printf(
            "CL_LoadSectorPositions (%3u/%3u): "
            "Sector %2u: %3u %5u %5u.\n",
            cl_current_world_index,
            cl_latest_world_index,
            i,
            position->world_index,
            position->ceiling_height >> FRACBITS,
            position->floor_height   >> FRACBITS
         );
      }
#endif
   }
   cl_setting_sector_positions = false;
}

#if 0

void CL_LoadCurrentSectorState(void)
{
   CL_LoadSectorState(cl_current_world_index);
}

void CL_LoadCurrentSectorPositions(void)
{
   CL_LoadSectorPositions(cl_current_world_index);
}

void CL_LoadSectorState(uint32_t index)
{
#if _SECTOR_PRED_DEBUG
   printf("CL_LoadSectorState: Loading state for index %u.\n", index);
#endif
}

void CL_SpawnParamCeiling(line_t *line, sector_t *sector,
                          CeilingThinker::status_t *status,
                          cs_ceilingdata_t *data)
{
   ceilingdata_t cd;

   cd.trigger_type = data->trigger_type;
   cd.crush = data->crush;
   cd.direction = data->direction;
   cd.speed_type = data->speed_type;
   cd.change_type = data->change_type;
   cd.change_model = data->change_model;
   cd.target_type = data->target_type;
   cd.height_value = data->height_value;
   cd.speed_value = data->speed_value;

   CeilingThinker *ceiling = P_SpawnParamCeiling(line, sector, &cd);
   ceiling->setStatus(status);
   NetCeilings.add(ceiling);
}

void CL_SpawnParamDoor(line_t *line, sector_t *sector,
                       VerticalDoorThinker::status_t *status,
                       cs_doordata_t *data)
{
   doordata_t dd;

   dd.delay_type     = data->delay_type;
   dd.kind           = data->kind;
   dd.speed_type     = data->speed_type;
   dd.trigger_type   = data->trigger_type;
   dd.speed_value    = data->speed_value;
   dd.delay_value    = data->delay_value;
   dd.altlighttag    = data->altlighttag;
   dd.usealtlighttag = data->usealtlighttag;
   dd.topcountdown   = data->topcountdown;

   VerticalDoorThinker *door = P_SpawnParamDoor(line, sector, &dd);
   door->setStatus(status);
   NetDoors.add(door);
}

void CL_SpawnParamFloor(line_t *line, sector_t *sector,
                        FloorMoveThinker::status_t *status,
                        cs_floordata_t *data)
{
   floordata_t fd;

   fd.trigger_type = data->trigger_type;
   fd.crush        = data->crush;
   fd.direction    = data->direction;
   fd.speed_type   = data->speed_type;
   fd.change_type  = data->change_type;
   fd.change_model = data->change_model;
   fd.target_type  = data->target_type;
   fd.height_value = data->height_value;
   fd.speed_value  = data->speed_value;

   FloorMoveThinker *floor = P_SpawnParamFloor(line, sector, &fd);
   floor->setStatus(status);
   NetFloors.add(floor);
}

void CL_SpawnCeilingFromStatus(line_t *line, sector_t *sector,
                               CeilingThinker::status_t *status)
{
   CeilingThinker *ceiling = P_SpawnCeiling(
      line, sector, (ceiling_e)status->type
   );

   ceiling->setStatus(status);
   NetCeilings.add(ceiling);
}

void CL_SpawnDoorFromStatus(line_t *line, sector_t *sector,
                            VerticalDoorThinker::status_t *status,
                            map_special_e type)
{
   VerticalDoorThinker *door;

   switch(type)
   {
   case ms_door_tagged:
      door = P_SpawnTaggedDoor(line, sector, (vldoor_e)status->type);
      break;
   case ms_door_manual:
      door = P_SpawnManualDoor(line, sector);
      break;
   case ms_door_closein30:
      door = P_SpawnDoorCloseIn30(sector);
      break;
   case ms_door_raisein300:
      door = P_SpawnDoorRaiseIn5Mins(sector);
      break;
   default:
      I_Error("Unknown door type %d.\n", type);
      break;
   }

   door->setStatus(status);
   NetDoors.add(door);
}

void CL_SpawnFloorFromStatus(line_t *line, sector_t *sector,
                             FloorMoveThinker::status_t *status,
                             map_special_e type)
{
   FloorMoveThinker *floor;

   switch(type)
   {
   case ms_floor:
   case ms_stairs:
      floor = P_SpawnFloor(line, sector, (floor_e)status->type);
      break;
   case ms_donut:
      floor = P_SpawnDonut(
         line, sector, status->texture, status->floor_dest_height
      );
      break;
   case ms_donut_hole:
      floor = P_SpawnDonutHole(line, sector, status->floor_dest_height);
      break;
   default:
      I_Error("Unknown floor type %d.\n", type);
      break;
   }

   floor->setStatus(status);
   NetFloors.add(floor);
}

void CL_SpawnElevatorFromStatus(line_t *line, sector_t *sector,
                                ElevatorThinker::status_t *status)
{
   ElevatorThinker *elevator = P_SpawnElevator(
      line, sector, (elevator_e)status->type
   );

   elevator->setStatus(status);
   NetElevators.add(elevator);
}

void CL_SpawnPillarFromStatus(line_t *line, sector_t *sector,
                              PillarThinker::status_t *status,
                              map_special_e type)
{
   PillarThinker *pillar;

   switch(type)
   {
   case ms_pillar_build:
      pillar = P_SpawnBuildPillar(line, sector, 0, 0, 0);
      break;
   case ms_pillar_open:
      pillar = P_SpawnOpenPillar(line, sector, 0, 1, 1);
      break;
   default:
      I_Error("Unknown pillar type %d.\n", type);
      break;
   }

   pillar->setStatus(status);
   NetPillars.add(pillar);
}

void CL_SpawnFloorWaggleFromStatus(line_t *line, sector_t *sector,
                                   FloorWaggleThinker::status_t *status)
{
   FloorWaggleThinker *waggle = P_SpawnFloorWaggle(line, sector, 0, 0, 0, 0);

   waggle->setStatus(status);
   NetFloorWaggles.add(waggle);
}

void CL_SpawnPlatformFromStatus(line_t *line, sector_t *sector,
                                PlatThinker::status_t *status)
{
   PlatThinker *platform = P_SpawnPlatform(
      line, sector, 0, (plattype_e)status->type
   );

   platform->setStatus(status);
   NetPlatforms.add(platform);
}

void CL_SpawnGenPlatformFromStatus(line_t *line, sector_t *sector,
                                   PlatThinker::status_t *status)
{
   PlatThinker *platform = P_SpawnGenPlatform(line, sector);

   platform->setStatus(status);
   NetPlatforms.add(platform);
}

void CL_SpawnParamCeilingFromBlob(line_t *line, sector_t *sector, void *blob)
{
   CeilingThinker::status_t *status = (CeilingThinker::status_t *)blob;
   cs_ceilingdata_t *data = (cs_ceilingdata_t *)(
      (char *)blob) + sizeof(CeilingThinker::status_t
   );

   CL_SpawnParamCeiling(line, sector, status, data);
}

void CL_SpawnParamDoorFromBlob(line_t *line, sector_t *sector, void *blob)
{
   VerticalDoorThinker::status_t *status =
      (VerticalDoorThinker::status_t *)blob;
   cs_doordata_t *data = (cs_doordata_t *)(
      (char *)blob) + sizeof(VerticalDoorThinker::status_t
   );

   CL_SpawnParamDoor(line, sector, status, data);
}

void CL_SpawnParamFloorFromBlob(line_t *line, sector_t *sector, void *blob)
{
   FloorMoveThinker::status_t *status = (FloorMoveThinker::status_t *)blob;
   cs_floordata_t *data = (cs_floordata_t *)(
      (char *)blob) + sizeof(FloorMoveThinker::status_t
   );

   CL_SpawnParamFloor(line, sector, status, data);
}

void CL_SpawnCeilingFromStatusBlob(line_t *line, sector_t *sector, void *blob)
{
   CeilingThinker::status_t *status = (CeilingThinker::status_t *)blob;

   CL_SpawnCeilingFromStatus(line, sector, status);
}

void CL_SpawnDoorFromStatusBlob(line_t *line, sector_t *sector, void *blob,
                                map_special_e type)
{
   VerticalDoorThinker::status_t *status =
      (VerticalDoorThinker::status_t *)blob;

   CL_SpawnDoorFromStatus(line, sector, status, type);
}

void CL_SpawnFloorFromStatusBlob(line_t *line, sector_t *sector, void *blob,
                                 map_special_e type)
{
   FloorMoveThinker::status_t *status = (FloorMoveThinker::status_t *)blob;

   CL_SpawnFloorFromStatus(line, sector, status, type);
}

void CL_SpawnElevatorFromStatusBlob(line_t *line, sector_t *sector, void *blob)
{
   ElevatorThinker::status_t *status = (ElevatorThinker:: status_t *)blob;

   CL_SpawnElevatorFromStatus(line, sector, status);
}

void CL_SpawnPillarFromStatusBlob(line_t *line, sector_t *sector, void *blob,
                                  map_special_e type)
{
   PillarThinker::status_t *status = (PillarThinker:: status_t *)blob;

   CL_SpawnPillarFromStatus(line, sector, status, type);
}

void CL_SpawnFloorWaggleFromStatusBlob(line_t *line, sector_t *sector,
                                       void *blob)
{
   FloorWaggleThinker::status_t *status =
      (FloorWaggleThinker:: status_t *)blob;

   CL_SpawnFloorWaggleFromStatus(line, sector, status);
}

void CL_SpawnPlatformFromStatusBlob(line_t *line, sector_t *sector, void *blob)
{
   PlatThinker::status_t *status = (PlatThinker:: status_t *)blob;

   CL_SpawnPlatformFromStatus(line, sector, status);
}

void CL_SpawnGenPlatformFromStatusBlob(line_t *line, sector_t *sector,
                                       void *blob)
{
   PlatThinker::status_t *status = (PlatThinker:: status_t *)blob;

   CL_SpawnGenPlatformFromStatus(line, sector, status);
}

void CL_ApplyCeilingStatusFromBlob(void *blob)
{
   CeilingThinker *t;
   CeilingThinker::status_t *status = (CeilingThinker::status_t *)blob;

   if((t = NetCeilings.lookup(status->net_id)))
      t->setStatus(status);
}

void CL_ApplyDoorStatusFromBlob(void *blob)
{
   VerticalDoorThinker *t;
   VerticalDoorThinker::status_t *status =
      (VerticalDoorThinker::status_t *)blob;

   if((t = NetDoors.lookup(status->net_id)))
      t->setStatus(status);
}

void CL_ApplyFloorStatusFromBlob(void *blob)
{
   FloorMoveThinker *t;
   FloorMoveThinker::status_t *status = (FloorMoveThinker::status_t *)blob;

   if((t = NetFloors.lookup(status->net_id)))
      t->setStatus(status);
}

void CL_ApplyElevatorStatusFromBlob(void *blob)
{
   ElevatorThinker *t;
   ElevatorThinker::status_t *status = (ElevatorThinker::status_t *)blob;

   if((t = NetElevators.lookup(status->net_id)))
      t->setStatus(status);
}

void CL_ApplyPillarStatusFromBlob(void *blob)
{
   PillarThinker *t;
   PillarThinker::status_t *status = (PillarThinker::status_t *)blob;

   if((t = NetPillars.lookup(status->net_id)))
      t->setStatus(status);
}

void CL_ApplyFloorWaggleStatusFromBlob(void *blob)
{
   FloorWaggleThinker *t;
   FloorWaggleThinker::status_t *status = (FloorWaggleThinker::status_t *)blob;

   if((t = NetFloorWaggles.lookup(status->net_id)))
      t->setStatus(status);
}

void CL_ApplyPlatformStatusFromBlob(void *blob)
{
   PlatThinker *t;
   PlatThinker::status_t *status = (PlatThinker::status_t *)blob;

   if((t = NetPlatforms.lookup(status->net_id)))
      t->setStatus(status);
}

// [CG] For debugging.
void CL_PrintSpecialStatuses(void)
{
#if _SECTOR_PRED_DEBUG
   door_netid_t *door_netid = NULL;
   platform_netid_t *platform_netid = NULL;

   while((door_netid = CS_IterateDoors(door_netid)) != NULL)
      P_PrintDoor(door_netid->door);

   while((platform_netid = CS_IteratePlatforms(platform_netid)) != NULL)
      P_PrintPlatform(platform_netid->platform);

   printf("===\n");
#endif
}

#endif

