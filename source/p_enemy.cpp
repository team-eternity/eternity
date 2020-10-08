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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Enemy thinking, AI.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

// Some action functions are still needed here.
#include "a_args.h"
#include "a_common.h"
#include "a_doom.h"

#include "a_small.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "d_gi.h"
#include "d_io.h"
#include "d_mod.h"
#include "doomstat.h"
#include "e_args.h"
#include "e_lib.h"
#include "e_player.h"
#include "e_sound.h"
#include "e_states.h"
#include "e_things.h"
#include "e_ttypes.h"
#include "ev_specials.h"
#include "g_game.h"
#include "m_bbox.h"
#include "metaapi.h"
#include "p_anim.h"      // haleyjd
#include "p_enemy.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_map3d.h"
#include "p_maputl.h"
#include "p_mobjcol.h"
#include "p_partcl.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_pcheck.h"
#include "r_portal.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"
#include "w_wad.h"

extern fixed_t FloatBobOffsets[64]; // haleyjd: Float Bobbing

static Mobj *current_actor;

//
// ENEMY THINKING
// Enemies are allways spawned
// with targetplayer = -1, threshold = 0
// Most monsters are spawned unaware of all players,
// but some can be made preaware
//

//
// P_RecursiveSound
//
// Called by P_NoiseAlert.
// Recursively traverse adjacent sectors,
// sound blocking lines cut off traversal.
//
// killough 5/5/98: reformatted, cleaned up
//
static void P_RecursiveSound(sector_t *sec, int soundblocks,
                             Mobj *soundtarget)
{
   int i;
   
   // wake up all monsters in this sector
   if(sec->validcount == validcount &&
      sec->soundtraversed <= soundblocks+1)
      return;             // already flooded

   sec->validcount = validcount;
   sec->soundtraversed = soundblocks+1;
   P_SetTarget<Mobj>(&sec->soundtarget, soundtarget);    // killough 11/98

   if(sec->srf.floor.pflags & PS_PASSSOUND)
   {
      // Ok, because the same portal can be used on many sectors and even
      // lines, the portal structure won't tell you what sector is on the
      // other side of the portal. SO
      sector_t *other;
      line_t *check = sec->lines[0];

      other = 
         R_PointInSubsector(((check->v1->x + check->v2->x) / 2) + R_FPLink(sec)->delta.x,
                            ((check->v1->y + check->v2->y) / 2) + R_FPLink(sec)->delta.y)->sector;

      P_RecursiveSound(other, soundblocks, soundtarget);
   }
   
   if(sec->srf.ceiling.pflags & PS_PASSSOUND)
   {
      // Ok, because the same portal can be used on many sectors and even 
      // lines, the portal structure won't tell you what sector is on the 
      // other side of the portal. SO
      sector_t *other;
      line_t *check = sec->lines[0];

      other = 
         R_PointInSubsector(((check->v1->x + check->v2->x) / 2) + R_CPLink(sec)->delta.x,
                            ((check->v1->y + check->v2->y) / 2) + R_CPLink(sec)->delta.y)->sector;

      P_RecursiveSound(other, soundblocks, soundtarget);
   }

   for(i=0; i<sec->linecount; i++)
   {
      sector_t *other;
      line_t *check = sec->lines[i];
      
      if(check->pflags & PS_PASSSOUND)
      {
         sector_t *iother;

         iother = 
         R_PointInSubsector(((check->v1->x + check->v2->x) / 2) + check->portal->data.link.delta.x,
                            ((check->v1->y + check->v2->y) / 2) + check->portal->data.link.delta.y)->sector;

         P_RecursiveSound(iother, soundblocks, soundtarget);
      }
      if(!(check->flags & ML_TWOSIDED))
         continue;

      P_LineOpening(check, nullptr);
      
      if(clip.openrange <= 0)
         continue;       // closed door

      other=sides[check->sidenum[sides[check->sidenum[0]].sector==sec]].sector;
      
      if(!(check->flags & ML_SOUNDBLOCK))
         P_RecursiveSound(other, soundblocks, soundtarget);
      else if(!soundblocks)
         P_RecursiveSound(other, 1, soundtarget);
   }
}

//
// P_NoiseAlert
//
// If a monster yells at a player,
// it will alert other monsters to the player.
//
void P_NoiseAlert(Mobj *target, Mobj *emitter)
{
   validcount++;
   P_RecursiveSound(emitter->subsector->sector, 0, target);
}

//
// P_CheckMeleeRange
//
bool P_CheckMeleeRange(Mobj *actor)
{
   Mobj *pl = actor->target;
   if (!pl)
       return false;
   
   // haleyjd 02/15/02: revision of joel's fix for z height check
   if(P_Use3DClipping())
   {
      if(pl->z > actor->z + actor->height || // pl is too far above
         actor->z > pl->z + pl->height)      // pl is too far below
         return false;
      // TODO: add support for Strife's z-clipping which doesn't limit vertical
      // range if target is below attacker!
   }

   // ioanch 20151225: make it linked-portal aware
   fixed_t tx, ty;
#ifdef R_LINKEDPORTALS
   tx = getThingX(actor, pl);
   ty = getThingY(actor, pl);
#else
   tx = pl->x;
   ty = pl->y;
#endif

   fixed_t range = GameModeInfo->monsterMeleeRange == meleecalc_raven ?
   MELEERANGE : MELEERANGE - 20 * FRACUNIT + pl->info->radius;

   return  // killough 7/18/98: friendly monsters don't attack other friends
      pl && !(actor->flags & pl->flags & MF_FRIEND) &&
      (P_AproxDistance(tx - actor->x, ty - actor->y) < range) &&
      P_CheckSight(actor, actor->target);
}

//
// P_HitFriend()
//
// killough 12/98
// This function tries to prevent shooting at friends
//
// haleyjd 09/22/02: reformatted
// haleyjd 09/22/09: rewrote to de-Killoughify ;)
//                   also added ignoring of things with NOFRIENDDMG flag
//
bool P_HitFriend(Mobj *actor)
{
   bool hitfriend = false;

   if(actor->target)
   {
      angle_t angle;
      fixed_t dist, tx, ty;

#ifdef R_LINKEDPORTALS
      tx = getTargetX(actor);
      ty = getTargetY(actor);
#else
      tx = actor->target->x;
      ty = actor->target->y;
#endif
      angle = P_PointToAngle(actor->x, actor->y, tx, ty);
      dist  = P_AproxDistance(actor->x - tx, actor->y - ty);

      P_AimLineAttack(actor, angle, dist, false);

      if(clip.linetarget 
         && clip.linetarget != actor->target 
         && !((clip.linetarget->flags ^ actor->flags) & MF_FRIEND) 
         && !(clip.linetarget->flags3 & MF3_NOFRIENDDMG))
      {
         hitfriend = true;
      }
   }

   return hitfriend;
}

