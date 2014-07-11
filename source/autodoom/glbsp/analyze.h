//------------------------------------------------------------------------
// ANALYZE : Analyzing level structures
//------------------------------------------------------------------------
//
//  GL-Friendly Node Builder (C) 2000-2007 Andrew Apted
//
//  Based on 'BSP 2.3' by Colin Reed, Lee Killough and others.
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
//-----------------------------------------------------------------------------

#ifndef __GLBSP_ANALYZE_H__
#define __GLBSP_ANALYZE_H__



#include "structs.h"
#include "level.h"

namespace glbsp
{
  namespace analyze
  {
    // detection routines
    void DetectDuplicateVertices();
    void DetectDuplicateSidedefs();
    void DetectPolyobjSectors();
    void DetectOverlappingLines();
    void DetectWindowEffects();

    // pruning routines
    void PruneLinedefs();
    void PruneVertices();
    void PruneSidedefs();
    void PruneSectors();

    // computes the wall tips for all of the vertices
    void CalculateWallTips();

    // return a new vertex (with correct wall_tip info) for the split that
    // happens along the given seg at the given location.
    //
    level::Vertex *NewVertexFromSplitSeg(const level::Seg &seg, float_g x, float_g y);

    // return a new end vertex to compensate for a seg that would end up
    // being zero-length (after integer rounding).  Doesn't compute the
    // wall_tip info (thus this routine should only be used _after_ node
    // building).
    //
    level::Vertex *NewVertexDegenerate(const level::Vertex &start, const level::Vertex &end);

    // check whether a line with the given delta coordinates and beginning
    // at this vertex is open.  Returns a sector reference if it's open,
    // or NULL if closed (void space or directly along a linedef).
    //
    level::Sector * VertexCheckOpen(const level::Vertex &vert, float_g dx, float_g dy);
  }
}






#endif /* __GLBSP_ANALYZE_H__ */
