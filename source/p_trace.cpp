// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 James Haley, Stephen McGranahan, et al.
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//
// Tracers and path traversal, moved here from p_map.c and p_maputl.c.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "d_gi.h"
#include "doomstat.h"
#include "e_exdata.h"
#include "p_clipen.h"
#include "p_mapcontext.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_inter.h"
#include "p_setup.h"
#include "p_skin.h"
#include "p_spec.h"
#include "p_traceengine.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_state.h"
#include "r_pcheck.h"
#include "s_sound.h"

// ----------------------------------------------------------------------------
// TracerContext

TracerContext::TracerContext() 
{
   from = NULL;
}


TracerContext::~TracerContext()
{
   done();
}


void TracerContext::setEngine(TracerEngine *te)
{
   from = te;
}


void TracerContext::done()
{
   if(from != NULL)
   {
      TracerEngine *te = from;
      
      from = NULL;
      te->releaseContext(this);
   }
}


//=============================================================================
//
// Line Attacks
//

static int aim_flags_mask; // killough 8/2/98: for more intelligent autoaiming


//=============================================================================
//
// Aiming
//

//
// P_AimAtThing
//
// Code to handle aiming tracers at things.
// haleyjd 03/21/08
//
static bool P_AimAtThing(intercept_t *in, TracerContext *tc)
{
   Mobj *th = in->d.thing;
   fixed_t thingtopslope, thingbottomslope, dist;
   
   if(th == tc->shootthing)
      return true;    // can't shoot self
   
   if(!(th->flags & MF_SHOOTABLE))
      return true;    // corpse or something
   
   // killough 7/19/98, 8/2/98:
   // friends don't aim at friends (except players), at least not first
   if(th->flags & tc->shootthing->flags & aim_flags_mask && !th->player)
      return true;
   
   // check angles to see if the thing can be aimed at
   
   // SoM: added distance so the slopes are calculated from the origin of the 
   // aiming tracer to keep the slopes consistent.
   if(demo_version >= 333)
      dist = FixedMul(tc->attackrange, in->frac) + tc->movefrac;
   else
      dist = FixedMul(tc->attackrange, in->frac);   

   thingtopslope = FixedDiv(th->z + th->height - tc->originz , dist);
   
   if(thingtopslope < tc->bottomslope)
      return true;    // shot over the thing
   
   thingbottomslope = FixedDiv(th->z - tc->originz, dist);
   
   if(thingbottomslope > tc->topslope)
      return true;    // shot under the thing
   
   // this thing can be hit!
   
   if(thingtopslope > tc->topslope)
      thingtopslope = tc->topslope;
   
   if(thingbottomslope < tc->bottomslope)
      thingbottomslope = tc->bottomslope;
   
   tc->aimslope = (thingtopslope + thingbottomslope) / 2;
   tc->linetarget = th;

   return false;   // don't go any farther
}
   
//
// PTR_AimTraverse
//
// Sets linetarget and aimslope when a target is aimed at.
//
static bool PTR_AimTraverse(intercept_t *in, TracerContext *tc)
{
   fixed_t slope, dist;
   
   if(in->isaline)
   {
      // shoot a line
      line_t *li = in->d.line;
      open_t opening;
      
      // haleyjd 04/30/11: added 'block everything' lines
      if(!(li->flags & ML_TWOSIDED) || (li->extflags & EX_ML_BLOCKALL))
         return false;   // stop

      // Crosses a two sided line.
      // A two sided line will restrict
      // the possible target ranges.
      
      clip->lineOpening(li, NULL, &opening);
      
      if(opening.bottom >= opening.top)
         return false;   // stop

      dist = FixedMul(tc->attackrange, in->frac);
      
      if(li->frontsector->floorheight != li->backsector->floorheight)
      {
         slope = FixedDiv(opening.bottom - tc->z , dist);
         if(slope > tc->bottomslope)
            tc->bottomslope = slope;
      }

      if(li->frontsector->ceilingheight != li->backsector->ceilingheight)
      {
         slope = FixedDiv(opening.top - tc->z , dist);
         if(slope < tc->topslope)
            tc->topslope = slope;
      }

      if(tc->topslope <= tc->bottomslope)
         return false;   // stop
      
      return true;    // shot continues
   }
   else
   {  
      // shoot a thing
      return P_AimAtThing(in, tc);
   }
}



