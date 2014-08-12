// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2014 Ioan Chera
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
//      Version of traversing functions from P_, that use self-contained vars
//      Meant to prevent any global variable interference and possible desync
//
//-----------------------------------------------------------------------------

#include "../z_zone.h"
#include "b_trace.h"
#include "../doomstat.h"
#include "../e_exdata.h"
#include "../p_mobj.h"
#include "../p_setup.h"
#include "../m_dllist.h"
#include "../polyobj.h"
#include "../r_data.h"
#include "../r_portal.h"
#include "../r_state.h"

//
// RTraversal::Execute
//
// Code copied from p_trace/P_PathTraverse, adapted for class-local state
//
bool RTraversal::Execute(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, int flags, bool (*trav)(intercept_t*, void*), void* parm)
{
   fixed_t xt1, yt1;
   fixed_t xt2, yt2;
   fixed_t xstep, ystep;
   fixed_t partial;
   fixed_t xintercept, yintercept;
   int     mapx, mapy;
   int     mapxstep, mapystep;
   int     count;
   
   ++m_validcount;
   m_intercept_p = m_intercepts;
   
   if(!((x1-bmaporgx)&(MAPBLOCKSIZE-1)))
      x1 += FRACUNIT;     // don't side exactly on a line
   
   if(!((y1-bmaporgy)&(MAPBLOCKSIZE-1)))
      y1 += FRACUNIT;     // don't side exactly on a line
   
   m_trace.dl.x = x1;
   m_trace.dl.y = y1;
   m_trace.dl.dx = x2 - x1;
   m_trace.dl.dy = y2 - y1;
   
   x1 -= bmaporgx;
   y1 -= bmaporgy;
   xt1 = x1 >> MAPBLOCKSHIFT;
   yt1 = y1 >> MAPBLOCKSHIFT;
   
   x2 -= bmaporgx;
   y2 -= bmaporgy;
   xt2 = x2 >> MAPBLOCKSHIFT;
   yt2 = y2 >> MAPBLOCKSHIFT;
   
   if(xt2 > xt1)
   {
      mapxstep = 1;
      partial = FRACUNIT - ((x1 >> MAPBTOFRAC) & (FRACUNIT-1));
      ystep = FixedDiv(y2 - y1, D_abs(x2 - x1));
   }
   else if(xt2 < xt1)
   {
      mapxstep = -1;
      partial = (x1 >> MAPBTOFRAC) & (FRACUNIT - 1);
      ystep = FixedDiv(y2 - y1, D_abs(x2 - x1));
   }
   else
   {
      mapxstep = 0;
      partial = FRACUNIT;
      ystep = 256 * FRACUNIT;
   }
   
   yintercept = (y1 >> MAPBTOFRAC) + FixedMul(partial, ystep);
   
   if(yt2 > yt1)
   {
      mapystep = 1;
      partial = FRACUNIT - ((y1 >> MAPBTOFRAC) & (FRACUNIT - 1));
      xstep = FixedDiv(x2 - x1, D_abs(y2 - y1));
   }
   else if(yt2 < yt1)
   {
      mapystep = -1;
      partial = (y1 >> MAPBTOFRAC) & (FRACUNIT - 1);
      xstep = FixedDiv(x2 - x1, D_abs(y2 - y1));
   }
   else
   {
      mapystep = 0;
      partial = FRACUNIT;
      xstep = 256 * FRACUNIT;
   }
   
   xintercept = (x1 >> MAPBTOFRAC) + FixedMul(partial, xstep);
   
   // Step through map blocks.
   // Count is present to prevent a round off error
   // from skipping the break.
   
   mapx = xt1;
   mapy = yt1;

   for(count = 0; count < 64; count++)
   {
      if(flags & PT_ADDLINES)
      {
         if(!blockLinesIterator(mapx, mapy, [this](line_t* ld){
            return addLineIntercepts(ld);
         }))
            return false; // early out
      }
      
      if(flags & PT_ADDTHINGS)
      {
         if(!blockThingsIterator(mapx, mapy, [this](Mobj* thing){
            return addThingIntercepts(thing);
         }))
            return false; // early out
      }
      
      if(mapx == xt2 && mapy == yt2)
         break;
      
      if((yintercept >> FRACBITS) == mapy)
      {
         yintercept += ystep;
         mapx += mapxstep;
      }
      else if((xintercept >> FRACBITS) == mapx)
      {
         xintercept += xstep;
         mapy += mapystep;
      }
   }
   
   // go through the sorted list
   // SoM: just store this for a sec
   
   return traverseIntercepts(trav, FRACUNIT, parm);
}

