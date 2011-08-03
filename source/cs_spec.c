// Emacs style mode select -*- C -*- vi:sw=3 ts=3:
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

#include "r_defs.h"
#include "r_state.h"

#include "cs_spec.h"
#include "cs_main.h"
#include "cs_netid.h"
#include "cl_main.h"

sector_position_t **cs_sector_positions = NULL;

static int old_numsectors = 0;

void CS_SpecInit(void)
{
   unsigned int i;

   // [CG] (Re)allocate and zero the sector position buffer.
   if(cs_sector_positions != NULL)
   {
      for(i = 0; i < old_numsectors; i++)
      {
         free(cs_sector_positions[i]);
      }
      cs_sector_positions = realloc(
         cs_sector_positions, numsectors * sizeof(sector_position_t *)
      );
      memset(
         cs_sector_positions, 0, numsectors * sizeof(sector_position_t *)
      );
   }
   else
   {
      cs_sector_positions = calloc(numsectors, sizeof(sector_position_t *));
   }

   // [CG] Serverside, we must record the initial positions of all sectors
   //      so that later we can tell if they've moved.  Clientside we must
   //      record the initial positions of all sectors so later we can tell
   //      if we've made a prediction error.
   for(i = 0; i < numsectors; i++)
   {
      cs_sector_positions[i] = calloc(
         MAX_POSITIONS, sizeof(sector_position_t)
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

boolean CS_SectorPositionsEqual(sector_position_t *position_one,
                                sector_position_t *position_two)
{
   if(position_one->ceiling_height == position_two->ceiling_height &&
      position_one->floor_height   == position_two->floor_height)
   {
      return true;
   }
   return false;
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

boolean CS_SectorPositionIs(sector_t *sector, sector_position_t *position)
{
   if(sector->ceilingheight == position->ceiling_height &&
      sector->floorheight   == position->floor_height)
   {
      return true;
   }
   return false;
}

void CS_SaveCeilingStatus(ceiling_status_t *status, ceiling_t *ceiling)
{
   status->net_id = ceiling->net_id;
   status->type = ceiling->type;
   status->bottom_height = ceiling->bottomheight;
   status->top_height = ceiling->topheight;
   status->speed = ceiling->speed;
   status->old_speed = ceiling->oldspeed;
   status->crush = ceiling->crush;
   status->texture = ceiling->texture;
   status->direction = ceiling->direction;
   status->tag = ceiling->tag;
   status->old_direction = ceiling->olddirection;
}

void CS_SaveDoorStatus(door_status_t *status, vldoor_t *door)
{
   status->net_id = door->net_id;
   status->type = door->type;
   status->top_height = door->topheight;
   status->speed = door->speed;
   status->direction = door->direction;
   status->top_wait = door->topwait;
   status->top_countdown = door->topcountdown;
}

void CS_SaveFloorStatus(floor_status_t *status, floormove_t *floor)
{
   status->net_id = floor->net_id;
   status->type = floor->type;
   status->crush = floor->crush;
   status->direction = floor->direction;
   status->new_special = floor->special.newspecial;
   status->flags = floor->special.flags;
   status->damage = floor->special.damage;
   status->damage_mask = floor->special.damagemask;
   status->damage_mod = floor->special.damagemod;
   status->damage_flags = floor->special.damageflags;
   status->texture = floor->texture;
   status->floor_dest_height = floor->floordestheight;
   status->speed = floor->speed;
   status->reset_time = floor->resetTime;
   status->reset_height = floor->resetHeight;
   status->step_raise_time = floor->stepRaiseTime;
   status->delay_time = floor->delayTime;
   status->delay_timer = floor->delayTimer;
}

void CS_SaveElevatorStatus(elevator_status_t *status, elevator_t *elevator)
{
   status->net_id = elevator->net_id;
   status->type = elevator->type;
   status->direction = elevator->direction;
   status->floor_dest_height = elevator->floordestheight;
   status->ceiling_dest_height = elevator->ceilingdestheight;
   status->speed = elevator->speed;
}

void CS_SavePillarStatus(pillar_status_t *status, pillar_t *pillar)
{
   status->net_id = pillar->net_id;
   status->ceiling_speed = pillar->ceilingSpeed;
   status->floor_speed = pillar->floorSpeed;
   status->floor_dest = pillar->floordest;
   status->ceiling_dest = pillar->ceilingdest;
   status->direction = pillar->direction;
   status->crush = pillar->crush;
}

void CS_SaveFloorWaggleStatus(floorwaggle_status_t *status,
                              floorwaggle_t *floorwaggle)
{
   status->net_id = floorwaggle->net_id;
   status->original_height = floorwaggle->originalHeight;
   status->accumulator = floorwaggle->accumulator;
   status->acc_delta = floorwaggle->accDelta;
   status->target_scale = floorwaggle->targetScale;
   status->scale = floorwaggle->scale;
   status->scale_delta = floorwaggle->scaleDelta;
   status->ticker = floorwaggle->ticker;
   status->state = floorwaggle->state;
}

void CS_SavePlatformStatus(platform_status_t *status, plat_t *platform)
{
   status->net_id = platform->net_id;
   status->type = platform->type;
   status->speed = platform->speed;
   status->low = platform->low;
   status->high = platform->high;
   status->wait = platform->wait;
   status->count = platform->count;
   status->status = platform->status;
   status->old_status = platform->oldstatus;
   status->crush = platform->crush;
   status->tag = platform->tag;
}

void CS_SetCeilingStatus(ceiling_t *ceiling, ceiling_status_t *status)
{
   ceiling->net_id = status->net_id;
   ceiling->type = status->type;
   ceiling->bottomheight = status->bottom_height;
   ceiling->topheight = status->top_height;
   ceiling->speed = status->speed;
   ceiling->oldspeed = status->old_speed;
   ceiling->crush = status->crush;
   ceiling->texture = status->texture;
   ceiling->direction = status->direction;
   ceiling->tag = status->tag;
   ceiling->olddirection = status->old_direction;
}

void CS_SetDoorStatus(vldoor_t *door, door_status_t *status)
{
   door->net_id = status->net_id;
   door->type = status->type;
   door->topheight = status->top_height;
   door->speed = status->speed;
   door->direction = status->direction;
   door->topwait = status->top_wait;
   door->topcountdown = status->top_countdown;
}

void CS_SetFloorStatus(floormove_t *floor, floor_status_t *status)
{
   floor->net_id = status->net_id;
   floor->type = status->type;
   floor->crush = status->crush;
   floor->direction = status->direction;
   floor->special.newspecial = status->new_special;
   floor->special.flags = status->flags;
   floor->special.damage = status->damage;
   floor->special.damagemask = status->damage_mask;
   floor->special.damagemod = status->damage_mod;
   floor->special.damageflags = status->damage_flags;
   floor->texture = status->texture;
   floor->floordestheight = status->floor_dest_height;
   floor->speed = status->speed;
   floor->resetTime = status->reset_time;
   floor->resetHeight = status->reset_height;
   floor->stepRaiseTime = status->step_raise_time;
   floor->delayTime = status->delay_time;
   floor->delayTimer = status->delay_timer;
}

void CS_SetElevatorStatus(elevator_t *elevator, elevator_status_t *status)
{
   elevator->net_id = status->net_id;
   elevator->direction = status->direction;
   elevator->floordestheight = status->floor_dest_height;
   elevator->ceilingdestheight = status->ceiling_dest_height;
   elevator->speed = status->speed;
}

void CS_SetPillarStatus(pillar_t *pillar, pillar_status_t *status)
{
   pillar->net_id = status->net_id;
   pillar->ceilingSpeed = status->ceiling_speed;
   pillar->floorSpeed = status->floor_speed;
   pillar->floordest = status->floor_dest;
   pillar->ceilingdest = status->ceiling_dest;
   pillar->direction = status->direction;
   pillar->crush = status->crush;
}

void CS_SetFloorWaggleStatus(floorwaggle_t *floorwaggle,
                             floorwaggle_status_t *status)
{
   floorwaggle->net_id = status->net_id;
   floorwaggle->originalHeight = status->original_height;
   floorwaggle->accumulator = status->accumulator;
   floorwaggle->accDelta = status->acc_delta;
   floorwaggle->targetScale = status->target_scale;
   floorwaggle->scale = status->scale;
   floorwaggle->scaleDelta = status->scale_delta;
   floorwaggle->ticker = status->ticker;
   floorwaggle->state = status->state;
}

void CS_SetPlatformStatus(plat_t *platform, platform_status_t *status)
{
   platform->net_id = status->net_id;
   platform->type = status->type;
   platform->speed = status->speed;
   platform->low = status->low;
   platform->high = status->high;
   platform->wait = status->wait;
   platform->count = status->count;
   platform->status = status->status;
   platform->oldstatus = status->old_status;
   platform->crush = status->crush;
   platform->tag = status->tag;
}

void CS_ApplyCeilingStatus(ceiling_status_t *status)
{
   ceiling_t *ceiling = CS_GetCeilingFromNetID(status->net_id);

   if(!ceiling)
   {
      I_Error("No ceiling for Net ID %u.\n", status->net_id);
   }
   CS_SetCeilingStatus(ceiling, status);
}

void CS_ApplyDoorStatus(door_status_t *status)
{
   vldoor_t *door = CS_GetDoorFromNetID(status->net_id);

   if(!door)
   {
      I_Error("No door for Net ID %u.\n", status->net_id);
   }
   CS_SetDoorStatus(door, status);
}

void CS_ApplyFloorStatus(floor_status_t *status)
{
   floormove_t *floor = CS_GetFloorFromNetID(status->net_id);

   if(!floor)
   {
      I_Error("No floor for Net ID %u.\n", status->net_id);
   }
   CS_SetFloorStatus(floor, status);
}

void CS_ApplyElevatorStatus(elevator_status_t *status)
{
   elevator_t *elevator = CS_GetElevatorFromNetID(status->net_id);

   if(!elevator)
   {
      I_Error("No elevator for Net ID %u.\n", status->net_id);
   }
   CS_SetElevatorStatus(elevator, status);
}

void CS_ApplyPillarStatus(pillar_status_t *status)
{
   pillar_t *pillar = CS_GetPillarFromNetID(status->net_id);

   if(!pillar)
   {
      I_Error("No pillar for Net ID %u.\n", status->net_id);
   }
   CS_SetPillarStatus(pillar, status);
}

void CS_ApplyFloorWaggleStatus(floorwaggle_status_t *status)
{
   floorwaggle_t *floorwaggle = CS_GetFloorWaggleFromNetID(status->net_id);

   if(!floorwaggle)
   {
      I_Error("No floorwaggle for Net ID %u.\n", status->net_id);
   }
   CS_SetFloorWaggleStatus(floorwaggle, status);
}

void CS_ApplyPlatformStatus(platform_status_t *status)
{
   plat_t *platform = CS_GetPlatformFromNetID(status->net_id);

   if(!platform)
   {
      I_Error("No platform for Net ID %u.\n", status->net_id);
   }
   CS_SetPlatformStatus(platform, status);
}

void CS_SpawnCeilingFromStatus(line_t *line, sector_t *sector,
                               ceiling_status_t *status)
{
   ceiling_t *ceiling = P_SpawnCeiling(line, sector, status->type);

   CS_SetCeilingStatus(ceiling, status);
   printf("CS_SpawnCeilingFromStatus: Registering ceiling Net ID.\n");
   CS_RegisterCeilingNetID(ceiling);
}

void CS_SpawnDoorFromStatus(line_t *line, sector_t *sector,
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

void CS_SpawnFloorFromStatus(line_t *line, sector_t *sector,
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

void CS_SpawnElevatorFromStatus(line_t *line, sector_t *sector,
                                elevator_status_t *status)
{
   elevator_t *elevator = P_SpawnElevator(line, sector, status->type);

   CS_SetElevatorStatus(elevator, status);
   CS_RegisterElevatorNetID(elevator);
}

void CS_SpawnPillarFromStatus(line_t *line, sector_t *sector,
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

void CS_SpawnFloorWaggleFromStatus(line_t *line, sector_t *sector,
                                   floorwaggle_status_t *status)
{
   floorwaggle_t *floorwaggle = P_SpawnFloorWaggle(line, sector, 0, 0, 0, 0);

   CS_SetFloorWaggleStatus(floorwaggle, status);
   CS_RegisterFloorWaggleNetID(floorwaggle);
}

void CS_SpawnPlatformFromStatus(line_t *line, sector_t *sector,
                                platform_status_t *status)
{
   plat_t *platform = P_SpawnPlatform(line, sector, 0, status->type);

   CS_SetPlatformStatus(platform, status);
   printf(
      "CS_SpawnPlatformFromStatus: Spawned platform at %u: %u.\n",
      cl_current_world_index,
      platform->net_id
   );
   CS_RegisterPlatformNetID(platform);
}

