// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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
//  Movement, collision handling.
//  Shooting and aiming.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "doomstat.h"
#include "e_exdata.h"
#include "e_things.h"
#include "m_bbox.h"
#include "m_collection.h"
#include "p_inter.h"
#include "p_mobj.h"
#include "p_maputl.h"
#include "p_portalclip.h"
#include "p_portal.h"
#include "p_setup.h"
#include "p_slopes.h"
#include "p_spec.h"
#include "p_traceengine.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_pcheck.h"
#include "r_portal.h"
#include "r_state.h"



//
// tryMove
//
// Attempt to move to a new position,
// crossing special lines unless MF_TELEPORT is set.
//
// killough 3/15/98: allow dropoff as option
//
bool PortalClipEngine::tryMove(Mobj *thing, fixed_t x, fixed_t y, int dropoff, ClipContext *cc)
{
   auto oldz = thing->z;

   if(!checkPosition(thing, x, y, cc))
   {
      return false;
   }

   // the move is ok,
   // so unlink from the old position and link into the new position

   unsetThingPosition (thing);
   
   auto oldx = thing->x;
   auto oldy = thing->y;
   thing->floorz = cc->floorz;
   thing->ceilingz = cc->ceilingz;
   thing->dropoffz = cc->dropoffz;      // killough 11/98: keep track of dropoffs
   thing->passfloorz = cc->passfloorz;
   thing->passceilz = cc->passceilz;
   thing->secfloorz = cc->secfloorz;
   thing->secceilz = cc->secceilz;

   thing->x = x;
   thing->y = y;
   
   setThingPosition(thing, cc, false);

   // haleyjd 08/07/04: new footclip system
   P_AdjustFloorClip(thing);

   // if any special lines were hit, do the effect
   // killough 11/98: simplified
   if(!(thing->flags & (MF_TELEPORT | MF_NOCLIP)))
   {
      while(cc->spechit.getLength() > 0)
      {
         line_t *l = cc->spechit.pop();
         
         if(l->special)  // see if the line was crossed
         {
            int oldside;
            if((oldside = P_PointOnLineSide(oldx, oldy, l)) !=
               P_PointOnLineSide(thing->x, thing->y, l))
               P_CrossSpecialLine(l, oldside, thing);
         }
      }
   }
   
   return true;
}

bool PortalClipEngine::tryZMove(Mobj *thing, fixed_t z, ClipContext *cc)
{
   return false;
}

bool PortalClipEngine::makeZMove(Mobj *thing, fixed_t z, ClipContext *cc)
{
   return false;
}


bool PortalClipEngine::teleportMove(Mobj *thing, fixed_t x, fixed_t y, bool boss)
{
   return false;
}

bool PortalClipEngine::teleportMoveStrict(Mobj *thing, fixed_t x, fixed_t y, bool boss)
{
   return false;
}

bool PortalClipEngine::portalTeleportMove(Mobj *thing, fixed_t x, fixed_t y)
{
   return false;
}


void PortalClipEngine::slideMove(Mobj *mo)
{
}


