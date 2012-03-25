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

#ifndef CL_SPEC_H__
#define CL_SPEC_H__

#include "cs_spec.h"

extern bool cl_predicting_sectors;
extern bool cl_setting_sector_positions;

void CL_LoadSectorPositions(unsigned int index);
void CL_LoadCurrentSectorPositions(void);

void CL_SpawnPlatform(sector_t *sec, cs_platform_data_t *dt);
void CL_SpawnVerticalDoor(line_t *ln, sector_t *sec, cs_door_data_t *dt);
void CL_SpawnCeiling(sector_t *sec, cs_ceiling_data_t *dt);
void CL_SpawnFloor(sector_t *sec, cs_floor_data_t *dt);
void CL_SpawnElevator(sector_t *sec, cs_elevator_data_t *dt);
void CL_SpawnPillar(sector_t *sec, cs_pillar_data_t *dt);
void CL_SpawnFloorWaggle(sector_t *sec, cs_floorwaggle_data_t *dt);

void CL_UpdatePlatform(cs_platform_data_t *dt);
void CL_UpdateVerticalDoor(cs_door_data_t *dt);
void CL_UpdateCeiling(cs_ceiling_data_t *dt);
void CL_UpdateFloor(cs_floor_data_t *dt);
void CL_UpdateElevator(cs_elevator_data_t *dt);
void CL_UpdatePillar(cs_pillar_data_t *dt);
void CL_UpdateFloorWaggle(cs_floorwaggle_data_t *dt);

#endif

