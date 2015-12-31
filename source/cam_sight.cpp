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
//--------------------------------------------------------------------------
//
// For portions of code explicitly marked as being under the 
// ZDoom Source Distribution License only:
//
// Copyright 1998-2012 Randy Heit  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions 
// are met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// 3. The name of the author may not be used to endorse or promote products
//    derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Line of sight checking for cameras
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "cam_sight.h"
#include "d_player.h"   // ioanch 20151230: for autoaim
#include "e_exdata.h"
#include "m_collection.h"
#include "m_fixed.h"
#include "p_chase.h"
#include "p_maputl.h"
#include "p_setup.h"
#include "polyobj.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_portal.h"
#include "r_state.h"

// ioanch 20151230: defined macros for the validcount sets here
#define VALID_ALLOC(set, n) ((set) = ecalloc(byte *, 1, (((n) + 7) & ~7) / 8))
#define VALID_FREE(set) efree(set)
#define VALID_ISSET(set, i) ((set)[(i) >> 3] & (1 << ((i) & 7)))
#define VALID_SET(set, i) ((set)[(i) >> 3] |= 1 << ((i) & 7))

//=============================================================================
//
// Structures
//

class CamSight
{
public:
   bool isAim; // ioanch: whether it's an aim cam (true) or checksight (false)
   
   fixed_t cx;          // camera coordinates
   fixed_t cy;
   fixed_t tx;          // target coordinates
   fixed_t ty;
   fixed_t sightzstart; // eye z of looker
   fixed_t topslope;    // slope to top of target
   fixed_t bottomslope; // slope to bottom of target

   fixed_t attackrange; // ioanch 20151230: only used for aim
   fixed_t aimslope; // ioanch 20151230: resulting aiming slope
   Mobj *linetarget;    // ioanch 20151230: found mobj
   const Mobj *source;  // ioanch 20151230: needed because of thing iterator
   uint32_t aimflagsmask;  // ioanch 20151230: to prevent friends

   divline_t trace;     // for line crossing tests

   fixed_t opentop;     // top of linedef silhouette
   fixed_t openbottom;  // bottom of linedef silhouette
   fixed_t openrange;   // height of opening

   // Intercepts vector
   PODCollection<intercept_t> intercepts;

   // linedef validcount substitute
   byte *validlines;
   byte *validpolys;

   // portal traversal information
   int  fromid;        // current source group id
   int  toid;          // group id of the target
   bool hitpblock;     // traversed a block with a line portal
   bool addedportal;   // added a portal line to the intercepts list in current block
   bool portalresult;  // result from portal recursion
   bool portalexit;    // if true, returned from portal

   // pointer to invocation parameters
   union
   {
      const camsightparams_t *params;
      const camaimparams_t *aimparams; // ioanch 20151230;
   };

   // ioanch 20151229: line portal sight fix
   // different names for different semantics
   union
   {
      fixed_t originfrac;  // for checking sight
      fixed_t origindist;  // for finding aim targets
   };

   // ioanch 20151229: added optional bottom and top slope preset
   // in case of portal continuation
   CamSight(const camsightparams_t &sp, fixed_t inbasefrac = 0,
      fixed_t inbottomslope = D_MAXINT, fixed_t intopslope = D_MAXINT)
      : isAim(false),
        cx(sp.cx), cy(sp.cy), tx(sp.tx), ty(sp.ty), 
        attackrange(0), aimslope(0), linetarget(nullptr), source(nullptr),
        aimflagsmask(0),
        opentop(0), openbottom(0), openrange(0),
        intercepts(),
        fromid(sp.cgroupid), toid(sp.tgroupid), 
        hitpblock(false), addedportal(false), 
        portalresult(false), portalexit(false),
        params(&sp), originfrac(inbasefrac)
   {
      memset(&trace, 0, sizeof(trace));
    
      sightzstart = params->cz + params->cheight - (params->cheight >> 2);
      if(inbottomslope == D_MAXINT)
         bottomslope = params->tz - sightzstart;
      else
         bottomslope = inbottomslope;
      if(intopslope == D_MAXINT)
         topslope    = bottomslope + params->theight;
      else
         topslope = intopslope;

      // ioanch 20151230: use the macros
      VALID_ALLOC(validlines, ::numlines);
      VALID_ALLOC(validpolys, ::numPolyObjects);
   }

