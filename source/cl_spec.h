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

#ifndef __CL_SPEC_H__
#define __CL_SPEC_H__

#include "p_spec.h"

#include "cs_main.h"
#include "cs_spec.h"

extern bool cl_predicting_sectors;
extern bool cl_setting_sector_positions;

void CL_LoadSectorState(unsigned int index);
void CL_LoadSectorPositions(unsigned int index);
void CL_LoadCurrentSectorPositions(void);
void CL_LoadCurrentSectorState(void);

void CL_SpawnParamCeiling(line_t *line, sector_t *sector,
                          CeilingThinker::status_t *status,
                          cs_ceilingdata_t *data);
void CL_SpawnParamDoor(line_t *line, sector_t *sector,
                       VerticalDoorThinker::status_t *status,
                       cs_doordata_t *data);
void CL_SpawnParamFloor(line_t *line, sector_t *sector,
                        FloorMoveThinker::status_t *status,
                        cs_floordata_t *data);
void CL_SpawnCeilingFromStatus(line_t *line, sector_t *sector,
                               CeilingThinker::status_t *status);
void CL_SpawnDoorFromStatus(line_t *line, sector_t *sector,
                            VerticalDoorThinker::status_t *status,
                            map_special_e type);
void CL_SpawnFloorFromStatus(line_t *line, sector_t *sector,
                             FloorMoveThinker::status_t *status,
                             map_special_e type);
void CL_SpawnElevatorFromStatus(line_t *line, sector_t *sector,
                                ElevatorThinker::status_t *status);
void CL_SpawnPillarFromStatus(line_t *line, sector_t *sector,
                              PillarThinker::status_t *status,
                              map_special_e type);
void CL_SpawnFloorWaggleFromStatus(line_t *line, sector_t *sector,
                                   FloorWaggleThinker::status_t *status);
void CL_SpawnPlatformFromStatus(line_t *line, sector_t *sector,
                                PlatThinker::status_t *status);
void CL_SpawnGenPlatformFromStatus(line_t *line, sector_t *sector,
                                   PlatThinker::status_t *status);

void CL_SpawnParamCeilingFromBlob(line_t *line, sector_t *sector, void *blob);
void CL_SpawnParamDoorFromBlob(line_t *line, sector_t *sector, void *blob);
void CL_SpawnParamFloorFromBlob(line_t *line, sector_t *sector, void *blob);

void CL_SpawnCeilingFromStatusBlob(line_t *line, sector_t *sector, void *blob);
void CL_SpawnDoorFromStatusBlob(line_t *line, sector_t *sector, void *blob,
                                map_special_e type);
void CL_SpawnFloorFromStatusBlob(line_t *line, sector_t *sector, void *blob,
                                 map_special_e type);
void CL_SpawnElevatorFromStatusBlob(line_t *line, sector_t *sector, void *blob);
void CL_SpawnPillarFromStatusBlob(line_t *line, sector_t *sector, void *blob,
                                    map_special_e type);
void CL_SpawnFloorWaggleFromStatusBlob(line_t *line, sector_t *sector,
                                       void *blob);
void CL_SpawnPlatformFromStatusBlob(line_t *line, sector_t *sector,
                                    void *blob);
void CL_SpawnGenPlatformFromStatusBlob(line_t *line, sector_t *sector,
                                       void *blob);

void CL_ApplyCeilingStatusFromBlob(void *blob);
void CL_ApplyDoorStatusFromBlob(void *blob);
void CL_ApplyFloorStatusFromBlob(void *blob);
void CL_ApplyElevatorStatusFromBlob(void *blob);
void CL_ApplyPillarStatusFromBlob(void *blob);
void CL_ApplyFloorWaggleStatusFromBlob(void *blob);
void CL_ApplyPlatformStatusFromBlob(void *blob);

void CL_ClearMapSpecialStatuses(void);
void CL_PrintSpecialStatuses(void);

#endif

