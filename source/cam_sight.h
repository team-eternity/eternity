// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
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
#include "tables.h"

struct camera_t;
class  Mobj;

struct camsightparams_t
{
   fixed_t cx;       // camera (or "looker") coordinates
   fixed_t cy;
   fixed_t cz;
   fixed_t tx;       // target coordinates
   fixed_t ty;
   fixed_t tz;
   fixed_t cheight;  // top height of camera above cz
   fixed_t theight;  // top height of target above cz
   int     cgroupid; // camera portal groupid
   int     tgroupid; // target portal groupid

   const camsightparams_t *prev; // previous invocation

   void setCamera(const camera_t &camera, fixed_t height);
   void setLookerMobj(const Mobj *mo);
   void setTargetMobj(const Mobj *mo);
};

bool CAM_CheckSight(const camsightparams_t &params);

fixed_t CAM_AimLineAttack(const Mobj *t1, angle_t angle, fixed_t distance, 
                          uint32_t mask, Mobj **outTarget);
// ioanch 20160101: bullet attack
void CAM_LineAttack(Mobj *source, angle_t angle, fixed_t distance, 
                    fixed_t slope, int damage);

#endif

// EOF

