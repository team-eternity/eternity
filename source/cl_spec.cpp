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
#include "m_dllist.h"
#include "e_hash.h"
#include "p_spec.h"
#include "r_defs.h"
#include "r_state.h"

#include "cs_netid.h"
#include "cs_spec.h"
#include "cs_main.h"
#include "cl_main.h"
#include "cl_spec.h"

boolean cl_predicting_sectors = false;
boolean cl_setting_sector_positions = false;

ehash_t *cl_sector_position_hash = NULL;
ehash_t *cl_ms_status_hash = NULL;

static const void* SPHashFunc(void *object)
{
   return &((sector_position_buffer_node_t *)object)->world_index;
}

static const void* MSStatusHashFunc(void *object)
{
   return &((ms_status_buffer_node_t *)object)->world_index;
}

static void CL_InitSectorPositionBuffer(void)
{
   sector_position_buffer_node_t *node = NULL;

   if(cl_sector_position_hash == NULL)
   {
      cl_sector_position_hash = calloc(1, sizeof(ehash_t));
      E_UintHashInit(
         cl_sector_position_hash, SPEC_INIT_CHAINS, SPHashFunc, NULL
      );
   }
   else
   {
      FOR_ALL_SECTOR_POSITION_NODES
      {
         if(node)
         {
            E_HashRemoveObject(cl_sector_position_hash, node);
            free(node);
            node = NULL;
         }
      }
   }
}

static void CL_InitMapSpecialStatusBuffer(void)
{
   ms_status_buffer_node_t *node = NULL;

   if(cl_ms_status_hash == NULL)
   {
      cl_ms_status_hash = calloc(1, sizeof(ehash_t));
      E_UintHashInit(
         cl_ms_status_hash, SPEC_INIT_CHAINS, MSStatusHashFunc, NULL
      );
   }
   else
   {
      FOR_ALL_SPECIAL_STATUS_NODES
      {
         E_HashRemoveObject(cl_ms_status_hash, node);
         free(node->status);
         free(node);
         node = NULL;
      }
   }
}

void CL_SpecInit(void)
{
   CS_SpecInit();
   CL_InitSectorPositionBuffer();
   CL_InitMapSpecialStatusBuffer();
}

void CL_LoadSectorState(unsigned int index)
{
   ms_status_buffer_node_t *node = NULL;

#if _SECTOR_PRED_DEBUG
   printf("CL_LoadSectorState: Loading state for index %u.\n", index);
#endif

   FOR_SPECIAL_STATUS_NODES_AT(index)
   {
      switch(node->type)
      {
      case ms_ceiling:
      case ms_ceiling_param:
         CL_ApplyCeilingStatus((ceiling_status_t *)node->status);
         break;
      case ms_door_tagged:
      case ms_door_manual:
      case ms_door_closein30:
      case ms_door_raisein300:
      case ms_door_param:
         CL_ApplyDoorStatus((door_status_t *)node->status);
         break;
      case ms_floor:
      case ms_stairs:
      case ms_donut:
      case ms_donut_hole:
      case ms_floor_param:
         CL_ApplyFloorStatus((floor_status_t *)node->status);
         break;
      case ms_elevator:
         CL_ApplyElevatorStatus((elevator_status_t *)node->status);
         break;
      case ms_pillar_build:
      case ms_pillar_open:
         CL_ApplyPillarStatus((pillar_status_t *)node->status);
         break;
      case ms_floorwaggle:
         CL_ApplyFloorWaggleStatus((floorwaggle_status_t *)node->status);
         break;
      case ms_platform:
      case ms_platform_gen:
         CL_ApplyPlatformStatus((platform_status_t *)node->status);
         break;
      default:
         doom_printf(
            "CL_LoadSectorState: unknown node type %d, ignoring.\n", node->type
         );
         break;
      }
   }
}

