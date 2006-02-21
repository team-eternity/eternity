// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2006 James Haley
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
//----------------------------------------------------------------------------
//
// Polyobjects
//
// Movable segs like in Hexen, but more flexible due to application of
// dynamic binary space partitioning theory.
//
// haleyjd 02/16/06
//
//----------------------------------------------------------------------------

#ifndef POLYOBJ_H__
#define POLYOBJ_H__

// haleyjd: temporary define
#ifdef POLYOBJECTS

#include "m_dllist.h"
#include "p_mobj.h"
#include "r_defs.h"

//
// Defines
//

// haleyjd: use zdoom-compatible doomednums

#define POLYOBJ_ANCHOR_DOOMEDNUM     9300
#define POLYOBJ_SPAWN_DOOMEDNUM      9301
#define POLYOBJ_SPAWNCRUSH_DOOMEDNUM 9302

#define POLYOBJ_START_LINE    348
#define POLYOBJ_EXPLICIT_LINE 349

//
// Polyobject Structure
//

typedef struct polyobj_s
{
   mdllistitem_t link; // for subsector links

   int id;    // numeric id
   int first; // for hashing: index of first polyobject in this hash chain
   int next;  // for hashing: next polyobject in this hash chain

   int segCount;        // number of segs in polyobject
   int numSegsAlloc;    // number of segs allocated
   struct seg_s **segs; // the segs, a reallocating array.

   int numVertices;            // number of vertices (generally == segCount)
   int numVerticesAlloc;       // number of vertices allocated
   struct vertex_s *origVerts; // original positions relative to spawn spot
   struct vertex_s **vertices; // vertices this polyobject must move   
   
   int numLines;          // number of linedefs (generally <= segCount)
   int numLinesAlloc;     // number of linedefs allocated
   struct line_s **lines; // linedefs this polyobject must move

   struct degenmobj_s spawnSpot; // location of spawn spot
   struct vertex_s    centerPt;  // center point
   boolean     hasMoved;  // if true, need to recalculate center pt
   boolean     attached;  // if true, is attached to a subsector

   boolean isBad; // a bad polyobject: should not be rendered
} polyobj_t;

//
// Functions
//

polyobj_t *Polyobj_GetForNum(int id);
void Polyobj_InitLevel(void);
void Polyobj_Ticker(void);

#endif // ifdef POLYOBJECTS

#endif

// EOF

