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

   for(i = 0; i < (unsigned int)numsectors; i++)
   {
      position = &cs_sector_positions[i][index % MAX_POSITIONS];

      if(position->world_index != index)
         continue;

      CS_SetSectorPosition(&sectors[i], position);
   }

   cl_setting_sector_positions = false;
}

void CL_LoadLatestSectorPositions(void)
{
   CL_LoadSectorPositions(cl_latest_world_index);
}

void CL_SpawnPlatform(sector_t *sec, cs_platform_data_t *dt)
{
   PlatThinker *platform = new PlatThinker;

   platform->sector = sec;
   platform->netUpdate(dt);

   P_AddActivePlat(platform);
}

void CL_SpawnVerticalDoor(line_t *ln, sector_t *sec, cs_door_data_t *dt)
{
   VerticalDoorThinker *door = new VerticalDoorThinker;

   door->sector = sec;
   door->netUpdate(dt);

   NetSectorThinkers.add(door);
}

void CL_SpawnCeiling(sector_t *sec, cs_ceiling_data_t *dt)
{
   CeilingThinker *ceiling = new CeilingThinker;

   ceiling->sector = sec;
   ceiling->netUpdate(dt);

   P_AddActiveCeiling(ceiling);
}

void CL_SpawnFloor(sector_t *sec, cs_floor_data_t *dt)
{
   FloorMoveThinker *floor = new FloorMoveThinker;

   floor->sector = sec;
   floor->netUpdate(dt);

   NetSectorThinkers.add(floor);
}

void CL_SpawnElevator(sector_t *sec, cs_elevator_data_t *dt)
{
   ElevatorThinker *elevator = new ElevatorThinker;

   elevator->sector = sec;
   elevator->netUpdate(dt);

   NetSectorThinkers.add(elevator);
}

void CL_SpawnPillar(sector_t *sec, cs_pillar_data_t *dt)
{
   PillarThinker *pillar = new PillarThinker;

   pillar->sector = sec;
   pillar->netUpdate(dt);

   NetSectorThinkers.add(pillar);
}

void CL_SpawnFloorWaggle(sector_t *sec, cs_floorwaggle_data_t *dt)
{
   FloorWaggleThinker *floor_waggle = new FloorWaggleThinker;

   floor_waggle->sector = sec;
   floor_waggle->netUpdate(dt);

   NetSectorThinkers.add(floor_waggle);
}

void CL_UpdatePlatform(cs_platform_data_t *dt)
{
   PlatThinker *platform = NULL;
   SectorThinker *thinker = NULL;
   
   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such platform %u.", dt->net_id);
   else if(!(platform = dynamic_cast<PlatThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a platform.", dt->net_id);
   else
      platform->netUpdate(dt);
}

void CL_UpdateVerticalDoor(cs_door_data_t *dt)
{
   VerticalDoorThinker *door = NULL;
   SectorThinker *thinker = NULL;
   
   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such door %u.", dt->net_id);
   else if(!(door = dynamic_cast<VerticalDoorThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a door.", dt->net_id);
   else
      door->netUpdate(dt);
}

void CL_UpdateCeiling(cs_ceiling_data_t *dt)
{
   CeilingThinker *ceiling = NULL;
   SectorThinker *thinker = NULL;
   
   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such ceiling %u.", dt->net_id);
   else if(!(ceiling = dynamic_cast<CeilingThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a ceiling.", dt->net_id);
   else
      ceiling->netUpdate(dt);
}

void CL_UpdateFloor(cs_floor_data_t *dt)
{
   FloorMoveThinker *floor = NULL;
   SectorThinker *thinker = NULL;
   
   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such floor %u.", dt->net_id);
   else if(!(floor = dynamic_cast<FloorMoveThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a floor.", dt->net_id);
   else
      floor->netUpdate(dt);
}

void CL_UpdateElevator(cs_elevator_data_t *dt)
{
   ElevatorThinker *elevator = NULL;
   SectorThinker *thinker = NULL;
   
   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such elevator %u.", dt->net_id);
   else if(!(elevator = dynamic_cast<ElevatorThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a elevator.", dt->net_id);
   else
      elevator->netUpdate(dt);
}

void CL_UpdatePillar(cs_pillar_data_t *dt)
{
   PillarThinker *pillar = NULL;
   SectorThinker *thinker = NULL;
   
   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such pillar %u.", dt->net_id);
   else if(!(pillar = dynamic_cast<PillarThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a pillar.", dt->net_id);
   else
      pillar->netUpdate(dt);
}

void CL_UpdateFloorWaggle(cs_floorwaggle_data_t *dt)
{
   FloorWaggleThinker *floorwaggle = NULL;
   SectorThinker *thinker = NULL;
   
   if(!(thinker = NetSectorThinkers.lookup(dt->net_id)))
      doom_printf("No such floorwaggle %u.", dt->net_id);
   else if(!(floorwaggle = dynamic_cast<FloorWaggleThinker *>(thinker)))
      doom_printf("Sector thinker %u is not a floor waggle.", dt->net_id);
   else
      floorwaggle->netUpdate(dt);
}