void PortalClipEngine::lineOpening(line_t *linedef, Mobj *mo, open_t *opening, ClipContext *cc)
{
   fixed_t frontceilz, frontfloorz, backceilz, backfloorz;
   // SoM: used for 3dmidtex
   fixed_t frontcz, frontfz, backcz, backfz, otop, obot;

   opening->openflags = 0;

   if(linedef->sidenum[1] == -1)      // single sided line
   {
      opening->range = 0;
      return;
   }
   
   opening->frontsector = linedef->frontsector;
   opening->backsector  = linedef->backsector;

   {
      if(mo && 
         opening->frontsector->c_pflags & PS_PASSABLE &&
         opening->backsector->c_pflags & PS_PASSABLE && 
         opening->frontsector->c_portal == opening->backsector->c_portal)
      {
         frontceilz = backceilz = D_MAXINT;
      }
      else
      {
         opening->openflags |= OF_SETSCEILINGZ;
         frontceilz = opening->frontsector->ceilingheight;
         backceilz  = opening->backsector->ceilingheight;

         if(frontceilz < backceilz)
            opening->top = frontceilz;
         else
            opening->top = backceilz;

         opening->secceil  = opening->top;
      }
      
      frontcz = opening->frontsector->ceilingheight;
      backcz  = opening->backsector->ceilingheight;
   }


   {
      if(mo &&
         opening->frontsector->f_pflags & PS_PASSABLE &&
         opening->backsector->f_pflags & PS_PASSABLE && 
         opening->frontsector->f_portal == opening->backsector->f_portal)
      {
         frontfloorz = backfloorz = D_MININT;
      }
      else 
      {
         opening->openflags |= OF_SETSFLOORZ;
         frontfloorz = opening->frontsector->floorheight;
         backfloorz  = opening->backsector->floorheight;

         opening->secfloor = opening->bottom;

         if(frontfloorz > backfloorz)
         {
            opening->bottom = frontfloorz;
            opening->lowfloor = backfloorz;
            // haleyjd
            cc->floorpic = opening->frontsector->floorpic;
         }
         else
         {
            opening->bottom = backfloorz;
            opening->lowfloor = frontfloorz;
            // haleyjd
            cc->floorpic = opening->backsector->floorpic;
         }
      }

      frontfz = opening->frontsector->floorheight;
      backfz = opening->backsector->floorheight;
   }
   
   if(frontcz < backcz)
      otop = frontcz;
   else
      otop = backcz;

   if(frontfz > backfz)
      obot = frontfz;
   else
      obot = backfz;

   // SoM 9/02/02: Um... I know I told Quasar` I would do this after 
   // I got SDL_Mixer support and all, but I WANT THIS NOW hehe
   if(mo && linedef->flags & ML_3DMIDTEX && sides[linedef->sidenum[0]].midtexture)
   {
      fixed_t textop, texbot, texmid;
      side_t *side = &sides[linedef->sidenum[0]];
      
      if(linedef->flags & ML_DONTPEGBOTTOM)
      {
         texbot = side->rowoffset + obot;
         textop = texbot + textures[side->midtexture]->heightfrac;
      }
      else
      {
         textop = otop + side->rowoffset;
         texbot = textop - textures[side->midtexture]->heightfrac;
      }
      texmid = (textop + texbot)/2;

      // SoM 9/7/02: use monster blocking line to provide better
      // clipping
      if((linedef->flags & ML_BLOCKMONSTERS) && 
         !(mo->flags & (MF_FLOAT | MF_DROPOFF)) &&
         D_abs(mo->z - textop) <= 24*FRACUNIT)
      {
         opening->openflags |= OF_SETSCEILINGZ|OF_SETSFLOORZ;
         opening->top = opening->bottom;
         opening->range = 0;
         return;
      }
      
      if(mo->z + (P_ThingInfoHeight(mo->info) / 2) < texmid)
      {
         if(texbot < opening->top)
         {
            opening->top = texbot;
            opening->openflags |= OF_SETSCEILINGZ;
         }
      }
      else
      {
         if(textop > opening->bottom)
         {
            opening->bottom = textop;
            opening->openflags |= OF_SETSFLOORZ;
         }

         // The mobj is above the 3DMidTex, so check to see if it's ON the 3DMidTex
         // SoM 01/12/06: let monsters walk over dropoffs
         if(abs(mo->z - textop) <= 24*FRACUNIT)
            cc->touch3dside = 1;
      }
   }

   if(!(opening->openflags & (OF_SETSCEILINGZ|OF_SETSFLOORZ)) )
      opening->range = D_MAXINT;
   else
      opening->range = opening->top - opening->bottom;
}



static inline fixed_t GetCeilingHeight(sector_t *s, fixed_t x, fixed_t y)
{
   return s->c_slope ? P_GetZAt(s->c_slope, x, y) : s->ceilingheight;
}


