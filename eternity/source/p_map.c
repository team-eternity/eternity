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

#include "c_io.h"
#include "doomstat.h"
#include "r_main.h"
#include "p_mobj.h"
#include "p_maputl.h"
#include "p_map.h"
#include "p_map3d.h"
#include "p_setup.h"
#include "p_spec.h"
#include "s_sound.h"
#include "sounds.h"
#include "p_inter.h"
#include "m_random.h"
#include "r_segs.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "p_partcl.h"
#include "p_tick.h"
#include "p_user.h"
#include "d_gi.h"
#include "e_states.h"
#include "e_things.h"


// SoM: This should be ok left out of the globals struct.
static int pe_x; // Pain Elemental position for Lost Soul checks // phares
static int pe_y; // Pain Elemental position for Lost Soul checks // phares
static int ls_x; // Lost Soul position for Lost Soul checks      // phares
static int ls_y; // Lost Soul position for Lost Soul checks      // phares


// SoM: Todo: Turn this into a full fledged stack.
static doom_mapinter_t  tmbase = {NULL, NULL, NULL, 0};
doom_mapinter_t  *tm = &tmbase;

static doom_mapinter_t  *unusedtm = NULL;

// TM STACK

// 
// P_PushTMStack
// Allocates or assigns a new head to the tm stack
void P_PushTMStack()
{
   doom_mapinter_t *newtm;

   if(!unusedtm)
      newtm = calloc(sizeof(doom_mapinter_t), 1);
   else
   {
      // SoM: Do not clear out the spechit stuff
      int msh = unusedtm->spechit_max;
      line_t **sh = unusedtm->spechit;

      newtm = unusedtm;
      unusedtm = unusedtm->prev;
      memset(newtm, 0, sizeof(*newtm));

      newtm->spechit_max = msh;
      newtm->spechit = sh;
   }

   newtm->prev = tm;
   tm = newtm;
}



//
// P_PopTMStack
// Removes the current tm object from the stack and places it in the unused stack. If
// tm is currently set to tmbase, this function will I_Error
void P_PopTMStack()
{
   doom_mapinter_t *oldtm;


   if(tm == &tmbase)
      I_Error("P_PopTMStack called on tmbase\n");

   oldtm = tm;
   tm = tm->prev;
   
   oldtm->prev = unusedtm;
   unusedtm = oldtm;   
}


int spechits_emulation;
#define MAXSPECHIT_OLD 8         // haleyjd 09/20/06: old limit for overflow emu

extern boolean reset_viewz;

// haleyjd 09/20/06: moved to top for maximum visibility
static int crushchange;
static boolean nofit;

//
// TELEPORT MOVE
//

//
// PIT_StompThing
//

static boolean telefrag; // killough 8/9/98: whether to telefrag at exit

// haleyjd 06/06/05: whether to return false if an inert thing 
// blocks a teleport. DOOM has allowed you to simply get stuck in
// such things so far.
static boolean ignore_inerts = true;

static boolean PIT_StompThing(mobj_t *thing)
{
   fixed_t blockdist;
   
   if(!(thing->flags & MF_SHOOTABLE)) // Can't shoot it? Can't stomp it!
   {
      // haleyjd 06/06/05: some teleports may not want to stick the
      // object right inside of an inert object at the destination...
      if(ignore_inerts)
         return true;
   }
   
   blockdist = thing->radius + tm->thing->radius;
   
   if(D_abs(thing->x - tm->x) >= blockdist ||
      D_abs(thing->y - tm->y) >= blockdist)
      return true; // didn't hit it
   
   // don't clip against self
   if(thing == tm->thing)
      return true;
   
   // monsters don't stomp things except on boss level
   // killough 8/9/98: make consistent across all levels
   if(!telefrag)
      return false;
   
   P_DamageMobj(thing, tm->thing, tm->thing, 10000, MOD_TELEFRAG); // Stomp!
   
   return true;
}

//
// killough 8/28/98:
//
// P_GetFriction()
//
// Returns the friction associated with a particular mobj.

