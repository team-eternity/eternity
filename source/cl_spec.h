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

void CL_InitSectorPositions();
void CL_CarrySectorPositions(uint32_t old_index);
void CL_SaveSectorPosition(uint32_t index, uint32_t sector_number,
                           sector_position_t *new_position);
void CL_LoadLatestSectorPositions();
void CL_LoadSectorPositionsAt(uint32_t index);
void CL_StoreLineActivation(Mobj *actor, line_t *line, int side,
                            activation_type_e type);
void CL_HandleLineActivation(uint32_t line_number, uint32_t command_index);
void CL_ActivateAllSectorMovementThinkers();
void CL_SavePredictedSectorPositions(uint32_t index);

void CL_SpawnPlatform(nm_sectorthinkerspawned_t *message);
void CL_SpawnVerticalDoor(nm_sectorthinkerspawned_t *message);
void CL_SpawnCeiling(nm_sectorthinkerspawned_t *message);
void CL_SpawnFloor(nm_sectorthinkerspawned_t *message);
void CL_SpawnElevator(nm_sectorthinkerspawned_t *message);
void CL_SpawnPillar(nm_sectorthinkerspawned_t *message);
void CL_SpawnFloorWaggle(nm_sectorthinkerspawned_t *message);

void CL_UpdatePlatform(uint32_t index, cs_sector_thinker_data_t *data);
void CL_UpdateVerticalDoor(uint32_t index, cs_sector_thinker_data_t *data);
void CL_UpdateCeiling(uint32_t index, cs_sector_thinker_data_t *data);
void CL_UpdateFloor(uint32_t index, cs_sector_thinker_data_t *data);
void CL_UpdateElevator(uint32_t index, cs_sector_thinker_data_t *data);
void CL_UpdatePillar(uint32_t index, cs_sector_thinker_data_t *data);
void CL_UpdateFloorWaggle(uint32_t index, cs_sector_thinker_data_t *data);

#endif