static inline fixed_t GetFloorHeight(sector_t *s, fixed_t x, fixed_t y)
{
   return s->f_slope ? P_GetZAt(s->f_slope, x, y) : s->floorheight;
}


//
// CheckSubsectorPosition
//
// Adjusts the thing's floor/ceilingz based on the sector it occupies.
void CheckSubsectorPosition(ClipContext *cc)
{
   auto subsec = R_PointInSubsector(cc->x, cc->y);
   bool noclip = !!(cc->thing->flags & MF_NOCLIP);

   if(subsec->sector->c_pflags & PS_PASSABLE && !noclip)
   {
      if(subsec->sector->ceilingheight < cc->z + cc->height)
         HitPortalGroup(R_CPLink(subsec->sector)->toid, AG_CEILPORTAL, cc);
   }
   else 
   {
      fixed_t h = GetCeilingHeight(subsec->sector, cc->x, cc->y);

      if(h < cc->ceilingz)
         cc->ceilingz = h;
      if(h < cc->secceilz)
         cc->secceilz = h;
   }

   if(subsec->sector->f_pflags & PS_PASSABLE && !noclip)
   {
      if(subsec->sector->floorheight > cc->z)
         HitPortalGroup(R_FPLink(subsec->sector)->toid, AG_FLOORPORTAL, cc);
   }
   else
   {
      fixed_t h = GetFloorHeight(subsec->sector, cc->x, cc->y);

      if(h > cc->floorz)
         cc->floorz = h;
      if(h > cc->secfloorz)
      {
         cc->secfloorz = h;
         cc->floorpic = subsec->sector->floorpic;
      }
   }
}


extern bool P_Touched(Mobj *thing, ClipContext *cc);
extern bool P_CheckPickUp(Mobj *thing, ClipContext *cc);
extern bool P_SkullHit(Mobj *thing, ClipContext *cc);

