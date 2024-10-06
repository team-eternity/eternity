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
//      Map functions
//
//-----------------------------------------------------------------------------

#ifndef P_MAP_H__
#define P_MAP_H__

#include "linkoffs.h"
#include "m_collection.h"
#include "m_surf.h"
#include "tables.h"

struct doom_mapinter_t;
struct line_t;
struct mobjinfo_t;
struct msecnode_t;
struct player_t;
struct polyobj_t; // ioanch 20160122
struct sector_t;

//=============================================================================
//
// Global Defines
//

#define USERANGE     (64 * FRACUNIT)
#define MELEERANGE   (64 * FRACUNIT)
#define MISSILERANGE (32 * 64 * FRACUNIT)

#define FPMELEERANGE (64.0)

// a couple of explicit constants for non-melee things that used to use MELEERANGE
#define WAKEUPRANGE     (64*FRACUNIT)
#define SNEAKRANGE      (128*FRACUNIT)

// MAXRADIUS is for precalculated sector block boxes the spider demon
// is larger, but we do not have any moving sectors nearby
#define MAXRADIUS    (32 * FRACUNIT)

// ioanch 20160103: use a macro for step size
#define STEPSIZE     (24 * FRACUNIT)

//=============================================================================
//
// Global Variables
//

extern int  spechits_emulation; // haleyjd 09/20/06
extern bool donut_emulation;    // haleyjd 10/16/09

//=============================================================================
//
// Movement and Clipping
//

// killough 3/15/98: add fourth argument to P_TryMove
bool P_TryMove(Mobj *thing, fixed_t x, fixed_t y, int dropoff);

void P_GetClipBasics(Mobj &thing, fixed_t x, fixed_t y, doom_mapinter_t &inter,
                     const sector_t *&bottomsector, const sector_t *&topsector);

bool P_CheckPosition(Mobj *thing, fixed_t x, fixed_t y, PODCollection<line_t *> *pushhit = nullptr);
bool P_CheckLineBlocksThing(line_t *ld, const linkoffset_t *link,
                            PODCollection<line_t *> *pushhit, bool &output);

//
// Context for PIT_CheckLine
//
struct pitcheckline_t
{
   PODCollection<line_t *> *pushhit;   // list of pushed lines
   bool haveslopes;  // mark this if any slope got detected
};

bool PIT_CheckLine(line_t *ld, polyobj_t *po, void *context);  // ioanch: used in the code

void P_SlideMove(Mobj *mo);

// ioanch
void P_CollectSpechits(line_t *ld, PODCollection<line_t *> *pushhit);

bool P_BlockedAsMonster(const Mobj &mo);

int P_MissileBlockHeight(const Mobj *mo);

//
// Various results
//
enum ItemCheckResult
{
   ItemCheck_furtherNeeded,
   ItemCheck_pass,
   ItemCheck_hit
};
ItemCheckResult P_CheckThingCommon(Mobj *thing);

//=============================================================================
//
// Teleportation
//

enum
{
   TELEMOVE_BOSS = 1,
   TELEMOVE_FRAG = 2,
};

// killough 8/9/98: extra argument for telefragging
bool P_TeleportMove(Mobj *thing, fixed_t x, fixed_t y, unsigned flags);

// haleyjd 06/06/05: new function that won't stick the thing inside inert objects
bool P_TeleportMoveStrict(Mobj *thing, fixed_t x, fixed_t y, bool boss);

//=============================================================================
//
// Sight Checks, Tracers, Path Traversal
//

bool P_CheckSight(Mobj *t1, Mobj *t2);
void P_UseLines(player_t *player);

// killough 8/2/98: add 'mask' argument to prevent friends autoaiming at others
fixed_t P_AimLineAttack(Mobj *t1, angle_t angle, fixed_t distance, bool mask);

void P_LineAttack(Mobj *t1, angle_t angle, fixed_t distance, fixed_t slope, 
                  int damage, const char *pufftype = nullptr);

bool Check_Sides(Mobj *, int, int, mobjtype_t type); // phares

//=============================================================================
//
// Radius Attacks
//

// flags for controlling radius attack behaviors
enum
{
   RAF_NOSELFDAMAGE = 0x00000001, // explosion does not damage originator
   RAF_CLIPHEIGHT   = 0x00000002, // height range is checked, like Hexen
};

void P_RadiusAttack(Mobj *spot, Mobj *source, int damage, int distance, 
                    int mod, unsigned int flags);

//=============================================================================
//
// Sector Motion
//

//jff 3/19/98 P_CheckSector(): new routine to replace P_ChangeSector()

//
// Plane for P_CheckSector
//
enum class CheckSectorPlane
{
   floor,
   ceiling,
   midtex3d
};

bool P_CheckSector(sector_t *sector, int crunch, int amt, CheckSectorPlane plane);
bool P_ChangeSector(sector_t *sector, int crunch);

//=============================================================================
//
// Physics Forces: Friction, Torque
//

int  P_GetMoveFactor(Mobj *mo, int *friction);      // killough 8/28/98
int  P_GetFriction(const Mobj *mo, int *factor);    // killough 8/28/98
void P_ApplyTorque(Mobj *mo);                       // killough 9/12/98

