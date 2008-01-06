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
//      Map functions
//
//-----------------------------------------------------------------------------

#ifndef __P_MAP__
#define __P_MAP__

#include "r_defs.h"
#include "d_player.h"

#define USERANGE        (64*FRACUNIT)
#define MELEERANGE      (64*FRACUNIT)
#define MISSILERANGE    (32*64*FRACUNIT)

// MAXRADIUS is for precalculated sector block boxes the spider demon
// is larger, but we do not have any moving sectors nearby
#define MAXRADIUS       (32*FRACUNIT)

// killough 3/15/98: add fourth argument to P_TryMove
boolean P_TryMove(mobj_t *thing, fixed_t x, fixed_t y, boolean dropoff);

// killough 8/9/98: extra argument for telefragging
boolean P_TeleportMove(mobj_t *thing, fixed_t x, fixed_t y,boolean boss);
// haleyjd 06/06/05: new function that won't stick the thing inside inert objects
boolean P_TeleportMoveStrict(mobj_t *thing, fixed_t x, fixed_t y, boolean boss);
#ifdef R_LINKEDPORTALS
// SoM: new function that won't telefrag things which the transporting mobj isn't
// touching on the z axis.
boolean P_PortalTeleportMove(mobj_t *thing, fixed_t x, fixed_t y);
#endif


void    P_SlideMove(mobj_t *mo);
boolean P_CheckSight(mobj_t *t1, mobj_t *t2);
void    P_UseLines(player_t *player);

// killough 8/2/98: add 'mask' argument to prevent friends autoaiming at others
fixed_t P_AimLineAttack(mobj_t *t1, angle_t angle, fixed_t distance,int mask);
void    P_LineAttack(mobj_t *t1, angle_t angle, fixed_t distance,
                     fixed_t slope, int damage );
void    P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage, int mod);
boolean P_CheckPosition(mobj_t *thing, fixed_t x, fixed_t y);

//jff 3/19/98 P_CheckSector(): new routine to replace P_ChangeSector()
boolean P_CheckSector(sector_t *sector, int crunch, int amt, int floorOrCeil);
void    P_DelSeclist(msecnode_t*);                          // phares 3/16/98
void    P_FreeSecNodeList();    // sf
void    P_CreateSecNodeList(mobj_t*,fixed_t,fixed_t);       // phares 3/14/98
boolean Check_Sides(mobj_t *, int, int);                    // phares

int     P_GetMoveFactor(const mobj_t *mo, int *friction);   // killough 8/28/98
int     P_GetFriction(const mobj_t *mo, int *factor);       // killough 8/28/98
void    P_ApplyTorque(mobj_t *mo);                          // killough 9/12/98