   // ioanch 20151230: autoaim support
   CamSight(const camaimparams_t &sp, fixed_t inbasedist = 0,
      fixed_t inbottomslope = D_MAXINT, fixed_t intopslope = D_MAXINT) : 
      isAim(true), cx(sp.cx), cy(sp.cy),
      attackrange(sp.distance), aimslope(0), linetarget(nullptr), 
      source(sp.source), aimflagsmask(sp.mask),
      opentop(0), openbottom(0), openrange(0),
      intercepts(), fromid(sp.cgroupid), toid(R_NOGROUP),
      hitpblock(false), addedportal(false), 
      portalresult(false), portalexit(false),
      aimparams(&sp), origindist(inbasedist)
   {
      memset(&trace, 0, sizeof(trace));

      tx = cx + (sp.distance >> FRACBITS) 
         * finecosine[sp.angle >> ANGLETOFINESHIFT];
      ty = cy + (sp.distance >> FRACBITS)
         * finesine[sp.angle >> ANGLETOFINESHIFT];
      sightzstart = sp.cz + (sp.cheight >> 1) + 8 * FRACUNIT;

      if(!sp.pitch)
      {
         if(intopslope == D_MAXINT)
            topslope = 100*FRACUNIT/160;
         else
            topslope = intopslope;

         if(inbottomslope == D_MAXINT)
            bottomslope = -100*FRACUNIT/160;
         else
            bottomslope = inbottomslope;
      }
      else
      {
         fixed_t topangle, bottomangle;

         if(intopslope == D_MAXINT)
         {
            topangle = sp.pitch - ANGLE_1 * 32;
            topslope = finetangent[(ANG90 - topangle) >> ANGLETOFINESHIFT];
         }
         else
            topslope = intopslope;

         if(inbottomslope == D_MAXINT)
         {
            bottomangle = sp.pitch + ANGLE_1*32;
            bottomslope = finetangent[(ANG90 - bottomangle) >> ANGLETOFINESHIFT];
         }
         else
            bottomslope = inbottomslope;
      }

      VALID_ALLOC(validlines, ::numlines);
      VALID_ALLOC(validpolys, ::numPolyObjects);
   }

   ~CamSight()
   {
      VALID_FREE(validlines);
      VALID_FREE(validpolys);
   }
};

//=============================================================================
//
// Camera Sight Parameter Methods
//

//
// camsightparams_t::setCamera
//
// Set a camera object as the camera point.
//
void camsightparams_t::setCamera(const camera_t &camera, fixed_t height)
{
   cx       = camera.x;
   cy       = camera.y;
   cz       = camera.z;
   cheight  = height;
   cgroupid = camera.groupid;
}

//
// camsightparams_t::setLookerMobj
//
// Set an Mobj as the camera point.
//
void camsightparams_t::setLookerMobj(const Mobj *mo)
{
   cx       = mo->x;
   cy       = mo->y;
   cz       = mo->z;
   cheight  = mo->height;
   cgroupid = mo->groupid;
}

//
// camsightparams_t::setTargetMobj
//
// Set an Mobj as the target point.
//
void camsightparams_t::setTargetMobj(const Mobj *mo)
{
   tx       = mo->x;
   ty       = mo->y;
   tz       = mo->z;
   theight  = mo->height;
   tgroupid = mo->groupid;
}

//
// camaimparams_t::set
//
// ioanch 20151230: Sets the data for the autoaim part
//
void camaimparams_t::set(const Mobj *mo, angle_t inAngle, fixed_t inDistance,
                         uint32_t inMask)
{
   source = mo;
   cx = mo->x; cy = mo->y; cz = mo->z;
   cheight = mo->height;
   cgroupid = mo->groupid;
   angle = inAngle;
   pitch = mo->player ? mo->player->pitch : 0;
   distance = inDistance;
   mask = inMask;
}

//=============================================================================
//
// Sight Checking
//

//
// CAM_LineOpening
//
// Sets opentop and openbottom to the window
// through a two sided line.
//
static void CAM_LineOpening(CamSight &cam, const line_t *linedef)
{
   sector_t *front, *back;
   fixed_t frontceilz, frontfloorz, backceilz, backfloorz;

   if(linedef->sidenum[1] == -1)      // single sided line
   {
      cam.openrange = 0;
      return;
   }
   
   front = linedef->frontsector;
   back  = linedef->backsector;

   frontceilz  = front->ceilingheight;
   backceilz   = back->ceilingheight;
   frontfloorz = front->floorheight;
   backfloorz  = back->floorheight;

   if(frontceilz < backceilz)
      cam.opentop = frontceilz;
   else
      cam.opentop = backceilz;

   if(frontfloorz > backfloorz)
      cam.openbottom = frontfloorz;
   else
      cam.openbottom = backfloorz;

   cam.openrange = cam.opentop - cam.openbottom;
}

//
// CAM_doPortalSight
//
// ioanch 20151229: common function to spawn a new camera beyond portal
//
static bool CAM_CheckSight(const camsightparams_t &params, fixed_t originfrac,
                           fixed_t bottomslope, fixed_t topslope);