//=============================================================================
//
// Shooting
//

//
// P_Shoot2SLine
//
// haleyjd 03/13/05: This code checks to see if a bullet is passing
// a two-sided line, isolated out of PTR_ShootTraverse below to keep it
// from becoming too messy. There was a problem with DOOM assuming that
// a bullet had nothing to hit when crossing a 2S line with the same
// floor and ceiling heights on both sides of it, causing line specials
// to be activated inappropriately.
//
// When running with plane shooting, we must ignore the floor/ceiling
// sameness checks and only consider the true position of the bullet
// with respect to the line opening.
//
// Returns true if PTR_ShootTraverse should exit, and false otherwise.
//
static bool P_Shoot2SLine(line_t *li, int side, fixed_t dist, TracerContext *tc, open_t *opening)
{
   // haleyjd: when allowing planes to be shot, we do not care if
   // the sector heights are the same; we must check against the
   // line opening, otherwise lines behind the plane will be activated.
   
   bool floorsame = 
      (li->frontsector->floorheight == li->backsector->floorheight &&
       (demo_version < 333 || comp[comp_planeshoot]));

   bool ceilingsame =
      (li->frontsector->ceilingheight == li->backsector->ceilingheight &&
       (demo_version < 333 || comp[comp_planeshoot]));

   if((floorsame   || FixedDiv(opening->bottom - tc->z , dist) <= tc->aimslope) &&
      (ceilingsame || FixedDiv(opening->top - tc->z , dist) >= tc->aimslope))
   {
      if(li->special && demo_version >= 329 && !comp[comp_planeshoot])
         P_ShootSpecialLine(tc->shootthing, li, side);
      
      return true;      // shot continues
   }

   return false;
}

//
// P_ShotCheck2SLine
//
// Routine to handle the crossing of a 2S line by a shot tracer.
// Returns true if PTR_ShootTraverse should return.
//
static bool P_ShotCheck2SLine(intercept_t *in, line_t *li, int lineside, TracerContext *tc)
{
   fixed_t dist;
   bool ret = false;

   // haleyjd 04/30/11: block-everything lines stop bullets
   if(li->extflags & EX_ML_BLOCKALL)
      return false;

   if(li->flags & ML_TWOSIDED)
   {  
      // crosses a two sided (really 2s) line
      open_t      opening;
      
      clip->lineOpening(li, NULL, &opening);
      
      dist = FixedMul(tc->attackrange, in->frac);
      
      // killough 11/98: simplify
      // haleyjd 03/13/05: fixed bug that activates 2S line specials
      // when shots hit the floor
      if(P_Shoot2SLine(li, lineside, dist, tc, &opening))
         ret = true;
   }

   return ret;
}

//
// P_PuffPosition
//
// Calculates the position for a bullet puff.
// haleyjd 03/21/08
//
static void P_PuffPosition(intercept_t *in, fixed_t *frac, 
                           fixed_t *x, fixed_t *y, fixed_t *z,
                           fixed_t dist, TracerContext *tc)
{
   *frac = in->frac - FixedDiv(dist, tc->attackrange);
   *x = tc->divline.x + FixedMul(tc->divline.dx, *frac);
   *y = tc->divline.y + FixedMul(tc->divline.dy, *frac);
   *z = tc->z + FixedMul(tc->aimslope, FixedMul(*frac, tc->attackrange));
}

