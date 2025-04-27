//--------------------------------------------------------------------------
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
//------------------------------------------------------------------------------
//
// Purpose: Chasecam.
// Authors: James Haley, Stephen McGranahan
//

#ifndef P_CHASE_H__
#define P_CHASE_H__

#include "m_fixed.h"
#include "r_interpolate.h"
#include "tables.h"

class Mobj;

// haleyjd 06/04/01: added heightsec field for use in R_FakeFlat
// to fix SMMU camera deep water bugs

struct camera_t
{
    fixed_t x;
    fixed_t y;
    fixed_t z;
    angle_t angle;
    fixed_t pitch;
    fixed_t prevpitch;
    int     groupid;
    bool    flying;

    prevpos_t prevpos; // previous position for interpolation

    // Save the current camera position for interpolation purposes.
    void backupPosition()
    {
        prevpos.x     = x;
        prevpos.y     = y;
        prevpos.z     = z;
        prevpos.angle = angle;
        prevpitch     = pitch;

        // TODO: misc. interpolation stuff
    }
};

extern int chasex;
extern int chasey;
extern int chasez;
extern int chaseangle;
extern int chasecam_active;

extern int chasecam_height;
extern int chasecam_dist;
extern int chasecam_speed;

extern int walkcam_active;

extern camera_t chasecam;
extern camera_t walkcamera;
extern camera_t followcam;

void P_ChaseTicker();
void P_ChaseStart();
void P_ChaseEnd();
void P_ResetChasecam();

void P_WalkTicker();
void P_WalkStart();
void P_WalkEnd();
void P_ResetWalkcam();

void P_LocateFollowCam(Mobj *target, fixed_t &destX, fixed_t &destY);
void P_SetFollowCam(fixed_t x, fixed_t y, Mobj *target);
void P_FollowCamOff();
bool P_FollowCamTicker();

#endif

// EOF