//
// P_CheckMissileRange
//
bool P_CheckMissileRange(Mobj *actor)
{
   fixed_t dist;
   
   if(!P_CheckSight(actor, actor->target))
      return false;
   
   if (actor->flags & MF_JUSTHIT)
   {      // the target just hit the enemy, so fight back!
      actor->flags &= ~MF_JUSTHIT;

      // killough 7/18/98: no friendly fire at corpses
      // killough 11/98: prevent too much infighting among friends

      return 
         !(actor->flags & MF_FRIEND) || 
         (actor->target->health > 0 &&
          (!(actor->target->flags & MF_FRIEND) ||
          (actor->target->player ? 
           monster_infighting || P_Random(pr_defect) >128 :
           !(actor->target->flags & MF_JUSTHIT) && P_Random(pr_defect) >128)));
   }

   // killough 7/18/98: friendly monsters don't attack other friendly
   // monsters or players (except when attacked, and then only once)
   if(actor->flags & actor->target->flags & MF_FRIEND)
      return false;
   
   if(actor->reactiontime)
      return false;       // do not attack yet

   // OPTIMIZE: get this from a global checksight
#ifdef R_LINKEDPORTALS
   dist = P_AproxDistance(actor->x - getTargetX(actor),
                          actor->y - getTargetY(actor)) - 64*FRACUNIT;
#else
   dist = P_AproxDistance(actor->x - actor->target->x,
                          actor->y - actor->target->y) - 64*FRACUNIT;
#endif

   if(actor->info->meleestate == NullStateNum)
      dist -= 128*FRACUNIT;       // no melee attack, so fire more

   dist >>= FRACBITS;

   // haleyjd 01/09/02: various changes made below to move
   // thing-type-specific differences in AI into flags

   if(actor->flags2 & MF2_SHORTMRANGE)
   {
      if(dist > 14*64)
         return false;     // too far away
   }

   if(actor->flags2 & MF2_LONGMELEE)
   {
      if (dist < 196)
         return false;   // close for fist attack
      // dist >>= 1;  -- this is now handled below
   }

   if(actor->flags2 & MF2_RANGEHALF)
      dist >>= 1;

   if(dist > 200)
      dist = 200;

   if((actor->flags2 & MF2_HIGHERMPROB) && dist > 160)
      dist = 160;

   if(P_Random(pr_missrange) < dist)
      return false;
  
   if((actor->flags & MF_FRIEND) && P_HitFriend(actor))
      return false;

   return true;
}

//
// P_IsOnLift
//
// killough 9/9/98:
//
// Returns true if the object is on a lift. Used for AI,
// since it may indicate the need for crowded conditions,
// or that a monster should stay on the lift for a while
// while it goes up or down.
//
static bool P_IsOnLift(const Mobj *actor)
{
   const sector_t *sec = actor->subsector->sector;
   line_t line;

   // Short-circuit: it's on a lift which is active.
   if(thinker_cast<PlatThinker *>(sec->srf.floor.data) != nullptr)
      return true;

   // Check to see if it's in a sector which can be 
   // activated as a lift.
   // ioanch 20160303: use args[0]
   if((line.args[0] = sec->tag))
   {
      for(int l = -1; (l = P_FindLineFromLineArg0(&line, l)) >= 0;)
      {
         // FIXME: I'm still keeping the old code because I don't know of any MBF
         // demos which can verify all of this. If you're confident you found one,
         // feel free to remove this block.
         if(demo_version <= 203)
         {
            switch(lines[l].special)
            {
            case  10: case  14: case  15: case  20: case  21: case  22:
            case  47: case  53: case  62: case  66: case  67: case  68:
            case  87: case  88: case  95: case 120: case 121: case 122:
            case 123: case 143: case 162: case 163: case 181: case 182:
            case 144: case 148: case 149: case 211: case 227: case 228:
            case 231: case 232: case 235: case 236:
               return true;
            }
         }
         else if(EV_CompositeActionFlags(EV_ActionForSpecial(lines[l].special)) &
            EV_ISMBFLIFT)
         {
            return true;
         }
      }
   }
   
   return false;
}

//
// P_IsUnderDamage
//
// killough 9/9/98:
//
// Returns nonzero if the object is under damage based on
// their current position. Returns 1 if the damage is moderate,
// -1 if it is serious. Used for AI.
//
static int P_IsUnderDamage(Mobj *actor)
{ 
   const msecnode_t *seclist;
   const CeilingThinker *cl;             // Crushing ceiling
   int dir = 0;

   for(seclist = actor->touching_sectorlist; seclist; seclist = seclist->m_tnext)
   {
      if((cl = thinker_cast<CeilingThinker *>(seclist->m_sector->srf.ceiling.data)) &&
         !cl->inStasis)
         dir |= cl->direction;
   }

   return dir;
}

fixed_t xspeed[8] = {FRACUNIT,47000,0,-47000,-FRACUNIT,-47000,0,47000};
fixed_t yspeed[8] = {0,47000,FRACUNIT,47000,0,-47000,-FRACUNIT,-47000};

// 1/11/98 killough: Limit removed on special lines crossed
extern  line_t **spechit;          // New code -- killough
extern  int    numspechit;

//
// P_Move
//
// Move in the current direction; returns false if the move is blocked.
//
int P_Move(Mobj *actor, int dropoff) // killough 9/12/98
{
   fixed_t tryx, tryy, deltax, deltay;
   bool try_ok;
   int movefactor = ORIG_FRICTION_FACTOR;    // killough 10/98
   int friction = ORIG_FRICTION;
   int speed;

   if(actor->movedir == DI_NODIR)
      return false;

   // haleyjd: OVER_UNDER:
   // [RH] Instead of yanking non-floating monsters to the ground,
   // let gravity drop them down, unless they're moving down a step.
   if(P_Use3DClipping())
   {
      if(!(actor->flags & MF_FLOAT) && actor->z > actor->zref.floor &&
         !(actor->intflags & MIF_ONMOBJ))
      {
         if (actor->z > actor->zref.floor + STEPSIZE)
            return false;
         else
            actor->z = actor->zref.floor;
      }
   }

#ifdef RANGECHECK
   if((unsigned int)actor->movedir >= 8)
      I_Error ("Weird actor->movedir!\n");
#endif
   
   // killough 10/98: make monsters get affected by ice and sludge too:

   if(monster_friction)
      movefactor = P_GetMoveFactor(actor, &friction);

   speed = actor->info->speed;

   if(friction < ORIG_FRICTION &&     // sludge
      !(speed = ((ORIG_FRICTION_FACTOR - (ORIG_FRICTION_FACTOR-movefactor)/2)
        * speed) / ORIG_FRICTION_FACTOR))
   {
      speed = 1;      // always give the monster a little bit of speed
   }

   tryx = actor->x + (deltax = speed * xspeed[actor->movedir]);
   tryy = actor->y + (deltay = speed * yspeed[actor->movedir]);

   // killough 12/98: rearrange, fix potential for stickiness on ice

   if(friction <= ORIG_FRICTION)
      try_ok = P_TryMove(actor, tryx, tryy, dropoff);
   else
   {
      fixed_t x = actor->x;
      fixed_t y = actor->y;
      fixed_t floorz = actor->zref.floor;
      int floorgroupid = actor->zref.floorgroupid;
      fixed_t ceilingz = actor->zref.ceiling;
      fixed_t dropoffz = actor->zref.dropoff;
      
      try_ok = P_TryMove(actor, tryx, tryy, dropoff);
      
      // killough 10/98:
      // Let normal momentum carry them, instead of steptoeing them across ice.

      if(try_ok)
      {
         P_UnsetThingPosition(actor);
         actor->x = x;
         actor->y = y;
         actor->zref.floor = floorz;
         actor->zref.floorgroupid = floorgroupid;
         actor->zref.ceiling = ceilingz;
         actor->zref.dropoff = dropoffz;
         P_SetThingPosition(actor);
         movefactor *= FRACUNIT / ORIG_FRICTION_FACTOR / 4;
         actor->momx += FixedMul(deltax, movefactor);
         actor->momy += FixedMul(deltay, movefactor);
      }
   }
   
   if(!try_ok)
   {      // open any specials
      int good;
      
      if(actor->flags & MF_FLOAT && clip.floatok)
      {
         fixed_t savedz = actor->z;

         if(actor->z < clip.zref.floor)          // must adjust height
            actor->z += FLOATSPEED;
         else
            actor->z -= FLOATSPEED;

         // haleyjd: OVER_UNDER:
         // [RH] Check to make sure there's nothing in the way of the float
         if(P_Use3DClipping())
         {
            if(P_TestMobjZ(actor, clip))
            {
               actor->flags |= MF_INFLOAT;
               return true;
            }
            actor->z = savedz;
         }
         else
         {
            actor->flags |= MF_INFLOAT;            
            return true;
         }
      }

      if(!clip.numspechit)
         return false;

#ifdef RANGECHECK
      // haleyjd 01/09/07: SPECHIT_DEBUG
      if(clip.numspechit < 0)
         I_Error("P_Move: numspechit == %d\n", clip.numspechit);
#endif

      actor->movedir = DI_NODIR;
      
      // if the special is not a door that can be opened, return false
      //
      // killough 8/9/98: this is what caused monsters to get stuck in
      // doortracks, because it thought that the monster freed itself
      // by opening a door, even if it was moving towards the doortrack,
      // and not the door itself.
      //
      // killough 9/9/98: If a line blocking the monster is activated,
      // return true 90% of the time. If a line blocking the monster is
      // not activated, but some other line is, return false 90% of the
      // time. A bit of randomness is needed to ensure it's free from
      // lockups, but for most cases, it returns the correct result.
      //
      // Do NOT simply return false 1/4th of the time (causes monsters to
      // back out when they shouldn't, and creates secondary stickiness).

      for(good = false; clip.numspechit--; )
      {
         if(P_UseSpecialLine(actor, clip.spechit[clip.numspechit], 0))
            good |= (clip.spechit[clip.numspechit] == clip.blockline ? 1 : 2);
      }

      // haleyjd 01/09/07: do not leave numspechit == -1
      clip.numspechit = 0;

      // haleyjd 04/11/10: wider compatibility range
      if(!good || comp[comp_doorstuck]) // v1.9, or BOOM 2.01 compatibility
         return good;
      else if(demo_version == 202) // BOOM 2.02
         return (P_Random(pr_trywalk) & 3);
      else // MBF or higher
         return ((P_Random(pr_opendoor) >= 230) ^ (good & 1));

      /*
      return good && (demo_version < 203 || comp[comp_doorstuck] ||
                      (P_Random(pr_opendoor) >= 230) ^ (good & 1));
      */
   }
   else
   {
      actor->flags &= ~MF_INFLOAT;
   }

   // killough 11/98: fall more slowly, under gravity, if felldown==true
   // haleyjd: OVER_UNDER: not while in 3D clipping mode
   // VANILLA_HERETIC: inferior 3D clipping engine
   if(!P_Use3DClipping() || vanilla_heretic)
   {
      if(!(actor->flags & MF_FLOAT) && (!clip.felldown || demo_version < 203))
      {
         fixed_t oldz = actor->z;
         actor->z = actor->zref.floor;
         
         if(actor->z < oldz)
            E_HitFloor(actor);
      }
   }
   return true;
}