//
// P_ShootSky
//
// Returns true if PTR_ShootTraverse should return (ie, shot hits sky).
//
static bool P_ShootSky(line_t *li, fixed_t z)
{
   sector_t *fs = li->frontsector, *bs = li->backsector;

   // don't shoot the sky!
   // don't shoot ceiling portals either

   if(fs->intflags & SIF_SKY || fs->c_portal)
   {      
      if(z > fs->ceilingheight)
         return true;
      
      // it's a sky hack wall
      // fix bullet-eaters -- killough:
      if(bs && bs->intflags & SIF_SKY)
      {
         if(demo_compatibility || bs->ceilingheight < z)
            return true;
      }
   }

   return false;
}

//
// P_ShootThing
//
// Routine to handle shooting a thing.
// haleyjd 03/21/08
//
static bool P_ShootThing(intercept_t *in, TracerContext *tc)
{
   fixed_t x, y, z, frac, dist, thingtopslope, thingbottomslope;
   Mobj *th = in->d.thing;
   
   if(th == tc->shootthing)
      return true;  // can't shoot self
   
   if(!(th->flags & MF_SHOOTABLE))
      return true;  // corpse or something
   
   // haleyjd: don't let players use melee attacks on ghosts
   if((th->flags3 & MF3_GHOST) && 
      tc->shootthing->player &&
      P_GetReadyWeapon(tc->shootthing->player)->flags & WPF_NOHITGHOSTS)
      return true;
   
   // check angles to see if the thing can be aimed at
   
   dist = FixedMul(tc->attackrange, in->frac);
   thingtopslope = FixedDiv(th->z + th->height - tc->z, dist);
   
   if(thingtopslope < tc->aimslope)
      return true;  // shot over the thing
   
   thingbottomslope = FixedDiv(th->z - tc->z, dist);
   
   if(thingbottomslope > tc->aimslope)
      return true;  // shot under the thing
   
   // hit thing
   // position a bit closer
   
   P_PuffPosition(in, &frac, &x, &y, &z, 10*FRACUNIT, tc);
   
   // Spawn bullet puffs or blood spots,
   // depending on target type. -- haleyjd: and status flags!
   if(th->flags & MF_NOBLOOD || 
      th->flags2 & (MF2_INVULNERABLE | MF2_DORMANT))
   {
      P_SpawnPuff(x, y, z, 
         P_PointToAngle(0, 0, tc->divline.dx, tc->divline.dy) - ANG180,
         2, true, tc->attackrange);
   }
   else
   {
      P_SpawnBlood(x, y, z,
         P_PointToAngle(0, 0, tc->divline.dx, tc->divline.dy) - ANG180,
         tc->la_damage, th);
   }
   
   if(tc->la_damage)
   {
      P_DamageMobj(th, tc->shootthing, tc->shootthing, tc->la_damage, 
                   tc->shootthing->info->mod);
   }

   // don't go any further
   return false;
}

//
// PTR_ShootTraverseComp
//
// Compatibility codepath for shot traversal.
// This is called if (demo_version < 329 || comp[comp_planeshoot] == true)
//
// haleyjd 03/21/08
//
static bool PTR_ShootTraverseComp(intercept_t *in, TracerContext *tc)
{
   fixed_t x, y, z, frac;
   
   if(in->isaline)
   {
      line_t *li = in->d.line;

      // haleyjd 03/13/05: move up point on line side check to here
      int lineside = P_PointOnLineSide(tc->shootthing->x, tc->shootthing->y, li);
      
      if(li->special)
         P_ShootSpecialLine(tc->shootthing, li, lineside);

      // shot crosses a 2S line?
      bool shootcheck = P_ShotCheck2SLine(in, li, lineside, tc);
      
      if(shootcheck)
         return true;
      
      // hit line
      // position a bit closer

      P_PuffPosition(in, &frac, &x, &y, &z, 4*FRACUNIT, tc);

      // don't hit the sky
      if(P_ShootSky(li, z))
         return false;
            
      // Spawn bullet puffs.
      P_SpawnPuff(x, y, z, 
                  P_PointToAngle(0, 0, li->dx, li->dy) - ANG90,
                  2, true, tc->attackrange);
      
      // don't go any farther
      return false;
   }
   else
   {
      // shoot a thing
      return P_ShootThing(in, tc);
   }
}

