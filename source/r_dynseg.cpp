// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
//      Dynamic segs for PolyObject re-implementation.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "m_bbox.h"
#include "m_collection.h"
#include "p_maputl.h"
#include "r_main.h"
#include "r_dynseg.h"
#include "r_dynabsp.h"
#include "r_state.h"

//
// dynaseg free list
//
// Let's do as little allocation as possible.
//
static dynaseg_t *dynaSegFreeList;

//
// dynaseg vertex free list
//
static dynavertex_t *dynaVertexFreeList;

//
// rpolyobj_t freelist
//
static rpolyobj_t *freePolyFragments;

//
// Dynavertices added this tic.
//
static PODCollection<dynavertex_t *> gTicDynavertices;
static PODCollection<dynaseg_t *> gTicDynasegs;

//
// External interface
//
void R_AddTicDynaSeg(dynaseg_t &seg)
{
   gTicDynasegs.add(&seg);
}

//
// Used for dynasegs, not base segs
//
void P_CalcDynaSegLength(dynaseg_t *dynaseg)
{
   seg_t *lseg = &dynaseg->seg;
   float dx = lseg->v2->fx - lseg->v1->fx;
   float dy = lseg->v2->fy - lseg->v1->fy;
   lseg->len = sqrtf(dx * dx + dy * dy);

   dynaseg->prevlen = R_calcPrevLen(dynaseg->seg);

   if(lseg->len != dynaseg->prevlen)
      R_AddTicDynaSeg(*dynaseg);
}

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

   // If the subsector has a BSP tree, it will need to be rebuilt.
   if(ss->bsp)
      ss->bsp->dirty = true;

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
         erealloc(subsector_t **, po->dynaSubsecs, 
                  po->numDSSAlloc * sizeof(subsector_t *));
   }
   po->dynaSubsecs[po->numDSS++] = ss;
}

//
// R_GetFreeDynaVertex
//
// Gets a vertex from the free list or allocates a new one.
//
dynavertex_t *R_GetFreeDynaVertex()
{
   dynavertex_t *ret = NULL;

   if(dynaVertexFreeList)
   {
      ret = dynaVertexFreeList;
      dynaVertexFreeList = dynaVertexFreeList->dynanext;
      memset(ret, 0, sizeof(dynavertex_t));
   }
   else
      ret = estructalloc(dynavertex_t, 1);

   gTicDynavertices.add(ret);
   return ret;
}

//
// R_FreeDynaVertex
//
// Puts a dynamic vertex onto the free list, if its refcount becomes zero.
//
void R_FreeDynaVertex(dynavertex_t **vtx)
{
   if(!*vtx)
      return;

   dynavertex_t *v = *vtx;

   if(v->refcount > 0)
   {
      v->refcount--;
      if(v->refcount == 0)
      {
         v->refcount = -1;
         v->dynanext = dynaVertexFreeList;
         dynaVertexFreeList = v;
      }
   }

   *vtx = NULL;
}

//
// Stores the positions of the relevant dynavertices at the beginning of tic.
// Called at the same moment sector heights are backed up.
//
void R_SaveDynasegPositions()
{
   for(dynavertex_t *vertex : gTicDynavertices)
   {
      vertex->backup.x = vertex->x;
      vertex->backup.y = vertex->y;
      vertex->fbackup.x = vertex->fx;
      vertex->fbackup.y = vertex->fy;
   }
   gTicDynavertices.makeEmpty();

   for(dynaseg_t *seg : gTicDynasegs)
   {
      seg->prevlen = seg->seg.len;
      seg->prevofs = seg->seg.offset;
   }
   gTicDynasegs.makeEmpty();
}

//
// R_SetDynaVertexRef
//
// Safely set a reference to a dynamic vertex, maintaining the reference count.
// Do not assign dynavertex pointers without using this routine! Note that if
// *target already points to a vertex, that vertex WILL be freed if its ref
// count reaches zero.
//
void R_SetDynaVertexRef(dynavertex_t **target, dynavertex_t *vtx)
{
   if(*target)
      R_FreeDynaVertex(target);

   if((*target = vtx))
      (*target)->refcount++;
}