typedef struct doom_mapinter_s
{
   // SoM: These are at the front of the struct because they need to be set initially
   // Temporary holder for thing_sectorlist threads
   struct doom_mapinter_s *prev; // SoM: previous entry in stack (for pop)
   msecnode_t *sector_list;      // phares 3/16/98
   mobj_t *BlockingMobj;         // haleyjd 1/17/00: global hit reference

   // -----------------------------------------------------------------------
   // The tm* items are used to hold information globally, usually for
   // line or object intersection checking
   // SoM: These used to be prefixed with tm


   // SoM 09/07/02: Solution to problem of monsters walking on 3dsides
   // haleyjd: values for tmtouch3dside:
   // 0 == no 3DMidTex involved in clipping
   // 1 == 3DMidTex involved but not responsible for floorz
   // 2 == 3DMidTex responsible for floorz
   int touch3dside; // Moved to the top of the struct because it needs an initial value

   mobj_t    *thing;
   int       flags;
   fixed_t   x;
   fixed_t   y;

   fixed_t   bbox[4];  // bounding box for line intersection checks
   fixed_t   floorz;   // floor you'd hit if free to fall
   fixed_t   ceilingz; // ceiling of sector you're in
   fixed_t   dropoffz; // dropoff on other side of line you're crossing

   // SoM : the floorz and ceilingz of the sectors.
   fixed_t   secfloorz;
   fixed_t   secceilz;

   // SoM: Stepup floorz from a thing... Not sure if it should be used?
   // set this instead of tmfloorz
   fixed_t   stepupfloorz;

   // SoM 11/6/02: UGHAH
   fixed_t   passfloorz;
   fixed_t   passceilz;

   // haleyjd
   int       floorpic;

   int       unstuck;     // killough 8/1/98: whether to allow unsticking
   // SoM: End of tm* list
   // -----------------------------------------------------------------------
   
   // If "floatok" true, move would be ok
   // if within "tmfloorz - tmceilingz".
   boolean   floatok;

   // killough 11/98: if "felldown" true, object was pushed down ledge
   boolean   felldown;

   // keep track of the line that lowers the ceiling,
   // so missiles don't explode against sky hack walls
   line_t    *ceilingline;
   line_t    *blockline;    // killough 8/11/98: blocking linedef
   line_t    *floorline;    // killough 8/1/98: Highest touched floor

   mobj_t    *linetarget; // who got hit (or NULL)

   // keep track of special lines as they are hit,
   // but don't process them until the move is proven valid
   // 1/11/98 killough: removed limit on special lines crossed
   line_t **spechit;    // new code -- killough
   int spechit_max;     // killough
   int numspechit;


   // See P_LineOpening
   fixed_t opentop;
   fixed_t openbottom;
   fixed_t openrange;
   fixed_t lowfloor;

   // SoM 11/3/02: opensecfloor, opensecceil.
   fixed_t opensecfloor;
   fixed_t opensecceil;

   // moved front and back outside P_LineOpening and changed    // phares 3/7/98
   // them to these so we can pick up the new friction value
   // in PIT_CheckLine()
   sector_t *openfrontsector; // made global                    // phares
   sector_t *openbacksector;  // made global

#ifdef R_LINKEDPORTALS
   // SoM: Used by PIT_CheckPortalLine to determine how lines should effect the
   // tm->floorz and tm->ceilingz. True means the portal is below tm->thing,
   // false means the portal is above.
   boolean  portalbelow;

   // SoM: Also used by PIT_CheckPortalLine to determine if tm->thing is in physical contact
   // with the portal or not.
   boolean  portalcontact;


   fixed_t portalfloorz, portaldropoffz, portalceilingz;
#endif
} doom_mapinter_t;


// Pushes the tm stack, clearing the new element
void P_PushTMStack();

// Pops the tm stack, storing the discarded element for later re-insertion.
void P_PopTMStack();


extern doom_mapinter_t  *tm;


extern int spechits_emulation; // haleyjd 09/20/06



#ifdef R_LINKEDPORTALS
// ---- Linked portals ----
// Tracer-Portal-Translation

// tpt_t is the main struct that holds in the information needed to translate the curretly running tracer to a new portal group.
typedef struct tptnode_s
{
   // The type of tracer involved.
   enum
   {
      tShoot,
      tAim,
      tUse,
   } type;

   // -- Used by both types --
   // Translated tracer coords keep the distance to be traveled constant
   fixed_t x, y, dx, dy;
   // Translated z coord should continue from height of the portal or whatever height it hit the portal line.
   fixed_t z;
   fixed_t originx, originy, originz;
   // Accumulated travel along the line. Should be the XY distance between (x,y) 
   // and (originx, originy) 
   fixed_t movefrac;
   fixed_t attackrange;

   // -- Used by tAim --
   fixed_t topslope, bottomslope;

   // -- Singly linked list --
   struct tptnode_s *next;
} tptnode_t;



boolean    P_CheckTPT(void);
tptnode_t *P_StartTPT();
void       P_FinishTPT(tptnode_t *node);
void       P_ClearTPT();
#endif

#endif // __P_MAP__

//----------------------------------------------------------------------------
//
// $Log: p_map.h,v $
// Revision 1.2  1998/05/07  00:53:07  killough
// Add more external declarations
//
// Revision 1.1  1998/05/03  22:19:23  killough
// External declarations formerly in p_local.h
//
//
//----------------------------------------------------------------------------
