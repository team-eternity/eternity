//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//  General plane mover routines
//
//-----------------------------------------------------------------------------

#ifndef T_PLANE_H__
#define T_PLANE_H__

// Required for fixed_t:
#include "m_fixed.h"

struct sector_t;

// crush check returns
typedef enum
{
   ok,
   crushed,
   pastdest
} result_e;

result_e T_MoveFloorDown  (sector_t *sector, fixed_t speed, fixed_t dest, int crush);
result_e T_MoveFloorUp    (sector_t *sector, fixed_t speed, fixed_t dest, int crush,
                           bool emulateStairCrush);
result_e T_MoveCeilingDown(sector_t *sector, fixed_t speed, fixed_t dest,
                           int crush, bool crushrest = false);
result_e T_MoveCeilingUp  (sector_t *sector, fixed_t speed, fixed_t dest, int crush);

result_e T_MoveFloorInDirection(sector_t *sector, fixed_t speed, fixed_t dest, 
                                int crush, int direction, bool emulateStairCrush);

result_e T_MoveCeilingInDirection(sector_t *sector, fixed_t speed, fixed_t dest, 
                                  int crush, int direction);

#endif

// EOF