void CL_BufferSectorPosition(nm_sectorposition_t *message)
{
   sector_position_buffer_node_t *node;

   node = calloc(1, sizeof(sector_position_buffer_node_t));

   CS_CopySectorPosition(&node->position, &message->sector_position);
   node->sector_number = message->sector_number;
   node->world_index = node->position.world_index = message->world_index;

#if _SECTOR_PRED_DEBUG
   if(node->sector_number == _DEBUG_SECTOR)
   {
      printf(
         "CL_BufferSectorPosition (%3u/%3u): Sector %2u: "
         "%3u/%3u %5u %5u.\n",
         cl_current_world_index,
         cl_latest_world_index,
         node->sector_number,
         node->world_index,
         message->sector_position.index,
         message->sector_position.ceiling_height >> FRACBITS,
         message->sector_position.floor_height   >> FRACBITS
      );
   }
#endif

   E_HashAddObject(cl_sector_position_hash, node);

   if(cl_sector_position_hash->loadfactor > SPEC_MAX_LOAD_FACTOR)
   {
      E_HashRebuild(
         cl_sector_position_hash, cl_sector_position_hash->numchains * 2
      );
   }
}

void CL_BufferMapSpecialStatus(nm_specialstatus_t *message, void *status)
{
   ms_status_buffer_node_t *node;

   node = calloc(1, sizeof(ms_status_buffer_node_t));
   node->status = NULL;

   switch(message->special_type)
   {
   case ms_ceiling:
   case ms_ceiling_param:
      node->status = calloc(1, sizeof(ceiling_status_t));
      memcpy(node->status, status, sizeof(ceiling_status_t));
      break;
   case ms_door_tagged:
   case ms_door_manual:
   case ms_door_closein30:
   case ms_door_raisein300:
   case ms_door_param:
      node->status = calloc(1, sizeof(door_status_t));
      memcpy(node->status, status, sizeof(door_status_t));
      break;
   case ms_floor:
   case ms_stairs:
   case ms_donut:
   case ms_donut_hole:
   case ms_floor_param:
      node->status = calloc(1, sizeof(floor_status_t));
      memcpy(node->status, status, sizeof(floor_status_t));
      break;
   case ms_elevator:
      node->status = calloc(1, sizeof(elevator_status_t));
      memcpy(node->status, status, sizeof(elevator_status_t));
      break;
   case ms_pillar_build:
   case ms_pillar_open:
      node->status = calloc(1, sizeof(pillar_status_t));
      memcpy(node->status, status, sizeof(pillar_status_t));
      break;
   case ms_floorwaggle:
      node->status = calloc(1, sizeof(floorwaggle_status_t));
      memcpy(node->status, status, sizeof(floorwaggle_status_t));
      break;
   case ms_platform:
   case ms_platform_gen:
      node->status = calloc(1, sizeof(platform_status_t));
      memcpy(node->status, status, sizeof(platform_status_t));
      break;
   default:
      doom_printf(
         "CL_BufferMapSpecialStatus: Received a map special status message "
         "with invalid type %d, ignoring.",
         message->special_type
      );
      free(node);
      return;
   }

   node->type = message->special_type;
   node->world_index = message->world_index;

   E_HashAddObject(cl_ms_status_hash, node);

   if(cl_ms_status_hash->loadfactor > SPEC_MAX_LOAD_FACTOR)
      E_HashRebuild(cl_ms_status_hash, cl_ms_status_hash->numchains * 2);
}

void CL_ProcessSectorPositions(unsigned int index)
{
   sector_position_buffer_node_t *node = NULL;

   cl_setting_sector_positions = true;
   FOR_SECTOR_POSITION_NODES_AT(index)
   {
      CS_SetSectorPosition(&sectors[node->sector_number], &node->position);
      CS_CopySectorPosition(
         &cs_sector_positions[node->sector_number][index % MAX_POSITIONS],
         &node->position
      );
#if _SECTOR_PRED_DEBUG
      if(node->sector_number == _DEBUG_SECTOR)
      {
         printf(
            "CL_ProcessSectorPositions(%3u/%3u): Set sector %u's "
            "position: %d/%d.\n",
            cl_current_world_index,
            cl_latest_world_index,
            node->sector_number,
            sectors[node->sector_number].ceilingheight >> FRACBITS,
            sectors[node->sector_number].floorheight >> FRACBITS
         );
      }
#endif
      E_HashRemoveObject(cl_sector_position_hash, node);
      free(node);
      node = NULL;
   }
   cl_setting_sector_positions = false;
}