bool RTraversal::traverseIntercepts(bool(*func)(intercept_t*, void*), fixed_t maxfrac, void* parm)
{
   intercept_t *in = nullptr;
   int count = static_cast<int>(m_intercept_p - m_intercepts);
   while(count--)
   {
      fixed_t dist = D_MAXINT;
      intercept_t *scan;
      for(scan = m_intercepts; scan < m_intercept_p; scan++)
         if(scan->frac < dist)
            dist = (in = scan)->frac;
      if(dist > maxfrac)
         return true;    // checked everything in range
      
      if(in) // haleyjd: for safety
      {
         if(!func(in, parm))
            return false;           // don't bother going farther
         in->frac = D_MAXINT;
      }
   }
   return true;                  // everything was traversed
}

//
// RTraversal::blockLinesIterator
//
// Pretty much from P_Trace, except with safe calls
//
bool RTraversal::blockLinesIterator(int x, int y, const std::function<bool(line_t*)>& func)
{
   int        offset;
   const int  *list;     // killough 3/1/98: for removal of blockmap limit
   const DLListItem<polymaplink_t> *plink; // haleyjd 02/22/06
   
   if(x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight)
      return true;
   offset = y * bmapwidth + x;
   
   // haleyjd 02/22/06: consider polyobject lines
   plink = polyblocklinks[offset];

   while(plink)
   {
      const polyobj_t *po = (*plink)->po;
      
      if(m_polyValidcount[po] != m_validcount) // if polyobj hasn't been checked
      {
         int i;
         m_polyValidcount[po] = m_validcount;
         
         for(i = 0; i < po->numLines; ++i)
         {
            if(m_lineValidcount[po->lines[i] - lines] == m_validcount) // line has been checked
               continue;
            m_lineValidcount[po->lines[i] - lines] = m_validcount;
            if(!func(po->lines[i]))
               return false;
         }
      }
      plink = plink->dllNext;
   }
   
   // original was reading delimiting 0 as linedef 0 -- phares
   offset = *(blockmap + offset);
   list = blockmaplump + offset;
   
   // killough 1/31/98: for compatibility we need to use the old method.
   // Most demos go out of sync, and maybe other problems happen, if we
   // don't consider linedef 0. For safety this should be qualified.
   
   // killough 2/22/98: demo_compatibility check
   // skip 0 starting delimiter -- phares
   if(!demo_compatibility)
      list++;
   for( ; *list != -1; list++)
   {
      line_t *ld;
      
      // haleyjd 04/06/10: to avoid some crashes during demo playback due to
      // invalid blockmap lumps
      if(*list >= numlines)
         continue;
      
      ld = &lines[*list];
      if(m_lineValidcount[*list] == m_validcount)
         continue;       // line has already been checked
      m_lineValidcount[*list] = m_validcount;
      if(!func(ld))
         return false;
   }
   return true;  // everything was checked
}

//
// RTraversal::blockThingsIterator
//
// This really doesn't look different from P_BlockThingsIterator
//
bool RTraversal::blockThingsIterator(int x, int y, const std::function<bool(Mobj*)>& func)
{
   if(!(x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight))
   {
      Mobj *mobj = blocklinks[y * bmapwidth + x];
      
      for(; mobj; mobj = mobj->bnext)
         if(!func(mobj))
            return false;
   }
   return true;
}