//
// R_GetFreeDynaSeg
//
// Gets a dynaseg from the free list or allocates a new one.
//
static dynaseg_t *R_GetFreeDynaSeg()
{
   dynaseg_t *ret = NULL;

   if(dynaSegFreeList)
   {
      ret = dynaSegFreeList;
      dynaSegFreeList = dynaSegFreeList->freenext;
      memset(ret, 0, sizeof(dynaseg_t));
   }
   else
      ret = estructalloc(dynaseg_t, 1);

   return ret;
}

//
// R_FreeDynaSeg
//
// Puts a dynaseg onto the free list.
//
void R_FreeDynaSeg(dynaseg_t *dseg)
{
   R_FreeDynaVertex(&dseg->seg.dyv1);
   R_FreeDynaVertex(&dseg->seg.dyv2);
   R_FreeDynaVertex(&dseg->originalv2);
   R_FreeDynaVertex(&dseg->linev1);
   R_FreeDynaVertex(&dseg->linev2);

   dseg->alterlink.remove();  // remove it from alterable list
   dseg->freenext = dynaSegFreeList;
   dynaSegFreeList = dseg;
}

//
// R_GetFreeRPolyObj
//
// Gets an rpolyobj_t from the free list or creates a new one.
//
static rpolyobj_t *R_GetFreeRPolyObj()
{
   rpolyobj_t *ret = NULL;

   if(freePolyFragments)
   {
      ret = freePolyFragments;
      freePolyFragments = freePolyFragments->freenext;
      memset(ret, 0, sizeof(rpolyobj_t));
   }
   else
      ret = estructalloc(rpolyobj_t, 1);

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
   DLListItem<rpolyobj_t> *link = ss->polyList;
   rpolyobj_t *rpo;

   while(link)
   {
      if((*link)->polyobj == po)
         return *link;

      link = link->dllNext;
   }
   
   // there is not one, so create a new one and link it in
   rpo = R_GetFreeRPolyObj();

   rpo->polyobj = po;

   rpo->link.insert(rpo, &ss->polyList);

   return rpo;
}

//
// Calculates dynaseg offset using the originating seg's dynavertices.
//
static void R_calcDynaSegOffset(dynaseg_t &dynaseg, int side)
{
   float dx = (side ? dynaseg.linev2->fx : dynaseg.linev1->fx) - dynaseg.seg.v1->fx;
   float dy = (side ? dynaseg.linev2->fy : dynaseg.linev1->fy) - dynaseg.seg.v1->fy;
   dynaseg.seg.offset = sqrtf(dx * dx + dy * dy);

   dx = (side ? dynaseg.linev2->fbackup.x : dynaseg.linev1->fbackup.x) - dynaseg.seg.dyv1->fbackup.x;
   dy = (side ? dynaseg.linev2->fbackup.y : dynaseg.linev1->fbackup.y) - dynaseg.seg.dyv1->fbackup.y;
   dynaseg.prevofs = sqrtf(dx * dx + dy * dy);

   if(dynaseg.seg.offset != dynaseg.prevofs)
      R_AddTicDynaSeg(dynaseg);
}

//
// R_CreateDynaSeg
//
// Gets a new dynaseg and initializes it with all needed information.
//
dynaseg_t *R_CreateDynaSeg(const dynaseg_t *proto, dynavertex_t *v1, dynavertex_t *v2)
{
   dynaseg_t *ret = R_GetFreeDynaSeg();

   // properties inherited from prototype seg
   ret->polyobj     = proto->polyobj;
   ret->seg.linedef = proto->seg.linedef;
   ret->seg.sidedef = proto->seg.sidedef;
   ret->backside = proto->backside;

   R_SetDynaVertexRef(&ret->linev1, proto->linev1);
   R_SetDynaVertexRef(&ret->linev2, proto->linev2);

   // vertices
   R_SetDynaVertexRef(&ret->seg.dyv1, v1);
   R_SetDynaVertexRef(&ret->seg.dyv2, v2);

   // calculate texture offset
   R_calcDynaSegOffset(*ret, ret->backside ? 1 : 0);

   return ret;
}

