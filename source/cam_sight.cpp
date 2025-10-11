//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
//
// ZDoom
// Copyright (C) 1998-2012 Marisa Heit
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
// Purpose: Line of sight checking for cameras.
// Authors: James Haley, Ioan Chera
//

#include "z_zone.h"

#include "cam_common.h"
#include "cam_sight.h"
#include "doomstat.h"  // ioanch 20160101: for bullet attacks
#include "d_gi.h"      // ioanch 20160131: for use
#include "d_player.h"  // ioanch 20151230: for autoaim
#include "m_compare.h" // ioanch 20160103: refactor
#include "e_exdata.h"
#include "m_collection.h"
#include "p_chase.h"
#include "p_info.h"
#include "p_inter.h" // ioanch 20160101: for damage
#include "p_portal.h"
#include "p_map.h" // ioanch 20160131: for use
#include "p_maputl.h"
#include "p_setup.h"
#include "p_skin.h" // ioanch 20160131: for use
#include "p_spec.h" // ioanch 20160101: for bullet effects
#include "polyobj.h"
#include "r_pcheck.h" // ioanch 20160109: for correct portal plane z
#include "r_defs.h"
#include "r_main.h"
#include "r_portal.h"
#include "r_sky.h" // ioanch 20160101: for bullets hitting sky
#include "r_state.h"
#include "s_sound.h" // ioanch 20160131: for use

static constexpr int RECURSION_LIMIT = 64;

//=============================================================================
//
// Structures
//

class CamSight
{
public:
    fixed_t cx; // camera coordinates
    fixed_t cy;
    fixed_t tx; // target coordinates
    fixed_t ty;
    fixed_t sightzstart; // eye z of looker
    fixed_t topslope;    // slope to top of target
    fixed_t bottomslope; // slope to bottom of target

    divline_t trace; // for line crossing tests

    fixed_t opentop;    // top of linedef silhouette
    fixed_t openbottom; // bottom of linedef silhouette
    fixed_t openrange;  // height of opening

    // Intercepts vector
    PODCollection<intercept_t> intercepts;

    // portal traversal information
    int  fromid;       // current source group id
    int  toid;         // group id of the target
    bool portalresult; // result from portal recursion
    bool portalexit;   // if true, returned from portal

    // pointer to invocation parameters
    const camsightparams_t *params;

    // ioanch 20151229: added optional bottom and top slope preset
    // in case of portal continuation
    explicit CamSight(const camsightparams_t &sp)
        : cx(sp.cx), cy(sp.cy), tx(sp.tx), ty(sp.ty), opentop(0), openbottom(0), openrange(0), intercepts(),
          fromid(sp.cgroupid), toid(sp.tgroupid), portalresult(false), portalexit(false), params(&sp)
    {
        memset(&trace, 0, sizeof(trace));

        sightzstart = params->cz + params->cheight - (params->cheight >> 2);

        const linkoffset_t *link = P_GetLinkIfExists(toid, fromid);
        bottomslope              = params->tz - sightzstart;
        if(link)
        {
            bottomslope += link->z; // also adjust for Z offset
        }

        topslope = bottomslope + params->theight;
    }
};

//=============================================================================
//
// Camera Sight Parameter Methods
//

//
// camsightparams_t::setCamera
//
// Set a camera object as the camera point.
//
void camsightparams_t::setCamera(const camera_t &camera, fixed_t height)
{
    cx       = camera.x;
    cy       = camera.y;
    cz       = camera.z;
    cheight  = height;
    cgroupid = camera.groupid;
}

//
// camsightparams_t::setLookerMobj
//
// Set an Mobj as the camera point.
//
void camsightparams_t::setLookerMobj(const Mobj *mo)
{
    cx       = mo->x;
    cy       = mo->y;
    cz       = mo->z;
    cheight  = mo->height;
    cgroupid = mo->groupid;
}

//
// camsightparams_t::setTargetMobj
//
// Set an Mobj as the target point.
//
void camsightparams_t::setTargetMobj(const Mobj *mo)
{
    tx       = mo->x;
    ty       = mo->y;
    tz       = mo->z;
    theight  = mo->height;
    tgroupid = mo->groupid;
}

