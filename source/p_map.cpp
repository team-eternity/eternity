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
//  Movement, collision handling.
//  Shooting and aiming.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "c_io.h"
#include "d_gi.h"
#include "d_mod.h"
#include "doomstat.h"
#include "e_exdata.h"
#include "e_player.h"
#include "e_states.h"
#include "e_things.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "m_compare.h"
#include "p_info.h"
#include "p_inter.h"
#include "p_map3d.h"
#include "p_mobjcol.h"
#include "p_portal.h"
#include "p_portalblockmap.h"
#include "p_portalcross.h"
#include "p_setup.h"
#include "p_skin.h"
#include "p_spec.h"
#include "r_main.h"
#include "r_portal.h"
#include "r_state.h"
#include "s_sound.h"
#include "v_misc.h"


// SoM: This should be ok left out of the globals struct.
static int pe_x; // Pain Elemental position for Lost Soul checks // phares
static int pe_y; // Pain Elemental position for Lost Soul checks // phares
static int ls_x; // Lost Soul position for Lost Soul checks      // phares
static int ls_y; // Lost Soul position for Lost Soul checks      // phares

// SoM: Todo: Turn this into a full fledged stack.
doom_mapinter_t  clip;
doom_mapinter_t *pClip = &clip;

static doom_mapinter_t *unusedclip = nullptr;

// CLIP STACK

// 
// P_PushClipStack
//
// Allocates or assigns a new head to the tm stack
//
void P_PushClipStack(void)
{
   doom_mapinter_t *newclip;

   if(!unusedclip)
      newclip = ecalloc(doom_mapinter_t *, 1, sizeof(doom_mapinter_t));
   else
   {
      // SoM: Do not clear out the spechit stuff
      int     msh = unusedclip->spechit_max;
      line_t **sh = unusedclip->spechit;

      // ioanch: same with portalhit
      int mph = unusedclip->portalhit_max;
      doom_mapinter_t::linepoly_t *ph = unusedclip->portalhit;

      newclip    = unusedclip;
      unusedclip = unusedclip->prev;
      memset(newclip, 0, sizeof(*newclip));

      newclip->spechit_max = msh;
      newclip->spechit     = sh;

      newclip->portalhit_max = mph;
      newclip->portalhit = ph;
   }

   newclip->prev = pClip;
   pClip = newclip;
}

//
// P_PopClipStack
//
// Removes the current tm object from the stack and places it in the unused 
// stack. If pClip is currently set to clip, this function will I_Error
//
void P_PopClipStack(void)
{
   doom_mapinter_t *oldclip;

#ifdef RANGECHECK
   if(pClip == &clip)
      I_Error("P_PopClipStack: clip stack underflow\n");
#endif

   oldclip = pClip;
   pClip   = pClip->prev;
   
   oldclip->prev = unusedclip;
   unusedclip    = oldclip;
}

int spechits_emulation;
#define MAXSPECHIT_OLD 8         // haleyjd 09/20/06: old limit for overflow emu

// haleyjd 09/20/06: moved to top for maximum visibility
static int crushchange;
static int nofit;

//
// TELEPORT MOVE
//

//
// PIT_StompThing
//

static bool telefrag; // killough 8/9/98: whether to telefrag at exit

// haleyjd 06/06/05: whether to return false if an inert thing 
// blocks a teleport. DOOM has allowed you to simply get stuck in
// such things so far.
static bool ignore_inerts = true;

// SoM: for portal teleports, PIT_StompThing will stomp anything the player is touching on the
// x/y plane which means if the player jumps through a mile above a demon, the demon will be
// telefragged. This simply will not do.
static bool stomp3d = false;

static bool PIT_StompThing3D(Mobj *thing, void *context)
{
   fixed_t blockdist;
   
   if(!(thing->flags & MF_SHOOTABLE)) // Can't shoot it? Can't stomp it!
   {
      return true;
   }
   
   blockdist = thing->radius + clip.thing->radius;
   
   if(D_abs(thing->x - clip.x) >= blockdist ||
      D_abs(thing->y - clip.y) >= blockdist)
      return true; // didn't hit it
   
   // don't clip against self
   if(thing == clip.thing)
      return true;

   // Don't stomp what you ain't touchin'!
   if(clip.thing->z >= thing->z + thing->height ||
      thing->z >= clip.thing->z + clip.thing->height)
      return true;

   // The object moving is a player?
   if(clip.thing->player)
   {
      // "thing" dies, unconditionally
      P_DamageMobj(thing, clip.thing, clip.thing, GOD_BREACH_DAMAGE, MOD_TELEFRAG); // Stomp!

      // if "thing" is also a player, both die, for fairness.
      if(thing->player)
         P_DamageMobj(clip.thing, thing, thing, GOD_BREACH_DAMAGE, MOD_TELEFRAG);
   }
   else if(thing->player) // Thing moving into a player?
   {
      // clip.thing dies
      P_DamageMobj(clip.thing, thing, thing, GOD_BREACH_DAMAGE, MOD_TELEFRAG);
   }
   else // Neither thing is a player...
   {
   }
   
   return true;
}

static bool PIT_StompThing(Mobj *thing, void *context)
{
   fixed_t blockdist;
   
   if(!(thing->flags & MF_SHOOTABLE)) // Can't shoot it? Can't stomp it!
   {
      // haleyjd 06/06/05: some teleports may not want to stick the
      // object right inside of an inert object at the destination...
      if(ignore_inerts)
         return true;
   }
   
   blockdist = thing->radius + clip.thing->radius;
   
   if(D_abs(thing->x - clip.x) >= blockdist ||
      D_abs(thing->y - clip.y) >= blockdist)
      return true; // didn't hit it
   
   // don't clip against self
   if(thing == clip.thing)
      return true;
   
   // monsters don't stomp things except on boss level
   // killough 8/9/98: make consistent across all levels
   if(!telefrag)
      return false;
   
   P_DamageMobj(thing, clip.thing, clip.thing, GOD_BREACH_DAMAGE, MOD_TELEFRAG); // Stomp!
   
   return true;
}

//
// killough 8/28/98:
//
// P_GetFriction()
//
// Returns the friction associated with a particular mobj.
//
int P_GetFriction(const Mobj *mo, int *frictionfactor)
{
   int friction = ORIG_FRICTION;
   int movefactor = ORIG_FRICTION_FACTOR;
   const msecnode_t *m;
   const sector_t *sec;

   // Assign the friction value to objects on the floor, non-floating,
   // and clipped. Normally the object's friction value is kept at
   // ORIG_FRICTION and this thinker changes it for icy or muddy floors.
   //
   // When the object is straddling sectors with the same
   // floorheight that have different frictions, use the lowest
   // friction value (muddy has precedence over icy).

   bool onfloor = P_OnGroundOrThing(*mo);

   if(mo->flags4 & MF4_FLY && !onfloor)
      friction = FRICTION_FLY;
   else if(mo->player && LevelInfo.airFriction < FRACUNIT && !onfloor)
   {
      // Air friction only affects players
      friction = FRACUNIT - LevelInfo.airFriction;
   }   
   else if(!(mo->flags & (MF_NOCLIP|MF_NOGRAVITY)) && 
           (demo_version >= 203 || (mo->player && !compatibility)) &&
           variable_friction)
   {
      for (m = mo->touching_sectorlist; m; m = m->m_tnext)
      {
         if(useportalgroups && full_demo_version >= make_full_version(340, 48) &&
            !P_SectorTouchesThingVertically(m->m_sector, mo))
         {
            continue;
         }
         if((sec = m->m_sector)->flags & SECF_FRICTION &&
            (sec->friction < friction || friction == ORIG_FRICTION) &&
            (mo->z <= sec->srf.floor.height ||
             (sec->heightsec != -1 &&
              mo->z <= sectors[sec->heightsec].srf.floor.height &&
              demo_version >= 203)))
         {
            friction = sec->friction;
            movefactor = sec->movefactor;
         }
      }
   }
   
   if(frictionfactor)
      *frictionfactor = movefactor;
   
   return friction;
}

// phares 3/19/98
// P_GetMoveFactor() returns the value by which the x,y
// movements are multiplied to add to player movement.
//
// killough 8/28/98: rewritten
//
int P_GetMoveFactor(Mobj *mo, int *frictionp)
{
   int movefactor, friction;

   // haleyjd 04/11/10: restored BOOM friction code for compatibility
   if(demo_version < 203)
   {
      movefactor = ORIG_FRICTION_FACTOR;

      if(!compatibility && variable_friction && 
         !(mo->flags & (MF_NOGRAVITY | MF_NOCLIP)))
      {
         friction = mo->friction;

         if (friction == ORIG_FRICTION)            // normal floor
            ;
         else if (friction > ORIG_FRICTION)        // ice
         {
            movefactor = mo->movefactor;
            mo->movefactor = ORIG_FRICTION_FACTOR;  // reset
         }
         else                                      // sludge
         {
            // phares 3/11/98: you start off slowly, then increase as
            // you get better footing

            int momentum = (P_AproxDistance(mo->momx, mo->momy));
            movefactor = mo->movefactor;
            if (momentum > MORE_FRICTION_MOMENTUM<<2)
               movefactor <<= 3;

            else if (momentum > MORE_FRICTION_MOMENTUM<<1)
               movefactor <<= 2;

            else if (momentum > MORE_FRICTION_MOMENTUM)
               movefactor <<= 1;

            mo->movefactor = ORIG_FRICTION_FACTOR;  // reset
         }
      }

      return movefactor;
   }
   
   // If the floor is icy or muddy, it's harder to get moving. This is where
   // the different friction factors are applied to 'trying to move'. In
   // p_mobj.c, the friction factors are applied as you coast and slow down.

   if((friction = P_GetFriction(mo, &movefactor)) < ORIG_FRICTION)
   {
      // phares 3/11/98: you start off slowly, then increase as
      // you get better footing
      
      int momentum = P_AproxDistance(mo->momx,mo->momy);
      
      if(momentum > MORE_FRICTION_MOMENTUM<<2)
         movefactor <<= 3;
      else if(momentum > MORE_FRICTION_MOMENTUM<<1)
         movefactor <<= 2;
      else if(momentum > MORE_FRICTION_MOMENTUM)
         movefactor <<= 1;
   }

   if(frictionp)
      *frictionp = friction;
   
   return movefactor;
}

//
// P_TeleportMove
//

