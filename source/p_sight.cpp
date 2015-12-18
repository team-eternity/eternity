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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      LineOfSight/Visibility checks, uses REJECT Lookup Table.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "cam_sight.h"
#include "doomstat.h"
#include "e_exdata.h"
#include "m_bbox.h"
#include "p_maputl.h"
#include "p_setup.h"
#include "r_dynseg.h"
#include "r_main.h"
#include "r_state.h"

//
// P_CheckSight
//
// killough 4/19/98:
// Convert LOS info to struct for reentrancy and efficiency of data locality

typedef struct los_s
{
   fixed_t sightzstart, t2x, t2y;   // eye z of looker
   divline_t strace;                // from t1 to t2
   fixed_t topslope, bottomslope;   // slopes to top and bottom of target
   fixed_t bbox[4];
} los_t;

//
// P_DivlineSide
// Returns side 0 (front), 1 (back), or 2 (on).
//
// haleyjd 11/11/02: applied cph's bug fix:
// !node->dy ? x == node->y ? 2 ...
//             ^          ^
// This bug compared the wrong coordinates to each other,
// and caused line-of-sight miscalculations. Turns out the
// P_CrossSubsector optimization demo sync problem was caused by
// masking this bug.
//
template<typename T>
static int P_DivlineSide(fixed_t x, fixed_t y, const T &node)
{
   if(!node.dx)
   {
      if(x == node.x)
         return 2;
      if(x <= node.x)
         return node.dy > 0;
      return node.dy < 0;
   }

   if(!node.dy)
   {
      if((demo_version < 331 ? x : y) == node.y) // here's the bug...
         return 2;
      if(y <= node.y)
         return node.dx < 0;
      return node.dx > 0;
   }

   fixed_t left  = ((x - node.x) >> FRACBITS) * (node.dy >> FRACBITS);
   fixed_t right = ((y - node.y) >> FRACBITS) * (node.dx >> FRACBITS);

   if(right < left)
      return 0; // front side

   if(left == right)
      return 2; // on

   return 1;    // back side
}

//
// P_InterceptVector2
// Returns the fractional intercept point
// along the first divline.
//
// killough 4/19/98: made static, cleaned up
// haleyjd  9/23/02: reformatted
//
static fixed_t P_InterceptVector2(const divline_t *v2, 
                                  const divline_t *v1)
{
   fixed_t den;

   if((den = FixedMul(v1->dy>>8, v2->dx) - FixedMul(v1->dx>>8, v2->dy)))
   {
      return 
         FixedDiv(FixedMul((v1->x - v2->x)>>8, v1->dy) +
                  FixedMul((v2->y - v1->y)>>8, v1->dx), den);
   }

   return 0;
}

//
// P_CrossSubsecPolyObj
//
// haleyjd:
// Checks a line of sight against lines belonging to the given polyobject.
//
static bool P_CrossSubsecPolyObj(polyobj_t *po, los_t *los)
{
   int i;

   for(i = 0; i < po->numLines; ++i)
   {
      line_t *line = po->lines[i];
      divline_t divl;
      const vertex_t *v1,*v2;
      
      // already checked other side?
      if(line->validcount == validcount)
         continue;

      line->validcount = validcount;
      
      // OPTIMIZE: killough 4/20/98: Added quick bounding-box rejection test
      if(line->bbox[BOXLEFT  ] > los->bbox[BOXRIGHT ] ||
         line->bbox[BOXRIGHT ] < los->bbox[BOXLEFT  ] ||
         line->bbox[BOXBOTTOM] > los->bbox[BOXTOP   ] ||
         line->bbox[BOXTOP]    < los->bbox[BOXBOTTOM])
         continue;

      v1 = line->v1;
      v2 = line->v2;
      
      // line isn't crossed?
      if(P_DivlineSide(v1->x, v1->y, los->strace) ==
         P_DivlineSide(v2->x, v2->y, los->strace))
         continue;

      divl.dx = v2->x - (divl.x = v1->x);
      divl.dy = v2->y - (divl.y = v1->y);
      
      // line isn't crossed?
      if(P_DivlineSide(los->strace.x, los->strace.y, divl) ==
         P_DivlineSide(los->t2x, los->t2y, divl))
         continue;

      // stop because it is not two sided
      return false;
   }
   
   return true;
}

