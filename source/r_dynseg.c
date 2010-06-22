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

#include "z_zone.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_dynseg.h"

extern void P_CalcSegLength(seg_t *);

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
// rpolyobj_t freelist
//
static rpolyobj_t *freePolyFragments;

//
// R_AddDynaSubsec
//
// Keeps track of pointers to subsectors which hold dynasegs in a
// reallocating array, for purposes of later detaching the dynasegs.
// Each polyobject contains its own subsector array.
//
static void R_AddDynaSubsec(subsector_t *ss, polyobj_t *po)
{
   int i;

   // make sure subsector is not already tracked
   for(i = 0; i < po->numDSS; ++i)
   {
      if(po->dynaSubsecs[i] == ss)
         return;
   }

   if(po->numDSS >= po->numDSSAlloc)
   {
      po->numDSSAlloc = po->numDSSAlloc ? po->numDSSAlloc * 2 : 8;
      po->dynaSubsecs = 
         (subsector_t **)realloc(po->dynaSubsecs, 
                                 po->numDSSAlloc * sizeof(subsector_t *));
   }
   po->dynaSubsecs[po->numDSS++] = ss;
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
      memset(ret, 0, sizeof(vertex_t));
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
   vtx->dynafree = true;
   vtx->dynanext = dynaVertexFreeList;
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
      memset(ret, 0, sizeof(dynaseg_t));
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
// R_GetFreeRPolyObj
//
// Gets an rpolyobj_t from the free list or creates a new one.
//
static rpolyobj_t *R_GetFreeRPolyObj(void)
{
   rpolyobj_t *ret = NULL;

   if(freePolyFragments)
   {
      ret = freePolyFragments;
      freePolyFragments = freePolyFragments->freenext;
      memset(ret, 0, sizeof(rpolyobj_t));
   }
   else
      ret = calloc(1, sizeof(rpolyobj_t));

   return ret;
}

//
// R_FreeRPolyObj
//
// Puts an rpolyobj_t on the freelist.
//
static void R_FreeRPolyObj(rpolyobj_t *rpo)
{
   rpo->freenext = freePolyFragments;
   freePolyFragments = rpo;
}

//
// R_FindFragment
//
// Looks in the given subsector for a polyobject fragment corresponding
// to the given polyobject. If one is not found, then a new one is created
// and returned.
//
static rpolyobj_t *R_FindFragment(subsector_t *ss, polyobj_t *po)
{
   rpolyobj_t *rpo = ss->polyList;

   while(rpo)
   {
      if(rpo->polyobj == po)
         return rpo;

      rpo = (rpolyobj_t *)(rpo->link.next);
   }
   
   // there is not one, so create a new one and link it in
   rpo = R_GetFreeRPolyObj();

   rpo->polyobj = po;

   M_DLListInsert((mdllistitem_t *)rpo, (mdllistitem_t **)&ss->polyList);

   return rpo;
}

//
// R_DynaSegOffset
//
// Computes the offset value of the seg relative to its parent linedef.
// Not terribly fast.
// Derived from BSP 5.2 SplitDist routine.
//
// haleyjd 06/14/10: made global for map loading in p_setup.c and added
//                   side parameter.
//
void R_DynaSegOffset(seg_t *seg, line_t *line, int side)
{
   double t;
   double dx = (side ? line->v2->fx : line->v1->fx) - seg->v1->fx;
   double dy = (side ? line->v2->fy : line->v1->fy) - seg->v1->fy;
 
   if(dx == 0.0 && dy == 0.0)
      t = 0;
   else
      t = sqrt((dx * dx) + (dy * dy));

   seg->offset = (float)t;
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
   ret->polyobj     = proto->polyobj;
   ret->seg.linedef = proto->seg.linedef;
   ret->seg.sidedef = proto->seg.sidedef;

   // vertices
   ret->seg.v1      = v1;
   ret->seg.v2      = v2;

   // calculate texture offset
   R_DynaSegOffset(&ret->seg, proto->seg.linedef, 0);

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
   
   // haleyjd 05/13/09: massive optimization
   double a2 = -bsp->a;
   double b2 = -bsp->b;
   double c2 = -bsp->c;

   double d = a1 * b2 - a2 * b1;

   // lines are parallel?? shouldn't be.
   // FIXME: could this occur due to roundoff error in R_PointOnSide? 
   //        Guess we'll find out the hard way ;)
   //        If so I'll need my own R_PointOnSide routine with some
   //        epsilon values.
   if(d == 0.0) 
      I_Error("R_IntersectPoint: lines are parallel\n");

   *x = (float)((b1 * c2 - b2 * c1) / d);
   *y = (float)((a2 * c1 - a1 * c2) / d);
}

//
// R_PartitionDistance
//
// This routine uses the general line equation, whose coefficients are now
// precalculated in the BSP nodes, to determine the distance of the point
// from the partition line. If the distance is too small, we may decide to
// change our idea of sidedness.
//
d_inline static double R_PartitionDistance(double x, double y, node_t *node)
{
   return fabs((node->a * x + node->b * y + node->c) / node->len);
}

#define DS_EPSILON 0.3125

//
// R_SplitLine
//
// Given a single dynaseg representing the full length of a linedef, generates a
// set of dynasegs by recursively splitting the line through the BSP tree.
//
static void R_SplitLine(dynaseg_t *dseg, int bspnum)
{
   while(!(bspnum & NF_SUBSECTOR))
   {
      node_t *bsp = &nodes[bspnum];
      seg_t  *seg = &dseg->seg;

      // test vertices against node line
      int side_v1 = R_PointOnSide(seg->v1->x, seg->v1->y, bsp);
      int side_v2 = R_PointOnSide(seg->v2->x, seg->v2->y, bsp);

      // get distance of vertices from partition line
      double dist_v1 = R_PartitionDistance(seg->v1->fx, seg->v1->fy, bsp);
      double dist_v2 = R_PartitionDistance(seg->v2->fx, seg->v2->fy, bsp);

      // If the distances are less than epsilon, consider the points as being
      // on the same side as the polyobj origin. Why? People like to build
      // polyobject doors flush with their door tracks. This breaks using the
      // usual assumptions.
      if(dist_v1 <= DS_EPSILON && dist_v2 <= DS_EPSILON)
      {
         // test polyobj origin against node line
         side_v1 = R_PointOnSide(dseg->polyobj->centerPt.x, 
                                 dseg->polyobj->centerPt.y,
                                 bsp);
      }
      else if(side_v1 != side_v2)
      {
         // if the partition line crosses this seg, we must split it.
         dynaseg_t *nds;
         vertex_t  *nv = R_GetFreeDynaVertex();

         R_IntersectPoint(seg, bsp, &nv->fx, &nv->fy);

         // also set fixed-point coordinates
         nv->x = M_FloatToFixed(nv->fx);
         nv->y = M_FloatToFixed(nv->fy);

         // create new dynaseg from nv to seg->v2
         nds = R_CreateDynaSeg(dseg, nv, seg->v2);

         // alter current seg to run from seg->v1 to nv
         seg->v2 = nv;

         // recurse to split v2 side
         R_SplitLine(nds, bsp->children[side_v2]);
      }

      // continue on v1 side
      bspnum = bsp->children[side_v1];
   }

   // reached a subsector: attach dynaseg
   {
      int num;
      rpolyobj_t *fragment;
      
      num = bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR;
      
#ifdef RANGECHECK
      if(num >= numsubsectors)
         I_Error("R_SplitLine: ss %d with numss = %d\n", num, numsubsectors);
#endif

      // see if this subsector already has an rpolyobj_t for this polyobject
      // if it does not, then one will be created.
      fragment = R_FindFragment(&subsectors[num], dseg->polyobj);
      
      // link this seg in at the end of the list in the rpolyobj_t
      if(fragment->dynaSegs)
      {
         dynaseg_t *seg = fragment->dynaSegs;
         
         while(seg->subnext)
            seg = seg->subnext;
         
         seg->subnext = dseg;
      }
      else
         fragment->dynaSegs = dseg;

      // 05/13/09: calculate seg length for SoM
      P_CalcSegLength(&dseg->seg);

      // 07/15/09: rendering consistency - set frontsector/backsector here
      dseg->seg.frontsector = subsectors[num].sector;

      // 10/30/09: only set backsector if line is 2S (it really shouldn't be...)
      if(dseg->seg.linedef->backsector)
         dseg->seg.backsector = subsectors[num].sector;
      else
         dseg->seg.backsector = NULL;

      // add the subsector if it hasn't been added already
      R_AddDynaSubsec(&subsectors[num], dseg->polyobj);
   }
}

//
// R_AttachPolyObject
//
// Generates dynamic segs for a single polyobject.
//
void R_AttachPolyObject(polyobj_t *poly)
{
   int i;

   // never attach a bad polyobject
   if(poly->flags & POF_ISBAD)
      return;

   // already attached?
   if(poly->flags & POF_ATTACHED) 
      return;

   // iterate on the polyobject lines array
   for(i = 0; i < poly->numLines; ++i)
   {
      line_t *line = poly->lines[i];

      // create initial dseg representing the entire linedef
      dynaseg_t *idseg = R_GetFreeDynaSeg();

      idseg->polyobj     = poly;
      idseg->seg.linedef = line;
      idseg->seg.sidedef = &sides[line->sidenum[0]];
      idseg->seg.v1      = R_GetFreeDynaVertex();
      idseg->seg.v2      = R_GetFreeDynaVertex();

      *(idseg->seg.v1) = *(line->v1);
      *(idseg->seg.v2) = *(line->v2);

      // Split seg into BSP tree to generate more dynasegs;
      // The dynasegs are stored in the subsectors in which they finally end up.
      R_SplitLine(idseg, numnodes - 1);
   }

   poly->flags |= POF_ATTACHED;
}

//
// R_DetachPolyObject
//
// Removes a polyobject from all subsectors to which it is attached, reclaiming
// all dynasegs, vertices, and rpolyobj_t fragment objects associated with the
// given polyobject.
//
void R_DetachPolyObject(polyobj_t *poly)
{
   int i;

   // a bad polyobject should never have been attached in the first place
   if(poly->flags & POF_ISBAD)
      return;

   // not attached?
   if(!(poly->flags & POF_ATTACHED))
      return;
   
   // no dynaseg-containing subsecs?
   if(!poly->dynaSubsecs || !poly->numDSS)
      return;

   // iterate over stored subsector pointers
   for(i = 0; i < poly->numDSS; ++i)
   {
      subsector_t *ss = poly->dynaSubsecs[i];
      rpolyobj_t *rpo = ss->polyList;
      rpolyobj_t *next;
      
      // iterate on subsector rpolyobj_t lists
      while(rpo)
      {
         next = (rpolyobj_t *)(rpo->link.next);

         if(rpo->polyobj == poly)
         {
            // iterate on segs in rpolyobj_t
            while(rpo->dynaSegs)
            {
               dynaseg_t *ds     = rpo->dynaSegs;
               dynaseg_t *nextds = ds->subnext;
               
               vertex_t *v1 = ds->seg.v1;
               vertex_t *v2 = ds->seg.v2;
               
               // put dynamic vertices on free list if they haven't already been
               // put there by another seg
               if(!v1->dynafree)
                  R_FreeDynaVertex(v1);
               if(!v2->dynafree)
                  R_FreeDynaVertex(v2);
               
               // put this dynaseg on the free list
               R_FreeDynaSeg(ds);
               
               rpo->dynaSegs = nextds;
            }
            
            // unlink this rpolyobj_t
            M_DLListRemove((mdllistitem_t *)rpo);
            
            // put it on the freelist
            R_FreeRPolyObj(rpo);
         }

         rpo = next;
      }

      // no longer tracking this subsector
      poly->dynaSubsecs[i] = NULL;
   }

   // no longer tracking any subsectors
   poly->numDSS = 0;   
   poly->flags &= ~POF_ATTACHED;
}

//
// R_ClearDynaSegs
//
// Call at the end of a level to clear all dynasegs.
//
// If this were not done, all dynasegs, their vertices, and polyobj fragments
// would be lost.
//
void R_ClearDynaSegs(void)
{
   int i;

   for(i = 0; i < numPolyObjects; ++i)
      R_DetachPolyObject(&PolyObjects[i]);
}

// EOF