void CL_SpawnParamCeiling(line_t *line, sector_t *sector,
                          ceiling_status_t *status, cs_ceilingdata_t *data)
{
   ceilingdata_t cd;
   ceiling_t *ceiling = NULL;

   cd.trigger_type = data->trigger_type;
   cd.crush = data->crush;
   cd.direction = data->direction;
   cd.speed_type = data->speed_type;
   cd.change_type = data->change_type;
   cd.change_model = data->change_model;
   cd.target_type = data->target_type;
   cd.height_value = data->height_value;
   cd.speed_value = data->speed_value;

   ceiling = P_SpawnParamCeiling(line, sector, &cd);
   CS_SetCeilingStatus(ceiling, status);
   CS_RegisterCeilingNetID(ceiling);
}

void CL_SpawnParamDoor(line_t *line, sector_t *sector, door_status_t *status,
                       cs_doordata_t *data)
{
   doordata_t dd;
   vldoor_t *door = NULL;

   dd.delay_type     = data->delay_type;
   dd.kind           = data->kind;
   dd.speed_type     = data->speed_type;
   dd.trigger_type   = data->trigger_type;
   dd.speed_value    = data->speed_value;
   dd.delay_value    = data->delay_value;
   dd.altlighttag    = data->altlighttag;
   dd.usealtlighttag = data->usealtlighttag;
   dd.topcountdown   = data->topcountdown;

   door = P_SpawnParamDoor(line, sector, &dd);
   CS_SetDoorStatus(door, status);
   CS_RegisterDoorNetID(door);
}

void CL_SpawnParamFloor(line_t *line, sector_t *sector, floor_status_t *status,
                        cs_floordata_t *data)
{
   floordata_t fd;
   floormove_t *floor = NULL;

   fd.trigger_type = data->trigger_type;
   fd.crush        = data->crush;
   fd.direction    = data->direction;
   fd.speed_type   = data->speed_type;
   fd.change_type  = data->change_type;
   fd.change_model = data->change_model;
   fd.target_type  = data->target_type;
   fd.height_value = data->height_value;
   fd.speed_value  = data->speed_value;

   floor = P_SpawnParamFloor(line, sector, &fd);
   CS_SetFloorStatus(floor, status);
   CS_RegisterFloorNetID(floor);
}

void CL_ApplyCeilingStatus(ceiling_status_t *status)
{
   ceiling_t *ceiling = CS_GetCeilingFromNetID(status->net_id);

   if(!ceiling)
      doom_printf("No ceiling for Net ID %u.\n", status->net_id);
   else
      CS_SetCeilingStatus(ceiling, status);
}

void CL_ApplyDoorStatus(door_status_t *status)
{
   vldoor_t *door = CS_GetDoorFromNetID(status->net_id);

   if(!door)
      doom_printf("No door for Net ID %u.\n", status->net_id);
   else
      CS_SetDoorStatus(door, status);
}

void CL_ApplyFloorStatus(floor_status_t *status)
{
   floormove_t *floor = CS_GetFloorFromNetID(status->net_id);

   if(!floor)
      doom_printf("No floor for Net ID %u.\n", status->net_id);
   else
      CS_SetFloorStatus(floor, status);
}

void CL_ApplyElevatorStatus(elevator_status_t *status)
{
   elevator_t *elevator = CS_GetElevatorFromNetID(status->net_id);

   if(!elevator)
      doom_printf("No elevator for Net ID %u.\n", status->net_id);
   else
      CS_SetElevatorStatus(elevator, status);
}

void CL_ApplyPillarStatus(pillar_status_t *status)
{
   pillar_t *pillar = CS_GetPillarFromNetID(status->net_id);

   if(!pillar)
      doom_printf("No pillar for Net ID %u.\n", status->net_id);
   else
      CS_SetPillarStatus(pillar, status);
}