//
// PTR_ShootTraverse
//
// haleyjd 11/21/01: fixed by SoM to allow bullets to puff on the
// floors and ceilings rather than along the line which they actually
// intersected far below or above the ceiling.
//
static bool PTR_ShootTraverse(intercept_t *in, TracerContext *tc)
{
   fixed_t x, y, z, frac, zdiff;
   bool hitplane = false; // SoM: Remember if the bullet hit a plane.
   int updown = 2; // haleyjd 05/02: particle puff z dist correction
   sector_t *sidesector;
   
   if(in->isaline)
   {
      line_t *li = in->d.line;

      // haleyjd 03/13/05: move up point on line side check to here
      // SoM: this was causing some trouble when shooting through linked 
      // portals because although trace.x and trace.y are offset when 
      // the shot travels through a portal, shootthing->x and 
      // shootthing->y are NOT. Demo comped just in case.
      int lineside =
         (demo_version >= 333) ?
            P_PointOnLineSide(tc->divline.x, tc->divline.y, li) :
            P_PointOnLineSide(tc->shootthing->x, tc->shootthing->y, li);
      
      // SoM: Shouldn't be called until A: we know the bullet passed or
      // B: We know it didn't hit a plane first
      //if(li->special && (demo_version < 329 || comp[comp_planeshoot]))
      //   P_ShootSpecialLine(shootthing, li, lineside);
     
      if(P_ShotCheck2SLine(in, li, lineside, tc))
         return true;

      // hit line
      // position a bit closer

      P_PuffPosition(in, &frac, &x, &y, &z, 4*FRACUNIT, tc);
      
      // SoM: Check for colision with a plane.
      sidesector = lineside ? li->backsector : li->frontsector;
      
      // SoM: If we are in no-clip and are shooting on the backside of a
      // 1s line, don't crash!
      if(sidesector)
      {
         if(z < sidesector->floorheight)
         {
            fixed_t pfrac = FixedDiv(sidesector->floorheight - tc->z, 
                                     tc->aimslope);
            
            // SoM: don't check for portals here anymore
            if(sidesector->floorpic == skyflatnum ||
               sidesector->floorpic == sky2flatnum) 
               return false;
            
            if(demo_version < 333)
            {
               zdiff = FixedDiv(D_abs(z - sidesector->floorheight),
                                D_abs(z - tc->originz));
               x += FixedMul(tc->divline.x - x, zdiff);
               y += FixedMul(tc->divline.y - y, zdiff);
            }
            else
            {
               x = tc->divline.x + FixedMul(tc->cos, pfrac);
               y = tc->divline.y + FixedMul(tc->sin, pfrac);
            }

            z = sidesector->floorheight;
            hitplane = true;
            updown = 0; // haleyjd
         }
         else if(z > sidesector->ceilingheight)
         {
            fixed_t pfrac = FixedDiv(sidesector->ceilingheight - tc->z, tc->aimslope);
            if(sidesector->intflags & SIF_SKY) // SoM
               return false;
            
            if(demo_version < 333)
            {
               zdiff = FixedDiv(D_abs(z - sidesector->ceilingheight),
                  D_abs(z - tc->originz));
               x += FixedMul(tc->divline.x - x, zdiff);
               y += FixedMul(tc->divline.y - y, zdiff);
            }
            else
            {
               x = tc->divline.x + FixedMul(tc->cos, pfrac);
               y = tc->divline.y + FixedMul(tc->sin, pfrac);
            }
            
            z = sidesector->ceilingheight;
            hitplane = true;
            updown = 1; // haleyjd
         }
      }
      
      if(!hitplane && li->special)
         P_ShootSpecialLine(tc->shootthing, li, lineside);

      // don't hit the sky
      if(P_ShootSky(li, z))
         return false;
      
      // don't shoot portal lines
      if(!hitplane && li->portal)
         return false;
      
      // Spawn bullet puffs.
      P_SpawnPuff(x, y, z, 
                  P_PointToAngle(0, 0, li->dx, li->dy) - ANG90,
                  updown, true, tc->attackrange);
      
      // don't go any farther
      
      return false;
   }
   else
   {
      // shoot a thing
      return P_ShootThing(in, tc);
   }
}