// killough 8/9/98
bool P_TeleportMove(Mobj *thing, fixed_t x, fixed_t y, unsigned flags)
{
   int xl, xh, yl, yh, bx, by;
   subsector_t *newsubsec;
   bool (*func)(Mobj *, void *);
   
   // killough 8/9/98: make telefragging more consistent, preserve compatibility
   // haleyjd 03/25/03: TELESTOMP flag handling moved here (was thing->player)
   // TODO: make this an EMAPINFO flag
   telefrag = (thing->flags3 & MF3_TELESTOMP) || !!(flags & TELEMOVE_FRAG) ||
              (!getComp(comp_telefrag) ? !!(flags & TELEMOVE_BOSS) : (gamemap == 30));

   // kill anything occupying the position
   
   clip.thing = thing;
   
   clip.x = x;
   clip.y = y;
   
   clip.bbox[BOXTOP]    = y + clip.thing->radius;
   clip.bbox[BOXBOTTOM] = y - clip.thing->radius;
   clip.bbox[BOXRIGHT]  = x + clip.thing->radius;
   clip.bbox[BOXLEFT]   = x - clip.thing->radius;
   
   newsubsec = R_PointInSubsector(x,y);
   clip.ceilingline = nullptr;
   
   // The base floor/ceiling is from the subsector
   // that contains the point.
   // Any contacted lines the step closer together
   // will adjust them.
   
   // ioanch 20160113: use correct floor and ceiling heights
   const sector_t *bottomfloorsector = newsubsec->sector;
    //newsubsec->sector->floorheight - clip.thing->height;
   if(demo_version >= 333 && newsubsec->sector->srf.floor.pflags & PS_PASSABLE)
   {
      bottomfloorsector = P_ExtremeSectorAtPoint(x, y, surf_floor, newsubsec->sector);
      clip.zref.floor = clip.zref.dropoff = bottomfloorsector->srf.floor.height;
      clip.zref.floorgroupid = bottomfloorsector->groupid;
   }
   else
   {
      clip.zref.floor = clip.zref.dropoff = newsubsec->sector->srf.floor.height;
      clip.zref.floorgroupid = newsubsec->sector->groupid;
   }

    //newsubsec->sector->ceilingheight + clip.thing->height;
   if(demo_version >= 333 && newsubsec->sector->srf.ceiling.pflags & PS_PASSABLE)
   {
      clip.zref.ceiling = P_ExtremeSectorAtPoint(x, y, surf_ceil,
            newsubsec->sector)->srf.ceiling.height;
   }
   else
      clip.zref.ceiling = newsubsec->sector->srf.ceiling.height;

   clip.zref.secfloor = clip.zref.passfloor = clip.zref.floor;
   clip.zref.secceil = clip.zref.passceil = clip.zref.ceiling;

   // haleyjd
   // ioanch 20160114: use the final sector below
   clip.open.floorpic = bottomfloorsector->srf.floor.pic;
   
   // SoM 09/07/02: 3dsides monster fix
   clip.open.touch3dside = 0;
   
   validcount++;
   clip.numspechit = 0;
   
   // stomp on any things contacted
   if(stomp3d)
      func = PIT_StompThing3D;
   else
      func = PIT_StompThing;
   
   xl = (clip.bbox[BOXLEFT  ] - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
   xh = (clip.bbox[BOXRIGHT ] - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
   yl = (clip.bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
   yh = (clip.bbox[BOXTOP   ] - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

   for(bx = xl; bx <= xh; bx++)
   {
      for(by = yl; by <= yh; by++)
      {
         if(!P_BlockThingsIterator(bx, by, func))
            return false;
      }
   }

   // the move is ok,
   // so unlink from the old position & link into the new position
   
   P_UnsetThingPosition(thing);

   thing->zref = clip.zref;
   
   thing->x = x;
   thing->y = y;
   
   thing->backupPosition();   
   P_SetThingPosition(thing);
   
   return true;
}

//
// P_TeleportMoveStrict
//
// haleyjd 06/06/05: Sets the ignore_inerts flag to false and calls
// P_TeleportMove. The result is that things won't get stuck inside
// inert objects that are at their destination. Rather, the teleport
// is rejected.
//
bool P_TeleportMoveStrict(Mobj *thing, fixed_t x, fixed_t y, bool boss)
{
   bool res;

   ignore_inerts = false;
   res = P_TeleportMove(thing, x, y, boss ? TELEMOVE_BOSS : 0);
   ignore_inerts = true;
   
   return res;
}

//
// MOVEMENT ITERATOR FUNCTIONS
//

//                                                                  // phares
// PIT_CrossLine                                                    //   |
// Checks to see if a PE->LS trajectory line crosses a blocking     //   V
// line. Returns false if it does.
//
// tmbbox holds the bounding box of the trajectory. If that box
// does not touch the bounding box of the line in question,
// then the trajectory is not blocked. If the PE is on one side
// of the line and the LS is on the other side, then the
// trajectory is blocked.
//
// Currently this assumes an infinite line, which is not quite
// correct. A more correct solution would be to check for an
// intersection of the trajectory and the line, but that takes
// longer and probably really isn't worth the effort.
//
// killough 11/98: reformatted

static bool PIT_CrossLine(line_t *ld, polyobj_t *po, void *context)
{
   auto type = static_cast<const mobjtype_t *>(context);
   // SoM 9/7/02: wow a killoughism... * SoM is scared
   int flags = ML_TWOSIDED | ML_BLOCKING |
      (mobjinfo[*type]->flags4 & MF4_MONSTERPASS ? 0 : ML_BLOCKMONSTERS);

   if(ld->flags & ML_3DMIDTEX)
      flags &= ~ML_BLOCKMONSTERS;

   return 
      !((ld->flags ^ ML_TWOSIDED) & flags)
      || clip.bbox[BOXLEFT]   > ld->bbox[BOXRIGHT]
      || clip.bbox[BOXRIGHT]  < ld->bbox[BOXLEFT]   
      || clip.bbox[BOXTOP]    < ld->bbox[BOXBOTTOM]
      || clip.bbox[BOXBOTTOM] > ld->bbox[BOXTOP]
      || P_PointOnLineSide(pe_x,pe_y,ld) == P_PointOnLineSide(ls_x,ls_y,ld);
}

// killough 8/1/98: used to test intersection between thing and line
// assuming NO movement occurs -- used to avoid sticky situations.

static int untouched(const line_t *ld)
{
   fixed_t x, y, tmbbox[4];
   return 
     (tmbbox[BOXRIGHT] = (x=clip.thing->x)+clip.thing->radius) <= ld->bbox[BOXLEFT] ||
     (tmbbox[BOXLEFT] = x-clip.thing->radius) >= ld->bbox[BOXRIGHT] ||
     (tmbbox[BOXTOP] = (y=clip.thing->y)+clip.thing->radius) <= ld->bbox[BOXBOTTOM] ||
     (tmbbox[BOXBOTTOM] = y-clip.thing->radius) >= ld->bbox[BOXTOP] ||
     P_BoxOnLineSide(tmbbox, ld) != -1;
}

//
// SpechitOverrun
//
// This function implements spechit overflow emulation. The spechit array only
// had eight entries in the original engine, far too few for a huge number of
// user-made wads. Any time an object triggered more than 8 walkover lines
// during one movement, some of the globals in this module would get trashed.
// Most of them don't matter, but a few can affect clipping. This code is
// originally by Andrey Budko of PrBoom-plus, and has some modifications from
// Chocolate Doom as well. Thanks to Andrey and fraggle.
//
static void SpechitOverrun(line_t *ld)
{
   static bool firsttime = true;
   static bool spechitparm = false;
   static unsigned int baseaddr = 0;
   unsigned int addr;
   
   // first time through, set the desired base address;
   // this is where magic number support comes in
   if(firsttime)
   {
      int p;

      if((p = M_CheckParm("-spechit")) && p < myargc - 1)
      {
         baseaddr = (unsigned int)strtol(myargv[p + 1], nullptr, 0);
         spechitparm = true;
      }
      else
         baseaddr = spechits_emulation == 2 ? 0x01C09C98 : 0x84F968E8;

      firsttime = false;
   }

   // if -spechit used, always emulate even outside of demos
   if(!spechitparm)
   {
      // otherwise, only during demos and when so specified
      if(!(demo_compatibility && spechits_emulation))
         return;
   }

   // What's this? The baseaddr is a value suitably close to the address of the
   // lines array as it was during the recording of the demo. This is added to
   // the offset of the line in the array times the original structure size to
   // reconstruct the approximate line addresses actually written. In most cases
   // this doesn't matter because of the nature of tmbbox, however.
   addr = static_cast<unsigned>(baseaddr + (ld - lines) * 0x3E);

   // Note: only the variables affected up to 20 are known, and it is of no
   // consequence to alter any of the variables between 15 and 20 because they
   // are all statics used by PIT_ iterator functions in this module and are
   // always reset to a valid value before being used again.
   switch(clip.numspechit)
   {
   case 9:
   case 10:
   case 11:
   case 12:
      clip.bbox[clip.numspechit - 9] = addr;
      break;
   case 13:
      crushchange = addr;
      break;
   case 14:
      nofit = addr;
      break;
   default:
      C_Printf(FC_ERROR "Warning: spechit overflow %d not emulated\a\n", 
               clip.numspechit);
      break;
   }
}

//
// P_CollectSpechits
//
// ioanch: moved this here so it may be called from elsewhere too
//
void P_CollectSpechits(line_t *ld, PODCollection<line_t *> *pushhit)
{
   // if contacted a special line, add it to the list
   // ioanch 20160121: check for PS_PASSABLE, to restrict just for linked portals
   if(ld->special || ld->pflags & PS_PASSABLE)  
   {
      if(ld->pflags & PS_PASSABLE)
         gGroupVisit[ld->portal->data.link.toid] = true;
      else if(pushhit)  // don't attempt adding passable lines to pushables.
         pushhit->add(ld);
      // 1/11/98 killough: remove limit on lines hit, by array doubling
      if(clip.numspechit >= clip.spechit_max)
      {
         clip.spechit_max = clip.spechit_max ? clip.spechit_max * 2 : 8;
         clip.spechit = erealloc(line_t **, clip.spechit, sizeof(*clip.spechit) * clip.spechit_max);
      }
      clip.spechit[clip.numspechit++] = ld;

      // haleyjd 09/20/06: spechit overflow emulation
      if(clip.numspechit > MAXSPECHIT_OLD)
         SpechitOverrun(ld);
   }
}

//
// Returns true if line should be blocked by ML_BLOCKMONSTERS lines.
//
bool P_BlockedAsMonster(const Mobj &mo)
{
   return !(mo.flags & MF_FRIEND) && !mo.player &&
          !(mo.flags4 & MF4_MONSTERPASS);
}

//
// PIT_CheckLine
//
// Adjusts tmfloorz and tmceilingz as lines are contacted
//
bool PIT_CheckLine(line_t *ld, polyobj_t *po, void *context)
{
   auto pushhit = static_cast<PODCollection<line_t *> *>(context);
   if(clip.bbox[BOXRIGHT]  <= ld->bbox[BOXLEFT]   || 
      clip.bbox[BOXLEFT]   >= ld->bbox[BOXRIGHT]  || 
      clip.bbox[BOXTOP]    <= ld->bbox[BOXBOTTOM] || 
      clip.bbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
      return true; // didn't hit it

   if(P_BoxOnLineSide(clip.bbox, ld) != -1)
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
   if(!ld->backsector || (ld->extflags & EX_ML_BLOCKALL)) // one sided line
   {
      clip.blockline = ld;
      bool result = clip.unstuck && !untouched(ld) &&
         FixedMul(clip.x-clip.thing->x,ld->dy) > FixedMul(clip.y-clip.thing->y,ld->dx);
      if(!result && pushhit && ld->special &&
         full_demo_version >= make_full_version(401, 0))
      {
         pushhit->add(ld);
      }
      return result;
   }

   // killough 8/10/98: allow bouncing objects to pass through as missiles
   if(!(clip.thing->flags & (MF_MISSILE | MF_BOUNCES)))
   {
      if((ld->flags & ML_BLOCKING) ||
         (mbf21_temp && !(ld->flags & ML_RESERVED) && clip.thing->player && (ld->flags & ML_BLOCKPLAYERS)))
      {
         // explicitly blocking everything
         // or blocking player
         bool result = clip.unstuck && !untouched(ld);  // killough 8/1/98: allow escape

         // When it's Hexen, keep side 0 even when hitting from backside
         if(!result && pushhit && ld->special &&
            full_demo_version >= make_full_version(401, 0))
         {
            pushhit->add(ld);
         }
         // TODO: add the other push special checks.
         // TODO: add for P_Map3D and P_PortalClip CPP files.
         return result;
      }

      // killough 8/9/98: monster-blockers don't affect friends
      // SoM 9/7/02: block monsters standing on 3dmidtex only
      // MaxW: Land-monster blockers gotta be factored in, too
      if(!(ld->flags & ML_3DMIDTEX) && P_BlockedAsMonster(*clip.thing) &&
         (
            ld->flags & ML_BLOCKMONSTERS ||
            (mbf21_temp && (ld->flags & ML_BLOCKLANDMONSTERS) && !(clip.thing->flags & MF_FLOAT))
         )
        )
      {
         return false; // block monsters only
      }
   }

   // set openrange, opentop, openbottom
   // these define a 'window' from one sector to another across this line
   
   clip.open = P_LineOpening(ld, clip.thing);

   // adjust floor & ceiling heights
   
   if(clip.open.height.ceiling < clip.zref.ceiling)
   {
      clip.zref.ceiling = clip.open.height.ceiling;
      clip.ceilingline = ld;
      clip.blockline = ld;
   }

   if(clip.open.height.floor > clip.zref.floor)
   {
      clip.zref.floor = clip.open.height.floor;
      clip.zref.floorgroupid = clip.open.bottomgroupid;

      clip.floorline = ld;          // killough 8/1/98: remember floor linedef
      clip.blockline = ld;
   }

   if(clip.open.lowfloor < clip.zref.dropoff)
      clip.zref.dropoff = clip.open.lowfloor;

   // haleyjd 11/10/04: 3DMidTex fix: never consider dropoffs when
   // touching 3DMidTex lines.
   if(demo_version >= 331 && clip.open.touch3dside)
      clip.zref.dropoff = clip.zref.floor;

   if(clip.open.sec.floor > clip.zref.secfloor)
      clip.zref.secfloor = clip.open.sec.floor;
   if(clip.open.sec.ceiling < clip.zref.secceil)
      clip.zref.secceil = clip.open.sec.ceiling;

   // SoM 11/6/02: AGHAH
   if(clip.zref.floor > clip.zref.passfloor)
      clip.zref.passfloor = clip.zref.floor;
   if(clip.zref.ceiling < clip.zref.passceil)
      clip.zref.passceil = clip.zref.ceiling;

   // ioanch 20160113: moved to a special function
   P_CollectSpechits(ld, pushhit);
   
   return true;
}

//
// P_Touched
//
// haleyjd: isolated code to test for and execute touchy thing death.
// Required for proper 3D clipping support.
//
// haleyjd 12/28/10: must read from clip.thing in case of reentrant calls
//
bool P_Touched(Mobj *thing)
{
   // EDF FIXME: temporary fix?
   int painType  = E_ThingNumForDEHNum(MT_PAIN); 
   int skullType = E_ThingNumForDEHNum(MT_SKULL);

   if(thing->flags & MF_TOUCHY &&                  // touchy object
      clip.thing->flags & MF_SOLID &&              // solid object touches it
      thing->health > 0 &&                         // touchy object is alive
      (thing->intflags & MIF_ARMED ||              // Thing is an armed mine
       sentient(thing)) &&                         // ... or a sentient thing
      (thing->type != clip.thing->type ||          // only different species
       thing->player) &&                           // ... or different players
      thing->z + thing->height >= clip.thing->z && // touches vertically
      clip.thing->z + clip.thing->height >= thing->z &&
      (thing->type ^ painType) |                   // PEs and lost souls
      (clip.thing->type ^ skullType) &&            // are considered same
      (thing->type ^ skullType) |                  // (but Barons & Knights
      (clip.thing->type ^ painType))               // are intentionally not)
   {
      P_DamageMobj(thing, nullptr, nullptr, thing->health, MOD_UNKNOWN); // kill object
      return true;
   }

   return false;
}

//
// P_CheckPickUp
//
// haleyjd: isolated code to test for and execute touching specials.
// Required for proper 3D clipping support.
//
// haleyjd 12/28/10: Must read from clip.thing in case of reentrant calls.
//
bool P_CheckPickUp(Mobj *thing)
{
   int solid = thing->flags & MF_SOLID;

   if(clip.thing->flags & MF_PICKUP)
      P_TouchSpecialThing(thing, clip.thing); // can remove thing

   return !solid;
}

//
// P_SkullHit
//
// haleyjd: isolated code to test for and execute SKULLFLY objects hitting
// things. Returns true if PIT_CheckThing should exit.
//
// haleyjd 12/28/10: must read from clip.thing or a reentrant call to this
// routine through P_DamageMobj can result in loss of demo compatibility.
//
bool P_SkullHit(Mobj *thing)
{
   bool ret = false;

   if(clip.thing->flags & MF_SKULLFLY)
   {
      // A flying skull is smacking something.
      // Determine damage amount, and the skull comes to a dead stop.

      int damage = (P_Random(pr_skullfly) % 8 + 1) * clip.thing->damage;
      
      P_DamageMobj(thing, clip.thing, clip.thing, damage, clip.thing->info->mod);
      
      clip.thing->flags &= ~MF_SKULLFLY;
      clip.thing->momx = clip.thing->momy = clip.thing->momz = 0;

      // haleyjd: Note that this is where potential for a recursive clipping
      // operation occurs -- P_SetMobjState causes a call to A_Look, which
      // causes another state-set to the seestate, which calls A_Chase.
      // A_Chase in turn calls P_TryMove and that can cause a lot of shit
      // to explode.

      if(clip.thing->intflags & MIF_SKULLFLYSEE)
         P_SetMobjState(clip.thing, clip.thing->info->seestate);
      else
         P_SetMobjState(clip.thing, clip.thing->info->spawnstate);
      clip.thing->intflags &= ~MIF_SKULLFLYSEE;

      clip.BlockingMobj = nullptr; // haleyjd: from zdoom

      ret = true; // stop moving
   }

   return ret;
}

//
// P_MissileBlockHeight
//
// haleyjd 07/06/05: function that returns the height to be used 
// when considering an object for missile collisions. Some decorative
// objects do not want to use their correct 3D height for clipping
// missiles, since this alters the playability of the game severely
// in areas of many maps.
//
int P_MissileBlockHeight(Mobj *mo)
{
   return (demo_version >= 333 && !getComp(comp_theights) &&
           mo->flags3 & MF3_3DDECORATION) ? mo->info->height : mo->height;
}

//
// True if missile damage should be allowed
//
inline static bool P_allowMissileDamage(const Mobj &shooter, const Mobj &target)
{
   return target.player || deh_species_infighting || (shooter.type == target.type &&
                            (shooter.flags4 & MF4_HARMSPECIESMISSILE ||
                             (shooter.flags4 & MF4_FRIENDFOEMISSILE &&
                              (shooter.flags ^ target.flags) & MF_FRIEND))) ||
   (shooter.type != target.type &&
    !E_ThingPairValid(shooter.type, target.type, TGF_PROJECTILEALLIANCE));
}

//
// Common stuff between PIT_CheckThing and PIT_CheckThing3D
//
ItemCheckResult P_CheckThingCommon(Mobj *thing)
{
   // check for skulls slamming into things

   if(P_SkullHit(thing))
      return ItemCheck_hit;

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
         return ItemCheck_pass;

      // see if it went over / under

      if(clip.thing->z > thing->z + height) // haleyjd 07/06/05
         return ItemCheck_pass;    // overhead

      if(clip.thing->z + clip.thing->height < thing->z)
         return ItemCheck_pass;    // underneath

      if(clip.thing->target)
      {
         if(thing == clip.thing->target)
            return ItemCheck_pass;   // Don't hit originator.
         if(!P_allowMissileDamage(*clip.thing->target, *thing))
            return ItemCheck_hit;
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

         S_StartSound(clip.thing, clip.thing->info->ripsound);

         P_DamageMobj(thing, clip.thing, clip.thing->target, damage, clip.thing->info->mod);

         if(thing->flags2 & MF2_PUSHABLE && !(clip.thing->flags3 & MF3_CANNOTPUSH))
         {
            // Push thing
            thing->momx += clip.thing->momx >> 2;
            thing->momy += clip.thing->momy >> 2;
         }

         // TODO: huh?
         if(vanilla_heretic)
            clip.numspechit = 0;
         return ItemCheck_pass;
      }

      // killough 8/10/98: if moving thing is not a missile, no damage
      // is inflicted, and momentum is reduced if object hit is solid.

      if(!(clip.thing->flags & MF_MISSILE))
      {
         if(!(thing->flags & MF_SOLID))
            return ItemCheck_pass;
         else
         {
            clip.thing->momx = -clip.thing->momx;
            clip.thing->momy = -clip.thing->momy;
            if(!(clip.thing->flags & MF_NOGRAVITY))
            {
               clip.thing->momx >>= 2;
               clip.thing->momy >>= 2;
            }

            return ItemCheck_hit;
         }
      }

      if(!(thing->flags & MF_SHOOTABLE))
         return !(thing->flags & MF_SOLID) ? ItemCheck_pass : ItemCheck_hit; // didn't do any damage

      // damage / explode
      
      damage = ((P_Random(pr_damage)%clip.thing->info->damagemod)+1)*clip.thing->damage;
      
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
      return ItemCheck_hit;
   }

   // haleyjd 1/16/00: Pushable objects -- at last!
   //   This is remarkably simpler than I had anticipated!
   
   if(thing->flags2 & MF2_PUSHABLE && !(clip.thing->flags3 & MF3_CANNOTPUSH))
   {
      // transfer one-fourth momentum along the x and y axes
      // ioanch: this may be pedantic, but do '& ~3' to make the '/ 4' operation equivalent to the
      // old '>> 2' arithmetic right shift. We can't add more '>>' here because it's only implemen-
      // tation defined.
      thing->momx += (clip.thing->momx & ~3) / 4;
      thing->momy += (clip.thing->momy & ~3) / 4;
   }

   return ItemCheck_furtherNeeded;
}

//
// PIT_CheckThing
// 
static bool PIT_CheckThing(Mobj *thing, void *context) // killough 3/26/98: make static
{
   fixed_t blockdist;

   // killough 11/98: add touchy things
   if(!(thing->flags & (MF_SOLID|MF_SPECIAL|MF_SHOOTABLE|MF_TOUCHY)))
      return true;

   blockdist = thing->radius + clip.thing->radius;
   
   if(D_abs(thing->x - clip.x) >= blockdist ||
      D_abs(thing->y - clip.y) >= blockdist)
      return true; // didn't hit it

   // killough 11/98:
   //
   // This test has less information content (it's almost always false), so it
   // should not be moved up to first, as it adds more overhead than it removes.
   
   // don't clip against self
   
   if(thing == clip.thing)
      return true;

   // haleyjd 1/17/00: set global hit reference
   clip.BlockingMobj = thing;

   // killough 11/98:
   //
   // TOUCHY flag, for mines or other objects which die on contact with solids.
   // If a solid object of a different type comes in contact with a touchy
   // thing, and the touchy thing is not the sole one moving relative to fixed
   // surroundings such as walls, then the touchy thing dies immediately.

   // haleyjd: functionalized
   if(P_Touched(thing))
      return true;

   ItemCheckResult result = P_CheckThingCommon(thing);
   if(result != ItemCheck_furtherNeeded)
      return result == ItemCheck_pass;

   // check for special pickup
   
   if(thing->flags & MF_SPECIAL)
      return P_CheckPickUp(thing);

   // killough 3/16/98: Allow non-solid moving objects to move through solid
   // ones, by allowing the moving thing (clip.thing) to move if it's non-solid,
   // despite another solid thing being in the way.
   // killough 4/11/98: Treat no-clipping things as not blocking

   // haleyjd: needed for old demos after all?
   if(demo_compatibility)
      return !(thing->flags & MF_SOLID);
   else
      return !((thing->flags & MF_SOLID && !(thing->flags & MF_NOCLIP))
               && (clip.thing->flags & MF_SOLID || demo_compatibility));

   //return !(thing->flags & MF_SOLID);   // old code -- killough
}


//
// Check_Sides
//
// This routine checks for Lost Souls trying to be spawned      // phares
// across 1-sided lines, impassible lines, or "monsters can't   //   |
// cross" lines. Draw an imaginary line between the PE          //   V
// and the new Lost Soul spawn spot. If that line crosses
// a 'blocking' line, then disallow the spawn. Only search
// lines in the blocks of the blockmap where the bounding box
// of the trajectory line resides. Then check bounding box
// of the trajectory vs. the bounding box of each blocking
// line to see if the trajectory and the blocking line cross.
// Then check the PE and LS to see if they're on different
// sides of the blocking line. If so, return true, otherwise
// false.
//
bool Check_Sides(Mobj *actor, int x, int y, mobjtype_t type)
{
   int bx,by,xl,xh,yl,yh;
   
   pe_x = actor->x;
   pe_y = actor->y;
   ls_x = x;
   ls_y = y;

   // Here is the bounding box of the trajectory
   
   clip.bbox[BOXLEFT]   = pe_x < x ? pe_x : x;
   clip.bbox[BOXRIGHT]  = pe_x > x ? pe_x : x;
   clip.bbox[BOXTOP]    = pe_y > y ? pe_y : y;
   clip.bbox[BOXBOTTOM] = pe_y < y ? pe_y : y;

   // Determine which blocks to look in for blocking lines
   
   xl = (clip.bbox[BOXLEFT]   - bmaporgx)>>MAPBLOCKSHIFT;
   xh = (clip.bbox[BOXRIGHT]  - bmaporgx)>>MAPBLOCKSHIFT;
   yl = (clip.bbox[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
   yh = (clip.bbox[BOXTOP]    - bmaporgy)>>MAPBLOCKSHIFT;

   // xl->xh, yl->yh determine the mapblock set to search

   validcount++; // prevents checking same line twice
   for(bx = xl ; bx <= xh ; bx++)
      for (by = yl ; by <= yh ; by++)
         if(!P_BlockLinesIterator(bx,by,PIT_CrossLine, R_NOGROUP, &type))
            return true;                                          //   ^
   return false;                                                  //   |
}                                                                 // phares

//
// MOVEMENT CLIPPING
//

//
// P_CheckPosition
// This is purely informative, nothing is modified
// (except things picked up).
//
// in:
//  a Mobj (can be valid or invalid)
//  a position to be checked
//   (doesn't need to be related to the Mobj->x,y)
//
// during:
//  special things are touched if MF_PICKUP
//  early out on solid lines?
//
// out:
//  newsubsec
//  zref.floor
//  zref.ceiling
//  tmdropoffz
//   the lowest point contacted
//   (monsters won't move to a dropoff)
//  speciallines[]
//  numspeciallines
//
bool P_CheckPosition(Mobj *thing, fixed_t x, fixed_t y, PODCollection<line_t *> *pushhit) 
{
   int xl, xh, yl, yh, bx, by;
   subsector_t *newsubsec;

   // haleyjd: OVER_UNDER
   if(P_Use3DClipping())
      return P_CheckPosition3D(thing, x, y, pushhit);
   
   clip.thing = thing;
   
   clip.x = x;
   clip.y = y;
   
   clip.bbox[BOXTOP]    = y + clip.thing->radius;
   clip.bbox[BOXBOTTOM] = y - clip.thing->radius;
   clip.bbox[BOXRIGHT]  = x + clip.thing->radius;
   clip.bbox[BOXLEFT]   = x - clip.thing->radius;
   
   newsubsec = R_PointInSubsector(x,y);
   clip.floorline = clip.blockline = clip.ceilingline = nullptr; // killough 8/1/98

   // Whether object can get out of a sticky situation:
   clip.unstuck = thing->player &&          // only players
      thing->player->mo == thing &&       // not voodoo dolls
      demo_version >= 203;                // not under old demos

   // The base floor / ceiling is from the subsector
   // that contains the point.
   // Any contacted lines the step closer together
   // will adjust them.

   clip.zref.floor = clip.zref.dropoff = newsubsec->sector->srf.floor.height;
   clip.zref.floorgroupid = newsubsec->sector->groupid;
   clip.zref.ceiling = newsubsec->sector->srf.ceiling.height;

   clip.zref.secfloor = clip.zref.passfloor = clip.zref.floor;
   clip.zref.secceil = clip.zref.passceil = clip.zref.ceiling;

   // haleyjd
   clip.open.floorpic = newsubsec->sector->srf.floor.pic;
   // SoM: 09/07/02: 3dsides monster fix
   clip.open.touch3dside = 0;
   validcount++;
   clip.numspechit = 0;

   if(clip.thing->flags & MF_NOCLIP)
      return true;

   // Check things first, possibly picking things up.
   // The bounding box is extended by MAXRADIUS
   // because Mobjs are grouped into mapblocks
   // based on their origin point, and can overlap
   // into adjacent blocks by up to MAXRADIUS units.

   xl = (clip.bbox[BOXLEFT]   - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
   xh = (clip.bbox[BOXRIGHT]  - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
   yl = (clip.bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
   yh = (clip.bbox[BOXTOP]    - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

   clip.BlockingMobj = nullptr; // haleyjd 1/17/00: global hit reference

   for(bx = xl; bx <= xh; bx++)
   {
      for(by = yl; by <= yh; by++)
      {
         if(!P_BlockThingsIterator(bx, by, PIT_CheckThing))
            return false;
      }
   }

   // check lines
   
   clip.BlockingMobj = nullptr; // haleyjd 1/17/00: global hit reference
   
   xl = (clip.bbox[BOXLEFT]   - bmaporgx) >> MAPBLOCKSHIFT;
   xh = (clip.bbox[BOXRIGHT]  - bmaporgx) >> MAPBLOCKSHIFT;
   yl = (clip.bbox[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
   yh = (clip.bbox[BOXTOP]    - bmaporgy) >> MAPBLOCKSHIFT;

   // From dsda-doom, originally fixed by kraflab
   // heretic - this must be incremented before iterating over the lines
   if(!vanilla_heretic)
      validcount++;

   for(bx = xl; bx <= xh; bx++)
   {
      for(by = yl; by <= yh; by++)
      {
         if(!P_BlockLinesIterator(bx, by, PIT_CheckLine, R_NOGROUP, pushhit))
            return false; // doesn't fit
      }
   }

   return true;
}

//
// P_CheckDropOffVanilla
//
// haleyjd 04/15/2010: Dropoff check for vanilla DOOM compatibility
//
static bool P_CheckDropOffVanilla(Mobj *thing, int dropoff)
{
   if(!(thing->flags & (MF_DROPOFF|MF_FLOAT)) &&
      clip.zref.floor - clip.zref.dropoff > STEPSIZE)
      return false; // don't stand over a dropoff

   return true;
}

//
// P_CheckDropOffBOOM
//
// haleyjd 04/15/2010: Dropoff check for BOOM compatibility
//
static bool P_CheckDropOffBOOM(Mobj *thing, int dropoff)
{
   // killough 3/15/98: Allow certain objects to drop off
   if(compatibility || !dropoff)
   {
      if(!(thing->flags & (MF_DROPOFF|MF_FLOAT)) &&
         clip.zref.floor - clip.zref.dropoff > STEPSIZE)
         return false; // don't stand over a dropoff
   }

   return true;
}

//
// P_CheckDropOffMBF
//
// haleyjd 04/15/2010: Dropoff check for MBF compatibility
//
// I am of the opinion that this is the single most-butchered segment of code
// in the entire engine.
//
static bool P_CheckDropOffMBF(Mobj *thing, int dropoff)
{
   // killough 3/15/98: Allow certain objects to drop off
   // killough 7/24/98, 8/1/98: 
   // Prevent monsters from getting stuck hanging off ledges
   // killough 10/98: Allow dropoffs in controlled circumstances
   // killough 11/98: Improve symmetry of clipping on stairs

   if(!(thing->flags & (MF_DROPOFF|MF_FLOAT)))
   {
      if(getComp(comp_dropoff))
      {
         // haleyjd: note missing 202 compatibility... WOOPS!
         if(clip.zref.floor - clip.zref.dropoff > STEPSIZE)
            return false;
      }
      else if(!dropoff || (dropoff == 2 &&
                           (clip.zref.floor - clip.zref.dropoff > 128 * FRACUNIT ||
                            !thing->target || thing->target->z > clip.zref.dropoff)))
      {
         // haleyjd: I can't even mentally parse this statement with 
         // any certainty.
         if(!monkeys || demo_version < 203 ?
            clip.zref.floor - clip.zref.dropoff > STEPSIZE :
            thing->zref.floor - clip.zref.floor > STEPSIZE ||
            thing->zref.dropoff - clip.zref.dropoff > STEPSIZE)
            return false;
      }
      else
      {
         clip.felldown = !(thing->flags & MF_NOGRAVITY) && 
                         thing->z - clip.zref.floor > STEPSIZE;
      }
   }

   return true;
}

static bool on3dmidtex;

//
// P_CheckDropOffEE
//
// haleyjd 04/15/2010: Dropoff checking code for Eternity.
//
static bool P_CheckDropOffEE(Mobj *thing, int dropoff)
{
   fixed_t floorz = clip.zref.floor;

   // haleyjd: OVER_UNDER:
   // [RH] If the thing is standing on something, use its current z as 
   // the zref.floor. This is so that it does not walk off of things onto a
   // drop off.
   if(P_Use3DClipping() && thing->intflags & MIF_ONMOBJ)
      floorz = thing->z > clip.zref.floor ? thing->z : clip.zref.floor;

   if(!(thing->flags & (MF_DROPOFF|MF_FLOAT)))
   {
      // haleyjd 11/10/04: 3dMidTex: you are never allowed to pass 
      // over a tall dropoff when on 3DMidTex lines. Note tmfloorz
      // considers 3DMidTex, so you can still move across 3DMidTex
      // lines that pass over sector dropoffs, as long as the dropoff
      // between the 3DMidTex lines is <= 24 units.

      if(on3dmidtex)
      {
         // allow appropriate forced dropoff behavior
         if(!dropoff || (dropoff == 2 && 
                         (thing->z - clip.zref.floor > 128*FRACUNIT ||
                          !thing->target || thing->target->z > clip.zref.floor)))
         {
            // deny any move resulting in a difference > 24
            if(thing->z - clip.zref.floor > STEPSIZE)
               return false;
         }
         else  // dropoff allowed
         {
            clip.felldown = !(thing->flags & MF_NOGRAVITY) &&
                           thing->z - clip.zref.floor > STEPSIZE;
         }
         
         return true;
      }

      if(getComp(comp_dropoff))
      {
         if(clip.zref.floor - clip.zref.dropoff > STEPSIZE)
            return false; // don't stand over a dropoff
      }
      else if(!dropoff || 
              (dropoff == 2 &&  // large jump down (e.g. dogs)
               (floorz - clip.zref.dropoff > 128*FRACUNIT ||
                !thing->target || thing->target->z > clip.zref.dropoff)))
      {
         // haleyjd 04/14/10: This is so impossible to read that I have
         // had to restore it, because I cannot be confident that any of 
         // my interpretations of it are correct relative to C grammar.

         if(!monkeys || demo_version < 203 ?
            floorz - clip.zref.dropoff > STEPSIZE :
            thing->zref.floor  - floorz > STEPSIZE ||
            thing->zref.dropoff - clip.zref.dropoff > STEPSIZE)
            return false;
      }
      else  // dropoff allowed -- check for whether it fell more than 24
      {
         clip.felldown = !(thing->flags & MF_NOGRAVITY) &&
                        thing->z - floorz > STEPSIZE;
      }
   }

   return true;
}

typedef bool (*dropoff_func_t)(Mobj *, int);

//
// Runs the spechit push specials
//
static void P_RunPushSpechits(Mobj &thing, PODCollection<line_t *> &pushhit)
{
   if(full_demo_version < make_full_version(401, 0) || thing.flags & (MF_TELEPORT | MF_NOCLIP))
      return;
   bool stacktop = true;
   while(!pushhit.isEmpty())
   {
      line_t &line = *pushhit.pop();
      const linkoffset_t *link = P_GetLinkOffset(thing.groupid, line.frontsector->groupid);

      // Emulate vanilla Hexen not caring about impassable line side. Must also
      // be the top of the stack.
      int side;
      if(P_LevelIsVanillaHexen() &&
         stacktop && (!line.backsector || (line.flags & ML_BLOCKING &&
                                           !(thing.flags & MF_MISSILE))))
      {
         side = 0;
      }
      else
         side = P_PointOnLineSide(thing.x + link->x, thing.y + link->y, &line);
      P_PushSpecialLine(thing, line, side);
      stacktop = false;
   }
}

//
// Checks if a thing can carry upper things up from an intended zref.floor. Assumed
// zref.floor > thing.z.
//
static bool P_checkCarryUp(Mobj &thing, fixed_t floorz)
{
   if(!(thing.flags4 & MF4_STICKYCARRY))
      return false;
   fixed_t orgz = thing.z;
   thing.z = floorz;
   MobjCollection coll;
   PODCollection<fixed_t> orgzcoll;
   doom_mapinter_t clip;
   P_FindAboveIntersectors(&thing, clip, coll); // already aware of MF_SOLID
   auto resetcoll = [&coll, orgz, &orgzcoll, &thing](const Mobj *other) {
      size_t i = 0;
      for(Mobj **previous = coll.begin(); *previous != other; ++previous)
         (*previous)->z = orgzcoll[i++];
      thing.z = orgz;
   };
   static Mobj *dummy;
   for(Mobj *other : coll)
   {
      if(!other->player)   // already collided with a non-player? Fail.
      {
         resetcoll(other);
         return false;
      }
      orgzcoll.add(other->z);
      other->z = thing.z + thing.height; // move it on top
      P_ZMovementTest(other); // it may bob back down due to ceiling
      if(!P_TestMobjZ(other, clip, &dummy))  // if it gets stuck, fail.
      {
         other->z = orgzcoll.pop();
         resetcoll(other);
         return false;
      }
      other->z = thing.z + thing.height;   // remake position after the test
   }
   thing.z = orgz;
   // Success? Check if any of these is a player
   fixed_t *orgzit = orgzcoll.begin();
   for(Mobj *other : coll)
   {
      if(other->player && other->player->mo == other)
      {
         other->player->viewheight += *orgzit - other->z;
         other->player->deltaviewheight =
         (other->player->pclass->viewheight - other->player->viewheight) >> 3;
      }
      ++orgzit;
   }
   return true;
}

//
// P_TryMove
//
// Attempt to move to a new position,
// crossing special lines unless MF_TELEPORT is set.
//
// killough 3/15/98: allow dropoff as option
//
bool P_TryMove(Mobj *thing, fixed_t x, fixed_t y, int dropoff)
{
   fixed_t oldx, oldy, oldz;
   int oldgroupid;
   dropoff_func_t dropofffunc;
   
   // haleyjd 11/10/04: 3dMidTex: determine if a thing is on a line:
   // zref.passfloor is the floor as determined from sectors and 3DMidTex.
   // zref.secfloor is the floor as determined only from sectors.
   // If the two are unequal, zref.passfloor is the thing's zref.floor, and
   // the thing is at its zref.floor, then it is on a 3DMidTex line.
   // Complicated. x_x

   on3dmidtex = (thing->zref.passfloor == thing->zref.floor &&
                 thing->zref.passfloor != thing->zref.secfloor &&
                 thing->z == thing->zref.floor);
   
   clip.felldown = clip.floatok = false;               // killough 11/98

   bool groupidchange = false;
   fixed_t prex = x, prey = y;

   PODCollection<line_t *> pushhit;
   PODCollection<line_t *> *pPushHit = full_demo_version >= make_full_version(401, 0) ? &pushhit : 
      nullptr;

   edefstructvar(portalcrossingoutcome_t, crossoutcome);
   crossoutcome.finalgroup = thing->groupid;

   // haleyjd: OVER_UNDER
   if(P_Use3DClipping())
   {
      oldx = thing->x;
      oldy = thing->y;
      oldz = thing->z;

      int ox = (emin(oldx, x) - bmaporgx) >> MAPBLOCKSHIFT;
      int oy = (emin(oldy, y) - bmaporgy) >> MAPBLOCKSHIFT;
      int tx = (emax(oldx, x) - bmaporgx) >> MAPBLOCKSHIFT;
      int ty = (emax(oldy, y) - bmaporgy) >> MAPBLOCKSHIFT;
   
      bool hasportals = false;
      if(gMapHasLinePortals && !(thing->flags & (MF_TELEPORT | MF_NOCLIP)) &&
         full_demo_version >= make_full_version(340, 48))
      {
         for(int bx = ox; bx <= tx; bx++)
         {
            if(bx < 0 || bx >= bmapwidth)
               continue;
            for(int by = oy; by <= ty; by++)
            {
               if(by < 0 || by >= bmapheight)
                  continue;
               if(P_BlockHasLinkedPortals(by * bmapwidth + bx, false))
               {
                  hasportals = true;
                  goto outloop;
               }
            }
         }
      outloop:
         ;
      }
      if(hasportals)
      {
         v2fixed_t dest = P_PrecisePortalCrossing(oldx, oldy, x - oldx, y - oldy, crossoutcome);
   
         // Update position
         groupidchange = crossoutcome.finalgroup != thing->groupid;
         prex = x;
         prey = y;

         x = dest.x;
         y = dest.y;
      }

      bool check = false;
      if(groupidchange)
      {
         oldgroupid = thing->groupid;
         thing->groupid = crossoutcome.finalgroup;
         check = P_CheckPosition3D(thing, x, y, pPushHit);
         thing->groupid = oldgroupid;
      }

      if((groupidchange && !check) || (!groupidchange && !P_CheckPosition3D(thing, x, y, pPushHit)))
      {
         // Solid wall or thing
         if(!clip.BlockingMobj || clip.BlockingMobj->player || !thing->player)
         {
            P_RunPushSpechits(*thing, pushhit);
            return false;
         }
         else
         {
            // haleyjd: yikes...
            // ioanch 20160111: updated for portals
            fixed_t steplimit;
            if((clip.BlockingMobj->flags & MF_CORPSE && LevelInfo.levelType != LI_TYPE_HEXEN) ||
               clip.BlockingMobj->flags4 & MF4_UNSTEPPABLE)
            {
               steplimit = 0;
            }
            else
               steplimit = STEPSIZE;

            if(clip.BlockingMobj->z + clip.BlockingMobj->height - thing->z > steplimit ||
               (P_ExtremeSectorAtPoint(clip.BlockingMobj, surf_ceil)->srf.ceiling.height
                 - (clip.BlockingMobj->z + clip.BlockingMobj->height) < thing->height) ||
               (clip.zref.ceiling - (clip.BlockingMobj->z + clip.BlockingMobj->height)
                 < thing->height))
            {
               P_RunPushSpechits(*thing, pushhit);
               return false;
            }

            // haleyjd: hack for touchies: don't allow running through them when
            // they die until they become non-solid (just being a corpse isn't
            // good enough)
            if(clip.BlockingMobj->flags & MF_TOUCHY)
            {
               if(clip.BlockingMobj->health <= 0)
               {
                  P_RunPushSpechits(*thing, pushhit);
                  return false;
               }
            }
         }
         if(!(clip.thing->flags3 & MF3_PASSMOBJ))
         {
            thing->z = oldz;
            P_RunPushSpechits(*thing, pushhit);
            return false;
         }
      }
   }
   else if(!P_CheckPosition(thing, x, y, pPushHit))
   {
      P_RunPushSpechits(*thing, pushhit);
      return false;   // solid wall or thing
   }

   if(!(thing->flags & MF_NOCLIP))
   {
      bool ret = clip.unstuck 
                    && !(clip.ceilingline && untouched(clip.ceilingline))
                    && !(  clip.floorline && untouched(  clip.floorline));

      // killough 7/26/98: reformatted slightly
      // killough 8/1/98: Possibly allow escape if otherwise stuck
      // haleyjd: OVER_UNDER: broke up impossible-to-understand predicate

      if(clip.zref.ceiling - clip.zref.floor < thing->height) // doesn't fit
      {
         if(!ret)
            P_RunPushSpechits(*thing, pushhit);
         return ret;
      }
         
      // mobj must lower to fit
      clip.floatok = true;
      if(!(thing->flags & MF_TELEPORT) && !(thing->flags4 & MF4_FLY) &&
         clip.zref.ceiling - thing->z < thing->height)
      {
         if(!ret)
            P_RunPushSpechits(*thing, pushhit);
         return ret;
      }

      // haleyjd 06/05/12: flying players - move up or down the lower/upper areas
      // of lines that are contacted when the player presses into them
      if(thing->flags4 & MF4_FLY)
      {
         if(thing->z + thing->height > clip.zref.ceiling)
         {
            thing->momz = -8*FRACUNIT;
            thing->intflags |= MIF_CLEARMOMZ;
            P_RunPushSpechits(*thing, pushhit);
            return false;
         }
         else if(thing->z < clip.zref.floor &&
                 clip.zref.floor - clip.zref.dropoff > STEPSIZE) // TODO: dropoff max
         {
            thing->momz = 8*FRACUNIT;
            thing->intflags |= MIF_CLEARMOMZ;
            P_RunPushSpechits(*thing, pushhit);
            return false;
         }
      }

      if(!(thing->flags & MF_TELEPORT) && !(thing->flags3 & MF3_FLOORMISSILE))
      {
         // too big a step up
         if(clip.zref.floor - thing->z > STEPSIZE)
         {
            if(!ret)
               P_RunPushSpechits(*thing, pushhit);
            return ret;
         }
         else if(P_Use3DClipping() && thing->z < clip.zref.floor)
         { 
            // TODO: make sure to add projectile impact checking if MISSILE
            // haleyjd: OVER_UNDER:
            // [RH] Check to make sure there's nothing in the way for the step up
            fixed_t savedz = thing->z;
            bool good;
            thing->z = clip.zref.floor;
            good = vanilla_heretic || P_TestMobjZ(thing, clip);
            thing->z = savedz;
            if(!good && !P_checkCarryUp(*thing, clip.zref.floor))
            {
               P_RunPushSpechits(*thing, pushhit);
               return false;
            }
         }
      }
      
      // haleyjd: dropoff handling moved to functions above for my sanity.
      // May I NEVER have to do something like this, ever again.
      if(demo_version < 200)                 // DOOM 1.9
         dropofffunc = P_CheckDropOffVanilla;
      else if(demo_version <= 202)           // BOOM
         dropofffunc = P_CheckDropOffBOOM;
      else if(demo_version == 203)           // MBF
         dropofffunc = P_CheckDropOffMBF;
      else                                   // EE
         dropofffunc = P_CheckDropOffEE;

      // ioanch: no P_RunPushSpechits on dropoff blocking.
      if(!dropofffunc(thing, dropoff))
         return false; // don't stand over a dropoff

      if(thing->flags & MF_BOUNCES &&    // killough 8/13/98
         !(thing->flags & (MF_MISSILE|MF_NOGRAVITY)) &&
         !sentient(thing) && clip.zref.floor - thing->z > 16*FRACUNIT)
      {
         P_RunPushSpechits(*thing, pushhit);
         return false; // too big a step up for bouncers under gravity
      }

      // killough 11/98: prevent falling objects from going up too many steps
      if(thing->intflags & MIF_FALLING && clip.zref.floor - thing->z >
         FixedMul(thing->momx,thing->momx)+FixedMul(thing->momy,thing->momy))
      {
         // ioanch: don't push in this case, as the force is presumably too low.
         return false;
      }

      // haleyjd: CANTLEAVEFLOORPIC flag
      // ioanch 20160114: use bottom sector floorpic
      if((thing->flags2 & MF2_CANTLEAVEFLOORPIC) &&
         (clip.open.floorpic != P_ExtremeSectorAtPoint(thing, surf_floor)->srf.floor.pic ||
          clip.zref.floor - thing->z != 0))
      {
         // thing must stay within its current floor type
         // ioanch: don't push
         return false;
      }
   }

   // the move is ok,
   // so unlink from the old position and link into the new position

   P_UnsetThingPosition (thing);
   
   // Check portal teleportation
   oldx = thing->x;
   oldy = thing->y;
   oldgroupid = thing->groupid;
   thing->zref = clip.zref;   // killough 11/98: keep track of dropoffs
   thing->x = x;
   thing->y = y;
   P_SetThingPosition(thing);

   if(crossoutcome.lastpassed)
   {
      // Passed through at least one portal
      // TODO: 3D teleport
      if(crossoutcome.multipassed)
      {
         thing->backupPosition();   // don't bother interpolating if passing through multiple
                                    // portals at once
      }
      else
      {
         thing->prevpos.portalline = crossoutcome.lastpassed;
         thing->prevpos.ldata = &crossoutcome.lastpassed->portal->data.link;
      }
      P_PortalDidTeleport(thing, x - prex, y - prey, 0, oldgroupid, crossoutcome.finalgroup);
   }

   // haleyjd 08/07/04: new footclip system
   P_AdjustFloorClip(thing);

   // if any special lines were hit, do the effect
   // killough 11/98: simplified
   if(!(thing->flags & (MF_TELEPORT | MF_NOCLIP)))
   {
      fixed_t tx, ty, ox, oy;   // portal aware offsetting
      const linkoffset_t *link;
      const linkoffset_t *oldlink;

      while(clip.numspechit--)
      {
// PTODO
         // ioanch 20160113: no longer use portals unless demo version is low
         line_t *line = clip.spechit[clip.numspechit];
         if(!line)   // skip if it's nulled out
            continue;

         if(line->special)  // see if the line was crossed
         {
            link = P_GetLinkOffset(thing->groupid, line->frontsector->groupid);
            oldlink = thing->groupid == oldgroupid ? link
            : P_GetLinkOffset(oldgroupid, line->frontsector->groupid);

            tx = thing->x + link->x;
            ty = thing->y + link->y;
            ox = oldx + oldlink->x;
            oy = oldy + oldlink->y;

            int oldside;
            if((oldside = P_PointOnLineSide(ox, oy, line)) !=
               P_PointOnLineSide(tx, ty, line))
               P_CrossSpecialLine(line, oldside, thing, nullptr);
         }
      }

      // haleyjd 01/09/07: do not leave numspechit == -1
      clip.numspechit = 0;
   }

   return true;
}

//
// PIT_ApplyTorque
//
// killough 9/12/98:
//
// Apply "torque" to objects hanging off of ledges, so that they
// fall off. It's not really torque, since Doom has no concept of
// rotation, but it's a convincing effect which avoids anomalies
// such as lifeless objects hanging more than halfway off of ledges,
// and allows objects to roll off of the edges of moving lifts, or
// to slide up and then back down stairs, or to fall into a ditch.
// If more than one linedef is contacted, the effects are cumulative,
// so balancing is possible.
//
static bool PIT_ApplyTorque(line_t *ld, polyobj_t *po, void *context)
{
   // ioanch 20160116: portal aware
   const linkoffset_t *link = P_GetLinkOffset(clip.thing->groupid,
      ld->frontsector->groupid);
   fixed_t bbox[4];
   bbox[BOXRIGHT] = clip.bbox[BOXRIGHT] + link->x;
   bbox[BOXLEFT] = clip.bbox[BOXLEFT] + link->x;
   bbox[BOXTOP] = clip.bbox[BOXTOP] + link->y;
   bbox[BOXBOTTOM] = clip.bbox[BOXBOTTOM] + link->y;

   if(ld->backsector &&       // If thing touches two-sided pivot linedef
      bbox[BOXRIGHT]  > ld->bbox[BOXLEFT]  &&
      bbox[BOXLEFT]   < ld->bbox[BOXRIGHT] &&
      bbox[BOXTOP]    > ld->bbox[BOXBOTTOM] &&
      bbox[BOXBOTTOM] < ld->bbox[BOXTOP] &&
      P_BoxOnLineSide(bbox, ld) == -1)
   {
      Mobj *mo = clip.thing;

      fixed_t mox = mo->x + link->x;
      fixed_t moy = mo->y + link->y;

      fixed_t dist =                               // lever arm
         + (ld->dx >> FRACBITS) * (moy >> FRACBITS)
         - (ld->dy >> FRACBITS) * (mox >> FRACBITS) 
         - (ld->dx >> FRACBITS) * (ld->v1->y >> FRACBITS)
         + (ld->dy >> FRACBITS) * (ld->v1->x >> FRACBITS);

      // ioanch: portal aware. Use two different behaviours depending on map
      bool cond;
      if(!useportalgroups || full_demo_version < make_full_version(340, 48))
      {
         cond = dist < 0 ?                               // dropoff direction
         ld->frontsector->srf.floor.height < mo->z &&
         ld->backsector->srf.floor.height >= mo->z :
         ld->backsector->srf.floor.height < mo->z &&
         ld->frontsector->srf.floor.height >= mo->z;
      }
      else
      {
         // with portals and advanced version, also allow equal floor heights
         // if one side has portals. Require equal floor height though
         cond = dist < 0 ?                               // dropoff direction
         (ld->frontsector->srf.floor.height < mo->z ||
         (ld->frontsector->srf.floor.height == mo->z &&
         ld->frontsector->srf.floor.pflags & PS_PASSABLE)) &&
         ld->backsector->srf.floor.height == mo->z :
         (ld->backsector->srf.floor.height < mo->z ||
         (ld->backsector->srf.floor.height == mo->z &&
         ld->backsector->srf.floor.pflags & PS_PASSABLE)) &&
         ld->frontsector->srf.floor.height == mo->z;
      }

      if(cond)
      {
         // At this point, we know that the object straddles a two-sided
         // linedef, and that the object's center of mass is above-ground.

         fixed_t x = D_abs(ld->dx), y = D_abs(ld->dy);

         if(y > x)
         {
            fixed_t t = x;
            x = y;
            y = t;
         }

         y = finesine[(tantoangle[FixedDiv(y,x)>>DBITS] +
                      ANG90) >> ANGLETOFINESHIFT];

         // Momentum is proportional to distance between the
         // object's center of mass and the pivot linedef.
         //
         // It is scaled by 2^(OVERDRIVE - gear). When gear is
         // increased, the momentum gradually decreases to 0 for
         // the same amount of pseudotorque, so that oscillations
         // are prevented, yet it has a chance to reach equilibrium.

         dist = FixedDiv(FixedMul(dist, (mo->gear < OVERDRIVE) ?
                 y << -(mo->gear - OVERDRIVE) :
                 y >> +(mo->gear - OVERDRIVE)), x);

         // Apply momentum away from the pivot linedef.
                 
         x = FixedMul(ld->dy, dist);
         y = FixedMul(ld->dx, dist);

         // Avoid moving too fast all of a sudden (step into "overdrive")

         dist = FixedMul(x,x) + FixedMul(y,y);

         while(dist > FRACUNIT*4 && mo->gear < MAXGEAR)
         {
            ++mo->gear;
            x >>= 1;
            y >>= 1;
            dist >>= 1;
         }
         
         mo->momx -= x;
         mo->momy += y;
      }
   }
   return true;
}

//
// P_ApplyTorque
//
// killough 9/12/98
// Applies "torque" to objects, based on all contacted linedefs
//
void P_ApplyTorque(Mobj *mo)
{
   // ioanch 20160116: portal aware
   clip.bbox[BOXLEFT] = mo->x - mo->radius;
   clip.bbox[BOXRIGHT] = mo->x + mo->radius;
   clip.bbox[BOXBOTTOM] = mo->y - mo->radius;
   clip.bbox[BOXTOP] = mo->y + mo->radius;

   int flags = mo->intflags; //Remember the current state, for gear-change

   clip.thing = mo;
   validcount++; // prevents checking same line twice

   P_TransPortalBlockWalker(clip.bbox, mo->groupid, true, nullptr, 
      [](int x, int y, int groupid, void *data) -> bool
   {
      P_BlockLinesIterator(x, y, PIT_ApplyTorque, groupid);
      return true;
   });
      
   // If any momentum, mark object as 'falling' using engine-internal flags
   if (mo->momx | mo->momy)
      mo->intflags |= MIF_FALLING;
   else  // Clear the engine-internal flag indicating falling object.
      mo->intflags &= ~MIF_FALLING;

   // If the object has been moving, step up the gear.
   // This helps reach equilibrium and avoid oscillations.
   //
   // Doom has no concept of potential energy, much less
   // of rotation, so we have to creatively simulate these 
   // systems somehow :)

   if(!((mo->intflags | flags) & MIF_FALLING))  // If not falling for a while,
      mo->gear = 0;                             // Reset it to full strength
   else if(mo->gear < MAXGEAR)                  // Else if not at max gear,
      mo->gear++;                               // move up a gear
}

//
// P_ThingHeightClip
//
// Takes a valid thing and adjusts the thing->zref.floor,
// thing->zref.ceiling, and possibly thing->z.
// This is called for all nearby monsters
// whenever a sector changes height.
// If the thing doesn't fit,
// the z will be set to the lowest value
// and false will be returned.
//
static bool P_ThingHeightClip(Mobj *thing)
{
   bool onfloor = thing->z == thing->zref.floor;
   fixed_t oldfloorz = thing->zref.floor; // haleyjd

   P_CheckPosition(thing, thing->x, thing->y);
  
   // what about stranding a monster partially off an edge?
   // killough 11/98: Answer: see below (upset balance if hanging off ledge)

   thing->zref = clip.zref;   // killough 11/98: remember dropoffs

   // haleyjd 09/19/06: floatbobbers require special treatment here now
   if(thing->flags2 & MF2_FLOATBOB)
   {
      if(thing->zref.floor > oldfloorz || !(thing->flags & MF_NOGRAVITY))
         thing->z = thing->z - oldfloorz + thing->zref.floor;

      if(thing->z + thing->height > thing->zref.ceiling)
         thing->z = thing->zref.ceiling - thing->height;
   }
   else if(onfloor)  // walking monsters rise and fall with the floor
   {
      thing->z = thing->zref.floor;
      
      // killough 11/98: Possibly upset balance of objects hanging off ledges
      if(thing->intflags & MIF_FALLING && thing->gear >= MAXGEAR)
         thing->gear = 0;
   }
   else          // don't adjust a floating monster unless forced to
      if(thing->z + thing->height > thing->zref.ceiling)
         thing->z = thing->zref.ceiling - thing->height;

   return thing->zref.ceiling - thing->zref.floor >= thing->height;
}

//
// SLIDE MOVE
// Allows the player to slide along any angled walls.
//

// killough 8/2/98: make variables static
static fixed_t  bestslidefrac;
static fixed_t  secondslidefrac;
static line_t  *bestslideline;
static line_t  *secondslideline;
static Mobj    *slidemo;
static fixed_t  tmxmove;
static fixed_t  tmymove;

//
// P_HitSlideLine
// Adjusts the xmove / ymove
// so that the next move will slide along the wall.
//
// If the floor is icy, then you can bounce off a wall.             // phares
//
static void P_HitSlideLine(line_t *ld)
{
   int     side;
   angle_t lineangle;
   angle_t moveangle;
   angle_t deltaangle;
   fixed_t movelen;
   bool icyfloor;  // is floor icy?

   // phares:
   // Under icy conditions, if the angle of approach to the wall
   // is more than 45 degrees, then you'll bounce and lose half
   // your momentum. If less than 45 degrees, you'll slide along
   // the wall. 45 is arbitrary and is believable.
   //
   // Check for the special cases of horz or vert walls.

   // haleyjd 04/11/10: BOOM friction compatibility fixes
   if(demo_version >= 203)
   {
      // killough 10/98: only bounce if hit hard (prevents wobbling)
      icyfloor =
         P_AproxDistance(tmxmove, tmymove) > 4*FRACUNIT &&
         variable_friction &&  // killough 8/28/98: calc friction on demand
         slidemo->z <= slidemo->zref.floor &&
         !(slidemo->flags4 & MF4_FLY) && // haleyjd: not when just flying
         P_GetFriction(slidemo, nullptr) > ORIG_FRICTION;
   }
   else
   {
      extern bool onground;
      icyfloor = !compatibility &&
         variable_friction &&
         slidemo->player &&
         onground && 
         slidemo->friction > ORIG_FRICTION;
   }

   if(ld->slopetype == ST_HORIZONTAL)
   {
      if(icyfloor && D_abs(tmymove) > D_abs(tmxmove))
      {
         // haleyjd: only the player should oof
         // 09/08/13: ... and only when he's alive.
         if(slidemo->player && slidemo->health > 0)
            S_StartSound(slidemo, GameModeInfo->playerSounds[sk_oof]); // oooff!
         tmxmove /= 2; // absorb half the momentum
         tmymove = -tmymove/2;
      }
      else
         tmymove = 0; // no more movement in the Y direction
      return;
   }

   if(ld->slopetype == ST_VERTICAL)
   {
      if(icyfloor && D_abs(tmxmove) > D_abs(tmymove))
      {
         // haleyjd: only the player should oof
         // 09/08/13: ... and again, only when alive.
         if(slidemo->player && slidemo->health > 0)
            S_StartSound(slidemo, GameModeInfo->playerSounds[sk_oof]); // oooff!
         tmxmove = -tmxmove/2; // absorb half the momentum
         tmymove /= 2;
      }
      else // phares
         tmxmove = 0; // no more movement in the X direction
      return;
   }

   // The wall is angled. Bounce if the angle of approach is  // phares
   // less than 45 degrees.

   side = P_PointOnLineSide (slidemo->x, slidemo->y, ld);
   
   lineangle = P_PointToAngle (0,0, ld->dx, ld->dy);
   if(side == 1)
      lineangle += ANG180;
   moveangle = P_PointToAngle (0,0, tmxmove, tmymove);

   // killough 3/2/98:
   // The moveangle+=10 breaks v1.9 demo compatibility in
   // some demos, so it needs demo_compatibility switch.
   
   if(!demo_compatibility)
      moveangle += 10;
   // ^ prevents sudden path reversal due to rounding error // phares

   deltaangle = moveangle-lineangle;
   movelen = P_AproxDistance (tmxmove, tmymove);

   if(icyfloor && deltaangle > ANG45 && deltaangle < ANG90+ANG45)
   {
      // haleyjd: only the player should oof
      // 09/08/13: ... only LIVING players
      if(slidemo->player && slidemo->health > 0)
         S_StartSound(slidemo, GameModeInfo->playerSounds[sk_oof]); // oooff!
      moveangle = lineangle - deltaangle;
      movelen /= 2; // absorb
      moveangle >>= ANGLETOFINESHIFT;
      tmxmove = FixedMul (movelen, finecosine[moveangle]);
      tmymove = FixedMul (movelen, finesine[moveangle]);
   }
   else
   {
      if(deltaangle > ANG180)
         deltaangle += ANG180;
      
      //  I_Error ("SlideLine: ang>ANG180");
      
      lineangle >>= ANGLETOFINESHIFT;
      deltaangle >>= ANGLETOFINESHIFT;
      fixed_t newlen = FixedMul (movelen, finecosine[deltaangle]);
      tmxmove = FixedMul (newlen, finecosine[lineangle]);
      tmymove = FixedMul (newlen, finesine[lineangle]);
   }
}

//
// PTR_SlideTraverse
//
static bool PTR_SlideTraverse(intercept_t *in, void *context)
{
   line_t *li;
   
#ifdef RANGECHECK
   if(!in->isaline)
      I_Error("PTR_SlideTraverse: not a line?\n");
#endif

   li = in->d.line;
   
   if(!(li->flags & ML_TWOSIDED))
   {
      if(P_PointOnLineSide (slidemo->x, slidemo->y, li))
         return true; // don't hit the back side
      goto isblocking;
   }

   // haleyjd 04/30/11: 'Block everything' lines block sliding
   if(li->extflags & EX_ML_BLOCKALL)
      goto isblocking;

   // set openrange, opentop, openbottom.
   // These define a 'window' from one sector to another across a line
   
   clip.open = P_LineOpening(li, slidemo);
   
   if(clip.open.range < slidemo->height)
      goto isblocking;  // doesn't fit
   
   if(clip.open.height.ceiling - slidemo->z < slidemo->height)
      goto isblocking;  // mobj is too high
   
   if(clip.open.height.floor - slidemo->z > STEPSIZE )
      goto isblocking;  // too big a step up
   else if(P_Use3DClipping() &&
           slidemo->z < clip.open.height.floor) // haleyjd: OVER_UNDER
   { 
      // [RH] Check to make sure there's nothing in the way for the step up
      bool good;
      fixed_t savedz = slidemo->z;
      slidemo->z = clip.open.height.floor;
      good = P_TestMobjZ(slidemo, clip);
      slidemo->z = savedz;
      if(!good)
         goto isblocking;
   }
   
   // this line doesn't block movement
   
   return true;
   
   // the line does block movement,
   // see if it is closer than best so far

isblocking:
   
   if(in->frac < bestslidefrac)
   {
      secondslidefrac = bestslidefrac;
      secondslideline = bestslideline;
      bestslidefrac = in->frac;
      bestslideline = li;
   }
   
   return false; // stop
}

//
// P_SlideMove
//
// The momx / momy move is bad, so try to slide along a wall.
// Find the first line hit, move flush to it, and slide along it.
//
// This is a kludgy mess.
//
// killough 11/98: reformatted
//
void P_SlideMove(Mobj *mo)
{
   int hitcount = 3;
   
   slidemo = mo; // the object that's sliding
   
   do
   {
      fixed_t leadx, leady, trailx, traily;

      if(!--hitcount)
         goto stairstep;   // don't loop forever

      // trace along the three leading corners
      
      if(mo->momx > 0)
      {
         leadx = mo->x + mo->radius;
         trailx = mo->x - mo->radius;
      }
      else
      {
         leadx = mo->x - mo->radius;
         trailx = mo->x + mo->radius;
      }

      if(mo->momy > 0)
      {
         leady = mo->y + mo->radius;
         traily = mo->y - mo->radius;
      }
      else
      {
         leady = mo->y - mo->radius;
         traily = mo->y + mo->radius;
      }

      bestslidefrac = FRACUNIT+1;
      
      P_PathTraverse(leadx, leady, leadx+mo->momx, leady+mo->momy,
                     PT_ADDLINES, PTR_SlideTraverse);
      P_PathTraverse(trailx, leady, trailx+mo->momx, leady+mo->momy,
                     PT_ADDLINES, PTR_SlideTraverse);
      P_PathTraverse(leadx, traily, leadx+mo->momx, traily+mo->momy,
                     PT_ADDLINES, PTR_SlideTraverse);

      // move up to the wall

      if(bestslidefrac == FRACUNIT+1)
      {
         // the move must have hit the middle, so stairstep
         
         stairstep:
      
         // killough 3/15/98: Allow objects to drop off ledges
         //
         // phares 5/4/98: kill momentum if you can't move at all
         // This eliminates player bobbing if pressed against a wall
         // while on ice.
         //
         // killough 10/98: keep buggy code around for old Boom demos
         
         // haleyjd: yet another compatibility fix by cph -- the
         // fix is only necessary for boom v2.01

         if(!P_TryMove(mo, mo->x, mo->y + mo->momy, true))
            if(!P_TryMove(mo, mo->x + mo->momx, mo->y, true))	      
               if(demo_version == 201)
                  mo->momx = mo->momy = 0;
               
         break;
      }

      // fudge a bit to make sure it doesn't hit
      
      if ((bestslidefrac -= 0x800) > 0)
      {
         fixed_t newx = FixedMul(mo->momx, bestslidefrac);
         fixed_t newy = FixedMul(mo->momy, bestslidefrac);
         
         // killough 3/15/98: Allow objects to drop off ledges
         
         if(!P_TryMove(mo, mo->x+newx, mo->y+newy, true))
            goto stairstep;
      }

      // Now continue along the wall.
      // First calculate remainder.
      
      bestslidefrac = FRACUNIT-(bestslidefrac+0x800);
      
      if(bestslidefrac > FRACUNIT)
         bestslidefrac = FRACUNIT;
      
      if(bestslidefrac <= 0)
         break;

      tmxmove = FixedMul(mo->momx, bestslidefrac);
      tmymove = FixedMul(mo->momy, bestslidefrac);
      
      P_HitSlideLine(bestslideline); // clip the moves
      
      mo->momx = tmxmove;
      mo->momy = tmymove;
      
      // killough 10/98: affect the bobbing the same way (but not voodoo dolls)
      if(mo->player && mo->player->mo == mo)
      {
         if(D_abs(mo->player->momx) > D_abs(tmxmove))
            mo->player->momx = tmxmove;
         if(D_abs(mo->player->momy) > D_abs(tmymove))
            mo->player->momy = tmymove;
      }
   }  // killough 3/15/98: Allow objects to drop off ledges:
   while(!P_TryMove(mo, mo->x+tmxmove, mo->y+tmymove, true));
}

//
// RADIUS ATTACK
//

// haleyjd 09/23/09: repair to non-reentrancy and stack-fault issues in
// PIT_RadiusAttack - information is grouped into a structure, and outside
// of old demos, the following are true:
// * bombdata_t's will be pushed and popped for recursive explosions
// * a limit of 128 recursive explosions is enforced

struct bombdata_t
{
   Mobj *bombsource;
   Mobj *bombspot;
   int   bombdamage;
   int   bombdistance; // haleyjd 12/22/12
   int   bombmod;      // haleyjd 07/13/03

   unsigned int bombflags; // haleyjd 12/22/12
};

#define MAXBOMBS 128               // a static limit to prevent stack faults.
static int bombindex;              // current index into bombs array
static bombdata_t bombs[MAXBOMBS]; // bombs away!
static bombdata_t *theBomb;        // it's the bomb, man. (the current explosion)

//
// Check if the actors are immune to each other's radius attack. This is a MBF21 feature.
//
inline static bool P_splashImmune(const Mobj *target, const Mobj *spot)
{
   return E_ThingPairValid(spot->type, target->type, TGF_NOSPLASHDAMAGE);
}

//
// PIT_RadiusAttack
//
// "bombsource" is the creature that caused the explosion at "bombspot".
//
static bool PIT_RadiusAttack(Mobj *thing, void *context)
{
   fixed_t dx, dy, dist;
   Mobj *bombspot     = theBomb->bombspot;
   Mobj *bombsource   = theBomb->bombsource;
   int   bombdistance = theBomb->bombdistance;
   int   bombdamage   = theBomb->bombdamage;
   
   // killough 8/20/98: allow bouncers to take damage 
   // (missile bouncers are already excluded with MF_NOBLOCKMAP)
   
   if(!(thing->flags & (MF_SHOOTABLE | MF_BOUNCES)))
      return true;

   if(bombspot && P_splashImmune(thing, bombspot))
      return true;

   // haleyjd 12/22/12: Hexen check for no self-damage
   if(theBomb->bombflags & RAF_NOSELFDAMAGE && thing == bombsource)
      return true;
   
   // Boss spider and cyborg
   // take no damage from concussion.
   
   // killough 8/10/98: allow grenades to hurt anyone, unless
   // fired by Cyberdemons, in which case it won't hurt Cybers.   
   // haleyjd 12/22/12: if bouncer has NORADIUSHACK, don't do this check.

   if(bombspot->flags & MF_BOUNCES && !(bombspot->flags4 & MF4_NORADIUSHACK))
   {
      int cyberType = E_ThingNumForDEHNum(MT_CYBORG);

      if(thing->type == cyberType && bombsource->type == cyberType)
         return true;
   }
   else if((thing->flags2 & MF2_BOSS || thing->flags4 & MF4_NORADIUSDMG) &&
           !(bombspot->flags4 & MF4_FORCERADIUSDMG))
   {      
      // haleyjd 05/22/99: exclude all bosses
      // haleyjd 09/21/09: support separate MF4_NORADIUSDMG flag and
      //                   force radius damage flag
      return true;
   }

   // ioanch 20151225: portal-aware behaviour
   dx   = D_abs(getThingX(bombspot, thing) - bombspot->x);
   dy   = D_abs(getThingY(bombspot, thing) - bombspot->y);
   dist = dx > dy ? dx : dy;
   dist = (dist - thing->radius) >> FRACBITS;

   if(dist < 0)
      dist = 0;

   if(dist >= bombdistance)
      return true;  // out of range

   // haleyjd: optional z check for Hexen-style explosions
   if(theBomb->bombflags & RAF_CLIPHEIGHT)
   {
      if((D_abs(getThingZ(bombspot, thing) - bombspot->z) / FRACUNIT) > 2 * bombdistance)
         return true;
   }

   if(P_CheckSight(thing, bombspot))      // must be in direct path
   {
      int damage;

      // haleyjd 12/22/12: support Hexen-compatible distance separate from damage
      if(bombdamage == bombdistance)
         damage = bombdamage - dist;
      else
         damage = (bombdamage * (bombdistance - dist) / bombdistance) + 1;
      
      P_DamageMobj(thing, bombspot, bombsource, damage, theBomb->bombmod);
   }
   
   return true;
}

//
// P_RadiusAttack
//
// Source is the creature that caused the explosion at spot.
//   haleyjd 07/13/03: added method of death flag
//   haleyjd 09/23/09: adjustments for reentrancy and recursion limit
//
void P_RadiusAttack(Mobj *spot, Mobj *source, int damage, int distance, 
                    int mod, unsigned int flags)
{
   fixed_t dist = (distance + MAXRADIUS) << FRACBITS;
  

   if(demo_version >= 335)
   {
      // woops! let's not stack-fault.
      if(bombindex >= MAXBOMBS)
      {
         doom_printf(FC_ERROR "P_RadiusAttack: too many bombs!");
         return;
      }

      // set bomb pointer and increment index
      theBomb = &bombs[bombindex++];
   }
   else
      theBomb = &bombs[0]; // otherwise, we use bomb 0 for everything :(

   // set up us the bomb!
   theBomb->bombspot     = spot;
   theBomb->bombsource   = source;
   theBomb->bombdamage   = damage;
   theBomb->bombdistance = distance;
   theBomb->bombmod      = mod;
   theBomb->bombflags    = flags;

   fixed_t bbox[4];
   bbox[BOXLEFT] = spot->x - dist;
   bbox[BOXTOP] = spot->y + dist;
   bbox[BOXRIGHT] = spot->x + dist;
   bbox[BOXBOTTOM] = spot->y - dist;

   // ioanch 20160107: walk through all portals
   P_TransPortalBlockWalker(bbox, spot->groupid, false, nullptr, 
      [](int x, int y, int groupid, void *data) -> bool
   {
      P_BlockThingsIterator(x, y, groupid, PIT_RadiusAttack);
      return true;
   });

   if(demo_version >= 335 && bombindex > 0)
      theBomb = &bombs[--bombindex];
}

//
// SECTOR HEIGHT CHANGING
// After modifying a sectors floor or ceiling height,
// call this routine to adjust the positions
// of all things that touch the sector.
//
// If anything doesn't fit anymore, true will be returned.
// If crunch is true, they will take damage
//  as they are being crushed.
// If Crunch is false, you should set the sector height back
//  the way it was and call P_ChangeSector again
//  to undo the changes.
//

//
// PIT_ChangeSector
//
static bool PIT_ChangeSector(Mobj *thing, void *context)
{
   if(P_ThingHeightClip(thing))
      return true; // keep checking
   
   // crunch bodies to giblets
   if(thing->health <= 0)
   {
      // sf: clear the skin which will mess things up
      // haleyjd 03/11/03: only in Doom
      if(GameModeInfo->type == Game_DOOM)
      {
         thing->skin = nullptr;
         P_SetMobjState(thing, E_SafeState(S_GIBS));
      }
      thing->flags &= ~MF_SOLID;
      thing->height = thing->radius = 0;
      return true;      // keep checking
   }

   // crunch dropped items
   if(thing->flags & MF_DROPPED)
   {
      thing->remove();
      return true;      // keep checking
   }

   // killough 11/98: kill touchy things immediately
   if(thing->flags & MF_TOUCHY &&
      (thing->intflags & MIF_ARMED || sentient(thing)))
   {
      // kill object
      P_DamageMobj(thing, nullptr, nullptr, thing->health, MOD_CRUSH);
      return true;   // keep checking
   }

   if(!(thing->flags & MF_SHOOTABLE))
      return true;        // assume it is bloody gibs or something

   nofit = 1;
   
   // haleyjd 06/19/00: fix for invulnerable things -- no crusher effects
   // haleyjd 05/20/05: allow custom crushing damage

   if(crushchange > 0 && !(leveltime & 3))
   {
      if(thing->flags2 & MF2_INVULNERABLE || thing->flags2 & MF2_DORMANT)
         return true;

      P_DamageMobj(thing, nullptr, nullptr, crushchange, MOD_CRUSH);
      
      // haleyjd 06/26/06: NOBLOOD objects shouldn't bleed when crushed
      // haleyjd FIXME: needs compflag
      if(demo_version < 333 || !(thing->flags & MF_NOBLOOD))
      {
         BloodSpawner(thing, crushchange).spawn(BLOOD_CRUSH);
      }
   }

   // keep checking (crush other things)
   return true;
}

//
// P_ChangeSector
//
// haleyjd: removed static; needed in p_floor.c
//
bool P_ChangeSector(sector_t *sector, int crunch)
{
   int x, y;
   
   nofit = 0;
   crushchange = crunch;
   
   // ARRGGHHH!!!!
   // This is horrendously slow!!!
   // killough 3/14/98
   
   // re-check heights for all things near the moving sector

   for(x=sector->blockbox[BOXLEFT] ; x<= sector->blockbox[BOXRIGHT] ; x++)
      for(y=sector->blockbox[BOXBOTTOM];y<= sector->blockbox[BOXTOP] ; y++)
         P_BlockThingsIterator(x, y, PIT_ChangeSector);
      
   return !!nofit;
}

//
// P_CheckSector
//
// jff 3/19/98 added to just check monsters on the periphery
// of a moving sector instead of all in bounding box of the
// sector. Both more accurate and faster.
//
// haleyjd: OVER_UNDER: pass down more information to P_ChangeSector3D
// when 3D object clipping is enabled.
//
bool P_CheckSector(sector_t *sector, int crunch, int amt, CheckSectorPlane plane)
{
   msecnode_t *n;
   
   // killough 10/98: sometimes use Doom's method
   if(getComp(comp_floors) && (demo_version >= 203 || demo_compatibility))
      return P_ChangeSector(sector, crunch);

   // haleyjd: call down to P_ChangeSector3D instead.
   if(P_Use3DClipping())
      return P_ChangeSector3D(sector, crunch, amt, plane);
   
   nofit = 0;
   crushchange = crunch;
   
   // killough 4/4/98: scan list front-to-back until empty or exhausted,
   // restarting from beginning after each thing is processed. Avoids
   // crashes, and is sure to examine all things in the sector, and only
   // the things which are in the sector, until a steady-state is reached.
   // Things can arbitrarily be inserted and removed and it won't mess up.
   //
   // killough 4/7/98: simplified to avoid using complicated counter
   
   // Mark all things invalid
   for(n = sector->touching_thinglist; n; n = n->m_snext)
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
         if(!n->visited)                     // unprocessed thing found
         {
            n->visited  = true;              // mark thing as processed
            if(!(n->m_thing->flags & MF_NOBLOCKMAP)) //jff 4/7/98 don't do these
               PIT_ChangeSector(n->m_thing, nullptr); // process it
            break;                           // exit and start over
         }
      }
   }
   while(n); // repeat from scratch until all things left are marked valid
   
   return !!nofit;
}

// phares 3/21/98
//
// Maintain a freelist of msecnode_t's to reduce memory allocs and frees.

msecnode_t *headsecnode = nullptr;

// sf: fix annoying crash on restarting levels
//
//      This crash occurred because the msecnode_t's are allocated as
//      PU_LEVEL. This meant that these were freed whenever a new level
//      was loaded or the level restarted. However, the actual list was
//      not emptied. As a result, msecnode_t's were being used that had
//      been freed back to the zone memory. I really do not understand
//      why boom or MBF never got this bug- or am I missing something?
//
//      additional comment 5/7/99
//      There _is_ code to free the list in g_game.c. But this is called
//      too late: some msecnode_t's are used during the loading of the
//      level. 
//
// haleyjd 05/11/09: This in fact was never a problem. Clear the pointer
// before or after Z_FreeTags; it doesn't matter as long as you clear it
// at some point in or before P_SetupLevel and before we start attaching
// Mobj's to sectors. I don't know what the hubbub above is about, but
// I almost copied this change into WinMBF unnecessarily. I will leave it
// be here, because as I said, changing it has no effect whatsoever.

//
// P_FreeSecNodeList
//
void P_FreeSecNodeList(void)
{
   headsecnode = nullptr; // this is all thats needed to fix the bug
}

//
// P_GetSecnode
//
// Retrieves a node from the freelist. The calling routine should make sure it
// sets all fields properly.
//
// killough 11/98: reformatted
//
static msecnode_t *P_GetSecnode(void)
{
   msecnode_t *node;

   return headsecnode ?
   node = headsecnode, headsecnode = node->m_snext, node :
      emalloctag(msecnode_t *, sizeof *node, PU_LEVEL, nullptr); 
}

//
// P_PutSecnode
//
// Returns a node to the freelist.
//
static void P_PutSecnode(msecnode_t *node)
{
   node->m_snext = headsecnode;
   headsecnode = node;
}

//
// P_AddSecnode
//
// phares 3/16/98
// Searches the current list to see if this sector is already there. If 
// not, it adds a sector node at the head of the list of sectors this 
// object appears in. This is called when creating a list of nodes that
// will get linked in later. Returns a pointer to the new node.
//
// killough 11/98: reformatted
//
static msecnode_t *P_AddSecnode(sector_t *s, Mobj *thing, msecnode_t *nextnode)
{
   msecnode_t *node;
   
   for(node = nextnode; node; node = node->m_tnext)
   {
      if(node->m_sector == s)   // Already have a node for this sector?
      {
         node->m_thing = thing; // Yes. Setting m_thing says 'keep it'.
         return nextnode;
      }
   }

   // Couldn't find an existing node for this sector. Add one at the head
   // of the list.
   
   node = P_GetSecnode();
   
   node->visited = 0;  // killough 4/4/98, 4/7/98: mark new nodes unvisited.

   node->m_sector = s;         // sector
   node->m_thing  = thing;     // mobj
   node->m_tprev  = nullptr;   // prev node on Thing thread
   node->m_tnext  = nextnode;  // next node on Thing thread
   
   if(nextnode)
      nextnode->m_tprev = node; // set back link on Thing

   // Add new node at head of sector thread starting at s->touching_thinglist
   
   node->m_sprev  = nullptr;    // prev node on sector thread
   node->m_snext  = s->touching_thinglist; // next node on sector thread
   if(s->touching_thinglist)
      node->m_snext->m_sprev = node;
   return s->touching_thinglist = node;
}

//
// P_DelSecnode
//
// Deletes a sector node from the list of sectors this object appears in.
// Returns a pointer to the next node on the linked list, or nullptr.
//
// killough 11/98: reformatted
//
static msecnode_t *P_DelSecnode(msecnode_t *node)
{
   if(node)
   {
      msecnode_t *tp = node->m_tprev;  // prev node on thing thread
      msecnode_t *tn = node->m_tnext;  // next node on thing thread
      msecnode_t *sp;  // prev node on sector thread
      msecnode_t *sn;  // next node on sector thread

      // Unlink from the Thing thread. The Thing thread begins at
      // sector_list and not from Mobj->touching_sectorlist.

      if(tp)
         tp->m_tnext = tn;
      
      if(tn)
         tn->m_tprev = tp;

      // Unlink from the sector thread. This thread begins at
      // sector_t->touching_thinglist.
      
      sp = node->m_sprev;
      sn = node->m_snext;
      
      if(sp)
         sp->m_snext = sn;
      else
         node->m_sector->touching_thinglist = sn;
      
      if(sn)
         sn->m_sprev = sp;

      // Return this node to the freelist
      
      P_PutSecnode(node);
      
      node = tn;
   }
   return node;
}

//
// P_DelSeclist
//
// Delete an entire sector list
//
void P_DelSeclist(msecnode_t *node)
{
   while(node)
      node = P_DelSecnode(node);
}

//
// PIT_GetSectors
//
// phares 3/14/98
// Locates all the sectors the object is in by looking at the lines that
// cross through it. You have already decided that the object is allowed
// at this location, so don't bother with checking impassable or
// blocking lines.
//
static bool PIT_GetSectors(line_t *ld, polyobj_t *po, void *context)
{
   // ioanch 20160115: portal aware
   fixed_t bbox[4];
   const linkoffset_t *link = P_GetLinkOffset(pClip->thing->groupid, 
      ld->frontsector->groupid);
   bbox[BOXRIGHT] = pClip->bbox[BOXRIGHT] + link->x;
   bbox[BOXLEFT] = pClip->bbox[BOXLEFT] + link->x;
   bbox[BOXTOP] = pClip->bbox[BOXTOP] + link->y;
   bbox[BOXBOTTOM] = pClip->bbox[BOXBOTTOM] + link->y;
   
   if(bbox[BOXRIGHT]  <= ld->bbox[BOXLEFT]   ||
      bbox[BOXLEFT]   >= ld->bbox[BOXRIGHT]  ||
      bbox[BOXTOP]    <= ld->bbox[BOXBOTTOM] ||
      bbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
      return true;

   if(P_BoxOnLineSide(bbox, ld) != -1)
      return true;

   // This line crosses through the object.
   
   // Collect the sector(s) from the line and add to the
   // sector_list you're examining. If the Thing ends up being
   // allowed to move to this position, then the sector_list
   // will be attached to the Thing's Mobj at touching_sectorlist.

   if(pClip->thing->groupid != ld->frontsector->groupid)
   {
      // similar behaviour to PIT_CheckLine3D
      v2fixed_t inters = P_BoxLinePoint(bbox, ld);
      v2fixed_t i2;

      angle_t angle = P_PointToAngle(ld->v1->x, ld->v1->y, ld->dx, ld->dy);
      angle -= ANG90;
      i2 = inters;
      i2.x += FixedMul(FRACUNIT >> 12, finecosine[angle >> ANGLETOFINESHIFT]);
      i2.y += FixedMul(FRACUNIT >> 12, finesine[angle >> ANGLETOFINESHIFT]);

      if(P_PointReachesGroupVertically(i2.x, i2.y, ld->frontsector->srf.floor.height,
                                       ld->frontsector->groupid,
                                       pClip->thing->groupid, ld->frontsector,
                                       pClip->thing->z))
      {
         pClip->sector_list = P_AddSecnode(ld->frontsector, pClip->thing,
                                           pClip->sector_list);
      }

      if(ld->backsector && ld->backsector != ld->frontsector)
      {
         angle += ANG180;
         i2 = inters;
         i2.x += FixedMul(FRACUNIT >> 12, finecosine[angle >> ANGLETOFINESHIFT]);
         i2.y += FixedMul(FRACUNIT >> 12, finesine[angle >> ANGLETOFINESHIFT]);
         if(P_PointReachesGroupVertically(i2.x, i2.y,
                                          ld->backsector->srf.floor.height,
                                          ld->backsector->groupid,
                                          pClip->thing->groupid, ld->backsector,
                                          pClip->thing->z))
         {
            pClip->sector_list = P_AddSecnode(ld->backsector, pClip->thing, pClip->sector_list);
         }
      }
   }
   else
   {
      pClip->sector_list = P_AddSecnode(ld->frontsector, pClip->thing, pClip->sector_list);

      // Don't assume all lines are 2-sided, since some Things
      // like teleport fog are allowed regardless of whether their
      // radius takes them beyond an impassable linedef.

      // killough 3/27/98, 4/4/98:
      // Use sidedefs instead of 2s flag to determine two-sidedness.
      // killough 8/1/98: avoid duplicate if same sector on both sides

      if(ld->backsector && ld->backsector != ld->frontsector)
         pClip->sector_list = P_AddSecnode(ld->backsector, pClip->thing,
                                           pClip->sector_list);

   }

   return true;
}

//
// P_CreateSecNodeList 
//
// phares 3/14/98
// Alters/creates the sector_list that shows what sectors the object resides in.
//
// killough 11/98: reformatted
//
// haleyjd 04/16/2010: rewritten to use clip stack for saving global clipping
// variables when required
//
msecnode_t *P_CreateSecNodeList(Mobj *thing, fixed_t x, fixed_t y)
{
   msecnode_t *node, *list;

   if(demo_version < 200 || demo_version >= 329)
      P_PushClipStack();

   // First, clear out the existing m_thing fields. As each node is
   // added or verified as needed, m_thing will be set properly. When
   // finished, delete all nodes where m_thing is still nullptr. These
   // represent the sectors the Thing has vacated.
   
   for(node = thing->old_sectorlist; node; node = node->m_tnext)
      node->m_thing = nullptr;

   pClip->thing = thing;

   pClip->x = x;
   pClip->y = y;
   
   pClip->bbox[BOXTOP]    = y + thing->radius;
   pClip->bbox[BOXBOTTOM] = y - thing->radius;
   pClip->bbox[BOXRIGHT]  = x + thing->radius;
   pClip->bbox[BOXLEFT]   = x - thing->radius;

   validcount++; // used to make sure we only process a line once

   pClip->sector_list = thing->old_sectorlist;

   // ioanch 20160115: use portal-aware gathering if there are portals. Sectors
   // may be touched both horizontally (like in Doom) or vertically (thing
   // touching portals
   if(useportalgroups && full_demo_version >= make_full_version(340, 48))
   {
      // FIXME: unfortunately all sectors need to be added, because this function
      // is only called on XY coordinate change.

      int curgroupid = R_NOGROUP;
      P_TransPortalBlockWalker(pClip->bbox, thing->groupid, true, &curgroupid,
         [](int x, int y, int groupid, void *data) -> bool
      {
         int &curgroupid = *static_cast<int *>(data);
         if(groupid != curgroupid)
         {
            curgroupid = groupid;
            // We're at a new groupid. Start by adding the midsector.

            // Get the offset from thing's position to the PREVIOUS groupid
            if(groupid == pClip->thing->groupid)
            {
               pClip->sector_list = P_AddSecnode(pClip->thing->subsector->sector,
                  pClip->thing, pClip->sector_list);
            }
            else
            {
               sector_t *sector
               = P_PointReachesGroupVertically(pClip->x, pClip->y,
                                               pClip->thing->z,
                                               pClip->thing->groupid, groupid,
                                               pClip->thing->subsector->sector,
                                               pClip->thing->z);
               if(sector)
               {
                  // Add it
                  pClip->sector_list = P_AddSecnode(sector, pClip->thing,
                                                    pClip->sector_list);
               }
            }
         }
         P_BlockLinesIterator(x, y, PIT_GetSectors, groupid);
         return true;
      });
      list = pClip->sector_list;
   }
   else
   {
      // ioanch: classic mode
      int xl = (pClip->bbox[BOXLEFT  ] - bmaporgx) >> MAPBLOCKSHIFT;
      int xh = (pClip->bbox[BOXRIGHT ] - bmaporgx) >> MAPBLOCKSHIFT;
      int yl = (pClip->bbox[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
      int yh = (pClip->bbox[BOXTOP   ] - bmaporgy) >> MAPBLOCKSHIFT;

      for(int bx = xl; bx <= xh; bx++)
      {
         for(int by = yl; by <= yh; by++)
            P_BlockLinesIterator(bx, by, PIT_GetSectors);
      }

      // Add the sector of the (x,y) point to sector_list.
      list = P_AddSecnode(thing->subsector->sector, thing, pClip->sector_list);
   }

   // Now delete any nodes that won't be used. These are the ones where
   // m_thing is still nullptr.
   
   for(node = list; node;)
   {
      if(node->m_thing == nullptr)
      {
         if(node == list)
            list = node->m_tnext;
         node = P_DelSecnode(node);
      }
      else
         node = node->m_tnext;
   }

  /* cph -
   * This is the strife we get into for using global variables. 
   *  clip.thing is being used by several different functions calling
   *  P_BlockThingIterator, including functions that can be called 
   *  *from* P_BlockThingIterator. Using a global clip.thing is not 
   *  reentrant. OTOH for Boom/MBF demos we have to preserve the buggy 
   *  behaviour. Fun. We restore its previous value unless we're in a 
   *  Boom/MBF demo. -- haleyjd: add SMMU too :)
   */

   if(demo_version < 200 || demo_version >= 329)
      P_PopClipStack();

   return list;
}

//
// Clears all remaining references to avoid dangling references on next PU_LEVEL session.
//
void P_ClearGlobalLevelReferences()
{
   clip.thing = nullptr;   // this isn't reference-counted
   clip.ceilingline = clip.blockline = clip.floorline = nullptr;
   clip.numspechit = 0;
   clip.open.frontsector = clip.open.backsector = nullptr;
   clip.BlockingMobj = nullptr;  // also not ref-counted
   clip.numportalhit = 0;
   P_ClearTarget(clip.linetarget);
}

//
// Return true if mobj is either on ground or another thing
//
bool P_OnGroundOrThing(const Mobj &mobj)
{
   return mobj.z <= mobj.zref.floor || ( P_Use3DClipping() &&   mobj.intflags & MIF_ONMOBJ);
   // negative:
   //     mobj.z >  mobj.zref.floor && (!P_Use3DClipping() || !(mobj.intflags & MIF_ONMOBJ))
}

//----------------------------------------------------------------------------
//
// $Log: p_map.c,v $
// Revision 1.35  1998/05/12  12:47:16  phares
// Removed OVER UNDER code
//
// Revision 1.34  1998/05/07  00:52:38  killough
// beautification
//
// Revision 1.33  1998/05/05  15:35:10  phares
// Documentation and Reformatting changes
//
// Revision 1.32  1998/05/04  12:29:27  phares
// Eliminate player bobbing when stuck against wall
//
// Revision 1.31  1998/05/03  23:22:19  killough
// Fix #includes and remove unnecessary decls at the top, make some vars static
//
// Revision 1.30  1998/04/20  11:12:59  killough
// Make topslope, bottomslope local
//
// Revision 1.29  1998/04/12  01:56:51  killough
// Prevent no-clipping objects from blocking things
//
// Revision 1.28  1998/04/07  11:39:21  jim
// Skip MF_NOBLOCK things in P_CheckSector to get puffs back
//
// Revision 1.27  1998/04/07  06:52:36  killough
// Simplify sector_thinglist traversal to use simpler markers
//
// Revision 1.26  1998/04/06  11:05:11  jim
// Remove LEESFIXES, AMAP bdg->247
//
// Revision 1.25  1998/04/06  04:46:13  killough
// Fix CheckSector problems
//
// Revision 1.24  1998/04/05  10:08:51  jim
// changed crusher check back to old code
//
// Revision 1.23  1998/04/03  14:44:14  jim
// Fixed P_CheckSector problem
//
// Revision 1.21  1998/04/01  14:46:48  jim
// Prevent P_CheckSector from passing NULL things
//
// Revision 1.20  1998/03/29  20:14:35  jim
// Fixed use of deleted link in P_CheckSector
//
// Revision 1.19  1998/03/28  18:00:14  killough
// Fix telefrag/spawnfrag bug, and use sidedefs rather than 2s flag
//
// Revision 1.18  1998/03/23  15:24:25  phares
// Changed pushers to linedef control
//
// Revision 1.17  1998/03/23  06:43:14  jim
// linedefs reference initial version
//
// Revision 1.16  1998/03/20  02:10:43  jim
// Improved crusher code with new mobj data structures
//
// Revision 1.15  1998/03/20  00:29:57  phares
// Changed friction to linedef control
//
// Revision 1.14  1998/03/16  12:25:17  killough
// Allow conveyors to push things off ledges
//
// Revision 1.13  1998/03/12  14:28:42  phares
// friction and IDCLIP changes
//
// Revision 1.12  1998/03/11  17:48:24  phares
// New cheats, clean help code, friction fix
//
// Revision 1.11  1998/03/09  22:27:23  phares
// Fixed friction problem when teleporting
//
// Revision 1.10  1998/03/09  18:27:00  phares
// Fixed bug in neighboring variable friction sectors
//
// Revision 1.9  1998/03/02  12:05:56  killough
// Add demo_compatibility switch around moveangle+=10
//
// Revision 1.8  1998/02/24  08:46:17  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.7  1998/02/17  06:01:51  killough
// Use new RNG calling sequence
//
// Revision 1.6  1998/02/05  12:15:03  phares
// cleaned up comments
//
// Revision 1.5  1998/01/28  23:42:02  phares
// Bug fix to PE->LS code; better line checking
//
// Revision 1.4  1998/01/28  17:36:06  phares
// Expanded comments on Pit_CrossLine
//
// Revision 1.3  1998/01/28  12:22:21  phares
// AV bug fix and Lost Soul trajectory bug fix
//
// Revision 1.2  1998/01/26  19:24:09  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:59  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------

