//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
//
// Copyright (C) 2013 Simon Howard
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
//  Follows the displayplayer. It does exactly what it says on the cover!
//
// Authors: Simon Howard, James Haley, Stephen McGranahan, Ioan Chera, Max Waine
//

#include <algorithm>
#include "z_zone.h"

#include "a_small.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "cam_sight.h"
#include "d_gi.h"
#include "d_main.h"
#include "d_net.h"
#include "doomdef.h"
#include "doomstat.h"
#include "e_exdata.h"
#include "g_game.h"
#include "info.h"
#include "m_collection.h"
#include "m_compare.h"
#include "p_chase.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_portal.h"
#include "p_portalcross.h"
#include "p_tick.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_pcheck.h"
#include "r_portal.h"
#include "r_state.h"

//=============================================================================
//
// Shared Camera Code
//

//=============================================================================
//
// Chasecam
//

camera_t         chasecam;
int              chasecam_active = 0;
static v3fixed_t pCamTarget;
static int       pCamTargetGroupId;

// for simplicity
#define playermobj players[displayplayer].mo
#define playerangle (playermobj->angle)

int chasecam_height;
int chasecam_dist;

//
// Z of the line at the point of intersection.
// this function is really just to cast all the
// variables to 64-bit integers
//
static int zi(int64_t dist, int64_t totaldist, int64_t ztarget, int64_t playerz)
{
    int64_t thezi;

    thezi  = (dist * (ztarget - playerz)) / totaldist;
    thezi += playerz;

    return (int)thezi;
}

//
// Chase traverse context
//
struct chasetraverse_t
{
    const linkdata_t *link;         // if set, a portal was passed
    fixed_t           startz;       // start Z position
    v2fixed_t         intersection; // intersection position
};

//
// Check for a sector portal being hit
//
static bool P_checkSectorPortal(fixed_t z, fixed_t frac, const sector_t *sector, chasetraverse_t &traverse)
{
    for(surf_e surf : SURFS)
    {
        const surface_t &surface = sector->srf[surf];
        fixed_t          pz      = P_PortalZ(surface);
        if(surface.pflags & PS_PASSABLE && isOuter(surf, z, pz))
        {
            if(!isOuter(surf, pz, traverse.startz))
                pz = traverse.startz;
            fixed_t zfrac         = FixedDiv(pz - traverse.startz, z - traverse.startz);
            fixed_t hfrac         = FixedMul(zfrac, frac);
            traverse.intersection = trace.dl.v + trace.dl.dv.fixedMul(hfrac);
            traverse.link         = &surface.portal->data.link;
            traverse.startz       = pz;
            return true;
        }
    }

    return false;
}

//
// Check if it's an edge portal
//
static bool P_checkEdgePortal(const line_t *li, fixed_t x, fixed_t y, fixed_t z, fixed_t frac,
                              chasetraverse_t &traverse)
{
    for(surf_e surf : SURFS)
    {
        // TODO: sloped edge portals (what?!)
        if(li->extflags & e_edgePortalFlags[surf] && li->backsector->srf[surf].pflags & PS_PASSABLE &&
           isOuter(surf, z, P_PortalZ(li->backsector->srf[surf])) &&
           !isOuter(surf, z, li->frontsector->srf[surf].getZAt(x, y)))
        {
            traverse.intersection = trace.dl.v + trace.dl.dv.fixedMul(frac);
            traverse.link         = &li->backsector->srf[surf].portal->data.link;
            traverse.startz       = z;
            return true;
        }
    }
    return false;
}

//
// Checks if the line is a line portal and updates traverse
//
static bool P_checkLinePortal(const line_t *li, fixed_t z, fixed_t frac, chasetraverse_t &traverse)
{
    if(li->pflags & PS_PASSABLE)
    {
        // Exact target pos
        v2fixed_t targpos = { trace.dl.x + trace.dl.dx, trace.dl.y + trace.dl.dy };
        // Portal stuff. Only count it if truly crossed
        if(P_PointOnLineSidePrecise(targpos.x, targpos.y, li) && !P_PointOnLineSidePrecise(trace.dl.x, trace.dl.y, li))
        {
            traverse.intersection.x = trace.dl.x + FixedMul(trace.dl.dx, frac);
            traverse.intersection.y = trace.dl.y + FixedMul(trace.dl.dy, frac);
            traverse.link           = &li->portal->data.link;
            traverse.startz         = z;

            return true;
        }
    }
    return false;
}