bool CheckThing(Mobj *mo, ClipContext *cc)
{
   fixed_t blockdist = mo->radius + cc->thing->radius;
   int damage;

   if(!(mo->flags & (MF_SOLID|MF_SPECIAL|MF_SHOOTABLE|MF_TOUCHY)))
      return true;

   if(D_abs(mo->x - cc->x) >= blockdist || D_abs(mo->y - cc->y) >= blockdist || mo == cc->thing)
      return true;

   fixed_t topz = mo->z + mo->height;
   cc->BlockingMobj = mo;

   // Raise floorz/lower ceilingz and check the new opening height to see if the player can step-up and fit
   if(cc->thing->flags3 & MF3_PASSMOBJ)
   {
      // check if a mobj passed over/under another object
      
      // Some things prefer not to overlap each other, if possible
      if(cc->thing->flags3 & mo->flags3 & MF3_DONTOVERLAP)
         return false;

      // haleyjd: touchies need to explode if being exactly touched
      if(mo->flags & MF_TOUCHY && !(cc->thing->intflags & MIF_NOTOUCH))
      {
         if(cc->thing->z == topz || cc->thing->z + cc->thing->height == mo->z)
         {
            // SoM: TODO
            //P_Touched(thing, cc);
            // haleyjd: make the thing fly up a bit so it can run across
            cc->thing->momz += FRACUNIT; 
            return true;
         }
      }

      // Adjust the floorz/ceilingz
      if(cc->thing->z + 24 * FRACUNIT >= topz)
      {
         if(topz > cc->floorz)
            cc->floorz = topz;
      }
      else if(cc->ceilingz > mo->z)
         cc->ceilingz = mo->z;

      // Return true if cc->thing can still stepup and fit within the new floor/ceiling
      if(cc->ceilingz >= cc->thing->z + cc->thing->height && 
         cc->floorz <= cc->thing->z + 24 * FRACUNIT &&
         cc->ceilingz - cc->floorz >= cc->thing->height)
         return true;
   }

   // Collision

   // killough 11/98:
   //
   // TOUCHY flag, for mines or other objects which die on contact with solids.
   // If a solid object of a different type comes in contact with a touchy
   // thing, and the touchy thing is not the sole one moving relative to fixed
   // surroundings such as walls, then the touchy thing dies immediately.

   if(!(cc->thing->intflags & MIF_NOTOUCH)) // haleyjd: not when just testing
      if(P_Touched(mo, cc))
         return true;

   // check for skulls slamming into things
   
   if(P_SkullHit(mo, cc))
      return false;

   // missiles can hit other things
   // killough 8/10/98: bouncing non-solid things can hit other things too
   
   if(cc->thing->flags & MF_MISSILE || 
      (cc->thing->flags & MF_BOUNCES && !(cc->thing->flags & MF_SOLID)))
   {
      // haleyjd 07/06/05: some objects may use info->height instead
      // of their current height value in this situation, to avoid
      // altering the playability of maps when 3D object clipping
      // with corrected thing heights is enabled.
      int height = (!comp[comp_theights] && mo->flags3 & MF3_3DDECORATION) ? mo->info->height : mo->height;

      // haleyjd: some missiles can go through ghosts
      if(mo->flags3 & MF3_GHOST && cc->thing->flags3 & MF3_THRUGHOST)
         return true;

      // see if it went over / under
      
      if(cc->thing->z > mo->z + height) // haleyjd 07/06/05
         return true;    // overhead

      if(cc->thing->z + cc->thing->height < mo->z)
         return true;    // underneath

      // EDF FIXME: haleyjd 07/13/03: these may be temporary fixes
      int bruiserType = E_ThingNumForDEHNum(MT_BRUISER); 
      int knightType  = E_ThingNumForDEHNum(MT_KNIGHT);

      if(cc->thing->target &&
         (cc->thing->target->type == mo->type ||
          (cc->thing->target->type == knightType && mo->type == bruiserType)||
          (cc->thing->target->type == bruiserType && mo->type == knightType)))
      {
         if(mo == cc->thing->target)
            return true;                // Don't hit same species as originator.
         else if(!(mo->player))         // Explode, but do no damage.
            return false;               // Let players missile other players.
      }

      // haleyjd 10/15/08: rippers
      if(cc->thing->flags3 & MF3_RIP)
      {
         // TODO: P_RipperBlood
         /*
         if(!(thing->flags&MF_NOBLOOD))
         { 
            // Ok to spawn some blood
            P_RipperBlood(cc->thing);
         }
         */
         
         // TODO: ripper sound - gamemode dependent? thing dependent?
         //S_StartSound(cc->thing, sfx_ripslop);
         
         damage = ((P_Random(pr_rip) & 3) + 2) * cc->thing->damage;
         
         P_DamageMobj(mo, cc->thing, cc->thing->target, damage, 
                      cc->thing->info->mod);
         
         if(mo->flags2 & MF2_PUSHABLE && !(cc->thing->flags3 & MF3_CANNOTPUSH))
         { 
            // Push thing
            mo->momx += cc->thing->momx >> 2;
            mo->momy += cc->thing->momy >> 2;
         }
         
         // TODO: huh?
         //numspechit = 0;
         return true;
      }

      // killough 8/10/98: if moving thing is not a missile, no damage
      // is inflicted, and momentum is reduced if object hit is solid.

      if(!(cc->thing->flags & MF_MISSILE))
      {
         if(!(mo->flags & MF_SOLID))
            return true;
         else
         {
            cc->thing->momx = -cc->thing->momx;
            cc->thing->momy = -cc->thing->momy;
            if(!(cc->thing->flags & MF_NOGRAVITY))
            {
               cc->thing->momx >>= 2;
               cc->thing->momy >>= 2;
            }

            return false;
         }
      }

      if(!(mo->flags & MF_SHOOTABLE))
         return !(mo->flags & MF_SOLID); // didn't do any damage

      // damage / explode
      
      damage = ((P_Random(pr_damage)%8)+1)*cc->thing->damage;
      P_DamageMobj(mo, cc->thing, cc->thing->target, damage,
                   cc->thing->info->mod);

      // don't traverse any more
      return false;
   }

   // haleyjd 1/16/00: Pushable objects -- at last!
   //   This is remarkably simpler than I had anticipated!
   
   if(mo->flags2 & MF2_PUSHABLE && !(cc->thing->flags3 & MF3_CANNOTPUSH))
   {
      // transfer one-fourth momentum along the x and y axes
      mo->momx += cc->thing->momx / 4;
      mo->momy += cc->thing->momy / 4;
   }

   // check for special pickup

   if(mo->flags & MF_SPECIAL
      // [RH] The next condition is to compensate for the extra height
      // that gets added by P_CheckPosition() so that you cannot pick
      // up things that are above your true height.
      && mo->z < cc->thing->z + cc->thing->height - 24*FRACUNIT)
      return P_CheckPickUp(mo, cc);

   // killough 3/16/98: Allow non-solid moving objects to move through solid
   // ones, by allowing the moving thing (tmthing) to move if it's non-solid,
   // despite another solid thing being in the way.
   // killough 4/11/98: Treat no-clipping things as not blocking

   return !((mo->flags & MF_SOLID && !(mo->flags & MF_NOCLIP))
          && cc->thing->flags & MF_SOLID);
}



