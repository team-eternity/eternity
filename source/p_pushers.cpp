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
//   BOOM Push / Pull / Current Effects
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "a_small.h"
#include "doomstat.h"
#include "e_states.h"
#include "e_things.h"
#include "ev_specials.h"
#include "m_bbox.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_portal.h"   // ioanch 20160115: portal aware
#include "p_portalcross.h"
#include "p_pushers.h"
#include "p_saveg.h"
#include "p_setup.h"
#include "p_spec.h"
#include "r_defs.h"
#include "r_state.h"

//=============================================================================
//
// PUSH/PULL EFFECT
//
// phares 3/20/98: Start of push/pull effects
//
// This is where push/pull effects are applied to objects in the sectors.
//
// There are four kinds of push effects
//
// 1) Pushing Away
//
//    Pushes you away from a point source defined by the location of an
//    PUSH Thing. The force decreases linearly with distance from the
//    source. This force crosses sector boundaries and is felt w/in a circle
//    whose center is at the PUSH. The force is felt only if the point
//    PUSH can see the target object.
//
// 2) Pulling toward
//
//    Same as Pushing Away except you're pulled toward an PULL point
//    source. This force crosses sector boundaries and is felt w/in a circle
//    whose center is at the PULL. The force is felt only if the point
//    PULL can see the target object.
//
// 3) Wind
//
//    Pushes you in a constant direction. Full force above ground, half
//    force on the ground, nothing if you're below it (water).
//
// 4) Current
//
//    Pushes you in a constant direction. No force above ground, full
//    force if on the ground or below it (water).
//
// The magnitude of the force is controlled by the length of a controlling
// linedef. The force vector for types 3 & 4 is determined by the angle
// of the linedef, and is constant.
//
// For each sector where these effects occur, the sector special type has
// to have the PUSH_MASK bit set. If this bit is turned off by a switch
// at run-time, the effect will not occur. The controlling sector for
// types 1 & 2 is the sector containing the PUSH/PULL Thing.

#define PUSH_FACTOR 7

//
// Add_Pusher
//
// Add a push thinker to the thinker list
//
static void Add_Pusher(int type, int x_mag, int y_mag,
                       Mobj *source, int affectee)
{
   PushThinker *p = new PushThinker;
   
   p->source = source;
   p->type = type;
   p->x_mag = x_mag>>FRACBITS;
   p->y_mag = y_mag>>FRACBITS;
   p->magnitude = P_AproxDistance(p->x_mag,p->y_mag);
   if(source) // point source exist?
   {
      p->radius = (p->magnitude)<<(FRACBITS+1); // where force goes to zero
      p->x = p->source->x;
      p->y = p->source->y;
   }
   p->affectee = affectee;
   p->addThinker();
}

static PushThinker *tmpusher; // pusher structure for blockmap searches

//
// PIT_PushThing determines the angle and magnitude of the effect.
// The object's x and y momentum values are changed.
//
// tmpusher belongs to the point source (PUSH/PULL).
//
// killough 10/98: allow to affect things besides players
//
static bool PIT_PushThing(Mobj* thing, void *context)
{
   if(demo_version < 203  ?     // killough 10/98: made more general
      thing->player && !(thing->flags & (MF_NOCLIP | MF_NOGRAVITY)) :
      (sentient(thing) || thing->flags & MF_SHOOTABLE) &&
      !(thing->flags & MF_NOCLIP) &&
      !(thing->flags2 & MF2_NOTHRUST)) // haleyjd
   {
      angle_t pushangle;
      fixed_t speed;
      fixed_t sx = tmpusher->x;
      fixed_t sy = tmpusher->y;

      speed = (tmpusher->magnitude -
               ((P_AproxDistance(thing->x - sx,thing->y - sy)
                 >>FRACBITS)>>1))<<(FRACBITS-PUSH_FACTOR-1);

      // killough 10/98: make magnitude decrease with square
      // of distance, making it more in line with real nature,
      // so long as it's still in range with original formula.
      //
      // Removes angular distortion, and makes effort required
      // to stay close to source, grow increasingly hard as you
      // get closer, as expected. Still, it doesn't consider z :(

      if(speed > 0 && demo_version >= 203)
      {
         int x = (thing->x-sx) >> FRACBITS;
         int y = (thing->y-sy) >> FRACBITS;
         speed = (fixed_t)(((int64_t)tmpusher->magnitude << 23) / (x*x+y*y+1));
      }

      // If speed <= 0, you're outside the effective radius. You also have
      // to be able to see the push/pull source point.

      if(speed > 0 && P_CheckSight(thing, tmpusher->source))
      {
         pushangle = P_PointToAngle(thing->x, thing->y, sx, sy);
         
         if(tmpusher->source->type == E_ThingNumForDEHNum(MT_PUSH))
            pushangle += ANG180;    // away
         
         P_ThrustMobj(thing, pushangle, speed);
      }
   }
   return true;
}

