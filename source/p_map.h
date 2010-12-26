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
boolean P_TryMove(Mobj *thing, fixed_t x, fixed_t y, int dropoff);

// killough 8/9/98: extra argument for telefragging
boolean P_TeleportMove(Mobj *thing, fixed_t x, fixed_t y,boolean boss);
// haleyjd 06/06/05: new function that won't stick the thing inside inert objects
boolean P_TeleportMoveStrict(Mobj *thing, fixed_t x, fixed_t y, boolean boss);
#ifdef R_LINKEDPORTALS
// SoM: new function that won't telefrag things which the transporting mobj isn't
// touching on the z axis.
boolean P_PortalTeleportMove(Mobj *thing, fixed_t x, fixed_t y);
#endif


void    P_SlideMove(Mobj *mo);
boolean P_CheckSight(Mobj *t1, Mobj *t2);
void    P_UseLines(player_t *player);

// killough 8/2/98: add 'mask' argument to prevent friends autoaiming at others
fixed_t P_AimLineAttack(Mobj *t1, angle_t angle, fixed_t distance,int mask);
void    P_LineAttack(Mobj *t1, angle_t angle, fixed_t distance,
                     fixed_t slope, int damage );
void    P_RadiusAttack(Mobj *spot, Mobj *source, int damage, int mod);
boolean P_CheckPosition(Mobj *thing, fixed_t x, fixed_t y);

//jff 3/19/98 P_CheckSector(): new routine to replace P_ChangeSector()
boolean     P_CheckSector(sector_t *sector, int crunch, int amt, int floorOrCeil);
void        P_DelSeclist(msecnode_t *);                      // phares 3/16/98
void        P_FreeSecNodeList(void);                         // sf
msecnode_t *P_CreateSecNodeList(Mobj *,fixed_t, fixed_t);  // phares 3/14/98
boolean     Check_Sides(Mobj *, int, int);                 // phares

int         P_GetMoveFactor(Mobj *mo, int *friction);      // killough 8/28/98
int         P_GetFriction(const Mobj *mo, int *factor);    // killough 8/28/98
void        P_ApplyTorque(Mobj *mo);                       // killough 9/12/98

typedef struct doom_mapinter_s
{
   struct doom_mapinter_s *prev; // SoM: previous entry in stack (for pop)

   //==========================================================================
   // The tm* items are used to hold information globally, usually for
   // line or object intersection checking
   // SoM: These used to be prefixed with tm

   // SoM 09/07/02: Solution to problem of monsters walking on 3dsides
   // haleyjd: values for tmtouch3dside:
   // 0 == no 3DMidTex involved in clipping
   // 1 == 3DMidTex involved but not responsible for floorz
   // 2 == 3DMidTex responsible for floorz
   int        touch3dside;

   Mobj     *thing;    // current thing being clipped
   fixed_t    x;         // x position, usually where we want to move
   fixed_t    y;         // y position, usually where we want to move

   fixed_t    bbox[4];   // bounding box for thing/line intersection checks
   fixed_t    floorz;    // floor you'd hit if free to fall
   fixed_t    ceilingz;  // ceiling of sector you're in
   fixed_t    dropoffz;  // dropoff on other side of line you're crossing

   fixed_t    secfloorz; // SoM: floorz considering only sector heights
   fixed_t    secceilz;  // SoM: ceilingz considering only sector heights
   
   fixed_t    passfloorz; // SoM 11/6/02: UGHAH
   fixed_t    passceilz;

   int        floorpic;  // haleyjd: for CANTLEAVEFLOORPIC flag

   int        unstuck;   // killough 8/1/98: whether to allow unsticking

   // SoM: End of tm* list
   //==========================================================================
   
   boolean    floatok;   // If "floatok" true, move ok if within floorz - ceilingz   
   boolean    felldown;  // killough 11/98: if "felldown" true, object was pushed down ledge

   // keep track of the line that lowers the ceiling,
   // so missiles don't explode against sky hack walls
   line_t     *ceilingline;
   line_t     *blockline;   // killough 8/11/98: blocking linedef
   line_t     *floorline;   // killough 8/1/98: Highest touched floor

   Mobj     *linetarget;  // who got hit (or NULL)

   // keep track of special lines as they are hit,
   // but don't process them until the move is proven valid
   // 1/11/98 killough: removed limit on special lines crossed
   line_t     **spechit;    // new code -- killough
   int        spechit_max;  // killough
   int        numspechit;

   // P_LineOpening
   fixed_t    opentop;      // top of line opening
   fixed_t    openbottom;   // bottom of line opening
   fixed_t    openrange;    // height of opening: top - bottom
   fixed_t    lowfloor;     // lowest floorheight involved   
   fixed_t    opensecfloor; // SoM 11/3/02: considering only sector floor
   fixed_t    opensecceil;  // SoM 11/3/02: considering only sector ceiling

   // moved front and back outside P_LineOpening and changed -- phares 3/7/98
   // them to these so we can pick up the new friction value
   // in PIT_CheckLine()
   sector_t   *openfrontsector; // made global
   sector_t   *openbacksector;  // made global

   // Temporary holder for thing_sectorlist threads
   // haleyjd: this is now *only* used inside P_CreateSecNodeList and callees
   msecnode_t *sector_list;     // phares 3/16/98
   
   Mobj     *BlockingMobj;    // haleyjd 1/17/00: global hit reference

} doom_mapinter_t;


// Pushes the tm stack, clearing the new element
void P_PushClipStack(void);

// Pops the tm stack, storing the discarded element for later re-insertion.
void P_PopClipStack(void);

extern doom_mapinter_t clip;   // haleyjd 04/16/10: made global, renamed
extern doom_mapinter_t *pClip; // haleyjd 04/16/10: renamed


extern int spechits_emulation;  // haleyjd 09/20/06
extern boolean donut_emulation; // haleyjd 10/16/09



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
