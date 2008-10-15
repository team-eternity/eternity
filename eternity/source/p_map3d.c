// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// This module, except for code marked otherwise, is covered by the 
// ZDoom source distribution license, which is included in the 
// Eternity source distribution, and is compatible with the terms of
// the GNU General Public License.
//
// See that license file for details.
//
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//
// 3D Mobj Clipping Code
//
// Largely from zdoom, this system seems to be more reliable than our old one.
// It is also kept totally separate from the old clipping code to avoid the
// entangling problems the earlier code had.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "d_gi.h"
#include "doomdef.h"
#include "doomstat.h"
#include "e_states.h"
#include "e_things.h"
#include "m_bbox.h"
#include "m_random.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_inter.h"
#include "p_partcl.h"
#include "p_setup.h"
#include "r_main.h"

// I HATE GLOBALS!!!
extern fixed_t   FloatBobOffsets[64];

//
// P_ZMovementTest
//
// Attempt vertical movement.
//
// haleyjd 06/28/06: Derived from P_ZMovement. Does the same logic, but
// without any side-effects so that it's reversible: the only thing changed
// is mo->z, which will be reset before the real move takes place.
//
static void P_ZMovementTest(mobj_t *mo)
{
   // killough 7/11/98:
   // BFG fireballs bounced on floors and ceilings in Pre-Beta Doom
   // killough 8/9/98: added support for non-missile objects bouncing
   // (e.g. grenade, mine, pipebomb)

   if(mo->flags & MF_BOUNCES && mo->momz)
   {
      mo->z += mo->momz;
            
      if(mo->z <= mo->floorz)                  // bounce off floors
      {
         mo->z = mo->floorz;

         if(mo->momz < 0)
         {
            // killough 11/98: touchy objects explode on impact
            if(mo->flags & MF_TOUCHY && mo->intflags & MIF_ARMED &&
               mo->health > 0)
               return;
            else if(mo->flags & MF_FLOAT && sentient(mo))
               goto floater;
         }
      }
      else if(mo->z >= mo->ceilingz - mo->height) // bounce off ceilings
      {
         mo->z = mo->ceilingz - mo->height;
         if(mo->momz > 0)
         {
            if(!(mo->subsector->sector->ceilingpic != skyflatnum &&
                 mo->subsector->sector->ceilingpic != sky2flatnum))
               if(mo->flags & MF_MISSILE)
                  return;      // missiles don't bounce off skies

            if(mo->flags & MF_FLOAT && sentient(mo))
               goto floater;
            
            return;
         }
      }
      else
      {
         if(mo->flags & MF_FLOAT && sentient(mo))
            goto floater;
         return;
      }
   }
   
   // adjust altitude
   mo->z += mo->momz;
   
floater:

   // float down towards target if too close
   
   if(!((mo->flags ^ MF_FLOAT) & (MF_FLOAT | MF_SKULLFLY | MF_INFLOAT)) && 
      mo->target)     // killough 11/98: simplify
   {
      fixed_t delta;
      if(P_AproxDistance(mo->x - mo->target->x, mo->y - mo->target->y) <
         D_abs(delta = mo->target->z + (mo->height >> 1) - mo->z) * 3)
         mo->z += delta < 0 ? -FLOATSPEED : FLOATSPEED;
   }

   // clip movement
   
   if(mo->z <= mo->floorz)
      mo->z = mo->floorz;

   if(mo->z + mo->height > mo->ceilingz)
      mo->z = mo->ceilingz - mo->height;
}

static mobj_t *testz_mobj; // used to hold object found by P_TestMobjZ

//
// PIT_TestMobjZ
//
// Derived from zdoom; iterator function for P_TestMobjZ
//
static boolean PIT_TestMobjZ(mobj_t *thing)
{
   fixed_t blockdist = thing->radius + tm->thing->radius;

   if(!(thing->flags & MF_SOLID) ||                      // non-solid?
      thing->flags & (MF_SPECIAL|MF_NOCLIP|MF_CORPSE) || // other is special?
      tm->thing->flags & MF_SPECIAL ||                   // this is special?
      thing == tm->thing ||                              // same as self?
      tm->thing->z > thing->z + thing->height ||         // over?
      tm->thing->z + tm->thing->height <= thing->z)      // under?
   {
      return true;
   }

   // test against collision - from PIT_CheckThing:
   if(D_abs(thing->x - tm->x) >= blockdist || 
      D_abs(thing->y - tm->y) >= blockdist)
      return true;

   // the thing may be blocking; save a pointer to it
   testz_mobj = thing;
   return false;
}