//
// P_CrossSubsector
// 
// Returns true if strace crosses the given subsector successfully.
//
// killough 4/19/98: made static and cleaned up
//
static bool P_CrossSubsector(int num, los_t *los)
{
   seg_t *lseg;
   int count;
   DLListItem<rpolyobj_t> *link; // haleyjd 05/16/08
   
#ifdef RANGECHECK
   if(num >= numsubsectors)
      I_Error("P_CrossSubsector: ss %i with numss = %i\n", num, numsubsectors);
#endif

   // haleyjd 02/23/06: this assignment should be after the above check
   lseg = segs + subsectors[num].firstline;

   // haleyjd 02/23/06: check polyobject lines
   if((link = subsectors[num].polyList))
   {
      while(link)
      {
         polyobj_t *po = (*link)->polyobj;

         if(po->validcount != validcount)
         {
            po->validcount = validcount;
            if(!P_CrossSubsecPolyObj(po, los))
               return false;
         }
         link = link->dllNext;
      }
   }

   for(count = subsectors[num].numlines; --count >= 0; lseg++)  // check lines
   {
      line_t *line = lseg->linedef;
      divline_t divl;
      fixed_t opentop, openbottom;
      const sector_t *front, *back;
      const vertex_t *v1,*v2;
      fixed_t frac;
      
      // already checked other side?
      if(line->validcount == validcount)
         continue;

      line->validcount = validcount;
      
      // OPTIMIZE: killough 4/20/98: Added quick bounding-box rejection test
      // haleyjd: another demo compatibility fix by cph -- who knows
      // why this is a problem, though
      // 11/11/02: see P_DivlineSide above to find out why
      if(!demo_compatibility)
      {
         if(line->bbox[BOXLEFT  ] > los->bbox[BOXRIGHT ] ||
            line->bbox[BOXRIGHT ] < los->bbox[BOXLEFT  ] ||
            line->bbox[BOXBOTTOM] > los->bbox[BOXTOP   ] ||
            line->bbox[BOXTOP   ] < los->bbox[BOXBOTTOM])
            continue;
      }

      v1 = line->v1;
      v2 = line->v2;
      
      // line isn't crossed?
      if(P_DivlineSide(v1->x, v1->y, los->strace) ==
         P_DivlineSide(v2->x, v2->y, los->strace))
         continue;

      divl.dx = v2->x - (divl.x = v1->x);
      divl.dy = v2->y - (divl.y = v1->y);
      
      // line isn't crossed?
      if(P_DivlineSide(los->strace.x, los->strace.y, divl) ==
         P_DivlineSide(los->t2x, los->t2y, divl))
         continue;

      // stop because it is not two sided anyway
      if(!(line->flags & ML_TWOSIDED))
         return false;

      // haleyjd 04/30/11: stop on BLOCKALL lines
      if(line->extflags & EX_ML_BLOCKALL)
         return false;

      // crosses a two sided line
      // no wall to block sight with?
      if((front = lseg->frontsector)->floorheight ==
         (back = lseg->backsector)->floorheight   &&
         front->ceilingheight == back->ceilingheight)
         continue;

      // possible occluder
      // because of ceiling height differences
      opentop = front->ceilingheight < back->ceilingheight ?
         front->ceilingheight : back->ceilingheight ;
      
      // because of floor height differences
      openbottom = front->floorheight > back->floorheight ?
         front->floorheight : back->floorheight ;
      
      // quick test for totally closed doors
      if(openbottom >= opentop)
         return false;               // stop

      frac = P_InterceptVector2(&los->strace, &divl);
      
      if(front->floorheight != back->floorheight)
      {
         fixed_t slope = FixedDiv(openbottom - los->sightzstart , frac);
         if(slope > los->bottomslope)
            los->bottomslope = slope;
      }
      
      if(front->ceilingheight != back->ceilingheight)
      {
         fixed_t slope = FixedDiv(opentop - los->sightzstart , frac);
         if(slope < los->topslope)
            los->topslope = slope;
      }

      if (los->topslope <= los->bottomslope)
         return false;               // stop
   }

   // passed the subsector ok
   return true;
}