static bool CAM_doPortalSight(const CamSight &cam, int newfromid, 
                               fixed_t x, fixed_t y, fixed_t bottomslope, 
                               fixed_t topslope, fixed_t totalfrac)
{
   camsightparams_t params;

   const camsightparams_t *prev = cam.params->prev;
   while(prev)
   {
      // we are trying to walk backward?
      if(prev->cgroupid == newfromid)
         return false; // ignore this plane
      prev = prev->prev;
   }

   const linkoffset_t *link = P_GetLinkIfExists(cam.fromid, newfromid);
   // Copy all except x and y
   params.cx           = x;
   params.cy           = y;
   params.cz           = cam.params->cz;
   params.cheight      = cam.params->cheight;
   params.tx           = cam.params->tx;
   params.ty           = cam.params->ty;
   params.tz           = cam.params->tz;
   params.theight      = cam.params->theight;
   params.cgroupid     = newfromid;
   params.tgroupid     = cam.toid;
   params.prev         = cam.params;

   if(link)
   {
      params.cx += link->x;
      params.cy += link->y;
   }

   return CAM_CheckSight(params, totalfrac, bottomslope, topslope);
}

//
// CAM_checkPortalSector
//
// ioanch 20151229: check sector if it has portals and create cameras for them
// Returns true if any branching sight check beyond a portal returned true
//
static bool CAM_checkPortalSector(const CamSight &cam, const sector_t *sector, 
                                  fixed_t totalfrac, fixed_t partialfrac)
{
   fixed_t linehitz, fixedratio;
   int newfromid;

   fixed_t x, y, newslope;

   if(cam.topslope > 0 && sector->c_portal && sector->c_pflags & PS_PASSABLE
      && (newfromid = sector->c_portal->data.link.toid) != cam.fromid)
   {
      // ceiling portal (slope must be up)
      linehitz = cam.sightzstart + FixedMul(cam.topslope, totalfrac);
      if(linehitz > sector->ceilingheight)
      {
         // update cam.bottomslope to be the top of the sector wall
         newslope = FixedDiv(sector->ceilingheight - cam.sightzstart, 
            totalfrac);
         // if totalfrac == 0, then it will just be a very big slope
         if(newslope < cam.bottomslope)
            newslope = cam.bottomslope;

         // get x and y of position
         if(linehitz == cam.sightzstart)
         {
            // handle this edge case: put point right on line
            fixedratio = 1;
            x = cam.params->cx + FixedMul(cam.trace.dx, partialfrac);
            y = cam.params->cy + FixedMul(cam.trace.dy, partialfrac);
         }
         else
         {
            // add a unit just to ensure that it enters the sector
            fixedratio = FixedDiv(sector->ceilingheight - cam.sightzstart, 
               linehitz - cam.sightzstart) + 1;
            // update z frac
            totalfrac = FixedMul(fixedratio, totalfrac);
            // retrieve the xy frac using the origin frac
            partialfrac = FixedDiv(totalfrac - cam.originfrac, 
               FRACUNIT - cam.originfrac);

            x = cam.params->cx + FixedMul(partialfrac, cam.trace.dx);
            y = cam.params->cy + FixedMul(partialfrac, cam.trace.dy);
               
         }
         
         if(CAM_doPortalSight(cam, newfromid, x, y, newslope, cam.topslope, 
            totalfrac))
         {
            return true;
         }
      }
   }
   if(cam.bottomslope < 0 && sector->f_portal && sector->f_pflags & PS_PASSABLE 
      && (newfromid = sector->f_portal->data.link.toid) != cam.fromid)
   {
      linehitz = cam.sightzstart + FixedMul(cam.bottomslope, totalfrac);
      if(linehitz < sector->floorheight)
      {
         newslope = FixedDiv(sector->floorheight - cam.sightzstart,
            totalfrac);
         if(newslope > cam.topslope)
            newslope = cam.topslope;

         if(linehitz == cam.sightzstart)
         {
            x = cam.params->cx + FixedMul(cam.trace.dx, partialfrac);
            y = cam.params->cy + FixedMul(cam.trace.dy, partialfrac);
         }
         else
         {
            fixedratio = FixedDiv(sector->floorheight - cam.sightzstart, 
               linehitz - cam.sightzstart) + 1;
            totalfrac = FixedMul(fixedratio, totalfrac);
            partialfrac = FixedDiv(totalfrac - cam.originfrac, 
               FRACUNIT - cam.originfrac);

            x = cam.params->cx + FixedMul(partialfrac, cam.trace.dx);
            y = cam.params->cy + FixedMul(partialfrac, cam.trace.dy);
         }

         if(CAM_doPortalSight(cam, newfromid, x, y, cam.bottomslope, newslope,
            totalfrac))
         {
            return true;
         }
      }
   }
   return false;
}

