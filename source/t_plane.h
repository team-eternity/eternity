// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 James Haley
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
result_e T_MoveFloorUp    (sector_t *sector, fixed_t speed, fixed_t dest, int crush);
result_e T_MoveCeilingDown(sector_t *sector, fixed_t speed, fixed_t dest, int crush);
result_e T_MoveCeilingUp  (sector_t *sector, fixed_t speed, fixed_t dest, int crush);

result_e T_MoveFloorInDirection(sector_t *sector, fixed_t speed, fixed_t dest, 
                                int crush, int direction);

result_e T_MoveCeilingInDirection(sector_t *sector, fixed_t speed, fixed_t dest, 
                                  int crush, int direction);

#endif

// EOF

