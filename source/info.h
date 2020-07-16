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
//      Thing frame/state LUT,
//      generated by multigen utilitiy.
//      This one is the original DOOM version, preserved.
//
//-----------------------------------------------------------------------------

#ifndef INFO_H__
#define INFO_H__

#include "m_dllist.h"

struct actionargs_t;
struct arglist_t;
struct e_pickupfx_t;
class  MetaTable;
class  Mobj;

// haleyjd 07/17/04: sprite constants removed

typedef int spritenum_t;
extern int NUMSPRITES;

// ********************************************************************
// States (frames) enumeration -- haleyjd: now used as DeHackEd nums
// ********************************************************************
// haleyjd 09/14/04: only enum values actually used remain here now.

enum
{
  S_NULL,
  
  S_PUNCH = 2,
  S_PUNCHDOWN,
  S_PUNCHUP,
  S_PUNCH1,

  S_PISTOL = 10,
  S_PISTOLDOWN,
  S_PISTOLUP,
  S_PISTOL1,
  S_PISTOLFLASH = 17,
  
  S_SGUN = 18,
  S_SGUNDOWN,
  S_SGUNUP,
  S_SGUN1,
  S_SGUNFLASH1 = 30,

  S_DSGUN = 32,
  S_DSGUNDOWN,
  S_DSGUNUP,
  S_DSGUN1,
  S_DSGUNFLASH1 = 47,

  S_CHAIN = 49,
  S_CHAINDOWN,
  S_CHAINUP,
  S_CHAIN1,
  S_CHAIN2, // not used
  S_CHAIN3,
  S_CHAINFLASH1,
  
  S_MISSILE = 57,
  S_MISSILEDOWN,
  S_MISSILEUP,
  S_MISSILE1,
  S_MISSILEFLASH1 = 63,

  S_SAW = 67,
  S_SAWB,    // not used
  S_SAWDOWN,
  S_SAWUP,
  S_SAW1,

  S_PLASMA = 74,
  S_PLASMADOWN,
  S_PLASMAUP,
  S_PLASMA1,
  S_PLASMAFLASH1 = 79,

  S_BFG = 81,
  S_BFGDOWN,
  S_BFGUP,
  S_BFG1,
  S_BFGFLASH1 = 88,

  S_BLOOD2 = 91,
  S_BLOOD3,

  S_PUFF3 = 95,

  S_TBALL1 = 97,

  S_EXPLODE1 = 127,

  S_VILE_HEAL1 = 266,

  S_SARG_RUN1 = 477,
  S_SARG_PAIN2 = 489,

  S_PAIN_DIE6 = 719,

  S_BRAINEXPLODE1 = 799,

  S_GIBS = 895,

  S_OLDBFG1 = 999,  // killough 7/11/98: the old BFG's 43 firing frames

  // Start Heretic frames
  S_POD_GROW1 = 2013,

  S_MUMMYFX1_1 = 2127,

  S_SRCR1_ATK4 = 2318,
  S_SRCR1_DIE17 = 2338,

  S_SOR2_RISE1 = 2347,
  S_SOR2_TELE1 = 2365,
  S_SOR2_DIE4  = 2374,

  S_MNTR_ATK3_1 = 2481,
  S_MNTR_ATK3_4 = 2484,
  S_MNTR_ATK4_1,

  S_LICHFX3_4 = 2561,

  S_IMP_XCRASH1 = 2642,

  // haleyjd: NUMSTATES is now a variable
  //NUMSTATES  // Counter of how many there are

};

typedef int statenum_t;

// state flags
// NOTE: STATEFI = internal flag, not meant for user consumption
enum stateflags_e
{
   STATEFI_DECORATE    = 0x00000001, // 01/01/12: reserved for DECORATE definition
   STATEF_SKILL5FAST   = 0x00000002, // 08/02/13: tics halve on nightmare skill
   STATEFI_VANILLA0TIC = 0x00000004, // always use old 0-tic behaviour (when spawnstate)
   STATEF_INTERPOLATE  = 0x00000008, // Interpolate when given offset
};