//
// PTR_chaseTraverse
//
// go til you hit a wall
// set the chasecam target x and ys if you hit one
// originally based on the shooting traverse function in p_maputl.c
//
static bool PTR_chaseTraverse(intercept_t *in, void *context)
{
    if(in->isaline)
    {
        line_t *li = in->d.line;

        auto &traverse = *static_cast<chasetraverse_t *>(context);

        // Keep using trace.attackrange even when passing portals, because dist is only used in ratio
        // with fractional part. Safer because trace.attackrange is always MELEERANGE here.
        fixed_t dist = FixedMul(trace.attackrange, in->frac);
        fixed_t frac = in->frac - FixedDiv(12 * FRACUNIT, trace.attackrange);

        // hit line
        // position a bit closer

        fixed_t x = trace.dl.x + FixedMul(trace.dl.dx, frac);
        fixed_t y = trace.dl.y + FixedMul(trace.dl.dy, frac);

        if(li->flags & ML_TWOSIDED)
        { // crosses a two sided line
            // sf: find which side it hit

            subsector_t *ss = R_PointInSubsector(x, y);

            const sector_t *othersector = li->backsector;
            const sector_t *mysector    = li->frontsector;

            if(ss->sector == li->backsector) // other side
            {
                othersector = li->frontsector;
                mysector    = li->backsector;
            }

            // interpolate, find z at the point of intersection

            int z = zi(dist, trace.attackrange, pCamTarget.z, traverse.startz);

            // First check if the Z went low enough to hit a sector portal
            if(mysector && P_checkSectorPortal(z, in->frac, mysector, traverse))
                return false;

            // found which side, check for intersections

            // NOTE: for portal lines, "othersector" may lapse into the hidden buffer sector.
            // Let's tolerate this for now, even though correctly it should mean the sector
            // behind the portal.

            // Check for edge portals here
            if(!(li->flags & ML_BLOCKING) && mysector == li->frontsector &&
               P_checkEdgePortal(li, x, y, z, in->frac, traverse))
            {
                return false;
            }

            if(li->flags & ML_BLOCKING || othersector->srf.floor.getZAt(x, y) > z ||
               othersector->srf.ceiling.getZAt(x, y) < z ||
               othersector->srf.ceiling.getZAt(x, y) - othersector->srf.floor.getZAt(x, y) < 40 * FRACUNIT)
            {
            } // hit
            else
                return !P_checkLinePortal(li, z, in->frac, traverse);
        }

        pCamTarget.x = x; // point the new chasecam target at the intersection
        pCamTarget.y = y;
        pCamTarget.z = zi(dist, trace.attackrange, pCamTarget.z, traverse.startz);

        // don't go any farther

        return false;
    }

    return true;
}

static void P_GetChasecamTarget()
{
    // aimfor is the preferred height of the chasecam above
    // the player
    // haleyjd: 1 unit for each degree of pitch works surprisingly well
    fixed_t aimfor = players[displayplayer].viewheight + chasecam_height * FRACUNIT +
                     FixedDiv(players[displayplayer].pitch, ANGLE_1);

    trace.sin = finesine[playerangle >> ANGLETOFINESHIFT];
    trace.cos = finecosine[playerangle >> ANGLETOFINESHIFT];

    pCamTarget.x = playermobj->x - chasecam_dist * trace.cos;
    pCamTarget.y = playermobj->y - chasecam_dist * trace.sin;
    pCamTarget.z = playermobj->z + aimfor;

    pCamTargetGroupId = playermobj->groupid;

    // the intersections test mucks up the first time, but
    // aiming at something seems to cure it
    // ioanch 20160101: don't let P_AimLineAttack change global trace.attackrange
    fixed_t oldAttackRange = trace.attackrange;
    // ioanch 20160225: just change trace.attackrange, don't call P_AimLineAttack
    trace.attackrange = MELEERANGE;

    // check for intersections
    chasetraverse_t traverse;
    traverse.startz         = playermobj->z + 28 * FRACUNIT;
    v2fixed_t travstart     = { playermobj->x, playermobj->y };
    int       repprotection = 0;
    do
    {
        traverse.link = nullptr;
        bool clear    = P_PathTraverse(travstart.x, travstart.y, pCamTarget.x, pCamTarget.y, PT_ADDLINES,
                                       PTR_chaseTraverse, &traverse);
        if(!traverse.link && clear)
        {
            const subsector_t *ss = R_PointInSubsector(pCamTarget.x, pCamTarget.y);
            P_checkSectorPortal(pCamTarget.z, FRACUNIT, ss->sector, traverse);
        }
        if(traverse.link)
        {
            travstart          = traverse.intersection + v2fixed_t(traverse.link->delta);
            traverse.startz   += traverse.link->delta.z;
            pCamTarget        += traverse.link->delta;
            pCamTargetGroupId  = traverse.link->toid;
        }
    }
    while(traverse.link && repprotection++ < 64);
    trace.attackrange = oldAttackRange;

    const subsector_t *ss = R_PointInSubsector(pCamTarget.x, pCamTarget.y);

    fixed_t floorheight   = ss->sector->srf.floor.getZAt(v2fixed_t(pCamTarget));
    fixed_t ceilingheight = ss->sector->srf.ceiling.getZAt(v2fixed_t(pCamTarget));

    // don't aim above the ceiling or below the floor
    if(!(ss->sector->srf.floor.pflags & PS_PASSABLE) && pCamTarget.z < floorheight + 10 * FRACUNIT)
        pCamTarget.z = floorheight + 10 * FRACUNIT;
    if(!(ss->sector->srf.ceiling.pflags & PS_PASSABLE) && pCamTarget.z > ceilingheight - 10 * FRACUNIT)
        pCamTarget.z = ceilingheight - 10 * FRACUNIT;
}