//
// CAM_SightTraverse
//
static bool CAM_SightTraverse(CamSight &cam, intercept_t *in)
{
   const line_t *li; // ioanch 20151229: made const
   fixed_t slope;
	
   li = in->d.line;

   //
   // crosses a two sided line
   //
   CAM_LineOpening(cam, li);

   // ioanch 20151229: line portal fix
   // total initial length = 1
   // we have (originfrac + infrac * (1 - originfrac))
   // this is totalfrac, and is the fraction from the master cam origin
   fixed_t totalfrac = cam.originfrac ? cam.originfrac 
      + FixedMul(in->frac, FRACUNIT - cam.originfrac) : in->frac;

   // ioanch 20151229: look for floor and ceiling portals too
   const sector_t *sector = P_PointOnLineSide(cam.cx, cam.cy, li) == 0 ?
      li->frontsector : li->backsector;
   const sector_t *osector = sector == li->frontsector ? 
      li->backsector : li->frontsector;
   if(sector && totalfrac > 0)
   {
      if(CAM_checkPortalSector(cam, sector, totalfrac, in->frac))
      {
         cam.portalresult = true;
         cam.portalexit = true;
         return false;
      }
   }

   // quick test for totally closed doors
   // ioanch 20151231: also check BLOCKALL lines
   if(cam.openrange <= 0 || li->extflags & EX_ML_BLOCKALL) 
      return false; // stop

   // ioanch 20151229: also check for plane portals, updating the slopes
   if(sector->floorheight != osector->floorheight 
      || sector->f_pflags & PS_PASSABLE || osector->f_pflags & PS_PASSABLE)
   {
      slope = FixedDiv(cam.openbottom - cam.sightzstart, totalfrac);
      if(slope > cam.bottomslope)
         cam.bottomslope = slope;
   }
	
   if(sector->ceilingheight != osector->ceilingheight
      || sector->c_pflags & PS_PASSABLE || osector->c_pflags & PS_PASSABLE)
   {
      slope = FixedDiv(cam.opentop - cam.sightzstart, totalfrac);
      if(slope < cam.topslope)
         cam.topslope = slope;
   }

   if(cam.topslope <= cam.bottomslope)
      return false; // stop

   // have we hit a portal line?
   if(li->pflags & PS_PASSABLE)
   {
      camsightparams_t params;
      int newfromid = li->portal->data.link.toid;

      if(newfromid == cam.fromid) // not taking us anywhere...
         return true;

      const camsightparams_t *prev = cam.params->prev;
      while(prev)
      {
         // we are trying to walk backward?
         if(prev->cgroupid == newfromid)
            return true; // ignore this line
         prev = prev->prev;
      }

      linkoffset_t *link = P_GetLinkIfExists(cam.fromid, newfromid);

      params.cx           = cam.params->cx + FixedMul(cam.trace.dx, in->frac);
      params.cy           = cam.params->cy + FixedMul(cam.trace.dy, in->frac);
      params.cz           = cam.params->cz;
      params.cheight      = cam.params->cheight;
      params.tx           = cam.params->tx;
      params.ty           = cam.params->ty;
      params.tz           = cam.params->tz;
      params.theight      = cam.params->theight;
      params.cgroupid     = newfromid;
      params.tgroupid     = cam.toid;
      params.prev         = cam.params;

      if(link)
      {
         params.cx += link->x;
         params.cy += link->y;
      }

      cam.portalresult = CAM_CheckSight(params, totalfrac, 
         cam.bottomslope, cam.topslope);
      cam.portalexit   = true;
      return false;    // break out      
   }

   return true; // keep going
}

//
// CAM_aimTraverse
//
// ioanch 20151230: autoaim support
//
static fixed_t CAM_AimLineAttack(const camaimparams_t &params, 
                                 Mobj **outTarget, fixed_t originfrac, 
                                 fixed_t bottomslope, fixed_t topslope);
static bool CAM_aimTraverse(CamSight &cam, intercept_t *in)
{
   fixed_t totaldist, slope;
   if(in->isaline)
   {
      const line_t *li = in->d.line;

      if(!(li->flags & ML_TWOSIDED) || li->extflags & EX_ML_BLOCKALL)
         return false;

      CAM_LineOpening(cam, li);

      totaldist = cam.origindist + FixedMul(cam.attackrange, in->frac);

      if(cam.openrange <= 0)
         return false;

      if(li->frontsector->floorheight != li->backsector->floorheight)
      {
         slope = FixedDiv(cam.openbottom - cam.sightzstart, totaldist);
         if(slope > cam.bottomslope)
            cam.bottomslope = slope;
      }

      if(li->frontsector->ceilingheight != li->backsector->ceilingheight)
      {
         slope = FixedDiv(cam.opentop - cam.sightzstart, totaldist);
         if(slope < cam.topslope)
            cam.topslope = slope;
      }

      if(cam.topslope <= cam.bottomslope)
         return false;

      if(li->pflags & PS_PASSABLE)
      {
         camaimparams_t params;
         int newfromid = li->portal->data.link.toid;
         if(newfromid == cam.fromid)   // not going anywhere
            return true;

         const camaimparams_t *prev = cam.aimparams->prev;
         while(prev)
         {
            if(prev->cgroupid == newfromid)
               return true;
            prev = prev->prev;
         }

         const linkoffset_t *link = P_GetLinkIfExists(cam.fromid, newfromid);

         params.angle = cam.aimparams->angle;
         params.cgroupid = newfromid;
         params.cheight = cam.aimparams->cheight;
         params.cx = cam.aimparams->cx + FixedMul(cam.trace.dx, in->frac);
         params.cy = cam.aimparams->cy + FixedMul(cam.trace.dy, in->frac);
         params.cz = cam.aimparams->cz;
         // this is the distance from the next point, not total.
         params.distance = cam.aimparams->distance 
            - FixedMul(cam.aimparams->distance, in->frac);
         params.mask = cam.aimparams->mask;
         params.pitch = cam.aimparams->pitch;
         params.prev = cam.aimparams;
         params.source = cam.aimparams->source;

         if(link)
         {
            params.cx += link->x;
            params.cy += link->y;
         }

         // For autoaim, we'll use the distance so far, not the fraction so far
         cam.aimslope = CAM_AimLineAttack(params, &cam.linetarget, totaldist, 
            cam.bottomslope, cam.topslope);
         return false;
      }

      return true;
   }
   else
   {
      Mobj *th = in->d.thing;
      fixed_t thingtopslope, thingbottomslope;
      // tests already passed when populating the intercepts array
      totaldist = cam.origindist + FixedMul(cam.attackrange, in->frac);
      thingtopslope = FixedDiv(th->z + th->height - cam.sightzstart, totaldist);

      if(thingtopslope < cam.bottomslope)
         return true; // shot over the thing

      thingbottomslope = FixedDiv(th->z - cam.sightzstart, totaldist);
      if(thingbottomslope > cam.topslope)
         return true; // shot under the thing

      // this thing can be hit!
      if(thingtopslope > cam.topslope)
         thingtopslope = cam.topslope;

      if(thingbottomslope < cam.bottomslope)
         thingbottomslope = cam.bottomslope;

      cam.aimslope = (thingtopslope + thingbottomslope) / 2;
      cam.linetarget = th;

      return false;
   }
}