// ----------------------------------------------------------------------------
// DoomTraceEngine

DoomTraceEngine::DoomTraceEngine() 
{
   this->tracec.setEngine(this);
}

//
// P_AimLineAttack
//
// killough 8/2/98: add mask parameter, which, if set to MF_FRIEND,
// makes autoaiming skip past friends.
//
fixed_t DoomTraceEngine::aimLineAttack(Mobj *t1, angle_t angle, fixed_t distance, int mask, TracerContext *tc)
{
   fixed_t x2, y2;
   fixed_t lookslope = 0;
   fixed_t pitch = 0;
   
   angle >>= ANGLETOFINESHIFT;
   tc->shootthing = t1;
   
   x2 = t1->x + (distance>>FRACBITS)*(tc->cos = finecosine[angle]);
   y2 = t1->y + (distance>>FRACBITS)*(tc->sin = finesine[angle]);
   tc->originz = tc->z = t1->z + (t1->height>>1) + 8*FRACUNIT;
   tc->movefrac = 0;

   // haleyjd 10/08/06: this should be gotten from t1->player, not 
   // players[displayplayer]. Also, if it's zero, use the old
   // code in all cases to avoid roundoff error.
   if(t1->player)
      pitch = t1->player->pitch;
   
   // can't shoot outside view angles

   if(pitch == 0 || demo_version < 333)
   {
      tc->topslope    =  100*FRACUNIT/160;
      tc->bottomslope = -100*FRACUNIT/160;
   }
   else
   {
      // haleyjd 04/05/05: use proper slope range for look slope
      fixed_t topangle, bottomangle;

      lookslope   = finetangent[(ANG90 - pitch) >> ANGLETOFINESHIFT];

      topangle    = pitch - ANGLE_1*32;
      bottomangle = pitch + ANGLE_1*32;

      tc->topslope    = finetangent[(ANG90 -    topangle) >> ANGLETOFINESHIFT];
      tc->bottomslope = finetangent[(ANG90 - bottomangle) >> ANGLETOFINESHIFT];
   }
   
   tc->attackrange = distance;
   tc->linetarget = NULL;

   // killough 8/2/98: prevent friends from aiming at friends
   aim_flags_mask = mask;
   
   pathTraverse(t1->x, t1->y, x2, y2, PT_ADDLINES|PT_ADDTHINGS, PTR_AimTraverse, tc);
  
   return tc->linetarget ? tc->aimslope : lookslope;
}

//
// P_LineAttack
//
// If damage == 0, it is just a test trace that will leave linetarget set.
//
void DoomTraceEngine::lineAttack(Mobj *t1, angle_t angle, fixed_t distance,
                                 fixed_t slope, int damage)
{
   fixed_t x2, y2;
   
   TracerContext *tc = getContext();
   
   angle >>= ANGLETOFINESHIFT;
   
   tc->shootthing = t1;
   tc->la_damage = damage;
   x2 = t1->x + (distance >> FRACBITS) * (tc->cos = finecosine[angle]);
   y2 = t1->y + (distance >> FRACBITS) * (tc->sin = finesine[angle]);
   
   tc->originz = tc->z = t1->z - t1->floorclip + (t1->height>>1) + 8*FRACUNIT;
   tc->attackrange = distance;
   tc->aimslope = slope;
   tc->movefrac = 0;

   pathTraverse(t1->x, t1->y, x2, y2, PT_ADDLINES|PT_ADDTHINGS, 
                  (demo_version < 329 || comp[comp_planeshoot]) ?
                      PTR_ShootTraverseComp : PTR_ShootTraverse, tc);
                      
   releaseContext(tc);
}