void CL_ApplyFloorWaggleStatus(floorwaggle_status_t *status)
{
   floorwaggle_t *floorwaggle = CS_GetFloorWaggleFromNetID(status->net_id);

   if(!floorwaggle)
      doom_printf("No floorwaggle for Net ID %u.\n", status->net_id);
   else
      CS_SetFloorWaggleStatus(floorwaggle, status);
}

void CL_ApplyPlatformStatus(platform_status_t *status)
{
   plat_t *platform = CS_GetPlatformFromNetID(status->net_id);

   if(!platform)
      doom_printf("No platform for Net ID %u.\n", status->net_id);
   else
      CS_SetPlatformStatus(platform, status);
}

void CL_SpawnCeilingFromStatus(line_t *line, sector_t *sector,
                               ceiling_status_t *status)
{
   ceiling_t *ceiling = P_SpawnCeiling(line, sector, status->type);

   CS_SetCeilingStatus(ceiling, status);
   printf("CL_SpawnCeilingFromStatus: Registering ceiling Net ID.\n");
   CS_RegisterCeilingNetID(ceiling);
}

void CL_SpawnDoorFromStatus(line_t *line, sector_t *sector,
                            door_status_t *status, map_special_t type)
{
   vldoor_t *door;

   switch(type)
   {
   case ms_door_tagged:
      door = P_SpawnTaggedDoor(line, sector, status->type);
      break;
   case ms_door_manual:
      door = P_SpawnManualDoor(line, sector);
      break;
   case ms_door_closein30:
      door = P_SpawnDoorCloseIn30(sector);
      break;
   case ms_door_raisein300:
      door = P_SpawnDoorRaiseIn5Mins(sector, sector - sectors);
      break;
   default:
      I_Error("Unknown door type %d.\n", type);
      break;
   }
   CS_SetDoorStatus(door, status);
   CS_RegisterDoorNetID(door);
}