int P_GetFriction(const mobj_t *mo, int *frictionfactor)
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
         if((sec = m->m_sector)->special & FRICTION_MASK &&
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

int P_GetMoveFactor(const mobj_t *mo, int *frictionp)
{
   int movefactor, friction;
   
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
boolean P_TeleportMove(mobj_t *thing, fixed_t x, fixed_t y, boolean boss)
{
   int xl, xh, yl, yh, bx, by;
   subsector_t *newsubsec;

   // killough 8/9/98: make telefragging more consistent, preserve compatibility
   // haleyjd 03/25/03: TELESTOMP flag handling moved here (was thing->player)
   telefrag = (thing->flags3 & MF3_TELESTOMP) || 
      (comp[comp_telefrag] || demo_version < 203 ? gamemap==30 : boss);

   // kill anything occupying the position
   
   tm->thing = thing;
   tm->flags = thing->flags;
   
   tm->x = x;
   tm->y = y;
   
   tm->bbox[BOXTOP] = y + tm->thing->radius;
   tm->bbox[BOXBOTTOM] = y - tm->thing->radius;
   tm->bbox[BOXRIGHT] = x + tm->thing->radius;
   tm->bbox[BOXLEFT] = x - tm->thing->radius;
   
   newsubsec = R_PointInSubsector (x,y);
   tm->ceilingline = NULL;
   
   // The base floor/ceiling is from the subsector
   // that contains the point.
   // Any contacted lines the step closer together
   // will adjust them.
   
#ifdef R_LINKEDPORTALS
   if(demo_version >= 333 && R_LinkedFloorActive(newsubsec->sector))
      tm->floorz = tm->dropoffz = newsubsec->sector->floorheight - (1024 << FRACBITS); //newsubsec->sector->floorheight - tm->thing->height;
   else
#endif
      tm->floorz = tm->dropoffz = newsubsec->sector->floorheight;

#ifdef R_LINKEDPORTALS
   if(demo_version >= 333 && R_LinkedCeilingActive(newsubsec->sector))
      tm->ceilingz = newsubsec->sector->ceilingheight + (1024 << FRACBITS); //newsubsec->sector->ceilingheight + tm->thing->height;
   else
#endif
      tm->ceilingz = newsubsec->sector->ceilingheight;

   tm->secfloorz = tm->stepupfloorz = tm->passfloorz = tm->floorz;
   tm->secceilz = tm->passceilz = tm->ceilingz;

   // haleyjd
   tm->floorpic = newsubsec->sector->floorpic;
   
   // SoM 09/07/02: 3dsides monster fix
   tm->touch3dside = 0;
   
   validcount++;
   tm->numspechit = 0;
   
   // stomp on any things contacted
   
   xl = (tm->bbox[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
   xh = (tm->bbox[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
   yl = (tm->bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
   yh = (tm->bbox[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;

   for(bx=xl ; bx<=xh ; bx++)
   {
      for(by=yl ; by<=yh ; by++)
      {
         if(!P_BlockThingsIterator(bx,by,PIT_StompThing))
            return false;
      }
   }


   // the move is ok,
   // so unlink from the old position & link into the new position
   
   P_UnsetThingPosition(thing);
   
   thing->floorz = tm->floorz;
   thing->ceilingz = tm->ceilingz;
   thing->dropoffz = tm->dropoffz;        // killough 11/98

   thing->passfloorz = tm->passfloorz;
   thing->passceilz = tm->passceilz;
   thing->secfloorz = tm->secfloorz;
   thing->secceilz = tm->secceilz;
   
   thing->x = x;
   thing->y = y;
   
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
boolean P_TeleportMoveStrict(mobj_t *thing, fixed_t x, fixed_t y, boolean boss)
{
   boolean res;

   ignore_inerts = false;
   res = P_TeleportMove(thing, x, y, boss);
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

static boolean PIT_CrossLine(line_t *ld)
{
   // SoM 9/7/02: wow a killoughism... * SoM is scared
   int flags = ML_TWOSIDED | ML_BLOCKING | ML_BLOCKMONSTERS;

   if(ld->flags & ML_3DMIDTEX)
      flags &= ~ML_BLOCKMONSTERS;

   return 
      !((ld->flags ^ ML_TWOSIDED) & flags)
      || tm->bbox[BOXLEFT]   > ld->bbox[BOXRIGHT]
      || tm->bbox[BOXRIGHT]  < ld->bbox[BOXLEFT]   
      || tm->bbox[BOXTOP]    < ld->bbox[BOXBOTTOM]
      || tm->bbox[BOXBOTTOM] > ld->bbox[BOXTOP]
      || P_PointOnLineSide(pe_x,pe_y,ld) == P_PointOnLineSide(ls_x,ls_y,ld);
}

// killough 8/1/98: used to test intersection between thing and line
// assuming NO movement occurs -- used to avoid sticky situations.

static int untouched(line_t *ld)
{
   fixed_t x, y, tmbbox[4];
   return 
     (tm->bbox[BOXRIGHT] = (x=tm->thing->x)+tm->thing->radius) <= ld->bbox[BOXLEFT] ||
     (tm->bbox[BOXLEFT] = x-tm->thing->radius) >= ld->bbox[BOXRIGHT] ||
     (tm->bbox[BOXTOP] = (y=tm->thing->y)+tm->thing->radius) <= ld->bbox[BOXBOTTOM] ||
     (tm->bbox[BOXBOTTOM] = y-tm->thing->radius) >= ld->bbox[BOXTOP] ||
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
   static unsigned int baseaddr = 0;
   unsigned int addr;

   // first time through, set the desired base address;
   // this is where magic number support comes in
   if(baseaddr == 0)
   {
      int p;

      if((p = M_CheckParm("-spechit")) && p < myargc - 1)
         baseaddr = atoi(myargv[p + 1]);
      else
         baseaddr = spechits_emulation == 2 ? 0x01C09C98 : 0x84F968E8;
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
   switch(tm->numspechit)
   {
   case 9:
   case 10:
   case 11:
   case 12:
      tm->bbox[tm->numspechit - 9] = addr;
      break;
   case 13:
      crushchange = addr;
      break;
   case 14:
      nofit = addr;
      break;
   default:
      C_Printf(FC_ERROR "Warning: spechit overflow %d not emulated\a\n", 
               tm->numspechit);
      break;
   }
}

//
// PIT_CheckLine
//
// Adjusts tmfloorz and tmceilingz as lines are contacted
//
boolean PIT_CheckLine(line_t *ld)
{
   if(tm->bbox[BOXRIGHT] <= ld->bbox[BOXLEFT]
      || tm->bbox[BOXLEFT] >= ld->bbox[BOXRIGHT]
      || tm->bbox[BOXTOP] <= ld->bbox[BOXBOTTOM]
      || tm->bbox[BOXBOTTOM] >= ld->bbox[BOXTOP] )
      return true; // didn't hit it

   if(P_BoxOnLineSide(tm->bbox, ld) != -1)
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
   if(!ld->backsector) // one sided line
   {
#ifdef R_LINKEDPORTALS
      linkoffset_t *link;

      if(R_LinkedLineActive(ld) &&
         (link = P_GetLinkOffset(ld->frontsector->groupid, ld->portal->data.camera.groupid)))
      {
         // 1/11/98 killough: remove limit on lines hit, by array doubling
         if(tm->numspechit >= tm->spechit_max)
         {
            tm->spechit_max = tm->spechit_max ? tm->spechit_max * 2 : 8;
            tm->spechit = realloc(tm->spechit, sizeof(*tm->spechit) * tm->spechit_max);
         }
         tm->spechit[tm->numspechit++] = ld;

         // haleyjd 09/20/06: spechit overflow emulation
         if(demo_compatibility && spechits_emulation && 
            tm->numspechit > MAXSPECHIT_OLD)
            SpechitOverrun(ld);

         return true;
      }
#endif
      tm->blockline = ld;
      return tm->unstuck && !untouched(ld) &&
         FixedMul(tm->x-tm->thing->x,ld->dy) > FixedMul(tm->y-tm->thing->y,ld->dx);
   }

   // killough 8/10/98: allow bouncing objects to pass through as missiles
   if(!(tm->thing->flags & (MF_MISSILE | MF_BOUNCES)))
   {
      if(ld->flags & ML_BLOCKING)           // explicitly blocking everything
         return tm->unstuck && !untouched(ld);  // killough 8/1/98: allow escape

      // killough 8/9/98: monster-blockers don't affect friends
      // SoM 9/7/02: block monsters standing on 3dmidtex only
      if(!(tm->thing->flags & MF_FRIEND || tm->thing->player) && 
         ld->flags & ML_BLOCKMONSTERS && 
         !(ld->flags & ML_3DMIDTEX))
         return false; // block monsters only
   }

   // set openrange, opentop, openbottom
   // these define a 'window' from one sector to another across this line
   
   P_LineOpening(ld, tm->thing);

   // adjust floor & ceiling heights
   
   if(tm->opentop < tm->ceilingz)
   {
      tm->ceilingz = tm->opentop;
      tm->ceilingline = ld;
      tm->blockline = ld;
   }

   if(tm->openbottom > tm->floorz)
   {
      tm->floorz = tm->openbottom;

      tm->floorline = ld;          // killough 8/1/98: remember floor linedef
      tm->blockline = ld;
   }

   if(tm->lowfloor < tm->dropoffz)
      tm->dropoffz = tm->lowfloor;

   // haleyjd 11/10/04: 3DMidTex fix: never consider dropoffs when
   // touching 3DMidTex lines.
   if(demo_version >= 331 && tm->touch3dside)
      tm->dropoffz = tm->floorz;

   if(tm->opensecfloor > tm->secfloorz)
      tm->secfloorz = tm->opensecfloor;
   if(tm->opensecceil < tm->secceilz)
      tm->secceilz = tm->opensecceil;

   // SoM 11/6/02: AGHAH
   if(tm->floorz > tm->passfloorz)
      tm->passfloorz = tm->floorz;
   if(tm->ceilingz < tm->passceilz)
      tm->passceilz = tm->ceilingz;

   // if contacted a special line, add it to the list
   
   if(ld->special)
   {
      // 1/11/98 killough: remove limit on lines hit, by array doubling
      if(tm->numspechit >= tm->spechit_max)
      {
         tm->spechit_max = tm->spechit_max ? tm->spechit_max * 2 : 8;
         tm->spechit = realloc(tm->spechit, sizeof(*tm->spechit) * tm->spechit_max);
      }
      tm->spechit[tm->numspechit++] = ld;

      // haleyjd 09/20/06: spechit overflow emulation
      if(demo_compatibility && spechits_emulation && 
         tm->numspechit > MAXSPECHIT_OLD)
         SpechitOverrun(ld);
   }
   
   return true;
}

//
// P_Touched
//
// haleyjd: isolated code to test for and execute touchy thing death.
// Required for proper 3D clipping support.
//
boolean P_Touched(mobj_t *thing, mobj_t *tmthing)
{
   static int painType = -1, skullType;

   // EDF FIXME: temporary fix?
   if(painType == -1)
   {
      painType  = E_ThingNumForDEHNum(MT_PAIN);
      skullType = E_ThingNumForDEHNum(MT_SKULL);
   }

   if(thing->flags & MF_TOUCHY &&                  // touchy object
      tmthing->flags & MF_SOLID &&                 // solid object touches it
      thing->health > 0 &&                         // touchy object is alive
      (thing->intflags & MIF_ARMED ||              // Thing is an armed mine
       sentient(thing)) &&                         // ... or a sentient thing
      (thing->type != tmthing->type ||             // only different species
       thing->player) &&                           // ... or different players
      thing->z + thing->height >= tmthing->z &&    // touches vertically
      tmthing->z + tmthing->height >= thing->z &&
      (thing->type ^ painType) |                   // PEs and lost souls
      (tmthing->type ^ skullType) &&               // are considered same
      (thing->type ^ skullType) |                  // (but Barons & Knights
      (tmthing->type ^ painType))                  // are intentionally not)
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
boolean P_CheckPickUp(mobj_t *thing, mobj_t *tmthing)
{
   int solid = thing->flags & MF_SOLID;

   if(tm->flags & MF_PICKUP)
      P_TouchSpecialThing(thing, tmthing); // can remove thing

   return !solid;
}

//
// P_SkullHit
//
// haleyjd: isolated code to test for and execute SKULLFLY objects hitting
// things. Returns true if PIT_CheckThing should exit.
//
boolean P_SkullHit(mobj_t *thing, mobj_t *tmthing)
{
   boolean ret = false;

   if(tmthing->flags & MF_SKULLFLY)
   {
      // A flying skull is smacking something.
      // Determine damage amount, and the skull comes to a dead stop.

      int damage = (P_Random(pr_skullfly) % 8 + 1) * tmthing->damage;
      
      P_DamageMobj(thing, tmthing, tmthing, damage, MOD_UNKNOWN);
      
      tmthing->flags &= ~MF_SKULLFLY;
      tmthing->momx = tmthing->momy = tmthing->momz = 0;

      // haleyjd: Note that this is where potential for a recursive clipping
      // operation occurs -- P_SetMobjState causes a call to A_Look, which
      // causes another state-set to the seestate, which calls A_Chase.
      // A_Chase in turn calls P_TryMove and that can cause a lot of shit
      // to explode... fixing it in a compatible manner is nigh impossible.

      P_SetMobjState(tmthing, tmthing->info->spawnstate);

      tm->BlockingMobj = NULL; // haleyjd: from zdoom

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
int P_MissileBlockHeight(mobj_t *mo)
{
   return (demo_version >= 333 && !comp[comp_theights] &&
           mo->flags3 & MF3_3DDECORATION) ? mo->info->height : mo->height;
}

//
// PIT_CheckThing
// 
static boolean PIT_CheckThing(mobj_t *thing) // killough 3/26/98: make static
{
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

   // killough 11/98:
   //
   // TOUCHY flag, for mines or other objects which die on contact with solids.
   // If a solid object of a different type comes in contact with a touchy
   // thing, and the touchy thing is not the sole one moving relative to fixed
   // surroundings such as walls, then the touchy thing dies immediately.

   // haleyjd: functionalized
   if(P_Touched(thing, tm->thing))
      return true;

   // check for skulls slamming into things

   if(P_SkullHit(thing, tm->thing))
      return false;
   
   // missiles can hit other things
   // killough 8/10/98: bouncing non-solid things can hit other things too
   
   if(tm->thing->flags & MF_MISSILE || (tm->thing->flags & MF_BOUNCES &&
                                      !(tm->thing->flags & MF_SOLID)))
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
   
   if(demo_version >= 329 && thing->flags2 & MF2_PUSHABLE &&
      (demo_version < 331 || !(tm->thing->flags3 & MF3_CANNOTPUSH)))
   {
      // transfer one-fourth momentum along the x and y axes
      thing->momx += tm->thing->momx / 4;
      thing->momy += tm->thing->momy / 4;
   }

   // check for special pickup
   
   if(thing->flags & MF_SPECIAL)
      return P_CheckPickUp(thing, tm->thing);

   // killough 3/16/98: Allow non-solid moving objects to move through solid
   // ones, by allowing the moving thing (tm->thing) to move if it's non-solid,
   // despite another solid thing being in the way.
   // killough 4/11/98: Treat no-clipping things as not blocking

   return !((thing->flags & MF_SOLID && !(thing->flags & MF_NOCLIP))
          && (tm->thing->flags & MF_SOLID || demo_compatibility));

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
boolean Check_Sides(mobj_t *actor, int x, int y)
{
   int bx,by,xl,xh,yl,yh;
   
   pe_x = actor->x;
   pe_y = actor->y;
   ls_x = x;
   ls_y = y;

   // Here is the bounding box of the trajectory
   
   tm->bbox[BOXLEFT]   = pe_x < x ? pe_x : x;
   tm->bbox[BOXRIGHT]  = pe_x > x ? pe_x : x;
   tm->bbox[BOXTOP]    = pe_y > y ? pe_y : y;
   tm->bbox[BOXBOTTOM] = pe_y < y ? pe_y : y;

   // Determine which blocks to look in for blocking lines
   
   xl = (tm->bbox[BOXLEFT]   - bmaporgx)>>MAPBLOCKSHIFT;
   xh = (tm->bbox[BOXRIGHT]  - bmaporgx)>>MAPBLOCKSHIFT;
   yl = (tm->bbox[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
   yh = (tm->bbox[BOXTOP]    - bmaporgy)>>MAPBLOCKSHIFT;

   // xl->xh, yl->yh determine the mapblock set to search

   validcount++; // prevents checking same line twice
   for(bx = xl ; bx <= xh ; bx++)
      for (by = yl ; by <= yh ; by++)
         if(!P_BlockLinesIterator(bx,by,PIT_CrossLine))
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
//  a mobj_t (can be valid or invalid)
//  a position to be checked
//   (doesn't need to be related to the mobj_t->x,y)
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
boolean P_CheckPosition(mobj_t *thing, fixed_t x, fixed_t y) 
{
   int xl, xh, yl, yh, bx, by;
   subsector_t *newsubsec;

   // haleyjd: OVER_UNDER
   if(demo_version >= 331 && !comp[comp_overunder])
      return P_CheckPosition3D(thing, x, y);
   
   tm->thing = thing;
   tm->flags = thing->flags;
   
   tm->x = x;
   tm->y = y;
   
   tm->bbox[BOXTOP] = y + tm->thing->radius;
   tm->bbox[BOXBOTTOM] = y - tm->thing->radius;
   tm->bbox[BOXRIGHT] = x + tm->thing->radius;
   tm->bbox[BOXLEFT] = x - tm->thing->radius;
   
   newsubsec = R_PointInSubsector(x,y);
   tm->floorline = tm->blockline = tm->ceilingline = NULL; // killough 8/1/98

   // Whether object can get out of a sticky situation:
   tm->unstuck = thing->player &&          // only players
      thing->player->mo == thing &&       // not voodoo dolls
      demo_version >= 203;                // not under old demos

   // The base floor / ceiling is from the subsector
   // that contains the point.
   // Any contacted lines the step closer together
   // will adjust them.

   tm->floorz = tm->dropoffz = newsubsec->sector->floorheight;
   tm->ceilingz = newsubsec->sector->ceilingheight;

   tm->secfloorz = tm->passfloorz = tm->stepupfloorz = tm->floorz;
   tm->secceilz = tm->passceilz = tm->ceilingz;

   // haleyjd
   tm->floorpic = newsubsec->sector->floorpic;
   // SoM: 09/07/02: 3dsides monster fix
   tm->touch3dside = 0;
   validcount++;
   tm->numspechit = 0;

   if(tm->flags & MF_NOCLIP)
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

   for(bx = xl; bx <= xh; bx++)
   {
      for(by = yl; by <= yh; by++)
      {
         if(!P_BlockThingsIterator(bx, by, PIT_CheckThing))
            return false;
      }
   }

   // check lines
   
   tm->BlockingMobj = NULL; // haleyjd 1/17/00: global hit reference
   
   xl = (tm->bbox[BOXLEFT]   - bmaporgx) >> MAPBLOCKSHIFT;
   xh = (tm->bbox[BOXRIGHT]  - bmaporgx) >> MAPBLOCKSHIFT;
   yl = (tm->bbox[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
   yh = (tm->bbox[BOXTOP]    - bmaporgy) >> MAPBLOCKSHIFT;

   for(bx = xl; bx <= xh; bx++)
   {
      for(by = yl; by <= yh; by++)
      {
         if(!P_BlockLinesIterator(bx, by, PIT_CheckLine))
            return false; // doesn't fit
      }
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
boolean P_TryMove(mobj_t *thing, fixed_t x, fixed_t y, boolean dropoff)
{
   fixed_t oldx, oldy, oldz;
   
   // haleyjd 11/10/04: 3dMidTex: determine if a thing is on a line:
   // passfloorz is the floor as determined from sectors and 3DMidTex.
   // secfloorz is the floor as determined only from sectors.
   // If the two are unequal, passfloorz is the thing's floorz, and
   // the thing is at its floorz, then it is on a 3DMidTex line.
   // Complicated. x_x

   boolean on3dmidtex = (thing->passfloorz == thing->floorz &&
                         thing->passfloorz != thing->secfloorz &&
                         thing->z == thing->floorz);
   
   tm->felldown = tm->floatok = false;               // killough 11/98

   // haleyjd: OVER_UNDER
   if(demo_version >= 331 && !comp[comp_overunder])
   {
      oldz = thing->z;

      if(!P_CheckPosition3D(thing, x, y))
      {
         // Solid wall or thing
         if(!tm->BlockingMobj || tm->BlockingMobj->player || !thing->player)
            return false;
         else
         {
            // haleyjd: yikes...
            if(tm->BlockingMobj->player || !thing->player)
               return false;
            else if(tm->BlockingMobj->z + tm->BlockingMobj->height-thing->z > 24*FRACUNIT || 
                    (tm->BlockingMobj->subsector->sector->ceilingheight
                     - (tm->BlockingMobj->z + tm->BlockingMobj->height) < thing->height) ||
                    (tm->ceilingz - (tm->BlockingMobj->z + tm->BlockingMobj->height) 
                     < thing->height))
            {
               return false;
            }
            
            // haleyjd: hack for touchies: don't allow running through them when
            // they die until they become non-solid (just being a corpse isn't good 
            // enough)
            if(tm->BlockingMobj->flags & MF_TOUCHY)
            {
               if(tm->BlockingMobj->health <= 0)
                  return false;
            }
         }
         if(!(tm->thing->flags3 & MF3_PASSMOBJ))
         {
            thing->z = oldz;
            return false;
         }
      }
   }
   else if(!P_CheckPosition(thing, x, y))
      return false;   // solid wall or thing

   if(!(thing->flags & MF_NOCLIP))
   {
      boolean ret = tm->unstuck 
                    && !(tm->ceilingline && untouched(tm->ceilingline))
                    && !(  tm->floorline && untouched(  tm->floorline));

      // killough 7/26/98: reformatted slightly
      // killough 8/1/98: Possibly allow escape if otherwise stuck
      // haleyjd: OVER_UNDER: broke up impossible-to-understand predicate

      if(tm->ceilingz - tm->floorz < thing->height) // doesn't fit
         return ret;
         
      // mobj must lower to fit
      if((tm->floatok = true, !(thing->flags & MF_TELEPORT) &&
          tm->ceilingz - thing->z < thing->height))
         return ret;          

      if(!(thing->flags & MF_TELEPORT) && !(thing->flags3 & MF3_FLOORMISSILE))
      {
         // too big a step up
         if(tm->floorz - thing->z > 24*FRACUNIT)
            return ret;
         else if(demo_version >= 331 && !comp[comp_overunder] && thing->z < tm->floorz)
         { 
            // haleyjd: OVER_UNDER:
            // [RH] Check to make sure there's nothing in the way for the step up
            fixed_t savedz = thing->z;
            boolean good;
            thing->z = tm->floorz;
            good = P_TestMobjZ(thing);
            thing->z = savedz;
            if(!good)
               return false;
         }
      }
      
      // killough 3/15/98: Allow certain objects to drop off
      // killough 7/24/98, 8/1/98: 
      // Prevent monsters from getting stuck hanging off ledges
      // killough 10/98: Allow dropoffs in controlled circumstances
      // killough 11/98: Improve symmetry of clipping on stairs

      // haleyjd 11/10/04: 3dMidTex: eliminated old code
      if(!(thing->flags & (MF_DROPOFF|MF_FLOAT)))
      {
         // haleyjd 11/10/04: 3dMidTex: you are never allowed to pass 
         // over a tall dropoff when on 3DMidTex lines. Note tmfloorz
         // considers 3DMidTex, so you can still move across 3DMidTex
         // lines that pass over sector dropoffs, as long as the dropoff
         // between the 3DMidTex lines is <= 24 units.

         if(demo_version >= 331 && on3dmidtex)
         {
            // allow appropriate forced dropoff behavior
            if(!dropoff || 
               (dropoff == 2 && 
                (thing->z - tm->floorz > 128*FRACUNIT ||
                 !thing->target || thing->target->z > tm->floorz)))
            {
               // deny any move resulting in a difference > 24
               if(thing->z - tm->floorz > 24*FRACUNIT)
                  return false;
            }
            else  // dropoff allowed
            {
               tm->felldown = !(thing->flags & MF_NOGRAVITY) &&
                      thing->z - tm->floorz > 24*FRACUNIT;
            }
         }
         else if(comp[comp_dropoff])
         {
            if(tm->floorz - tm->dropoffz > 24*FRACUNIT)
               return false;                      // don't stand over a dropoff
         }
         else
         {
            fixed_t floorz = tm->floorz;

            // haleyjd: OVER_UNDER:
            // [RH] If the thing is standing on something, use its current z as 
            // the floorz. This is so that it does not walk off of things onto a 
            // drop off.
            if(demo_version >= 331 && !comp[comp_overunder] &&
               thing->intflags & MIF_ONMOBJ)
            {
               floorz = thing->z > tm->floorz ? thing->z : tm->floorz;
            }

            if(!dropoff || 
               (dropoff == 2 &&  // large jump down (e.g. dogs)
                (floorz - tm->dropoffz > 128*FRACUNIT || 
                 !thing->target || thing->target->z > tm->dropoffz)))
            {
               fixed_t zdist;

               if(!monkeys || demo_version < 203)
                  zdist = floorz - tm->dropoffz;
               else
                  zdist = thing->floorz - floorz;
               
               if(zdist > 24*FRACUNIT || 
                  thing->dropoffz - tm->dropoffz > 24*FRACUNIT)
                  return false;
            }
            else  // dropoff allowed -- check for whether it fell more than 24
            {
               tm->felldown = !(thing->flags & MF_NOGRAVITY) &&
                      thing->z - floorz > 24*FRACUNIT;
            }
         }
      }

      if(thing->flags & MF_BOUNCES &&    // killough 8/13/98
         !(thing->flags & (MF_MISSILE|MF_NOGRAVITY)) &&
         !sentient(thing) && tm->floorz - thing->z > 16*FRACUNIT)
      {
         return false; // too big a step up for bouncers under gravity
      }

      // killough 11/98: prevent falling objects from going up too many steps
      if(thing->intflags & MIF_FALLING && tm->floorz - thing->z >
         FixedMul(thing->momx,thing->momx)+FixedMul(thing->momy,thing->momy))
      {
         return false;
      }

      // haleyjd: CANTLEAVEFLOORPIC flag
      if((thing->flags2 & MF2_CANTLEAVEFLOORPIC) &&
         (tm->floorpic != thing->subsector->sector->floorpic ||
          tm->floorz - thing->z != 0))
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
   thing->floorz = tm->floorz;
   thing->ceilingz = tm->ceilingz;
   thing->dropoffz = tm->dropoffz;      // killough 11/98: keep track of dropoffs
   thing->passfloorz = tm->passfloorz;
   thing->passceilz = tm->passceilz;
   thing->secfloorz = tm->secfloorz;
   thing->secceilz = tm->secceilz;

   thing->x = x;
   thing->y = y;
   
   P_SetThingPosition(thing);

   // haleyjd 08/07/04: new footclip system
   P_AdjustFloorClip(thing);

   // if any special lines were hit, do the effect
   // killough 11/98: simplified
   if(!(thing->flags & (MF_TELEPORT | MF_NOCLIP)))
   {
      while(tm->numspechit--)
      {
#ifdef R_LINKEDPORTALS
         if(R_LinkedLineActive(tm->spechit[tm->numspechit]))
         {
            // SoM: if the mobj is touching a portal line, and the line is behind
            // the mobj no matter what the previous lineside was, we missed the 
            // teleport and NEED to do so now.
            if(P_PointOnLineSide(thing->x, thing->y, tm->spechit[tm->numspechit]))
            {
               linkoffset_t *link = 
                  P_GetLinkOffset(tm->spechit[tm->numspechit]->frontsector->groupid, 
                                  tm->spechit[tm->numspechit]->portal->data.camera.groupid);
               EV_PortalTeleport(thing, link);
            }
         }
#endif
         if(tm->spechit[tm->numspechit]->special)  // see if the line was crossed
         {
            int oldside;
            if((oldside = P_PointOnLineSide(oldx, oldy, tm->spechit[tm->numspechit])) !=
               P_PointOnLineSide(thing->x, thing->y, tm->spechit[tm->numspechit]))
               P_CrossSpecialLine(tm->spechit[tm->numspechit], oldside, thing);
         }
      }

      // haleyjd 01/09/07: do not leave numspechit == -1
      tm->numspechit = 0;
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
static boolean PIT_ApplyTorque(line_t *ld)
{
   if(ld->backsector &&       // If thing touches two-sided pivot linedef
      tm->bbox[BOXRIGHT]  > ld->bbox[BOXLEFT]  &&
      tm->bbox[BOXLEFT]   < ld->bbox[BOXRIGHT] &&
      tm->bbox[BOXTOP]    > ld->bbox[BOXBOTTOM] &&
      tm->bbox[BOXBOTTOM] < ld->bbox[BOXTOP] &&
      P_BoxOnLineSide(tm->bbox, ld) == -1)
   {
      mobj_t *mo = tm->thing;

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
void P_ApplyTorque(mobj_t *mo)
{
   int xl = ((tm->bbox[BOXLEFT] = 
              mo->x - mo->radius) - bmaporgx) >> MAPBLOCKSHIFT;
   int xh = ((tm->bbox[BOXRIGHT] = 
              mo->x + mo->radius) - bmaporgx) >> MAPBLOCKSHIFT;
   int yl = ((tm->bbox[BOXBOTTOM] =
              mo->y - mo->radius) - bmaporgy) >> MAPBLOCKSHIFT;
   int yh = ((tm->bbox[BOXTOP] = 
      mo->y + mo->radius) - bmaporgy) >> MAPBLOCKSHIFT;
   int bx,by,flags = mo->intflags; //Remember the current state, for gear-change

   tm->thing = mo;
   validcount++; // prevents checking same line twice
      
   for(bx = xl ; bx <= xh ; bx++)
      for(by = yl ; by <= yh ; by++)
         P_BlockLinesIterator(bx, by, PIT_ApplyTorque);
      
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
static boolean P_ThingHeightClip(mobj_t *thing)
{
   boolean onfloor = thing->z == thing->floorz;
   fixed_t oldfloorz = thing->floorz; // haleyjd

   P_CheckPosition(thing, thing->x, thing->y);
  
   // what about stranding a monster partially off an edge?
   // killough 11/98: Answer: see below (upset balance if hanging off ledge)
   
   thing->floorz = tm->floorz;
   thing->ceilingz = tm->ceilingz;
   thing->dropoffz = tm->dropoffz;         // killough 11/98: remember dropoffs

   thing->passfloorz = tm->passfloorz;
   thing->passceilz = tm->passceilz;
   thing->secfloorz = tm->secfloorz;
   thing->secceilz = tm->secceilz;

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
static mobj_t    *slidemo;
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
   boolean icyfloor;  // is floor icy?

   // phares:
   // Under icy conditions, if the angle of approach to the wall
   // is more than 45 degrees, then you'll bounce and lose half
   // your momentum. If less than 45 degrees, you'll slide along
   // the wall. 45 is arbitrary and is believable.
   //
   // Check for the special cases of horz or vert walls.

   // killough 10/98: only bounce if hit hard (prevents wobbling)
   icyfloor = 
      (demo_version >= 203 ? 
       P_AproxDistance(tmxmove, tmymove) > 4*FRACUNIT : !compatibility) &&
      variable_friction &&  // killough 8/28/98: calc friction on demand
      slidemo->z <= slidemo->floorz &&
      P_GetFriction(slidemo, NULL) > ORIG_FRICTION;

   if(ld->slopetype == ST_HORIZONTAL)
   {
      if(icyfloor && D_abs(tmymove) > D_abs(tmxmove))
      {
         // haleyjd: only the player should oof
         if(slidemo->player)
            S_StartSound(slidemo, gameModeInfo->playerSounds[sk_oof]); // oooff!
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
            S_StartSound(slidemo, gameModeInfo->playerSounds[sk_oof]); // oooff!
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
   
   lineangle = R_PointToAngle2 (0,0, ld->dx, ld->dy);
   if(side == 1)
      lineangle += ANG180;
   moveangle = R_PointToAngle2 (0,0, tmxmove, tmymove);

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
         S_StartSound(slidemo, gameModeInfo->playerSounds[sk_oof]); // oooff!
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
static boolean PTR_SlideTraverse(intercept_t *in)
{
   line_t *li;
   
#ifdef RANGECHECK
   if(!in->isaline)
      I_Error ("PTR_SlideTraverse: not a line?");
#endif

   li = in->d.line;
   
   if(!(li->flags & ML_TWOSIDED))
   {
      if(P_PointOnLineSide (slidemo->x, slidemo->y, li))
         return true; // don't hit the back side
      goto isblocking;
   }

   // set openrange, opentop, openbottom.
   // These define a 'window' from one sector to another across a line
   
   P_LineOpening(li, slidemo);
   
   if(tm->openrange < slidemo->height)
      goto isblocking;  // doesn't fit
   
   if(tm->opentop - slidemo->z < slidemo->height)
      goto isblocking;  // mobj is too high
   
   if(tm->openbottom - slidemo->z > 24*FRACUNIT )
      goto isblocking;  // too big a step up
   else if(demo_version >= 331 && !comp[comp_overunder] &&
           slidemo->z < tm->openbottom) // haleyjd: OVER_UNDER
   { 
      // [RH] Check to make sure there's nothing in the way for the step up
      boolean good;
      fixed_t savedz = slidemo->z;
      slidemo->z = tm->openbottom;
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
void P_SlideMove(mobj_t *mo)
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
         leadx = mo->x + mo->radius, trailx = mo->x - mo->radius;
      else
         leadx = mo->x - mo->radius, trailx = mo->x + mo->radius;

      if(mo->momy > 0)
         leady = mo->y + mo->radius, traily = mo->y - mo->radius;
      else
         leady = mo->y - mo->radius, traily = mo->y + mo->radius;

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

//=============================================================================
//
// Line Attacks
//

static mobj_t *shootthing;

static int aim_flags_mask; // killough 8/2/98: for more intelligent autoaiming

// SoM: Moved globals into a structure. See p_maputl.h

#ifdef R_LINKEDPORTALS
static tptnode_t *tptlist = NULL, *tptend = NULL, *tptunused = NULL;

//
// TPT_NewNode
//
static tptnode_t *TPT_NewNode(void)
{
   tptnode_t *ret;

   // Make a new node or unlink an existing node from the unused list.
   if(!tptunused)
      ret = (tptnode_t *)Z_Malloc(sizeof(tptnode_t), PU_STATIC, 0);
   else
   {
      ret = tptunused;
      tptunused = tptunused->next;
   }

   // Link it to the end of the list.
   if(!tptlist)
      tptlist = tptend = ret;
   else
   {
      tptend->next = ret;
      tptend = ret;
   }

   ret->next = NULL;
   return ret;
}

//
// P_NewShootTPT
//
void P_NewShootTPT(linkoffset_t *link, fixed_t frac, fixed_t newz)
{
   tptnode_t *node = TPT_NewNode();

   node->type = tShoot;

   node->x = trace.x + FixedMul(frac, trace.cos);
   //node->dx = trace.dx - (node->x - trace.x);
   node->x -= link->x;

   node->y = trace.y + FixedMul(frac, trace.sin);
   //node->dy = trace.dy - (node->y - trace.y);
   node->y -= link->y;

   node->z = newz - link->z;

   node->originx = trace.originx - link->x;
   node->originy = trace.originy - link->y;
   node->originz = trace.originz - link->z;

   node->movefrac = trace.movefrac + frac;
   node->attackrange = trace.attackrange - frac;
   node->dx = FixedMul(trace.attackrange, trace.cos);
   node->dy = FixedMul(trace.attackrange, trace.sin);
}

//
// P_NewAimTPT
//
void P_NewAimTPT(linkoffset_t *link, fixed_t frac, fixed_t newz, 
                 fixed_t newtopslope, fixed_t newbottomslope)
{
   tptnode_t *node = TPT_NewNode();

   node->type = tAim;

   node->x = trace.x + FixedMul(frac, trace.cos);
   //node->dx = trace.dx - (node->x - trace.x);
   node->x -= link->x;

   node->y = trace.y + FixedMul(frac, trace.sin);
   //node->dy = trace.dy - (node->y - trace.y);
   node->y -= link->y;

   node->z = newz - link->z;

   node->originx = trace.originx - link->x;
   node->originy = trace.originy - link->y;
   node->originz = trace.originz - link->z;

   node->movefrac = trace.movefrac + frac;
   node->attackrange = trace.attackrange - frac;
   node->dx = FixedMul(trace.cos, node->attackrange);
   node->dy = FixedMul(trace.sin, node->attackrange);

   node->topslope = newtopslope;
   node->bottomslope = newbottomslope;
}

//
// P_NewUseTPT
//
void P_NewUseTPT(linkoffset_t *link, fixed_t frac)
{
   tptnode_t *node = TPT_NewNode();

   node->type = tUse;

   node->x = trace.x + FixedMul(frac, trace.cos);
   node->x -= link->x;

   node->y = trace.y + FixedMul(frac, trace.sin);
   node->y -= link->y;

   node->originx = trace.originx - link->x;
   node->originy = trace.originy - link->y;
   node->originz = trace.originz - link->x;

   node->movefrac = trace.movefrac + frac;
   node->attackrange = trace.attackrange - frac;
   node->dx = FixedMul(trace.cos, node->attackrange);
   node->dy = FixedMul(trace.sin, node->attackrange);
}

//
// P_CheckTPT
//
// Returns true if there are any TPT nodes on the list, false otherwise.
//
boolean P_CheckTPT(void)
{
   return (tptlist != NULL);
}

//
// P_StartTPT
//
tptnode_t *P_StartTPT(void)
{
   tptnode_t *ret = tptlist;
   if(!ret)
      return NULL;

   tptlist = ret->next;

   if(!tptlist)
      tptend = NULL;
   else
      ret->next = NULL;

   if(ret->type == tShoot)
   {
      trace.x = ret->x;
      trace.y = ret->y;
      trace.z = ret->z;
      trace.dx = ret->dx;
      trace.dy = ret->dy;
      trace.originx = ret->originx;
      trace.originy = ret->originy;
      trace.originz = ret->originz;
      trace.movefrac = ret->movefrac;
      trace.attackrange = ret->attackrange;

      return ret;
   }
   else if(ret->type == tAim)
   {
      trace.x = ret->x;
      trace.y = ret->y;
      trace.z = ret->z;
      trace.dx = ret->dx;
      trace.dy = ret->dy;
      trace.originx = ret->originx;
      trace.originy = ret->originy;
      trace.originz = ret->originz;
      trace.topslope = ret->topslope;
      trace.bottomslope = ret->bottomslope;
      trace.movefrac = ret->movefrac;
      trace.attackrange = ret->attackrange;

      return ret;
   }
   else if(ret->type == tUse)
   {
      trace.x = ret->x;
      trace.y = ret->y;
      trace.z = ret->z;
      trace.dx = ret->dx;
      trace.dy = ret->dy;
      trace.originx = ret->originx;
      trace.originy = ret->originy;
      trace.originz = ret->originz;
      trace.movefrac = ret->movefrac;
      trace.attackrange = ret->attackrange;

      return ret;
   }

   I_Error("P_StartTPT: Node had invalid type value %i\n", (int)ret->type);
   return NULL;
}

//
// P_FinishTPT
//
// Puts a TPT node onto the free list.
//
void P_FinishTPT(tptnode_t *node)
{
   // Link the node back into the unused list
   node->next = tptunused;
   tptunused = node;
}

//
// P_ClearTPT
//
// Call this after you use TPT!
//
void P_ClearTPT(void)
{
   tptnode_t *rover;

   while((rover = tptlist))
   {
      tptlist = rover->next;

      rover->next = tptunused;
      tptunused = rover;
   }

   tptlist = tptend = NULL;
}

#endif

//
// PTR_AimTraverse
// Sets linetaget and aimslope when a target is aimed at.
//
#ifdef R_LINKEDPORTALS
static boolean PTR_AimTraverse(intercept_t *in)
{
   fixed_t slope, thingtopslope, thingbottomslope, dist;
   line_t *li;
   mobj_t *th;
   sector_t *sidesector = NULL;
   int      lineside;
   
   if(in->isaline)
   {
      li = in->d.line;
      
      dist = FixedMul (trace.attackrange, in->frac);

      if(demo_version >= 333 && useportalgroups)
      {
         dist += trace.movefrac;
         lineside = P_PointOnLineSide(trace.x, trace.y, li);
         sidesector = lineside ? li->backsector : li->frontsector; 

         // Marked twosided but really one sided?      
         if(!sidesector)
            return false;
      }

      if(!(li->flags & ML_TWOSIDED))
         goto blockedsolid;   // stop

      // Crosses a two sided line.
      // A two sided line will restrict
      // the possible target ranges.

      P_LineOpening (li, NULL);
      
      if(tm->openbottom >= tm->opentop)
         goto blockedsolid;   // stop

      // Check the portals, even if the line doesn't block the tracer, a/the 
      // target may be sitting on top of a ledge. If we don't hit any monsters,
      // we'll need to continue looking through portals, so store the 
      // intersection.
      if(demo_version >= 333 && R_LinkedCeilingActive(sidesector) &&  
         trace.originz <= sidesector->ceilingheight &&
         li->frontsector->c_portal != li->backsector->c_portal)
      {
         slope = FixedDiv(sidesector->ceilingheight - trace.originz, dist);
         if(trace.topslope > slope)
         {
            // Find the distance from the ogrigin to the intersection with the 
            // plane.
            fixed_t z = sidesector->ceilingheight;
            fixed_t frac = FixedDiv(z - trace.z, trace.topslope);
            linkoffset_t *link = 
               P_GetLinkOffset(sidesector->groupid, 
                               sidesector->c_portal->data.camera.groupid);
            if(link)
               P_NewAimTPT(link, frac, z, trace.topslope, slope);
         }
      }

      if(demo_version >= 333 && R_LinkedFloorActive(sidesector) &&
         trace.originz >= sidesector->floorheight &&
         li->frontsector->f_portal != li->backsector->f_portal)
      {
         slope = FixedDiv(sidesector->floorheight - trace.originz, dist);
         if(slope > trace.bottomslope)
         {
            // Find the distance from the ogrigin to the intersection with the 
            // plane.
            fixed_t z = sidesector->floorheight;
            fixed_t frac = FixedDiv(z - trace.z, trace.bottomslope);
            linkoffset_t *link = 
               P_GetLinkOffset(sidesector->groupid, 
                               sidesector->f_portal->data.camera.groupid);
            if(link)
               P_NewAimTPT(link, frac, z, slope, trace.bottomslope);
         }
      }

      if((li->frontsector->floorheight != li->backsector->floorheight || 
            (demo_version >= 333 && useportalgroups && (R_LinkedFloorActive(li->frontsector) ||
            R_LinkedFloorActive(li->backsector)) && 
            li->frontsector->f_portal != li->backsector->f_portal)) &&
         (demo_version < 333 || 
            (useportalgroups && sidesector && sidesector->floorheight <= trace.originz)))
      {
         slope = FixedDiv (tm->openbottom - trace.originz , dist);
         if(slope > trace.bottomslope)
            trace.bottomslope = slope;
      }

      if((li->frontsector->ceilingheight != li->backsector->ceilingheight ||
            (demo_version >= 333 && useportalgroups && (R_LinkedCeilingActive(li->frontsector) ||
            R_LinkedCeilingActive(li->backsector)) && 
            li->frontsector->c_portal != li->backsector->c_portal)) &&
         (demo_version < 333 || 
            (useportalgroups && sidesector && sidesector->ceilingheight >= trace.originz)))
      {
         slope = FixedDiv (tm->opentop - trace.originz , dist);
         if(slope < trace.topslope)
            trace.topslope = slope;
      }

      if(trace.topslope <= trace.bottomslope)
         return false;   // stop
      
      return true;    // shot continues

      blockedsolid:
      if(demo_version < 333 || !li->frontsector || !useportalgroups)
         return false;

      // The line is blocking so check for portals in the frontsector.
      if(R_LinkedCeilingActive(li->frontsector) &&
         trace.originz <= li->frontsector->ceilingheight)
      {
         slope = FixedDiv(li->frontsector->ceilingheight - trace.originz, dist);
         if(trace.topslope > slope)
         {
            // Find the distance from the ogrigin to the intersection with the 
            // plane.
            fixed_t z = li->frontsector->ceilingheight;
            fixed_t frac = FixedDiv(z - trace.z, trace.topslope);
            linkoffset_t *link = 
               P_GetLinkOffset(li->frontsector->groupid, 
                               li->frontsector->c_portal->data.camera.groupid);
            if(link)
               P_NewAimTPT(link, frac, z, trace.topslope, slope);
         }
      }

      if(R_LinkedFloorActive(li->frontsector) &&
         trace.originz >= li->frontsector->floorheight)
      {
         slope = FixedDiv(li->frontsector->floorheight - trace.originz, dist);
         if(slope > trace.bottomslope)
         {
            // Find the distance from the origin to the intersection with the 
            // plane.
            fixed_t z = li->frontsector->floorheight;
            fixed_t frac = FixedDiv(z - trace.z, trace.bottomslope);
            linkoffset_t *link = 
               P_GetLinkOffset(li->frontsector->groupid, 
                               li->frontsector->f_portal->data.camera.groupid);
            if(link)
               P_NewAimTPT(link, frac, z, slope, trace.bottomslope);
         }
      }

      // also check line portals.
      if(R_LinkedLineActive(li))
      {
         fixed_t slope2;

         slope = FixedDiv(li->frontsector->floorheight - trace.originz, dist);
         slope2 = FixedDiv(li->frontsector->ceilingheight - trace.originz, dist);
         if(slope2 < trace.bottomslope && slope > trace.topslope)
            return false;
         else
         {
            fixed_t frac = dist - trace.movefrac + FRACUNIT;
            linkoffset_t *link = 
               P_GetLinkOffset(li->frontsector->groupid, 
                               li->portal->data.camera.groupid);
            if(slope2 > trace.topslope)
               slope2 = trace.topslope;
            if(slope < trace.bottomslope)
               slope = trace.bottomslope;

            if(link)
               P_NewAimTPT(link, frac, trace.z, slope2, slope);
         }
      }

      return false;
   }

   // shoot a thing
   
   th = in->d.thing;
   if(th == shootthing)
      return true;    // can't shoot self

   if(!(th->flags&MF_SHOOTABLE))
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

   // SoM: From the origin
   thingtopslope = FixedDiv(th->z + th->height - trace.originz, dist); 

   if(thingtopslope < trace.bottomslope)
      return true;    // shot over the thing
   
   thingbottomslope = FixedDiv (th->z - trace.originz, dist);
   
   if(thingbottomslope > trace.topslope)
      return true;    // shot under the thing

   // this thing can be hit!
   
   if(thingtopslope > trace.topslope)
      thingtopslope = trace.topslope;
   
   if(thingbottomslope < trace.bottomslope)
      thingbottomslope = trace.bottomslope;
   
   trace.aimslope = (thingtopslope+thingbottomslope)/2;
   tm->linetarget = th;

   // We hit a thing so stop any furthur TPTs
   trace.finished = true;
   
   return false;   // don't go any farther
}

#else

static boolean PTR_AimTraverse(intercept_t *in)
{
   fixed_t slope, thingtopslope, thingbottomslope, dist;
   line_t *li;
   mobj_t *th;
   
   if(in->isaline)
   {
      li = in->d.line;
      
      if(!(li->flags & ML_TWOSIDED))
         return false;   // stop

      // Crosses a two sided line.
      // A two sided line will restrict
      // the possible target ranges.
      
      P_LineOpening (li, NULL);
      
      if(openbottom >= opentop)
         return false;   // stop

      dist = FixedMul (trace.attackrange, in->frac);
      
      if(li->frontsector->floorheight != li->backsector->floorheight)
      {
         slope = FixedDiv (openbottom - trace.z , dist);
         if(slope > trace.bottomslope)
            trace.bottomslope = slope;
      }

      if(li->frontsector->ceilingheight != li->backsector->ceilingheight)
      {
         slope = FixedDiv (opentop - trace.z , dist);
         if(slope < trace.topslope)
            trace.topslope = slope;
      }

      if(trace.topslope <= trace.bottomslope)
         return false;   // stop
      
      return true;    // shot continues
   }

   // shoot a thing
   
   th = in->d.thing;
   if(th == shootthing)
      return true;    // can't shoot self

   if(!(th->flags&MF_SHOOTABLE))
      return true;    // corpse or something

   // killough 7/19/98, 8/2/98:
   // friends don't aim at friends (except players), at least not first
   if(th->flags & shootthing->flags & aim_flags_mask && !th->player)
      return true;

   // check angles to see if the thing can be aimed at
   
   dist = FixedMul(trace.attackrange, in->frac);
   thingtopslope = FixedDiv(th->z+th->height - trace.z , dist);

   if(thingtopslope < trace.bottomslope)
      return true;    // shot over the thing
   
   thingbottomslope = FixedDiv (th->z - trace.z, dist);
   
   if(thingbottomslope > trace.topslope)
      return true;    // shot under the thing

   // this thing can be hit!
   
   if(thingtopslope > trace.topslope)
      thingtopslope = trace.topslope;
   
   if(thingbottomslope < trace.bottomslope)
      thingbottomslope = trace.bottomslope;
   
   trace.aimslope = (thingtopslope+thingbottomslope)/2;
   linetarget = th;
   
   return false;   // don't go any farther
}

#endif

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

   if((floorsame   || FixedDiv(tm->openbottom - trace.z , dist) <= trace.aimslope) &&
      (ceilingsame || FixedDiv(tm->opentop - trace.z , dist) >= trace.aimslope))
   {
      if(li->special && demo_version >= 329 && !comp[comp_planeshoot])
         P_ShootSpecialLine(shootthing, li, side);
      
      return true;      // shot continues
   }

   return false;
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
   fixed_t dist, thingtopslope, thingbottomslope, x, y, z, frac;
   mobj_t *th;
   boolean hitplane = false; // SoM: Remember if the bullet hit a plane.
   int updown = 2; // haleyjd 05/02: particle puff z dist correction
   
   if(in->isaline)
   {
      line_t *li = in->d.line;

      // haleyjd 03/13/05: move up point on line side check to here
      // SoM: this was causing some trouble when shooting through linked 
      // portals because although trace.x and trace.y are offset when 
      // the shot travels through a portal, shootthing->x and 
      // shootthing->y are NOT. Demo comped just in case.
      int lineside;

      if(demo_version >= 333)
         lineside = P_PointOnLineSide(trace.x, trace.y, li);
      else
         lineside = P_PointOnLineSide(shootthing->x, shootthing->y, li);
      
      // SoM: Shouldn't be called until A: we know the bullet passed or
      // B: We know it didn't hit a plane first
      if(li->special && (demo_version < 329 || comp[comp_planeshoot]))
         P_ShootSpecialLine(shootthing, li, lineside);
      
      if(li->flags & ML_TWOSIDED)
      {  
         // crosses a two sided (really 2s) line
         P_LineOpening(li, NULL);
         dist = FixedMul(trace.attackrange, in->frac);
         
         // killough 11/98: simplify
         // haleyjd 03/13/05: fixed bug that activates 2S line specials
         // when shots hit the floor
         if(P_Shoot2SLine(li, lineside, dist))
            return true;
      }
      
      // hit line
      // position a bit closer

      frac = in->frac - FixedDiv(4*FRACUNIT, trace.attackrange);
      x = trace.x + FixedMul(trace.dx, frac);
      y = trace.y + FixedMul(trace.dy, frac);
      z = trace.z + FixedMul(trace.aimslope, FixedMul(frac, trace.attackrange));

      if(demo_version >= 329 && !comp[comp_planeshoot])
      {
         // SoM: Check for colision with a plane.
         sector_t*  sidesector = lineside ? li->backsector : li->frontsector;
         fixed_t    zdiff;

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

               // SoM: Check here for portals
               if(sidesector->f_portal)
               {
#ifdef R_LINKEDPORTALS
                  if(R_LinkedFloorActive(sidesector))
                  {
                     linkoffset_t *link = 
                        P_GetLinkOffset(sidesector->groupid, 
                                        sidesector->f_portal->data.camera.groupid);
                     if(link)
                        P_NewShootTPT(link, pfrac, sidesector->floorheight);
                  }
#endif
                  return false;
               }

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
               if(sidesector->ceilingpic == skyflatnum ||
                  sidesector->ceilingpic == sky2flatnum) // SoM
                  return false;

               // SoM: Check here for portals
               if(sidesector->c_portal)
               {
#ifdef R_LINKEDPORTALS
                  if(R_LinkedCeilingActive(sidesector))
                  {
                     linkoffset_t *link = 
                        P_GetLinkOffset(sidesector->groupid, 
                                        sidesector->c_portal->data.camera.groupid);
                     if(link)
                        P_NewShootTPT(link, pfrac, sidesector->ceilingheight);
                  }
#endif
                  return false;
               }

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
#ifdef R_LINKEDPORTALS
            else if(R_LinkedLineActive(li))
            {
               linkoffset_t *link = 
                  P_GetLinkOffset(sidesector->groupid, 
                                  li->portal->data.camera.groupid);

               // SoM: Hit the center of the line
               // check for line-side portals
               // Do NOT position closer heh
               frac = FixedMul(in->frac, trace.attackrange); 
               z = trace.z + FixedMul(trace.aimslope, frac);
               if(link)
                  P_NewShootTPT(link, frac, z);

               return false;
            }
         }
         else if(demo_version >= 333 && R_LinkedLineActive(li))
            return true;
#else
         }
#endif

         if(!hitplane && li->special)
            P_ShootSpecialLine(shootthing, li, lineside);
      }

      if(li->frontsector->ceilingpic == skyflatnum ||
         li->frontsector->ceilingpic == sky2flatnum || 
         li->frontsector->c_portal)
      {
         // don't shoot the sky!
         // don't shoot ceiling portals either
         
         if(z > li->frontsector->ceilingheight)
            return false;
         
         // it's a sky hack wall
         // fix bullet-eaters -- killough:
         if(li->backsector && 
            (li->backsector->ceilingpic == skyflatnum ||
             li->backsector->ceilingpic == sky2flatnum))
         {
            if(demo_compatibility || li->backsector->ceilingheight < z)
               return false;
         }
      }
      
      // don't shoot portal lines
      if(!hitplane && li->portal)
         return false;
      
      // Spawn bullet puffs.
      P_SpawnPuff(x, y, z, 
                  R_PointToAngle2(0, 0, li->dx, li->dy) - ANG90,
                  updown, true);
      
      // don't go any farther
      
      return false;
   }
   
   // shoot a thing
   
   th = in->d.thing;
   if(th == shootthing)
      return true;  // can't shoot self
   
   if(!(th->flags & MF_SHOOTABLE))
      return true;  // corpse or something

   // haleyjd: don't let players use melee attacks on ghosts
   if(th->flags3 & MF3_GHOST && shootthing->player &&
      P_GetReadyWeapon(shootthing->player)->flags & WPF_NOHITGHOSTS)
   {
      return true;
   }
   
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
   
   frac = in->frac - FixedDiv(10*FRACUNIT, trace.attackrange);
   
   x = trace.x + FixedMul(trace.dx, frac);
   y = trace.y + FixedMul(trace.dy, frac);
   z = trace.z + FixedMul(trace.aimslope, FixedMul(frac, trace.attackrange));
   
   // Spawn bullet puffs or blood spots,
   // depending on target type. -- haleyjd: and status flags!
   if(th->flags & MF_NOBLOOD || 
      th->flags2 & (MF2_INVULNERABLE | MF2_DORMANT))
   {
      P_SpawnPuff(x, y, z, 
                  R_PointToAngle2(0, 0, trace.dx, trace.dy) - ANG180,
                  2, true);
   }
   else
   {
      P_SpawnBlood(x, y, z,
                   R_PointToAngle2(0, 0, trace.dx, trace.dy) - ANG180,
                   trace.la_damage, th);
   }
   
   if(trace.la_damage)
   {
      P_DamageMobj(th, shootthing, shootthing, trace.la_damage, 
                   shootthing->info->mod);
   }

   // SoM: we hit a thing!
   trace.finished = true;
   
   // don't go any further
   return false;
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
      trace.topslope = 100*FRACUNIT/160;
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
   tm->linetarget = NULL;

   // killough 8/2/98: prevent friends from aiming at friends
   aim_flags_mask = mask;
   
   P_PathTraverse(t1->x, t1->y, x2, y2, PT_ADDLINES|PT_ADDTHINGS, PTR_AimTraverse);
   
   return tm->linetarget ? trace.aimslope : lookslope;
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
   
   angle >>= ANGLETOFINESHIFT;
   shootthing = t1;
   trace.la_damage = damage;
   x2 = t1->x + (distance >> FRACBITS) * (trace.cos = finecosine[angle]);
   y2 = t1->y + (distance >> FRACBITS) * (trace.sin = finesine[angle]);
   
   trace.originz = trace.z = t1->z - t1->floorclip + (t1->height>>1) + 8*FRACUNIT;
   trace.attackrange = distance;
   trace.aimslope = slope;
   trace.movefrac = 0;

   P_PathTraverse(t1->x, t1->y, x2, y2, PT_ADDLINES|PT_ADDTHINGS, 
                  PTR_ShootTraverse);
}

//
// USE LINES
//

static mobj_t *usething;

// killough 11/98: reformatted
// haleyjd  09/02: reformatted again.

static boolean PTR_UseTraverse(intercept_t *in)
{
#ifdef R_LINKEDPORTALS
   if(R_LinkedLineActive(in->d.line))
   {
      linkoffset_t *link;
      sector_t *sidesector;
      int side = P_PointOnLineSide(trace.originx, trace.originy, in->d.line);

      if(side)
         return true;
      sidesector = in->d.line->frontsector;
      if(!sidesector)
         return true;

      link = P_GetLinkOffset(sidesector->groupid, 
                             in->d.line->portal->data.camera.groupid);
      if(!link)
         return false;

      P_NewUseTPT(link, in->frac);
      return false;
   }
   else
#endif
   if(in->d.line->special)
   {
      P_UseSpecialLine(usething, in->d.line,
         P_PointOnLineSide(trace.originx, trace.originy,in->d.line)==1);

      //WAS can't use for than one special line in a row
      //jff 3/21/98 NOW multiple use allowed with enabling line flag
      return (!demo_compatibility && in->d.line->flags & ML_PASSUSE);
   }
   else
   {
      P_LineOpening(in->d.line, NULL);
      if(tm->openrange <= 0)
      {
         // can't use through a wall
         // PCLASS_FIXME: add noway sound to skins
         S_StartSound(usething, gameModeInfo->playerSounds[sk_oof]);
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
        tm->openrange <= 0 ||                         // No opening
        tm->openbottom > usething->z+24*FRACUNIT ||   // Too high it blocks
        tm->opentop < usething->z+usething->height)); // Too low it blocks
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
      if(!P_PathTraverse(x1, y1, x2, y2, PT_ADDLINES, PTR_NoWayTraverse))
         // PCLASS_FIXME: add noway sound to skins
         S_StartSound(usething, gameModeInfo->playerSounds[sk_oof]);
}

//
// RADIUS ATTACK
//

static mobj_t *bombsource, *bombspot;
static int bombdamage;
static int bombmod; // haleyjd 07/13/03

//
// PIT_RadiusAttack
//
// "bombsource" is the creature that caused the explosion at "bombspot".
//
static boolean PIT_RadiusAttack(mobj_t *thing)
{
   fixed_t dx, dy, dist;

   // EDF FIXME: temporary solution
   static int cyberType = -1;
   
   if(cyberType == -1)
      cyberType = E_ThingNumForDEHNum(MT_CYBORG);
   
   // killough 8/20/98: allow bouncers to take damage 
   // (missile bouncers are already excluded with MF_NOBLOCKMAP)
   
   if(!(thing->flags & (MF_SHOOTABLE | MF_BOUNCES)))
      return true;
   
   // Boss spider and cyborg
   // take no damage from concussion.
   
   // killough 8/10/98: allow grenades to hurt anyone, unless
   // fired by Cyberdemons, in which case it won't hurt Cybers.
   // haleyjd 05/22/99: exclude all bosses

   if(bombspot->flags & MF_BOUNCES ?
      thing->type == cyberType && bombsource->type == cyberType :
      thing->flags2 & MF2_BOSS)
      return true;

   dx = D_abs(thing->x - bombspot->x);
   dy = D_abs(thing->y - bombspot->y);
   dist = dx>dy ? dx : dy;
   dist = (dist - thing->radius) >> FRACBITS;

   if(dist < 0)
      dist = 0;

   if(dist >= bombdamage)
      return true;  // out of range

   if(P_CheckSight(thing, bombspot))      // must be in direct path
      P_DamageMobj(thing, bombspot, bombsource, bombdamage - dist, bombmod);
   
   return true;
}

//
// P_RadiusAttack
//
// Source is the creature that caused the explosion at spot.
//   haleyjd 07/13/03: added method of death flag
//
void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage, int mod)
{
   fixed_t  dist = (damage + MAXRADIUS) << FRACBITS;
   int yh = (spot->y + dist - bmaporgy) >> MAPBLOCKSHIFT;
   int yl = (spot->y - dist - bmaporgy) >> MAPBLOCKSHIFT;
   int xh = (spot->x + dist - bmaporgx) >> MAPBLOCKSHIFT;
   int xl = (spot->x - dist - bmaporgx) >> MAPBLOCKSHIFT;
   int x, y;

   bombspot = spot;
   bombsource = source;
   bombdamage = damage;
   bombmod = mod;       // haleyjd
   
   for(y = yl; y <= yh; ++y)
      for(x = xl; x <= xh; ++x)
         P_BlockThingsIterator(x, y, PIT_RadiusAttack);
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
static boolean PIT_ChangeSector(mobj_t *thing)
{
   mobj_t *mo;

   if(P_ThingHeightClip(thing))
      return true; // keep checking
   
   // crunch bodies to giblets
   if(thing->health <= 0)
   {
      // sf: clear the skin which will mess things up
      // haleyjd 03/11/03: only in Doom
      if(gameModeInfo->type == Game_DOOM)
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
      P_RemoveMobj(thing);
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

   nofit = true;
   
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
   }

   // keep checking (crush other things)
   return true;
}

//
// P_ChangeSector
//
// haleyjd: removed static; needed in p_floor.c
//
boolean P_ChangeSector(sector_t *sector, int crunch)
{
   int x, y;
   
   nofit = false;
   crushchange = crunch;
   
   // ARRGGHHH!!!!
   // This is horrendously slow!!!
   // killough 3/14/98
   
   // re-check heights for all things near the moving sector

   for(x=sector->blockbox[BOXLEFT] ; x<= sector->blockbox[BOXRIGHT] ; x++)
      for(y=sector->blockbox[BOXBOTTOM];y<= sector->blockbox[BOXTOP] ; y++)
         P_BlockThingsIterator(x, y, PIT_ChangeSector);
      
   return nofit;
}

//
// P_CheckSector
// jff 3/19/98 added to just check monsters on the periphery
// of a moving sector instead of all in bounding box of the
// sector. Both more accurate and faster.
//
// haleyjd: OVER_UNDER: pass down more information to P_ChangeSector3D
// when 3D object clipping is enabled.
//
boolean P_CheckSector(sector_t *sector, int crunch, int amt, int floorOrCeil)
{
   msecnode_t *n;
   
   // killough 10/98: sometimes use Doom's method
   if(comp[comp_floors] && (demo_version >= 203 || demo_compatibility))
      return P_ChangeSector(sector, crunch);

   // haleyjd: call down to P_ChangeSector3D instead.
   if(demo_version >= 331 && !comp[comp_overunder])
      return P_ChangeSector3D(sector, crunch, amt, floorOrCeil);
   
   nofit = false;
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
               PIT_ChangeSector(n->m_thing); // process it
            break;                           // exit and start over
         }
      }
   }
   while(n); // repeat from scratch until all things left are marked valid
   
   return nofit;
}

// phares 3/21/98
//
// Maintain a freelist of msecnode_t's to reduce memory allocs and frees.

msecnode_t *headsecnode = NULL;

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
// P_FreeSecNodeList
//
void P_FreeSecNodeList(void)
{
   headsecnode = NULL; // this is all thats needed to fix the bug
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
      Z_Malloc(sizeof *node, PU_LEVEL, NULL); 
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
static msecnode_t *P_AddSecnode(sector_t *s, mobj_t *thing, msecnode_t *nextnode)
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
   node->m_tprev  = NULL;      // prev node on Thing thread
   node->m_tnext  = nextnode;  // next node on Thing thread
   
   if(nextnode)
      nextnode->m_tprev = node; // set back link on Thing

   // Add new node at head of sector thread starting at s->touching_thinglist
   
   node->m_sprev  = NULL;    // prev node on sector thread
   node->m_snext  = s->touching_thinglist; // next node on sector thread
   if(s->touching_thinglist)
      node->m_snext->m_sprev = node;
   return s->touching_thinglist = node;
}

//
// P_DelSecnode
//
// Deletes a sector node from the list of sectors this object appears in.
// Returns a pointer to the next node on the linked list, or NULL.
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
      // sector_list and not from mobj_t->touching_sectorlist.

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
static boolean PIT_GetSectors(line_t *ld)
{
   if(tm->bbox[BOXRIGHT]  <= ld->bbox[BOXLEFT]   ||
      tm->bbox[BOXLEFT]   >= ld->bbox[BOXRIGHT]  ||
      tm->bbox[BOXTOP]    <= ld->bbox[BOXBOTTOM] ||
      tm->bbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
      return true;

   if(P_BoxOnLineSide(tm->bbox, ld) != -1)
      return true;

   // This line crosses through the object.
   
   // Collect the sector(s) from the line and add to the
   // sector_list you're examining. If the Thing ends up being
   // allowed to move to this position, then the sector_list
   // will be attached to the Thing's mobj_t at touching_sectorlist.

   tm->sector_list = P_AddSecnode(ld->frontsector, tm->thing, tm->sector_list);

   // Don't assume all lines are 2-sided, since some Things
   // like teleport fog are allowed regardless of whether their 
   // radius takes them beyond an impassable linedef.
   
   // killough 3/27/98, 4/4/98:
   // Use sidedefs instead of 2s flag to determine two-sidedness.
   // killough 8/1/98: avoid duplicate if same sector on both sides

   if(ld->backsector && ld->backsector != ld->frontsector)
      tm->sector_list = P_AddSecnode(ld->backsector, tm->thing, tm->sector_list);
   
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
// haleyjd 01/02/00: added cph's fix to stop clobbering the tm->thing
//  global variable (whose stupid idea was it to use that?)
//
void P_CreateSecNodeList(mobj_t *thing,fixed_t x,fixed_t y)
{
   int xl, xh, yl, yh, bx, by;
   msecnode_t *node;
   mobj_t *saved_tmthing = tm->thing; // haleyjd
   fixed_t saved_tmx = tm->x, saved_tmy = tm->y;
   int saved_tmflags = tm->flags;

   fixed_t saved_bbox[4]; // haleyjd

   // SoM says tmbbox needs to be saved too -- don't bother if
   // not needed for compatibility though
   if(demo_version < 200 || demo_version >= 331)
      memcpy(saved_bbox, tm->bbox, 4*sizeof(fixed_t));

   // First, clear out the existing m_thing fields. As each node is
   // added or verified as needed, m_thing will be set properly. When
   // finished, delete all nodes where m_thing is still NULL. These
   // represent the sectors the Thing has vacated.
   
   for(node = tm->sector_list; node; node = node->m_tnext)
      node->m_thing = NULL;

   tm->thing = thing;
   tm->flags = thing->flags;

   tm->x = x;
   tm->y = y;
   
   tm->bbox[BOXTOP]  = y + tm->thing->radius;
   tm->bbox[BOXBOTTOM] = y - tm->thing->radius;
   tm->bbox[BOXRIGHT]  = x + tm->thing->radius;
   tm->bbox[BOXLEFT]   = x - tm->thing->radius;

   validcount++; // used to make sure we only process a line once
   
   xl = (tm->bbox[BOXLEFT] - bmaporgx)>>MAPBLOCKSHIFT;
   xh = (tm->bbox[BOXRIGHT] - bmaporgx)>>MAPBLOCKSHIFT;
   yl = (tm->bbox[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
   yh = (tm->bbox[BOXTOP] - bmaporgy)>>MAPBLOCKSHIFT;

   for(bx=xl ; bx<=xh ; bx++)
      for(by=yl ; by<=yh ; by++)
         P_BlockLinesIterator(bx,by,PIT_GetSectors);

   // Add the sector of the (x,y) point to sector_list.

   tm->sector_list = 
      P_AddSecnode(thing->subsector->sector,thing,tm->sector_list);

   // Now delete any nodes that won't be used. These are the ones where
   // m_thing is still NULL.
   
   for(node = tm->sector_list; node;)
   {
      if(node->m_thing == NULL)
      {
         if(node == tm->sector_list)
            tm->sector_list = node->m_tnext;
         node = P_DelSecnode(node);
      }
      else
         node = node->m_tnext;
   }

  /* cph -
   * This is the strife we get into for using global variables. 
   *  tm->thing is being used by several different functions calling
   *  P_BlockThingIterator, including functions that can be called 
   *  *from* P_BlockThingIterator. Using a global tm->thing is not 
   *  reentrant. OTOH for Boom/MBF demos we have to preserve the buggy 
   *  behaviour. Fun. We restore its previous value unless we're in a 
   *  Boom/MBF demo. -- haleyjd: add SMMU too :)
   */

   if(demo_version < 200 || demo_version >= 329)
      tm->thing = saved_tmthing;

   // haleyjd 11/11/02: cph forgot these in the original fix
   if(demo_version < 200 || demo_version >= 331)
   {
      tm->x = saved_tmx;
      tm->y = saved_tmy;
      tm->flags = saved_tmflags;
      memcpy(tm->bbox, saved_bbox, 4*sizeof(fixed_t)); // haleyjd
   }
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