//
// P_SmartMove
//
// killough 9/12/98: Same as P_Move, except smarter
//
bool P_SmartMove(Mobj *actor)
{
   Mobj *target = actor->target;
   int on_lift, dropoff = 0, under_damage;

   // killough 9/12/98: Stay on a lift if target is on one
   on_lift = !comp[comp_staylift] && target && target->health > 0 &&
      target->subsector->sector->tag==actor->subsector->sector->tag &&
      P_IsOnLift(actor);

   under_damage = monster_avoid_hazards && P_IsUnderDamage(actor);
   
   // killough 10/98: allow dogs to drop off of taller ledges sometimes.
   // dropoff==1 means always allow it, dropoff==2 means only up to 128 high,
   // and only if the target is immediately on the other side of the line.

   // haleyjd: allow things of HelperType to jump down,
   // as well as any thing that has the MF2_JUMPDOWN flag
   // (includes DOGS)

   if((actor->flags2 & MF2_JUMPDOWN || (actor->type == HelperThing)) &&
      target && dog_jumping &&
      !((target->flags ^ actor->flags) & MF_FRIEND) &&
#ifdef R_LINKEDPORTALS
      P_AproxDistance(actor->x - getTargetX(actor),
                      actor->y - getTargetY(actor)) < FRACUNIT*144 &&
#else
      P_AproxDistance(actor->x - target->x,
                      actor->y - target->y) < FRACUNIT*144 &&
#endif      
      P_Random(pr_dropoff) < 235)
   {
      dropoff = 2;
   }

   if(!P_Move(actor, dropoff))
      return false;

   // killough 9/9/98: avoid crushing ceilings or other damaging areas
   if(
      (on_lift && P_Random(pr_stayonlift) < 230 &&      // Stay on lift
       !P_IsOnLift(actor))
      ||
      (monster_avoid_hazards && !under_damage &&  // Get away from damage
       (under_damage = P_IsUnderDamage(actor)) &&
       (under_damage < 0 || P_Random(pr_avoidcrush) < 200))
     )
   {
      actor->movedir = DI_NODIR;    // avoid the area (most of the time anyway)
   }
   
   return true;
}

//
// P_TryWalk
//
// Attempts to move actor on in its current (ob->moveangle) direction.
// If blocked by either a wall or an actor returns FALSE
// If move is either clear or blocked only by a door, returns TRUE and sets...
// If a door is in the way, an OpenDoor call is made to start it opening.
//
static bool P_TryWalk(Mobj *actor)
{
   if(!P_SmartMove(actor))
      return false;
   actor->movecount = P_Random(pr_trywalk)&15;
   return true;
}

//
// P_DoNewChaseDir
//
// killough 9/8/98:
//
// Most of P_NewChaseDir(), except for what
// determines the new direction to take
//
static void P_DoNewChaseDir(Mobj *actor, fixed_t deltax, fixed_t deltay)
{
   dirtype_t xdir, ydir, tdir;
   dirtype_t olddir = actor->movedir;
   dirtype_t turnaround = olddir;
   
   if(turnaround != DI_NODIR)         // find reverse direction
      turnaround ^= 4;

   xdir = 
      (deltax >  10*FRACUNIT ? DI_EAST :
       deltax < -10*FRACUNIT ? DI_WEST : DI_NODIR);
   
   ydir =
      (deltay < -10*FRACUNIT ? DI_SOUTH :
       deltay >  10*FRACUNIT ? DI_NORTH : DI_NODIR);

   // try direct route
   if(xdir != DI_NODIR && ydir != DI_NODIR && turnaround != 
      (actor->movedir = deltay < 0 ? deltax > 0 ? DI_SOUTHEAST : DI_SOUTHWEST :
       deltax > 0 ? DI_NORTHEAST : DI_NORTHWEST) && P_TryWalk(actor))
   {
      return;
   }

   // try other directions
   if(P_Random(pr_newchase) > 200 || D_abs(deltay)>abs(deltax))
   {
      tdir = xdir;
      xdir = ydir;
      ydir = tdir;
   }

   if((xdir == turnaround ? xdir = DI_NODIR : xdir) != DI_NODIR &&
      (actor->movedir = xdir, P_TryWalk(actor)))
      return;         // either moved forward or attacked

   if((ydir == turnaround ? ydir = DI_NODIR : ydir) != DI_NODIR &&
      (actor->movedir = ydir, P_TryWalk(actor)))
      return;

   // there is no direct path to the player, so pick another direction.
   if(olddir != DI_NODIR && (actor->movedir = olddir, P_TryWalk(actor)))
      return;

   // randomly determine direction of search
   if(P_Random(pr_newchasedir) & 1)
   {
      for(tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir++)
      {
         if(tdir != turnaround &&
            (actor->movedir = tdir, P_TryWalk(actor)))
            return;
      }
   }
   else
   {
      for (tdir = DI_SOUTHEAST; tdir != DI_EAST-1; tdir--)
      {
         if(tdir != turnaround &&
            (actor->movedir = tdir, P_TryWalk(actor)))
            return;
      }
   }
  
   if((actor->movedir = turnaround) != DI_NODIR && !P_TryWalk(actor))
      actor->movedir = DI_NODIR;
}

//
// killough 11/98:
//
// Monsters try to move away from tall dropoffs.
//
// In Doom, they were never allowed to hang over dropoffs,
// and would remain stuck if involuntarily forced over one.
// This logic, combined with p_map.c (P_TryMove), allows
// monsters to free themselves without making them tend to
// hang over dropoffs.

static fixed_t dropoff_deltax, dropoff_deltay, floorz;