//=============================================================================
//
// Secnode List Maintenance Routines
//

void P_DelSeclist(msecnode_t *); // phares 3/16/98
void P_FreeSecNodeList();        // sf
msecnode_t *P_CreateSecNodeList(Mobj *, fixed_t, fixed_t);  // phares 3/14/98

//=============================================================================
//
// MapInter Structure
//
// Replaces the strictly non-reentrant vast set of globals used throughout the
// DOOM clipping engine with a single structure which can optionally be used
// reentrantly by pushing and popping an instance of it onto the clip stack.
//

//
// All the various Z coordinates carried around when clipping with lines and sectors
//
struct zrefs_t
{
   // The closest interval over all contacted Sectors.
   fixed_t floor;
   fixed_t ceiling;
   // killough 11/98: the lowest floor over all contacted Sectors.
   fixed_t dropoff;

   // Sometimes we need to know if we're on top of a floor slope or below a ceiling slope. To that 
   // end, we need the complete floor or ceiling sector reference
   Surfaces<const sector_t *> sector;

   int floorgroupid;

   // Strictly sector floor and ceiling z, not counting 3dmidtex
   fixed_t secfloor;
   fixed_t secceil;

   // SoM 11/6/02: Yet again! Two more z values that must be stored
   // in the mobj struct 9_9
   // These are the floor and ceiling heights given by the first
   // clipping pass (map architecture + 3d sides).
   fixed_t passfloor;
   fixed_t passceil;
};

//
// The return structure of P_LineOpening
//
struct lineopening_t
{
   void intersect(const lineopening_t &other);

   Surfaces<fixed_t> height;  // bottom/top of line opening
   Surfaces<fixed_t> sec;  // SoM 11/3/02: considering only sector floor/ceiling
   fixed_t range; // height of opening: top - bottom
   int bottomgroupid;   // openbottom group id
   fixed_t lowfloor; // lowest floorheight involved
   int floorpic;  // haleyjd: for CANTLEAVEFLOORPIC flag
   const sector_t *floorsector;  // for floor slope detection
   const sector_t *ceilsector;   // needed for certain dropoff checks when having slopes

   // SoM 09/07/02: Solution to problem of monsters walking on 3dsides
   // haleyjd: values for tmtouch3dside:
   // 0 == no 3DMidTex involved in clipping
   // 1 == 3DMidTex involved but not responsible for floorz
   // 2 == 3DMidTex responsible for floorz
   int touch3dside;
};

struct doom_mapinter_t
{
   doom_mapinter_t *prev; // SoM: previous entry in stack (for pop)

   //=====================================================================
   // The tm* items are used to hold information globally, usually for
   // line or object intersection checking
   // SoM: These used to be prefixed with tm

   Mobj      *thing;     // current thing being clipped
   fixed_t    x;         // x position, usually where we want to move
   fixed_t    y;         // y position, usually where we want to move

   fixed_t    bbox[4];   // bounding box for thing/line intersection checks
   zrefs_t zref;  // keep all various plane Z here

   int        unstuck;   // killough 8/1/98: whether to allow unsticking

   // SoM: End of tm* list
   //=====================================================================
   
   bool       floatok;  // If "floatok" true, move ok if within floorz - ceilingz   
   bool       felldown; // killough 11/98: if "felldown" true, object was pushed down ledge

   // keep track of the line that lowers the ceiling,
   // so missiles don't explode against sky hack walls
   const line_t *ceilingline;
   const line_t *blockline;   // killough 8/11/98: blocking linedef
   const line_t *floorline;   // killough 8/1/98: Highest touched floor

   // TODO: equivalent blocking slopes maybe!

   Mobj      *linetarget;  // who got hit (or nullptr)

   // keep track of special lines as they are hit,
   // but don't process them until the move is proven valid
   // 1/11/98 killough: removed limit on special lines crossed
   line_t   **spechit;      // new code -- killough
   int        spechit_max;  // killough
   int        numspechit;

   // P_LineOpening
   lineopening_t open;

   // Temporary holder for thing_sectorlist threads
   // haleyjd: this is now *only* used inside P_CreateSecNodeList and callees
   msecnode_t *sector_list;     // phares 3/16/98
   
   Mobj       *BlockingMobj;    // haleyjd 1/17/00: global hit reference

   // ioanch 20160121: list of lines postponed to be visited thru portals
   struct linepoly_t
   {
      line_t *ld;
      polyobj_t *po;
   } *portalhit;
   int         portalhit_max;
   int         numportalhit;
};

// Pushes the tm stack, clearing the new element
void P_PushClipStack();

// Pops the tm stack, storing the discarded element for later re-insertion.
void P_PopClipStack();

void P_ClearGlobalLevelReferences();

bool P_OnGroundOrThing(const Mobj &mobj);
void P_UpdateFromOpening(const lineopening_t &open, const line_t *ld, doom_mapinter_t &inter,
   bool underportal, bool aboveportal, uint32_t lineclipflags,
   bool samegroupid, fixed_t linetop);

extern doom_mapinter_t  clip;  // haleyjd 04/16/10: made global, renamed
extern doom_mapinter_t *pClip; // haleyjd 04/16/10: renamed

#endif

// EOF