// ********************************************************************
// Definition of the state (frames) structure
// ********************************************************************
struct state_t
{
   DLListItem<state_t> namelinks;             // haleyjd 03/30/10: new hashing: by name
   DLListItem<state_t> numlinks;              // haleyjd 03/30/10: new hashing: by dehnum

   spritenum_t  sprite;                       // sprite number to show
   int          frame;                        // which frame/subframe of the sprite is shown
   int          tics;                         // number of gametics this frame should last
   void         (*action)(actionargs_t *);    // code pointer to function for action if any
   void         (*oldaction)(actionargs_t *); // haleyjd: original action, for DeHackEd
   statenum_t   nextstate;                    // index of next state, or -1
   int          misc1, misc2;                 // used for psprite positioning
   int          particle_evt;                 // haleyjd: determines an event to run  
   arglist_t   *args;                         // haleyjd: state arguments
   unsigned int flags;                        // haleyjd: flags
   
   // haleyjd: fields needed for EDF identification and hashing
   char       *name;      // buffer for name
   int         dehnum;    // DeHackEd number for fast access, comp.
   int         index;     // 06/12/09: number of state in states array
};

// these are in info.c

extern state_t **states;
extern int NUMSTATES;

extern char **sprnames;

// ********************************************************************
// Thing enumeration -- haleyjd: now used as DeHackEd nums
// ********************************************************************
// Note that many of these are generically named for the ornamentals
// haleyjd 08/02/04: only enum values actually used remain here now.
//
enum
{
  MT_VILE = 4,
  MT_FIRE,
  
  MT_TRACER = 7,
  MT_SMOKE,
  
  MT_FATSHOT = 10,

  MT_BRUISER = 16,
  MT_BRUISERSHOT,
  MT_KNIGHT,
  MT_SKULL,
  MT_SPIDER,
  MT_BABY,
  MT_CYBORG,
  MT_PAIN,

  MT_BOSSTARGET = 28,
  MT_SPAWNSHOT,
  MT_SPAWNFIRE,
  MT_BARREL,
  MT_TROOPSHOT,
  MT_HEADSHOT,
  MT_ROCKET,
  MT_PLASMA,
  MT_BFG,
  MT_ARACHPLAZ,
  MT_PUFF,
  MT_BLOOD,
  MT_TFOG,
  MT_IFOG,
  MT_TELEPORTMAN,
  MT_EXTRABFG,

  MT_PUSH = 138, // controls push source -- phares
  MT_PULL,       // controls pull source -- phares 3/20/98
  MT_DOGS,       // killough 7/19/98: Marine's best friend
  MT_PLASMA1,    // killough 7/11/98: first  of alternating beta plasma fireballs
  MT_PLASMA2,    // killough 7/11/98: second of alternating beta plasma fireballs

  // haleyjd 10/08/02: Heretic things
  MT_POD = 305,
  MT_PODGOO,

  MT_HTFOG = 334,
  MT_HTICBLOOD,
  
  MT_MUMMYSOUL = 340,
  MT_MUMMYFX1,

  MT_BEASTBALL = 343,
  MT_PUFFY,

  MT_SNAKEPRO_A = 346,
  MT_SNAKEPRO_B,

  MT_WIZARD = 349,
  MT_WIZFX1,

  MT_KNIGHTGHOST = 352,
  MT_KNIGHTAXE,
  MT_REDAXE,

  MT_SRCRFX1 = 356,
  MT_SORCERER2,
  MT_SOR2FX1,
  MT_SOR2FXSPARK,
  MT_SOR2FX2,
  MT_SOR2TELEFADE,

  MT_DSPARILSPOT = 367,

  MT_VOLCANOBLAST = 369,
  MT_VOLCANOTBLAST,

  MT_MNTRFX1 = 372,
  MT_MNTRFX2,
  MT_MNTRFX3,
  MT_PHOENIXPUFF,

  MT_LICH = 376,
  MT_LICHFX1,
  MT_LICHFX2,
  MT_LICHFX3,
  MT_WHIRLWIND,

  MT_IMPCHUNK1 = 395,
  MT_IMPCHUNK2,
  MT_IMPBALL,