static bool PIT_AvoidDropoff(line_t *line, polyobj_t *po, void *context)
{
   if(line->backsector                          && // Ignore one-sided linedefs
      clip.bbox[BOXRIGHT]  > line->bbox[BOXLEFT]   &&
      clip.bbox[BOXLEFT]   < line->bbox[BOXRIGHT]  &&
      clip.bbox[BOXTOP]    > line->bbox[BOXBOTTOM] && // Linedef must be contacted
      clip.bbox[BOXBOTTOM] < line->bbox[BOXTOP]    &&
      P_BoxOnLineSide(clip.bbox, line) == -1)
   {
      fixed_t front = line->frontsector->srf.floor.height;
      fixed_t back  = line->backsector->srf.floor.height;
      angle_t angle;

      // The monster must contact one of the two floors,
      // and the other must be a tall dropoff (more than 24).

      if(back == floorz && front < floorz - STEPSIZE)
      {
         // front side dropoff
         angle = P_PointToAngle(0,0,line->dx,line->dy);
      }
      else
      {
         // back side dropoff
         if(front == floorz && back < floorz - STEPSIZE)
            angle = P_PointToAngle(line->dx,line->dy,0,0);
         else
            return true;
      }

      // Move away from dropoff at a standard speed.
      // Multiple contacted linedefs are cumulative (e.g. hanging over corner)
      dropoff_deltax -= finesine[angle >> ANGLETOFINESHIFT]*32;
      dropoff_deltay += finecosine[angle >> ANGLETOFINESHIFT]*32;
   }

   return true;
}

//
// P_AvoidDropoff
//
// Driver for above
//
static fixed_t P_AvoidDropoff(Mobj *actor)
{
   int yh=((clip.bbox[BOXTOP]   = actor->y+actor->radius)-bmaporgy)>>MAPBLOCKSHIFT;
   int yl=((clip.bbox[BOXBOTTOM]= actor->y-actor->radius)-bmaporgy)>>MAPBLOCKSHIFT;
   int xh=((clip.bbox[BOXRIGHT] = actor->x+actor->radius)-bmaporgx)>>MAPBLOCKSHIFT;
   int xl=((clip.bbox[BOXLEFT]  = actor->x-actor->radius)-bmaporgx)>>MAPBLOCKSHIFT;
   int bx, by;

   floorz = actor->z;            // remember floor height

   dropoff_deltax = dropoff_deltay = 0;

   // check lines

   validcount++;
   for(bx = xl; bx <= xh; ++bx)
   {
      // all contacted lines
      for(by = yl; by <= yh; ++by)
         P_BlockLinesIterator(bx, by, PIT_AvoidDropoff);
   }
   
   // Non-zero if movement prescribed
   return dropoff_deltax | dropoff_deltay;
}

//
// P_NewChaseDir
//
// killough 9/8/98: Split into two functions
//
void P_NewChaseDir(Mobj *actor)
{
   Mobj *target = actor->target;
#ifdef R_LINKEDPORTALS
   fixed_t deltax = getTargetX(actor) - actor->x;
   fixed_t deltay = getTargetY(actor) - actor->y;
#else
   fixed_t deltax = target->x - actor->x;
   fixed_t deltay = target->y - actor->y;
#endif

   // killough 8/8/98: sometimes move away from target, keeping distance
   //
   // 1) Stay a certain distance away from a friend, to avoid being in their way
   // 2) Take advantage over an enemy without missiles, by keeping distance

   actor->strafecount = 0;

   if(demo_version >= 203)
   {
      if(actor->zref.floor - actor->zref.dropoff > STEPSIZE &&
         actor->z <= actor->zref.floor &&
         !(actor->flags & (MF_DROPOFF|MF_FLOAT)) &&
         (!P_Use3DClipping() || 
          !(actor->intflags & MIF_ONMOBJ)) && // haleyjd: OVER_UNDER
         !comp[comp_dropoff] && P_AvoidDropoff(actor)) // Move away from dropoff
      {
         P_DoNewChaseDir(actor, dropoff_deltax, dropoff_deltay);
         
         // If moving away from dropoff, set movecount to 1 so that 
         // small steps are taken to get monster away from dropoff.
         
         actor->movecount = 1;
         return;
      }
      else
      {
         fixed_t dist = P_AproxDistance(deltax, deltay);
         
         // Move away from friends when too close, except
         // in certain situations (e.g. a crowded lift)
         
         if(actor->flags & target->flags & MF_FRIEND &&
            distfriend << FRACBITS > dist && 
            !P_IsOnLift(target) && !P_IsUnderDamage(actor))
         {
            deltax = -deltax;
            deltay = -deltay;
         }
         else
         {
            if(target->health > 0 &&
               (actor->flags ^ target->flags) & MF_FRIEND)
            {   
               // Live enemy target
               if(monster_backing &&
                  actor->info->missilestate != NullStateNum && 
                  !(actor->flags2 & MF2_NOSTRAFE) &&
                  ((target->info->missilestate == NullStateNum && dist < MELEERANGE*2) ||
                   (target->player && dist < MELEERANGE*3 &&
                    target->player->readyweapon->flags & WPF_FLEEMELEE)))
               {
                  // Back away from melee attacker
                  actor->strafecount = P_Random(pr_enemystrafe) & 15;
                  deltax = -deltax;
                  deltay = -deltay;
               }
            }
         }
      }
   }

   P_DoNewChaseDir(actor, deltax, deltay);

   // If strafing, set movecount to strafecount so that old Doom
   // logic still works the same, except in the strafing part
   
   if(actor->strafecount)
      actor->movecount = actor->strafecount;
}

//
// P_IsVisible
//
// killough 9/9/98: whether a target is visible to a monster
// ioanch 20151229: made portal aware
//
static bool P_IsVisible(Mobj *actor, Mobj *mo, int allaround)
{
   if(mo->flags4 & MF4_TOTALINVISIBLE)
      return 0;  // haleyjd: total invisibility!

   // ioanch
   fixed_t mox = getThingX(actor, mo), moy = getThingY(actor, mo);

   // haleyjd 11/14/02: Heretic ghost effects
   if(mo->flags3 & MF3_GHOST)
   {
      if(P_AproxDistance(mox - actor->x, moy - actor->y) > 2*MELEERANGE 
         && P_AproxDistance(mo->momx, mo->momy) < 5*FRACUNIT)
      {
         // when distant and moving slow, target is considered
         // to be "sneaking"
         return false;
      }
      if(P_Random(pr_ghostsneak) < 225)
         return false;
   }

   if(!allaround)
   {
      angle_t an = P_PointToAngle(actor->x, actor->y, 
                                   mox, moy) - actor->angle;
      if(an > ANG90 && an < ANG270 &&
         P_AproxDistance(mox-actor->x, moy-actor->y) > MELEERANGE)
         return false;
   }

   return P_CheckSight(actor, mo);
}

int p_lastenemyroar;

//
// P_LastEnemyRoar
//
// haleyjd 01/20/11: BOOM's change to AI made in order to keep monsters from
// falling asleep had the unfortunate side effect of eliminating the wake-up
// sound they would make when killing their current target and returning to
// fighting the player or another monster. This is an optional routine that
// re-enables this behavior.
//
static void P_LastEnemyRoar(Mobj *actor)
{
   if(demo_version >= 340 && p_lastenemyroar)
      P_MakeSeeSound(actor, pr_misc); // don't affect the play sim.
}

static int current_allaround;

//
// PIT_FindTarget
//
// killough 9/5/98
//
// Finds monster targets for other monsters
//
static bool PIT_FindTarget(Mobj *mo, void *context)
{
   Mobj *actor = current_actor;

   if(!((mo->flags ^ actor->flags) & MF_FRIEND && // Invalid target
      mo->health > 0 &&
      (mo->flags & MF_COUNTKILL || mo->flags3 & MF3_KILLABLE)))
      return true;

   // If the monster is already engaged in a one-on-one attack
   // with a healthy friend, don't attack around 60% the time
   const Mobj *targ = mo->target;
   if(targ && targ->target == mo &&
      P_Random(pr_skiptarget) > 100 &&
      (targ->flags ^ mo->flags) & MF_FRIEND &&
       targ->health*2 >= targ->getModifiedSpawnHealth())
   {
      return true;
   }

   if(!P_IsVisible(actor, mo, current_allaround))
      return true;

   P_SetTarget<Mobj>(&actor->lastenemy, actor->target); // Remember previous target
   P_SetTarget<Mobj>(&actor->target, mo);               // Found target

   // Move the selected monster to the end of its associated
   // list, so that it gets searched last next time.
   Thinker *cap = &thinkerclasscap[mo->flags & MF_FRIEND ? th_friends : th_enemies];
   (mo->cprev->cnext = mo->cnext)->cprev = mo->cprev;
   (mo->cprev = cap->cprev)->cnext = mo;
   (mo->cnext = cap)->cprev = mo;
   
   return false;
}