// the 'speed' of the chasecam: the percentage closer we
// get to the target each tic
int chasecam_speed;

void P_ChaseTicker()
{
    int xdist, ydist, zdist;

    // backup current position for interpolation
    chasecam.backupPosition();

    // find the target
    P_GetChasecamTarget();

    // find distance to target..

    if(chasecam.groupid != pCamTargetGroupId)
    {
        // FIXME: this causes some twitching, need to improve it.
        chasecam.x       = pCamTarget.x;
        chasecam.y       = pCamTarget.y;
        chasecam.z       = pCamTarget.z;
        chasecam.groupid = pCamTargetGroupId;
        chasecam.backupPosition();
        return;
    }

    xdist = pCamTarget.x - chasecam.x;
    ydist = pCamTarget.y - chasecam.y;
    zdist = pCamTarget.z - chasecam.z;
    // haleyjd: patched these lines with cph's fix
    //          for overflow occuring in the multiplication
    // now move chasecam

    chasecam.groupid = R_PointInSubsector(chasecam.x, chasecam.y)->sector->groupid;
    v2fixed_t dest   = P_LinePortalCrossing(chasecam.x, chasecam.y, FixedMul(xdist, chasecam_speed * (FRACUNIT / 100)),
                                            FixedMul(ydist, chasecam_speed * (FRACUNIT / 100)), &chasecam.groupid);

    chasecam.x  = dest.x;
    chasecam.y  = dest.y;
    chasecam.z += FixedMul(zdist, chasecam_speed * (FRACUNIT / 100));

    chasecam.pitch = players[displayplayer].pitch;
    chasecam.angle = playerangle;
}

// console commands

VARIABLE_BOOLEAN(chasecam_active, nullptr, onoff);

CONSOLE_VARIABLE(chasecam, chasecam_active, 0)
{
    if(!Console.argc)
        return;

    if(Console.argv[0]->toInt())
        P_ChaseStart();
    else
        P_ChaseEnd();
}

VARIABLE_INT(chasecam_height, nullptr, -31, 100, nullptr);
CONSOLE_VARIABLE(chasecam_height, chasecam_height, 0) {}

VARIABLE_INT(chasecam_dist, nullptr, 10, 1024, nullptr);
CONSOLE_VARIABLE(chasecam_dist, chasecam_dist, 0) {}

VARIABLE_INT(chasecam_speed, nullptr, 1, 100, nullptr);
CONSOLE_VARIABLE(chasecam_speed, chasecam_speed, 0) {}

void P_ChaseStart()
{
    chasecam_active = true;
    camera          = &chasecam;
    P_ResetChasecam();
}

void P_ChaseEnd()
{
    chasecam_active = false;
    camera          = nullptr;
}

// SoM: moved globals into linetracer_t see p_maputil.h
extern linetracer_t trace;