//
// R_IntersectPoint
//
// Finds the point where a node line crosses a seg.
//
static bool R_IntersectPoint(const seg_t *lseg, const node_t *node, dynavertex_t &nv)
{
   // get the fnode for the node
   fnode_t *bsp = &fnodes[node - nodes];

   double a1 = lseg->v2->fy - lseg->v1->fy;
   double b1 = lseg->v1->fx - lseg->v2->fx;
   double c1 = lseg->v2->fx * lseg->v1->fy - lseg->v1->fx * lseg->v2->fy;

   v2float_t fbackup[2] = 
   { 
      lseg->dyv1->fbackup, 
      lseg->dyv2->fbackup 
   };
   double ba1 = fbackup[1].y - fbackup[0].y;
   double bb1 = fbackup[0].x - fbackup[1].x;
   double bc1 = fbackup[1].x * fbackup[0].y - fbackup[0].x * fbackup[1].y;
   
   // haleyjd 05/13/09: massive optimization
   double a2 = -bsp->a;
   double b2 = -bsp->b;
   double c2 = -bsp->c;

   double d = a1 * b2 - a2 * b1;
   double bd = ba1 * b2 - a2 * bb1;

   // lines are parallel?? shouldn't be.
   // FIXME: could this occur due to roundoff error in R_PointOnSide? 
   //        Guess we'll find out the hard way ;)
   //        If so I'll need my own R_PointOnSide routine with some
   //        epsilon values.
   if(d == 0.0) 
      return false;

   nv.fx = static_cast<float>((b1 * c2 - b2 * c1) / d);
   nv.fy = static_cast<float>((a2 * c1 - a1 * c2) / d);
   nv.fbackup.x = static_cast<float>((bb1 * c2 - b2 * bc1) / bd);
   nv.fbackup.y = static_cast<float>((a2 * bc1 - ba1 * c2) / bd);
   // also set fixed-point coordinates
   nv.x = M_FloatToFixed(nv.fx);
   nv.y = M_FloatToFixed(nv.fy);
   nv.backup.x = M_FloatToFixed(nv.fbackup.x);
   nv.backup.y = M_FloatToFixed(nv.fbackup.y);

   return true;
}

//
// R_PartitionDistance
//
// This routine uses the general line equation, whose coefficients are now
// precalculated in the BSP nodes, to determine the distance of the point
// from the partition line. If the distance is too small, we may decide to
// change our idea of sidedness.
//
inline static double R_PartitionDistance(double x, double y, const fnode_t *node)
{
   return fabs((node->a * x + node->b * y + node->c) / node->len);
}

#define DS_EPSILON 0.3125

//
// Checks if seg is on top of a partition line
//
static bool R_segIsOnPartition(const seg_t &seg, const subsector_t &frontss)
{
   if(seg.backsector)
      return true;
   const line_t &line = *seg.linedef;
   int sign = line.frontsector == seg.frontsector ? 1 : -1;
   v2float_t midp = {
      static_cast<float>((seg.v1->fx + seg.v2->fx) / 2 - line.nx * DS_EPSILON * sign),
      static_cast<float>((seg.v1->fy + seg.v2->fy) / 2 - line.ny * DS_EPSILON * sign)
   };

   return R_PointInSubsector(M_FloatToFixed(midp.x), M_FloatToFixed(midp.y)) != &frontss;
}