//
// P_HereticMadMelee
//
// haleyjd 07/30/04: This function is used to make monsters start
// battling like mad when the player dies in Heretic. Who knows why
// Raven added that "feature," but it's fun ^_^
//
static bool P_HereticMadMelee(Mobj *actor)
{
   Mobj *mo;
   Thinker *th;

   // only monsters within sight of the player will go crazy
   if(!P_CheckSight(players[0].mo, actor))
      return false;

   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      if(!(mo = thinker_cast<Mobj *>(th)))
         continue;

      // Must be:
      // * killable
      // * not self (will fight same type, however)
      // * alive
      if(!(mo->flags & MF_COUNTKILL || mo->flags3 & MF3_KILLABLE) || 
         mo == actor || 
         mo->health <= 0)
         continue;

      // skip some at random
      if(P_Random(pr_madmelee) < 16)
         continue;

      // must be within sight
      if(!P_CheckSight(actor, mo))
         continue;

      // got one
      P_SetTarget<Mobj>(&actor->target, mo);
      return true;
   }

   return false;
}

//
// P_LookForPlayers
//
// If allaround is false, only look 180 degrees in front.
// Returns true if a player is targeted.
//
bool P_LookForPlayers(Mobj *actor, int allaround)
{
   player_t *player;
   int stop, stopc, c;
   
   if(actor->flags & MF_FRIEND)
   {  // killough 9/9/98: friendly monsters go about players differently
      int anyone;

#if 0
      if(!allaround) // If you want friendly monsters not to awaken unprovoked
         return false;
#endif

      // Go back to a player, no matter whether it's visible or not
      for(anyone = 0; anyone <= 1; ++anyone)
      {
         for(c = 0; c < MAXPLAYERS; ++c)
         {
            if(playeringame[c] 
               && players[c].playerstate == PST_LIVE 
               && (anyone || P_IsVisible(actor, players[c].mo, allaround)))
            {
               P_SetTarget<Mobj>(&actor->target, players[c].mo);

               // killough 12/98:
               // get out of refiring loop, to avoid hitting player accidentally

               if(actor->info->missilestate != NullStateNum)
               {
                  P_SetMobjState(actor, actor->info->seestate);
                  actor->flags &= ~MF_JUSTHIT;
               }
               
               return true;
            }
         }
      }
      
      return false;
   }

   if(GameModeInfo->flags & GIF_HASMADMELEE && GameType == gt_single &&
      players[0].health <= 0)
   {
      // Heretic monsters go mad when player dies
      return P_HereticMadMelee(actor);
   }

   // Change mask of 3 to (MAXPLAYERS-1) -- killough 2/15/98:
   stop = (actor->lastlook - 1) & (MAXPLAYERS - 1);

   c = 0;

   stopc = demo_version < 203 && !demo_compatibility && monsters_remember ?
           MAXPLAYERS : 2;       // killough 9/9/98

   bool unseen[MAXPLAYERS] = {};

   for(;; actor->lastlook = (actor->lastlook + 1) & (MAXPLAYERS - 1))
   {
      if(!playeringame[actor->lastlook])
         continue;

      // killough 2/15/98, 9/9/98:
      if(c++ == stopc || actor->lastlook == stop)  // done looking
      {
         // e6y
         // Fixed Boom incompatibilities. The following code was missed.
         // There are no more desyncs on Donce's demos on horror.wad
         if(demo_version < 203 && !demo_compatibility && monsters_remember)
         {
            if(actor->lastenemy && actor->lastenemy->health > 0)
            {
               // haleyjd: This SHOULD use P_SetTarget for both of the below
               // assignments; however, doing so may adversely affect 202 demo
               // sync according to 4mer. I am taking the chance of allowing 
               // this to use direct assignment, as it appears the ref counts
               // would ideally remain balanced (lastenemy already has a ref
               // from actor, so it's only moving to target).
               actor->target = actor->lastenemy;
               actor->lastenemy = nullptr;
               return true;
            }
         }

         return false;
      }
      
      player = &players[actor->lastlook];
      
      if(player->health <= 0)
         continue;               // dead
      
      if(unseen[actor->lastlook] || !P_IsVisible(actor, player->mo, allaround))
      {
         unseen[actor->lastlook] = true;
         continue;
      }
      
      P_SetTarget<Mobj>(&actor->target, player->mo);

      // killough 9/9/98: give monsters a threshold towards getting players
      // (we don't want it to be too easy for a player with dogs :)
      if(demo_version >= 203 && !comp[comp_pursuit])
         actor->threshold = 60;
      
      return true;
   }
}

//
// P_LookForMonsters
// 
// Friendly monsters, by Lee Killough 7/18/98
//
// Friendly monsters go after other monsters first, but 
// also return to owner if they cannot find any targets.
// A marine's best friend :)  killough 7/18/98, 9/98
//
static bool P_LookForMonsters(Mobj *actor, int allaround)
{
   Thinker *cap, *th;
   
   if(demo_compatibility)
      return false;

   if(actor->lastenemy && actor->lastenemy->health > 0 && monsters_remember &&
      !(actor->lastenemy->flags & actor->flags & MF_FRIEND)) // not friends
   {
      if(actor->target != actor->lastenemy)
         P_LastEnemyRoar(actor);
      P_SetTarget<Mobj>(&actor->target, actor->lastenemy);
      P_SetTarget<Mobj>(&actor->lastenemy, nullptr);
      return true;
   }

   if(demo_version < 203)  // Old demos do not support monster-seeking bots
      return false;

   // Search the threaded list corresponding to this object's potential targets
   cap = &thinkerclasscap[actor->flags & MF_FRIEND ? th_enemies : th_friends];

   // Search for new enemy

   if(cap->cnext != cap)        // Empty list? bail out early
   {
      int x = (actor->x - bmaporgx)>>MAPBLOCKSHIFT;
      int y = (actor->y - bmaporgy)>>MAPBLOCKSHIFT;
      int d;

      current_actor = actor;
      current_allaround = allaround;
      
      // Search first in the immediate vicinity.
      
      if(!P_BlockThingsIterator(x, y, PIT_FindTarget))
         return true;

      for(d = 1; d < 5; ++d)
      {
         int i = 1 - d;
         do
         {
            if(!P_BlockThingsIterator(x+i, y-d, PIT_FindTarget) ||
               !P_BlockThingsIterator(x+i, y+d, PIT_FindTarget))
               return true;
         }
         while(++i < d);
         do
         {
            if(!P_BlockThingsIterator(x-d, y+i, PIT_FindTarget) ||
               !P_BlockThingsIterator(x+d, y+i, PIT_FindTarget))
               return true;
         }
         while(--i + d >= 0);
      }

      {   // Random number of monsters, to prevent patterns from forming
         int n = (P_Random(pr_friends) & 31) + 15;

         for(th = cap->cnext; th != cap; th = th->cnext)
         {
            if(--n < 0)
            { 
               // Only a subset of the monsters were searched. Move all of
               // the ones which were searched so far to the end of the list.
               
               (cap->cnext->cprev = cap->cprev)->cnext = cap->cnext;
               (cap->cprev = th->cprev)->cnext = cap;
               (th->cprev = cap)->cnext = th;
               break;
            }
            else if(th->isInstanceOf(RTTI(Mobj)))
            {
               if(!PIT_FindTarget(static_cast<Mobj *>(th), nullptr)) // If target sighted
                  return true;
            }
         }
      }
   }
   
   return false;  // No monster found
}