///////////////////////////////////////////////////////////////////////////////
//
// CamContext
//
// All the data needed for sight checking
//
class CamContext
{
public:
    struct State
    {
        fixed_t           originfrac;
        Surfaces<fixed_t> slope;
        int               reclevel;
    };

    static bool checkSight(const camsightparams_t &inparams, const State *state);

private:
    CamContext(const camsightparams_t &inparams, const State *state);
    static bool sightTraverse(const intercept_t *in, void *context, const divline_t &trace);
    bool        checkPortalSector(const sector_t *sector, fixed_t totalfrac, fixed_t partialfrac,
                                  const divline_t &trace) const;
    bool recurse(int groupid, fixed_t x, fixed_t y, const State &state, bool *result, const linkdata_t &data) const;

    bool                    portalexit, portalresult;
    State                   state;
    const camsightparams_t *params;
    fixed_t                 sightzstart;
};

//
// CamContext::CamContext
//
// Constructs it, either from a previous state or scratch
//
CamContext::CamContext(const camsightparams_t &inparams, const State *instate)
    : portalexit(false), portalresult(false), params(&inparams)
{

    sightzstart = params->cz + params->cheight - (params->cheight >> 2);
    if(instate)
        state = *instate;
    else
    {
        state.originfrac    = 0;
        state.slope.floor   = params->tz - sightzstart;
        state.slope.ceiling = state.slope.floor + params->theight;
        state.reclevel      = 0;
    }
}

//
// CamContext::sightTraverse
//
// Intercept routine for CamContext
//
bool CamContext::sightTraverse(const intercept_t *in, void *vcontext, const divline_t &trace)
{
    const line_t      *li      = in->d.line;
    tracelineopening_t lo      = { 0 };
    v2fixed_t          edgepos = trace.v + trace.dv.fixedMul(in->frac);
    lo.calculateAtPoint(*li, edgepos);

    CamContext &context = *static_cast<CamContext *>(vcontext);

    // avoid round-off errors if possible
    fixed_t         totalfrac = context.state.originfrac ?
                                    context.state.originfrac + FixedMul(in->frac, FRACUNIT - context.state.originfrac) :
                                    in->frac;
    const sector_t *sector    = P_PointOnLineSidePrecise(trace.x, trace.y, li) == 0 ? li->frontsector : li->backsector;
    if(sector && totalfrac > 0)
    {
        if(context.checkPortalSector(sector, totalfrac, in->frac, trace))
        {
            context.portalresult = context.portalexit = true;
            return false;
        }
    }

    // Keep support for DOOM and Strife's lack of passing through misflagged lines
    if(LevelInfo.levelType != LI_TYPE_HERETIC && LevelInfo.levelType != LI_TYPE_HEXEN && !(li->flags & ML_TWOSIDED))
    {
        return false;
    }

    // quick test for totally closed doors
    // ioanch 20151231: also check BLOCKALL lines
    if(lo.openrange <= 0 || li->extflags & EX_ML_BLOCKALL)
        return false;

    const sector_t *osector = sector == li->frontsector ? li->backsector : li->frontsector;
    fixed_t         slope;

    for(surf_e surf : SURFS)
    {
        const surface_t &surface      = sector->srf[surf];
        const surface_t &otherSurface = osector->srf[surf];
        if(surface.getZAt(edgepos) != otherSurface.getZAt(edgepos) ||
           (surface.pflags & PS_PASSABLE) != (otherSurface.pflags & PS_PASSABLE))
        {
            slope = FixedDiv(lo.open[surf] - context.sightzstart, totalfrac);
            if(isInner(surf, slope, context.state.slope[surf]))
                context.state.slope[surf] = slope;
        }
    }

    if(context.state.slope.ceiling <= context.state.slope.floor)
        return false; // stop

    // have we hit an edge portal?
    for(surf_e surf : SURFS)
    {
        const surface_t &backSurface = li->backsector->srf[surf];

        if(li->extflags & e_edgePortalFlags[surf] && li->backsector && backSurface.pflags & PS_PASSABLE &&
           !isInner(surf, context.state.slope[surf],
                    FixedDiv(backSurface.getZAt(edgepos) - context.sightzstart, totalfrac)) &&
           P_PointOnLineSidePrecise(trace.x, trace.y, li) == 0 && in->frac > 0)
        {
            State state(context.state);
            state.originfrac = totalfrac;
            if(context.recurse(backSurface.portal->data.link.toid, context.params->cx + FixedMul(trace.dx, in->frac),
                               context.params->cy + FixedMul(trace.dy, in->frac), state, &context.portalresult,
                               backSurface.portal->data.link))
            {
                context.portalexit = true;
                return false;
            }
        }
    }

    // have we hit a portal line
    if(li->pflags & PS_PASSABLE && P_PointOnLineSidePrecise(trace.x, trace.y, li) == 0 && in->frac > 0)
    {
        State state(context.state);
        state.originfrac = totalfrac;
        if(context.recurse(li->portal->data.link.toid, context.params->cx + FixedMul(trace.dx, in->frac),
                           context.params->cy + FixedMul(trace.dy, in->frac), state, &context.portalresult,
                           li->portal->data.link))
        {
            context.portalexit = true;
            return false;
        }
    }

    return true; // keep going
}