//
// P_ResetChasecam
//
// Reset chasecam eg after teleporting etc
//
void P_ResetChasecam()
{
    if(!chasecam_active)
        return;

    if(gamestate != GS_LEVEL)
        return; // only in level

    // find the chasecam target
    P_GetChasecamTarget();

    chasecam.x = pCamTarget.x;
    chasecam.y = pCamTarget.y;
    chasecam.z = pCamTarget.z;

    chasecam.groupid = pCamTargetGroupId;

    chasecam.backupPosition();
}

//==============================================================================
//
// WALK AROUND CAMERA
//
// Walk around inside demos without upsetting demo sync.
//

camera_t walkcamera;
int      walkcam_active = 0;

//
// Checks walkcam for passing through an interactive portal plane.
//
static void P_checkWalkcamSectorPortal(const sector_t *sector)
{
    static const int MAXIMUM_PER_TIC = 8;
    bool             movedalready    = false;

    for(surf_e surf : SURFS)
    {
        if(movedalready)
            return;
        const surface_t &surface = sector->srf[surf];
        for(int j = 0; j < MAXIMUM_PER_TIC; ++j)
        {
            if(!(surface.pflags & PS_PASSABLE))
                break;
            fixed_t planez = P_PortalZ(surface);
            if(!isOuter(surf, walkcamera.z, planez))
                break;
            const linkdata_t &ldata  = surface.portal->data.link;
            walkcamera.x            += ldata.delta.x;
            walkcamera.y            += ldata.delta.y;
            walkcamera.z            += ldata.delta.z;
            walkcamera.groupid       = ldata.toid;
            sector                   = R_PointInSubsector(walkcamera.x, walkcamera.y)->sector;
            movedalready             = true;
            walkcamera.backupPosition();
        }
    }
}

void P_WalkTicker()
{
    ticcmd_t *walktic = &netcmds[consoleplayer][(gametic / ticdup) % BACKUPTICS];
    int       look    = walktic->look;
    int       fly     = walktic->fly;
    angle_t   fwan, san;

    // backup position for interpolation
    walkcamera.backupPosition();

    walkcamera.angle += walktic->angleturn << 16;
    bool moved        = false;

    // looking up/down
    // haleyjd: this is the same as new code in p_user.c, but for walkcam
    if(look)
    {
        // test for special centerview value
        if(look == -32768)
            walkcamera.pitch = 0;
        else
        {
            walkcamera.pitch -= look << 16;
            int maxpitchup    = GameModeInfo->lookPitchUp;
            int maxpitchdown  = GameModeInfo->lookPitchDown;
            if(walkcamera.pitch < -ANGLE_1 * maxpitchup)
                walkcamera.pitch = -ANGLE_1 * maxpitchup;
            else if(walkcamera.pitch > ANGLE_1 * maxpitchdown)
                walkcamera.pitch = ANGLE_1 * maxpitchdown;
        }
    }

    if(fly == FLIGHT_CENTER)
        walkcamera.flying = false;
    else if(fly)
    {
        walkcamera.z      += 2 * fly * FRACUNIT;
        moved              = true;
        walkcamera.flying  = true;
    }

    if(walkcamera.flying && walkcamera.pitch)
    {
        angle_t an     = static_cast<angle_t>(walkcamera.pitch);
        an           >>= ANGLETOFINESHIFT;
        walkcamera.z  -= FixedMul((ORIG_FRICTION / 4) * walktic->forwardmove, finesine[an]);
        moved          = true;
    }

    v2fixed_t dest = { walkcamera.x, walkcamera.y };

    // moving forward
    if(walktic->forwardmove)
    {
        fwan     = walkcamera.angle;
        fwan   >>= ANGLETOFINESHIFT;
        dest.x  += FixedMul((ORIG_FRICTION / 4) * walktic->forwardmove, finecosine[fwan]);
        dest.y  += FixedMul((ORIG_FRICTION / 4) * walktic->forwardmove, finesine[fwan]);
    }

    // strafing
    if(walktic->sidemove)
    {
        san      = walkcamera.angle - ANG90;
        san    >>= ANGLETOFINESHIFT;
        dest.x  += FixedMul((ORIG_FRICTION / 6) * walktic->sidemove, finecosine[san]);
        dest.y  += FixedMul((ORIG_FRICTION / 6) * walktic->sidemove, finesine[san]);
    }

    if(dest.x != walkcamera.x || dest.y != walkcamera.y)
    {
        int oldgroupid = walkcamera.groupid;
        dest           = P_LinePortalCrossing(walkcamera.x, walkcamera.y, dest.x - walkcamera.x, dest.y - walkcamera.y,
                                              &walkcamera.groupid);
        walkcamera.x   = dest.x;
        walkcamera.y   = dest.y;
        if(walkcamera.groupid != oldgroupid)
            walkcamera.backupPosition();
        moved = true;
    }

    // haleyjd: FIXME -- this could be optimized by only
    // doing a traversal when the camera actually moves, rather
    // than every frame, naively
    if(!moved)
        return;

    subsector_t *subsec = R_PointInSubsector(walkcamera.x, walkcamera.y);

    v2fixed_t topdelta, bottomdelta;

    const sector_t *topsector =
        P_ExtremeSectorAtPoint(walkcamera.x, walkcamera.y, surf_ceil, subsec->sector, &topdelta);
    const sector_t *bottomsector =
        P_ExtremeSectorAtPoint(walkcamera.x, walkcamera.y, surf_floor, subsec->sector, &bottomdelta);

    if(!walkcamera.flying)
    {
        // keep on the ground
        walkcamera.z =
            bottomsector->srf.floor.getZAt(walkcamera.x + bottomdelta.x, walkcamera.y + bottomdelta.y) + 41 * FRACUNIT;
    }

    fixed_t maxheight =
        topsector->srf.ceiling.getZAt(walkcamera.x + topdelta.x, walkcamera.y + topdelta.y) - 8 * FRACUNIT;
    fixed_t minheight =
        bottomsector->srf.floor.getZAt(walkcamera.x + bottomdelta.x, walkcamera.y + bottomdelta.y) + 4 * FRACUNIT;

    if(walkcamera.z > maxheight)
        walkcamera.z = maxheight;
    if(walkcamera.z < minheight)
        walkcamera.z = minheight;

    // Now check portal teleport
    P_checkWalkcamSectorPortal(subsec->sector);
}