//
// P_LookForTargets
//
// killough 9/5/98: look for targets to go after, depending on kind of monster
//
bool P_LookForTargets(Mobj *actor, int allaround)
{
   return actor->flags & MF_FRIEND ?
      P_LookForMonsters(actor, allaround) || P_LookForPlayers (actor, allaround):
      P_LookForPlayers (actor, allaround) || P_LookForMonsters(actor, allaround);
}

//
// P_HelpFriend
//
// killough 9/8/98: Help friends in danger of dying
//
bool P_HelpFriend(Mobj *actor)
{
   Thinker *cap, *th;

   // If less than 33% health, self-preservation rules
   if(actor->health*3 < actor->getModifiedSpawnHealth())
      return false;

   current_actor = actor;
   current_allaround = true;

   // Possibly help a friend under 50% health
   cap = &thinkerclasscap[actor->flags & MF_FRIEND ? th_friends : th_enemies];

   for (th = cap->cnext; th != cap; th = th->cnext)
   {
      Mobj *mo;
      
      if(!th->isInstanceOf(RTTI(Mobj)))
         continue;
         
      mo = static_cast<Mobj *>(th);

      if(mo->health*2 >= mo->getModifiedSpawnHealth())
      {
         if(P_Random(pr_helpfriend) < 180)
            break;
      }
      else
         if(mo->flags & MF_JUSTHIT &&
            mo->target && 
            mo->target != actor->target &&
            !PIT_FindTarget(mo->target, nullptr))
         {
            // Ignore any attacking monsters, while searching for 
            // friend
            actor->threshold = BASETHRESHOLD;
            return true;
         }
   }
   
   return false;
}

//
// P_SkullFly
//
// haleyjd 08/07/04: generalized code to make a thing skullfly.
// actor->target must be valid.
//
void P_SkullFly(Mobj *actor, fixed_t speed, bool useSeeState)
{
   Mobj *dest;
   angle_t an;
   int     dist;

   dest = actor->target;
   actor->flags |= MF_SKULLFLY;
   if(useSeeState)
      actor->intflags |= MIF_SKULLFLYSEE;
   else
      actor->intflags &= ~MIF_SKULLFLYSEE;

   actionargs_t actionargs;

   actionargs.actiontype = actionargs_t::MOBJFRAME;
   actionargs.actor      = actor;
   actionargs.args       = ESAFEARGS(actor);
   actionargs.pspr       = nullptr;

   A_FaceTarget(&actionargs);
   an = actor->angle >> ANGLETOFINESHIFT;
   actor->momx = FixedMul(speed, finecosine[an]);
   actor->momy = FixedMul(speed, finesine[an]);
   
   dist = P_AproxDistance(getTargetX(actor) - actor->x, 
                          getTargetY(actor) - actor->y);
   dist = dist / speed;
   if(dist < 1)
      dist = 1;

   actor->momz = (getTargetZ(actor)+ (dest->height >> 1) - actor->z) / dist;
}

//
// P_BossTeleport
//
// haleyjd 11/19/02
//
// Generalized function for teleporting a boss (or any other thing) around at 
// random between a provided set of map spots, along with special effects on 
// demand.
// Currently used by D'Sparil.
//
void P_BossTeleport(bossteleport_t *bt)
{
   Mobj *boss, *targ;
   fixed_t prevx, prevy, prevz;

   if(bt->mc->isEmpty())
      return;

   boss = bt->boss;

   // if minimum distance is specified, use a different point selection method
   if(bt->minDistance)
   {
      int i = P_Random(bt->rngNum) % bt->mc->getLength();
      int starti = i;
      fixed_t x, y;
      bool foundSpot = true;

      while(1)
      {
         targ = (*bt->mc)[(unsigned int)i];
         // ioanch 20151230: portal aware
         x = getThingX(boss, targ);
         y = getThingY(boss, targ);
         if(P_AproxDistance(boss->x - x, boss->y - y) >= bt->minDistance)
         {
            foundSpot = true;
            break;
         }

         // wrapped around? abort loop
         if((i = (i + 1) % bt->mc->getLength()) == starti)
            break;
      }

      // no spot? no teleport
      if(!foundSpot)
         return;
   }
   else
      targ = bt->mc->getRandom(bt->rngNum);

   prevx = boss->x;
   prevy = boss->y;
   prevz = boss->z;

   if(P_TeleportMove(boss, targ->x, targ->y, false))
   {
      if(bt->hereThere <= BOSSTELE_BOTH && bt->hereThere != BOSSTELE_NONE)
      {
         Mobj *mo = P_SpawnMobj(prevx, prevy, prevz + bt->zpamt, bt->fxtype);
         S_StartSound(mo, bt->soundNum);
      }

      if(bt->state >= 0)
         P_SetMobjState(boss, bt->state);
      S_StartSound(boss, bt->soundNum);

      if(bt->hereThere >= BOSSTELE_BOTH && bt->hereThere != BOSSTELE_NONE)
         P_SpawnMobj(boss->x, boss->y, boss->z + bt->zpamt, bt->fxtype);

      boss->z = boss->zref.floor;
      boss->angle = targ->angle;
      boss->momx = boss->momy = boss->momz = 0;
      boss->backupPosition();
   }
}

//
// P_GetAimShift
//
// Get the aiming inaccuracy shift for an enemy. Returns -1 if there is none.
//
int P_GetAimShift(Mobj *target, bool missile)
{
   static MetaKeyIndex aimShiftKey("aimshift");
   int shiftamount = -1;

   // some thing flags set a shift amount
   if(target->flags & MF_SHADOW || target->flags4 & MF4_TOTALINVISIBLE)
      shiftamount = missile ? 20 : 21;
   if(target->flags3 & MF3_GHOST)
      shiftamount = 21;

   // check out the metatable aimshift amount; if it is within range, it
   // will override the shift amount set by any flags. If it is out of 
   // range, it will remove any aim shifting.
   int metashift = target->info->meta->getInt(aimShiftKey, -1);
   if(metashift >= 0)
      shiftamount = (metashift <= 24 ? metashift : -1);

   return shiftamount;
}

//=============================================================================
//
// Most action functions have been moved to specialized modules. Those that
// remain here will also eventually find new, more appropriate homes, or will
// be eliminated entirely in a couple of cases.
//

//
// A_PlayerStartScript
//
// Parameterized player gun frame codepointer for starting
// Small scripts
//
// args[0] - script number to start
// args[1] - select vm (0 == gamescript, 1 == levelscript)
// args[2-4] - parameters to script (must accept 3 params)
//
void A_PlayerStartScript(actionargs_t *actionargs)
{
   // FIXME: support ACS, Aeon here
}

//
// haleyjd: Start Eternity TC Action Functions
// TODO: possibly eliminate some of these
//

//
// A_FogSpawn
//
// The slightly-more-complicated-than-you-thought Hexen fog cloud generator
// Modified to use random initializer values as opposed to actor->args[]
// set in mapthing data fields
//
#define FOGSPEED 2
#define FOGFREQUENCY 8

void A_FogSpawn(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   Mobj *mo = nullptr;

   if(actor->counters[0]-- > 0)
     return;

   actor->counters[0] = FOGFREQUENCY; // reset frequency

   switch(P_Random(pr_cloudpick)%3)
   {
   case 0:
      mo = P_SpawnMobj(actor->x, actor->y, actor->z, 
         E_SafeThingType(MT_FOGPATCHS));
      break;
   case 1:
      mo = P_SpawnMobj(actor->x, actor->y, actor->z, 
         E_SafeThingType(MT_FOGPATCHM));
      break;
   case 2:
      mo = P_SpawnMobj(actor->x, actor->y, actor->z, 
         E_SafeThingType(MT_FOGPATCHL));
      break;
   }
   if(mo)
   {
      angle_t delta = (P_Random(pr_fogangle)&0x7f)+1;
      mo->angle = actor->angle + (((P_Random(pr_fogangle)%delta)-(delta>>1))<<24);
      mo->target = actor;
      mo->args[0] = (P_Random(pr_fogangle)%FOGSPEED)+1; // haleyjd: narrowed range
      mo->args[3] = (P_Random(pr_fogcount)&0x7f)+1;
      mo->args[4] = 1;
      mo->counters[1] = P_Random(pr_fogfloat)&63;
   }
}