//
// RTraversal::addLineIntercepts
//
bool RTraversal::addLineIntercepts(line_t* ld)
{
   int       s1;
   int       s2;
   fixed_t   frac;
   divline_t dl;
   
   // avoid precision problems with two routines
   if(m_trace.dl.dx >  FRACUNIT*16 || m_trace.dl.dy >  FRACUNIT*16 ||
      m_trace.dl.dx < -FRACUNIT*16 || m_trace.dl.dy < -FRACUNIT*16)
   {
      s1 = P_PointOnDivlineSide (ld->v1->x, ld->v1->y, &m_trace.dl);
      s2 = P_PointOnDivlineSide (ld->v2->x, ld->v2->y, &m_trace.dl);
   }
   else
   {
      s1 = P_PointOnLineSide (m_trace.dl.x, m_trace.dl.y, ld);
      s2 = P_PointOnLineSide (m_trace.dl.x + m_trace.dl.dx, m_trace.dl.y + m_trace.dl.dy, ld);
   }
   
   if(s1 == s2)
      return true;        // line isn't crossed
   
   // hit the line
   P_MakeDivline(ld, &dl);
   frac = P_InterceptVector(&m_trace.dl, &dl);
   
   if(frac < 0)
      return true;        // behind source
   
   checkIntercept();    // killough
   
   m_intercept_p->frac = frac;
   m_intercept_p->isaline = true;
   m_intercept_p->d.line = ld;
   m_intercept_p++;
   
   return true;  // continue
}

bool RTraversal::addThingIntercepts(Mobj *thing)
{
   fixed_t x1, y1;
   fixed_t x2, y2;
   int s1, s2;
   divline_t dl;
   fixed_t frac;
   
   if((m_trace.dl.dx ^ m_trace.dl.dy) > 0)
   {
      x1 = thing->x - thing->radius;
      y1 = thing->y + thing->radius;
      x2 = thing->x + thing->radius;
      y2 = thing->y - thing->radius;
   }
   else
   {
      x1 = thing->x - thing->radius;
      y1 = thing->y - thing->radius;
      x2 = thing->x + thing->radius;
      y2 = thing->y + thing->radius;
   }
   
   s1 = P_PointOnDivlineSide (x1, y1, &m_trace.dl);
   s2 = P_PointOnDivlineSide (x2, y2, &m_trace.dl);
   
   if(s1 == s2)
      return true;
   
   dl.x = x1;
   dl.y = y1;
   dl.dx = x2-x1;
   dl.dy = y2-y1;
   
   frac = P_InterceptVector (&m_trace.dl, &dl);
   
   if (frac < 0)
      return true;                // behind source
   
   checkIntercept();
   
   m_intercept_p->frac = frac;
   m_intercept_p->isaline = false;
   m_intercept_p->d.thing = thing;
   m_intercept_p++;
   
   return true;          // keep going
}

void RTraversal::checkIntercept()
{
   size_t offset = m_intercept_p - m_intercepts;
   if(offset >= m_num_intercepts)
   {
      m_num_intercepts = m_num_intercepts ? m_num_intercepts * 2 : 128;
      m_intercepts = erealloc(intercept_t *, m_intercepts, sizeof(*m_intercepts) * m_num_intercepts);
      m_intercept_p = m_intercepts + offset;
   }
   
   
}

//
// RTraversal::RTraversal
//
// Constructor
//
// IMPORTANT NOTE: upon construction, it will build an array the size of
// numlines. And it will not update after P_SetupLevel. So it's best if the
// lifetime of such an object is the same as the level.
//
RTraversal::RTraversal() :
m_trace(),
m_validcount(0),
m_intercepts(nullptr),
m_intercept_p(nullptr),
m_num_intercepts(0),
m_clip()
{
   m_lineValidcount = ecalloc(int*, numlines, sizeof(int));
}