//
// USE LINES
//

static Mobj *usething;

// killough 11/98: reformatted
// haleyjd  09/02: reformatted again.

static bool PTR_UseTraverse(intercept_t *in, TracerContext *tc)
{
   if(in->d.line->special)
   {
      P_UseSpecialLine(usething, in->d.line,
         P_PointOnLineSide(tc->originx, tc->originy, in->d.line)==1);

      //WAS can't use for than one special line in a row
      //jff 3/21/98 NOW multiple use allowed with enabling line flag
      return (!demo_compatibility && in->d.line->flags & ML_PASSUSE);
   }
   else
   {
      open_t opening;
      if(in->d.line->extflags & EX_ML_BLOCKALL) // haleyjd 04/30/11
         opening.range = 0;
      else
      {
         clip->lineOpening(in->d.line, NULL, &opening);
      }

      if(opening.range <= 0)
      {
         // can't use through a wall
         S_StartSound(usething, GameModeInfo->playerSounds[sk_noway]);
         return false;
      }

      // not a special line, but keep checking
      return true;
   }
}

//
// PTR_NoWayTraverse
//
// Returns false if a "oof" sound should be made because of a blocking
// linedef. Makes 2s middles which are impassable, as well as 2s uppers
// and lowers which block the player, cause the sound effect when the
// player tries to activate them. Specials are excluded, although it is
// assumed that all special linedefs within reach have been considered
// and rejected already (see P_UseLines).
//
// by Lee Killough
//
static bool PTR_NoWayTraverse(intercept_t *in, TracerContext *tc)
{
   line_t *ld = in->d.line;                       // This linedef

   if(ld->special) // Ignore specials
      return true;

   if(ld->flags & ML_BLOCKING) // Always blocking
      return false;

   // Find openings
   open_t opening;
   clip->lineOpening(ld, NULL, &opening);

   return 
      !(opening.range  <= 0 ||                            // No opening
        opening.bottom > usething->z + 24 * FRACUNIT ||   // Too high, it blocks
        opening.top    < usething->z + usething->height); // Too low, it blocks
}

//
// P_UseLines
//
// Looks for special lines in front of the player to activate.
//
void DoomTraceEngine::useLines(player_t *player)
{
   fixed_t x1, y1, x2, y2;
   int angle;
   
   TracerContext *tc = getContext();
   
   usething = player->mo;
   
   angle = player->mo->angle >> ANGLETOFINESHIFT;
   
   x1 = player->mo->x;
   y1 = player->mo->y;
   x2 = x1 + (USERANGE>>FRACBITS)*(tc->cos = finecosine[angle]);
   y2 = y1 + (USERANGE>>FRACBITS)*(tc->sin = finesine[angle]);

   tc->attackrange = USERANGE;
   tc->movefrac = 0;

   // old code:
   //
   // P_PathTraverse ( x1, y1, x2, y2, PT_ADDLINES, PTR_UseTraverse );
   //
   // This added test makes the "oof" sound work on 2s lines -- killough:
   
   if(pathTraverse(x1, y1, x2, y2, PT_ADDLINES, PTR_UseTraverse, tc))
      if(!pathTraverse(x1, y1, x2, y2, PT_ADDLINES, PTR_NoWayTraverse, tc))
         S_StartSound(usething, GameModeInfo->playerSounds[sk_noway]);
         
   releaseContext(tc);
}

//
// INTERCEPT ROUTINES
//

// 1/11/98 killough: Intercept limit removed
static intercept_t *intercepts, *intercept_p;

// Check for limit and double size if necessary -- killough
static void check_intercept(void)
{
   static size_t num_intercepts;
   size_t offset = intercept_p - intercepts;
   if(offset >= num_intercepts)
   {
      num_intercepts = num_intercepts ? num_intercepts*2 : 128;
      intercepts = erealloc(intercept_t *, intercepts, sizeof(*intercepts)*num_intercepts);
      intercept_p = intercepts + offset;
   }
}