bool CheckLineThing(line_t *line, MapContext *mc)
{
   ClipContext *cc = mc->clipContext();
   if(cc->bbox[BOXRIGHT]  <= line->bbox[BOXLEFT]   || 
      cc->bbox[BOXLEFT]   >= line->bbox[BOXRIGHT]  || 
      cc->bbox[BOXTOP]    <= line->bbox[BOXBOTTOM] || 
      cc->bbox[BOXBOTTOM] >= line->bbox[BOXTOP])
      return true; // didn't hit it

   if(P_BoxOnLineSide(cc->bbox, line) != -1)
      return true; // didn't hit it

   // A line has been hit
   
   // The moving thing's destination position will cross the given line.
   // If this should not be allowed, return false.
   // If the line is special, keep track of it
   // to process later if the move is proven ok.
   // NOTE: specials are NOT sorted by order,
   // so two special lines that are only 8 pixels apart
   // could be crossed in either order.

   // killough 7/24/98: allow player to move out of 1s wall, to prevent sticking
   // haleyjd 04/30/11: treat block-everything lines like they're 1S
   if(!line->backsector || (line->extflags & EX_ML_BLOCKALL)) // one sided line
   {
      cc->blockline = line;
      return cc->unstuck &&
         FixedMul(cc->x - cc->thing->x, line->dy) > FixedMul(cc->y - cc->thing->y, line->dx);
   }
   // killough 8/10/98: allow bouncing objects to pass through as missiles
   if(!(cc->thing->flags & (MF_MISSILE | MF_BOUNCES)))
   {
      if(line->flags & ML_BLOCKING)           // explicitly blocking everything
         return !!cc->unstuck;  // killough 8/1/98: allow escape

      // killough 8/9/98: monster-blockers don't affect friends
      // SoM 9/7/02: block monsters standing on 3dmidtex only
      if(!(cc->thing->flags & MF_FRIEND || cc->thing->player) && 
         line->flags & ML_BLOCKMONSTERS && 
         !(line->flags & ML_3DMIDTEX))
         return false; // block monsters only
   }

   // set openrange, opentop, openbottom
   // these define a 'window' from one sector to another across this line
   
   open_t opening;
   clip->lineOpening(line, cc->thing, &opening, cc);

   // adjust floor & ceiling heights
   
   if(opening.openflags & OF_SETSCEILINGZ && cc->adjacencytype != AG_FLOORPORTAL) 
   {
      if(opening.top < cc->ceilingz)
      {
         cc->ceilingz = opening.top;
         cc->ceilingline = line;
         cc->blockline = line;
      }

      if(opening.secceil < cc->secceilz)
         cc->secceilz = opening.secceil;
      if(cc->ceilingz < cc->passceilz)
         cc->passceilz = cc->ceilingz;
   }

   if(opening.openflags & OF_SETSFLOORZ  && cc->adjacencytype != AG_CEILPORTAL)
   {
      if(opening.bottom > cc->floorz)
      {
         cc->floorz = opening.bottom;

         cc->floorline = line;          // killough 8/1/98: remember floor linedef
         cc->blockline = line;
      }

      if(opening.lowfloor < cc->dropoffz)
         cc->dropoffz = opening.lowfloor;

      // haleyjd 11/10/04: 3DMidTex fix: never consider dropoffs when
      // touching 3DMidTex lines.
      if(cc->touch3dside)
         cc->dropoffz = cc->floorz;

      if(opening.secfloor > cc->secfloorz)
         cc->secfloorz = opening.secfloor;
      if(cc->floorz > cc->passfloorz)
         cc->passfloorz = cc->floorz;
   }

   PIT_FindAdjacentPortals(line, mc);

   // if contacted a special line, add it to the list
   if(line->special || line->portal)
   {
      cc->spechit.add(line);
   }
   
   return true;
}