//
// CamContext::checkPortalSector
//
// ioanch 20151229: check sector if it has portals and create cameras for them
// Returns true if any branching sight check beyond a portal returned true
//
bool CamContext::checkPortalSector(const sector_t *sector, const fixed_t totalfrac, const fixed_t partialfrac,
                                   const divline_t &trace) const
{
    for(const surf_e surf : SURFS)
    {
        fixed_t          slope     = state.slope[surf];
        const surface_t &surface   = sector->srf[surf];
        int              newfromid = R_NOGROUP;
        if(isOuter(surf, slope, 0) && surface.pflags & PS_PASSABLE &&
           (newfromid = surface.portal->data.link.toid) != params->cgroupid)
        {
            // Surface portal (slope must lead to it)
            fixed_t linehitz = sightzstart + FixedMul(slope, totalfrac);
            fixed_t planez   = P_PortalZ(surface);
            if(isOuter(surf, linehitz, planez))
            {
                // update other slope to be the edge of the sector wall
                // if totalfrac == 0, then it will just be a very big slope
                fixed_t newslope = FixedDiv(planez - sightzstart, totalfrac);

                if(isOuter(!surf, newslope, state.slope[!surf]))
                    newslope = state.slope[!surf];

                // get x and y of position
                fixed_t x, y;
                fixed_t newtotalfrac   = totalfrac;
                fixed_t newpartialfrac = partialfrac;
                if(linehitz == sightzstart)
                {
                    // handle this edge case: put point right on line
                    x = trace.x + FixedMul(trace.dx, newpartialfrac);
                    y = trace.y + FixedMul(trace.dy, newpartialfrac);
                }
                else
                {
                    fixed_t sectorfrac = FixedDiv(planez - sightzstart, linehitz - sightzstart);
                    // update z frac
                    newtotalfrac = FixedMul(sectorfrac, newtotalfrac);
                    // retrieve the xy frac using the origin frac
                    newpartialfrac = FixedDiv(newtotalfrac - state.originfrac, FRACUNIT - state.originfrac);

                    // HACK: add a unit just to ensure that it enters the sector
                    x = trace.x + FixedMul(newpartialfrac + 1, trace.dx);
                    y = trace.y + FixedMul(newpartialfrac + 1, trace.dy);
                }

                if(newpartialfrac + 1 > 0) // don't allow going back
                {
                    State newstate;
                    newstate.slope[surf]  = slope;
                    newstate.slope[!surf] = newslope;
                    newstate.originfrac   = newtotalfrac;
                    newstate.reclevel     = state.reclevel + 1;

                    bool result = false;
                    if(recurse(newfromid, x, y, newstate, &result, surface.portal->data.link) && result)
                        return true;
                }
            }
        }
    }

    return false;
}