static bool CAM_SightCheckLine(CamSight &cam, int linenum)
{
   line_t *ld = &lines[linenum];
   int s1, s2;
   divline_t dl;

   cam.validlines[linenum >> 3] |= 1 << (linenum & 7);

   s1 = P_PointOnDivlineSide(ld->v1->x, ld->v1->y, &cam.trace);
   s2 = P_PointOnDivlineSide(ld->v2->x, ld->v2->y, &cam.trace);
   if(s1 == s2)
      return true; // line isn't crossed

   P_MakeDivline(ld, &dl);
   s1 = P_PointOnDivlineSide(cam.trace.x, cam.trace.y, &dl);
   s2 = P_PointOnDivlineSide(cam.trace.x + cam.trace.dx, 
                             cam.trace.y + cam.trace.dy, &dl);
   if(s1 == s2)
      return true; // line isn't crossed

   // Early outs are only possible if we haven't crossed a portal block
   // ioanch 20151230: aim cams never quit
   if(!cam.isAim && !cam.hitpblock) 
   {
      // try to early out the check
      if(!ld->backsector)
         return false; // stop checking

      // haleyjd: block-all lines block sight
      if(ld->extflags & EX_ML_BLOCKALL)
         return false; // can't see through it
   }

   // store the line for later intersection testing
   intercept_t &inter = cam.intercepts.addNew();
   inter.isaline = true;
   inter.d.line = ld;

   // if this is a passable portal line, remember we just added it
   // ioanch 20151229: also check sectors
   const sector_t *fsec = ld->frontsector, *bsec = ld->backsector;
   if(ld->pflags & PS_PASSABLE 
      || (fsec && (fsec->c_pflags & PS_PASSABLE || fsec->f_pflags & PS_PASSABLE))
      || (bsec && (bsec->c_pflags & PS_PASSABLE || bsec->f_pflags & PS_PASSABLE)))
   {
      cam.addedportal = true;
   }

   return true;
}

//
// CAM_SightBlockThingsIterator
//
// ioanch 20151230: for things
//
static bool CAM_SightBlockThingsIterator(CamSight &cam, int x, int y)
{
   if(cam.isAim && (x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight))
      return true;

   Mobj *thing = blocklinks[y * bmapwidth + x];
   for(; thing; thing = thing->bnext)
   {
      // reject them here
      if(!(thing->flags & MF_SHOOTABLE) || thing == cam.source)
         continue;
      // reject friends (except players) also here
      if(thing->flags & cam.source->flags & cam.aimflagsmask && !thing->player)
         continue;

      fixed_t   x1, y1;
      fixed_t   x2, y2;
      int       s1, s2;
      divline_t dl;
      fixed_t   frac;

      // check a corner to corner crossection for hit
      if((trace.dl.dx ^ trace.dl.dy) > 0)
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

      s1 = P_PointOnDivlineSide(x1, y1, &cam.trace);
      s2 = P_PointOnDivlineSide(x2, y2, &cam.trace);

      if(s1 == s2)
         continue;

      dl.x  = x1;
      dl.y  = y1;
      dl.dx = x2 - x1;
      dl.dy = y2 - y1;

      frac = P_InterceptVector(&cam.trace, &dl);

      if(frac < 0)
         continue;                // behind source

      intercept_t &inter = cam.intercepts.addNew();
      inter.frac = frac;
      inter.isaline = false;
      inter.d.thing = thing;
   }
   return true;
}

