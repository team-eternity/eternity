// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 James Haley
//
// Portions derived from BSP 5.2, "Node builder for DOOM levels"
//   (c) 1996 Raphael Quinet
//   (c) 1998 Colin Reed, Lee Killough
//   (c) 2001 Simon Howard
//   (c) 2006 Colin Phipps
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

#include "z_zone.h"

#include "r_dynabsp.h"

//=============================================================================
//
// rpolynode Maintenance
//

static rpolynode_t *polyNodeFreeList;

//
// R_GetFreePolyNode
//
// Gets a node from the free list or allocates a new one.
//
static rpolynode_t *R_GetFreePolyNode()
{
   rpolynode_t *ret = NULL;

   if(polyNodeFreeList)
   {
      ret = polyNodeFreeList;
      polyNodeFreeList = polyNodeFreeList->left;
      memset(ret, 0, sizeof(*ret));
   }
   else
      ret = estructalloc(rpolynode_t, 1);

   return ret;
}

//
// R_FreePolyNode
//
// Puts a dynamic node onto the free list.
//
static void R_FreePolyNode(rpolynode_t *rpn)
{
   rpn->left = polyNodeFreeList;
   polyNodeFreeList = rpn;
}

//=============================================================================
//
// Dynaseg Setup
//

// 
// R_setupDSForBSP
//
// Dynasegs need a small amount of preparation in order to achieve maximum
// efficiency during the node building process.
//
static void R_setupDSForBSP(dynaseg_t &ds)
{
   // Fast access to double-precision coordinates
   ds.psx = ds.seg.v1->fx;
   ds.psy = ds.seg.v1->fy;
   ds.pex = ds.seg.v2->fx;
   ds.pey = ds.seg.v2->fy;

   // Fast access to delta x, delta y
   ds.pdx = ds.pex - ds.psx;
   ds.pdy = ds.pey - ds.psy;

   // General line equation coefficient 'd'
   ds.ptmp = ds.pdx * ds.psy - ds.psx * ds.pdy;
   
   // Length - we DEFINITELY don't want to do this any more than necessary
   ds.len = sqrt(ds.pdx * ds.pdx + ds.pdy * ds.pdy);
}

//=============================================================================
//
// Partition Selection
//

// I am not TOO worried about accuracy during partition selection, but...
#define CHECK_EPSILON 0.0001

static inline bool nearzero(double d)
{
   return (d < CHECK_EPSILON && d > -CHECK_EPSILON);
}

// split weighting factor
// I have no idea why "17" is good, but let's stick with it.
#define FACTOR 17

//
// R_selectPartition
//
// This routine decides the best dynaseg to use as a nodeline by attempting to
// minimize the number of splits it would cause in remaining dynasegs.
//
// Credit to Raphael Quinet and DEU.
//
// Rewritten by Lee Killough for significant performance increases.
//  (haleyjd - using gotos, naturally ;)
//
static dynaseg_t *R_selectPartition(DLListItem<dynaseg_t> *segs)
{
   DLListItem<dynaseg_t> *rover;
   dynaseg_t *best;
   dynaseg_t *part;
   int bestcost = INT_MAX;
   int cnt = 0;

   // count the number of segs on the input list
   for(rover = segs; rover; rover = rover->dllNext)
      ++cnt;

   // Try each seg as a partition line
   for(rover = segs; rover; rover = rover->dllNext)
   {
      part = rover->dllObject;
      DLListItem<dynaseg_t> *crover;
      int cost = 0, tot = 0, diff = cnt;

      // Check partition against all segs
      for(crover = segs; crover; crover = crover->dllNext)
      {
         dynaseg_t *check = crover->dllObject;

         // classify both end points
         double a = part->pdy * check->psx - part->pdx * check->psy + part->ptmp;
         double b = part->pdy * check->pex - part->pdx * check->pey + part->ptmp;

         if(!(a*b >= 0)) // oppositely signed?
         {
            if(!nearzero(a) && !nearzero(b)) // not within epsilon of 0?
            {
               // line is split
               double l = check->len;
               double d = (l * a) / (a - b); // distance from intersection

               if(d >= 2.0)
               {
                  cost += FACTOR;

                  // killough: This is the heart of my pruning idea - it 
                  // catches bad segs early on.
                  if(cost > bestcost)
                     goto prune;

                  ++tot;
               }
               else if(l - d < 2.0 ? check->pdx * part->pdx + check->pdy * part->pdy < 0 : b < 0)
                  goto leftside;
            }
            else
               goto leftside;
         }
         else if(a <= 0 && (!nearzero(a) || 
            (nearzero(b) && check->pdx * part->pdx + check->pdy * part->pdy < 0)))
         {
leftside:
            diff -= 2;
         }
      } // end for

      // Take absolute value; diff is being used to obtain the min/max values
      // by way of min(a, b) = (a + b - abs(a - b)) / 2
      if((diff -= tot) < 0)
         diff = -diff;

      // Make sure at least one seg is on each side of the partition
      if(tot + cnt > diff && (cost += diff) < bestcost)
      {
         // we have a new better choice
         bestcost = cost;
         best = part;     // remember the best seg
      }
prune: ; // early exit and skip past the tests above
   } // end for

   return best; // All finished, return best seg
}

//=============================================================================
//
// Tree Building
//

//
// R_createNode
//
// Primary recursive node building routine. Partition the segs using one of the
// segs as the partition line, then recurse into the back and front spaces until
// there are no segs left to classify.
//
// A tree of rpolynode instances is returned. NULL is returned in the terminal
// case where there are no segs left to classify.
//
static rpolynode_t *R_createNode(DLListItem<dynaseg_t> *ts)
{
   if(!ts)
      return NULL; // terminal case: empty list

   rpolynode_t *tn = R_GetFreePolyNode();

   // TODO: DivideSegs

   // TODO: Recurse into left  space
   // TODO: Recurse into right space

   return tn;
}

//=============================================================================
//
// External Interface
//

//
// R_BuildDynaBSP
//
// Call to build a dynamic BSP sub-tree for sorting of dynasegs.
//
rpolynode_t *R_BuildDynaBSP(DLListItem<dynaseg_t> *segs)
{
   // TODO
   return NULL;
}

//
// R_CollapseFragmentsToDSList
//
// Take a subsector and turn its list of rpolyobj fragments into a flat list of
// dynasegs linked by their bsplinks. The dynasegs' BSP-related fields will also
// be initialized. The result is suitable for input to R_BuildDynaBSP.
//
bool R_CollapseFragmentsToDSList(subsector_t *subsec,
                                 DLListItem<dynaseg_t> **list)
{
   DLListItem<rpolyobj_t> *fragment = subsec->polyList;

   // Nothing to do? (We shouldn't really be called in that case, but, hey...)
   if(!fragment)
      return false;

   while(fragment)
   {
      dynaseg_t *ds = fragment->dllObject->dynaSegs;

      while(ds)
      {
         R_setupDSForBSP(*ds);
         ds->bsplink.insert(ds, list);
         ds = ds->subnext;
      }

      fragment = fragment->dllNext;
   }

   return (*list != NULL);
}

// EOF