void P_ResetWalkcam()
{
    if(gamestate != GS_LEVEL)
        return; // only in level

    sector_t *sec;
    // ioanch 20151218: fixed point mapthing coordinates
    walkcamera.x      = playerstarts[0].x;
    walkcamera.y      = playerstarts[0].y;
    walkcamera.angle  = R_WadToAngle(playerstarts[0].angle);
    walkcamera.pitch  = 0;
    walkcamera.flying = false;

    // haleyjd
    sec                = R_PointInSubsector(walkcamera.x, walkcamera.y)->sector;
    walkcamera.z       = sec->srf.floor.getZAt(walkcamera.x, walkcamera.y) + 41 * FRACUNIT;
    walkcamera.groupid = sec->groupid;

    walkcamera.backupPosition();
}

VARIABLE_BOOLEAN(walkcam_active, nullptr, onoff);
CONSOLE_VARIABLE(walkcam, walkcam_active, cf_notnet)
{
    if(!Console.argc)
        return;

    if(Console.argv[0]->toInt())
        P_WalkStart();
    else
        P_WalkEnd();
}

void P_WalkStart()
{
    walkcam_active = true;
    camera         = &walkcamera;
    P_ResetWalkcam();
}

void P_WalkEnd()
{
    walkcam_active = false;
    camera         = nullptr;
}

//==============================================================================
//
// Follow Cam
//
// Follows a designated target.
//

camera_t     followcam;
static Mobj *followtarget;

