// Emacs style mode select   -*- C -*- vi:sw=3 ts=3:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2008 James Haley, Stephen McGranahan, et al.
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
//
// Tracers and path traversal, moved here from p_map.c and p_maputl.c.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "d_gi.h"
#include "doomstat.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_inter.h"
#include "p_setup.h"
#include "p_spec.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_state.h"
#include "r_pcheck.h"
#include "s_sound.h"

// [CG] Added
#include "cs_main.h"
#include "cs_netid.h"
#include "cl_cmd.h"
#include "cl_pred.h"
#include "sv_main.h"

//=============================================================================
//
// Line Attacks
//

static mobj_t *shootthing;

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
static boolean P_AimAtThing(intercept_t *in)
{
   mobj_t *th = in->d.thing;
   fixed_t thingtopslope, thingbottomslope, dist;
   
   if(th == shootthing)
      return true;    // can't shoot self
   
   if(!(th->flags & MF_SHOOTABLE))
      return true;    // corpse or something
   
   // killough 7/19/98, 8/2/98:
   // friends don't aim at friends (except players), at least not first
   if(th->flags & shootthing->flags & aim_flags_mask && !th->player)
      return true;
   
   // check angles to see if the thing can be aimed at
   
   // SoM: added distance so the slopes are calculated from the origin of the 
   // aiming tracer to keep the slopes consistent.
   if(demo_version >= 333)
      dist = FixedMul(trace.attackrange, in->frac) + trace.movefrac;
   else
      dist = FixedMul(trace.attackrange, in->frac);   

   thingtopslope = FixedDiv(th->z + th->height - trace.originz , dist);
   
   if(thingtopslope < trace.bottomslope)
      return true;    // shot over the thing
   
   thingbottomslope = FixedDiv(th->z - trace.originz, dist);
   
   if(thingbottomslope > trace.topslope)
      return true;    // shot under the thing
   
   // this thing can be hit!
   
   if(thingtopslope > trace.topslope)
      thingtopslope = trace.topslope;
   
   if(thingbottomslope < trace.bottomslope)
      thingbottomslope = trace.bottomslope;
   
   trace.aimslope = (thingtopslope + thingbottomslope) / 2;
   clip.linetarget = th;

   // We hit a thing so stop any furthur TPTs
   trace.finished = true;
   
   return false;   // don't go any farther
}
   
