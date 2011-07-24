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
#include "i_system.h"

#include "c_io.h"
#include "d_gi.h"
#include "d_mod.h"
#include "doomstat.h"
#include "e_exdata.h"
#include "e_states.h"
#include "e_things.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "m_random.h"
#include "p_inter.h"
#include "p_mobj.h"
#include "p_maputl.h"
#include "p_map3d.h"
#include "p_mapcontext.h"
#include "p_doomclip.h"
#include "p_partcl.h"
#include "p_portal.h"
#include "p_setup.h"
#include "p_skin.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_traceengine.h"
#include "p_user.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_portal.h"
#include "r_segs.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"
#include "v_misc.h"
#include "v_video.h"


// SoM: This should be ok left out of the globals struct.
static int pe_x; // Pain Elemental position for Lost Soul checks // phares
static int pe_y; // Pain Elemental position for Lost Soul checks // phares
static int ls_x; // Lost Soul position for Lost Soul checks      // phares
static int ls_y; // Lost Soul position for Lost Soul checks      // phares


int spechits_emulation;
#define MAXSPECHIT_OLD 8         // haleyjd 09/20/06: old limit for overflow emu

extern bool reset_viewz;

// haleyjd 09/20/06: moved to top for maximum visibility
static int crushchange;
static int nofit;


// ----------------------------------------------------------------------------
// ClipContext

ClipContext* DoomClipEngine::getContext()
{
   return &clipc;
}

void DoomClipEngine::freeContext(ClipContext *cc)
{
}



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

#ifdef R_LINKEDPORTALS
// SoM: for portal teleports, PIT_StompThing will stomp anything the player is touching on the
// x/y plane which means if the player jumps through a mile above a demon, the demon will be
// telefragged. This simply will not do.
static bool stomp3d = false;

static bool PIT_StompThing3D(Mobj *thing, MapContext *mc)
{
   ClipContext *cc = mc->clipContext();
   fixed_t blockdist;
   
   if(!(thing->flags & MF_SHOOTABLE)) // Can't shoot it? Can't stomp it!
   {
      return true;
   }
   
   blockdist = thing->radius + cc->thing->radius;
   
   if(D_abs(thing->x - cc->x) >= blockdist ||
      D_abs(thing->y - cc->y) >= blockdist)
      return true; // didn't hit it
   
   // don't clip against self
   if(thing == cc->thing)
      return true;

   // Don't stomp what you ain't touchin'!
   if(cc->thing->z >= thing->z + thing->height ||
      thing->z >= cc->thing->z + cc->thing->height)
      return true;

   // The object moving is a player?
   if(cc->thing->player)
   {
      // "thing" dies, unconditionally
      P_DamageMobj(thing, cc->thing, cc->thing, 10000, MOD_TELEFRAG); // Stomp!

      // if "thing" is also a player, both die, for fairness.
      if(thing->player)
         P_DamageMobj(cc->thing, thing, thing, 10000, MOD_TELEFRAG);
   }
   else if(thing->player) // Thing moving into a player?
   {
      // clip.thing dies
      P_DamageMobj(cc->thing, thing, thing, 10000, MOD_TELEFRAG);
   }
   else // Neither thing is a player...
   {
   }
   
   return true;
}
#endif


static bool PIT_StompThing(Mobj *thing, MapContext *mc)
{
   ClipContext *cc = mc->clipContext();
   fixed_t blockdist;
   
   if(!(thing->flags & MF_SHOOTABLE)) // Can't shoot it? Can't stomp it!
   {
      // haleyjd 06/06/05: some teleports may not want to stick the
      // object right inside of an inert object at the destination...
      if(ignore_inerts)
         return true;
   }
   
   blockdist = thing->radius + cc->thing->radius;
   
   if(D_abs(thing->x - cc->x) >= blockdist ||
      D_abs(thing->y - cc->y) >= blockdist)
      return true; // didn't hit it
   
   // don't clip against self
   if(thing == cc->thing)
      return true;
   
   // monsters don't stomp things except on boss level
   // killough 8/9/98: make consistent across all levels
   if(!telefrag)
      return false;
   
   P_DamageMobj(thing, cc->thing, cc->thing, 10000, MOD_TELEFRAG); // Stomp!
   
   return true;
}
//
// P_TeleportMove
//