IMPLEMENT_THINKER_TYPE(PushThinker)

//
// T_Pusher 
//
// Thinker function for BOOM push/pull effects that looks for all 
// objects that are inside the radius of the effect.
//
void PushThinker::Think()
{
   sector_t   *sec;
   Mobj     *thing;
   msecnode_t *node;
   int xspeed, yspeed;
   int xl, xh, yl, yh, bx, by;
   int radius;
   int ht = 0;
   
   if(!allow_pushers)
      return;

   sec = sectors + this->affectee;
   
   // Be sure the special sector type is still turned on. If so, proceed.
   // Else, bail out; the sector type has been changed on us.
   
   if(!(sec->flags & SECF_PUSH))
      return;

   // For constant pushers (wind/current) there are 3 situations:
   //
   // 1) Affected Thing is above the floor.
   //
   //    Apply the full force if wind, no force if current.
   //
   // 2) Affected Thing is on the ground.
   //
   //    Apply half force if wind, full force if current.
   //
   // 3) Affected Thing is below the ground (underwater effect).
   //
   //    Apply no force if wind, full force if current.
   //
   // haleyjd:
   // 4) Affected thing bears MF2_NOTHRUST flag
   //
   //    Apply nothing at any time!

   if(this->type == PushThinker::p_push)
   {
      // Seek out all pushable things within the force radius of this
      // point pusher. Crosses sectors, so use blockmap.

      tmpusher = this; // PUSH/PULL point source
      radius = this->radius; // where force goes to zero
      clip.bbox[BOXTOP]    = this->y + radius;
      clip.bbox[BOXBOTTOM] = this->y - radius;
      clip.bbox[BOXRIGHT]  = this->x + radius;
      clip.bbox[BOXLEFT]   = this->x - radius;
      
      xl = (clip.bbox[BOXLEFT]   - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
      xh = (clip.bbox[BOXRIGHT]  - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
      yl = (clip.bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
      yh = (clip.bbox[BOXTOP]    - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

      for (bx = xl; bx <= xh; bx++)
      {
         for(by = yl; by <= yh; by++)
            P_BlockThingsIterator(bx, by, PIT_PushThing);
      }
      return;
   }

   // constant pushers p_wind and p_current
   
   if(sec->heightsec != -1) // special water sector?
      ht = sectors[sec->heightsec].floorheight;

   node = sec->touching_thinglist; // things touching this sector

   for( ; node; node = node->m_snext)
    {
      // ioanch 20160115: portal aware
      if(useportalgroups && full_demo_version >= make_full_version(340, 48) &&
         !P_SectorTouchesThingVertically(sec, node->m_thing))
      {
         continue;
      }

      thing = node->m_thing;
      if(!thing->player || 
         (thing->flags2 & MF2_NOTHRUST) ||                // haleyjd
         (thing->flags & (MF_NOGRAVITY | MF_NOCLIP)))
         continue;

      if(this->type == PushThinker::p_wind)
      {
         if(sec->heightsec == -1) // NOT special water sector
         {
            if(thing->z > thing->floorz) // above ground
            {
               xspeed = this->x_mag; // full force
               yspeed = this->y_mag;
            }
            else // on ground
            {
               xspeed = (this->x_mag)>>1; // half force
               yspeed = (this->y_mag)>>1;
            }
         }
         else // special water sector
         {
            if(thing->z > ht) // above ground
            {
               xspeed = this->x_mag; // full force
               yspeed = this->y_mag;
            }
            else if(thing->player->viewz < ht) // underwater
               xspeed = yspeed = 0; // no force
            else // wading in water
            {
               xspeed = (this->x_mag)>>1; // half force
               yspeed = (this->y_mag)>>1;
            }
         }
      }
      else // p_current
      {
         if(sec->heightsec == -1) // NOT special water sector
         {
            if(thing->z > sec->floorheight) // above ground
               xspeed = yspeed = 0; // no force
            else // on ground
            {
               xspeed = this->x_mag; // full force
               yspeed = this->y_mag;
            }
         }
         else // special water sector
         {
            if(thing->z > ht) // above ground
               xspeed = yspeed = 0; // no force
            else // underwater
            {
               xspeed = this->x_mag; // full force
               yspeed = this->y_mag;
            }
         }
      }
      thing->momx += xspeed<<(FRACBITS-PUSH_FACTOR);
      thing->momy += yspeed<<(FRACBITS-PUSH_FACTOR);
   }
}

//
// PushThinker::serialize
//
// Saves/loads a PushThinker thinker.
//
void PushThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << type << x_mag << y_mag << magnitude << radius << x << y << affectee;

   // Restore point source origin if loading
   if(arc.isLoading())
      source = P_GetPushThing(affectee);
}

//
// P_GetPushThing
//
// returns a pointer to an PUSH or PULL thing, NULL otherwise.
//
Mobj* P_GetPushThing(int s)
{
   Mobj *thing;
   sector_t *sec;
   int PushType = E_ThingNumForDEHNum(MT_PUSH); 
   int PullType = E_ThingNumForDEHNum(MT_PULL);

   sec = sectors + s;
   thing = sec->thinglist;

   while(thing)
   {
      if(thing->type == PushType || thing->type == PullType)
         return thing;

      thing = thing->snext;
   }
   
   return NULL;
}

//
// P_spawnHereticWind
//
// haleyjd 03/12/03: Heretic Wind/Current Transfer specials
//
static void P_spawnHereticWind(int tag, fixed_t x_mag, fixed_t y_mag, int pushType)
{
   int s;
   angle_t lineAngle;
   fixed_t magnitude;

   lineAngle = P_PointToAngle(0, 0, x_mag, y_mag);
   magnitude = (P_AproxDistance(x_mag, y_mag) >> FRACBITS) * 512;

   // types 20-39 affect the player in P_PlayerThink
   // types 40-51 affect MF3_WINDTHRUST things in P_MobjThinker
   // this is selected by use of lines 294 or 293, respectively

   for(s = -1; (s = P_FindSectorFromTag(tag, s)) >= 0; )
   {
      sectors[s].hticPushType  = pushType;
      sectors[s].hticPushAngle = lineAngle;
      sectors[s].hticPushForce = magnitude;
   }
}

//
// Computes wind or current strength from parameterized linedef
//
static void P_getPusherParams(const line_t *line, int &x_mag, int &y_mag)
{
   if(line->args[ev_SetWind_Arg_Flags] & ev_SetWind_Flag_UseLine)
   {
      x_mag = line->dx;
      y_mag = line->dy;
   }
   else
   {
      fixed_t strength = line->args[ev_SetWind_Arg_Strength] << FRACBITS;
      angle_t angle = line->args[ev_SetWind_Arg_Angle] << 24;
      int fineangle = angle >> ANGLETOFINESHIFT;
      x_mag = FixedMul(strength, finecosine[fineangle]);
      y_mag = FixedMul(strength, finesine[fineangle]);
   }
}

//
// P_SpawnPushers
//
// Initialize the sectors where pushers are present
//
void P_SpawnPushers()
{
   int i, s;
   line_t *line = lines;

   for(i = 0; i < numlines; i++, line++)
   {
      // haleyjd 02/03/13: get special binding
      int staticFn;
      if(!(staticFn = EV_StaticInitForSpecial(line->special)))
         continue;

      switch(staticFn)
      {
      case EV_STATIC_WIND_CONTROL: // wind
         for(s = -1; (s = P_FindSectorFromLineArg0(line, s)) >= 0; )
            Add_Pusher(PushThinker::p_wind, line->dx, line->dy, NULL, s);
         break;

      case EV_STATIC_WIND_CONTROL_PARAM:
         {
            int x_mag, y_mag;
            P_getPusherParams(line, x_mag, y_mag);

            if(line->args[ev_SetWind_Arg_Flags] & ev_SetWind_Flag_Heretic)
               P_spawnHereticWind(line->args[0], x_mag, y_mag, SECTOR_HTIC_WIND);
            else
            {
               for(s = -1; (s = P_FindSectorFromLineArg0(line, s)) >= 0; )
                  Add_Pusher(PushThinker::p_wind, x_mag, y_mag, NULL, s);
            }
            break;
         }

      case EV_STATIC_CURRENT_CONTROL: // current
         for(s = -1; (s = P_FindSectorFromLineArg0(line, s)) >= 0; )
            Add_Pusher(PushThinker::p_current, line->dx, line->dy, NULL, s);
         break;

      case EV_STATIC_CURRENT_CONTROL_PARAM:
         {
            int x_mag, y_mag;
            P_getPusherParams(line, x_mag, y_mag);

            if(line->args[ev_SetWind_Arg_Flags] & ev_SetWind_Flag_Heretic)
               P_spawnHereticWind(line->args[0], x_mag, y_mag, SECTOR_HTIC_CURRENT);
            else 
            {
               for(s = -1; (s = P_FindSectorFromLineArg0(line, s)) >= 0; )
                  Add_Pusher(PushThinker::p_current, x_mag, y_mag, NULL, s);
            }
            break;
         }

      case EV_STATIC_PUSHPULL_CONTROL: // push/pull
         for(s = -1; (s = P_FindSectorFromLineArg0(line, s)) >= 0; )
         {
            Mobj *thing = P_GetPushThing(s);
            if(thing) // No P* means no effect
               Add_Pusher(PushThinker::p_push, line->dx, line->dy, thing, s);
         }
         break;

      case EV_STATIC_PUSHPULL_CONTROL_PARAM:
         {
            int tag = line->args[0];
            int x_mag, y_mag;
            if(line->args[3])
            {
               x_mag = line->dx;
               y_mag = line->dy;
            }
            else
            {
               x_mag = line->args[2] << FRACBITS;
               y_mag = 0;
            }

            if(tag)
            {
               for(s = -1; (s = P_FindSectorFromTag(tag, s)) >= 0; )
               {
                  Mobj *thing = P_GetPushThing(s);
                  if(thing) // No P* means no effect
                  {
                     Add_Pusher(PushThinker::p_push, line->dx, line->dy, thing,
                                s);
                  }
               }
            }
            else
            {
               Mobj *thing = nullptr;
               const int PushType = E_ThingNumForDEHNum(MT_PUSH);
               const int PullType = E_ThingNumForDEHNum(MT_PULL);
               while((thing = P_FindMobjFromTID(line->args[1], thing, nullptr)))
               {
                  if(thing->type == PushType || thing->type == PullType)
                  {
                     Add_Pusher(PushThinker::p_push, x_mag, y_mag, thing,
                                eindex(thing->subsector->sector - sectors));
                  }
               }
            }
            break;
         }

      case EV_STATIC_HERETIC_WIND:
      case EV_STATIC_HERETIC_CURRENT:
         // haleyjd 03/12/03: Heretic wind and current transfer specials
         P_spawnHereticWind(line->args[0], line->dx, line->dy, 
            staticFn == EV_STATIC_HERETIC_CURRENT ? SECTOR_HTIC_CURRENT 
                                                  : SECTOR_HTIC_WIND);
         break;

      default: // not a function we handle here
         break;
      }
   }
}

//
// phares 3/20/98: End of Pusher effects
//
//=============================================================================

// EOF