//
// A_FogMove
//

void A_FogMove(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   int speed = actor->args[0]<<FRACBITS;
   angle_t angle;

   if(!(actor->args[4]))
     return;

   if(actor->args[3]-- <= 0)
   {
      P_SetMobjStateNF(actor, actor->info->deathstate);
      return;
   }

   if((actor->args[3]%4) == 0)
   {
      int weaveindex = actor->counters[1];
      actor->z += FloatBobOffsets[weaveindex]>>1;
      actor->counters[1] = (weaveindex+1)&63;
   }
   angle = actor->angle>>ANGLETOFINESHIFT;
   actor->momx = FixedMul(speed, finecosine[angle]);
   actor->momy = FixedMul(speed, finesine[angle]);
}

#if 0
void P_ClericTeleport(Mobj *actor)
{
   bossteleport_t bt;

   if(actor->flags2&MF2_INVULNERABLE)
     P_ClericSparkle(actor);

   bt.mc        = &braintargets;            // use braintargets
   bt.rngNum    = pr_clr2choose;            // use this rng
   bt.boss      = actor;                    // teleport leader cleric
   bt.state     = E_SafeState(S_LCLER_TELE1); // put cleric in this state
   bt.fxtype    = E_SafeThingType(MT_IFOG); // spawn item fog for fx
   bt.zpamt     = 24*FRACUNIT;              // add 24 units to fog z 
   bt.hereThere = BOSSTELE_BOTH;            // spawn fx @ origin & dest.
   bt.soundNum  = sfx_itmbk;                // use item respawn sound

   P_BossTeleport(&bt);
}
#endif

//=============================================================================
//
// Console Commands for Things
//

// haleyjd 07/05/03: new console commands that can use
// EDF thing type names instead of internal type numbers

extern unsigned int *deh_ParseFlagsCombined(const char *strval);

static void P_ConsoleSummon(int type, angle_t an, int flagsmode, const char *flags)
{
   int fountainType  = E_ThingNumForName("EEParticleFountain");
   int dripType      = E_ThingNumForName("EEParticleDrip");
   int ambienceType  = E_ThingNumForName("EEAmbience");
   int enviroType    = E_ThingNumForName("EEEnviroSequence");
   int vileFireType  = E_ThingNumForName("VileFire");
   int spawnSpotType = E_ThingNumForName("BossSpawnSpot");

   Mobj   *newmobj;
   player_t *plyr = &players[consoleplayer];

   // if it's a missile, shoot it
   if(mobjinfo[type]->flags & MF_MISSILE)
   {
      newmobj = P_SpawnPlayerMissile(plyr->mo, type);

      // set the tracer target in case it is a homing missile
      P_BulletSlope(plyr->mo);
      if(clip.linetarget)
         P_SetTarget<Mobj>(&newmobj->tracer, clip.linetarget);
   }
   else
   {
      // do a good old Pain-Elemental style summoning
      an = (plyr->mo->angle + an) >> ANGLETOFINESHIFT;
      int prestep = 4*FRACUNIT + 3*(plyr->mo->info->radius + mobjinfo[type]->radius)/2;
      
      fixed_t x = plyr->mo->x + FixedMul(prestep, finecosine[an]);
      fixed_t y = plyr->mo->y + FixedMul(prestep, finesine[an]);
      
      fixed_t z = (mobjinfo[type]->flags & MF_SPAWNCEILING) ? ONCEILINGZ : ONFLOORZ;
      
      if(Check_Sides(plyr->mo, x, y, type))
         return;
      
      newmobj = P_SpawnMobj(x, y, z, type);
      
      newmobj->angle = plyr->mo->angle;
   }

   // tweak the object's flags
   if(flagsmode != -1)
   {
      unsigned int *res = deh_ParseFlagsCombined(flags);

      switch(flagsmode)
      {
      case 0: // set flags
         newmobj->flags  = res[0];
         newmobj->flags2 = res[1];
         newmobj->flags3 = res[2];
         newmobj->flags4 = res[3];
         break;
      case 1: // add flags
         newmobj->flags  |= res[0];
         newmobj->flags2 |= res[1];
         newmobj->flags3 |= res[2];
         newmobj->flags4 |= res[3];
         break;
      case 2: // rem flags
         newmobj->flags  &= ~res[0];
         newmobj->flags2 &= ~res[1];
         newmobj->flags3 &= ~res[2];
         newmobj->flags4 &= ~res[3];
         break;
      default:
         break;
      }
   }

   // code to tweak interesting objects
   if(type == vileFireType)
   {
      P_BulletSlope(plyr->mo);
      if(clip.linetarget)
      {
         P_SetTarget<Mobj>(&newmobj->target, plyr->mo);
         P_SetTarget<Mobj>(&newmobj->tracer, clip.linetarget);

         actionargs_t fireargs;
         fireargs.actiontype = actionargs_t::MOBJFRAME;
         fireargs.actor      = newmobj;
         fireargs.args       = ESAFEARGS(newmobj);
         fireargs.pspr       = nullptr;

         A_Fire(&fireargs);
      }
   }

   // code to make spawning parameterized objects more useful
      
   // fountain: random color
   if(type == fountainType)
   {
      unsigned int ft = 9027u + M_Random() % 7u;
      
      newmobj->effects |= (ft - 9026u) << FX_FOUNTAINSHIFT;
   }

   // drip: random parameters
   if(type == dripType)
   {
      newmobj->args[0] = M_Random();
      newmobj->args[1] = (M_Random() % 8) + 1;
      newmobj->args[2] = M_Random() + 35;
      newmobj->args[3] = 1;
   }

   // ambience: cycle through first 64 ambience #'s
   if(type == ambienceType)
   {
      static int ambnum;

      newmobj->args[0] = ambnum++;

      if(ambnum == 64)
         ambnum = 0;
   }

   if(type == enviroType)
   {
      static int envnum;

      newmobj->args[0] = envnum++;

      if(envnum == 64)
         envnum = 0;
   }

   if(type == spawnSpotType)
      P_SpawnBrainTargets();

   // adjust count* flags to avoid messing up the map

   if(newmobj->flags & MF_COUNTKILL)
   {
     newmobj->flags &= ~MF_COUNTKILL;
     newmobj->flags3 |= MF3_KILLABLE;
   }
   
   if(newmobj->flags & MF_COUNTITEM)
      newmobj->flags &= ~MF_COUNTITEM;
   
   // 06/09/02: set player field for voodoo dolls
   if(E_IsPlayerClassThingType(newmobj->type))
      newmobj->player = plyr;
   
   // killough 8/29/98: add to appropriate thread
   newmobj->updateThinker();
}

CONSOLE_COMMAND(summon, cf_notnet|cf_level|cf_hidden)
{
   int type;
   int flagsmode = -1;
   const char *flags = nullptr;

   if(!Console.argc)
   {
      C_Printf("usage: summon thingtype flags mode\n");
      return;
   }

   if(Console.argc >= 2)
   {
      flagsmode = 1;
      flags = Console.argv[1]->constPtr();
   }

   if(Console.argc >= 3)
   {
      if(!Console.argv[2]->strCaseCmp("set"))
         flagsmode = 0; // set
      else if(!Console.argv[2]->strCaseCmp("remove"))
         flagsmode = 2; // remove
   }

   if((type = E_ThingNumForName(Console.argv[0]->constPtr())) == -1 &&
      (type = E_ThingNumForCompatName(Console.argv[0]->constPtr())) == -1)
   {
      C_Printf("unknown thing type\n");
      return;
   }

   P_ConsoleSummon(type, 0, flagsmode, flags);
}