//
// PIT_AddLineIntercepts
//
// Looks for lines in the given block
// that intercept the given trace
// to add to the intercepts list.
//
// A line is crossed if its endpoints
// are on opposite sides of the trace.
//
// killough 5/3/98: reformatted, cleaned up
//
bool PIT_AddLineIntercepts(line_t *ld, MapContext *mc)
{
   TracerContext *tc = dynamic_cast<TracerContext *>(mc);
   int       s1;
   int       s2;
   fixed_t   frac;
   divline_t dl;

   // avoid precision problems with two routines
   if(tc->divline.dx >  FRACUNIT*16 || tc->divline.dy >  FRACUNIT*16 ||
      tc->divline.dx < -FRACUNIT*16 || tc->divline.dy < -FRACUNIT*16)
   {
      s1 = P_PointOnDivlineSide (ld->v1->x, ld->v1->y, &tc->divline);
      s2 = P_PointOnDivlineSide (ld->v2->x, ld->v2->y, &tc->divline);
   }
   else
   {
      s1 = P_PointOnLineSide (tc->divline.x, tc->divline.y, ld);
      s2 = P_PointOnLineSide (tc->divline.x+tc->divline.dx, tc->divline.y+tc->divline.dy, ld);
   }

   if(s1 == s2)
      return true;        // line isn't crossed
   
   // hit the line
   P_MakeDivline(ld, &dl);
   frac = P_InterceptVector(&tc->divline, &dl);
   
   if(frac < 0)
      return true;        // behind source

   check_intercept();    // killough
   
   intercept_p->frac = frac;
   intercept_p->isaline = true;
   intercept_p->d.line = ld;
   intercept_p++;
   
   return true;  // continue
}

//
// PIT_AddThingIntercepts
//
// killough 5/3/98: reformatted, cleaned up
//
bool PIT_AddThingIntercepts(Mobj *thing, MapContext *mc)
{
   TracerContext *tc = dynamic_cast<TracerContext *>(mc);
   
   fixed_t   x1, y1;
   fixed_t   x2, y2;
   int       s1, s2;
   divline_t dl;
   fixed_t   frac;

   // check a corner to corner crossection for hit
   if((tc->divline.dx ^ tc->divline.dy) > 0)
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

   s1 = P_PointOnDivlineSide (x1, y1, &tc->divline);
   s2 = P_PointOnDivlineSide (x2, y2, &tc->divline);
   
   if(s1 == s2)
      return true;                // line isn't crossed

   dl.x = x1;
   dl.y = y1;
   dl.dx = x2-x1;
   dl.dy = y2-y1;
   
   frac = P_InterceptVector (&tc->divline, &dl);
   
   if (frac < 0)
      return true;                // behind source
   
   check_intercept();            // killough
   
   intercept_p->frac = frac;
   intercept_p->isaline = false;
   intercept_p->d.thing = thing;
   intercept_p++;
   
   return true;          // keep going
}

//
// P_TraverseIntercepts
//
// Returns true if the traverser function returns true
// for all lines.
//
// killough 5/3/98: reformatted, cleaned up
//
bool P_TraverseIntercepts(pathfunc_t func, fixed_t maxfrac, TracerContext *tc)
{
   intercept_t *in = NULL;
   int count = intercept_p - intercepts;
   while(count--)
   {
      fixed_t dist = D_MAXINT;
      intercept_t *scan;
      for(scan = intercepts; scan < intercept_p; scan++)
         if(scan->frac < dist)
            dist = (in=scan)->frac;
      if(dist > maxfrac)
         return true;    // checked everything in range

      if(in) // haleyjd: for safety
      {
         if(!func(in, tc))
            return false;           // don't bother going farther
         in->frac = D_MAXINT;
      }
   }
   return true;                  // everything was traversed
}

