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
} map_special_e;

// [CG] Everything below is either sent over the network or saved in demos, and
//      therefore must be packed and use exact-width integer types.

#pragma pack(push, 1)

typedef struct sector_position_s
{
   uint32_t world_index;
   fixed_t ceiling_height;
   fixed_t floor_height;
} sector_position_t;

// [CG] 09/22/11: Status for sending over the network.
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

// [CG] 09/22/11: Status for sending over the network.
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

// [CG] 09/22/11: Status for sending over the network.
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

#pragma pack(pop)

extern sector_position_t **cs_sector_positions;

void CS_InitSectorPositions(void);
void CS_SetSectorPosition(sector_t *sector, sector_position_t *position);
bool CS_SectorPositionsEqual(sector_position_t *position_one,
                             sector_position_t *position_two);
void CS_SaveSectorPosition(sector_position_t *position, sector_t *sector);
void CS_CopySectorPosition(sector_position_t *position_one,
                           sector_position_t *position_two);
void CS_PrintSectorPosition(size_t sector_number, sector_position_t *position);
bool CS_SectorPositionIs(sector_t *sector, sector_position_t *position);
void CS_InitSectorPositions(void);
void CS_LoadSectorPositions(unsigned int index);
void CS_LoadCurrentSectorPositions(void);
void CS_SaveCeilingData(cs_ceilingdata_t *cscd, ceilingdata_t *cd);
void CS_SaveDoorData(cs_doordata_t *csdd, doordata_t *dd);
void CS_SaveFloorData(cs_floordata_t *csfd, floordata_t *fd);

#endif

