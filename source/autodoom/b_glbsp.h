// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 Ioan Chera
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      C interface with GLBSP module
//
//-----------------------------------------------------------------------------

#ifndef __EternityEngine__b_glbsp__
#define __EternityEngine__b_glbsp__

#ifdef __cplusplus
extern "C"
{
#endif
   
	void B_GLBSP_Start(void *cacheStreamPtr);

   int B_GLBSP_GetNextVertex(double *coordx, double *coordy);
   
   int B_GLBSP_GetNextSector(int *floor_h, int *ceil_h);
   
   int B_GLBSP_GetNextSidedef(int *index);
   
   int B_GLBSP_GetNextLinedef(int *startIdx, int *endIdx, int *rightIdx,
                              int *leftIdx);
   
   void B_GLBSP_CreateVertexArray(int numverts);
   void B_GLBSP_PutVertex(double coordx, double coordy, int index);
   
   void B_GLBSP_CreateLineArray(int numlines);
   void B_GLBSP_PutLine(int v1idx, int v2idx, int s1idx, int s2idx, int lnidx);
   
   void B_GLBSP_CreateSegArray(int numsegs);
   void B_GLBSP_PutSegment(int v1idx, int v2idx, int back, int lnidx, int part,
                           int sgidx);
   
   void B_GLBSP_CreateSubsectorArray(int numssecs);
   void B_GLBSP_PutSubsector(int first, int num, int ssidx);
   
   void B_GLBSP_CreateNodeArray(int numnodes);
   void B_GLBSP_PutNode(double x, double y, double dx, double dy, int rnode,
                        int lnode, int riss, int liss, int ndidx);
   
#ifdef __cplusplus
}
#endif

#endif /* defined(__EternityEngine__b_glbsp__) */

// EOF

