// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
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
#include "d_main.h"
#include "d_mod.h"
#include "doomstat.h"
#include "e_states.h"
#include "e_things.h"
#include "m_bbox.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobjcol.h"
#include "p_inter.h"
#include "p_map3d.h"
#include "p_portal.h"
#include "p_portalclip.h"  // ioanch 20160115
#include "p_portalcross.h"
#include "p_sector.h"
#include "p_setup.h"
#include "r_main.h"
#include "r_pcheck.h"

// I HATE GLOBALS!!!
extern fixed_t FloatBobOffsets[64];

//
// P_Use3DClipping
//
// haleyjd 02/20/13: Whether we want to use 3D clipping or not is more
// complicated than just checking comp[comp_overunder] now.
//
bool P_Use3DClipping()
{
   return (!comp[comp_overunder] || useportalgroups);
}

//
// P_ZMovementTest
//
// Attempt vertical movement.
//
// haleyjd 06/28/06: Derived from P_ZMovement. Does the same logic, but
// without any side-effects so that it's reversible: the only thing changed
// is mo->z, which will be reset before the real move takes place.
//
static void P_ZMovementTest(Mobj *mo)
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
            if(!(mo->subsector->sector->intflags & SIF_SKY))
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
      // ioanch 20160110: portal aware
      if(P_AproxDistance(mo->x - getTargetX(mo), mo->y - getTargetY(mo)) <
         D_abs(delta = getTargetZ(mo) + (mo->height >> 1) - mo->z) * 3)
         mo->z += delta < 0 ? -FLOATSPEED : FLOATSPEED;
   }

   // haleyjd 06/05/12: flying players
   if(mo->player && mo->flags4 & MF4_FLY && mo->z > mo->floorz)
      mo->z += finesine[(FINEANGLES / 80 * leveltime) & FINEMASK] / 8;

   // clip movement
   
   if(mo->z <= mo->floorz)
      mo->z = mo->floorz;

   if(mo->z + mo->height > mo->ceilingz)
      mo->z = mo->ceilingz - mo->height;
}

static Mobj *testz_mobj; // used to hold object found by P_TestMobjZ