//
// Checks the subsector for any wall segs which should cut or totally remove dseg.
// Necessary to avoid polyobject bleeding. Returns true if entire dynaseg is gone.
//
static bool R_cutByWallSegs(dynaseg_t &dseg, dynaseg_t *backdseg, const subsector_t &ss)
{
   // The dynaseg must be in front of all wall segs. Otherwise, it's considered
   // hidden behind walls.
   seg_t &lseg = dseg.seg;
   dseg.psx = dseg.seg.v1->fx;
   dseg.psy = dseg.seg.v1->fy;
   dseg.pex = dseg.seg.v2->fx;
   dseg.pey = dseg.seg.v2->fy;

   // Fast access to delta x, delta y
   dseg.pdx = dseg.pex - dseg.psx;
   dseg.pdy = dseg.pey - dseg.psy;
   for(int i = 0; i < ss.numlines; ++i)
   {
      const seg_t &wall = segs[ss.firstline + i];
      if(R_segIsOnPartition(wall, ss))
         continue;   // only check 1-sided lines
      const vertex_t &v1 = *wall.v1;
      const vertex_t &v2 = *wall.v2;
      const divline_t walldl = { v1.x, v1.y, v2.x - v1.x, v2.y - v1.y };
      int side_v1 = P_PointOnDivlineSide(lseg.v1->x, lseg.v1->y, &walldl);
      int side_v2 = P_PointOnDivlineSide(lseg.v2->x, lseg.v2->y, &walldl);
      if(side_v1 == 0 && side_v2 == 0)
         continue;   // this one is fine.
      if(side_v1 == 1 && side_v2 == 1)
         return true;  // totally occluded by one
      // We have a real intersection: cut it now.
      dynaseg_t part;   // this shall be the wall
      part.psx = wall.v1->fx;
      part.psy = wall.v1->fy;
      part.pex = wall.v2->fx;
      part.pey = wall.v2->fy;
      double vx, vy;
      v2float_t backup;
      R_ComputeIntersection(&part, &dseg, vx, vy, &backup);
      dynavertex_t *nv = R_GetFreeDynaVertex();
      nv->fx = static_cast<float>(vx);
      nv->fy = static_cast<float>(vy);
      nv->fbackup = backup;
      nv->x = M_DoubleToFixed(vx);
      nv->y = M_DoubleToFixed(vy);
      nv->backup.x = M_FloatToFixed(nv->fbackup.x);
      nv->backup.y = M_FloatToFixed(nv->fbackup.y);
      if(side_v1 == 0)
      {
         R_SetDynaVertexRef(&lseg.dyv2, nv);
         if(backdseg)
         {
            R_SetDynaVertexRef(&backdseg->seg.dyv1, nv);
            R_calcDynaSegOffset(*backdseg, 1);
         }
      }
      else
      {
         R_SetDynaVertexRef(&lseg.dyv1, nv);
         R_calcDynaSegOffset(dseg, 0); // also need to update this
         if(backdseg)
            R_SetDynaVertexRef(&backdseg->seg.dyv2, nv);
      }
      // Keep looking for other intersectors
   }
   return false;   // all are in front. So return.
}