//
// CamContext::recurse
//
// If valid, starts a new lookup from below
//
bool CamContext::recurse(int newfromid, fixed_t x, fixed_t y, const State &instate, bool *result,
                         const linkdata_t &data) const
{
    if(newfromid == params->cgroupid)
        return false; // not taking us anywhere...

    if(instate.reclevel >= RECURSION_LIMIT)
        return false; // protection

    camsightparams_t params;
    params.cx       = x + data.delta.x;
    params.cy       = y + data.delta.y;
    params.cz       = this->params->cz + data.delta.z;
    params.cheight  = this->params->cheight;
    params.tx       = this->params->tx;
    params.ty       = this->params->ty;
    params.tz       = this->params->tz;
    params.theight  = this->params->theight;
    params.cgroupid = newfromid;
    params.tgroupid = this->params->tgroupid;
    params.prev     = this->params;

    *result = checkSight(params, &instate);
    return true;
}

//
// CAM_CheckSight
//
// Returns true if a straight line between the camera location and a
// thing's coordinates is unobstructed.
//
// ioanch 20151229: line portal sight fix
// Also added bottomslope and topslope pre-setting
//
bool CamContext::checkSight(const camsightparams_t &params, const CamContext::State *state)
{
    const linkoffset_t *link = nullptr;
    // Camera and target are not in same group?
    if(params.cgroupid != params.tgroupid)
    {
        // is there a link between these groups?
        // if so, ignore reject
        link = P_GetLinkIfExists(params.cgroupid, params.tgroupid);
    }

    const sector_t *csec, *tsec;
    size_t          s1, s2, pnum;
    //
    // check for trivial rejection
    //
    s1   = (csec = R_PointInSubsector(params.cx, params.cy)->sector) - sectors;
    s2   = (tsec = R_PointInSubsector(params.tx, params.ty)->sector) - sectors;
    pnum = s1 * numsectors + s2;

    bool result = false;

    if(link || !(rejectmatrix[pnum >> 3] & (1 << (pnum & 7))))
    {
        // killough 4/19/98: make fake floors and ceilings block monster view
        if((csec->heightsec != -1 &&
            ((params.cz + params.cheight <= sectors[csec->heightsec].srf.floor.getZAt(params.cx, params.cy) &&
              params.tz >= sectors[csec->heightsec].srf.floor.getZAt(params.tx, params.ty)) ||
             (params.cz >= sectors[csec->heightsec].srf.ceiling.getZAt(params.cx, params.cy) &&
              params.tz + params.cheight <= sectors[csec->heightsec].srf.ceiling.getZAt(params.tx, params.ty)))) ||
           (tsec->heightsec != -1 &&
            ((params.tz + params.theight <= sectors[tsec->heightsec].srf.floor.getZAt(params.tx, params.ty) &&
              params.cz >= sectors[tsec->heightsec].srf.floor.getZAt(params.cx, params.cy)) ||
             (params.tz >= sectors[tsec->heightsec].srf.ceiling.getZAt(params.tx, params.ty) &&
              params.cz + params.theight <= sectors[tsec->heightsec].srf.ceiling.getZAt(params.cx, params.cy)))))
        {
            return false;
        }

        //
        // check precisely
        //
        CamContext context(params, state);

        // if there is a valid portal link, adjust the target's coordinates now
        // so that we trace in the proper direction given the current link

        // ioanch 20151229: store displaced target coordinates because newCam.tx
        // and .ty will be modified internally
        fixed_t tx = params.tx;
        fixed_t ty = params.ty;
        if(link)
        {
            tx -= link->x;
            ty -= link->y;
        }
        PTDef def;
        def.flags    = CAM_ADDLINES;
        def.earlyOut = link ? PTDef::eo_noearlycheck : PTDef::eo_always;
        def.trav     = CamContext::sightTraverse;
        PathTraverser traverser(def, &context);
        result = traverser.traverse(params.cx, params.cy, tx, ty);

        if(context.portalexit)
            result = context.portalresult;
        else if(result && params.cgroupid != params.tgroupid)
        {
            result = false;
            if(link)
            {
                tsec = R_PointInSubsector(tx, ty)->sector;
                if(context.checkPortalSector(tsec, FRACUNIT, FRACUNIT, traverser.trace))
                {
                    result = true;
                }
            }
        }
    }
    return result;
}

//
// CAM_CheckSight
//
// Entry
//
bool CAM_CheckSight(const camsightparams_t &params)
{
    return CamContext::checkSight(params, nullptr);
}

// EOF

