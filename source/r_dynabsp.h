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
// Purpose: Dynamic BSP sub-trees for dynaseg sorting.
// Authors: James Haley, Ioan Chera
//

#ifndef R_DYNABSP_H__
#define R_DYNABSP_H__

#include "r_dynseg.h"

struct rpolynode_t
{
    dynaseg_t   *partition;   // partition dynaseg
    rpolynode_t *children[2]; // child node lists (0=right, 1=left)
    dseglink_t  *owned;       // owned segs created by partition splits
    dseglink_t  *altered;     // polyobject-owned segs altered by partitions.
};

struct rpolybsp_t
{
    bool         dirty; // needs to be rebuilt if true
    rpolynode_t *root;  // root of tree
};

rpolybsp_t *R_BuildDynaBSP(const subsector_t *subsec);
void        R_FreeDynaBSP(rpolybsp_t *bsp);

//
// R_PointOnDynaSegSide
//
// Returns 0 for front/right, 1 for back/left.
//
inline static int R_PointOnDynaSegSide(const dynaseg_t *ds, float x, float y)
{
    return ((ds->pdx * (y - ds->psy)) >= (ds->pdy * (x - ds->psx)));
}

void R_ComputeIntersection(const dynaseg_t *part, const dynaseg_t *seg, double &outx, double &outy,
                           v2float_t *fbackup = nullptr);

#endif

// EOF