//
// P_LocateFollowCam
//
// Find a suitable location for the followcam by finding the furthest vertex
// in its sector from which it is visible, using CAM_CheckSight, which is
// guaranteed to never disturb the state of the game engine.
//
void P_LocateFollowCam(Mobj *target, fixed_t &destX, fixed_t &destY)
{
    PODCollection<vertex_t *> cvertexes;
    sector_t                 *sec = target->subsector->sector;

    // Get all vertexes in the target's sector within 256 units
    for(int i = 0; i < sec->linecount; i++)
    {
        vertex_t *v1 = sec->lines[i]->v1;
        vertex_t *v2 = sec->lines[i]->v2;

        if(P_AproxDistance(v1->x - target->x, v1->y - target->y) <= 256 * FRACUNIT)
            cvertexes.add(v1);
        if(P_AproxDistance(v2->x - target->x, v2->y - target->y) <= 256 * FRACUNIT)
            cvertexes.add(v2);
    }

    // Sort by distance from the target, with the furthest vertex first.
    std::sort(cvertexes.begin(), cvertexes.end(), [&](vertex_t *a, vertex_t *b) {
        return (P_AproxDistance(target->x - a->x, target->y - a->y) >
                P_AproxDistance(target->x - b->x, target->y - b->y));
    });

    // Find the furthest one from which the target is visible
    for(vertex_t *&v : cvertexes)
    {
        camsightparams_t camparams;

        camparams.cx       = v->x;
        camparams.cy       = v->y;
        camparams.cz       = sec->srf.floor.getZAt(target->x, target->y);
        camparams.cheight  = 41 * FRACUNIT;
        camparams.cgroupid = sec->groupid;
        camparams.prev     = nullptr;
        camparams.setTargetMobj(target);

        if(CAM_CheckSight(camparams))
        {
            angle_t ang = P_PointToAngle(v->x, v->y, target->x, target->y);

            // Push coordinates in slightly toward the target
            destX = v->x + 10 * finecosine[ang >> ANGLETOFINESHIFT];
            destY = v->y + 10 * finesine[ang >> ANGLETOFINESHIFT];

            return; // We've found our location
        }
    }

    // If we got here, somehow the target isn't visible... (shouldn't happen)
    // Use the target's coordinates.
    destX = target->x;
    destY = target->y;
}

//
// P_setFollowPitch
//
static void P_setFollowPitch()
{
    fixed_t aimz = followtarget->z + 41 * FRACUNIT;
    fixed_t zabs = abs(aimz - followcam.z);

    fixed_t fixedang;
    double  zdist;
    bool    camlower = (followcam.z < aimz);
    double  xydist   = M_FixedToDouble(P_AproxDistance(followtarget->x - followcam.x, followtarget->y - followcam.y));

    zdist    = M_FixedToDouble(zabs);
    fixedang = (fixed_t)(atan2(zdist, xydist) * ((unsigned int)ANG180 / PI));

    if(fixedang > ANGLE_1 * 32)
        fixedang = ANGLE_1 * 32;

    followcam.prevpitch = followcam.pitch;
    followcam.pitch     = camlower ? -fixedang : fixedang;
}

//
// P_SetFollowCam
//
// Locate the followcam at the indicated location, looking at the target.
//
void P_SetFollowCam(fixed_t x, fixed_t y, Mobj *target)
{
    subsector_t *subsec;

    followcam.x = x;
    followcam.y = y;
    P_SetTarget<Mobj>(&followtarget, target);

    followcam.angle = P_PointToAngle(followcam.x, followcam.y, followtarget->x, followtarget->y);

    subsec      = R_PointInSubsector(followcam.x, followcam.y);
    followcam.z = subsec->sector->srf.floor.getZAt(followcam.x, followcam.y) + 41 * FRACUNIT;

    P_setFollowPitch();
    followcam.backupPosition();
}

void P_FollowCamOff()
{
    P_SetTarget<Mobj>(&followtarget, nullptr);
}

bool P_FollowCamTicker()
{
    subsector_t *subsec;

    if(!followtarget)
        return false;

    followcam.backupPosition();

    followcam.angle = P_PointToAngle(followcam.x, followcam.y, followtarget->x, followtarget->y);

    subsec            = R_PointInSubsector(followcam.x, followcam.y);
    followcam.z       = subsec->sector->srf.floor.getZAt(followcam.x, followcam.y) + 41 * FRACUNIT;
    followcam.groupid = subsec->sector->groupid;
    P_setFollowPitch();

    // still visible?
    camsightparams_t camparams;
    camparams.prev = nullptr;
    camparams.setCamera(followcam, 41 * FRACUNIT);
    camparams.setTargetMobj(followtarget);

    return CAM_CheckSight(camparams);
}

#if 0
static cell AMX_NATIVE_CALL sm_chasecam(AMX *amx, cell *params)
{
    int cam_onoff = (int)params[1];

    if(gamestate != GS_LEVEL)
    {
        amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
        return -1;
    }

    if(cam_onoff)
        P_ChaseStart();
    else
        P_ChaseEnd();

    return 0;
}

static cell AMX_NATIVE_CALL sm_ischaseon(AMX *amx, cell *params)
{
    if(gamestate != GS_LEVEL)
    {
        amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
        return -1;
    }

    return chasecam_active;
}

AMX_NATIVE_INFO chase_Natives[] = {
    { "_ToggleChasecam", sm_chasecam  },
    { "_IsChasecamOn",   sm_ischaseon },
    { nullptr,           nullptr      }
};
#endif

// EOF