void RTraversal::LineOpening(const line_t* linedef, const Mobj* mo)
{
    fixed_t frontceilz, frontfloorz, backceilz, backfloorz;
    // SoM: used for 3dmidtex
    fixed_t frontcz, frontfz, backcz, backfz, otop, obot;

    if (linedef->sidenum[1] == -1)      // single sided line
    {
        m_clip.openrange = 0;
        return;
    }

    m_clip.openfrontsector = linedef->frontsector;
    m_clip.openbacksector = linedef->backsector;

    {
#ifdef R_LINKEDPORTALS
        if (mo && demo_version >= 333 &&
            m_clip.openfrontsector->c_pflags & PS_PASSABLE &&
            m_clip.openbacksector->c_pflags & PS_PASSABLE &&
            m_clip.openfrontsector->c_portal == m_clip.openbacksector->c_portal)
        {
            frontceilz = backceilz = m_clip.openfrontsector->ceilingheight + (1024 * FRACUNIT);
        }
        else
#endif
        {
            frontceilz = m_clip.openfrontsector->ceilingheight;
            backceilz = m_clip.openbacksector->ceilingheight;
        }

        frontcz = m_clip.openfrontsector->ceilingheight;
        backcz = m_clip.openbacksector->ceilingheight;
    }

    {
#ifdef R_LINKEDPORTALS
        if (mo && demo_version >= 333 &&
            m_clip.openfrontsector->f_pflags & PS_PASSABLE &&
            m_clip.openbacksector->f_pflags & PS_PASSABLE &&
            m_clip.openfrontsector->f_portal == m_clip.openbacksector->f_portal)
        {
            frontfloorz = backfloorz = m_clip.openfrontsector->floorheight - (1024 * FRACUNIT); //mo->height;
        }
        else
#endif
        {
            frontfloorz = m_clip.openfrontsector->floorheight;
            backfloorz = m_clip.openbacksector->floorheight;
        }

        frontfz = m_clip.openfrontsector->floorheight;
        backfz = m_clip.openbacksector->floorheight;
    }

    if (frontceilz < backceilz)
        m_clip.opentop = frontceilz;
    else
        m_clip.opentop = backceilz;

    if (frontfloorz > backfloorz)
    {
        m_clip.openbottom = frontfloorz;
        m_clip.lowfloor = backfloorz;
        // haleyjd
        m_clip.floorpic = m_clip.openfrontsector->floorpic;
    }
    else
    {
        m_clip.openbottom = backfloorz;
        m_clip.lowfloor = frontfloorz;
        // haleyjd
        m_clip.floorpic = m_clip.openbacksector->floorpic;
    }

    if (frontcz < backcz)
        otop = frontcz;
    else
        otop = backcz;

    if (frontfz > backfz)
        obot = frontfz;
    else
        obot = backfz;

    m_clip.opensecfloor = m_clip.openbottom;
    m_clip.opensecceil = m_clip.opentop;

    if (demo_version >= 331 && mo && (linedef->flags & ML_3DMIDTEX) &&
        sides[linedef->sidenum[0]].midtexture)
    {
        fixed_t textop, texbot, texmid;
        side_t *side = &sides[linedef->sidenum[0]];

        if (linedef->flags & ML_DONTPEGBOTTOM)
        {
            texbot = side->rowoffset + obot;
            textop = texbot + textures[side->midtexture]->heightfrac;
        }
        else
        {
            textop = otop + side->rowoffset;
            texbot = textop - textures[side->midtexture]->heightfrac;
        }
        texmid = (textop + texbot) / 2;

        // SoM 9/7/02: use monster blocking line to provide better
        // clipping
        if ((linedef->flags & ML_BLOCKMONSTERS) &&
            !(mo->flags & (MF_FLOAT | MF_DROPOFF)) &&
            D_abs(mo->z - textop) <= 24 * FRACUNIT)
        {
            m_clip.opentop = m_clip.openbottom;
            m_clip.openrange = 0;
            return;
        }

        if (mo->z + (P_ThingInfoHeight(mo->info) / 2) < texmid)
        {
            if (texbot < m_clip.opentop)
                m_clip.opentop = texbot;
        }
        else
        {
            if (textop > m_clip.openbottom)
                m_clip.openbottom = textop;

            // The mobj is above the 3DMidTex, so check to see if it's ON the 3DMidTex
            // SoM 01/12/06: let monsters walk over dropoffs
            if (abs(mo->z - textop) <= 24 * FRACUNIT)
                m_clip.touch3dside = 1;
        }
    }

    m_clip.openrange = m_clip.opentop - m_clip.openbottom;
}