//
// PIT_TestMobjZ
//
// Derived from zdoom; iterator function for P_TestMobjZ
//
static bool PIT_TestMobjZ(Mobj *thing, void *context)
{
   fixed_t blockdist = thing->radius + clip.thing->radius;

   if(!(thing->flags & MF_SOLID) ||                      // non-solid?
      thing->flags & (MF_SPECIAL|MF_NOCLIP) || // other is special?
      clip.thing->flags & MF_SPECIAL ||                   // this is special?
      thing == clip.thing ||                              // same as self?
      clip.thing->z > thing->z + thing->height ||         // over?
      clip.thing->z + clip.thing->height <= thing->z)      // under?
   {
      return true;
   }

   // test against collision - from PIT_CheckThing:
   // ioanch 20160110: portal aware
   if(D_abs(getThingX(clip.thing, thing) - clip.x) >= blockdist || 
      D_abs(getThingY(clip.thing, thing) - clip.y) >= blockdist)
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
bool P_TestMobjZ(Mobj *mo)
{
   // a no-clipping thing is always good
   if(mo->flags & MF_NOCLIP)
      return true;

   clip.x = mo->x;
   clip.y = mo->y;
   clip.thing = mo;
   
   // standard blockmap bounding box extension
   clip.bbox[BOXLEFT]   = clip.x - mo->radius;
   clip.bbox[BOXRIGHT]  = clip.x + mo->radius;
   clip.bbox[BOXBOTTOM] = clip.y - mo->radius;
   clip.bbox[BOXTOP]    = clip.y + mo->radius;

   // ioanch 20160110: portal aware
   fixed_t bbox[4];
   bbox[BOXLEFT] = clip.bbox[BOXLEFT] - MAXRADIUS;
   bbox[BOXRIGHT] = clip.bbox[BOXRIGHT] + MAXRADIUS;
   bbox[BOXBOTTOM] = clip.bbox[BOXBOTTOM] - MAXRADIUS;
   bbox[BOXTOP] = clip.bbox[BOXTOP] + MAXRADIUS;

   if(!P_TransPortalBlockWalker(bbox, mo->groupid, true, nullptr, 
      [](int x, int y, int groupid, void *data) -> bool
   {
      return P_BlockThingsIterator(x, y, groupid, PIT_TestMobjZ);
   }))
      return false;

   return true;
}

//
// P_GetThingUnder
//
// If we're standing on something, this function will return it. Otherwise,
// it'll return NULL.
//
Mobj *P_GetThingUnder(Mobj *mo)
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
// ioanch 20160110: added optional groupid
//
static bool P_SBlockThingsIterator(int x, int y, bool (*func)(Mobj *), 
                                   Mobj *actor, int groupid = R_NOGROUP)
{
   Mobj *mobj;

   if(x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight)
      return true;
   
   if(actor == NULL)
      mobj = blocklinks[y * bmapwidth + x];
   else
      mobj = actor->bnext;

   for(; mobj; mobj = mobj->bnext)
   {
      if(groupid != R_NOGROUP && mobj->groupid != R_NOGROUP && 
         groupid != mobj->groupid)
      {
         continue;
      }
      if(!func(mobj))
         return false;
   }
   
   return true;
}

static Mobj *stepthing;

extern bool P_Touched(Mobj *thing);
extern int  P_MissileBlockHeight(Mobj *mo);
extern bool P_CheckPickUp(Mobj *thing);
extern bool P_SkullHit(Mobj *thing);

//
// PIT_CheckThing3D
// 
static bool PIT_CheckThing3D(Mobj *thing) // killough 3/26/98: make static
{
   fixed_t topz;      // haleyjd: from zdoom
   fixed_t blockdist;

   // killough 11/98: add touchy things
   if(!(thing->flags & (MF_SOLID|MF_SPECIAL|MF_SHOOTABLE|MF_TOUCHY)))
      return true;

   blockdist = thing->radius + clip.thing->radius;

   // ioanch 20160110: portal aware
   const linkoffset_t *link = P_GetLinkOffset(clip.thing->groupid, 
      thing->groupid);

   if(D_abs(thing->x - link->x - clip.x) >= blockdist ||
      D_abs(thing->y - link->y - clip.y) >= blockdist)
      return true; // didn't hit it

   // ioanch 20160122: reject if the things don't belong to the same group and
   // there's no visible connection between them
   if(clip.thing->groupid != thing->groupid)
   {
      // Important: find line portals between three coordinates
      // first get between 
      int finalgroup = clip.thing->groupid;   // default placeholder
      v2fixed_t pos = P_LinePortalCrossing(*clip.thing, clip.x - clip.thing->x, 
                                          clip.y - clip.thing->y, &finalgroup);
      P_LinePortalCrossing(pos, thing->x - link->x - pos.x, 
         thing->y - link->y - pos.y, &finalgroup);

      if(finalgroup != thing->groupid && 
         !P_ThingReachesGroupVertically(thing, finalgroup, 
                                        clip.thing->z + clip.thing->height / 2))
      {
         return true;
      }
   }

   // killough 11/98:
   //
   // This test has less information content (it's almost always false), so it
   // should not be moved up to first, as it adds more overhead than it removes.
   
   // don't clip against self
  
   if(thing == clip.thing)
      return true;

   // haleyjd 1/17/00: set global hit reference
   clip.BlockingMobj = thing;

   // haleyjd: from zdoom: OVER_UNDER
   topz = thing->z + thing->height;

   if(!(clip.thing->flags & (MF_FLOAT|MF_MISSILE|MF_SKULLFLY|MF_NOGRAVITY)) &&
      (thing->flags & MF_SOLID))
   {
      // [RH] Let monsters walk on actors as well as floors
      if(((clip.thing->flags & MF_COUNTKILL) || (clip.thing->flags3 & MF3_KILLABLE)) &&
         topz >= clip.floorz && topz <= clip.thing->z + STEPSIZE)
      {
         stepthing = thing;
         clip.floorz = topz;
      }
   }

   if(clip.thing->flags3 & MF3_PASSMOBJ)
   {
      // check if a mobj passed over/under another object
      
      // Some things prefer not to overlap each other, if possible
      if(clip.thing->flags3 & thing->flags3 & MF3_DONTOVERLAP)
         return false;

      // haleyjd: touchies need to explode if being exactly touched
      if(thing->flags & MF_TOUCHY && !(clip.thing->intflags & MIF_NOTOUCH))
      {
         if(clip.thing->z == topz || clip.thing->z + clip.thing->height == thing->z)
         {
            P_Touched(thing);
            // haleyjd: make the thing fly up a bit so it can run across
            clip.thing->momz += FRACUNIT; 
            return true;
         }
      }

      if((clip.thing->z >= topz) || (clip.thing->z + clip.thing->height <= thing->z))
         return true;
   }

   // killough 11/98:
   //
   // TOUCHY flag, for mines or other objects which die on contact with solids.
   // If a solid object of a different type comes in contact with a touchy
   // thing, and the touchy thing is not the sole one moving relative to fixed
   // surroundings such as walls, then the touchy thing dies immediately.

   if(!(clip.thing->intflags & MIF_NOTOUCH)) // haleyjd: not when just testing
      if(P_Touched(thing))
         return true;

   // check for skulls slamming into things
   
   if(P_SkullHit(thing))
      return false;

   // missiles can hit other things
   // killough 8/10/98: bouncing non-solid things can hit other things too
   
   if(clip.thing->flags & MF_MISSILE || 
      (clip.thing->flags & MF_BOUNCES && !(clip.thing->flags & MF_SOLID)))
   {
      int damage;
      // haleyjd 07/06/05: some objects may use info->height instead
      // of their current height value in this situation, to avoid
      // altering the playability of maps when 3D object clipping
      // with corrected thing heights is enabled.
      int height = P_MissileBlockHeight(thing);

      // haleyjd: some missiles can go through ghosts
      if(thing->flags3 & MF3_GHOST && clip.thing->flags3 & MF3_THRUGHOST)
         return true;

      // see if it went over / under
      
      if(clip.thing->z > thing->z + height) // haleyjd 07/06/05
         return true;    // overhead

      if(clip.thing->z + clip.thing->height < thing->z)
         return true;    // underneath

      if(clip.thing->target)
      {
         if(thing == clip.thing->target)
            return true;   // Don't hit originator.
         if(!P_AllowMissileDamage(*clip.thing->target, *thing))
            return false;
      }

      // haleyjd 10/15/08: rippers
      if(clip.thing->flags3 & MF3_RIP)
      {
         damage = ((P_Random(pr_rip) & 3) + 2) * clip.thing->damage;

         if(!(thing->flags & MF_NOBLOOD) &&
            !(thing->flags2 & MF2_REFLECTIVE) &&
            !(thing->flags2 & MF2_INVULNERABLE))
         {
            BloodSpawner(thing, clip.thing, damage, clip.thing).spawn(BLOOD_RIP);
         }
         
         // TODO: ripper sound - gamemode dependent? thing dependent?
         //S_StartSound(clip.thing, sfx_ripslop);
         
         damage = ((P_Random(pr_rip) & 3) + 2) * clip.thing->damage;
         
         P_DamageMobj(thing, clip.thing, clip.thing->target, damage, 
                      clip.thing->info->mod);
         
         if(thing->flags2 & MF2_PUSHABLE && !(clip.thing->flags3 & MF3_CANNOTPUSH))
         { 
            // Push thing
            thing->momx += clip.thing->momx >> 2;
            thing->momy += clip.thing->momy >> 2;
         }
         
         // TODO: huh?
         //numspechit = 0;
         return true;
      }

      // killough 8/10/98: if moving thing is not a missile, no damage
      // is inflicted, and momentum is reduced if object hit is solid.

      if(!(clip.thing->flags & MF_MISSILE))
      {
         if(!(thing->flags & MF_SOLID))
            return true;
         else
         {
            clip.thing->momx = -clip.thing->momx;
            clip.thing->momy = -clip.thing->momy;
            if(!(clip.thing->flags & MF_NOGRAVITY))
            {
               clip.thing->momx >>= 2;
               clip.thing->momy >>= 2;
            }

            return false;
         }
      }

      if(!(thing->flags & MF_SHOOTABLE))
         return !(thing->flags & MF_SOLID); // didn't do any damage

      // damage / explode
      
      damage = ((P_Random(pr_damage)%8)+1)*clip.thing->damage;

      // haleyjd: in Heretic & Hexen, zero-damage missiles don't make this call
      if(damage || !(clip.thing->flags4 & MF4_NOZERODAMAGE))
      {
         // 20160312: ability for missiles to draw blood
         if(clip.thing->flags4 & MF4_DRAWSBLOOD &&
            !(thing->flags & MF_NOBLOOD) &&
            !(thing->flags2 & MF2_REFLECTIVE) &&
            !(thing->flags2 & MF2_INVULNERABLE))
         {
            if(P_Random(pr_drawblood) < 192)
            {
               BloodSpawner(thing, clip.thing, damage, clip.thing).spawn(BLOOD_IMPACT);
            }
         }

         P_DamageMobj(thing, clip.thing, clip.thing->target, damage,
                      clip.thing->info->mod);
      }

      // don't traverse any more
      return false;
   }

   // haleyjd 1/16/00: Pushable objects -- at last!
   //   This is remarkably simpler than I had anticipated!
   
   if(thing->flags2 & MF2_PUSHABLE && !(clip.thing->flags3 & MF3_CANNOTPUSH))
   {
      // transfer one-fourth momentum along the x and y axes
      thing->momx += clip.thing->momx / 4;
      thing->momy += clip.thing->momy / 4;
   }

   // check for special pickup

   if(thing->flags & MF_SPECIAL
      // [RH] The next condition is to compensate for the extra height
      // that gets added by P_CheckPosition() so that you cannot pick
      // up things that are above your true height.
      && thing->z < clip.thing->z + clip.thing->height - STEPSIZE)
      return P_CheckPickUp(thing);

   // killough 3/16/98: Allow non-solid moving objects to move through solid
   // ones, by allowing the moving thing (tmthing) to move if it's non-solid,
   // despite another solid thing being in the way.
   // killough 4/11/98: Treat no-clipping things as not blocking

   return !((thing->flags & MF_SOLID && !(thing->flags & MF_NOCLIP))
          && (clip.thing->flags & MF_SOLID || demo_compatibility));
}

//
// P_CheckPosition3D
//
// A 3D version of P_CheckPosition.
//
bool P_CheckPosition3D(Mobj *thing, fixed_t x, fixed_t y, PODCollection<line_t *> *pushhit) 
{
   subsector_t *newsubsec;
   fixed_t thingdropoffz;

   // haleyjd: from zdoom:
   Mobj  *thingblocker;
   fixed_t realheight = thing->height;

#ifdef RANGECHECK
   if(GameModeInfo->type == Game_DOOM && demo_version < 329)
      I_Error("P_CheckPosition3D: called in an old demo!\n");
#endif

   clip.thing = thing;
   
   clip.x = x;
   clip.y = y;
   
   clip.bbox[BOXTOP]    = y + clip.thing->radius;
   clip.bbox[BOXBOTTOM] = y - clip.thing->radius;
   clip.bbox[BOXRIGHT]  = x + clip.thing->radius;
   clip.bbox[BOXLEFT]   = x - clip.thing->radius;
   
   newsubsec = R_PointInSubsector(x,y);
   clip.floorline = clip.blockline = clip.ceilingline = NULL; // killough 8/1/98

   // Whether object can get out of a sticky situation:
   clip.unstuck = thing->player &&        // only players
      thing->player->mo == thing;        // not voodoo dolls

   // The base floor / ceiling is from the subsector
   // that contains the point.
   // Any contacted lines the step closer together
   // will adjust them.

   // ioanch 20160110: portal aware floor and ceiling z detection
   const sector_t *bottomsector = newsubsec->sector;
#ifdef R_LINKEDPORTALS
   if(demo_version >= 333 && newsubsec->sector->f_pflags & PS_PASSABLE && 
      !(clip.thing->flags & MF_NOCLIP))
   {
      bottomsector = P_ExtremeSectorAtPoint(x, y, false, 
         newsubsec->sector);
      clip.floorz = clip.dropoffz = bottomsector->floorheight;
   }
   else
#endif
      clip.floorz = clip.dropoffz = newsubsec->sector->floorheight;

#ifdef R_LINKEDPORTALS
   if(demo_version >= 333 && newsubsec->sector->c_pflags & PS_PASSABLE &&
      !(clip.thing->flags & MF_NOCLIP))
   {
      clip.ceilingz = P_ExtremeSectorAtPoint(x, y, true, 
         newsubsec->sector)->ceilingheight;
   }
   else
#endif
      clip.ceilingz = newsubsec->sector->ceilingheight;

   clip.secfloorz = clip.passfloorz = clip.floorz;
   clip.secceilz = clip.passceilz = clip.ceilingz;

   // haleyjd
   // ioanch 20160114: use bottom sector
   clip.floorpic = bottomsector->floorpic;
   // SoM: 09/07/02: 3dsides monster fix
   clip.touch3dside = 0;
   validcount++;
   
   clip.numspechit = 0;

   // haleyjd 06/28/06: skullfly check from zdoom
   if(clip.thing->flags & MF_NOCLIP && !(clip.thing->flags & MF_SKULLFLY))
      return true;

   // Check things first, possibly picking things up.
   // The bounding box is extended by MAXRADIUS
   // because Mobjs are grouped into mapblocks
   // based on their origin point, and can overlap
   // into adjacent blocks by up to MAXRADIUS units.

   // ioanch 20160110: portal aware iterator
   fixed_t bbox[4];
   bbox[BOXLEFT] = clip.bbox[BOXLEFT] - MAXRADIUS;
   bbox[BOXRIGHT] = clip.bbox[BOXRIGHT] + MAXRADIUS;
   bbox[BOXBOTTOM] = clip.bbox[BOXBOTTOM] - MAXRADIUS;
   bbox[BOXTOP] = clip.bbox[BOXTOP] + MAXRADIUS;
   
   clip.BlockingMobj = NULL; // haleyjd 1/17/00: global hit reference
   thingblocker = NULL;
   stepthing    = NULL;

   // [RH] Fake taller height to catch stepping up into things.
   if(thing->player)   
      thing->height = realheight + STEPSIZE;

   // ioanch: portal aware
   // keep the lines indented to minimize git diff
   if(!P_TransPortalBlockWalker(bbox, thing->groupid, true,
      [thing, realheight, &thingblocker](int x, int y, int groupid) -> bool
   {
         // haleyjd: from zdoom:
         Mobj *robin = NULL;

         do
         {
            if(!P_SBlockThingsIterator(x, y, PIT_CheckThing3D, robin, groupid))
            { 
               // [RH] If a thing can be stepped up on, we need to continue checking
               // other things in the blocks and see if we hit something that is
               // definitely blocking. Otherwise, we need to check the lines, or we
               // could end up stuck inside a wall.
               if(clip.BlockingMobj == NULL)
               { 
                  // Thing slammed into something; don't let it move now.
                  thing->height = realheight;

                  return false;
               }
               else if(!clip.BlockingMobj->player && 
                       !(thing->flags & (MF_FLOAT|MF_MISSILE|MF_SKULLFLY)) &&
                       clip.BlockingMobj->z + clip.BlockingMobj->height - thing->z <= STEPSIZE)
               {
                  if(thingblocker == NULL || clip.BlockingMobj->z > thingblocker->z)
                     thingblocker = clip.BlockingMobj;
                  robin = clip.BlockingMobj;
                  clip.BlockingMobj = NULL;
               }
               else if(thing->player &&
                       thing->z + thing->height - clip.BlockingMobj->z <= STEPSIZE)
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
                  robin = clip.BlockingMobj;
                  clip.BlockingMobj = NULL;
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
         return true;
   }))
      return false;
   
   // check lines
   
   clip.BlockingMobj = NULL; // haleyjd 1/17/00: global hit reference
   thing->height = realheight;
   if(clip.thing->flags & MF_NOCLIP)
      return (clip.BlockingMobj = thingblocker) == NULL;

   memcpy(bbox, clip.bbox, sizeof(bbox));
   
   thingdropoffz = clip.floorz;
   clip.floorz = clip.dropoffz;

   // ioanch 20160121: reset portalhits and thing-visited groups
   clip.numportalhit = 0;
   if(gGroupVisit)
   {
      memset(gGroupVisit, 0, sizeof(bool) * P_PortalGroupCount());
      gGroupVisit[clip.thing->groupid] = true;
   }

   // ioanch 20160112: portal-aware
   if(!P_TransPortalBlockWalker(bbox, thing->groupid, true, pushhit, 
      [](int x, int y, int groupid, void *data) -> bool
   {
      // ioanch 20160112: try 3D portal check-line
      if(!P_BlockLinesIterator(x, y, PIT_CheckLine3D, groupid, data))
         return false; // doesn't fit
      return true;
   }))
      return false;

   // ioanch 20160121: we may have some portalhits. Check postponed visited.
   int nph = clip.numportalhit;
   clip.numportalhit = 0;  // reset it on time
   for(int i = 0; i < nph; ++i)
   {
      // they will not change the spechit list
      if(gGroupVisit[clip.portalhit[i].ld->frontsector->groupid])
         if(!PIT_CheckLine3D(clip.portalhit[i].ld, clip.portalhit[i].po, pushhit))
            return false;
   }

   if(clip.ceilingz - clip.floorz < thing->height)
      return false;
         
   if(stepthing != NULL)
      clip.dropoffz = thingdropoffz;
   
   return (clip.BlockingMobj = thingblocker) == NULL;
}

//
// P_CheckPositionExt
//
// Calls the 3D version of P_CheckPosition, and also performs an additional
// floorz/ceilingz clip. This is just for testing, and stuff like collecting
// powerups and exploding touchy objects won't happen.
//
bool P_CheckPositionExt(Mobj *mo, fixed_t x, fixed_t y, fixed_t z)
{
   unsigned int flags;
   bool xygood;
   
   // save the thing's flags, some flags must be removed to avoid side effects
   flags = mo->flags;
   mo->flags &= ~MF_PICKUP;
   mo->intflags |= MIF_NOTOUCH; // haleyjd: don't blow up touchies!

   xygood = P_CheckPosition(mo, x, y);
   mo->flags = flags;
   mo->intflags &= ~MIF_NOTOUCH;

   if(xygood)
   {
      subsector_t *newsubsec = R_PointInSubsector(x, y);
      
      if(mo->flags2 & MF2_FLOATBOB)
         z -= FloatBobOffsets[(mo->floatbob + leveltime - 1) & 63];

      if(z < newsubsec->sector->floorheight || z + mo->height > newsubsec->sector->ceilingheight)
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
static int nofit;
static MobjCollection intersectors; // haleyjd: use MobjCollection

//
// P_AdjustFloorCeil
//
// From zdoom: what our system was mostly lacking.
//
static bool P_AdjustFloorCeil(Mobj *thing, bool midtex)
{
   bool isgood;
   unsigned int oldfl3 = thing->flags3;
   
   // haleyjd: ALL things must be treated as PASSMOBJ when moving
   // 3DMidTex lines, otherwise you get stuck in them.
   if(midtex)
      thing->flags3 |= MF3_PASSMOBJ;

   // don't trigger push specials when moving strictly vertically.
   isgood = P_CheckPosition3D(thing, thing->x, thing->y);

   thing->floorz     = clip.floorz;
   thing->secfloorz  = clip.secfloorz;
   thing->passfloorz = clip.passfloorz;
   thing->ceilingz   = clip.ceilingz;
   thing->secceilz   = clip.secceilz;
   thing->passceilz  = clip.passceilz;
   thing->dropoffz   = clip.dropoffz; // killough 11/98: remember dropoffs
   
   thing->flags3 = oldfl3;

   // no sector linear interpolation tic
   // Use this to prevent all subsequent movement from interpolating if one just
   // triggered a portal teleport
   static int noseclerptic = INT_MIN;

   // Teleport thngs in the way if this is a portal sector. If targeted thing
   // is the displayplayer, prevent interpolation.
   if(noseclerptic == gametic || (demo_version >= 342 &&
      P_CheckPortalTeleport(thing) && !camera &&
      thing == players[displayplayer].mo))
   {
      // Prevent interpolation both for moving sector and player's destination 
      // sector.
      P_SaveSectorPosition(*movesec);
      P_SaveSectorPosition(*thing->subsector->sector);
      noseclerptic = gametic;
   }

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
static bool PIT_FindAboveIntersectors(Mobj *thing, void *context)
{
   fixed_t blockdist;
   if(!(thing->flags & MF_SOLID) ||               // Can't hit thing?
      (thing->flags & MF_SPECIAL) ||  // Corpse or special?
      thing == clip.thing)                         // clipping against self?
      return true;
   
   blockdist = thing->radius + clip.thing->radius;
   
   // Be portal aware
   const linkoffset_t *link = P_GetLinkOffset(clip.thing->groupid, thing->groupid);

   // Didn't hit thing?
   if(D_abs(thing->x - link->x - clip.x) >= blockdist || 
      D_abs(thing->y - link->y - clip.y) >= blockdist)
      return true;

   // Thing intersects above the base?
   if(thing->z >= clip.thing->z && thing->z <= clip.thing->z + clip.thing->height)
      intersectors.add(thing);

   return true;
}

bool PIT_FindBelowIntersectors(Mobj *thing, void *context)
{
   fixed_t blockdist;
   if(!(thing->flags & MF_SOLID) ||               // Can't hit thing?
      (thing->flags & MF_SPECIAL) ||  // Corpse or special?
      thing == clip.thing)                           // clipping against self?
      return true;

   blockdist = thing->radius + clip.thing->radius;
   
   // Be portal aware
   const linkoffset_t *link = P_GetLinkOffset(clip.thing->groupid, thing->groupid);

   // Didn't hit thing?
   if(D_abs(thing->x - link->x - clip.x) >= blockdist ||
      D_abs(thing->y - link->y - clip.y) >= blockdist)
      return true;

   if(thing->z + thing->height <= clip.thing->z + clip.thing->height &&
      thing->z + thing->height > clip.thing->z)
   { 
      // Thing intersects below the base
      intersectors.add(thing);
   }

   return true;
}

//
// P_FindAboveIntersectors
//
// Finds all the things above the thing being moved and puts them into an
// MobjCollection (this is partially explained above). From zdoom.
//
static void P_FindAboveIntersectors(Mobj *actor)
{
   fixed_t x, y;

   if(actor->flags & MF_NOCLIP)
      return;
   
   if(!(actor->flags & MF_SOLID))
      return;

   clip.x = x = actor->x;
   clip.y = y = actor->y;
   clip.thing = actor;

   clip.bbox[BOXTOP]    = y + actor->radius;
   clip.bbox[BOXBOTTOM] = y - actor->radius;
   clip.bbox[BOXRIGHT]  = x + actor->radius;
   clip.bbox[BOXLEFT]   = x - actor->radius;

   fixed_t bbox[4];
   bbox[BOXLEFT] = clip.bbox[BOXLEFT] - MAXRADIUS;
   bbox[BOXRIGHT] = clip.bbox[BOXRIGHT] + MAXRADIUS;
   bbox[BOXBOTTOM] = clip.bbox[BOXBOTTOM] - MAXRADIUS;
   bbox[BOXTOP] = clip.bbox[BOXTOP] + MAXRADIUS;

   if(!P_TransPortalBlockWalker(bbox, actor->groupid, true, nullptr,
      [](int x, int y, int groupid, void *data) -> bool {
      return P_BlockThingsIterator(x, y, groupid, PIT_FindAboveIntersectors);
   }))
      return;

   return;
}

//
// P_FindBelowIntersectors
//
// As the above function, but for things that are below the actor.
//
static void P_FindBelowIntersectors(Mobj *actor)
{
   fixed_t x, y;
   
   if(actor->flags & MF_NOCLIP)
      return;
   
   if(!(actor->flags & MF_SOLID))
      return;

   clip.x = x = actor->x;
   clip.y = y = actor->y;
   clip.thing = actor;
   
   clip.bbox[BOXTOP]    = y + actor->radius;
   clip.bbox[BOXBOTTOM] = y - actor->radius;
   clip.bbox[BOXRIGHT]  = x + actor->radius;
   clip.bbox[BOXLEFT]   = x - actor->radius;

   fixed_t bbox[4];
   bbox[BOXLEFT] = clip.bbox[BOXLEFT] - MAXRADIUS;
   bbox[BOXRIGHT] = clip.bbox[BOXRIGHT] + MAXRADIUS;
   bbox[BOXBOTTOM] = clip.bbox[BOXBOTTOM] - MAXRADIUS;
   bbox[BOXTOP] = clip.bbox[BOXTOP] + MAXRADIUS;

   if(!P_TransPortalBlockWalker(bbox, actor->groupid, true, nullptr,
      [](int x, int y, int groupid, void *data) -> bool {
      return P_BlockThingsIterator(x, y, groupid, PIT_FindBelowIntersectors);
   }))
      return;

   return;
}

//
// P_DoCrunch
//
// As in zdoom, the inner core of PIT_ChangeSector isolated for effects that
// should occur when a thing definitely doesn't fit.
//
static void P_DoCrunch(Mobj *thing)
{
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
      thing->remove();
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

   nofit = 1;
   
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
         BloodSpawner(thing, crushchange).spawn(BLOOD_CRUSH);
      }
   } // end if
}

// haleyjd: if true, we're moving 3DMidTex lines
static bool midtex_moving;

//
// P_PushUp
//
// Returns 0 if thing fits, 1 if ceiling got in the way, or 2 if something
// above it didn't fit. From zdoom.
//
static int P_PushUp(Mobj *thing)
{
   unsigned int firstintersect = static_cast<unsigned>(intersectors.getLength());
   unsigned int lastintersect;
   int mymass = thing->info->mass;

   if(thing->z + thing->height > thing->ceilingz)
      return 1;

   P_FindAboveIntersectors(thing);
   lastintersect = static_cast<unsigned>(intersectors.getLength());
   for(; firstintersect < lastintersect; ++firstintersect)
   {
      Mobj *intersect = intersectors[firstintersect];
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
static int P_PushDown(Mobj *thing)
{
   unsigned int firstintersect = static_cast<unsigned>(intersectors.getLength());
   unsigned int lastintersect;
   int mymass = thing->info->mass;

   if(thing->z <= thing->floorz)
      return 1;

   P_FindBelowIntersectors(thing);
   lastintersect = static_cast<unsigned>(intersectors.getLength());
   for(; firstintersect < lastintersect; ++firstintersect)
   {
      Mobj *intersect = intersectors[firstintersect];
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
static void PIT_FloorDrop(Mobj *thing)
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
static void PIT_FloorRaise(Mobj *thing)
{
   fixed_t oldfloorz = thing->floorz;
   
   P_AdjustFloorCeil(thing, midtex_moving);

   // Move things intersecting the floor up
   if(thing->z <= thing->floorz ||
      (!(thing->flags & MF_NOGRAVITY) && (thing->flags2 & MF2_FLOATBOB)))
   {
      fixed_t oldz = thing->z;

      intersectors.makeEmpty();

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
static void PIT_CeilingLower(Mobj *thing)
{
   bool onfloor;
   
   onfloor = thing->z <= thing->floorz;
   P_AdjustFloorCeil(thing, midtex_moving);

   if(thing->z + thing->height > thing->ceilingz)
   {
      intersectors.makeEmpty();

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
static void PIT_CeilingRaise(Mobj *thing)
{
   bool isgood = P_AdjustFloorCeil(thing, midtex_moving);
   
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
bool P_ChangeSector3D(sector_t *sector, int crunch, int amt, int floorOrCeil)
{
   void (*iterator)(Mobj *)  = NULL;
   void (*iterator2)(Mobj *) = NULL;
   msecnode_t *n;

   midtex_moving = false;
   nofit         = 0;
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
      break; // haleyjd 10/29/09: probably nice.
   default:
      I_Error("P_ChangeSector3D: unknown movement type %d\n", floorOrCeil);
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
         // ioanch 20160115: portal aware
         if(useportalgroups && full_demo_version >= make_full_version(340, 48) &&
            !P_SectorTouchesThingVertically(sector, n->m_thing))
         {
            continue;
         }
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
   
   return !!nofit;
}

// EOF