//
// P_PathTraverse
//
// Traces a line from x1,y1 to x2,y2,
// calling the traverser function for each.
// Returns true if the traverser function returns true
// for all lines.
//
// killough 5/3/98: reformatted, cleaned up
//
bool DoomTraceEngine::pathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                                   int flags, pathfunc_t trav, TracerContext *tc)
{
   fixed_t xt1, yt1;
   fixed_t xt2, yt2;
   fixed_t xstep, ystep;
   fixed_t partial;
   fixed_t xintercept, yintercept;
   int     mapx, mapy;
   int     mapxstep, mapystep;
   int     count;
   // SoM: just a little bit-o-change...
   bool    result;

   validcount++;
   intercept_p = intercepts;
   
   if(!((x1-bmaporgx)&(MAPBLOCKSIZE-1)))
      x1 += FRACUNIT;     // don't side exactly on a line
   
   if(!((y1-bmaporgy)&(MAPBLOCKSIZE-1)))
      y1 += FRACUNIT;     // don't side exactly on a line

   tc->divline.x = tc->originx = x1;
   tc->divline.y = tc->originy = y1;
   tc->divline.dx = x2 - x1;
   tc->divline.dy = y2 - y1;
   
   x1 -= bmaporgx;
   y1 -= bmaporgy;
   xt1 = x1>>MAPBLOCKSHIFT;
   yt1 = y1>>MAPBLOCKSHIFT;
   
   x2 -= bmaporgx;
   y2 -= bmaporgy;
   xt2 = x2>>MAPBLOCKSHIFT;
   yt2 = y2>>MAPBLOCKSHIFT;

   if(xt2 > xt1)
   {
      mapxstep = 1;
      partial = FRACUNIT - ((x1>>MAPBTOFRAC)&(FRACUNIT-1));
      ystep = FixedDiv (y2-y1,D_abs(x2-x1));
   }
   else if(xt2 < xt1)
   {
      mapxstep = -1;
      partial = (x1>>MAPBTOFRAC)&(FRACUNIT-1);
      ystep = FixedDiv (y2-y1,D_abs(x2-x1));
   }
   else
   {
      mapxstep = 0;
      partial = FRACUNIT;
      ystep = 256*FRACUNIT;
   }

   yintercept = (y1 >> MAPBTOFRAC) + FixedMul(partial, ystep);
   
   if(yt2 > yt1)
   {
      mapystep = 1;
      partial = FRACUNIT - ((y1>>MAPBTOFRAC)&(FRACUNIT-1));
      xstep = FixedDiv (x2-x1,D_abs(y2-y1));
   }
   else if(yt2 < yt1)
   {
      mapystep = -1;
      partial = (y1>>MAPBTOFRAC)&(FRACUNIT-1);
      xstep = FixedDiv (x2-x1,D_abs(y2-y1));
   }
   else
   {
      mapystep = 0;
      partial = FRACUNIT;
      xstep = 256*FRACUNIT;
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
         if(!P_BlockLinesIterator(mapx, mapy, PIT_AddLineIntercepts, tc))
            return false; // early out
      }
      
      if(flags & PT_ADDTHINGS)
      {
         if(!P_BlockThingsIterator(mapx, mapy, PIT_AddThingIntercepts, tc))
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
   result = P_TraverseIntercepts(trav, FRACUNIT, tc);

   return result;
}



//
// pathTraverse
//
// This gets/disposes of the clipping context to allow outside code that 
// doesn't need to use it to avoid doing so.
//
bool DoomTraceEngine::pathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                                   int flags, pathfunc_t trav)
{
   TracerContext *tc = getContext();
   bool res = pathTraverse(x1, y1, x2, y2, flags, trav, tc);
   releaseContext(tc);
   return res;
}


// HAX
fixed_t DoomTraceEngine::tracerPuffAttackRange()
{
   return tracec.attackrange;
}

// EOF

