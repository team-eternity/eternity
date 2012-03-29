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
//   C/S routines for handling sectors and map specials.
//
//----------------------------------------------------------------------------

#ifndef CS_SPEC_H__
#define CS_SPEC_H__

const uint8_t st_platform    = 1;
const uint8_t st_door        = 2;
const uint8_t st_ceiling     = 3;
const uint8_t st_floor       = 4;
const uint8_t st_elevator    = 5;
const uint8_t st_pillar      = 6;
const uint8_t st_floorwaggle = 7;

// [CG] Everything below is either sent over the network or saved in demos, and
//      therefore must be packed and use exact-width integer types.

#pragma pack(push, 1)

struct sector_position_t
{
   uint32_t world_index;
   fixed_t  ceiling_height;
   fixed_t  floor_height;
};

struct cs_platform_spawn_data_t
{
   char seqname[14];
};

struct cs_door_spawn_data_t
{
   uint8_t make_sound;
   uint8_t raise;
   uint8_t turbo;
   uint8_t bounce;
};

struct cs_ceiling_spawn_data_t
{
   int8_t noise;
};

struct cs_floor_spawn_data_t
{
   uint8_t make_sound;
};

union cs_sector_thinker_spawn_data_t
{
   cs_platform_spawn_data_t    platform_spawn_data;
   cs_door_spawn_data_t        door_spawn_data;
   cs_ceiling_spawn_data_t     ceiling_spawn_data;
   cs_floor_spawn_data_t       floor_spawn_data;
};

struct cs_platform_data_t
{
   uint32_t net_id;
   fixed_t  speed;
   fixed_t  low;
   fixed_t  high;
   int32_t  wait;
   int32_t  count;
   int32_t  status;
   int32_t  oldstatus;
   int32_t  crush;
   int32_t  tag;
   int32_t  type;
};

struct cs_door_data_t
{
   uint32_t net_id;
   int32_t  type;
   fixed_t  topheight;
   fixed_t  speed;
   int32_t  direction;
   int32_t  topwait;
   int32_t  topcountdown;
   uint32_t line_number;
   int32_t  lighttag;
   uint8_t  turbo;
};

struct cs_spectransfer_t
{
   uint32_t net_id;
   int32_t  newspecial;
   uint32_t flags;
   int32_t  damage;
   int32_t  damagemask;
   int32_t  damagemod;
   int32_t  damageflags;
};

struct cs_ceiling_data_t
{
   uint32_t          net_id;
   int32_t           type;
   fixed_t           bottomheight;
   fixed_t           topheight;
   fixed_t           speed;
   fixed_t           oldspeed;
   int32_t           crush;
   cs_spectransfer_t special;
   int32_t           texture;
   int32_t           direction;
   uint8_t           inStasis;
   int32_t           tag;
   int32_t           olddirection;
};

struct cs_floor_data_t
{
   uint32_t          net_id;
   int32_t           type;
   int32_t           crush;
   int32_t           direction;
   cs_spectransfer_t special;
   int16_t           texture;
   fixed_t           floordestheight;
   fixed_t           speed;
   int32_t           resetTime;
   fixed_t           resetHeight;
   int32_t           stepRaiseTime;
   int32_t           delayTime;
   int32_t           delayTimer;
};

struct cs_elevator_data_t
{
   uint32_t net_id;
   int32_t  type;
   int32_t  direction;
   fixed_t  floordestheight;
   fixed_t  ceilingdestheight;
   fixed_t  speed;
};

struct cs_pillar_data_t
{
   uint32_t net_id;
   int32_t  ceilingSpeed;
   int32_t  floorSpeed;
   int32_t  floordest;
   int32_t  ceilingdest;
   int32_t  direction;
   int32_t  crush;
};

struct cs_floorwaggle_data_t
{
   uint32_t net_id;
   fixed_t  originalHeight;
   fixed_t  accumulator;
   fixed_t  accDelta;
   fixed_t  targetScale;
   fixed_t  scale;
   fixed_t  scaleDelta;
   int32_t  ticker;
   int32_t  state;
};

union cs_sector_thinker_data_t
{
   cs_platform_data_t    platform_data;
   cs_door_data_t        door_data;
   cs_ceiling_data_t     ceiling_data;
   cs_floor_data_t       floor_data;
   cs_elevator_data_t    elevator_data;
   cs_pillar_data_t      pillar_data;
   cs_floorwaggle_data_t floorwaggle_data;
};

#pragma pack(pop)

struct sector_t;

void CS_LogSMT(const char *fmt, ...);
void CS_InitSectorPositions(void);
sector_position_t* CS_GetSectorPosition(uint32_t sector_number, uint32_t index);
void CS_SetSectorPosition(uint32_t sector_number, uint32_t index);
bool CS_SectorPositionsEqual(uint32_t sector_number, uint32_t index_one,
                             uint32_t index_two);
void CS_SaveSectorPosition(uint32_t index, uint32_t sector_number);
void CS_PrintPositionForSector(uint32_t sector_number);
void CS_PrintSectorPosition(size_t sector_number, sector_position_t *position);
bool CS_SectorPositionIs(sector_t *sector, sector_position_t *position);

#endif