  MT_HPLAYERSKULL = 399,

  MT_HFIREBOMB = 409,

  // Heretic weapon-associated things (not actual weapons)
  MT_STAFFPUFF = 416,
  MT_STAFFPUFF2,
  MT_BEAKPUFF,
  MT_GAUNTLETPUFF1,
  MT_GAUNTLETPUFF2,
  MT_BLASTERSMOKE,
  MT_RIPPER,
  MT_BLASTERPUFF1,
  MT_BLASTERPUFF2,
  MT_HORNRODFX1,
  MT_GOLDWANDFX1,
  MT_GOLDWANDFX2,
  MT_GOLDWANDPUFF1,
  MT_GOLDWANDPUFF2,
  MT_PHOENIXFX1,

  MT_CRBOWFX1 = 434,
  MT_CRBOWFX2,
  MT_CRBOWFX3,
  MT_CRBOWFX4,
  MT_MACEFX1,
  MT_MACEFX2,
  MT_MACEFX3,
  MT_MACEFX4,
  MT_PHOENIXFX2,

// Start Eternity TC New Things

  MT_FOGPATCHS = 228,
  MT_FOGPATCHM,
  MT_FOGPATCHL,
  
// End Eternity TC New Things

  MT_CAMERA = 1062, // SMMU camera spot
  MT_PLASMA3,       // haleyjd: for burst bfg

  // haleyjd: NUMMOBJTYPES is a variable now
  //NUMMOBJTYPES  // Counter of how many there are
}; 

typedef int mobjtype_t;

// ********************************************************************
// Definition of the Thing structure
// ********************************************************************
// Note that these are only indices to the state, sound, etc. arrays
// and not actual pointers.  Most can be set to zero if the action or
// sound doesn't apply (like lamps generally don't attack or whistle).

struct mobjinfo_t
{
   int doomednum;       // Thing number used in id's editor, and now
                        //  probably by every other editor too
   int spawnstate;      // The state (frame) index when this Thing is
                        //  first created
   int spawnhealth;     // The initial hit points for this Thing
   int seestate;        // The state when it sees you or wakes up
   int seesound;        // The sound it makes when waking
   int reactiontime;    // How many tics it waits after it wakes up
                        //  before it will start to attack, in normal
                        //  skills (halved for nightmare)
   int attacksound;     // The sound it makes when it attacks
   int painstate;       // The state to indicate pain
   int painchance;      // A number that is checked against a random
                        //  number 0-255 to see if the Thing is supposed
                        //  to go to its painstate or not.  Note this
                        //  has absolutely nothing to do with the chance
                        //  it will get hurt, just the chance of it
                        //  reacting visibly.
   int painsound;       // The sound it emits when it feels pain
   int meleestate;      // Melee==close attack
   int missilestate;    // What states to use when it's in the air, if
                        //  in fact it is ever used as a missile
   int deathstate;      // What state begins the death sequence
   int xdeathstate;     // What state begins the horrible death sequence
                        //  like when a rocket takes out a trooper
   int deathsound;      // The death sound.  See also A_Scream() in
                        //  p_enemy.c for some tweaking that goes on
                        //  for certain monsters
   int speed;           // How fast it moves.  Too fast and it can miss
                        //  collision logic.
   int radius;          // An often incorrect radius
   int height;          // An often incorrect height, used only to see
                        //  if a monster can enter a sector
   int c3dheight;       // haleyjd 07/06/05: a height value corrected for
                        //  3D object clipping. Used only if non-zero and
                        //  comp_theights is enabled.
   int mass;            // How much an impact will move it.  Cacodemons
                        //  seem to retreat when shot because they have
                        //  very little mass and are moved by impact
   int damage;          // If this is a missile, how much does it hurt?
   int damagemod;       // [XA] damage modulus (i.e. the 8 in '1d8')
   int activesound;     // What sound it makes wandering around, once
                        //  in a while.  Chance is 3/256 it will.
   unsigned int flags;  // Bit masks for lots of things.  See p_mobj.h
   unsigned int flags2; // More bit masks for lots of other things -- haleyjd
   unsigned int flags3; // haleyjd 11/03/02: flags3
   unsigned int flags4; // haleyjd 09/13/09: flags4
   int raisestate;      // The first state for an Archvile or respawn
                        //  resurrection.  Zero means it won't come
                        //  back to life.
   int translucency;    // haleyjd 09/01/02: zdoom-style translucency
   int tranmap;         // ioanch  20170903: Boom-style translucency map
   int bloodcolor;      // haleyjd 05/08/03: particle blood color
   unsigned int particlefx; // haleyjd 07/13/03: particle effects
   int mod;             // haleyjd 07/13/03: method of death
   char *obituary;      // haleyjd 07/13/03: normal obituary
   char *meleeobit;     // haleyjd 07/13/03: melee obituary
   int colour;          // haleyjd 01/12/04: translations
   int dmgspecial;      // haleyjd 08/01/04: special damage actions
   int crashstate;      // haleyjd 08/07/04: a dead object hitting the ground
                        //  will enter this state if it has one.
   int altsprite;       // haleyjd 09/26/04: alternate sprite
   int defsprite;       // haleyjd 06/11/08: need to track default sprite
   int topdamage;       // haleyjd 09/22/06: burn damage for 3D clipping :)
   int topdamagemask;   // haleyjd 09/23/06: time mask for topdamage
   int alphavelocity;   // haleyjd 05/23/08: alpha velocity
   int respawntime;     // haleyjd 09/23/09: minimum time before respawn
   int respawnchance;   // haleyjd 09/23/09: chance of respawn (out of 255)   
   float xscale;        // haleyjd 11/22/09: x scaling
   float yscale;        // haleyjd 11/22/09: y scaling
   int activestate;     // haleyjd 03/19/11: Hexen activation state
   int inactivestate;   // haleyjd 03/19/11: Hexen deactivation state
   int activatesound;   // haleyjd 03/19/11: Hexen activation sound
   int deactivatesound; // haleyjd 03/19/11: Hexen deactivation sound
   int gibhealth;       // haleyjd 09/12/13: health at which actor gibs

