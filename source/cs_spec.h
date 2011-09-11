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

#ifndef __CS_SPEC_H__
#define __CS_SPEC_H__

#include "m_fixed.h"
#include "p_spec.h"

typedef enum
{
   ms_ceiling,
   ms_ceiling_param,
   ms_door_tagged,
   ms_door_manual,
   ms_door_closein30,
   ms_door_raisein300,
   ms_door_param,
   ms_floor,
   ms_floor_param,
   ms_stairs,
   ms_donut,
   ms_donut_hole,
   ms_elevator,
   ms_pillar_build,
   ms_pillar_open,
   ms_floorwaggle,
   ms_platform,
   ms_platform_gen,
   // [CG] These are parameterized/general specials that I've yet to add.
   ms_paramstairs,
   ms_gencrusher,
} map_special_t;

// [CG] Everything below is either sent over the network or saved in demos, and
//      therefore must be packed and use exact-width integer types.

#pragma pack(push, 1)

typedef struct sector_position_s
{
   uint32_t world_index;
   fixed_t ceiling_height;
   fixed_t floor_height;
} sector_position_t;

typedef struct ceiling_status_s
{
   uint32_t net_id;
   int32_t type;
   fixed_t bottom_height;
   fixed_t top_height;
   fixed_t speed;
   fixed_t old_speed;
   int32_t crush;
   int32_t texture;
   int8_t direction;
   int32_t tag;
   int32_t old_direction;
} ceiling_status_t;

typedef struct cs_ceilingdata_s
{
   int32_t trigger_type;
   int32_t crush;
   int32_t direction;
   int32_t speed_type;
   int32_t change_type;
   int32_t change_model;
   int32_t target_type;
   fixed_t height_value;
   fixed_t speed_value;
} cs_ceilingdata_t;

typedef struct door_status_s
{
   uint32_t net_id;
   int32_t type;
   fixed_t top_height;
   fixed_t speed;
   int32_t direction;
   int32_t top_wait;
   int32_t top_countdown;
} door_status_t;

typedef struct cs_doordata_s
{
   int32_t delay_type;
   int32_t kind;
   int32_t speed_type;
   int32_t trigger_type;
   fixed_t speed_value;
   int32_t delay_value;
   int32_t altlighttag;
   int8_t  usealtlighttag;
   int32_t topcountdown;
} cs_doordata_t;


typedef struct floor_status_s
{
   uint32_t net_id;
   int32_t type;
   int32_t crush;
   int8_t direction;
   int32_t new_special;
   uint32_t flags;
   int32_t damage;
   int32_t damage_mask;
   int32_t damage_mod;
   int32_t damage_flags;
   int16_t texture;
   fixed_t floor_dest_height;
   fixed_t speed;
   int32_t reset_time;
   fixed_t reset_height;
   int32_t step_raise_time;
   int32_t delay_time;
   int32_t delay_timer;
} floor_status_t;

typedef struct cs_floordata_s
{
   int32_t trigger_type;
   int32_t crush;
   int32_t direction;
   int32_t speed_type;
   int32_t change_type;
   int32_t change_model;
   int32_t target_type;
   fixed_t height_value;
   fixed_t speed_value;
} cs_floordata_t;

typedef struct elevator_status_s
{
   uint32_t net_id;
   int32_t type;
   int8_t direction;
   fixed_t floor_dest_height;
   fixed_t ceiling_dest_height;
   fixed_t speed;
} elevator_status_t;

typedef struct pillar_status_s
{
   uint32_t net_id;
   int32_t ceiling_speed;
   int32_t floor_speed;
   int32_t floor_dest;
   int32_t ceiling_dest;
   int8_t direction;
   int32_t crush;
} pillar_status_t;

typedef struct floorwaggle_status_s
{
   uint32_t net_id;
   fixed_t original_height;
   fixed_t accumulator;
   fixed_t acc_delta;
   fixed_t target_scale;
   fixed_t scale;
   fixed_t scale_delta;
   int32_t ticker;
   int32_t state;
} floorwaggle_status_t;

typedef struct platform_status_s
{
   uint32_t net_id;
   int32_t type;
   fixed_t speed;
   fixed_t low;
   fixed_t high;
   int32_t wait;
   int32_t count;
   int32_t status;
   int32_t old_status;
   int32_t crush;
   int32_t tag;
} platform_status_t;

#pragma pack(pop)

extern sector_position_t **cs_sector_positions;

void CS_SpecInit(void);
void CS_SetSectorPosition(sector_t *sector, sector_position_t *position);
boolean CS_SectorPositionsEqual(sector_position_t *position_one,
                                sector_position_t *position_two);
void CS_SaveSectorPosition(sector_position_t *position, sector_t *sector);
void CS_CopySectorPosition(sector_position_t *position_one,
                           sector_position_t *position_two);
void CS_PrintSectorPosition(size_t sector_number, sector_position_t *position);
boolean CS_SectorPositionIs(sector_t *sector, sector_position_t *position);
void CS_InitSectorPositions(void);
void CS_LoadSectorPositions(unsigned int index);
void CS_LoadCurrentSectorPositions(void);
void CS_SaveCeilingStatus(ceiling_status_t *status, ceiling_t *ceiling);
void CS_SaveCeilingData(cs_ceilingdata_t *cscd, ceilingdata_t *cd);
void CS_SaveDoorStatus(door_status_t *status, vldoor_t *door);
void CS_SaveDoorData(cs_doordata_t *csdd, doordata_t *dd);
void CS_SaveFloorStatus(floor_status_t *status, floormove_t *floor);
void CS_SaveFloorData(cs_floordata_t *csfd, floordata_t *fd);
void CS_SaveElevatorStatus(elevator_status_t *status, elevator_t *elevator);
void CS_SavePillarStatus(pillar_status_t *status, pillar_t *pillar);
void CS_SaveFloorWaggleStatus(floorwaggle_status_t *status,
                              floorwaggle_t *floorwaggle);
void CS_SavePlatformStatus(platform_status_t *status, plat_t *platform);
void CS_SetCeilingStatus(ceiling_t *ceiling, ceiling_status_t *status);
void CS_SetDoorStatus(vldoor_t *door, door_status_t *status);
void CS_SetFloorStatus(floormove_t *floor, floor_status_t *status);
void CS_SetElevatorStatus(elevator_t *elevator, elevator_status_t *status);
void CS_SetPillarStatus(pillar_t *pillar, pillar_status_t *status);
void CS_SetFloorWaggleStatus(floorwaggle_t *floorwaggle,
                             floorwaggle_status_t *status);
void CS_SetPlatformStatus(plat_t *platform, platform_status_t *status);

#endif