// CODE FROM PTR_AimTraverse
bool RTraversal::TRaimTraverse(intercept_t* in, void* v)
{
    RTraversal& self = *static_cast<RTraversal*>(v);
    fixed_t slope, dist;

    if (in->isaline)
    {
        const line_t *li = in->d.line;
        if (!(li->flags & ML_TWOSIDED) || (li->extflags & EX_ML_BLOCKALL))
            return false; // stop

        self.LineOpening(li, nullptr);

        if (self.m_clip.openbottom >= self.m_clip.opentop)
            return false;

        dist = FixedMul(self.m_trace.attackrange, in->frac);
        if (li->frontsector->floorheight != li->backsector->floorheight)
        {
            slope = FixedDiv(self.m_clip.openbottom - self.m_trace.z, dist);
            if (slope > self.m_trace.bottomslope)
                self.m_trace.bottomslope = slope;
        }

        if (li->frontsector->ceilingheight != li->backsector->ceilingheight)
        {
            slope = FixedDiv(self.m_clip.opentop - self.m_trace.z, dist);
            if (slope < self.m_trace.topslope)
                self.m_trace.topslope = slope;
        }

        if (self.m_trace.topslope <= self.m_trace.bottomslope)
            return false;

        return true;
    }
    else
    {
        // shoot a thing
        Mobj *th = in->d.thing;
        fixed_t thingtopslope, thingbottomslope;

        if (th == self.m_trace.thing)
            return true; // can't shoot self

        if (!(th->flags & MF_SHOOTABLE))
            return true; // corpse or something

        // killough 7/19/98, 8/2/98:
        // friends don't aim at friends (except players), at least not first
        if (th->flags & self.m_trace.thing->flags & self.m_trace.aimflagsmask && !th->player)
            return true;

        // check angles to see if the thing can be aimed at
        dist = FixedMul(self.m_trace.attackrange, in->frac);
        thingtopslope = FixedDiv(th->z + th->height - self.m_trace.z, dist);

        if (thingtopslope < self.m_trace.bottomslope)
            return true; // shot over the thing

        thingbottomslope = FixedDiv(th->z - self.m_trace.z, dist);

        if (thingbottomslope > self.m_trace.topslope)
            return true; // shot under the thing

        // this thing can be hit!
        if (thingtopslope > self.m_trace.topslope)
            thingtopslope = self.m_trace.topslope;

        if (thingbottomslope < self.m_trace.bottomslope)
            thingbottomslope = self.m_trace.bottomslope;

        self.m_trace.aimslope = (thingtopslope + thingbottomslope) / 2;
        self.m_clip.linetarget = th;

        return false; // don't go any further
    }
}

// LARGELY TAKEN FROM P_AimLineAttack
fixed_t RTraversal::SafeAimLineAttack(Mobj* t1, angle_t angle, fixed_t distance, int mask)
{
    fixed_t x2, y2;
    fixed_t lookslope = 0;
    fixed_t pitch = 0;

    angle >>= ANGLETOFINESHIFT;
    m_trace.thing = t1;

    x2 = t1->x + (distance >> FRACBITS) * (m_trace.cos = finecosine[angle]);
    y2 = t1->y + (distance >> FRACBITS)*(m_trace.sin = finesine[angle]);
    m_trace.z = t1->z + (t1->height >> 1) + 8 * FRACUNIT;

    if (t1->player)
        pitch = t1->player->pitch;

    if (pitch == 0 || demo_version < 333)
    {
        m_trace.topslope = 100 * FRACUNIT / 160;
        m_trace.bottomslope = -100 * FRACUNIT / 160;
    }
    else
    {
        fixed_t topangle, bottomangle;

        lookslope = finetangent[(ANG90 - pitch) >> ANGLETOFINESHIFT];

        topangle = pitch - ANGLE_1 * 32;
        bottomangle = pitch + ANGLE_1 * 32;

        m_trace.topslope = finetangent[(ANG90 - topangle) >> ANGLETOFINESHIFT];
        m_trace.bottomslope = finetangent[(ANG90 - bottomangle) >> ANGLETOFINESHIFT];
    }

    m_trace.attackrange = distance;
    m_clip.linetarget = nullptr;

    m_trace.aimflagsmask = mask;

    Execute(t1->x, t1->y, x2, y2, PT_ADDLINES | PT_ADDTHINGS, RTraversal::TRaimTraverse, this);
    return m_clip.linetarget ? m_trace.aimslope : lookslope;
}