//
// P_CrossBSPNode
//
// Returns true if strace crosses the given node successfully.
// killough 4/20/98: rewritten to remove tail recursion, clean up, and optimize
//
static bool P_CrossBSPNode(int bspnum, los_t *los)
{
   while(!(bspnum & NF_SUBSECTOR))
   {
      const node_t &bsp = nodes[bspnum];
      int side = P_DivlineSide(los->strace.x, los->strace.y, bsp)&1;
      if(side == P_DivlineSide(los->t2x,      los->t2y,      bsp))
         bspnum = bsp.children[side]; // doesn't touch the other side
      else         // the partition plane is crossed here
      {
         if(!P_CrossBSPNode(bsp.children[side], los))
            return 0;  // cross the starting side
         else
            bspnum = bsp.children[side^1];  // cross the ending side
      }
   }
   return 
      P_CrossSubsector((bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR), los);
}

//
// P_CheckSight
// Returns true
//  if a straight line between t1 and t2 is unobstructed.
// Uses REJECT.
//
// killough 4/20/98: cleaned up, made to use new LOS struct
//
bool P_CheckSight(Mobj *t1, Mobj *t2)
{
   if(full_demo_version >= make_full_version(340, 24))
   {
      camsightparams_t camparams;
      camparams.prev = nullptr;
      camparams.setLookerMobj(t1);
      camparams.setTargetMobj(t2);
      return CAM_CheckSight(camparams);
   }

   const sector_t *s1 = t1->subsector->sector;
   const sector_t *s2 = t2->subsector->sector;
   int pnum = static_cast<int>((s1-sectors)*numsectors + (s2-sectors));
   los_t los;

   // First check for trivial rejection.
   // Determine subsector entries in REJECT table.
   //
   // Check in REJECT table.

   if(rejectmatrix[pnum>>3] & (1 << (pnum&7)))   // can't possibly be connected
      return false;

   // killough 4/19/98: make fake floors and ceilings block monster view
   if((s1->heightsec != -1 &&
       ((t1->z + t1->height <= sectors[s1->heightsec].floorheight &&
         t2->z >= sectors[s1->heightsec].floorheight) ||
        (t1->z >= sectors[s1->heightsec].ceilingheight &&
         t2->z + t1->height <= sectors[s1->heightsec].ceilingheight)))
      ||
      (s2->heightsec != -1 &&
       ((t2->z + t2->height <= sectors[s2->heightsec].floorheight &&
         t1->z >= sectors[s2->heightsec].floorheight) ||
        (t2->z >= sectors[s2->heightsec].ceilingheight &&
         t1->z + t2->height <= sectors[s2->heightsec].ceilingheight))))
      return false;

   // killough 11/98: shortcut for melee situations
   // same subsector? obviously visible
   // haleyjd: compatibility optioned for old demos -- thanks to cph
   // haleyjd 02/23/06: can't do this if there are polyobjects in the subsec
   // haleyjd 12/28/10: This "fix" was not in BOOM 200-202 either!

   if(demo_version >= 203 && !t1->subsector->polyList && 
      t1->subsector == t2->subsector)
      return true;

   // An unobstructed LOS is possible.
   // Now look from eyes of t1 to any part of t2.
   
   validcount++;

   los.topslope = 
      (los.bottomslope = t2->z - (los.sightzstart =
                                 t1->z + t1->height -
                                  (t1->height>>2))) + t2->height;
   los.strace.dx = (los.t2x = t2->x) - (los.strace.x = t1->x);
   los.strace.dy = (los.t2y = t2->y) - (los.strace.y = t1->y);

   if(t1->x > t2->x)
      los.bbox[BOXRIGHT] = t1->x, los.bbox[BOXLEFT] = t2->x;
   else
      los.bbox[BOXRIGHT] = t2->x, los.bbox[BOXLEFT] = t1->x;

   if(t1->y > t2->y)
      los.bbox[BOXTOP] = t1->y, los.bbox[BOXBOTTOM] = t2->y;
   else
      los.bbox[BOXTOP] = t2->y, los.bbox[BOXBOTTOM] = t1->y;
   
   // the head node is the last node output
   return P_CrossBSPNode(numnodes-1, &los);
}

//----------------------------------------------------------------------------
//
// $Log: p_sight.c,v $
// Revision 1.7  1998/05/07  00:55:55  killough
// Make monsters directly tangent to water surface blind
//
// Revision 1.6  1998/05/03  22:34:33  killough
// beautification, header cleanup
//
// Revision 1.5  1998/05/01  14:52:09  killough
// beautification
//
// Revision 1.4  1998/04/24  11:43:08  killough
// minor optimization
//
// Revision 1.3  1998/04/20  11:13:41  killough
// Fix v1.9 demo sync probs, make monsters blind across water
//
// Revision 1.2  1998/01/26  19:24:24  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:00  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