//
// CAM_SightBlockLinesIterator
//
static bool CAM_SightBlockLinesIterator(CamSight &cam, int x, int y)
{
   // ioanch 20151231: make sure to block here for aim sighting
   if(cam.isAim && (x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight))
      return true;

   int  offset;
   int *list;
   DLListItem<polymaplink_t> *plink;

   offset = y * bmapwidth + x;

   // haleyjd 05/17/13: check portalmap; once we enter a cell with
   // a line portal in it, we can't short-circuit any further and must
   // build a full intercepts list.
   // ioanch 20151229: don't just check for line portals, also consider 
   // floor/ceiling
   if(portalmap[offset])
      cam.hitpblock = true;

   // Check polyobjects first
   // haleyjd 02/22/06: consider polyobject lines
   plink = polyblocklinks[offset];

   while(plink)
   {
      polyobj_t *po = (*plink)->po;
      int polynum = po - PolyObjects;

       // if polyobj hasn't been checked
      if(!(cam.validpolys[polynum >> 3] & (1 << (polynum & 7))))
      {
         cam.validpolys[polynum >> 3] |= 1 << (polynum & 7);
         
         for(int i = 0; i < po->numLines; ++i)
         {
            int linenum = po->lines[i] - lines;

            if(cam.validlines[linenum >> 3] & (1 << (linenum & 7)))
               continue; // line has already been checked

            if(!CAM_SightCheckLine(cam, po->lines[i] - lines))
               return false;
         }
      }
      plink = plink->dllNext;
   }


   offset = *(blockmap + offset);
   list = blockmaplump + offset;

   // skip 0 starting delimiter
   ++list;

   for(; *list != -1; list++)
   {
      int linenum = *list;
      
      if(linenum >= numlines)
         continue;

      if(cam.validlines[linenum >> 3] & (1 << (linenum & 7)))
         continue; // line has already been checked

      if(!CAM_SightCheckLine(cam, linenum))
         return false;
   }

   // haleyjd 08/20/13: optimization
   // we can go back to short-circuiting in future blockmap cells if we haven't 
   // actually intercepted any portal lines yet.
   if(!cam.addedportal)
      cam.hitpblock = false;

   return true; // everything was checked
}

//
// CAM_SightTraverseIntercepts
//
// Returns true if the traverser function returns true for all lines
//
static bool CAM_SightTraverseIntercepts(CamSight &cam)
{
   size_t    count;
   fixed_t   dist;
   divline_t dl;
   PODCollection<intercept_t>::iterator scan, end, in;

   count = cam.intercepts.getLength();
   end   = cam.intercepts.end();

   //
   // calculate intercept distance
   //
   for(scan = cam.intercepts.begin(); scan < end; scan++)
   {
      if(!scan->isaline)
         continue;   // ioanch 20151230: only lines need this treatment
      P_MakeDivline(scan->d.line, &dl);
      scan->frac = P_InterceptVector(&cam.trace, &dl);
   }
	
   //
   // go through in order
   //	
   in = NULL; // shut up compiler warning

   // ioanch 20151230: support autoaim variant
   auto func = cam.isAim ? CAM_aimTraverse : CAM_SightTraverse;
	
   while(count--)
   {
      dist = D_MAXINT;

      for(scan = cam.intercepts.begin(); scan < end; scan++)
      {
         if(scan->frac < dist)
         {
            dist = scan->frac;
            in   = scan;
         }
      }

      if(in)
      {
         if(!func(cam, in))
            return false; // don't bother going farther

         in->frac = D_MAXINT;
      }
   }

   return true; // everything was traversed
}