//
// PTR_AimTraverse
//
// Sets linetarget and aimslope when a target is aimed at.
//
static boolean PTR_AimTraverse(intercept_t *in)
{
   fixed_t slope, dist;
   
   if(in->isaline)
   {
      // shoot a line
      line_t *li = in->d.line;
      
      if(!(li->flags & ML_TWOSIDED))
         return false;   // stop

      // Crosses a two sided line.
      // A two sided line will restrict
      // the possible target ranges.
      
      P_LineOpening(li, NULL);
      
      if(clip.openbottom >= clip.opentop)
         return false;   // stop

      dist = FixedMul(trace.attackrange, in->frac);
      
      if(li->frontsector->floorheight != li->backsector->floorheight)
      {
         slope = FixedDiv(clip.openbottom - trace.z , dist);
         if(slope > trace.bottomslope)
            trace.bottomslope = slope;
      }

      if(li->frontsector->ceilingheight != li->backsector->ceilingheight)
      {
         slope = FixedDiv(clip.opentop - trace.z , dist);
         if(slope < trace.topslope)
            trace.topslope = slope;
      }

      if(trace.topslope <= trace.bottomslope)
         return false;   // stop
      
      return true;    // shot continues
   }
   else
   {  
      // shoot a thing
      return P_AimAtThing(in);
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
static boolean P_Shoot2SLine(line_t *li, int side, fixed_t dist)
{
   // haleyjd: when allowing planes to be shot, we do not care if
   // the sector heights are the same; we must check against the
   // line opening, otherwise lines behind the plane will be activated.
   
   boolean floorsame = 
      (li->frontsector->floorheight == li->backsector->floorheight &&
       (demo_version < 333 || comp[comp_planeshoot]));

   boolean ceilingsame =
      (li->frontsector->ceilingheight == li->backsector->ceilingheight &&
       (demo_version < 333 || comp[comp_planeshoot]));

   if((floorsame   || FixedDiv(clip.openbottom - trace.z , dist) <= trace.aimslope) &&
      (ceilingsame || FixedDiv(clip.opentop - trace.z , dist) >= trace.aimslope))
   {
      if(li->special && demo_version >= 329 && !comp[comp_planeshoot])
      {
         if(CS_SERVER)
            SV_BroadcastLineShot(shootthing, li, side);
         P_ShootSpecialLine(shootthing, li, side);
      }

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
static boolean P_ShotCheck2SLine(intercept_t *in, line_t *li, int lineside)
{
   fixed_t dist;
   boolean ret = false;

   if(li->flags & ML_TWOSIDED)
   {  
      // crosses a two sided (really 2s) line
      P_LineOpening(li, NULL);
      dist = FixedMul(trace.attackrange, in->frac);
      
      // killough 11/98: simplify
      // haleyjd 03/13/05: fixed bug that activates 2S line specials
      // when shots hit the floor
      if(P_Shoot2SLine(li, lineside, dist))
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
                           fixed_t dist)
{
   *frac = in->frac - FixedDiv(dist, trace.attackrange);
   *x = trace.x + FixedMul(trace.dx, *frac);
   *y = trace.y + FixedMul(trace.dy, *frac);
   *z = trace.z + FixedMul(trace.aimslope, FixedMul(*frac, trace.attackrange));
}

//
// P_ShootSky
//
// Returns true if PTR_ShootTraverse should return (ie, shot hits sky).
//
static boolean P_ShootSky(line_t *li, fixed_t z)
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
static boolean P_ShootThing(intercept_t *in)
{
   angle_t angle;
   fixed_t x, y, z, frac, dist, thingtopslope, thingbottomslope;
   mobj_t *th = in->d.thing;
   
   if(th == shootthing)
      return true;  // can't shoot self
   
   if(!(th->flags & MF_SHOOTABLE))
      return true;  // corpse or something
   
   // haleyjd: don't let players use melee attacks on ghosts
   if((th->flags3 & MF3_GHOST) && 
      shootthing->player &&
      P_GetReadyWeapon(shootthing->player)->flags & WPF_NOHITGHOSTS)
      return true;
   
   // check angles to see if the thing can be aimed at
   
   dist = FixedMul(trace.attackrange, in->frac);
   thingtopslope = FixedDiv(th->z + th->height - trace.z, dist);
   
   if(thingtopslope < trace.aimslope)
      return true;  // shot over the thing
   
   thingbottomslope = FixedDiv(th->z - trace.z, dist);
   
   if(thingbottomslope > trace.aimslope)
      return true;  // shot under the thing
   
   // hit thing
   // position a bit closer
   
   P_PuffPosition(in, &frac, &x, &y, &z, 10 * FRACUNIT);
   
   // Spawn bullet puffs or blood spots,
   // depending on target type. -- haleyjd: and status flags!
   angle = P_PointToAngle(0, 0, trace.dx, trace.dy) - ANG180;

   if(th->flags & MF_NOBLOOD || th->flags2 & (MF2_INVULNERABLE | MF2_DORMANT))
      CS_SpawnPuff(shootthing, x, y, z, angle, 2, true);
   else
      CS_SpawnBlood(shootthing, x, y, z, angle, trace.la_damage, th);
   
   if(serverside && trace.la_damage)
   {
      P_DamageMobj(
         th, shootthing, shootthing, trace.la_damage, shootthing->info->mod
      );
   }

   // SoM: we hit a thing!
   trace.finished = true;
   
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
static boolean PTR_ShootTraverseComp(intercept_t *in)
{
   angle_t angle;
   fixed_t x, y, z, frac;
   mobj_t *puff;

   
   if(in->isaline)
   {
      line_t *li = in->d.line;

      // haleyjd 03/13/05: move up point on line side check to here
      int lineside = P_PointOnLineSide(shootthing->x, shootthing->y, li);
      
      if(li->special && serverside)
      {
         P_ShootSpecialLine(shootthing, li, lineside);
         if(CS_SERVER)
            SV_BroadcastLineShot(shootthing, li, lineside);
      }

      // shot crosses a 2S line?
      if(P_ShotCheck2SLine(in, li, lineside))
         return true;

      // hit line
      // position a bit closer

      P_PuffPosition(in, &frac, &x, &y, &z, 4 * FRACUNIT);

      // don't hit the sky
      if(P_ShootSky(li, z))
         return false;

      angle = P_PointToAngle(0, 0, li->dx, li->dy) - ANG90;

      // Spawn bullet puffs.
      CS_SpawnPuff(shootthing, x, y, z, angle, 2, true);

      // don't go any farther
      return false;
   }
   else
   {
      // shoot a thing
      return P_ShootThing(in);
   }
}

//
// PTR_ShootTraverse
//
// haleyjd 11/21/01: fixed by SoM to allow bullets to puff on the
// floors and ceilings rather than along the line which they actually
// intersected far below or above the ceiling.
//
static boolean PTR_ShootTraverse(intercept_t *in)
{
   angle_t angle;
   fixed_t x, y, z, frac, zdiff;
   boolean hitplane = false; // SoM: Remember if the bullet hit a plane.
   int updown = 2; // haleyjd 05/02: particle puff z dist correction
   sector_t *sidesector;
   mobj_t *puff;
   
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
            P_PointOnLineSide(trace.x, trace.y, li) :
            P_PointOnLineSide(shootthing->x, shootthing->y, li);
      
      // SoM: Shouldn't be called until A: we know the bullet passed or
      // B: We know it didn't hit a plane first
      //if(li->special && (demo_version < 329 || comp[comp_planeshoot]))
      //   P_ShootSpecialLine(shootthing, li, lineside);
     
      if(P_ShotCheck2SLine(in, li, lineside))
         return true;

      // hit line
      // position a bit closer

      P_PuffPosition(in, &frac, &x, &y, &z, 4 * FRACUNIT);
      
      // SoM: Check for colision with a plane.
      sidesector = lineside ? li->backsector : li->frontsector;
      
      // SoM: If we are in no-clip and are shooting on the backside of a
      // 1s line, don't crash!
      if(sidesector)
      {
         if(z < sidesector->floorheight)
         {
            fixed_t pfrac = FixedDiv(sidesector->floorheight - trace.z, 
                                     trace.aimslope);
            
            // SoM: don't check for portals here anymore
            if(sidesector->floorpic == skyflatnum ||
               sidesector->floorpic == sky2flatnum) 
               return false;
            
            if(demo_version < 333)
            {
               zdiff = FixedDiv(D_abs(z - sidesector->floorheight),
                                D_abs(z - trace.originz));
               x += FixedMul(trace.x - x, zdiff);
               y += FixedMul(trace.y - y, zdiff);
            }
            else
            {
               x = trace.x + FixedMul(trace.cos, pfrac);
               y = trace.y + FixedMul(trace.sin, pfrac);
            }

            z = sidesector->floorheight;
            hitplane = true;
            updown = 0; // haleyjd
         }
         else if(z > sidesector->ceilingheight)
         {
            fixed_t pfrac = FixedDiv(sidesector->ceilingheight - trace.z, trace.aimslope);
            if(sidesector->intflags & SIF_SKY) // SoM
               return false;
            
            if(demo_version < 333)
            {
               zdiff = FixedDiv(D_abs(z - sidesector->ceilingheight),
                  D_abs(z - trace.originz));
               x += FixedMul(trace.x - x, zdiff);
               y += FixedMul(trace.y - y, zdiff);
            }
            else
            {
               x = trace.x + FixedMul(trace.cos, pfrac);
               y = trace.y + FixedMul(trace.sin, pfrac);
            }
            
            z = sidesector->ceilingheight;
            hitplane = true;
            updown = 1; // haleyjd
         }
      }
      
      if(serverside)
      {
         if(!hitplane && li->special)
         {
            if(CS_SERVER)
               SV_BroadcastLineShot(shootthing, li, lineside);
            P_ShootSpecialLine(shootthing, li, lineside);
         }
      }

      // don't hit the sky
      if(P_ShootSky(li, z))
         return false;

      // don't shoot portal lines
      if(!hitplane && li->portal)
         return false;

      // Spawn bullet puffs.
      angle = P_PointToAngle(0, 0, li->dx, li->dy) - ANG90;
      CS_SpawnPuff(shootthing, x, y, z, angle, updown, true);

      // don't go any farther
      return false;
   }
   else
   {
      // shoot a thing
      return P_ShootThing(in);
   }
}

//
// P_AimLineAttack
//
// killough 8/2/98: add mask parameter, which, if set to MF_FRIEND,
// makes autoaiming skip past friends.
//
fixed_t P_AimLineAttack(mobj_t *t1, angle_t angle, fixed_t distance, int mask)
{
   fixed_t x2, y2;
   fixed_t lookslope = 0;
   fixed_t pitch = 0;
   
   angle >>= ANGLETOFINESHIFT;
   shootthing = t1;
   
   x2 = t1->x + (distance>>FRACBITS)*(trace.cos = finecosine[angle]);
   y2 = t1->y + (distance>>FRACBITS)*(trace.sin = finesine[angle]);
   trace.originz = trace.z = t1->z + (t1->height>>1) + 8*FRACUNIT;
   trace.movefrac = 0;

   // haleyjd 10/08/06: this should be gotten from t1->player, not 
   // players[displayplayer]. Also, if it's zero, use the old
   // code in all cases to avoid roundoff error.
   if(t1->player)
      pitch = t1->player->pitch;
   
   // can't shoot outside view angles

   if(pitch == 0 || demo_version < 333)
   {
      trace.topslope    =  100*FRACUNIT/160;
      trace.bottomslope = -100*FRACUNIT/160;
   }
   else
   {
      // haleyjd 04/05/05: use proper slope range for look slope
      fixed_t topangle, bottomangle;

      lookslope   = finetangent[(ANG90 - pitch) >> ANGLETOFINESHIFT];

      topangle    = pitch - ANGLE_1*32;
      bottomangle = pitch + ANGLE_1*32;

      trace.topslope    = finetangent[(ANG90 -    topangle) >> ANGLETOFINESHIFT];
      trace.bottomslope = finetangent[(ANG90 - bottomangle) >> ANGLETOFINESHIFT];
   }
   
   trace.attackrange = distance;
   clip.linetarget = NULL;

   // killough 8/2/98: prevent friends from aiming at friends
   aim_flags_mask = mask;
   
   P_PathTraverse(t1->x, t1->y, x2, y2, PT_ADDLINES|PT_ADDTHINGS, PTR_AimTraverse);
   
   return clip.linetarget ? trace.aimslope : lookslope;
}

//
// P_LineAttack
//
// If damage == 0, it is just a test trace that will leave linetarget set.
//
void P_LineAttack(mobj_t *t1, angle_t angle, fixed_t distance,
                  fixed_t slope, int damage)
{
   fixed_t x2, y2;

   if(clientserver && t1->player)
      cs_shooting_player = t1->player - players;

   angle >>= ANGLETOFINESHIFT;
   shootthing = t1;
   trace.la_damage = damage;
   x2 = t1->x + (distance >> FRACBITS) * (trace.cos = finecosine[angle]);
   y2 = t1->y + (distance >> FRACBITS) * (trace.sin = finesine[angle]);
   
   trace.originz = trace.z =
      t1->z - t1->floorclip + (t1->height >> 1) + 8 * FRACUNIT;
   trace.attackrange = distance;
   trace.aimslope = slope;
   trace.movefrac = 0;

   P_PathTraverse(t1->x, t1->y, x2, y2, PT_ADDLINES|PT_ADDTHINGS, 
                  (demo_version < 329 || comp[comp_planeshoot]) ?
                      PTR_ShootTraverseComp : PTR_ShootTraverse);
   if(clientserver)
      cs_shooting_player = 0;
}

//
// USE LINES
//

static mobj_t *usething;

// killough 11/98: reformatted
// haleyjd  09/02: reformatted again.

static boolean PTR_UseTraverse(intercept_t *in)
{
   int side = P_PointOnLineSide(trace.originx, trace.originy,in->d.line) == 1;

   if(in->d.line->special)
   {
      // [CG] Only servers use lines.
      if(serverside)
      {
         if(CS_SERVER)
            SV_BroadcastLineUsed(usething, in->d.line, side);
         P_UseSpecialLine(usething, in->d.line, side);
      }

      //WAS can't use for than one special line in a row
      //jff 3/21/98 NOW multiple use allowed with enabling line flag
      return (!demo_compatibility && in->d.line->flags & ML_PASSUSE);
   }
   else
   {
      P_LineOpening(in->d.line, NULL);
      if(clip.openrange <= 0)
      {
         // can't use through a wall
         // [CG] Only make this sound if not predicting.
         if(!cl_predicting)
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
static boolean PTR_NoWayTraverse(intercept_t *in)
{
   line_t *ld = in->d.line;                       // This linedef

   return ld->special ||                          // Ignore specials
     !(ld->flags & ML_BLOCKING ||                 // Always blocking
       (P_LineOpening(ld, NULL),                  // Find openings
        clip.openrange <= 0 ||                         // No opening
        clip.openbottom > usething->z+24*FRACUNIT ||   // Too high it blocks
        clip.opentop < usething->z+usething->height)); // Too low it blocks
}

//
// P_UseLines
//
// Looks for special lines in front of the player to activate.
//
void P_UseLines(player_t *player)
{
   fixed_t x1, y1, x2, y2;
   int angle;
   
   usething = player->mo;
   
   angle = player->mo->angle >> ANGLETOFINESHIFT;
   
   x1 = player->mo->x;
   y1 = player->mo->y;
   x2 = x1 + (USERANGE>>FRACBITS)*(trace.cos = finecosine[angle]);
   y2 = y1 + (USERANGE>>FRACBITS)*(trace.sin = finesine[angle]);

   trace.attackrange = USERANGE;
   trace.movefrac = 0;

   // old code:
   //
   // P_PathTraverse ( x1, y1, x2, y2, PT_ADDLINES, PTR_UseTraverse );
   //
   // This added test makes the "oof" sound work on 2s lines -- killough:
   
   if(P_PathTraverse(x1, y1, x2, y2, PT_ADDLINES, PTR_UseTraverse))
   {
      if(!P_PathTraverse(x1, y1, x2, y2, PT_ADDLINES, PTR_NoWayTraverse))
      {
         // [CG] Only make this sound if not predicting.
         if(!cl_predicting)
            S_StartSound(usething, GameModeInfo->playerSounds[sk_noway]);
      }
   }
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
      intercepts = realloc(intercepts, sizeof(*intercepts)*num_intercepts);
      intercept_p = intercepts + offset;
   }
}

linetracer_t trace;

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
boolean PIT_AddLineIntercepts(line_t *ld)
{
   int       s1;
   int       s2;
   fixed_t   frac;
   divline_t dl;

   // avoid precision problems with two routines
   if(trace.dx >  FRACUNIT*16 || trace.dy >  FRACUNIT*16 ||
      trace.dx < -FRACUNIT*16 || trace.dy < -FRACUNIT*16)
   {
      s1 = P_PointOnDivlineSide (ld->v1->x, ld->v1->y, (divline_t *)&trace);
      s2 = P_PointOnDivlineSide (ld->v2->x, ld->v2->y, (divline_t *)&trace);
   }
   else
   {
      s1 = P_PointOnLineSide (trace.x, trace.y, ld);
      s2 = P_PointOnLineSide (trace.x+trace.dx, trace.y+trace.dy, ld);
   }

   if(s1 == s2)
      return true;        // line isn't crossed
   
   // hit the line
   P_MakeDivline(ld, &dl);
   frac = P_InterceptVector((divline_t *)&trace, &dl);
   
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
boolean PIT_AddThingIntercepts(mobj_t *thing)
{
   fixed_t   x1, y1;
   fixed_t   x2, y2;
   int       s1, s2;
   divline_t dl;
   fixed_t   frac;

   // check a corner to corner crossection for hit
   if((trace.dx ^ trace.dy) > 0)
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

   s1 = P_PointOnDivlineSide (x1, y1, (divline_t *)&trace);
   s2 = P_PointOnDivlineSide (x2, y2, (divline_t *)&trace);
   
   if(s1 == s2)
      return true;                // line isn't crossed

   dl.x = x1;
   dl.y = y1;
   dl.dx = x2-x1;
   dl.dy = y2-y1;
   
   frac = P_InterceptVector ((divline_t *)&trace, &dl);
   
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
boolean P_TraverseIntercepts(traverser_t func, fixed_t maxfrac)
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
         if(!func(in))
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
boolean P_PathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                       int flags, boolean trav(intercept_t *))
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
   boolean result;

   // Only PTR_s that use TPTs need to worry about this value.
   trace.finished = false;

   validcount++;
   intercept_p = intercepts;
   
   if(!((x1-bmaporgx)&(MAPBLOCKSIZE-1)))
      x1 += FRACUNIT;     // don't side exactly on a line
   
   if(!((y1-bmaporgy)&(MAPBLOCKSIZE-1)))
      y1 += FRACUNIT;     // don't side exactly on a line

   trace.x = trace.originx = x1;
   trace.y = trace.originy = y1;
   trace.dx = x2 - x1;
   trace.dy = y2 - y1;
   
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
         if(!P_BlockLinesIterator(mapx, mapy, PIT_AddLineIntercepts))
            return false; // early out
      }
      
      if(flags & PT_ADDTHINGS)
      {
         if(!P_BlockThingsIterator(mapx, mapy, PIT_AddThingIntercepts))
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
   result = P_TraverseIntercepts(trav, FRACUNIT);

   return result;
}

// EOF

