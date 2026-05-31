//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Ioan Chera, et al.
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
// Purpose: Reentrant path traversing, used by several new functions
// Authors: James Haley, Ioan Chera, Max Waine
//

#include "z_zone.h"

#include "cam_common.h"
#include "cam_sight.h"
#include "d_gi.h"
#include "doomstat.h"
#include "e_exdata.h"
#include "m_compare.h"
#include "p_info.h"
#include "p_portal.h"
#include "p_portalblockmap.h"
#include "p_setup.h"
#include "polyobj.h"
#include "r_main.h"
#include "r_portal.h"
#include "r_state.h"

//
// Stores data into a LineOpening struct
//
void tracelineopening_t::calculate(const line_t *linedef)
{
    calculateAtPoint(*linedef, v2fixed_t(linedef->soundorg.x, linedef->soundorg.y));
}

//
// Calculates opening for a given zero-volume point. Needed for slopes.
//
void tracelineopening_t::calculateAtPoint(const line_t &line, v2fixed_t pos)
{
    if(line.sidenum[1] == -1)
    {
        openrange = 0;
        return;
    }

    const bool isPolyObj2Sided =
        !P_LevelIsVanillaHexen() && demo_version >= 406 && line.flags & ML_TWOSIDED && line.intflags & MLI_DYNASEGLINE;

    const sector_t *front;
    const sector_t *back;
    if(isPolyObj2Sided && !(line.intflags & MLI_1SPORTALLINE))
    {
        // For a polyobject 2-sided line, we don't actually have a top and bottom limit -- that's established by static
        // lines
        open.floor   = D_MININT;
        open.ceiling = D_MAXINT;
        openrange    = D_MAXINT;
        return;
    }
    else
    {
        front = isPolyObj2Sided ? R_PointInSubsector(pos)->sector : line.frontsector;
        back  = line.backsector;
    }

    v2fixed_t backpos = pos;

    const sector_t *beyond =
        line.intflags & MLI_1SPORTALLINE && line.beyondportalline ? line.beyondportalline->frontsector : nullptr;
    if(beyond)
    {
        back = beyond;
        if(line.portal && line.portal->type == R_LINKED)
        {
            backpos.x += line.portal->data.link.delta.x;
            backpos.y += line.portal->data.link.delta.y;
        }
    }

    if(line.extflags & EX_ML_UPPERPORTAL && back->srf.ceiling.pflags & PS_PASSABLE)
        open.ceiling = front->srf.ceiling.getZAt(pos);
    else
        open.ceiling = emin(front->srf.ceiling.getZAt(pos), back->srf.ceiling.getZAt(backpos));

    if(line.extflags & EX_ML_LOWERPORTAL && back->srf.floor.pflags & PS_PASSABLE)
        open.floor = front->srf.floor.getZAt(pos);
    else
        open.floor = emax(front->srf.floor.getZAt(pos), back->srf.floor.getZAt(backpos));
    openrange = open.ceiling - open.floor;
}

// EOF
