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
//    Dynamic BSP sub-trees for dynaseg sorting.
//
//-----------------------------------------------------------------------------

#ifndef R_DYNABSP_H__
#define R_DYNABSP_H__

#include "r_dynseg.h"

struct rpolynode_t
{
   dynaseg_t   *partition;   // partition dynaseg
   rpolynode_t *children[2]; // child node lists (0=right, 1=left)
   dseglink_t  *owned;       // owned segs created by partition splits
};

struct rpolybsp_t
{
   bool         dirty; // needs to be rebuilt if true
   rpolynode_t *root;  // root of tree
};

rpolybsp_t *R_BuildDynaBSP(subsector_t *subsec);
void R_FreeDynaBSP(rpolybsp_t *bsp);


//
// R_PointOnDynaSegSide
//
// Returns 0 for front/right, 1 for back/left.
//
static inline int R_PointOnDynaSegSide(dynaseg_t *ds, float x, float y)
{
   return ((ds->pdx * (y - ds->psy)) >= (ds->pdy * (x - ds->psx)));
}

#endif

// EOF