bool PortalClipEngine::checkPosition(Mobj *thing, fixed_t x, fixed_t y, ClipContext *cc)
{
   int xl, xh, yl, yh;

   auto newsubsec = R_PointInSubsector(x,y);
   auto startgroup = newsubsec->sector->groupid;

   cc->thing = thing;
   cc->z = thing->z;
   cc->height = thing->info->height + (24 >> FRACBITS);
   
   cc->floorz = D_MININT;
   cc->ceilingz = D_MAXINT;
   
   cc->spechit.makeEmpty();
   cc->adjacent_groups.makeEmpty();
   cc->getMarkedGroups()->clearMarks();

   HitPortalGroup(startgroup, AG_CURRENTLOCATION, cc);

   // SoM: 09/07/02: 3dsides monster fix
   cc->touch3dside = 0;
   validcount++;
   
   cc->spechit.makeEmpty();

   // haleyjd 06/28/06: skullfly check from zdoom
   if(cc->thing->flags & MF_NOCLIP && !(cc->thing->flags & MF_SKULLFLY))
   {
      // Emulate old noclip behavior
      cc->x = x;
      cc->y = y;
      CheckSubsectorPosition(cc);
      return true;
   }

   if(!(thing->flags & MF_NOCLIP))
      cc->getMarkedLines()->clearMarks();

   for(size_t i = 0; i < cc->adjacent_groups.getLength(); ++i)
   {
      auto adjacentgroup = cc->adjacent_groups.at(i);
      auto link = P_GetLinkOffset(startgroup, adjacentgroup.group);

      cc->x = x + link->x;
      cc->y = y + link->y;
      cc->adjacencytype = adjacentgroup.linktype;

      CheckSubsectorPosition(cc);

      CalculateBBoxForThing(cc, x, y, thing->radius, link);
      GetBlockmapBoundsFromBBox(cc, xl, yl, xh, yh);

      int rejectmask = 0;
      int bx, by;
      for(by = yl; by <= yh; ++by)
      {
         rejectmask &= ~EAST_ADJACENT;

         if(by < 0 || by >= bmapheight)
            continue;

         for(bx = xl; bx <= xh; ++bx)
         {
            if(bx < 0 || bx >= bmapwidth)
               continue;

            auto link = blocklinks[by * bmapwidth + bx];

            while(link)
            {
               if(link->adjacencymask & rejectmask || CheckThing(link->mo, cc))
                  link = link->bnext;
               else
                  return false;
            }

            rejectmask |= EAST_ADJACENT;

            if(!(thing->flags & MF_NOCLIP))
            {
               if(!blockLinesIterator(bx, by, CheckLineThing, cc))
                  return false; // doesn't fit
            }
         }
      }
   }
   return true;
}