// killough 8/9/98
bool DoomClipEngine::teleportMove(Mobj *thing, fixed_t x, fixed_t y, bool boss)
{
   int xl, xh, yl, yh, bx, by;
   subsector_t *newsubsec;
   bool (*func)(Mobj *, MapContext *);
   
   // killough 8/9/98: make telefragging more consistent, preserve compatibility
   // haleyjd 03/25/03: TELESTOMP flag handling moved here (was thing->player)
   telefrag = (thing->flags3 & MF3_TELESTOMP) || 
              (!comp[comp_telefrag] ? boss : (gamemap == 30));

   // kill anything occupying the position
   
   ClipContext *cc = getContext();
   
   cc->thing = thing;
   
   cc->x = x;
   cc->y = y;
   
   cc->bbox[BOXTOP]    = y + cc->thing->radius;
   cc->bbox[BOXBOTTOM] = y - cc->thing->radius;
   cc->bbox[BOXRIGHT]  = x + cc->thing->radius;
   cc->bbox[BOXLEFT]   = x - cc->thing->radius;
   
   newsubsec = R_PointInSubsector(x,y);
   cc->ceilingline = NULL;
   
   // The base floor/ceiling is from the subsector
   // that contains the point.
   // Any contacted lines the step closer together
   // will adjust them.
   
#ifdef R_LINKEDPORTALS
    //newsubsec->sector->floorheight - clip.thing->height;
   if(demo_version >= 333 && newsubsec->sector->f_pflags & PS_PASSABLE)
      cc->floorz = cc->dropoffz = newsubsec->sector->floorheight - (1024 << FRACBITS);
   else
#endif
      cc->floorz = cc->dropoffz = newsubsec->sector->floorheight;

#ifdef R_LINKEDPORTALS
    //newsubsec->sector->ceilingheight + clip.thing->height;
   if(demo_version >= 333 && newsubsec->sector->c_pflags & PS_PASSABLE)
      cc->ceilingz = newsubsec->sector->ceilingheight + (1024 << FRACBITS);
   else
#endif
      cc->ceilingz = newsubsec->sector->ceilingheight;

   cc->secfloorz = cc->passfloorz = cc->floorz;
   cc->secceilz = cc->passceilz = cc->ceilingz;

   // haleyjd
   cc->floorpic = newsubsec->sector->floorpic;
   
   // SoM 09/07/02: 3dsides monster fix
   cc->touch3dside = 0;
   
   validcount++;
   cc->spechit.makeEmpty();
   
   // stomp on any things contacted
#ifdef R_LINKEDPORTALS
   if(stomp3d)
      func = PIT_StompThing3D;
   else
#endif
      func = PIT_StompThing;
   
   xl = (cc->bbox[BOXLEFT  ] - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
   xh = (cc->bbox[BOXRIGHT ] - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
   yl = (cc->bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
   yh = (cc->bbox[BOXTOP   ] - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

   for(bx = xl; bx <= xh; bx++)
   {
      for(by = yl; by <= yh; by++)
      {
         if(!P_BlockThingsIterator(bx, by, func, cc))
         {
            freeContext(cc);
            return false;
         }
      }
   }

   // the move is ok,
   // so unlink from the old position & link into the new position
   
   P_UnsetThingPosition(thing);
   
   thing->floorz   = cc->floorz;
   thing->ceilingz = cc->ceilingz;
   thing->dropoffz = cc->dropoffz;        // killough 11/98

   thing->passfloorz = cc->passfloorz;
   thing->passceilz  = cc->passceilz;
   thing->secfloorz  = cc->secfloorz;
   thing->secceilz   = cc->secceilz;
   
   thing->x = x;
   thing->y = y;
   
   P_SetThingPosition(thing);

   freeContext(cc);   
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
bool DoomClipEngine::teleportMoveStrict(Mobj *thing, fixed_t x, fixed_t y, bool boss)
{
   bool res;

   ignore_inerts = false;
   res = teleportMove(thing, x, y, boss);
   ignore_inerts = true;
   
   return res;
}


#ifdef R_LINKEDPORTALS
//
// P_PortalTeleportMove
//
// SoM: calls P_TeleportMove with the stomp3d flag set to true
bool DoomClipEngine::portalTeleportMove(Mobj *thing, fixed_t x, fixed_t y)
{
   bool res;

   stomp3d = true;
   res = teleportMove(thing, x, y, false);
   stomp3d = false;

   return res;
}
#endif


// SoM: Note to killough: There was no reason to put these two functions 
// between the teleport iterators and the teleport functions. THANKS

//
// killough 8/28/98:
//
// getFriction()
//
// Returns the friction associated with a particular mobj.
//
int DoomClipEngine::getFriction(const Mobj *mo, int *frictionfactor)
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

   if(!(mo->flags & (MF_NOCLIP|MF_NOGRAVITY)) 
      && (demo_version >= 203 || (mo->player && !compatibility)) &&
      variable_friction)
   {
      for (m = mo->touching_sectorlist; m; m = m->m_tnext)
      {
         if((sec = m->m_sector)->flags & SECF_FRICTION &&
            (sec->friction < friction || friction == ORIG_FRICTION) &&
            (mo->z <= sec->floorheight ||
             (sec->heightsec != -1 &&
              mo->z <= sectors[sec->heightsec].floorheight &&
              demo_version >= 203)))
         {
            friction = sec->friction, movefactor = sec->movefactor;
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
int DoomClipEngine::getMoveFactor(Mobj *mo, int *frictionp)
{
   int movefactor, friction;

   // haleyjd 04/11/10: restored BOOM friction code for compatibility
   if(demo_version < 203)
   {
      int momentum;

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

            momentum = (P_AproxDistance(mo->momx, mo->momy));
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

   if((friction = getFriction(mo, &movefactor)) < ORIG_FRICTION)
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

static bool PIT_CrossLine(line_t *ld, MapContext *mc)
{
   ClipContext *cc = mc->clipContext();
   // SoM 9/7/02: wow a killoughism... * SoM is scared
   int flags = ML_TWOSIDED | ML_BLOCKING | ML_BLOCKMONSTERS;

   if(ld->flags & ML_3DMIDTEX)
      flags &= ~ML_BLOCKMONSTERS;

   return 
      !((ld->flags ^ ML_TWOSIDED) & flags)
      || cc->bbox[BOXLEFT]   > ld->bbox[BOXRIGHT]
      || cc->bbox[BOXRIGHT]  < ld->bbox[BOXLEFT]   
      || cc->bbox[BOXTOP]    < ld->bbox[BOXBOTTOM]
      || cc->bbox[BOXBOTTOM] > ld->bbox[BOXTOP]
      || P_PointOnLineSide(pe_x,pe_y,ld) == P_PointOnLineSide(ls_x,ls_y,ld);
}

// killough 8/1/98: used to test intersection between thing and line
// assuming NO movement occurs -- used to avoid sticky situations.

static int untouched(line_t *ld, ClipContext *cc)
{
   fixed_t x, y, tmbbox[4];
   return 
     (tmbbox[BOXRIGHT] = (x=cc->thing->x)+cc->thing->radius) <= ld->bbox[BOXLEFT] ||
     (tmbbox[BOXLEFT] = x-cc->thing->radius) >= ld->bbox[BOXRIGHT] ||
     (tmbbox[BOXTOP] = (y=cc->thing->y)+cc->thing->radius) <= ld->bbox[BOXBOTTOM] ||
     (tmbbox[BOXBOTTOM] = y-cc->thing->radius) >= ld->bbox[BOXTOP] ||
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
static void SpechitOverrun(line_t *ld, ClipContext *cc)
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
         baseaddr = (unsigned int)strtol(myargv[p + 1], NULL, 0);
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
   addr = baseaddr + (ld - lines) * 0x3E;

   // Note: only the variables affected up to 20 are known, and it is of no
   // consequence to alter any of the variables between 15 and 20 because they
   // are all statics used by PIT_ iterator functions in this module and are
   // always reset to a valid value before being used again.
   switch(cc->spechit.getLength())
   {
   case 9:
   case 10:
   case 11:
   case 12:
      cc->bbox[cc->spechit.getLength() - 9] = addr;
      break;
   case 13:
      crushchange = addr;
      break;
   case 14:
      nofit = addr;
      break;
   default:
      C_Printf(FC_ERROR "Warning: spechit overflow %d not emulated\a\n", 
               cc->spechit.getLength());
      break;
   }
}

//
// PIT_CheckLine
//
// Adjusts tmfloorz and tmceilingz as lines are contacted
//
bool PIT_CheckLine(line_t *ld, MapContext *mc)
{
   ClipContext *cc = mc->clipContext();
   if(cc->bbox[BOXRIGHT]  <= ld->bbox[BOXLEFT]   || 
      cc->bbox[BOXLEFT]   >= ld->bbox[BOXRIGHT]  || 
      cc->bbox[BOXTOP]    <= ld->bbox[BOXBOTTOM] || 
      cc->bbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
      return true; // didn't hit it

   if(P_BoxOnLineSide(cc->bbox, ld) != -1)
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
      cc->blockline = ld;
      return cc->unstuck && !untouched(ld, cc) &&
         FixedMul(cc->x-cc->thing->x,ld->dy) > FixedMul(cc->y-cc->thing->y,ld->dx);
   }

   // killough 8/10/98: allow bouncing objects to pass through as missiles
   if(!(cc->thing->flags & (MF_MISSILE | MF_BOUNCES)))
   {
      if(ld->flags & ML_BLOCKING)           // explicitly blocking everything
         return cc->unstuck && !untouched(ld, cc);  // killough 8/1/98: allow escape

      // killough 8/9/98: monster-blockers don't affect friends
      // SoM 9/7/02: block monsters standing on 3dmidtex only
      if(!(cc->thing->flags & MF_FRIEND || cc->thing->player) && 
         ld->flags & ML_BLOCKMONSTERS && 
         !(ld->flags & ML_3DMIDTEX))
         return false; // block monsters only
   }

   // set openrange, opentop, openbottom
   // these define a 'window' from one sector to another across this line
   
   P_LineOpening(ld, cc->thing, cc);

   // adjust floor & ceiling heights
   
   if(opening.top < cc->ceilingz)
   {
      cc->ceilingz = opening.top;
      cc->ceilingline = ld;
      cc->blockline = ld;
   }

   if(opening.bottom > cc->floorz)
   {
      cc->floorz = opening.bottom;

      cc->floorline = ld;          // killough 8/1/98: remember floor linedef
      cc->blockline = ld;
   }

   if(opening.lowfloor < cc->dropoffz)
      cc->dropoffz = opening.lowfloor;

   // haleyjd 11/10/04: 3DMidTex fix: never consider dropoffs when
   // touching 3DMidTex lines.
   if(demo_version >= 331 && cc->touch3dside)
      cc->dropoffz = cc->floorz;

   if(opening.secfloor > cc->secfloorz)
      cc->secfloorz = opening.secfloor;
   if(opening.secceil < cc->secceilz)
      cc->secceilz = opening.secceil;

   // SoM 11/6/02: AGHAH
   if(cc->floorz > cc->passfloorz)
      cc->passfloorz = cc->floorz;
   if(cc->ceilingz < cc->passceilz)
      cc->passceilz = cc->ceilingz;

   // if contacted a special line, add it to the list
#ifdef R_LINKEDPORTALS   
   if(ld->special || ld->portal)
#else
   if(ld->special)
#endif
   {
      // 1/11/98 killough: remove limit on lines hit, by array doubling
      cc->spechit.add(ld);

      // haleyjd 09/20/06: spechit overflow emulation
      if(cc->spechit.getLength() > MAXSPECHIT_OLD)
         SpechitOverrun(ld, cc);
   }
   
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
bool P_Touched(Mobj *thing, ClipContext *cc)
{
   static int painType = -1, skullType;   

   // EDF FIXME: temporary fix?
   if(painType == -1)
   {
      painType  = E_ThingNumForDEHNum(MT_PAIN);
      skullType = E_ThingNumForDEHNum(MT_SKULL);
   }

   if(thing->flags & MF_TOUCHY &&                  // touchy object
      cc->thing->flags & MF_SOLID &&              // solid object touches it
      thing->health > 0 &&                         // touchy object is alive
      (thing->intflags & MIF_ARMED ||              // Thing is an armed mine
       sentient(thing)) &&                         // ... or a sentient thing
      (thing->type != cc->thing->type ||          // only different species
       thing->player) &&                           // ... or different players
      thing->z + thing->height >= cc->thing->z && // touches vertically
      cc->thing->z + cc->thing->height >= thing->z &&
      (thing->type ^ painType) |                   // PEs and lost souls
      (cc->thing->type ^ skullType) &&            // are considered same
      (thing->type ^ skullType) |                  // (but Barons & Knights
      (cc->thing->type ^ painType))               // are intentionally not)
   {
      P_DamageMobj(thing, NULL, NULL, thing->health, MOD_UNKNOWN); // kill object
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
bool P_CheckPickUp(Mobj *thing, ClipContext *cc)
{
   int solid = thing->flags & MF_SOLID;

   if(cc->thing->flags & MF_PICKUP)
      P_TouchSpecialThing(thing, cc->thing); // can remove thing

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
bool P_SkullHit(Mobj *thing, ClipContext *cc)
{
   bool ret = false;

   if(cc->thing->flags & MF_SKULLFLY)
   {
      // A flying skull is smacking something.
      // Determine damage amount, and the skull comes to a dead stop.

      int damage = (P_Random(pr_skullfly) % 8 + 1) * cc->thing->damage;
      
      P_DamageMobj(thing, cc->thing, cc->thing, damage, cc->thing->info->mod);
      
      cc->thing->flags &= ~MF_SKULLFLY;
      cc->thing->momx = cc->thing->momy = cc->thing->momz = 0;

      // haleyjd: Note that this is where potential for a recursive clipping
      // operation occurs -- P_SetMobjState causes a call to A_Look, which
      // causes another state-set to the seestate, which calls A_Chase.
      // A_Chase in turn calls P_TryMove and that can cause a lot of shit
      // to explode.
      //
      // 09/29/09: Repair outside of old demos by calling P_SetMobjStateNF.
      // This function cannot cause recursion, since it does not execute the
      // state's action function. This actually fixes a lot of problems.

      if(demo_version >= 335)
         P_SetMobjStateNF(cc->thing, cc->thing->info->spawnstate);
      else
         P_SetMobjState(cc->thing, cc->thing->info->spawnstate);

      cc->BlockingMobj = NULL; // haleyjd: from zdoom

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
   return (demo_version >= 333 && !comp[comp_theights] &&
           mo->flags3 & MF3_3DDECORATION) ? mo->info->height : mo->height;
}

//
// PIT_CheckThing
// 
// killough 3/26/98: make static
static bool PIT_CheckThing(Mobj *thing, MapContext *mc)
{
   ClipContext *cc = mc->clipContext();
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

   blockdist = thing->radius + cc->thing->radius;
   
   if(D_abs(thing->x - cc->x) >= blockdist ||
      D_abs(thing->y - cc->y) >= blockdist)
      return true; // didn't hit it

   // killough 11/98:
   //
   // This test has less information content (it's almost always false), so it
   // should not be moved up to first, as it adds more overhead than it removes.
   
   // don't clip against self
   
   if(thing == cc->thing)
      return true;

   // haleyjd 1/17/00: set global hit reference
   cc->BlockingMobj = thing;

   // killough 11/98:
   //
   // TOUCHY flag, for mines or other objects which die on contact with solids.
   // If a solid object of a different type comes in contact with a touchy
   // thing, and the touchy thing is not the sole one moving relative to fixed
   // surroundings such as walls, then the touchy thing dies immediately.

   // haleyjd: functionalized
   if(P_Touched(thing, cc))
      return true;

   // check for skulls slamming into things

   if(P_SkullHit(thing, cc))
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
      int height = P_MissileBlockHeight(thing);

      // haleyjd: some missiles can go through ghosts
      if(thing->flags3 & MF3_GHOST && cc->thing->flags3 & MF3_THRUGHOST)
         return true;

      // see if it went over / under
      
      if(cc->thing->z > thing->z + height) // haleyjd 07/06/05
         return true;    // overhead
      
      if(cc->thing->z + cc->thing->height < thing->z)
         return true;    // underneath

      if(cc->thing->target &&
         (cc->thing->target->type == thing->type ||
          (cc->thing->target->type == knightType && thing->type == bruiserType)||
          (cc->thing->target->type == bruiserType && thing->type == knightType)))
      {
         if(thing == cc->thing->target)
            return true;                // Don't hit same species as originator.
         else if(!(thing->player))      // Explode, but do no damage.
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
            P_RipperBlood(clip.thing);
         }
         */
         
         // TODO: ripper sound - gamemode dependent? thing dependent?
         //S_StartSound(clip.thing, sfx_ripslop);
         
         damage = ((P_Random(pr_rip) & 3) + 2) * cc->thing->damage;
         
         P_DamageMobj(thing, cc->thing, cc->thing->target, damage, 
                      cc->thing->info->mod);
         
         if(thing->flags2 & MF2_PUSHABLE && !(cc->thing->flags3 & MF3_CANNOTPUSH))
         { 
            // Push thing
            thing->momx += cc->thing->momx >> 2;
            thing->momy += cc->thing->momy >> 2;
         }
         
         // TODO: huh?
         //numspechit = 0;
         return true;
      }

      // killough 8/10/98: if moving thing is not a missile, no damage
      // is inflicted, and momentum is reduced if object hit is solid.

      if(!(cc->thing->flags & MF_MISSILE))
      {
         if(!(thing->flags & MF_SOLID))
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

      if(!(thing->flags & MF_SHOOTABLE))
         return !(thing->flags & MF_SOLID); // didn't do any damage

      // damage / explode
      
      damage = ((P_Random(pr_damage)%8)+1)*cc->thing->damage;
      P_DamageMobj(thing, cc->thing, cc->thing->target, damage,
                   cc->thing->info->mod);

      // don't traverse any more
      return false;
   }

   // haleyjd 1/16/00: Pushable objects -- at last!
   //   This is remarkably simpler than I had anticipated!
   
   if(thing->flags2 & MF2_PUSHABLE && !(cc->thing->flags3 & MF3_CANNOTPUSH))
   {
      // transfer one-fourth momentum along the x and y axes
      thing->momx += cc->thing->momx / 4;
      thing->momy += cc->thing->momy / 4;
   }

   // check for special pickup
   
   if(thing->flags & MF_SPECIAL)
      return P_CheckPickUp(thing, cc);

   // killough 3/16/98: Allow non-solid moving objects to move through solid
   // ones, by allowing the moving thing (clip.thing) to move if it's non-solid,
   // despite another solid thing being in the way.
   // killough 4/11/98: Treat no-clipping things as not blocking

   // haleyjd: needed for old demos after all?
   if(demo_compatibility)
      return !(thing->flags & MF_SOLID);
   else
      return !((thing->flags & MF_SOLID && !(thing->flags & MF_NOCLIP))
               && (cc->thing->flags & MF_SOLID || demo_compatibility));

   //return !(thing->flags & MF_SOLID);   // old code -- killough
}


//
// checkSides
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
bool DoomClipEngine::checkSides(Mobj *actor, int x, int y, ClipContext *cc)
{
   int bx,by,xl,xh,yl,yh;
   
   pe_x = actor->x;
   pe_y = actor->y;
   ls_x = x;
   ls_y = y;

   // Here is the bounding box of the trajectory
   
   cc->bbox[BOXLEFT]   = pe_x < x ? pe_x : x;
   cc->bbox[BOXRIGHT]  = pe_x > x ? pe_x : x;
   cc->bbox[BOXTOP]    = pe_y > y ? pe_y : y;
   cc->bbox[BOXBOTTOM] = pe_y < y ? pe_y : y;

   // Determine which blocks to look in for blocking lines
   
   xl = (cc->bbox[BOXLEFT]   - bmaporgx)>>MAPBLOCKSHIFT;
   xh = (cc->bbox[BOXRIGHT]  - bmaporgx)>>MAPBLOCKSHIFT;
   yl = (cc->bbox[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
   yh = (cc->bbox[BOXTOP]    - bmaporgy)>>MAPBLOCKSHIFT;

   // xl->xh, yl->yh determine the mapblock set to search

   validcount++; // prevents checking same line twice
   for(bx = xl ; bx <= xh ; bx++)
      for (by = yl ; by <= yh ; by++)
         if(!P_BlockLinesIterator(bx,by,PIT_CrossLine,cc))
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
//  floorz
//  ceilingz
//  tmdropoffz
//   the lowest point contacted
//   (monsters won't move to a dropoff)
//  speciallines[]
//  numspeciallines
//
bool DoomClipEngine::checkPosition(Mobj *thing, fixed_t x, fixed_t y, ClipContext *cc) 
{
   int xl, xh, yl, yh, bx, by;
   subsector_t *newsubsec;

   // haleyjd: OVER_UNDER
   if(!comp[comp_overunder])
      return P_CheckPosition3D(thing, x, y, cc);
   
   cc->thing = thing;
   
   cc->x = x;
   cc->y = y;
   
   cc->bbox[BOXTOP]    = y + cc->thing->radius;
   cc->bbox[BOXBOTTOM] = y - cc->thing->radius;
   cc->bbox[BOXRIGHT]  = x + cc->thing->radius;
   cc->bbox[BOXLEFT]   = x - cc->thing->radius;
   
   newsubsec = R_PointInSubsector(x,y);
   cc->floorline = cc->blockline = cc->ceilingline = NULL; // killough 8/1/98

   // Whether object can get out of a sticky situation:
   cc->unstuck = thing->player &&          // only players
      thing->player->mo == thing &&       // not voodoo dolls
      demo_version >= 203;                // not under old demos

   // The base floor / ceiling is from the subsector
   // that contains the point.
   // Any contacted lines the step closer together
   // will adjust them.

   cc->floorz = cc->dropoffz = newsubsec->sector->floorheight;
   cc->ceilingz = newsubsec->sector->ceilingheight;

   cc->secfloorz = cc->passfloorz = cc->floorz;
   cc->secceilz = cc->passceilz = cc->ceilingz;

   // haleyjd
   cc->floorpic = newsubsec->sector->floorpic;
   
   // SoM: 09/07/02: 3dsides monster fix
   cc->touch3dside = 0;
   validcount++;
   
   cc->spechit.makeEmpty();

   if(cc->thing->flags & MF_NOCLIP)
      return true;

   // Check things first, possibly picking things up.
   // The bounding box is extended by MAXRADIUS
   // because Mobjs are grouped into mapblocks
   // based on their origin point, and can overlap
   // into adjacent blocks by up to MAXRADIUS units.

   xl = (cc->bbox[BOXLEFT]   - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
   xh = (cc->bbox[BOXRIGHT]  - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
   yl = (cc->bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
   yh = (cc->bbox[BOXTOP]    - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

   cc->BlockingMobj = NULL; // haleyjd 1/17/00: global hit reference

   for(bx = xl; bx <= xh; bx++)
   {
      for(by = yl; by <= yh; by++)
      {
         if(!P_BlockThingsIterator(bx, by, PIT_CheckThing,cc))
            return false;
      }
   }

   // check lines
   
   cc->BlockingMobj = NULL; // haleyjd 1/17/00: global hit reference
   
   xl = (cc->bbox[BOXLEFT]   - bmaporgx) >> MAPBLOCKSHIFT;
   xh = (cc->bbox[BOXRIGHT]  - bmaporgx) >> MAPBLOCKSHIFT;
   yl = (cc->bbox[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
   yh = (cc->bbox[BOXTOP]    - bmaporgy) >> MAPBLOCKSHIFT;

   for(bx = xl; bx <= xh; bx++)
   {
      for(by = yl; by <= yh; by++)
      {
         if(!P_BlockLinesIterator(bx, by, PIT_CheckLine, cc))
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
static bool P_CheckDropOffVanilla(Mobj *thing, int dropoff, ClipContext *cc)
{
   if(!(thing->flags & (MF_DROPOFF|MF_FLOAT)) &&
      cc->floorz - cc->dropoffz > 24 * FRACUNIT)
      return false; // don't stand over a dropoff

   return true;
}

//
// P_CheckDropOffBOOM
//
// haleyjd 04/15/2010: Dropoff check for BOOM compatibility
//
static bool P_CheckDropOffBOOM(Mobj *thing, int dropoff, ClipContext *cc)
{
   // killough 3/15/98: Allow certain objects to drop off
   if(compatibility || !dropoff)
   {
      if(!(thing->flags & (MF_DROPOFF|MF_FLOAT)) &&
         cc->floorz - cc->dropoffz > 24 * FRACUNIT)
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
static bool P_CheckDropOffMBF(Mobj *thing, int dropoff, ClipContext *cc)
{
   // killough 3/15/98: Allow certain objects to drop off
   // killough 7/24/98, 8/1/98: 
   // Prevent monsters from getting stuck hanging off ledges
   // killough 10/98: Allow dropoffs in controlled circumstances
   // killough 11/98: Improve symmetry of clipping on stairs

   if(!(thing->flags & (MF_DROPOFF|MF_FLOAT)))
   {
      if(comp[comp_dropoff])
      {
         // haleyjd: note missing 202 compatibility... WOOPS!
         if(cc->floorz - cc->dropoffz > 24 * FRACUNIT)
            return false;
      }
      else if(!dropoff || (dropoff == 2 &&
                           (cc->floorz - cc->dropoffz > 128 * FRACUNIT ||
                            !thing->target || thing->target->z > cc->dropoffz)))
      {
         // haleyjd: I can't even mentally parse this statement with 
         // any certainty.
         if(!monkeys || demo_version < 203 ?
            cc->floorz - cc->dropoffz > 24 * FRACUNIT :
            thing->floorz - cc->floorz > 24 * FRACUNIT ||
            thing->dropoffz - cc->dropoffz > 24 * FRACUNIT)
            return false;
      }
      else
      {
         cc->felldown = !(thing->flags & MF_NOGRAVITY) && 
                         thing->z - cc->floorz > 24 * FRACUNIT;
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
static bool P_CheckDropOffEE(Mobj *thing, int dropoff, ClipContext *cc)
{
   fixed_t floorz = cc->floorz;

   // haleyjd: OVER_UNDER:
   // [RH] If the thing is standing on something, use its current z as 
   // the floorz. This is so that it does not walk off of things onto a 
   // drop off.
   if(!comp[comp_overunder] && thing->intflags & MIF_ONMOBJ)
      floorz = thing->z > cc->floorz ? thing->z : cc->floorz;

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
                         (thing->z - cc->floorz > 128*FRACUNIT ||
                          !thing->target || thing->target->z > cc->floorz)))
         {
            // deny any move resulting in a difference > 24
            if(thing->z - cc->floorz > 24*FRACUNIT)
               return false;
         }
         else  // dropoff allowed
         {
            cc->felldown = !(thing->flags & MF_NOGRAVITY) &&
                           thing->z - cc->floorz > 24*FRACUNIT;
         }
         
         return true;
      }

      if(comp[comp_dropoff])
      {
         if(cc->floorz - cc->dropoffz > 24*FRACUNIT)
            return false; // don't stand over a dropoff
      }
      else if(!dropoff || 
              (dropoff == 2 &&  // large jump down (e.g. dogs)
               (floorz - cc->dropoffz > 128*FRACUNIT || 
                !thing->target || thing->target->z > cc->dropoffz)))
      {
         // haleyjd 04/14/10: This is so impossible to read that I have
         // had to restore it, because I cannot be confident that any of 
         // my interpretations of it are correct relative to C grammar.

         if(!monkeys || demo_version < 203 ?
            floorz - cc->dropoffz > 24*FRACUNIT :
            thing->floorz  - floorz > 24*FRACUNIT ||
            thing->dropoffz - cc->dropoffz > 24*FRACUNIT)
            return false;
      }
      else  // dropoff allowed -- check for whether it fell more than 24
      {
         cc->felldown = !(thing->flags & MF_NOGRAVITY) &&
                        thing->z - floorz > 24*FRACUNIT;
      }
   }

   return true;
}

typedef bool (*dropoff_func_t)(Mobj *, int, ClipContext *);

//
// P_TryMove
//
// Attempt to move to a new position,
// crossing special lines unless MF_TELEPORT is set.
//
// killough 3/15/98: allow dropoff as option
//
bool DoomClipEngine::tryMove(Mobj *thing, fixed_t x, fixed_t y, int dropoff, ClipContext *cc)
{
   fixed_t oldx, oldy, oldz;
   dropoff_func_t dropofffunc;
   
   // haleyjd 11/10/04: 3dMidTex: determine if a thing is on a line:
   // passfloorz is the floor as determined from sectors and 3DMidTex.
   // secfloorz is the floor as determined only from sectors.
   // If the two are unequal, passfloorz is the thing's floorz, and
   // the thing is at its floorz, then it is on a 3DMidTex line.
   // Complicated. x_x

   on3dmidtex = (thing->passfloorz == thing->floorz &&
                 thing->passfloorz != thing->secfloorz &&
                 thing->z == thing->floorz);
   
   cc->felldown = cc->floatok = false;               // killough 11/98

   // haleyjd: OVER_UNDER
   if(!comp[comp_overunder])
   {
      oldz = thing->z;

      if(!P_CheckPosition3D(thing, x, y, cc))
      {
         // Solid wall or thing
         if(!cc->BlockingMobj || cc->BlockingMobj->player || !thing->player)
            return false;
         else
         {
            // haleyjd: yikes...
            if(cc->BlockingMobj->z + cc->BlockingMobj->height-thing->z > 24*FRACUNIT || 
               (cc->BlockingMobj->subsector->sector->ceilingheight
                 - (cc->BlockingMobj->z + cc->BlockingMobj->height) < thing->height) ||
               (cc->ceilingz - (cc->BlockingMobj->z + cc->BlockingMobj->height) 
                 < thing->height))
            {
               return false;
            }
            
            // haleyjd: hack for touchies: don't allow running through them when
            // they die until they become non-solid (just being a corpse isn't 
            // good enough)
            if(cc->BlockingMobj->flags & MF_TOUCHY)
            {
               if(cc->BlockingMobj->health <= 0)
                  return false;
            }
         }
         if(!(cc->thing->flags3 & MF3_PASSMOBJ))
         {
            thing->z = oldz;
            return false;
         }
      }
   }
   else if(!checkPosition(thing, x, y, cc))
      return false;   // solid wall or thing

   if(!(thing->flags & MF_NOCLIP))
   {
      bool ret = cc->unstuck 
                    && !(cc->ceilingline && untouched(cc->ceilingline, cc))
                    && !(  cc->floorline && untouched(  cc->floorline, cc));

      // killough 7/26/98: reformatted slightly
      // killough 8/1/98: Possibly allow escape if otherwise stuck
      // haleyjd: OVER_UNDER: broke up impossible-to-understand predicate

      if(cc->ceilingz - cc->floorz < thing->height) // doesn't fit
         return ret;
         
      // mobj must lower to fit
      if((cc->floatok = true, !(thing->flags & MF_TELEPORT) &&
          cc->ceilingz - thing->z < thing->height))
         return ret;          

      if(!(thing->flags & MF_TELEPORT) && !(thing->flags3 & MF3_FLOORMISSILE))
      {
         // too big a step up
         if(cc->floorz - thing->z > 24*FRACUNIT)
            return ret;
         else if(!comp[comp_overunder] && thing->z < cc->floorz)
         { 
            // haleyjd: OVER_UNDER:
            // [RH] Check to make sure there's nothing in the way for the step up
            fixed_t savedz = thing->z;
            bool good;
            thing->z = cc->floorz;
            good = P_TestMobjZ(thing);
            thing->z = savedz;
            if(!good)
               return false;
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

      if(!dropofffunc(thing, dropoff, cc))
         return false; // don't stand over a dropoff

      if(thing->flags & MF_BOUNCES &&    // killough 8/13/98
         !(thing->flags & (MF_MISSILE|MF_NOGRAVITY)) &&
         !sentient(thing) && cc->floorz - thing->z > 16*FRACUNIT)
      {
         return false; // too big a step up for bouncers under gravity
      }

      // killough 11/98: prevent falling objects from going up too many steps
      if(thing->intflags & MIF_FALLING && cc->floorz - thing->z >
         FixedMul(thing->momx,thing->momx)+FixedMul(thing->momy,thing->momy))
      {
         return false;
      }

      // haleyjd: CANTLEAVEFLOORPIC flag
      if((thing->flags2 & MF2_CANTLEAVEFLOORPIC) &&
         (cc->floorpic != thing->subsector->sector->floorpic ||
          cc->floorz - thing->z != 0))
      {
         // thing must stay within its current floor type
         return false;
      }
   }

   // the move is ok,
   // so unlink from the old position and link into the new position

   P_UnsetThingPosition (thing);
   
   oldx = thing->x;
   oldy = thing->y;
   thing->floorz = cc->floorz;
   thing->ceilingz = cc->ceilingz;
   thing->dropoffz = cc->dropoffz;      // killough 11/98: keep track of dropoffs
   thing->passfloorz = cc->passfloorz;
   thing->passceilz = cc->passceilz;
   thing->secfloorz = cc->secfloorz;
   thing->secceilz = cc->secceilz;

   thing->x = x;
   thing->y = y;
   
   P_SetThingPosition(thing);

   // haleyjd 08/07/04: new footclip system
   P_AdjustFloorClip(thing);

   // if any special lines were hit, do the effect
   // killough 11/98: simplified
   if(!(thing->flags & (MF_TELEPORT | MF_NOCLIP)))
   {
      while(cc->spechit.getLength() > 0)
      {
         line_t *l = cc->spechit.removeLast();
         
// PTODO
#ifdef R_LINKEDPORTALS
         if(l->pflags & PS_PASSABLE)
         {
            // SoM: if the mobj is touching a portal line, and the line is behind
            // the mobj no matter what the previous lineside was, we missed the 
            // teleport and NEED to do so now.
            if(P_PointOnLineSide(thing->x, thing->y, l))
            {
               linkoffset_t *link = 
                  P_GetLinkOffset(l->frontsector->groupid, 
                                  l->portal->data.link.toid);
               EV_PortalTeleport(thing, link);
            }
         }
#endif
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
static bool PIT_ApplyTorque(line_t *ld, MapContext *mc)
{
   ClipContext *cc = mc->clipContext();
   
   if(ld->backsector &&       // If thing touches two-sided pivot linedef
      cc->bbox[BOXRIGHT]  > ld->bbox[BOXLEFT]  &&
      cc->bbox[BOXLEFT]   < ld->bbox[BOXRIGHT] &&
      cc->bbox[BOXTOP]    > ld->bbox[BOXBOTTOM] &&
      cc->bbox[BOXBOTTOM] < ld->bbox[BOXTOP] &&
      P_BoxOnLineSide(cc->bbox, ld) == -1)
   {
      Mobj *mo = cc->thing;

      fixed_t dist =                               // lever arm
         + (ld->dx >> FRACBITS) * (mo->y >> FRACBITS)
         - (ld->dy >> FRACBITS) * (mo->x >> FRACBITS) 
         - (ld->dx >> FRACBITS) * (ld->v1->y >> FRACBITS)
         + (ld->dy >> FRACBITS) * (ld->v1->x >> FRACBITS);

      if(dist < 0 ?                               // dropoff direction
         ld->frontsector->floorheight < mo->z &&
         ld->backsector->floorheight >= mo->z :
         ld->backsector->floorheight < mo->z &&
         ld->frontsector->floorheight >= mo->z)
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
            ++mo->gear, x >>= 1, y >>= 1, dist >>= 1;
         
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
void DoomClipEngine::applyTorque(Mobj *mo, ClipContext *cc)
{
   int xl = ((cc->bbox[BOXLEFT] = 
              mo->x - mo->radius) - bmaporgx) >> MAPBLOCKSHIFT;
   int xh = ((cc->bbox[BOXRIGHT] = 
              mo->x + mo->radius) - bmaporgx) >> MAPBLOCKSHIFT;
   int yl = ((cc->bbox[BOXBOTTOM] =
              mo->y - mo->radius) - bmaporgy) >> MAPBLOCKSHIFT;
   int yh = ((cc->bbox[BOXTOP] = 
      mo->y + mo->radius) - bmaporgy) >> MAPBLOCKSHIFT;
   int bx,by,flags = mo->intflags; //Remember the current state, for gear-change

   cc->thing = mo;
   validcount++; // prevents checking same line twice
      
   for(bx = xl ; bx <= xh ; bx++)
      for(by = yl ; by <= yh ; by++)
         P_BlockLinesIterator(bx, by, PIT_ApplyTorque,cc);
      
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
// Takes a valid thing and adjusts the thing->floorz,
// thing->ceilingz, and possibly thing->z.
// This is called for all nearby monsters
// whenever a sector changes height.
// If the thing doesn't fit,
// the z will be set to the lowest value
// and false will be returned.
//
static bool P_ThingHeightClip(Mobj *thing, ClipContext *cc)
{
   bool onfloor = thing->z == thing->floorz;
   fixed_t oldfloorz = thing->floorz; // haleyjd

   clip->checkPosition(thing, thing->x, thing->y, cc);
  
   // what about stranding a monster partially off an edge?
   // killough 11/98: Answer: see below (upset balance if hanging off ledge)
   
   thing->floorz = cc->floorz;
   thing->ceilingz = cc->ceilingz;
   thing->dropoffz = cc->dropoffz;         // killough 11/98: remember dropoffs

   thing->passfloorz = cc->passfloorz;
   thing->passceilz = cc->passceilz;
   thing->secfloorz = cc->secfloorz;
   thing->secceilz = cc->secceilz;

   // haleyjd 09/19/06: floatbobbers require special treatment here now
   if(thing->flags2 & MF2_FLOATBOB)
   {
      if(thing->floorz > oldfloorz || !(thing->flags & MF_NOGRAVITY))
         thing->z = thing->z - oldfloorz + thing->floorz;

      if(thing->z + thing->height > thing->ceilingz)
         thing->z = thing->ceilingz - thing->height;
   }
   else if(onfloor)  // walking monsters rise and fall with the floor
   {
      thing->z = thing->floorz;
      
      // killough 11/98: Possibly upset balance of objects hanging off ledges
      if(thing->intflags & MIF_FALLING && thing->gear >= MAXGEAR)
         thing->gear = 0;
   }
   else          // don't adjust a floating monster unless forced to
      if(thing->z + thing->height > thing->ceilingz)
         thing->z = thing->ceilingz - thing->height;

   return thing->ceilingz - thing->floorz >= thing->height;
}

//
// SLIDE MOVE
// Allows the player to slide along any angled walls.
//

// killough 8/2/98: make variables static
static fixed_t   bestslidefrac;
static fixed_t   secondslidefrac;
static line_t    *bestslideline;
static line_t    *secondslideline;
static Mobj      *slidemo;
static fixed_t   tmxmove;
static fixed_t   tmymove;

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
   fixed_t newlen;
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
         slidemo->z <= slidemo->floorz &&
         clip->getFriction(slidemo, NULL) > ORIG_FRICTION;
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
         if(slidemo->player)
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
         if(slidemo->player)
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
      if(slidemo->player)
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
      newlen = FixedMul (movelen, finecosine[deltaangle]);
      tmxmove = FixedMul (newlen, finecosine[lineangle]);
      tmymove = FixedMul (newlen, finesine[lineangle]);
   }
}

//
// PTR_SlideTraverse
//
static bool PTR_SlideTraverse(intercept_t *in, TracerContext *tc)
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
   
   ClipContext *cc = clip->getContext();
   P_LineOpening(li, slidemo, cc);
   cc->done();
   
   if(opening.range < slidemo->height)
      goto isblocking;  // doesn't fit
   
   if(opening.top - slidemo->z < slidemo->height)
      goto isblocking;  // mobj is too high
   
   if(opening.bottom - slidemo->z > 24*FRACUNIT )
      goto isblocking;  // too big a step up
   else if(!comp[comp_overunder] &&
           slidemo->z < opening.bottom) // haleyjd: OVER_UNDER
   { 
      // [RH] Check to make sure there's nothing in the way for the step up
      bool good;
      fixed_t savedz = slidemo->z;
      slidemo->z = opening.bottom;
      good = P_TestMobjZ(slidemo);
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
void DoomClipEngine::slideMove(Mobj *mo)
{
   ClipContext *cc      = getContext();
   int         hitcount = 3;
   
   slidemo = mo; // the object that's sliding
   
   do
   {
      fixed_t leadx, leady, trailx, traily;

      if(!--hitcount)
         goto stairstep;   // don't loop forever

      // trace along the three leading corners
      
      if(mo->momx > 0)
         leadx = mo->x + mo->radius, trailx = mo->x - mo->radius;
      else
         leadx = mo->x - mo->radius, trailx = mo->x + mo->radius;

      if(mo->momy > 0)
         leady = mo->y + mo->radius, traily = mo->y - mo->radius;
      else
         leady = mo->y - mo->radius, traily = mo->y + mo->radius;

      bestslidefrac = FRACUNIT+1;
      
      trace->pathTraverse(leadx, leady, leadx+mo->momx, leady+mo->momy,
                          PT_ADDLINES, PTR_SlideTraverse);
      trace->pathTraverse(trailx, leady, trailx+mo->momx, leady+mo->momy,
                          PT_ADDLINES, PTR_SlideTraverse);
      trace->pathTraverse(leadx, traily, leadx+mo->momx, traily+mo->momy,
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

         if(!tryMove(mo, mo->x, mo->y + mo->momy, true, cc))
            if(!tryMove(mo, mo->x + mo->momx, mo->y, true, cc))	      
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
         
         if(!tryMove(mo, mo->x+newx, mo->y+newy, true, cc))
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
   while(!tryMove(mo, mo->x+tmxmove, mo->y+tmymove, true, cc));
   
   freeContext(cc);
}

//
// RADIUS ATTACK
//

// haleyjd 09/23/09: repair to non-reentrancy and stack-fault issues in
// PIT_RadiusAttack - information is grouped into a structure, and outside
// of old demos, the following are true:
// * bombdata_t's will be pushed and popped for recursive explosions
// * a limit of 128 recursive explosions is enforced

typedef struct bombdata_s
{
   Mobj *bombsource;
   Mobj *bombspot;
   int     bombdamage;
   int     bombmod;    // haleyjd 07/13/03
} bombdata_t;

#define MAXBOMBS 128               // a static limit to prevent stack faults.
static int bombindex;              // current index into bombs array
static bombdata_t bombs[MAXBOMBS]; // bombs away!
static bombdata_t *theBomb;        // it's the bomb, man. (the current explosion)

//
// PIT_RadiusAttack
//
// "bombsource" is the creature that caused the explosion at "bombspot".
//
static bool PIT_RadiusAttack(Mobj *thing, MapContext *mc)
{
   ClipContext *cc = mc->clipContext();
   
   fixed_t dx, dy, dist;
   Mobj *bombspot   = theBomb->bombspot;
   Mobj *bombsource = theBomb->bombsource;
   
   // killough 8/20/98: allow bouncers to take damage 
   // (missile bouncers are already excluded with MF_NOBLOCKMAP)
   
   if(!(thing->flags & (MF_SHOOTABLE | MF_BOUNCES)))
      return true;
   
   // Boss spider and cyborg
   // take no damage from concussion.
   
   // killough 8/10/98: allow grenades to hurt anyone, unless
   // fired by Cyberdemons, in which case it won't hurt Cybers.   
   // haleyjd 09/21/09: do this only in old demos because it really
   // doesn't make sense with our newer features.

   if(demo_version < 335 && bombspot->flags & MF_BOUNCES)
   {
      static int cyberType = -1;
      
      if(cyberType == -1)
         cyberType = E_ThingNumForDEHNum(MT_CYBORG);

      if(thing->type == cyberType && bombsource->type == cyberType)
         return true;
   }
   else if((thing->flags2 & MF2_BOSS || thing->flags4 & MF4_NORADIUSDMG) &&
           !(bombspot->flags4 && MF4_FORCERADIUSDMG))
   {      
      // haleyjd 05/22/99: exclude all bosses
      // haleyjd 09/21/09: support separate MF4_NORADIUSDMG flag and
      //                   force radius damage flag
      return true;
   }

   dx   = D_abs(thing->x - bombspot->x);
   dy   = D_abs(thing->y - bombspot->y);
   dist = dx > dy ? dx : dy;
   dist = (dist - thing->radius) >> FRACBITS;

   if(dist < 0)
      dist = 0;

   if(dist >= theBomb->bombdamage)
      return true;  // out of range

   if(trace->checkSight(thing, bombspot))      // must be in direct path
   {
      P_DamageMobj(thing, bombspot, bombsource, theBomb->bombdamage - dist, 
                   theBomb->bombmod);
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
void DoomClipEngine::radiusAttack(Mobj *spot, Mobj *source, int damage, int mod, ClipContext *cc)
{
   fixed_t  dist = (damage + MAXRADIUS) << FRACBITS;
   int yh = (spot->y + dist - bmaporgy) >> MAPBLOCKSHIFT;
   int yl = (spot->y - dist - bmaporgy) >> MAPBLOCKSHIFT;
   int xh = (spot->x + dist - bmaporgx) >> MAPBLOCKSHIFT;
   int xl = (spot->x - dist - bmaporgx) >> MAPBLOCKSHIFT;
   int x, y;

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
   theBomb->bombspot   = spot;
   theBomb->bombsource = source;
   theBomb->bombdamage = damage;
   theBomb->bombmod    = mod;
   
   for(y = yl; y <= yh; ++y)
      for(x = xl; x <= xh; ++x)
         P_BlockThingsIterator(x, y, PIT_RadiusAttack, cc);

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
static bool PIT_ChangeSector(Mobj *thing, MapContext *mc)
{
   ClipContext *cc = mc->clipContext();
   Mobj *mo;

   if(P_ThingHeightClip(thing, cc))
      return true; // keep checking
   
   // crunch bodies to giblets
   if(thing->health <= 0)
   {
      // sf: clear the skin which will mess things up
      // haleyjd 03/11/03: only in Doom
      if(GameModeInfo->type == Game_DOOM)
      {
         thing->skin = NULL;
         P_SetMobjState(thing, E_SafeState(S_GIBS));
      }
      thing->flags &= ~MF_SOLID;
      thing->height = thing->radius = 0;
      return true;      // keep checking
   }

   // crunch dropped items
   if(thing->flags & MF_DROPPED)
   {
      thing->removeThinker();
      return true;      // keep checking
   }

   // killough 11/98: kill touchy things immediately
   if(thing->flags & MF_TOUCHY &&
      (thing->intflags & MIF_ARMED || sentient(thing)))
   {
      // kill object
      P_DamageMobj(thing, NULL, NULL, thing->health, MOD_CRUSH);
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

      P_DamageMobj(thing, NULL, NULL, crushchange, MOD_CRUSH);
      
      // haleyjd 06/26/06: NOBLOOD objects shouldn't bleed when crushed
      // haleyjd FIXME: needs compflag
      if(demo_version < 333 || !(thing->flags & MF_NOBLOOD))
      {
         // spray blood in a random direction
         mo = P_SpawnMobj(thing->x, thing->y, thing->z + thing->height/2,
                          E_SafeThingType(MT_BLOOD));
         
         // haleyjd 08/05/04: use new function
         mo->momx = P_SubRandom(pr_crush) << 12;
         mo->momy = P_SubRandom(pr_crush) << 12;         
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
bool DoomClipEngine::changeSector(sector_t *sector, int crunch, ClipContext *cc)
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
         P_BlockThingsIterator(x, y, PIT_ChangeSector, cc);
      
   return !!nofit;
}

//
// checkSector
//
// jff 3/19/98 added to just check monsters on the periphery
// of a moving sector instead of all in bounding box of the
// sector. Both more accurate and faster.
//
// haleyjd: OVER_UNDER: pass down more information to P_ChangeSector3D
// when 3D object clipping is enabled.
//
bool DoomClipEngine::checkSector(sector_t *sector, int crunch, int amt, int floorOrCeil, ClipContext *cc)
{
   msecnode_t *n;
   
   // killough 10/98: sometimes use Doom's method
   if(comp[comp_floors] && (demo_version >= 203 || demo_compatibility))
      return changeSector(sector, crunch, cc);

   // haleyjd: call down to P_ChangeSector3D instead.
   if(!comp[comp_overunder])
      return P_ChangeSector3D(sector, crunch, amt, floorOrCeil);
   
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
         if(!n->visited)                     // unprocessed thing found
         {
            n->visited  = true;              // mark thing as processed
            if(!(n->m_thing->flags & MF_NOBLOCKMAP)) //jff 4/7/98 don't do these
               PIT_ChangeSector(n->m_thing, cc); // process it
            break;                           // exit and start over
         }
      }
   }
   while(n); // repeat from scratch until all things left are marked valid
   
   return !!nofit;
}


//
// delSeclist
//
// Delete an entire sector list
//
void DoomClipEngine::delSeclist(msecnode_t *node)
{
   while(node)
      node = delSecnode(node);
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
static bool PIT_GetSectors(line_t *ld, MapContext *mc)
{
   ClipContext *cc = mc->clipContext();
   
   if(cc->bbox[BOXRIGHT]  <= ld->bbox[BOXLEFT]   ||
      cc->bbox[BOXLEFT]   >= ld->bbox[BOXRIGHT]  ||
      cc->bbox[BOXTOP]    <= ld->bbox[BOXBOTTOM] ||
      cc->bbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
      return true;

   if(P_BoxOnLineSide(cc->bbox, ld) != -1)
      return true;

   // This line crosses through the object.
   
   // Collect the sector(s) from the line and add to the
   // sector_list you're examining. If the Thing ends up being
   // allowed to move to this position, then the sector_list
   // will be attached to the Thing's Mobj at touching_sectorlist.

   cc->sector_list = ClipEngine::addSecnode(ld->frontsector, cc->thing, cc->sector_list);

   // Don't assume all lines are 2-sided, since some Things
   // like teleport fog are allowed regardless of whether their 
   // radius takes them beyond an impassable linedef.
   
   // killough 3/27/98, 4/4/98:
   // Use sidedefs instead of 2s flag to determine two-sidedness.
   // killough 8/1/98: avoid duplicate if same sector on both sides

   if(ld->backsector && ld->backsector != ld->frontsector)
      cc->sector_list = ClipEngine::addSecnode(ld->backsector, cc->thing, cc->sector_list);
   
   return true;
}




static ClipContext altc = ClipContext();

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
msecnode_t *DoomClipEngine::createSecNodeList(Mobj *thing, fixed_t x, fixed_t y)
{
   int xl, xh, yl, yh, bx, by;
   msecnode_t *node, *list;
   
   // First, clear out the existing m_thing fields. As each node is
   // added or verified as needed, m_thing will be set properly. When
   // finished, delete all nodes where m_thing is still NULL. These
   // represent the sectors the Thing has vacated.
   
   for(node = thing->old_sectorlist; node; node = node->m_tnext)
      node->m_thing = NULL;

   altc.thing = thing;

   altc.x = x;
   altc.y = y;
   
   altc.bbox[BOXTOP]    = y + thing->radius;
   altc.bbox[BOXBOTTOM] = y - thing->radius;
   altc.bbox[BOXRIGHT]  = x + thing->radius;
   altc.bbox[BOXLEFT]   = x - thing->radius;

   validcount++; // used to make sure we only process a line once
   
   xl = (altc.bbox[BOXLEFT  ] - bmaporgx) >> MAPBLOCKSHIFT;
   xh = (altc.bbox[BOXRIGHT ] - bmaporgx) >> MAPBLOCKSHIFT;
   yl = (altc.bbox[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
   yh = (altc.bbox[BOXTOP   ] - bmaporgy) >> MAPBLOCKSHIFT;

   altc.sector_list = thing->old_sectorlist;

   for(bx = xl; bx <= xh; bx++)
   {
      for(by = yl; by <= yh; by++)
         P_BlockLinesIterator(bx, by, PIT_GetSectors, &altc);
   }

   // Add the sector of the (x,y) point to sector_list.
   list = addSecnode(thing->subsector->sector, thing, altc.sector_list);

   // Now delete any nodes that won't be used. These are the ones where
   // m_thing is still NULL.
   
   for(node = list; node;)
   {
      if(node->m_thing == NULL)
      {
         if(node == list)
            list = node->m_tnext;
         node = delSecnode(node);
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

   return list;
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

static bool PIT_AvoidDropoff(line_t *line, MapContext *mc)
{
   ClipContext *cc = mc->clipContext();
   
   if(line->backsector                          && // Ignore one-sided linedefs
      cc->bbox[BOXRIGHT]  > line->bbox[BOXLEFT]   &&
      cc->bbox[BOXLEFT]   < line->bbox[BOXRIGHT]  &&
      cc->bbox[BOXTOP]    > line->bbox[BOXBOTTOM] && // Linedef must be contacted
      cc->bbox[BOXBOTTOM] < line->bbox[BOXTOP]    &&
      P_BoxOnLineSide(cc->bbox, line) == -1)
   {
      fixed_t front = line->frontsector->floorheight;
      fixed_t back  = line->backsector->floorheight;
      angle_t angle;

      // The monster must contact one of the two floors,
      // and the other must be a tall dropoff (more than 24).

      if(back == cc->dropoff_floorz && front < cc->dropoff_floorz - FRACUNIT*24)
      {
         // front side dropoff
         angle = P_PointToAngle(0,0,line->dx,line->dy);
      }
      else
      {
         // back side dropoff
         if(front == cc->dropoff_floorz && back < cc->dropoff_floorz - FRACUNIT*24)
            angle = P_PointToAngle(line->dx,line->dy,0,0);
         else
            return true;
      }

      // Move away from dropoff at a standard speed.
      // Multiple contacted linedefs are cumulative (e.g. hanging over corner)
      cc->dropoff_deltax -= finesine[angle >> ANGLETOFINESHIFT]*32;
      cc->dropoff_deltay += finecosine[angle >> ANGLETOFINESHIFT]*32;
   }

   return true;
}


//
// P_AvoidDropoff
//
// Driver for above
//
// SoM: Moved this here as part of the clipping engine.
fixed_t DoomClipEngine::avoidDropoff(Mobj *actor, ClipContext *cc)
{
   int yh=((cc->bbox[BOXTOP]   = actor->y+actor->radius)-bmaporgy)>>MAPBLOCKSHIFT;
   int yl=((cc->bbox[BOXBOTTOM]= actor->y-actor->radius)-bmaporgy)>>MAPBLOCKSHIFT;
   int xh=((cc->bbox[BOXRIGHT] = actor->x+actor->radius)-bmaporgx)>>MAPBLOCKSHIFT;
   int xl=((cc->bbox[BOXLEFT]  = actor->x-actor->radius)-bmaporgx)>>MAPBLOCKSHIFT;
   int bx, by;

   cc->dropoff_floorz = actor->z;            // remember floor height

   dropoff_deltax = dropoff_deltay = 0;

   // check lines

   validcount++;
   for(bx = xl; bx <= xh; ++bx)
   {
      // all contacted lines
      for(by = yl; by <= yh; ++by)
         P_BlockLinesIterator(bx, by, PIT_AvoidDropoff, cc);
   }
   
   // Non-zero if movement prescribed
   return dropoff_deltax | dropoff_deltay;
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