//
// CAM_SightPathTraverse
//
// Traces a line from x1,y1 to x2,y2, calling the traverser function for each
// Returns true if the traverser function returns true for all lines
//
static bool CAM_SightPathTraverse(CamSight &cam)
{
   fixed_t xt1, yt1, xt2, yt2;
   fixed_t xstep, ystep;
   fixed_t partialx, partialy;
   fixed_t xintercept, yintercept;
   int     mapx, mapy, mapxstep, mapystep;
		
   if(((cam.cx - bmaporgx) & (MAPBLOCKSIZE - 1)) == 0)
      cam.cx += FRACUNIT; // don't side exactly on a line
   
   if(((cam.cy - bmaporgy) & (MAPBLOCKSIZE - 1)) == 0)
      cam.cy += FRACUNIT; // don't side exactly on a line

   cam.trace.x  = cam.cx;
   cam.trace.y  = cam.cy;
   cam.trace.dx = cam.tx - cam.cx;
   cam.trace.dy = cam.ty - cam.cy;

   cam.cx -= bmaporgx;
   cam.cy -= bmaporgy;
   xt1 = cam.cx >> MAPBLOCKSHIFT;
   yt1 = cam.cy >> MAPBLOCKSHIFT;

   cam.tx -= bmaporgx;
   cam.ty -= bmaporgy;
   xt2 = cam.tx >> MAPBLOCKSHIFT;
   yt2 = cam.ty >> MAPBLOCKSHIFT;

   // points should never be out of bounds, but check once instead of
   // each block
   // ioanch 20151231: only for sight
   if(!cam.isAim &&
      (xt1 < 0 || yt1 < 0 || xt1 >= bmapwidth || yt1 >= bmapheight ||
      xt2 < 0 || yt2 < 0 || xt2 >= bmapwidth || yt2 >= bmapheight))
      return false;

   if(xt2 > xt1)
   {
      mapxstep = 1;
      partialx = FRACUNIT - ((cam.cx >> MAPBTOFRAC) & (FRACUNIT - 1));
      ystep    = FixedDiv(cam.ty - cam.cy, abs(cam.tx - cam.cx));
   }
   else if(xt2 < xt1)
   {
      mapxstep = -1;
      partialx = (cam.cx >> MAPBTOFRAC) & (FRACUNIT - 1);
      ystep    = FixedDiv(cam.ty - cam.cy, abs(cam.tx - cam.cx));
   }
   else
   {
      mapxstep = 0;
      partialx = FRACUNIT;
      ystep    = 256*FRACUNIT;
   }	

   yintercept = (cam.cy >> MAPBTOFRAC) + FixedMul(partialx, ystep);
	
   if(yt2 > yt1)
   {
      mapystep = 1;
      partialy = FRACUNIT - ((cam.cy >> MAPBTOFRAC) & (FRACUNIT - 1));
      xstep    = FixedDiv(cam.tx - cam.cx, abs(cam.ty - cam.cy));
   }
   else if(yt2 < yt1)
   {
      mapystep = -1;
      partialy = (cam.cy >> MAPBTOFRAC) & (FRACUNIT - 1);
      xstep    = FixedDiv(cam.tx - cam.cx, abs(cam.ty - cam.cy));
   }
   else
   {
      mapystep = 0;
      partialy = FRACUNIT;
      xstep    = 256*FRACUNIT;
   }	

   xintercept = (cam.cx >> MAPBTOFRAC) + FixedMul(partialy, xstep);

   // From ZDoom (usable under ZDoom code license):
   // [RH] Fix for traces that pass only through blockmap corners. In that case,
   // xintercept and yintercept can both be set ahead of mapx and mapy, so the
   // for loop would never advance anywhere.

   if(abs(xstep) == FRACUNIT && abs(ystep) == FRACUNIT)
   {
      if(ystep < 0)
         partialx = FRACUNIT - partialx;
      if(xstep < 0)
         partialy = FRACUNIT - partialy;
      if(partialx == partialy)
      {
         xintercept = xt1 << FRACBITS;
         yintercept = yt1 << FRACBITS;
      }
   }
	
   // step through map blocks
   // Count is present to prevent a round off error from skipping the break
   mapx = xt1;
   mapy = yt1;
	
   for(int count = 0; count < 100; count++)
   {
      if(!CAM_SightBlockLinesIterator(cam, mapx, mapy))
         return false;	// early out (ioanch: not for aim)
      if(cam.isAim && !CAM_SightBlockThingsIterator(cam, mapx, mapy))
         return false;  // ioanch 20151230: aim also looks for a thing

		
      if((mapxstep | mapystep) == 0)
         break;

#if 1
      // From ZDoom (usable under the ZDoom code license):
      // This is the fix for the "Anywhere Moo" bug, which caused monsters to
      // occasionally see the player through an arbitrary number of walls in
      // Doom 1.2, and persisted into Heretic, Hexen, and some versions of 
      // ZDoom.
      switch((((yintercept >> FRACBITS) == mapy) << 1) | 
              ((xintercept >> FRACBITS) == mapx))
      {
      case 0: 
         // Neither xintercept nor yintercept match!
         // Continuing won't make things any better, so we might as well stop.
         count = 100;
         break;
      case 1: 
         // xintercept matches
         xintercept += xstep;
         mapy += mapystep;
         if(mapy == yt2)
            mapystep = 0;
         break;
      case 2: 
         // yintercept matches
         yintercept += ystep;
         mapx += mapxstep;
         if(mapx == xt2)
            mapxstep = 0;
         break;
      case 3: 
         // xintercept and yintercept both match
         // The trace is exiting a block through its corner. Not only does the
         // block being entered need to be checked (which will happen when this
         // loop continues), but the other two blocks adjacent to the corner
         // also need to be checked.
         if(!CAM_SightBlockLinesIterator(cam, mapx + mapxstep, mapy) ||
            !CAM_SightBlockLinesIterator(cam, mapx, mapy + mapystep))
         {
            return false;
         }
         // ioanch 20151230: autoaim support
         if(cam.isAim 
            && (!CAM_SightBlockThingsIterator(cam, mapx + mapxstep, mapy)
            || !CAM_SightBlockThingsIterator(cam, mapx, mapy + mapystep)))
         {
            return false;
         }
         xintercept += xstep;
         yintercept += ystep;
         mapx += mapxstep;
         mapy += mapystep;
         if(mapx == xt2)
            mapxstep = 0;
         if(mapy == yt2)
            mapystep = 0;
         break;
      }
#else
      // Original code - this fails to account for all cases.
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
#endif
   }

   //
   // couldn't early out, so go through the sorted list
   //
   return CAM_SightTraverseIntercepts(cam);
}

