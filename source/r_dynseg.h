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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//------------------------------------------------------------------------------
//
// Purpose: Dynamic segs for PolyObject re-implementation.
// Authors: James Haley, Ioan Chera, Max Waine
//

#ifndef R_DYNSEG_H__
#define R_DYNSEG_H__

#include "r_defs.h"
#include "m_dllist.h"
#include "m_vector.h"
#include "polyobj.h"

struct seginterp_t
{
    float     offset;
    float     len;
    v2fixed_t org[2];
    v2float_t forg[2];
};

//
// dynaseg
//
struct dynaseg_t
{
    seg_t seg;      // a dynaseg is a seg, after all ;)
    bool  backside; // true if it's for the backside

    dynavertex_t *originalv2;      // reference to original v2 before a split
    dynavertex_t *linev1, *linev2; // dynavertices belonging to the endpoint segs, for interpolation

    dynaseg_t *subnext;  // next dynaseg in fragment
    dynaseg_t *freenext; // next dynaseg on freelist
    polyobj_t *polyobj;  // polyobject

    DLListItem<dynaseg_t> bsplink;   // link for BSP chains
    DLListItem<dynaseg_t> ownerlink; // link for owning node chain
    DLListItem<dynaseg_t> alterlink; // link for non-dynaBSP segs changed by dynaBSP

    // FIXME: Can probably cut this down somewhat. Seems a bit space inefficient (data-wise).
    float       prevlen, prevofs; // for interpolation (keep them out of seg_t)
    seginterp_t prev;

    // properties needed for efficiency in the BSP builder
    double psx, psy, pex, pey; // end points
    double pdx, pdy;           // delta x, delta y
    double ptmp;               // general line coefficient 'c'
    double len;                // length

    int validcount; // used to keep track of visiting
};

using dseglink_t = DLListItem<dynaseg_t>;
using dseglist_t = dseglink_t *;

//
// rpolyobj_t
//
// Subsectors now hold pointers to rpolyobj_t's instead of to polyobj_t's.
// An rpolyobj_t is a set of dynasegs belonging to a single polyobject.
// It is necessary to keep dynasegs belonging to different polyobjects
// separate from each other so that the renderer can continue to efficiently
// support multiple polyobjects per subsector (we do not want to do a z-sort
// on every single dynaseg, as that is significant unnecessary overhead).
//
struct rpolyobj_t
{
    DLListItem<rpolyobj_t> link; // for subsector links

    dynaseg_t  *dynaSegs; // list of dynasegs
    polyobj_t  *polyobj;  // polyobject of which this rpolyobj_t is a fragment
    rpolyobj_t *freenext; // next on freelist
};

struct dynavertex_t : vertex_t
{
    struct dynavertex_t *dynanext;
    int                  refcount;
    v2fixed_t            backup;
    v2float_t            fbackup;
};

void R_AddTicDynaSeg(dynaseg_t &seg);
void P_CalcDynaSegLength(dynaseg_t *lseg);

dynavertex_t *R_GetFreeDynaVertex();
void          R_FreeDynaVertex(dynavertex_t **vtx);
void          R_SetDynaVertexRef(dynavertex_t **target, dynavertex_t *vtx);
dynaseg_t    *R_CreateDynaSeg(const dynaseg_t *proto, dynavertex_t *v1, dynavertex_t *v2);
void          R_FreeDynaSeg(dynaseg_t *dseg);

void R_SaveDynasegPositions();

void R_AttachPolyObject(polyobj_t *poly);
void R_DetachPolyObject(polyobj_t *poly);
void R_ClearDynaSegs();

//
// Quick way to calculate previous length
//
inline static float R_calcPrevLen(seg_t &seg)
{
    float dx = seg.dyv2->fbackup.x - seg.dyv1->fbackup.x;
    float dy = seg.dyv2->fbackup.y - seg.dyv1->fbackup.y;
    return sqrtf(dy * dy + dx * dx);
}

#endif

// EOF

