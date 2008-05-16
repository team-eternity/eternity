// Emacs style mode select   -*- C++ -*-
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

#ifdef R_DYNASEGS

#include "z_zone.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_dynseg.h"

//
// dynaseg free list
//
// Let's do as little allocation as possible.
//
static dynaseg_t *dynaSegFreeList;

//
// dynaseg vertex free list
//
static vertex_t *dynaVertexFreeList;

//
// subsector list
//
// We must keep track of subsectors which have dynasegs added to them
// every frame, for purposes of later releasing them.
//
static subsector_t **dynaSubsecs;
static int numDSS;
static int numDSSAlloc;

//
// R_AddDynaSubsec
//
// Keeps track of pointers to subsectors which hold dynasegs in a
// reallocating array for purposes of detaching the dynasegs at the beginning
// of a new frame, or at the end of a level.
//
void R_AddDynaSubsec(subsector_t *ss)
{
   if(numDSS >= numDSSAlloc)
   {
      numDSSAlloc = numDSSAlloc ? numDSSAlloc * 2 : 32;
      dynaSubsecs = 
         (subsector_t **)realloc(dynaSubsecs, 
                                 numDSSAlloc * sizeof(subsector_t *));
   }
   dynaSubsecs[numDSS++] = ss;
}

//
// R_GetFreeDynaVertex
//
// Gets a vertex from the free list or allocates a new one.
//
static vertex_t *R_GetFreeDynaVertex(void)
{
   vertex_t *ret = NULL;

   if(dynaVertexFreeList)
   {
      ret = dynaVertexFreeList;
      dynaVertexFreeList = dynaVertexFreeList->dynanext;
      memset(&ret, 0, sizeof(vertex_t));
   }
   else
      ret = calloc(1, sizeof(vertex_t));

   return ret;
}

//
// R_FreeDynaVertex
//
// Puts a dynamic vertex onto the free list.
//
static void R_FreeDynaVertex(vertex_t *vtx)
{
   vtx->dynanext = dynaVertexFreeList;
   vtx->dynafree = true;
   dynaVertexFreeList = vtx;
}

//
// R_GetFreeDynaSeg
//
// Gets a dynaseg from the free list or allocates a new one.
//
static dynaseg_t *R_GetFreeDynaSeg(void)
{
   dynaseg_t *ret = NULL;

   if(dynaSegFreeList)
   {
      ret = dynaSegFreeList;
      dynaSegFreeList = dynaSegFreeList->freenext;
      memset(&ret, 0, sizeof(dynaseg_t));
   }
   else
      ret = calloc(1, sizeof(dynaseg_t));

   return ret;
}

//
// R_FreeDynaSeg
//
// Puts a dynaseg onto the free list.
//
static void R_FreeDynaSeg(dynaseg_t *seg)
{
   seg->freenext = dynaSegFreeList;
   dynaSegFreeList = seg;
}

//
// R_DynaSegOffset
//
// Computes the offset value of the seg relative to its parent linedef.
// Not terribly fast.
// Derived from BSP 5.2 SplitDist routine.
//
static void R_DynaSegOffset(seg_t *seg, line_t *line)
{
   double t;
   double dx = line->v1->x - seg->v1->x;
   double dy = line->v1->y - seg->v1->y;

   if(dx == 0.0 && dy == 0.0)
      t = 0;
   else
      t = sqrt((dx*dx) + (dy*dy));

   seg->offset = (fixed_t)(t / 65536.0);
}

//
// R_CreateDynaSeg
//
// Gets a new dynaseg and initializes it with all needed information.
//
static dynaseg_t *R_CreateDynaSeg(dynaseg_t *proto, vertex_t *v1, vertex_t *v2)
{
   dynaseg_t *ret = R_GetFreeDynaSeg();

   // properties inherited from prototype seg
   ret->seg.linedef     = proto->seg.linedef;
   ret->seg.frontsector = proto->seg.frontsector;
   ret->seg.backsector  = proto->seg.backsector;
   ret->seg.sidedef     = proto->seg.sidedef;
   ret->seg.v1          = v1;
   ret->seg.v2          = v2;
   ret->seg.angle       = proto->seg.angle;

   R_DynaSegOffset(&proto->seg, proto->seg.linedef);

   return ret;
}

//
// R_IntersectPoint
//
// Finds the point where a node line crosses a seg.
//
static void R_IntersectPoint(seg_t *seg, node_t *bsp, float *x, float *y)
{
   double a1 = seg->v2->fy - seg->v1->fy;
   double b1 = seg->v1->fx - seg->v2->fx;
   double c1 = seg->v2->fx * seg->v1->fy - seg->v1->fx * seg->v2->fy;

   double nx2 = bsp->fx + bsp->fdx
   double ny2 = bsp->fy + bsp->fdy;
   
   double a2 = ny2 - bsp->fy;
   double b2 = bsp->fx - nx2;
   double c2 = nx2 * bsp->fy - bsp->fx * ny2;

   double d = a1 * b2 - a2 * b1;

   // lines are parallel?? shouldn't be.
   // FIXME: could this occur due to roundoff error in R_PointOnSide? 
   //        Guess we'll find out the hard way ;)
   //        If so I'll need my own R_PointOnSide routine.
   if(d == 0.0) 
      I_Error("R_IntersectPoint: lines are parallel\n");

   *x = (b1 * c2 - b2 * c1) / d;
   *y = (a2 * c1 - a1 * c2) / d;
}