//
// CAM_CheckSight
//
// Returns true if a straight line between the camera location and a
// thing's coordinates is unobstructed.
//
// ioanch 20151229: line portal sight fix
// Also added bottomslope and topslope pre-setting
//
static bool CAM_CheckSight(const camsightparams_t &params, fixed_t originfrac,
                           fixed_t bottomslope, fixed_t topslope)
{
   sector_t *csec, *tsec;
   int s1, s2, pnum;
   bool result = false;
   linkoffset_t *link = NULL;

   // Camera and target are not in same group?
   if(params.cgroupid != params.tgroupid)
   {
      // is there a link between these groups?
      // if so, ignore reject
      link = P_GetLinkIfExists(params.cgroupid, params.tgroupid);
   }

   //
   // check for trivial rejection
   //
   s1   = (csec = R_PointInSubsector(params.cx, params.cy)->sector) - sectors;
   s2   = (tsec = R_PointInSubsector(params.tx, params.ty)->sector) - sectors;
   pnum = s1 * numsectors + s2;
	
   if(link || !(rejectmatrix[pnum >> 3] & (1 << (pnum & 7))))
   {
      // killough 4/19/98: make fake floors and ceilings block monster view
      if((csec->heightsec != -1 &&
          ((params.cz + params.cheight <= sectors[csec->heightsec].floorheight &&
            params.tz >= sectors[csec->heightsec].floorheight) ||
           (params.cz >= sectors[csec->heightsec].ceilingheight &&
            params.tz + params.cheight <= sectors[csec->heightsec].ceilingheight)))
         ||
         (tsec->heightsec != -1 &&
          ((params.tz + params.theight <= sectors[tsec->heightsec].floorheight &&
            params.cz >= sectors[tsec->heightsec].floorheight) ||
           (params.tz >= sectors[tsec->heightsec].ceilingheight &&
            params.cz + params.theight <= sectors[tsec->heightsec].ceilingheight))))
         return false;

      //
      // check precisely
      //
      CamSight newCam(params, originfrac, bottomslope, topslope);

      // if there is a valid portal link, adjust the target's coordinates now
      // so that we trace in the proper direction given the current link

      // ioanch 20151229: store displaced target coordinates because newCam.tx
      // and .ty will be modified internally
      fixed_t stx, sty;
      if(link)
      {
         newCam.tx -= link->x;
         newCam.ty -= link->y;
      }
      stx = newCam.tx;
      sty = newCam.ty;

      result = CAM_SightPathTraverse(newCam);

      // if we broke out due to doing a portal recursion, replace the local
      // result with the result of the portal operation
      if(newCam.portalexit)
         result = newCam.portalresult;

      // ioanch 20151229: modified condition: if found but
      // and discovered to belong to different group IDs, scan the floor or
      // ceiling portals
      else if(result && newCam.fromid != newCam.toid)
      {
         result = false;   // reset it
         // didn't hit a portal but in different groups; not visible.
         // ioanch 20151229: this can happen if the final sector is a portal floor or ceiling
         if(link)
         {
            // reload this variable
            tsec = R_PointInSubsector(stx, sty)->sector;
            // test this final thing
            if(CAM_checkPortalSector(newCam, tsec, FRACUNIT, FRACUNIT))
            {
               result = true;
            }
         }
      }
   }

   return result;
}
bool CAM_CheckSight(const camsightparams_t &params)
{
   return CAM_CheckSight(params, 0, D_MAXINT, D_MAXINT);
}

///////////////////////////////////////////////////////////////////////////////
// ioanch 20151230: reentrant aim attack

//
// CAM_AimLineAttack
//
// Reentrant version of P_AimLineAttack. Uses the same path traverser as
// CAM_CheckSight
//
static fixed_t CAM_AimLineAttack(const camaimparams_t &params, Mobj **outTarget, 
                          fixed_t origindist, fixed_t bottomslope, 
                          fixed_t topslope)
{
   fixed_t lookslope = params.pitch == 0 ? 
      0 : finetangent[(ANG90 - params.pitch) >> ANGLETOFINESHIFT];

   CamSight newCam(params, origindist, bottomslope, topslope);

   CAM_SightPathTraverse(newCam);

   if(outTarget)
      *outTarget = newCam.linetarget;

   return newCam.linetarget ? newCam.aimslope : lookslope;
}
fixed_t CAM_AimLineAttack(const camaimparams_t &params, Mobj **outTarget)
{
   return CAM_AimLineAttack(params, outTarget, 0, D_MAXINT, D_MAXINT);
}

// EOF