CONSOLE_COMMAND(viles, cf_notnet|cf_level|cf_hidden)
{
   // only in DOOM II ;)
   if(GameModeInfo->id == commercial)
   {
      int vileType = E_ThingNumForName("Archvile");
      
      S_ChangeMusicNum(mus_stalks, true);
      
      P_ConsoleSummon(vileType, 0,            1, "FRIEND");
      P_ConsoleSummon(vileType, ANG45,        1, "FRIEND");
      P_ConsoleSummon(vileType, ANG270+ANG45, 1, "FRIEND");
   }
}

CONSOLE_COMMAND(give, cf_notnet|cf_level)
{
   int i;
   int itemnum;
   int thingnum;
   player_t *plyr = &players[consoleplayer];

   if(!Console.argc)
      return;
   
   thingnum = E_ThingNumForName(Console.argv[0]->constPtr());
   if(thingnum == -1)
   {
      C_Printf("unknown thing type\n");
      return;
   }
   if(!(mobjinfo[thingnum]->flags & MF_SPECIAL))
   {
      C_Printf("thing type is not a special\n");
      return;
   }
   itemnum = (Console.argc >= 2) ? Console.argv[1]->toInt() : 1;

   for(i = 0; i < itemnum; i++)
   {
      Mobj *mo = P_SpawnMobj(plyr->mo->x, plyr->mo->y, plyr->mo->z,
                               thingnum);
      mo->flags &= ~(MF_SPECIAL|MF_COUNTITEM);

      P_TouchSpecialThing(mo, plyr->mo);

      // if it wasn't picked up, remove it
      if(!mo->isRemoved())
         mo->remove();
   }
}

//
// whistle
//
// This command lets the player "whistle" to teleport a friend
// of the given type to his location.
//
CONSOLE_COMMAND(whistle, cf_notnet|cf_level)
{
   int thingnum;
   player_t *plyr = &players[consoleplayer];

   if(!Console.argc)
      return;
   
   thingnum = E_ThingNumForName(Console.argv[0]->constPtr());
   if(thingnum == -1)
   {
      C_Printf("unknown thing type\n");
      return;
   }

   P_Whistle(plyr->mo, thingnum);
}

CONSOLE_COMMAND(mdk, cf_notnet|cf_level)
{
   player_t *plyr = &players[consoleplayer];
   fixed_t slope;
   int damage = GOD_BREACH_DAMAGE;

   slope = P_AimLineAttack(plyr->mo, plyr->mo->angle, MISSILERANGE, false);

   if(clip.linetarget
      && !(clip.linetarget->flags2 & MF2_INVULNERABLE) // use 10k damage to
      && !(clip.linetarget->flags4 & MF4_NODAMAGE))    // defeat special flags
      damage = clip.linetarget->health;

   P_LineAttack(plyr->mo, plyr->mo->angle, MISSILERANGE, slope, damage);
}

CONSOLE_COMMAND(mdkbomb, cf_notnet|cf_level)
{
   player_t *plyr = &players[consoleplayer];
   fixed_t slope;
   int damage = GOD_BREACH_DAMAGE;

   for(int i = 0; i < 60; i++)  // offset angles from its attack angle
   {
      angle_t an = (ANG360/60)*i;
      
      slope = P_AimLineAttack(plyr->mo, an, MISSILERANGE,false);

      if(clip.linetarget)
         damage = clip.linetarget->health;

      P_LineAttack(plyr->mo, an, MISSILERANGE, slope, damage);
   }
}

CONSOLE_COMMAND(banish, cf_notnet|cf_level)
{
   player_t *plyr = &players[consoleplayer];

   P_AimLineAttack(plyr->mo, plyr->mo->angle, MISSILERANGE, false);

   if(clip.linetarget)
      clip.linetarget->remove();
}

CONSOLE_COMMAND(vilehit, cf_notnet|cf_level)
{
   player_t *plyr = &players[consoleplayer];

   P_BulletSlope(plyr->mo);
   if(!clip.linetarget)
      return;
   
   S_StartSound(plyr->mo, sfx_barexp);
   P_DamageMobj(clip.linetarget, plyr->mo, plyr->mo, 20, MOD_UNKNOWN);
   clip.linetarget->momz = 1000*FRACUNIT/clip.linetarget->info->mass;

   P_RadiusAttack(clip.linetarget, plyr->mo, 70, 70, MOD_UNKNOWN, 0);
}

void P_SpawnPlayer(mapthing_t* mthing);

static void P_ResurrectPlayer()
{
   player_t *p = &players[consoleplayer];

   if(p->health <= 0 || p->mo->health <= 0 || p->playerstate == PST_DEAD)
   {
      edefstructvar(mapthing_t, mthing);
      Mobj *oldmo = p->mo;

      // IOANCH 20151218: 32-bit mapthing_t coordinates
      // To avoid any change, keep truncating the subunit values
      // This is to preserve the old behaviour, when mapthing_t had short-int
      // coordinates.
      mthing.x     = p->mo->x & ~(FRACUNIT - 1);
      mthing.y     = p->mo->y & ~(FRACUNIT - 1);
      mthing.angle = (int16_t)(p->mo->angle / ANGLE_1);
      mthing.type  = static_cast<int16_t>(p - players + 1);

      p->health = p->pclass->initialhealth;
      P_SpawnPlayer(&mthing);
      oldmo->player = nullptr;
      P_TeleportMove(p->mo, p->mo->x, p->mo->y, true);
   }
}

CONSOLE_COMMAND(resurrect, cf_notnet|cf_level)
{
   P_ResurrectPlayer();
}

VARIABLE_BOOLEAN(p_lastenemyroar, nullptr, onoff);
CONSOLE_VARIABLE(p_lastenemyroar, p_lastenemyroar, 0) {}

//----------------------------------------------------------------------------
//
// $Log: p_enemy.c,v $
// Revision 1.22  1998/05/12  12:47:10  phares
// Removed OVER UNDER code
//
// Revision 1.21  1998/05/07  00:50:55  killough
// beautification, remove dependence on evaluation order
//
// Revision 1.20  1998/05/03  22:28:02  killough
// beautification, move declarations and includes around
//
// Revision 1.19  1998/04/01  12:58:44  killough
// Disable boss brain if no targets
//
// Revision 1.18  1998/03/28  17:57:05  killough
// Fix boss spawn savegame bug
//
// Revision 1.17  1998/03/23  15:18:03  phares
// Repaired AV ghosts stuck together bug
//
// Revision 1.16  1998/03/16  12:33:12  killough
// Use new P_TryMove()
//
// Revision 1.15  1998/03/09  07:17:58  killough
// Fix revenant tracer bug
//
// Revision 1.14  1998/03/02  11:40:52  killough
// Use separate monsters_remember flag instead of bitmask
//
// Revision 1.13  1998/02/24  08:46:12  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.12  1998/02/23  04:43:44  killough
// Add revenant p_atracer, optioned monster ai_vengence
//
// Revision 1.11  1998/02/17  06:04:55  killough
// Change RNG calling sequences
// Fix minor icon landing bug
// Use lastenemy to make monsters remember former targets, and fix player look
//
// Revision 1.10  1998/02/09  03:05:22  killough
// Remove icon landing limit
//
// Revision 1.9  1998/02/05  12:15:39  phares
// tighten lost soul wall fix to compatibility
//
// Revision 1.8  1998/02/02  13:42:54  killough
// Relax lost soul wall fix to demo_compatibility
//
// Revision 1.7  1998/01/28  13:21:01  phares
// corrected Option3 in AV bug
//
// Revision 1.6  1998/01/28  12:22:17  phares
// AV bug fix and Lost Soul trajectory bug fix
//
// Revision 1.5  1998/01/26  19:24:00  phares
// First rev with no ^Ms
//
// Revision 1.4  1998/01/23  14:51:51  phares
// No content change. Put ^Ms back.
//
// Revision 1.3  1998/01/23  14:42:14  phares
// No content change. Removed ^Ms for experimental checkin.
//
// Revision 1.2  1998/01/19  14:45:01  rand
// Temporary line for checking checkins
//
// Revision 1.1.1.1  1998/01/19  14:02:59  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