//
// R_SplitLine
//
// Given a single dynaseg representing the full length of a linedef, generates a
// set of dynasegs by recursively splitting the line through the BSP tree.
// Also does the same for a back dynaseg for 2-sided lines.
//
static void R_SplitLine(dynaseg_t *dseg, dynaseg_t *backdseg, int bspnum)
{
   while(!(bspnum & NF_SUBSECTOR))
   {
      node_t  *bsp   = &nodes[bspnum];
      const fnode_t *fnode = &fnodes[bspnum];
      seg_t   *lseg  = &dseg->seg;

      // test vertices against node line
      int side_v1 = R_PointOnSide(lseg->v1->x, lseg->v1->y, bsp);
      int side_v2 = R_PointOnSide(lseg->v2->x, lseg->v2->y, bsp);

      // ioanch 20160226: fix the polyobject visual clipping bug
      M_AddToBox(bsp->bbox[side_v1], lseg->v1->x, lseg->v1->y);
      M_AddToBox(bsp->bbox[side_v2], lseg->v2->x, lseg->v2->y);

      // get distance of vertices from partition line
      double dist_v1 = R_PartitionDistance(lseg->v1->fx, lseg->v1->fy, fnode);
      double dist_v2 = R_PartitionDistance(lseg->v2->fx, lseg->v2->fy, fnode);

      // If the distances are less than epsilon, consider the points as being
      // on the same side as the polyobj origin. Why? People like to build
      // polyobject doors flush with their door tracks. This breaks using the
      // usual assumptions.

      if(dist_v1 <= DS_EPSILON)
      {
         if(dist_v2 <= DS_EPSILON)
         {
            // both vertices are within epsilon distance; classify the seg
            // with respect to the polyobject center point
            side_v1 = side_v2 = R_PointOnSide(dseg->polyobj->centerPt.x,
                                              dseg->polyobj->centerPt.y, 
                                              bsp);
         }
         else
            side_v1 = side_v2; // v1 is very close; classify as v2 side
      }
      else if(dist_v2 <= DS_EPSILON)
      {         
         side_v2 = side_v1; // v2 is very close; classify as v1 side
      }

      if(side_v1 != side_v2)
      {
         // the partition line crosses this seg, so we must split it.
         dynaseg_t *nds;
         dynavertex_t  *nv = R_GetFreeDynaVertex();

         if(R_IntersectPoint(lseg, bsp, *nv))
         {
            // ioanch 20160722: fix the polyobject visual clipping bug (more needed)
            M_AddToBox(bsp->bbox[0], nv->x, nv->y);
            M_AddToBox(bsp->bbox[1], nv->x, nv->y);

            // create new dynaseg from nv to seg->v2
            nds = R_CreateDynaSeg(dseg, nv, lseg->dyv2);

            // alter current seg to run from seg->v1 to nv
            R_SetDynaVertexRef(&lseg->dyv2, nv);

            dynaseg_t *backnds;
            if(backdseg)
            {
               backnds = R_CreateDynaSeg(backdseg, backdseg->seg.dyv1, nv);
               R_SetDynaVertexRef(&backdseg->seg.dyv1, nv);
               R_calcDynaSegOffset(*backdseg, 1);
            }
            else
               backnds = nullptr;

            // recurse to split v2 side
            R_SplitLine(nds, backnds, bsp->children[side_v2]);
         }
         else
         {
            // Classification failed (this really should not happen, but, math
            // on computers is not ideal...). Return the dynavertex and do
            // nothing here; the seg will be classified on v1 side for lack of
            // anything better to do with it.
            R_FreeDynaVertex(&nv);
         }
      }

      // continue on v1 side
      bspnum = bsp->children[side_v1];
   }

   // reached a subsector: attach dynaseg
   int num;
   rpolyobj_t *fragment;
      
   num = bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR;
      
#ifdef RANGECHECK
   if(num >= numsubsectors)
      I_Error("R_SplitLine: ss %d with numss = %d\n", num, numsubsectors);
#endif

   // First, cut it off by any wall segs
   if(R_cutByWallSegs(*dseg, backdseg, subsectors[num]))
   {
      // If it's occluded by everything, cancel it.
      R_FreeDynaSeg(dseg);
      if(backdseg)
         R_FreeDynaSeg(backdseg);
      return;
   }

   // see if this subsector already has an rpolyobj_t for this polyobject
   // if it does not, then one will be created.
   fragment = R_FindFragment(&subsectors[num], dseg->polyobj);
      
   // link this seg in at the end of the list in the rpolyobj_t
   if(fragment->dynaSegs)
   {
      dynaseg_t *fdseg = fragment->dynaSegs;
         
      while(fdseg->subnext)
         fdseg = fdseg->subnext;
        
      fdseg->subnext = dseg;
   }
   else
      fragment->dynaSegs = dseg;
   dseg->subnext = backdseg;

   // 05/13/09: calculate seg length for SoM
   P_CalcDynaSegLength(dseg);
   if(backdseg)
   {
      backdseg->seg.len = dseg->seg.len;
      backdseg->prevlen = dseg->prevlen;
      if(backdseg->prevlen != backdseg->seg.len)
         R_AddTicDynaSeg(*backdseg);
   }

   // 07/15/09: rendering consistency - set frontsector/backsector here
   dseg->seg.frontsector = subsectors[num].sector;

   // 10/30/09: only set backsector if line is 2S
   if(dseg->seg.linedef->backsector)
      dseg->seg.backsector = subsectors[num].sector;
   else
      dseg->seg.backsector = NULL;

   if(backdseg)
      backdseg->seg.frontsector = backdseg->seg.backsector = subsectors[num].sector;

   // add the subsector if it hasn't been added already
   R_AddDynaSubsec(&subsectors[num], dseg->polyobj);
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
      
      dynavertex_t *v1 = R_GetFreeDynaVertex();
      dynavertex_t *v2 = R_GetFreeDynaVertex();

      // NOTE: currently polyobjects with portals won't be interpolated. We need
      // to implement portal transform interpolation first.
      *static_cast<vertex_t *>(v1) = *line->v1;
      if(poly->numPortals)
      {
         v1->backup.x = v1->x;
         v1->backup.y = v1->y;
         v1->fbackup.x = v1->fx;
         v1->fbackup.y = v1->fy;
      }
      else
      {
         v1->backup.x = poly->tmpVerts[line->v1->polyindex].x;
         v1->backup.y = poly->tmpVerts[line->v1->polyindex].y;
         v1->fbackup.x = poly->tmpVerts[line->v1->polyindex].fx;
         v1->fbackup.y = poly->tmpVerts[line->v1->polyindex].fy;
      }
      *static_cast<vertex_t *>(v2) = *line->v2;
      if(poly->numPortals)
      {
         v2->backup.x = v2->x;
         v2->backup.y = v2->y;
         v2->fbackup.x = v2->fx;
         v2->fbackup.y = v2->fy;
      }
      else
      {
         v2->backup.x = poly->tmpVerts[line->v2->polyindex].x;
         v2->backup.y = poly->tmpVerts[line->v2->polyindex].y;
         v2->fbackup.x = poly->tmpVerts[line->v2->polyindex].fx;
         v2->fbackup.y = poly->tmpVerts[line->v2->polyindex].fy;
      }

      R_SetDynaVertexRef(&idseg->seg.dyv1, v1);
      R_SetDynaVertexRef(&idseg->seg.dyv2, v2);
      R_SetDynaVertexRef(&idseg->linev1, v1);
      R_SetDynaVertexRef(&idseg->linev2, v2);

      dynaseg_t *backdseg;
      // Make sure not to render portal line backsides if they were generated
      // 1-sided lines turned into 2-sided.
      if(line->sidenum[1] != -1 && (!(line->intflags & MLI_1SPORTALLINE) ||
                                    line->sidenum[1] != line->sidenum[0]))
      {
         // create backside dynaseg now
         backdseg = R_GetFreeDynaSeg();
         backdseg->backside = true;
         backdseg->polyobj = poly;
         backdseg->seg.linedef = line;
         backdseg->seg.sidedef = &sides[line->sidenum[1]];
         R_SetDynaVertexRef(&backdseg->seg.dyv1, v2);
         R_SetDynaVertexRef(&backdseg->seg.dyv2, v1);
         R_SetDynaVertexRef(&backdseg->linev1, v2);
         R_SetDynaVertexRef(&backdseg->linev2, v1);
      }
      else
         backdseg = nullptr;

      // Split seg into BSP tree to generate more dynasegs;
      // The dynasegs are stored in the subsectors in which they finally end up.
      R_SplitLine(idseg, backdseg, numnodes - 1);
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
   {
      poly->flags &= ~POF_ATTACHED;
      return;
   }

   // iterate over stored subsector pointers
   for(i = 0; i < poly->numDSS; ++i)
   {
      subsector_t *ss = poly->dynaSubsecs[i];
      DLListItem<rpolyobj_t> *link = ss->polyList;
      DLListItem<rpolyobj_t> *next;

      // mark BSPs dirty
      if(ss->bsp)
         ss->bsp->dirty = true;
      
      // iterate on subsector rpolyobj_t lists
      while(link)
      {
         next = link->dllNext;
         rpolyobj_t *rpo = *link;

         if(rpo->polyobj == poly)
         {
            // iterate on segs in rpolyobj_t
            while(rpo->dynaSegs)
            {
               dynaseg_t *ds     = rpo->dynaSegs;
               dynaseg_t *nextds = ds->subnext;
               
               // free dynamic vertices
               // put this dynaseg on the free list
               R_FreeDynaSeg(ds);
               
               rpo->dynaSegs = nextds;
            }
            
            // unlink this rpolyobj_t
            link->remove();
            
            // put it on the freelist
            R_FreeRPolyObj(rpo);
         }

         link = next;
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
void R_ClearDynaSegs()
{
   int i;

   gTicDynasegs.clear();
   gTicDynavertices.clear();

   for(i = 0; i < numPolyObjects; i++)
      R_DetachPolyObject(&PolyObjects[i]);

   for(i = 0; i < numsubsectors; i++)
   {
      if(subsectors[i].bsp)
         R_FreeDynaBSP(subsectors[i].bsp);
   }
}

// EOF