//
// R_SplitLine
//
// Given a single dynaseg representing the full length of a linedef, generates a
// set of dynasegs by recursively splitting the line through the BSP tree.
//
static void R_SplitLine(dynaseg_t *dseg, int bspnum)
{
   int num;

   while(!(bspnum & NF_SUBSECTOR))
   {
      node_t *bsp = &nodes[bspnum];
      seg_t  *seg = &dseg->seg;

      // test vertices against node line
      int side_v1 = R_PointOnSide(seg->v1->x, seg->v1->y, bsp);
      int side_v2 = R_PointOnSide(seg->v2->x, seg->v2->y, bsp);

      // does the partition line cross this seg? if so, we must split it.
      if(side_v1 != side_v2)
      {
         dynaseg_t *nds;
         vertex_t  *nv = R_GetFreeDynaVertex();

         R_IntersectPoint(seg, bsp, &nv->fx, &nv->fy);

         // also set fixed-point coordinates
         nv->x = (fixed_t)(nv->fx * 65536.0);
         nv->y = (fixed_t)(nv->fy * 65536.0);

         // create new dynaseg from nv to seg->v2
         nds = R_CreateDynaSeg(dseg, nv, seg->v2);

         // alter current seg to run from seg->v1 to nv
         seg->v2 = nv;

         // recurse to split v2 side
         R_SplitLine(nds, bsp->children[side_v2]);
      }

      // continue on side 1
      bspnum = bsp->children[side_v1];
   }
   
   // reached a subsector: attach dynaseg
   num = bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR;

#ifdef RANGECHECK
   if(num >= numsubsectors)
      I_Error("R_SplitLine: ss %i with numss = %i", num, numsubsectors);
#endif

   // FIXME: need to change to create/manage seg addition to an
   // rpolyobj_t object for purposes of keeping segs z-sorted

#if 0
   // link it in at the end of the list
   if(subsectors[num].dynaSegs)
   {
      dynaseg_t *seg = subsectors[num].dynaSegs;

      while(seg->subnext)
         seg = seg->subnext;

      seg->subnext = dseg;
   }
#endif
}

//
// R_GenPolyObjDynaSegs
//
// Generates dynamic segs for a single polyobject.
//
void R_GenPolyObjDynaSegs(polyobj_t *poly)
{
   int i;

   // bad polyobjects do not generate dynasegs
   if(poly->isBad)
      return;

   // iterate on the polyobject lines array
   for(i = 0; i < poly->numLines; ++i)
   {
      line_t *line = poly->lines[i];

      // create initial dseg representing the entire linedef
      dynaseg_t *idseg = R_GetFreeDynaSeg();

      idseg->polyobj         = poly;
      idseg->seg.linedef     = line;
      idseg->seg.offset      = 0;
      idseg->seg.frontsector = line->frontsector;
      idseg->seg.backsector  = line->backsector;
      idseg->seg.sidedef     = &sides[line->sidenum[0]];
      idseg->seg.v1          = R_GetFreeDynaVertex();
      idseg->seg.v2          = R_GetFreeDynaVertex();

      *(idseg->seg.v1) = *(line->v1);
      *(idseg->seg.v2) = *(line->v2);

      idseg->seg.angle = R_PointToAngle2(idseg->seg.v1->x, idseg->seg.v1->y,
                                         idseg->seg.v2->x, idseg->seg.v2->y);

      // Split seg into BSP tree to generate more dynasegs;
      // The dynasegs are stored in the subsectors in which they finally end up.
      R_SplitLine(idseg, numnodes - 1);
   }
}

//
// R_ClearDynaSegs
//
// Call at the start of a frame to clear all dynasegs from subsectors that have
// them attached. When this is done, all dynasegs and their vertices will be
// placed on their respective freelists, and the dynaSegs list of every
// subsector should be NULL.
//
// This must also be called immediately before freeing a level, or all of the
// dynasegs will be lost, and the dynaseg subsector list will be full of
// dangerous dangling subsector pointers.
//
void R_ClearDynaSegs(void)
{
   int i;
   
   // no dynaseg-containing subsecs?
   if(!dynaSubsecs || !numDSS)
      return;

   // FIXME: need to handle via rpolyobj_t's

   for(i = 0; i < numDSS; ++i)
   {
      subsector_t *ss = dynaSubsecs[i];

      while(ss->dynaSegs)
      {
         dynaseg_t *ds     = ss->dynaSegs;
         dynaseg_t *nextds = ds->subnext;

         vertex_t *v1 = ds->seg.v1;
         vertex_t *v2 = ds->seg.v2;

         // put dynamic vertices on free list if they haven't already been
         // put there by another seg.
         if(!v1->dynafree)
            R_FreeDynaVertex(v1);
         if(!v2->dynafree)
            R_FreeDynaVertex(v2);

         // put this dynaseg on the free list
         R_FreeDynaSeg(ds);

         ss->dynaSegs = nextds;
      }

      // clear this subsector slot
      dynaSubsecs[i] = NULL;
   }

   // we aren't tracking any subsectors now
   numDSS = 0;
}

#endif

// EOF

