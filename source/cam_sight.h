// Emacs style mode select   -*- C++ -*- vi:ts=3:sw=3:set et:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2012 James Haley
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Line of sight checking for cameras
//
//-----------------------------------------------------------------------------

#ifndef CAM_SIGHT_H__
#define CAM_SIGHT_H__

#include "m_fixed.h"

bool CAM_CheckSight(fixed_t cx, fixed_t cy, fixed_t cz, fixed_t cheight,
                    fixed_t tx, fixed_t ty, fixed_t tz, fixed_t theight);

#endif

// EOF