//
// P_TestMobjZ
//
// From zdoom; tests a thing's z position for validity.
//
boolean P_TestMobjZ(mobj_t *mo)
{
   int xl, xh, yl, yh, x, y;
   
   // a no-clipping thing is always good
   if(mo->flags & MF_NOCLIP)
      return true;

   tm->x = mo->x;
   tm->y = mo->y;
   tm->thing = mo;
   
   // standard blockmap bounding box extension
   tm->bbox[BOXLEFT]   = tm->x - mo->radius;
   tm->bbox[BOXRIGHT]  = tm->x + mo->radius;
   tm->bbox[BOXBOTTOM] = tm->y - mo->radius;
   tm->bbox[BOXTOP]    = tm->y + mo->radius;
   
   xl = (tm->bbox[BOXLEFT]   - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
   xh = (tm->bbox[BOXRIGHT]  - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
   yl = (tm->bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
   yh = (tm->bbox[BOXTOP]    - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

   // standard P_BlockThingsIterator loop
   for(x = xl; x <= xh; ++x)
      for(y = yl; y <= yh; ++y)
         if(!P_BlockThingsIterator(x, y, PIT_TestMobjZ))
            return false;

   return true;
}

//
// P_GetThingUnder
//
// If we're standing on something, this function will return it. Otherwise,
// it'll return NULL.
//
mobj_t *P_GetThingUnder(mobj_t *mo)
{
   // save the current z coordinate
   fixed_t mo_z = mo->z;

   // fake the move, then test
   P_ZMovementTest(mo);
   testz_mobj = NULL;
   P_TestMobjZ(mo);

   // restore z
   mo->z = mo_z;

   return testz_mobj;
}

//
// P_SBlockThingsIterator
//
// Special version of P_BlockThingsIterator: takes an actor from which to start
// a search, which is an extension borrowed from zdoom and is needed for 3D 
// object clipping.
//
boolean P_SBlockThingsIterator(int x, int y, boolean (*func)(mobj_t *), 
                               mobj_t *actor)
{
   mobj_t *mobj;

   if(x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight)
      return true;
   
   if(actor == NULL)
      mobj = blocklinks[y * bmapwidth + x];
   else
      mobj = actor->bnext;

   for(; mobj; mobj = mobj->bnext)
      if(!func(mobj))
         return false;
   
   return true;
}

static mobj_t *stepthing;

extern boolean PIT_CheckLine(line_t *ld);

extern boolean P_Touched(mobj_t *thing, mobj_t *tmthing);
extern int     P_MissileBlockHeight(mobj_t *mo);
extern boolean P_CheckPickUp(mobj_t *thing, mobj_t *tmthing);
extern boolean P_SkullHit(mobj_t *thing, mobj_t *tmthing);

//
// PIT_CheckThing3D
// 
static boolean PIT_CheckThing3D(mobj_t *thing) // killough 3/26/98: make static
{
   fixed_t topz;      // haleyjd: from zdoom
   fixed_t blockdist;
   int damage;

   // EDF FIXME: haleyjd 07/13/03: these may be temporary fixes
   static int bruiserType = -1;
   static int knightType  = -1;

   if(bruiserType == -1)
   {
      bruiserType = E_ThingNumForDEHNum(MT_BRUISER);
      knightType  = E_ThingNumForDEHNum(MT_KNIGHT);
   }

   // killough 11/98: add touchy things
   if(!(thing->flags & (MF_SOLID|MF_SPECIAL|MF_SHOOTABLE|MF_TOUCHY)))
      return true;

   blockdist = thing->radius + tm->thing->radius;

   if(D_abs(thing->x - tm->x) >= blockdist ||
      D_abs(thing->y - tm->y) >= blockdist)
      return true; // didn't hit it

   // killough 11/98:
   //
   // This test has less information content (it's almost always false), so it
   // should not be moved up to first, as it adds more overhead than it removes.
   
   // don't clip against self
  
   if(thing == tm->thing)
      return true;

   // haleyjd 1/17/00: set global hit reference
   tm->BlockingMobj = thing;

   // haleyjd: from zdoom: OVER_UNDER
   topz = thing->z + thing->height;

   if(!(tm->thing->flags & (MF_FLOAT|MF_MISSILE|MF_SKULLFLY|MF_NOGRAVITY)) &&
      (thing->flags & MF_SOLID))
   {
      // [RH] Let monsters walk on actors as well as floors
      if(((tm->thing->flags & MF_COUNTKILL) || (tm->thing->flags3 & MF3_KILLABLE)) &&
         topz >= tm->floorz && topz <= tm->thing->z + 24*FRACUNIT)
      {
         stepthing = thing;
         tm->floorz = topz;
      }
   }

   if(tm->thing->flags3 & MF3_PASSMOBJ)
   {
      // check if a mobj passed over/under another object
      
      // Some things prefer not to overlap each other, if possible
      if(tm->thing->flags3 & thing->flags3 & MF3_DONTOVERLAP)
         return false;

      // haleyjd: touchies need to explode if being exactly touched
      if(thing->flags & MF_TOUCHY && !(tm->thing->intflags & MIF_NOTOUCH))
      {
         if(tm->thing->z == topz || tm->thing->z + tm->thing->height == thing->z)
         {
            P_Touched(thing, tm->thing);
            // haleyjd: make the thing fly up a bit so it can run across
            tm->thing->momz += FRACUNIT; 
            return true;
         }
      }

      if((tm->thing->z >= topz) || (tm->thing->z + tm->thing->height <= thing->z))
         return true;
   }

   // killough 11/98:
   //
   // TOUCHY flag, for mines or other objects which die on contact with solids.
   // If a solid object of a different type comes in contact with a touchy
   // thing, and the touchy thing is not the sole one moving relative to fixed
   // surroundings such as walls, then the touchy thing dies immediately.

   if(!(tm->thing->intflags & MIF_NOTOUCH)) // haleyjd: not when just testing
      if(P_Touched(thing, tm->thing))
         return true;

   // check for skulls slamming into things
   
   if(P_SkullHit(thing, tm->thing))
      return false;

   // missiles can hit other things
   // killough 8/10/98: bouncing non-solid things can hit other things too
   
   if(tm->thing->flags & MF_MISSILE || 
      (tm->thing->flags & MF_BOUNCES && !(tm->thing->flags & MF_SOLID)))
   {
      // haleyjd 07/06/05: some objects may use info->height instead
      // of their current height value in this situation, to avoid
      // altering the playability of maps when 3D object clipping
      // with corrected thing heights is enabled.
      int height = P_MissileBlockHeight(thing);

      // haleyjd: some missiles can go through ghosts
      if(thing->flags3 & MF3_GHOST && tm->thing->flags3 & MF3_THRUGHOST)
         return true;

      // see if it went over / under
      
      if(tm->thing->z > thing->z + height) // haleyjd 07/06/05
         return true;    // overhead

      if(tm->thing->z + tm->thing->height < thing->z)
         return true;    // underneath

      if(tm->thing->target &&
         (tm->thing->target->type == thing->type ||
          (tm->thing->target->type == knightType && thing->type == bruiserType)||
          (tm->thing->target->type == bruiserType && thing->type == knightType)))
      {
         if(thing == tm->thing->target)
            return true;                // Don't hit same species as originator.
         else if(!(thing->player))      // Explode, but do no damage.
            return false;               // Let players missile other players.
      }

      // haleyjd 10/15/08: rippers
      if(tm->thing->flags3 & MF3_RIP)
      {
         // TODO: P_RipperBlood
         /*
         if(!(thing->flags&MF_NOBLOOD))
         { 
            // Ok to spawn some blood
            P_RipperBlood(tmthing);
         }
         */
         
         // TODO: ripper sound - gamemode dependent? thing dependent?
         //S_StartSound(tm->thing, sfx_ripslop);
         
         damage = ((P_Random(pr_rip) & 3) + 2) * tm->thing->damage;
         
         P_DamageMobj(thing, tm->thing, tm->thing->target, damage, 
                      tm->thing->info->mod);
         
         if(thing->flags2 & MF2_PUSHABLE && !(tm->thing->flags3 & MF3_CANNOTPUSH))
         { 
            // Push thing
            thing->momx += tm->thing->momx >> 2;
            thing->momy += tm->thing->momy >> 2;
         }
         
         // TODO: huh?
         //numspechit = 0;
         return true;
      }

      // killough 8/10/98: if moving thing is not a missile, no damage
      // is inflicted, and momentum is reduced if object hit is solid.

      if(!(tm->thing->flags & MF_MISSILE))
      {
         if(!(thing->flags & MF_SOLID))
            return true;
         else
         {
            tm->thing->momx = -tm->thing->momx;
            tm->thing->momy = -tm->thing->momy;
            if(!(tm->thing->flags & MF_NOGRAVITY))
            {
               tm->thing->momx >>= 2;
               tm->thing->momy >>= 2;
            }

            return false;
         }
      }

      if(!(thing->flags & MF_SHOOTABLE))
         return !(thing->flags & MF_SOLID); // didn't do any damage

      // damage / explode
      
      damage = ((P_Random(pr_damage)%8)+1)*tm->thing->damage;
      P_DamageMobj(thing, tm->thing, tm->thing->target, damage,
                   tm->thing->info->mod);

      // don't traverse any more
      return false;
   }

   // haleyjd 1/16/00: Pushable objects -- at last!
   //   This is remarkably simpler than I had anticipated!
   
   if(thing->flags2 & MF2_PUSHABLE && !(tm->thing->flags3 & MF3_CANNOTPUSH))
   {
      // transfer one-fourth momentum along the x and y axes
      thing->momx += tm->thing->momx / 4;
      thing->momy += tm->thing->momy / 4;
   }

   // check for special pickup

   if(thing->flags & MF_SPECIAL
      // [RH] The next condition is to compensate for the extra height
      // that gets added by P_CheckPosition() so that you cannot pick
      // up things that are above your true height.
      && thing->z < tm->thing->z + tm->thing->height - 24*FRACUNIT)
      return P_CheckPickUp(thing, tm->thing);

   // killough 3/16/98: Allow non-solid moving objects to move through solid
   // ones, by allowing the moving thing (tmthing) to move if it's non-solid,
   // despite another solid thing being in the way.
   // killough 4/11/98: Treat no-clipping things as not blocking

   return !((thing->flags & MF_SOLID && !(thing->flags & MF_NOCLIP))
          && (tm->thing->flags & MF_SOLID || demo_compatibility));
}

//
// P_CheckPosition3D
//
// A 3D version of P_CheckPosition.
//
boolean P_CheckPosition3D(mobj_t *thing, fixed_t x, fixed_t y) 
{
   int xl, xh, yl, yh, bx, by;
   subsector_t *newsubsec;
   fixed_t thingdropoffz;

   // haleyjd: from zdoom:
   mobj_t  *thingblocker;
   mobj_t  *fakedblocker;
   fixed_t realheight = thing->height;

   tm->thing = thing;
   tm->flags = thing->flags;
   
   tm->x = x;
   tm->y = y;
   
   tm->bbox[BOXTOP]    = y + tm->thing->radius;
   tm->bbox[BOXBOTTOM] = y - tm->thing->radius;
   tm->bbox[BOXRIGHT]  = x + tm->thing->radius;
   tm->bbox[BOXLEFT]   = x - tm->thing->radius;
   
   newsubsec = R_PointInSubsector(x,y);
   tm->floorline = tm->blockline = tm->ceilingline = NULL; // killough 8/1/98

   // Whether object can get out of a sticky situation:
   tm->unstuck = thing->player &&        // only players
      thing->player->mo == thing;        // not voodoo dolls

   // The base floor / ceiling is from the subsector
   // that contains the point.
   // Any contacted lines the step closer together
   // will adjust them.

#ifdef R_LINKEDPORTALS
   if(demo_version >= 333 && R_LinkedFloorActive(newsubsec->sector) && 
      !(tm->thing->flags & MF_NOCLIP))
      tm->floorz = tm->dropoffz = newsubsec->sector->floorheight - (1024 * FRACUNIT);
   else
#endif
      tm->floorz = tm->dropoffz = newsubsec->sector->floorheight;

#ifdef R_LINKEDPORTALS
   if(demo_version >= 333 && R_LinkedCeilingActive(newsubsec->sector) &&
      !(tm->thing->flags & MF_NOCLIP))
      tm->ceilingz = newsubsec->sector->ceilingheight + (1024 * FRACUNIT);
   else
#endif
      tm->ceilingz = newsubsec->sector->ceilingheight;

   tm->secfloorz = tm->passfloorz = tm->stepupfloorz = tm->floorz;
   tm->secceilz = tm->passceilz = tm->ceilingz;

   // haleyjd
   tm->floorpic = newsubsec->sector->floorpic;
   // SoM: 09/07/02: 3dsides monster fix
   tm->touch3dside = 0;
   validcount++;
   
   tm->numspechit = 0;

   // haleyjd 06/28/06: skullfly check from zdoom
   if(tm->flags & MF_NOCLIP && !(tm->flags & MF_SKULLFLY))
      return true;

   // Check things first, possibly picking things up.
   // The bounding box is extended by MAXRADIUS
   // because mobj_ts are grouped into mapblocks
   // based on their origin point, and can overlap
   // into adjacent blocks by up to MAXRADIUS units.

   xl = (tm->bbox[BOXLEFT]   - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
   xh = (tm->bbox[BOXRIGHT]  - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
   yl = (tm->bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
   yh = (tm->bbox[BOXTOP]    - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

   tm->BlockingMobj = NULL; // haleyjd 1/17/00: global hit reference
   thingblocker = NULL;
   fakedblocker = NULL;
   stepthing    = NULL;

   // [RH] Fake taller height to catch stepping up into things.
   if(thing->player)   
      thing->height = realheight + 24*FRACUNIT;
   
   for(bx = xl; bx <= xh; ++bx)
   {
      for(by = yl; by <= yh; ++by)
      {
         // haleyjd: from zdoom:
         mobj_t *robin = NULL;

         do
         {
            if(!P_SBlockThingsIterator(bx, by, PIT_CheckThing3D, robin))
            { 
               // [RH] If a thing can be stepped up on, we need to continue checking
               // other things in the blocks and see if we hit something that is
               // definitely blocking. Otherwise, we need to check the lines, or we
               // could end up stuck inside a wall.
               if(tm->BlockingMobj == NULL)
               { 
                  // Thing slammed into something; don't let it move now.
                  thing->height = realheight;

                  return false;
               }
               else if(!tm->BlockingMobj->player && 
                       !(thing->flags & (MF_FLOAT|MF_MISSILE|MF_SKULLFLY)) &&
                       tm->BlockingMobj->z + tm->BlockingMobj->height-thing->z <= 24*FRACUNIT)
               {
                  if(thingblocker == NULL || tm->BlockingMobj->z > thingblocker->z)
                     thingblocker = tm->BlockingMobj;
                  robin = tm->BlockingMobj;
                  tm->BlockingMobj = NULL;
               }
               else if(thing->player &&
                       thing->z + thing->height - tm->BlockingMobj->z <= 24*FRACUNIT)
               {
                  if(thingblocker)
                  { 
                     // There is something to step up on. Return this thing as
                     // the blocker so that we don't step up.
                     thing->height = realheight;

                     return false;
                  }
                  // Nothing is blocking us, but this actor potentially could
                  // if there is something else to step on.
                  fakedblocker = tm->BlockingMobj;
                  robin = tm->BlockingMobj;
                  tm->BlockingMobj = NULL;
               }
               else
               { // Definitely blocking
                  thing->height = realheight;

                  return false;
               }
            }
            else
               robin = NULL;
         } 
         while(robin);
      }
   }

   // check lines
   
   tm->BlockingMobj = NULL; // haleyjd 1/17/00: global hit reference
   thing->height = realheight;
   if(tm->flags & MF_NOCLIP)
      return (tm->BlockingMobj = thingblocker) == NULL;
   
   xl = (tm->bbox[BOXLEFT]   - bmaporgx) >> MAPBLOCKSHIFT;
   xh = (tm->bbox[BOXRIGHT]  - bmaporgx) >> MAPBLOCKSHIFT;
   yl = (tm->bbox[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
   yh = (tm->bbox[BOXTOP]    - bmaporgy) >> MAPBLOCKSHIFT;

   thingdropoffz = tm->floorz;
   tm->floorz = tm->dropoffz;

   for(bx = xl; bx <= xh; ++bx)
      for(by = yl; by <= yh; ++by)
         if(!P_BlockLinesIterator(bx, by, PIT_CheckLine))
            return false; // doesn't fit

   if(tm->ceilingz - tm->floorz < thing->height)
      return false;
         
   if(stepthing != NULL)
      tm->dropoffz = thingdropoffz;
   
   return (tm->BlockingMobj = thingblocker) == NULL;
}

//
// P_CheckPositionExt
//
// Calls the 3D version of P_CheckPosition, and also performs an additional
// floorz/ceilingz clip. This is just for testing, and stuff like collecting
// powerups and exploding touchy objects won't happen.
//
boolean P_CheckPositionExt(mobj_t *mo, fixed_t x, fixed_t y)
{
   int flags;
   boolean xygood;
   
   // save the thing's flags, some flags must be removed to avoid side effects
   flags = mo->flags;
   mo->flags &= ~MF_PICKUP;
   mo->intflags |= MIF_NOTOUCH; // haleyjd: don't blow up touchies!

   xygood = P_CheckPosition3D(mo, x, y);
   mo->flags = flags;
   mo->intflags &= ~MIF_NOTOUCH;

   if(xygood)
   { 
      fixed_t z = mo->z;
      
      if(mo->flags2 & MF2_FLOATBOB)
         z -= FloatBobOffsets[(mo->floatbob + leveltime - 1) & 63];

      if(z < mo->floorz || z + mo->height > mo->ceilingz)
         return false;
   }
   
   return xygood;
}

//=============================================================================
//
// Sector Movement
//

static int moveamt;
static int crushchange;
static sector_t *movesec;
static boolean nofit;
static MobjCollection intersectors; // haleyjd: use MobjCollection

//
// P_AdjustFloorCeil
//
// From zdoom: what our system was mostly lacking.
//
static boolean P_AdjustFloorCeil(mobj_t *thing, boolean midtex)
{
   boolean isgood;
   unsigned long oldfl3 = thing->flags3;
   
   // haleyjd: ALL things must be treated as PASSMOBJ when moving
   // 3DMidTex lines, otherwise you get stuck in them.
   if(midtex)
      thing->flags3 |= MF3_PASSMOBJ;

   isgood = P_CheckPosition3D(thing, thing->x, thing->y);

   thing->floorz     = tm->floorz;
   thing->secfloorz  = tm->secfloorz;
   thing->passfloorz = tm->passfloorz;
   thing->ceilingz   = tm->ceilingz;
   thing->secceilz   = tm->secceilz;
   thing->passceilz  = tm->passceilz;
   thing->dropoffz   = tm->dropoffz; // killough 11/98: remember dropoffs
   
   thing->flags3 = oldfl3;

   return isgood;
}

//
// PIT_FindAboveIntersectors
//
// haleyjd: From zdoom. I was about to implement this exact type of iterative
// check for each sector movement type when I noticed this is how zdoom does it
// already. This and the other functions below gather up things that are in
// over/under situations with an object being moved by a sector floor or
// ceiling so that they can be dealt with uniformly at one time.
//
static boolean PIT_FindAboveIntersectors(mobj_t *thing)
{
   fixed_t blockdist;
   if(!(thing->flags & MF_SOLID) ||               // Can't hit thing?
      (thing->flags & (MF_CORPSE|MF_SPECIAL)) ||  // Corpse or special?
      thing == tm->thing)                         // clipping against self?
      return true;
   
   blockdist = thing->radius + tm->thing->radius;
   
   // Didn't hit thing?
   if(D_abs(thing->x - tm->x) >= blockdist || 
      D_abs(thing->y - tm->y) >= blockdist)
      return true;

   // Thing intersects above the base?
   if(thing->z >= tm->thing->z && thing->z <= tm->thing->z + tm->thing->height)
      P_AddToCollection(&intersectors, thing);

   return true;
}

boolean PIT_FindBelowIntersectors(mobj_t *thing)
{
   fixed_t blockdist;
   if(!(thing->flags & MF_SOLID) ||               // Can't hit thing?
      (thing->flags & (MF_CORPSE|MF_SPECIAL)) ||  // Corpse or special?
      thing == tm->thing)                           // clipping against self?
      return true;

   blockdist = thing->radius + tm->thing->radius;
   
   // Didn't hit thing?
   if(D_abs(thing->x - tm->x) >= blockdist || 
      D_abs(thing->y - tm->y) >= blockdist)
      return true;

   if(thing->z + thing->height <= tm->thing->z + tm->thing->height &&
      thing->z + thing->height > tm->thing->z)
   { 
      // Thing intersects below the base
      P_AddToCollection(&intersectors, thing);
   }

   return true;
}

//
// P_FindAboveIntersectors
//
// Finds all the things above the thing being moved and puts them into an
// MobjCollection (this is partially explained above). From zdoom.
//
static void P_FindAboveIntersectors(mobj_t *actor)
{
   int	xl, xh, yl, yh, bx, by;
   fixed_t x, y;

   if(actor->flags & MF_NOCLIP)
      return;
   
   if(!(actor->flags & MF_SOLID))
      return;

   tm->x = x = actor->x;
   tm->y = y = actor->y;
   tm->thing = actor;

   tm->bbox[BOXTOP]    = y + actor->radius;
   tm->bbox[BOXBOTTOM] = y - actor->radius;
   tm->bbox[BOXRIGHT]  = x + actor->radius;
   tm->bbox[BOXLEFT]   = x - actor->radius;

   xl = (tm->bbox[BOXLEFT]   - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
   xh = (tm->bbox[BOXRIGHT]  - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
   yl = (tm->bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
   yh = (tm->bbox[BOXTOP]    - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

   for(bx = xl; bx <= xh; bx++)
      for(by = yl; by <= yh; by++)
         if(!P_BlockThingsIterator(bx, by, PIT_FindAboveIntersectors))
            return;

   return;
}

//
// P_FindBelowIntersectors
//
// As the above function, but for things that are below the actor.
//
static void P_FindBelowIntersectors(mobj_t *actor)
{
   int	xl,xh,yl,yh,bx,by;
   fixed_t x, y;
   
   if(actor->flags & MF_NOCLIP)
      return;
   
   if(!(actor->flags & MF_SOLID))
      return;

   tm->x = x = actor->x;
   tm->y = y = actor->y;
   tm->thing = actor;
   
   tm->bbox[BOXTOP]    = y + actor->radius;
   tm->bbox[BOXBOTTOM] = y - actor->radius;
   tm->bbox[BOXRIGHT]  = x + actor->radius;
   tm->bbox[BOXLEFT]   = x - actor->radius;

   xl = (tm->bbox[BOXLEFT]   - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
   xh = (tm->bbox[BOXRIGHT]  - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
   yl = (tm->bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
   yh = (tm->bbox[BOXTOP]    - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

   for(bx = xl; bx <= xh; bx++)
      for(by = yl; by <= yh; by++)
         if(!P_BlockThingsIterator(bx, by, PIT_FindBelowIntersectors))
            return;

   return;
}

//
// P_DoCrunch
//
// As in zdoom, the inner core of PIT_ChangeSector isolated for effects that
// should occur when a thing definitely doesn't fit.
//
static void P_DoCrunch(mobj_t *thing)
{
   mobj_t *mo;
   
   // crunch bodies to giblets
   // TODO: support DONTGIB flag like zdoom?
   // TODO: support custom gib state for actors?
   if(thing->health <= 0)
   {
      // sf: clear the skin which will mess things up
      // haleyjd 03/11/03: not in heretic
      if(GameModeInfo->type == Game_DOOM)
      {
         thing->skin = NULL;
         P_SetMobjState(thing, E_SafeState(S_GIBS));
      }
      thing->flags &= ~MF_SOLID;
      thing->height = thing->radius = 0;
      return;
   }

   // crunch dropped items
   if(thing->flags & MF_DROPPED)
   {
      P_RemoveMobj(thing);
      return;
   }

   // killough 11/98: kill touchy things immediately
   if(thing->flags & MF_TOUCHY &&
      (thing->intflags & MIF_ARMED || sentient(thing)))
   {
      // kill object
      P_DamageMobj(thing, NULL, NULL, thing->health, MOD_CRUSH);
      return;
   }

   if(!(thing->flags & MF_SHOOTABLE))
      return;

   nofit = true;
   
   // haleyjd 06/19/00: fix for invulnerable things -- no crusher effects
   // haleyjd 05/20/05: allow custom crushing damage

   if(crushchange > 0 && !(leveltime & 3))
   {
      if(thing->flags2 & MF2_INVULNERABLE || thing->flags2 & MF2_DORMANT)
         return;

      P_DamageMobj(thing, NULL, NULL, crushchange, MOD_CRUSH);
      
      // haleyjd 06/26/06: NOBLOOD objects shouldn't bleed when crushed
      // FIXME: needs comp flag!
      if(demo_version < 333 || !(thing->flags & MF_NOBLOOD))
      {
         // spray blood in a random direction
         mo = P_SpawnMobj(thing->x, thing->y, thing->z + thing->height/2,
                          E_SafeThingType(MT_BLOOD));
         
         // haleyjd 08/05/04: use new function
         mo->momx = P_SubRandom(pr_crush) << 12;
         mo->momy = P_SubRandom(pr_crush) << 12;
         
         if(drawparticles && bloodsplat_particle)
         {
            angle_t an;
            an = (M_Random() - 128) << 24;
            
            if(bloodsplat_particle != 2)
               mo->translucency = 0;
            
            P_DrawSplash2(32, thing->x, thing->y, thing->z + thing->height/2, 
                          an, 2, thing->info->bloodcolor | MBC_BLOODMASK); 
         }
      }
   } // end if
}

// haleyjd: if true, we're moving 3DMidTex lines
static boolean midtex_moving;

//
// P_PushUp
//
// Returns 0 if thing fits, 1 if ceiling got in the way, or 2 if something
// above it didn't fit. From zdoom.
//
static int P_PushUp(mobj_t *thing)
{
   unsigned int firstintersect = intersectors.num;
   unsigned int lastintersect;
   int mymass = thing->info->mass;

   if(thing->z + thing->height > thing->ceilingz)
      return 1;

   P_FindAboveIntersectors(thing);
   lastintersect = intersectors.num;
   for(; firstintersect < lastintersect; ++firstintersect)
   {
      mobj_t *intersect = P_CollectionGetAt(&intersectors, firstintersect);
      fixed_t oldz = intersect->z;
      
      if(/*!(intersect->flags3 & MF3_PASSMOBJ) ||*/
         (!((intersect->flags & MF_COUNTKILL) ||
            (intersect->flags3 & MF3_KILLABLE)) && 
          intersect->info->mass > mymass))
      { 
         // Can't push things more massive than ourself
         return 2;
      }
      
      P_AdjustFloorCeil(intersect, midtex_moving);
      intersect->z = thing->z + thing->height + 1;
      if(P_PushUp(intersect))
      { 
         // Move blocked
         P_DoCrunch(intersect);
         intersect->z = oldz;
         return 2;
      }
   }
   return 0;
}

//
// P_PushDown
//
// Returns 0 if thing fits, 1 if floor got in the way, or 2 if something
// below it didn't fit.
//
static int P_PushDown(mobj_t *thing)
{
   unsigned int firstintersect = intersectors.num;
   unsigned int lastintersect;
   int mymass = thing->info->mass;

   if(thing->z <= thing->floorz)
      return 1;

   P_FindBelowIntersectors(thing);
   lastintersect = intersectors.num;
   for(; firstintersect < lastintersect; ++firstintersect)
   {
      mobj_t *intersect = P_CollectionGetAt(&intersectors, firstintersect);
      fixed_t oldz = intersect->z;

      if(/*!(intersect->flags3 & MF3_PASSMOBJ) || */
         (!((intersect->flags & MF_COUNTKILL) ||
            (intersect->flags3 & MF3_KILLABLE)) && 
          intersect->info->mass > mymass))
      { // Can't push things more massive than ourself
         return 2;
      }

      P_AdjustFloorCeil(intersect, midtex_moving);
      if(oldz > thing->z - intersect->height)
      { 
         // Only push things down, not up.
         intersect->z = thing->z - intersect->height;
         if(P_PushDown(intersect))
         { 
            // Move blocked
            P_DoCrunch(intersect);
            intersect->z = oldz;
            return 2;
         }
      }
   }
   return 0;
}

//
// PIT_FloorDrop
//
// haleyjd: From zdoom. It is critical for 3D object clipping for the various
// types of sector movement to be broken up. Treating them all the same as DOOM
// did leads to not being able to know who should move whom and in what
// direction, which is evidently what was mainly wrong with our own code.
//
static void PIT_FloorDrop(mobj_t *thing)
{
   fixed_t oldfloorz = thing->floorz;
   
   P_AdjustFloorCeil(thing, midtex_moving);
   
   if(thing->momz == 0 &&
      (!(thing->flags & MF_NOGRAVITY) || thing->z == oldfloorz))
   {
      // If float bob, always stay the same approximate distance above
      // the floor; otherwise only move things standing on the floor,
      // and only do it if the drop is slow enough.
      if(thing->flags2 & MF2_FLOATBOB)
      {
         thing->z = thing->z - oldfloorz + thing->floorz;
      }
      else if((thing->flags & MF_NOGRAVITY) || 
              thing->z - thing->floorz <= moveamt)
      {
         thing->z = thing->floorz;
      }
   }
}

//
// PIT_FloorRaise
//
// haleyjd: here's where the fun begins!
//
static void PIT_FloorRaise(mobj_t *thing)
{
   fixed_t oldfloorz = thing->floorz;
   
   P_AdjustFloorCeil(thing, midtex_moving);

   // Move things intersecting the floor up
   if(thing->z <= thing->floorz ||
      (!(thing->flags & MF_NOGRAVITY) && (thing->flags2 & MF2_FLOATBOB)))
   {
      fixed_t oldz = thing->z;

      P_ReInitMobjCollection(&intersectors, 0);

      if(!(thing->flags2 & MF2_FLOATBOB))
         thing->z = thing->floorz;
      else
         thing->z = thing->z - oldfloorz + thing->floorz;

      switch(P_PushUp(thing))
      {
      default:
         break;
      case 1:
         P_DoCrunch(thing);
         break;
      case 2:
         P_DoCrunch(thing);
         thing->z = oldz;
         break;
      }
   }
}

//
// PIT_CeilingLower
//
static void PIT_CeilingLower(mobj_t *thing)
{
   boolean onfloor;
   
   onfloor = thing->z <= thing->floorz;
   P_AdjustFloorCeil(thing, midtex_moving);

   if(thing->z + thing->height > thing->ceilingz)
   {
      P_ReInitMobjCollection(&intersectors, 0);

      if(thing->ceilingz - thing->height >= thing->floorz)
         thing->z = thing->ceilingz - thing->height;
      else
         thing->z = thing->floorz;

      switch(P_PushDown(thing))
      {
      case 2:
         // intentional fall-through
      case 1:
         if(onfloor)
            thing->z = thing->floorz;
         P_DoCrunch(thing);
         break;
      default:
         break;
      }
   }
}

//
// PIT_CeilingRaise
//
static void PIT_CeilingRaise(mobj_t *thing)
{
   boolean isgood = P_AdjustFloorCeil(thing, midtex_moving);
   
   // For DOOM compatibility, only move things that are inside the floor.
   // (or something else?) Things marked as hanging from the ceiling will
   // stay where they are.
   if(thing->z < thing->floorz &&
      thing->z + thing->height >= thing->ceilingz - moveamt)
   {
      thing->z = thing->floorz;

      if(thing->z + thing->height > thing->ceilingz)
         thing->z = thing->ceilingz - thing->height;
   }
   else if(/*(thing->flags3 & MF3_PASSMOBJ) &&*/ !isgood && 
           thing->z + thing->height < thing->ceilingz)
   {
      if(!P_TestMobjZ(thing) && testz_mobj->z <= thing->z)
      {
         fixed_t ceildiff = thing->ceilingz - thing->height;
         fixed_t thingtop = testz_mobj->z + testz_mobj->height;
         
         thing->z = ceildiff < thingtop ? ceildiff : thingtop;
      }
   }
}

//
// P_ChangeSector3D        [RH] Was P_CheckSector in BOOM
//
// haleyjd: 3D version of P_CheckSector. This uses all those routines above to
// do the appropriate type of sector adjustment, something that was totally
// missing for us before; of particular importance are the amt and floorOrCeil
// parameters, which have to be passed down here from code in p_map.c
//
// haleyjd: if floorOrCeil == 2, this is a 3DMidTex move. We must treat the move
// as both a floor and a ceiling move simultaneously, because things may not fit
// both above and below the 3DMidTex. Tricky.
//
boolean P_ChangeSector3D(sector_t *sector, int crunch, int amt, int floorOrCeil)
{
   void (*iterator)(mobj_t *)  = NULL;
   void (*iterator2)(mobj_t *) = NULL;
   msecnode_t *n;

   midtex_moving = false;
   nofit         = false;
   crushchange   = crunch;
   moveamt       = D_abs(amt);
   movesec       = sector;

   // [RH] Use different functions for the four different types of sector
   // movement.

   switch(floorOrCeil)
   {
   case 0: // floor
      iterator = (amt < 0) ? PIT_FloorDrop : PIT_FloorRaise;
      break;
   case 1: // ceiling
      iterator = (amt < 0) ? PIT_CeilingLower : PIT_CeilingRaise;
      break;
   case 2: // 3DMidTex -- haleyjd
      iterator  = (amt < 0) ? PIT_FloorDrop : PIT_FloorRaise;
      iterator2 = (amt < 0) ? PIT_CeilingLower : PIT_CeilingRaise;
      midtex_moving = true;
   }

   // killough 4/4/98: scan list front-to-back until empty or exhausted,
   // restarting from beginning after each thing is processed. Avoids
   // crashes, and is sure to examine all things in the sector, and only
   // the things which are in the sector, until a steady-state is reached.
   // Things can arbitrarily be inserted and removed and it won't mess up.
   //
   // killough 4/7/98: simplified to avoid using complicated counter
   
   // Mark all things invalid

   for (n = sector->touching_thinglist; n; n = n->m_snext)
      n->visited = false;

   do
   {
      for(n = sector->touching_thinglist; n; n = n->m_snext) // go through list
      {
         if(!n->visited) // unprocessed thing found
         {
            n->visited = true;                       // mark thing as processed
            if(!(n->m_thing->flags & MF_NOBLOCKMAP)) // jff 4/7/98 don't do these
            {
               iterator(n->m_thing);                 // process it
               if(iterator2)
                  iterator2(n->m_thing);
            }
            break;                                   // exit and start over
         }
      }
   } while(n); // repeat from scratch until all things left are marked valid
   
   return nofit;
}

// EOF

