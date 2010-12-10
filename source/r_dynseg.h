// Emacs style mode select   -*- C -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2008 James Haley
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
//      Dynamic segs for PolyObject re-implementation.
//
//-----------------------------------------------------------------------------

#ifndef R_DYNSEG_H__
#define R_DYNSEG_H__

#include "r_defs.h"
#include "polyobj.h"

//
// dynaseg
//
typedef struct dynaseg_s
{
   seg_t seg; // a dynaseg is a seg, after all ;)

   struct dynaseg_s *subnext;  // next dynaseg in subsector
   struct dynaseg_s *freenext; // next dynaseg on freelist
   struct polyobj_s *polyobj;  // polyobject
} dynaseg_t;

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
   CDLListItem<rpolyobj_t> link;  // for subsector links

   dynaseg_t  *dynaSegs; // list of dynasegs
   polyobj_t  *polyobj;  // polyobject of which this rpolyobj_t is a fragment
   rpolyobj_t *freenext; // next on freelist
};

void R_AttachPolyObject(polyobj_t *poly);
void R_DetachPolyObject(polyobj_t *poly);
void R_ClearDynaSegs(void);

#endif

// EOF