   e_pickupfx_t *pickupfx;

   void (*nukespec)(actionargs_t *); // haleyjd 08/18/09: nukespec made a native property
   
   // haleyjd: fields needed for EDF identification and hashing
   DLListItem<mobjinfo_t> namelinks;  // hashing: by name
   DLListItem<mobjinfo_t> cnamelinks; // hashing: by compatname
   DLListItem<mobjinfo_t> numlinks;   // hashing: by dehnum

   char *name;         // buffer for name (max 128 chars)
   char *compatname;   // compatibility name for ACS and UDMF
   int   dehnum;       // DeHackEd number for fast access, comp.
   int   index;        // index in mobjinfo
   int   generation;   // EDF generation number

   // 08/17/09: metatable
   MetaTable *meta;

   // 06/19/09: inheritance chain for DECORATE-like semantics where required
   mobjinfo_t *parent;
};

// See p_mobj_h for addition more technical info

extern mobjinfo_t **mobjinfo;
extern int NUMMOBJTYPES;

#endif

//----------------------------------------------------------------------------
//
// $Log: info.h,v $
// Revision 1.10  1998/05/12  12:47:31  phares
// Removed OVER_UNDER code
//
// Revision 1.9  1998/05/06  11:31:53  jim
// Moved predefined lump writer info->w_wad
//
// Revision 1.8  1998/05/04  21:35:54  thldrmn
// commenting and reformatting
//
// Revision 1.7  1998/04/22  06:33:58  killough
// Add const to WritePredefinedLumpWad() parm
//
// Revision 1.6  1998/04/21  23:47:10  jim
// Predefined lump dumper option
//
// Revision 1.5  1998/03/23  15:24:09  phares
// Changed pushers to linedef control
//
// Revision 1.4  1998/03/09  18:30:43  phares
// Added invisible sprite for MT_PUSH
//
// Revision 1.3  1998/02/24  08:45:53  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.2  1998/01/26  19:27:02  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:57  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