void CL_SpawnFloorFromStatus(line_t *line, sector_t *sector,
                             floor_status_t *status, map_special_t type)
{
   floormove_t *floor;

   switch(type)
   {
   case ms_floor:
      floor = P_SpawnFloor(line, sector, status->type);
      break;
   case ms_stairs:
      // [CG] TODO.
#if 0
      saved_floor = (floormove_t *)special;
      floor = P_SpawnStairs(line, sector, saved_floor->type);
      P_CopyFloor(floor, saved_floor);
      CS_RegisterFloorNetID(floor);
      // floor->thinker.function(floor);
#if _SPECIAL_DEBUG
      printf("Created new stairs, Net ID %d.\n", floor->net_id);
#endif
#endif
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

   CS_SetFloorStatus(floor, status);
   CS_RegisterFloorNetID(floor);
}

void CL_SpawnElevatorFromStatus(line_t *line, sector_t *sector,
                                elevator_status_t *status)
{
   elevator_t *elevator = P_SpawnElevator(line, sector, status->type);

   CS_SetElevatorStatus(elevator, status);
   CS_RegisterElevatorNetID(elevator);
}

void CL_SpawnPillarFromStatus(line_t *line, sector_t *sector,
                              pillar_status_t *status, map_special_t type)
{
   pillar_t *pillar;

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

   CS_SetPillarStatus(pillar, status);
   CS_RegisterPillarNetID(pillar);
}

void CL_SpawnFloorWaggleFromStatus(line_t *line, sector_t *sector,
                                   floorwaggle_status_t *status)
{
   floorwaggle_t *floorwaggle = P_SpawnFloorWaggle(line, sector, 0, 0, 0, 0);

   CS_SetFloorWaggleStatus(floorwaggle, status);
   CS_RegisterFloorWaggleNetID(floorwaggle);
}

void CL_SpawnPlatformFromStatus(line_t *line, sector_t *sector,
                                platform_status_t *status)
{
   plat_t *platform = P_SpawnPlatform(line, sector, 0, status->type);

   CS_SetPlatformStatus(platform, status);
   printf(
      "CL_SpawnPlatformFromStatus: Spawned platform at %u: %u.\n",
      cl_current_world_index,
      platform->net_id
   );
   CS_RegisterPlatformNetID(platform);
}

void CL_SpawnGenPlatformFromStatus(line_t *line, sector_t *sector,
                                   platform_status_t *status)
{
   plat_t *platform = P_SpawnGenPlatform(line, sector);

   CS_SetPlatformStatus(platform, status);
   printf(
      "CL_SpawnGenPlatformFromStatus: Spawned platform at %u: %u.\n",
      cl_current_world_index,
      platform->net_id
   );
   CS_RegisterPlatformNetID(platform);
}


void CL_HandleMapSpecialSpawnedMessage(nm_specialspawned_t *message)
{
   line_t *line = &lines[message->line_number];
   sector_t *sector = &sectors[message->sector_number];
   void *status = (void *)((char *)message) + sizeof(nm_specialspawned_t);
   void *data = NULL;

   switch(message->special_type)
   {
   case ms_ceiling:
      CL_SpawnCeilingFromStatus(line, sector, (ceiling_status_t *)status);
      break;
   case ms_ceiling_param:
      data = (void *)((char *)status) + sizeof(ceiling_status_t);
      CL_SpawnParamCeiling(
         line, sector, (ceiling_status_t *)status, (cs_ceilingdata_t *)data
      );
      break;
   case ms_door_tagged:
   case ms_door_manual:
   case ms_door_closein30:
   case ms_door_raisein300:
      CL_SpawnDoorFromStatus(
         line, sector, (door_status_t *)status, message->special_type
      );
      break;
   case ms_door_param:
      data = (void *)((char *)status) + sizeof(door_status_t);
      CL_SpawnParamDoor(
         line, sector, (door_status_t *)status, (cs_doordata_t *)data
      );
      break;
   case ms_floor:
   case ms_stairs:
   case ms_donut:
   case ms_donut_hole:
      CL_SpawnFloorFromStatus(
         line, sector, (floor_status_t *)status, message->special_type
      );
      break;
   case ms_floor_param:
      data = (void *)((char *)status) + sizeof(floor_status_t);
      CL_SpawnParamFloor(
         line, sector, (floor_status_t *)status, (cs_floordata_t *)data
      );
      break;
   case ms_elevator:
      CL_SpawnElevatorFromStatus(line, sector, (elevator_status_t *)status);
      break;
   case ms_pillar_build:
   case ms_pillar_open:
      CL_SpawnPillarFromStatus(
         line, sector, (pillar_status_t *)status, message->special_type
      );
      break;
   case ms_floorwaggle:
      CL_SpawnFloorWaggleFromStatus(
         line, sector, (floorwaggle_status_t *)status
      );
      break;
   case ms_platform:
      CL_SpawnPlatformFromStatus(line, sector, (platform_status_t *)status);
      break;
   case ms_platform_gen:
      CL_SpawnGenPlatformFromStatus(line, sector, (platform_status_t *)status);
      break;
   default:
      doom_printf(
         "CL_ProcessMapSpecialSpawns: unknown special type %d, ignoring.\n",
         message->special_type
      );
      break;
   }
}

void CL_HandleMapSpecialRemovedMessage(nm_specialremoved_t *message)
{
   ceiling_t *ceiling;
   vldoor_t *door;
   floormove_t *floor;
   elevator_t *elevator;
   pillar_t *pillar;
   floorwaggle_t *floorwaggle;
   plat_t *platform;

   switch(message->special_type)
   {
   case ms_ceiling:
   case ms_ceiling_param:
      ceiling = CS_GetCeilingFromNetID(message->net_id);
      if(ceiling == NULL)
      {
         doom_printf(
            "CL_HandleMapSpecialRemoval: No ceiling for Net ID %d.\n",
            message->net_id
         );
      }
      else
      {
         P_RemoveActiveCeiling(ceiling);
      }
      break;
   case ms_door_tagged:
   case ms_door_manual:
   case ms_door_closein30:
   case ms_door_raisein300:
   case ms_door_param:
      door = CS_GetDoorFromNetID(message->net_id);
      if(door == NULL)
      {
         doom_printf(
            "CL_HandleMapSpecialRemoval: No door for Net ID %d.\n",
            message->net_id
         );
      }
      else
      {
         P_RemoveDoor(door);
      }
      break;
   case ms_floor:
   case ms_stairs:
   case ms_donut:
   case ms_donut_hole:
   case ms_floor_param:
      floor = CS_GetFloorFromNetID(message->net_id);
      if(floor == NULL)
      {
         doom_printf(
            "CL_HandleMapSpecialRemoval: No floor for Net ID %d.\n",
            message->net_id
         );
      }
      else
      {
         P_RemoveFloor(floor);
      }
      break;
   case ms_elevator:
      elevator = CS_GetElevatorFromNetID(message->net_id);
      if(elevator == NULL)
      {
         doom_printf(
            "CL_HandleMapSpecialRemoval: No elevator for Net ID %d.\n",
            message->net_id
         );
      }
      else
      {
         P_RemoveElevator(elevator);
      }
      break;
   case ms_pillar_build:
   case ms_pillar_open:
      pillar = CS_GetPillarFromNetID(message->net_id);
      if(pillar == NULL)
      {
         doom_printf(
            "CL_HandleMapSpecialRemoval: No pillar for Net ID %d.\n",
            message->net_id
         );
      }
      else
      {
         P_RemovePillar(pillar);
      }
      break;
   case ms_floorwaggle:
      floorwaggle = CS_GetFloorWaggleFromNetID(message->net_id);
      if(floorwaggle == NULL)
      {
         doom_printf(
            "CL_HandleMapSpecialRemoval: No floorwaggle for Net ID %d.\n",
            message->net_id
         );
      }
      else
      {
         P_RemoveFloorWaggle(floorwaggle);
      }
      break;
   case ms_platform:
   case ms_platform_gen:
      platform = CS_GetPlatformFromNetID(message->net_id);
      if(platform == NULL)
      {
         doom_printf(
            "CL_HandleMapSpecialRemoval: No platform for Net ID %d.\n",
            message->net_id
         );
      }
      else
      {
         P_RemoveActivePlat(platform);
      }
      break;
   default:
      doom_printf(
         "CL_HandleMapSpecialRemoval: unknown special type %d, ignoring.\n",
         message->special_type
      );
      break;
   }
}

void CL_ClearSectorPositions(void)
{
   sector_position_buffer_node_t *node = NULL;
   unsigned int index = cl_current_world_index - 1;

   // [CG] Same as removing buffered map special removals; we go back one from
   //      cl_current_world_index to avoid messing up clientside prediction.

   FOR_SECTOR_POSITION_NODES_AT(index)
   {
      E_HashRemoveObject(cl_sector_position_hash, node);
      free(node);
      node = NULL;
   }
}

void CL_ClearMapSpecialStatuses(void)
{
   unsigned int index = cl_current_world_index - 1;
   ms_status_buffer_node_t *node = NULL;

   // [CG] Same as removing buffered map special removals; we go back one from
   //      cl_current_world_index to avoid messing up clientside prediction.

   FOR_SPECIAL_STATUS_NODES_AT(index)
   {
      E_HashRemoveObject(cl_ms_status_hash, node);
      free(node->status);
      free(node);
      node = NULL;
   }
}

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

void CL_LoadCurrentSectorState(void)
{
   CL_LoadSectorState(cl_current_world_index);
}

void CL_LoadCurrentSectorPositions(void)
{
   CL_LoadSectorPositions(cl_current_world_index);
}

// [CG] For debugging.
void CL_PrintSpecialStatuses(void)
{
#if _SECTOR_PRED_DEBUG
   door_netid_t *door_netid = NULL;
   platform_netid_t *platform_netid = NULL;

   while((door_netid = CS_IterateDoors(door_netid)) != NULL)
   {
      P_PrintDoor(door_netid->door);
   }
   while((platform_netid = CS_IteratePlatforms(platform_netid)) != NULL)
   {
      P_PrintPlatform(platform_netid->platform);
   }
   printf("===\n");
#endif
}

