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
// $Id: d_deh.c,v 1.20 1998/06/01 22:30:38 thldrmn Exp $
//
// Dehacked file support
// New for the TeamTNT "Boom" engine
//
// Author: Ty Halderman, TeamTNT
//
//-----------------------------------------------------------------------------

// killough 5/2/98: fixed headers, removed redundant external declarations:
#include "z_zone.h"

#include "d_dehtbl.h"
#include "d_dwfile.h"
#include "d_io.h"
#include "d_main.h" // haleyjd
#include "doomdef.h"
#include "doomstat.h"
#include "e_args.h"
#include "e_inventory.h"
#include "e_lib.h"
#include "e_player.h"
#include "e_sound.h"
#include "e_states.h"
#include "e_things.h"
#include "e_weapons.h"
#include "g_game.h"
#include "info.h"
#include "metaapi.h"
#include "m_cheat.h"
#include "m_utils.h"
#include "p_info.h"
#include "p_inter.h"
#include "p_mobj.h"
#include "p_tick.h"
#include "sounds.h"
#include "w_wad.h"

// haleyjd 11/01/02: moved deh file/wad stdio emulation to d_io.c
// and generalized, strengthened encapsulation

// killough 10/98: new functions, to allow processing DEH files
// in-memory (e.g. from wads)

// killough 10/98: emulate IO whether input really comes from a file
// or not

// variables used in other routines
bool deh_pars = false; // in wi_stuff to allow pars in modified games

// Function prototypes
static void  lfstrip(char *);     // strip the \r and/or \n off of a line
static void  rstrip(char *);      // strip trailing whitespace
static char *ptr_lstrip(char *);  // point past leading whitespace
static bool  deh_GetData(char *, char *, int *, char **);
static bool  deh_procStringSub(char *, char *, char *);
static char *dehReformatStr(char *);

// Prototypes for block processing functions
// Pointers to these functions are used as the blocks are encountered.

static void deh_procThing(DWFILE *, char *);
static void deh_procFrame(DWFILE *, char *);
static void deh_procPointer(DWFILE *, char *);
static void deh_procSounds(DWFILE *, char *);
static void deh_procAmmo(DWFILE *, char *);
static void deh_procWeapon(DWFILE *, char *);
static void deh_procSprite(DWFILE *, char *);
static void deh_procCheat(DWFILE *, char *);
static void deh_procMisc(DWFILE *, char *);
static void deh_procText(DWFILE *, char *);
static void deh_procPars(DWFILE *, char *);
static void deh_procStrings(DWFILE *, char *);
static void deh_procError(DWFILE *, char *);
static void deh_procBexCodePointers(DWFILE *, char *);
static void deh_procHelperThing(DWFILE *, char *); // haleyjd 9/22/99
// haleyjd: handlers to fully deprecate the DeHackEd text section
static void deh_procBexSounds(DWFILE *, char *);
static void deh_procBexMusic(DWFILE *, char *);
static void deh_procBexSprites(DWFILE *, char *);

// Structure deh_block is used to hold the block names that can
// be encountered, and the routines to use to decipher them

struct deh_block
{
  const char *key;       // a mnemonic block code name
  void (*const fptr)(DWFILE *, char *); // handler
};

struct dehflagremap_t
{
   unsigned int inFlag;
   int          inIndex;
   unsigned int outFlag;
   int          outIndex;
};


#define DEH_BUFFERMAX 1024 // input buffer area size, hardcoded for now
#define DEH_MAXKEYLEN 32   // as much of any key as we'll look at
#define DEH_MAXKEYLEN_FMT "31"  // for scanf format strings

// Put all the block header values, and the function to be called when that
// one is encountered, in this array:
static deh_block deh_blocks[] =
{
   /* 0 */  {"Thing",   deh_procThing},
   /* 1 */  {"Frame",   deh_procFrame},
   /* 2 */  {"Pointer", deh_procPointer},
   /* 3 */  {"Sound",   deh_procSounds},  // Ty 03/16/98 corrected from "Sounds"
   /* 4 */  {"Ammo",    deh_procAmmo},
   /* 5 */  {"Weapon",  deh_procWeapon},
   /* 6 */  {"Sprite",  deh_procSprite},
   /* 7 */  {"Cheat",   deh_procCheat},
   /* 8 */  {"Misc",    deh_procMisc},
   /* 9 */  {"Text",    deh_procText},  // --  end of standard "deh" entries,

   //     begin BOOM Extensions (BEX)

   /* 10 */ {"[STRINGS]", deh_procStrings},    // new string changes
   /* 11 */ {"[PARS]",    deh_procPars},          // par times
   /* 12 */ {"[CODEPTR]", deh_procBexCodePointers}, // bex codepointers by mnemonic
   /* 13 */ {"[HELPER]",  deh_procHelperThing}, // helper thing substitution haleyjd 9/22/99
   /* 14 */ {"[SPRITES]", deh_procBexSprites}, // bex style sprites
   /* 15 */ {"[SOUNDS]",  deh_procBexSounds},   // bex style sounds
   /* 16 */ {"[MUSIC]",   deh_procBexMusic},     // bex style music
   /* 17 */ {"",          deh_procError} // dummy to handle anything else
};

// flag to skip included deh-style text, used with INCLUDE NOTEXT directive
static bool includenotext = false;

// MOBJINFO - Dehacked block name = "Thing"
// Usage: Thing nn (name)
// These are for mobjinfo_t types.  Each is an integer
// within the structure, so we can use index of the string in this
// array to offset by sizeof(int) into the mobjinfo_t array at [nn]
// * things are base zero but dehacked considers them to start at #1. ***


enum dehmobjinfoid_e : int
{
   dehmobjinfoid_doomednum,
   dehmobjinfoid_spawnstate,
   dehmobjinfoid_spawnhealth,
   dehmobjinfoid_seestate,
   dehmobjinfoid_seesound,
   dehmobjinfoid_reactiontime,
   dehmobjinfoid_attacksound,
   dehmobjinfoid_painstate,
   dehmobjinfoid_painchance,
   dehmobjinfoid_painsound,
   dehmobjinfoid_meleestate,
   dehmobjinfoid_missilestate,
   dehmobjinfoid_deathstate,
   dehmobjinfoid_xdeathstate,
   dehmobjinfoid_deathsound,
   dehmobjinfoid_speed,
   dehmobjinfoid_radius,
   dehmobjinfoid_height,
   dehmobjinfoid_mass,
   dehmobjinfoid_damage,
   dehmobjinfoid_activesound,
   dehmobjinfoid_flags,
   dehmobjinfoid_flags2,
   dehmobjinfoid_raisestate,
   dehmobjinfoid_translucency,
   dehmobjinfoid_flags3,
   dehmobjinfoid_bloodcolor,
   dehmobjinfoid_droppeditem,
   dehmobjinfoid_mbf21flags,
   dehmobjinfoid_fastspeed,
   DEH_MOBJINFOMAX
};

static constexpr const char *deh_mobjinfo[DEH_MOBJINFOMAX] =
{
  "ID #",                // .doomednum
  "Initial frame",       // .spawnstate
  "Hit points",          // .spawnhealth
  "First moving frame",  // .seestate
  "Alert sound",         // .seesound
  "Reaction time",       // .reactiontime
  "Attack sound",        // .attacksound
  "Injury frame",        // .painstate
  "Pain chance",         // .painchance
  "Pain sound",          // .painsound
  "Close attack frame",  // .meleestate
  "Far attack frame",    // .missilestate
  "Death frame",         // .deathstate
  "Exploding frame",     // .xdeathstate
  "Death sound",         // .deathsound
  "Speed",               // .speed
  "Width",               // .radius
  "Height",              // .height
  "Mass",                // .mass
  "Missile damage",      // .damage
  "Action sound",        // .activesound
  "Bits",                // .flags
  "Bits2",               // .flags2 haleyjd 04/09/99
  "Respawn frame",       // .raisestate
  "Translucency",        // .translucency  haleyjd 09/01/02
  "Bits3",               // .flags3 haleyjd 02/02/03
  "Blood color",         // .bloodcolor haleyjd 05/08/03
  "Dropped item",        // .meta sorta kinda it's complicated
  "MBF21 Bits",          // .flags[2-5] (they're scattered across)
  "Fast speed",          // .meta sorta kinda it's complicated
};

// Strings that are used to indicate flags ("Bits" in mobjinfo)
// This is an array of bit masks that are related to p_mobj.h
// values, using the same names without the MF_ in front.
// Ty 08/27/98 new code
//
// killough 10/98:
//
// Convert array to struct to allow multiple values, make array size variable

// haleyjd 11/03/02: isolated struct into a non-anonymous type
// haleyjd 04/10/03: moved struct to d_dehtbl.h for global visibility
// haleyjd 02/19/04: combined into one array for new cflags support --
//                   also changed to be terminated by a zero entry

static dehflags_t deh_mobjflags[] =
{
  {"SPECIAL",          0x00000001}, // call  P_Specialthing when touched
  {"SOLID",            0x00000002}, // block movement
  {"SHOOTABLE",        0x00000004}, // can be hit
  {"NOSECTOR",         0x00000008}, // invisible but touchable
  {"NOBLOCKMAP",       0x00000010}, // inert but displayable
  {"AMBUSH",           0x00000020}, // deaf monster
  {"JUSTHIT",          0x00000040}, // will try to attack right back
  {"JUSTATTACKED",     0x00000080}, // take at least 1 step before attacking
  {"SPAWNCEILING",     0x00000100}, // initially hang from ceiling
  {"NOGRAVITY",        0x00000200}, // don't apply gravity during play
  {"DROPOFF",          0x00000400}, // can jump from high places
  {"PICKUP",           0x00000800}, // will pick up items
  {"NOCLIP",           0x00001000}, // goes through walls

  // haleyjd: for combined flags fields, the flags3 SLIDE must take
  // precedence, and thus has to be listed up here. This is a bit of
  // a kludge, but it will preserve compatibility perfectly. The flags
  // SLIDE bit has no effect, as it is never used. I will make a point
  // of avoiding duplicate flag names in the future ;)

  {"SLIDE",            0x00000100, 2}, // mobj slides against walls (for real)
  {"SLIDE",            0x00002000}, // keep info about sliding along walls

  {"FLOAT",            0x00004000}, // allow movement to any height
  {"TELEPORT",         0x00008000}, // don't cross lines or look at heights
  {"MISSILE",          0x00010000}, // don't hit same species, explode on block
  {"DROPPED",          0x00020000}, // dropped, not spawned (like ammo clip)
  {"SHADOW",           0x00040000}, // use fuzzy draw like spectres
  {"NOBLOOD",          0x00080000}, // puffs instead of blood when shot
  {"CORPSE",           0x00100000}, // so it will slide down steps when dead
  {"INFLOAT",          0x00200000}, // float but not to target height
  {"COUNTKILL",        0x00400000}, // count toward the kills total
  {"COUNTITEM",        0x00800000}, // count toward the items total
  {"SKULLFLY",         0x01000000}, // special handling for flying skulls
  {"NOTDMATCH",        0x02000000}, // do not spawn in deathmatch

  // killough 10/98: TRANSLATION consists of 2 bits, not 1:

  {"TRANSLATION",      0x04000000}, // for Boom bug-compatibility
  {"TRANSLATION1",     0x04000000}, // use translation table for color (players)
  {"TRANSLATION2",     0x08000000}, // use translation table for color (players)
  {"UNUSED1",          0x08000000}, // unused bit # 1 -- For Boom bug-compatibility
  {"UNUSED2",          0x10000000}, // unused bit # 2 -- For Boom compatibility
  {"UNUSED3",          0x20000000}, // unused bit # 3 -- For Boom compatibility
  {"UNUSED4",          0x40000000}, // unused bit # 4 -- For Boom compatibility
  {"TOUCHY",           0x10000000}, // dies on contact with solid objects (MBF)
  {"BOUNCES",          0x20000000}, // bounces off floors, ceilings and maybe walls
  {"FRIEND",           0x40000000}, // a friend of the player(s) (MBF)
  {"TRANSLUCENT",      0x80000000}, // apply translucency to sprite (BOOM)

  // flags2 bits
  {"LOGRAV",           0x00000001, 1}, // low gravity
  {"NOSPLASH",         0x00000002, 1}, // no splash object
  {"NOSTRAFE",         0x00000004, 1}, // never uses strafing logic
  {"NORESPAWN",        0x00000008, 1}, // never respawns
  {"ALWAYSRESPAWN",    0x00000010, 1}, // always respawns
  {"REMOVEDEAD",       0x00000020, 1}, // removes self after death
  {"NOTHRUST",         0x00000040, 1}, // not affected by external pushers
  {"NOCROSS",          0x00000080, 1}, // cannot trigger special lines
  {"JUMPDOWN",         0x00000100, 1}, // can jump down to follow player
  {"PUSHABLE",         0x00000200, 1}, // pushable
  {"MAP07BOSS1",       0x00000400, 1}, // triggers map07 special 1
  {"MAP07BOSS2",       0x00000800, 1}, // triggers map07 special 2
  {"E1M8BOSS",         0x00001000, 1}, // triggers e1m8 special
  {"E2M8BOSS",         0x00002000, 1}, // triggers e2m8 end
  {"E3M8BOSS",         0x00004000, 1}, // triggers e3m8 end
  {"BOSS",             0x00008000, 1}, // mobj is a boss
  {"E4M6BOSS",         0x00010000, 1}, // triggers e4m6 special
  {"E4M8BOSS",         0x00020000, 1}, // triggers e4m8 special
  {"FOOTCLIP",         0x00040000, 1}, // feet clipped by liquids
  {"FLOATBOB",         0x00080000, 1}, // use floatbob z movement
  {"DONTDRAW",         0x00100000, 1}, // don't draw vissprite
  {"SHORTMRANGE",      0x00200000, 1}, // has shorter missile range
  {"LONGMELEE",        0x00400000, 1}, // has longer melee range
  {"RANGEHALF",        0x00800000, 1}, // considers 1/2 distance
  {"HIGHERMPROB",      0x01000000, 1}, // higher min. missile attack prob.
  {"CANTLEAVEFLOORPIC",0x02000000, 1}, // can't leave floor type
  {"SPAWNFLOAT",       0x04000000, 1}, // spawn @ random float z
  {"INVULNERABLE",     0x08000000, 1}, // mobj is invincible
  {"DORMANT",          0x10000000, 1}, // mobj is dormant
  {"SEEKERMISSILE",    0x20000000, 1}, // internal, may be a tracer
  {"DEFLECTIVE",       0x40000000, 1}, // if reflective, deflects projectiles
  {"REFLECTIVE",       0x80000000, 1}, // mobj reflects projectiles

  // flags3 bits
  {"GHOST",            0x00000001, 2}, // heretic-style ghost
  {"THRUGHOST",        0x00000002, 2}, // passes through ghosts
  {"NODMGTHRUST",      0x00000004, 2}, // doesn't inflict thrust
  {"ACTSEESOUND",      0x00000008, 2}, // uses see sound randomly
  {"LOUDACTIVE",       0x00000010, 2}, // has full-volume activesnd
  {"E5M8BOSS",         0x00000020, 2}, // boss of E5M8
  {"DMGIGNORED",       0x00000040, 2}, // damage is ignored
  {"BOSSIGNORE",       0x00000080, 2}, // ignores damage by others with flag
  // See above for flags3 SLIDE flag
  {"TELESTOMP",        0x00000200, 2}, // can telestomp
  {"WINDTHRUST",       0x00000400, 2}, // affected by heretic wind
  {"FIREDAMAGE",       0x00000800, 2}, // does fire damage
  {"KILLABLE",         0x00001000, 2}, // is killable, but doesn't count
  {"DEADFLOAT",        0x00002000, 2}, // keeps NOGRAVITY when dead
  {"NOTHRESHOLD",      0x00004000, 2}, // has no target threshold
  {"FLOORMISSILE",     0x00008000, 2}, // is a floor missile
  {"SUPERITEM",        0x00010000, 2}, // is a super powerup
  {"NOITEMRESP",       0x00020000, 2}, // won't item respawn
  {"SUPERFRIEND",      0x00040000, 2}, // won't attack other friends
  {"INVULNCHARGE",     0x00080000, 2}, // invincible when skull flying
  {"EXPLOCOUNT",       0x00100000, 2}, // doesn't explode until count expires
  {"CANNOTPUSH",       0x00200000, 2}, // can't push other things
  {"TLSTYLEADD",       0x00400000, 2}, // uses additive translucency
  {"SPACMONSTER",      0x00800000, 2}, // monster that can activate param lines
  {"SPACMISSILE",      0x01000000, 2}, // missile that can activate param lines
  {"NOFRIENDDMG",      0x02000000, 2}, // object isn't hurt by friends
  {"3DDECORATION",     0x04000000, 2}, // object is a decor. with 3D height info
  {"ALWAYSFAST",       0x08000000, 2}, // object is always in -fast mode
  {"PASSMOBJ",         0x10000000, 2}, // OVER_UNDER
  {"DONTOVERLAP",      0x20000000, 2}, // OVER_UNDER
  {"CYCLEALPHA",       0x40000000, 2}, // alpha cycles perpetually
  {"RIP",              0x80000000, 2}, // ripper projectile

  // flags4 bits
  {"AUTOTRANSLATE",    0x00000001, 3}, // automatic translation for non-DOOM modes
  {"NORADIUSDMG",      0x00000002, 3}, // doesn't take radius damage
  {"FORCERADIUSDMG",   0x00000004, 3}, // forces radius damage despite other flags
  {"LOOKALLAROUND",    0x00000008, 3}, // looks all around for targets
  {"NODAMAGE",         0x00000010, 3}, // takes no damage but reacts normally
  {"SYNCHRONIZED",     0x00000020, 3}, // spawn state tics are not randomized
  {"NORANDOMIZE",      0x00000040, 3}, // missile spawn and death not randomized
  {"BRIGHT",           0x00000080, 3}, // actor is always fullbright
  {"FLY",              0x00000100, 3}, // actor is flying
  {"NORADIUSHACK",     0x00000200, 3}, // bouncing missile obeys normal radius attack flags
  {"NOSOUNDCUTOFF",    0x00000400, 3}, // actor can play any number of sounds
  {"RAVENRESPAWN",     0x00000800, 3}, // special respawns Raven style
  {"NOTSHAREWARE",     0x00001000, 3}, // item won't spawn in shareware gamemodes
  {"NOTORQUE",         0x00002000, 3}, // never subject to torque simulation
  {"ALWAYSTORQUE",     0x00004000, 3}, // torque not restricted by comp_falloff
  {"NOZERODAMAGE",     0x00008000, 3}, // missile won't inflict damage if damage is 0
  {"TLSTYLESUB",       0x00010000, 3}, // use subtractive blending map
  {"TOTALINVISIBLE",   0x00020000, 3}, // thing is totally invisible to monsters
  {"DRAWSBLOOD",       0x00040000, 3}, // missile draws blood
  {"SPACPUSHWALL",     0x00080000, 3}, // thing can activate push walls
  {"NOSPECIESINFIGHT", 0x00100000, 3}, // no infighting in this species, but still damage
  {"HARMSPECIESMISSILE", 0x00200000, 3}, // harmed even by projectiles of same species
  {"FRIENDFOEMISSILE",   0x00400000, 3}, // friends and foes of same species hurt each other
  {"BLOODLESSIMPACT",    0x00800000, 3}, // doesn't draw blood when it hits or rips a thing
  {"HERETICBOUNCES",     0x01000000, 3}, // thing bounces à la Heretic
  {"MONSTERPASS",        0x02000000, 3}, // not blocked by blockmonsters
  {"LOWAIMPRIO",         0x04000000, 3}, // less likely to be autoaimed
  {"STICKYCARRY",        0x08000000, 3}, // can carry other things on top of it
  {"SETTARGETONDEATH",   0x10000000, 3}, // target is updated even when one-shot
  {"SLIDEOVERTHINGS",    0x20000000, 3}, // thing will keep sliding when on top of things
  {"UNSTEPPABLE",        0x40000000, 3}, // thing cannot be stepped on like stairs
  {"RANGEEIGHTH",        0x80000000, 3}, // uses eighth actual distance

   // flags5 bits
  {"NOTAUTOAIMED",       0x00000001, 4}, // can't be autoaimed (for real)
  {"FULLVOLSOUNDS",      0x00000002, 4}, // full-volume see/death sounds

  { nullptr,             0 }             // nullptr terminator
};

// haleyjd 02/19/04: new dehflagset for combined flags

static dehflagset_t dehacked_flags =
{
   deh_mobjflags, // flaglist
};

static dehflags_t deh_mbf21mobjflags[] =
{
   { "LOGRAV",         0x00000001 }, // low gravity
   { "SHORTMRANGE",    0x00000002 }, // short missile range
   { "DMGIGNORED",     0x00000004 }, // other things ignore its attacks
   { "NORADIUSDMG",    0x00000008 }, // doesn't take splash damage
   { "FORCERADIUSDMG", 0x00000010 }, // causes splash damage even if target immune
   { "HIGHERMPROB",    0x00000020 }, // higher missile attack probability
   { "RANGEHALF",      0x00000040 }, // use half distance for missile attack probability
   { "NOTHRESHOLD",    0x00000080 }, // no targeting threshold
   { "LONGMELEE",      0x00000100 }, // long melee range
   { "BOSS",           0x00000200 }, // full volume see / death sound + splash immunity
   { "MAP07BOSS1",     0x00000400 }, // Tag 666 "boss" on doom 2 map 7
   { "MAP07BOSS2",     0x00000800 }, // Tag 667 "boss" on doom 2 map 7
   { "E1M8BOSS",       0x00001000 }, // E1M8 boss
   { "E2M8BOSS",       0x00002000 }, // E2M8 boss
   { "E3M8BOSS",       0x00004000 }, // E3M8 boss
   { "E4M6BOSS",       0x00008000 }, // E4M6 boss
   { "E4M8BOSS",       0x00010000 }, // E4M8 boss
   { "RIP",            0x00020000 }, // projectile rips through targets
   { "FULLVOLSOUNDS",  0x00040000 }, // full volume see / death sound
   { nullptr,          0          }  // nullptr terminator
};

static dehflagset_t dehacked_mbf21flags =
{
   deh_mbf21mobjflags, // flaglist
};

dehflagremap_t dehacked_mbf21mobjflags_remappings[earrlen(deh_mbf21mobjflags)] =
{
   { 0x00000001, 0, MF2_LOGRAV,         1 },
   { 0x00000002, 0, MF2_SHORTMRANGE,    1 },
   { 0x00000004, 0, MF3_DMGIGNORED,     2 },
   { 0x00000008, 0, MF4_NORADIUSDMG,    3 },
   { 0x00000010, 0, MF4_FORCERADIUSDMG, 3 },
   { 0x00000020, 0, MF2_HIGHERMPROB,    1 },
   { 0x00000040, 0, MF2_RANGEHALF,      1 },
   { 0x00000080, 0, MF3_NOTHRESHOLD,    2 },
   { 0x00000100, 0, MF2_LONGMELEE,      1 },
   { 0x00000200, 0, MF2_BOSS,           1 },
   { 0x00000400, 0, MF2_MAP07BOSS1,     1 },
   { 0x00000800, 0, MF2_MAP07BOSS2,     1 },
   { 0x00001000, 0, MF2_E1M8BOSS,       1 },
   { 0x00002000, 0, MF2_E2M8BOSS,       1 },
   { 0x00004000, 0, MF2_E3M8BOSS,       1 },
   { 0x00008000, 0, MF2_E4M6BOSS,       1 },
   { 0x00010000, 0, MF2_E4M8BOSS,       1 },
   { 0x00020000, 0, MF3_RIP,            2 },
   { 0x00040000, 0, MF5_FULLVOLSOUNDS,  4 },
   { 0,          0, 0,                  0 }
};


// STATE - Dehacked block name = "Frame" and "Pointer"
// Usage: Frame nn
// Usage: Pointer nn (Frame nn)
// These are indexed separately, for lookup to the actual
// function pointers.  Here we'll take whatever Dehacked gives
// us and go from there.  The (Frame nn) after the pointer is the
// real place to put this value.  The "Pointer" value is an xref
// that Dehacked uses and is useless to us.
// * states are base zero and have a dummy #0 (TROO)

enum dehstateid_e : int
{
   dehstateid_sprite,
   dehstateid_frame,
   dehstateid_tics,
   dehstateid_nextstate,
   dehstateid_action,
   dehstateid_misc1,
   dehstateid_misc2,
   dehstateid_particleEvent,
   dehstateid_args1,
   dehstateid_args2,
   dehstateid_args3,
   dehstateid_args4,
   dehstateid_args5,
   dehstateid_args6,
   dehstateid_args7,
   dehstateid_args8,
   NUMDEHSTATEIDS
};

static constexpr const char *deh_state[NUMDEHSTATEIDS] =
{
  "Sprite number",    // .sprite (spritenum_t) // an enum
  "Sprite subnumber", // .frame
  "Duration",         // .tics
  "Next frame",       // .nextstate (statenum_t)
  // This is set in a separate "Pointer" block from Dehacked
  "Codep Frame",      // pointer to first use of action (actionf_t)
  "Unknown 1",        // .misc1
  "Unknown 2",        // .misc2
  "Particle event",   // haleyjd 08/09/02: particle event num
  "Args1",            // haleyjd 08/09/02: arguments
  "Args2",
  "Args3",
  "Args4",
  "Args5",
  "Args6",
  "Args7",
  "Args8",
};

// SFXINFO_STRUCT - Dehacked block name = "Sounds"
// Sound effects, typically not changed (redirected, and new sfx put
// into the pwad, but not changed here.  Can you tell that Greg didn't
// know what they were for, mostly?  Can you tell that I don't either?
// Mostly I just put these into the same slots as they are in the struct.
// This may not be supported in our -deh option if it doesn't make sense by then.

// * sounds are base zero but have a dummy #0

enum dehsfxinfoid_e : int
{
   dehsfxinfoid_offset,
   dehsfxinfoid_singularity,
   dehsfxinfoid_priority,
   dehsfxinfoid_link,
   dehsfxinfoid_pitch,
   dehsfxinfoid_volume,
   dehsfxinfoid_data,
   dehsfxinfoid_usefulness,
   dehsfxinfoid_lumpnum,
   NUMDEHSFXINFOIDS
};

static constexpr const char *deh_sfxinfo[] =
{
  "Offset",     // pointer to a name string, changed in text
  "Zero/One",   // .singularity (int, one at a time flag)
  "Value",      // .priority
  "Zero 1",     // .link (sfxinfo_t*) referenced sound if linked
  "Zero 2",     // .pitch
  "Zero 3",     // .volume
  "Zero 4",     // .data (SAMPLE*) sound data
  "Neg. One 1", // .usefulness
  "Neg. One 2"  // .lumpnum
};

// MUSICINFO is not supported in Dehacked.  Ignored here.
// * music entries are base zero but have a dummy #0

// SPRITE - Dehacked block name = "Sprite"
// Usage = Sprite nn
// Sprite redirection by offset into the text area - unsupported by BOOM
// * sprites are base zero and dehacked uses it that way.

static constexpr const char *deh_sprite[] =
{
  "Offset"      // supposed to be the offset into the text section
};

// AMMO - Dehacked block name = "Ammo"
// usage = Ammo n (name)
// Ammo information for the few types of ammo

// WEAPONS - Dehacked block name = "Weapon"
// Usage: Weapon nn (name)
// Basically a list of frames and what kind of ammo (see above)it uses.

enum dehweaponid_e : int
{
   dehweaponid_ammoType,
   dehweaponid_deselect,
   dehweaponid_select,
   dehweaponid_bobbing,
   dehweaponid_shooting,
   dehweaponid_firing,
   dehweaponid_ammoPerShot,
   dehweaponid_mbf21flags,
   NUMDEHWEAPONIDS
};

static constexpr const char *deh_weapon[NUMDEHWEAPONIDS] =
{
  "Ammo type",      // .ammo
  "Deselect frame", // .upstate
  "Select frame",   // .downstate
  "Bobbing frame",  // .readystate
  "Shooting frame", // .atkstate
  "Firing frame",   // .flashstate
  "Ammo per shot",  // haleyjd 08/10/02: .ammopershot
};

static dehflags_t deh_mbf21weaponflags[] =
{
   { "NOTHRUST",       0x00000001 },
   { "SILENT",         0x00000002 },
   { "NOAUTOFIRE",     0x00000004 },
   { "FLEEMELEE",      0x00000008 },
   { "AUTOSWITCHFROM", 0x00000010 },
   { "NOAUTOSWITCHTO", 0x00000020 },
   { nullptr,          0          }
};

static dehflagset_t dehacked_mbf21weaponflags =
{
   deh_mbf21weaponflags, // flaglist
};

dehflagremap_t dehacked_mbf21weaponflags_remappings[earrlen(deh_mbf21weaponflags)] =
{
   { 0x00000001, 0, WPF_NOTHRUST,       0 },
   { 0x00000002, 0, WPF_SILENT,         0 },
   { 0x00000004, 0, WPF_NOAUTOFIRE,     0 },
   { 0x00000008, 0, WPF_FLEEMELEE,      0 },
   { 0x00000010, 0, WPF_AUTOSWITCHFROM, 0 },
   { 0x00000020, 0, WPF_NOAUTOSWITCHTO, 0 },
   { 0,          0, 0,                  0 }
};

// MISC - Dehacked block name = "Misc"
// Usage: Misc 0
// Always uses a zero in the dehacked file, for consistency.  No meaning.

// CHEATS - Dehacked block name = "Cheat"
// Usage: Cheat 0
// Always uses a zero in the dehacked file, for consistency.  No meaning.
// These are just plain funky terms compared with id's
//
// killough 4/18/98: integrated into main cheat table now (see st_stuff.c)
// haleyjd 08/21/13: Moved DEH-related cheat data back here where it belongs,
// instead of having it pollute the main cheat table.

struct dehcheat_t
{
   cheatnum_e cheatnum; // index of the cheat in the cheat array
   const char *name;    // DEH name of the cheat
};

static dehcheat_t deh_cheats[] =
{
   { CHEAT_IDMUS,      "Change music"     },
   { CHEAT_IDCHOPPERS, "Chainsaw"         },
   { CHEAT_IDDQD,      "God mode"         },
   { CHEAT_IDKFA,      "Ammo & Keys"      },
   { CHEAT_IDFA,       "Ammo"             },
   { CHEAT_IDSPISPOPD, "No Clipping 1"    },
   { CHEAT_IDCLIP,     "No Clipping 2"    },
   { CHEAT_IDBEHOLDV,  "Invincibility"    },
   { CHEAT_IDBEHOLDS,  "Berserk"          },
   { CHEAT_IDBEHOLDI,  "Invisibility"     },
   { CHEAT_IDBEHOLDR,  "Radiation Suit"   },
   { CHEAT_IDBEHOLDA,  "Auto-map"         },
   { CHEAT_IDBEHOLDL,  "Lite-Amp Goggles" },
   { CHEAT_IDBEHOLD,   "BEHOLD menu"      },
   { CHEAT_IDCLEV,     "Level Warp"       },
   { CHEAT_IDMYPOS,    "Player Position"  },
   { CHEAT_IDDT,       "Map cheat"        }
};

// TEXT - Dehacked block name = "Text"
// Usage: Text fromlen tolen
// Dehacked allows a bit of adjustment to the length (why?)
//   haleyjd 03/25/10: Because Watcom aligns strings to 4-byte boundaries,
//   leaving as many as 3 unused bytes before the next string in the binary.

// haleyjd: moved text table to d_dehtbl.c

// BEX extension [CODEPTR]
// Usage: Start block, then each line is:
// FRAME nnn = PointerMnemonic

// haleyjd: moved BEX ptr table to d_dehtbl.c

// haleyjd 03/25/10: eliminated deh_codeptr cache by storing original
// codepointer values in the states themselves.

// haleyjd 10/08/06: DeHackEd log file made module-global
static FILE *fileout;

//
// deh_LogPrintf
//
// haleyjd 10/08/06: cleaned up some more of the mess in here by creating this
// logging function, similar to the one used by EDF.
//
static void deh_LogPrintf(E_FORMAT_STRING(const char *fmt), ...)
{
   if(fileout)
   {
      va_list argptr;
      va_start(argptr, fmt);
      vfprintf(fileout, fmt, argptr);
      va_end(argptr);
   }
}

//
// deh_OpenLog
//
// Opens (or re-opens) the DeHackEd log file. Logging may be done to stdout
// when no file name is given, or a file cannot be created.
//
static void deh_OpenLog(const char *fn)
{
   static bool firstfile = true; // to allow append to output log

   if(!strcmp(fn, "-"))
      fileout = stdout;
   else if(!(fileout = fopen(fn, firstfile ? "wt" : "at")))
   {
      usermsg("Could not open -dehout file %s\n... using stdout.", fn);
      fileout = stdout;
   }
   firstfile = false;
}

//
// deh_CloseLog
//
// Closes the DeHackEd log if one is open.
//
static void deh_CloseLog()
{
   // haleyjd 05/21/02: must check fileout for validity!
   if(fileout && fileout != stdout)
      fclose(fileout);

   fileout = nullptr;
}

// ====================================================================
// ProcessDehFile
// Purpose: Read and process a DEH or BEX file
// Args:    filename    -- name of the DEH/BEX file
//          outfilename -- output file (DEHOUT.TXT), appended to here
// Returns: void
//
// killough 10/98:
// substantially modified to allow input from wad lumps instead of .deh files.
//
// haleyjd 09/07/01: this can be called while in video mode now,
// so printf calls needed to be converted to usermsg calls
//
void ProcessDehFile(const char *filename, const char *outfilename, int lumpnum)
{
   DWFILE infile;                 // killough 10/98
   char inbuffer[DEH_BUFFERMAX];  // Place to put the primary infostring

   // Open output file if we're writing output
   if(outfilename && *outfilename && !fileout)
      deh_OpenLog(outfilename);

   // killough 10/98: allow DEH files to come from wad lumps

   if(filename)
   {
      infile.openFile(filename, "rt");

      if(!infile.isOpen())
      {
         usermsg("-deh file %s not found", filename);
         return;  // should be checked up front anyway
      }
   }
   else  // DEH file comes from lump indicated by third argument
   {
      infile.openLump(lumpnum);
      filename = "(WAD)";
   }

   usermsg("Loading DEH file %s",filename);

   deh_LogPrintf("\nLoading DEH file %s\n\n", filename);

   // loop until end of file

   while(infile.getStr(inbuffer, sizeof(inbuffer)))
   {
      lfstrip(inbuffer);

      deh_LogPrintf("Line='%s'\n", inbuffer);

      if(!*inbuffer || *inbuffer == '#' || *inbuffer == ' ')
         continue; /* Blank line or comment line */

      // -- If DEH_BLOCKMAX is set right, the processing is independently
      // -- handled based on data in the deh_blocks[] structure array

      // killough 10/98: INCLUDE code rewritten to allow arbitrary nesting,
      // and to greatly simplify code, fix memory leaks, other bugs

      if(!strncasecmp(inbuffer,"INCLUDE",7)) // include a file
      {
         // preserve state while including a file
         // killough 10/98: moved to here

         char *nextfile;
         bool oldnotext = includenotext;       // killough 10/98

         // killough 10/98: exclude if inside wads (only to discourage
         // the practice, since the code could otherwise handle it)

         if(infile.isLump())
         {
            deh_LogPrintf("No files may be included from wads: %s\n", inbuffer);
            continue;
         }

         // check for no-text directive, used when including a DEH
         // file but using the BEX format to handle strings

         if(!strncasecmp(nextfile = ptr_lstrip(inbuffer+7),"NOTEXT",6))
         {
            includenotext = true;
            nextfile = ptr_lstrip(nextfile+6);
         }

         deh_LogPrintf("Branching to include file %s...\n", nextfile);

         // killough 10/98:
         // Second argument must be nullptr to prevent closing fileout too soon
         ProcessDehFile(nextfile, nullptr, 0); // do the included file

         includenotext = oldnotext;

         deh_LogPrintf("...continuing with %s\n", filename);

         continue;
      }

      for(size_t i = 0; i < earrlen(deh_blocks); i++)
      {
         if(!strncasecmp(inbuffer, deh_blocks[i].key, strlen(deh_blocks[i].key)))
         {
            // matches one
            deh_LogPrintf("Processing function [%d] for %s\n",
                          i, deh_blocks[i].key);

            deh_blocks[i].fptr(&infile, inbuffer);  // call function

            break;  // we got one, that's enough for this block
         }
      }
   }

   // killough 10/98: only at top recursion level
   if(outfilename)
      deh_CloseLog();
}


// ====================================================================
// deh_procBexCodePointers
// Purpose: Handle [CODEPTR] block, BOOM Extension
// Args:    fpin  -- input file stream
//          line  -- current line in file to process
// Returns: void
//
// haleyjd 03/14/03: rewritten to replace linear search on deh_bexptrs
// table with in-table chained hashing -- table is now in d_dehtbl.c
//
static void deh_procBexCodePointers(DWFILE *fpin, char *line)
{
   char key[DEH_MAXKEYLEN];
   char inbuffer[DEH_BUFFERMAX];
   int indexnum;
   char mnemonic[DEH_MAXKEYLEN];  // to hold the codepointer mnemonic
   deh_bexptr *bexptr = nullptr; // haleyjd 03/14/03

   // Ty 05/16/98 - initialize it to something, dummy!
   strncpy(inbuffer, line, DEH_BUFFERMAX);

   // for this one, we just read 'em until we hit a blank line
   while(!fpin->atEof() && *inbuffer && (*inbuffer != ' '))
   {
      if(!fpin->getStr(inbuffer, sizeof(inbuffer)))
         break;

      lfstrip(inbuffer);
      if(!*inbuffer)
         break;   // killough 11/98: really exit on blank line

      // killough 8/98: allow hex numbers in input:
      if((3 != sscanf(inbuffer,"%s %i = %" DEH_MAXKEYLEN_FMT "s", key, &indexnum, mnemonic))
         || strcasecmp(key, "FRAME")) // NOTE: different format from normal
      {
         deh_LogPrintf(
            "Invalid BEX codepointer line - must start with 'FRAME': '%s'\n",
            inbuffer);
         return;  // early return
      }

      // haleyjd: resolve DeHackEd num of state through EDF
      indexnum = E_GetStateNumForDEHNum(indexnum);

      deh_LogPrintf("Processing pointer at index %d: %s\n", indexnum, mnemonic);

      // haleyjd 03/14/03: why do this? how wasteful and useless...
      //strcpy(key,"A_");  // reusing the key area to prefix the mnemonic

      memset(key, 0, DEH_MAXKEYLEN);
      strcat(key, ptr_lstrip(mnemonic));

      // haleyjd 03/14/03: rewrite for hash chaining begins here
      bexptr = D_GetBexPtr(key);

      if(!bexptr)
      {
         deh_LogPrintf("Invalid pointer mnemonic '%s' for frame %d\n",
                       key, indexnum);
      }
      else
      {
         // copy codepointer to state
         states[indexnum]->action = bexptr->cptr;
         deh_LogPrintf("- applied codepointer %p to states[%d]\n",
                       bexptr->cptr, indexnum);
      }
   }
   return;
}

//
// deh_ParseFlag
//
// davidph 01/14/14: split from deh_ParseFlags
//
dehflags_t *deh_ParseFlag(const dehflagset_t *flagset, const char *name)
{
   int mode = flagset->mode;

   for(dehflags_t *flag = flagset->flaglist; flag->name; ++flag)
   {
      if(!strcasecmp(name, flag->name) &&
         (flag->index == mode || mode == DEHFLAGS_MODE_ALL))
      {
         return flag;
      }
   }

   return nullptr;
}

dehflags_t *deh_ParseFlagCombined(const char *name)
{
   dehacked_flags.mode = DEHFLAGS_MODE_ALL;

   return deh_ParseFlag(&dehacked_flags, name);
}

// ============================================================
// deh_ParseFlags
// Purpose: Handle thing flag fields in a general manner
// Args:    flagset -- pointer to a dehflagset_t object
//          strval  -- ptr-to-ptr to string containing flags
//                     Note: MUST be a mutable string pointer!
// Returns: Nothing. Results for each parsing mode are written
//          into the corresponding index of the results array
//          within the flagset object.
//
// haleyjd 11/03/02: generalized from code that was previously below
// haleyjd 04/10/03: made global for use in EDF and ExtraData
// haleyjd 02/19/04: rewrote for combined flags support
//
void deh_ParseFlags(dehflagset_t *flagset, char **strval)
{
   unsigned int *results  = flagset->results;  // pointer to results array

   // haleyjd: init all results to zero
   memset(results, 0, MAXFLAGFIELDS * sizeof(*results));

   // killough 10/98: replace '+' kludge with strtok() loop
   // Fix error-handling case ('found' var wasn't being reset)
   //
   // Use OR logic instead of addition, to allow repetition

   for(;(*strval = strtok(*strval, ",+| \t\f\r")); *strval = nullptr)
   {
      dehflags_t *flag = deh_ParseFlag(flagset, *strval);

      if(flag)
      {
         deh_LogPrintf("ORed value 0x%08lx %s\n", flag->value, *strval);
         results[flag->index] |= flag->value;
      }
      else
      {
         deh_LogPrintf("Could not find bit mnemonic %s\n", *strval);
      }
   }
}

//
// Functions for external use (ie EDF) -- this prevents the need for
// the flags data above to be global, and simplifies the external
// interface.
//
unsigned int deh_ParseFlagsSingle(const char *strval, int mode)
{
   char *buffer;
   char *bufferptr;

   bufferptr = buffer = estrdup(strval);

   dehacked_flags.mode = mode;

   deh_ParseFlags(&dehacked_flags, &bufferptr);

   efree(buffer);

   return dehacked_flags.results[mode];
}

unsigned int *deh_ParseFlagsCombined(const char *strval)
{
   char *buffer;
   char *bufferptr;

   bufferptr = buffer = estrdup(strval);

   dehacked_flags.mode = DEHFLAGS_MODE_ALL;

   deh_ParseFlags(&dehacked_flags, &bufferptr);

   efree(buffer);

   return dehacked_flags.results;
}

static unsigned int *deh_ParseFlagsCombinedRemapped(
   const char *strval, dehflagset_t *flagset, dehflagremap_t *flagremapping
)
{
   char *buffer;
   char *bufferptr;
   unsigned int results[MAXFLAGFIELDS];

   bufferptr = buffer = estrdup(strval);

   flagset->mode = DEHFLAGS_MODE_ALL;

   deh_ParseFlags(flagset, &bufferptr);

   efree(buffer);

   memcpy(results, flagset->results, MAXFLAGFIELDS * sizeof(unsigned int));
   memset(flagset->results, 0, MAXFLAGFIELDS * sizeof(unsigned int));
   for(dehflagremap_t *currRemap = flagremapping; currRemap->inFlag; currRemap++)
   {
      if(results[currRemap->inIndex] & currRemap->inFlag)
         flagset->results[currRemap->outIndex] |= currRemap->outFlag;
   }

   return flagset->results;
}

static void SetMobjInfoValue(int mobjInfoIndex, int keyIndex, int value)
{
   mobjinfo_t *mi;

   if(mobjInfoIndex < 0 || mobjInfoIndex >= NUMMOBJTYPES)
      return;

   mi = mobjinfo[mobjInfoIndex];

   // haleyjd 07/05/03: field resolution adjusted for EDF

   switch(keyIndex)
   {
   case dehmobjinfoid_doomednum:
      mi->doomednum = value;
      break;
   case dehmobjinfoid_spawnstate:
   {
      const int statenum = E_GetStateNumForDEHNum(value);
      mi->spawnstate = statenum;
      states[statenum]->flags |= STATEFI_VANILLA0TIC;
      break;
   }
   case dehmobjinfoid_spawnhealth:
      mi->spawnhealth = value;
      E_ThingDefaultGibHealth(mi); // haleyjd 01/02/15: reset gibhealth
      break;
   case dehmobjinfoid_seestate:
      mi->seestate = E_GetStateNumForDEHNum(value);
      break;
   case dehmobjinfoid_seesound:
      mi->seesound = value;
      break;
   case dehmobjinfoid_reactiontime:
      mi->reactiontime = value;
      break;
   case dehmobjinfoid_attacksound:
      mi->attacksound = value;
      break;
   case dehmobjinfoid_painstate:
      mi->painstate = E_GetStateNumForDEHNum(value);
      break;
   case dehmobjinfoid_painchance:
      mi->painchance = value;
      break;
   case dehmobjinfoid_painsound:
      mi->painsound = value;
      break;
   case dehmobjinfoid_meleestate:
      mi->meleestate = E_GetStateNumForDEHNum(value);
      break;
   case dehmobjinfoid_missilestate:
      mi->missilestate = E_GetStateNumForDEHNum(value);
      break;
   case dehmobjinfoid_deathstate:
      mi->deathstate = E_GetStateNumForDEHNum(value);
      break;
   case dehmobjinfoid_xdeathstate:
      mi->xdeathstate = E_GetStateNumForDEHNum(value);
      break;
   case dehmobjinfoid_deathsound:
      mi->deathsound = value;
      break;
   case dehmobjinfoid_speed:
      mi->speed = value;
      break;
   case dehmobjinfoid_radius:
      mi->radius = value;
      break;
   case dehmobjinfoid_height:
      mi->height    = value;
      mi->c3dheight = 0; // haleyjd 08/23/09
      break;
   case dehmobjinfoid_mass:
      mi->mass = value;
      break;
   case dehmobjinfoid_damage:
      mi->damage = value;
      break;
   case dehmobjinfoid_activesound:
      mi->activesound = value;
      break;
   case dehmobjinfoid_flags:
      mi->flags = value;
      if(mi->flags & MF_SPAWNCEILING)
         mi->c3dheight = 0; // haleyjd 08/23/09
      break;
   case dehmobjinfoid_flags2:
      mi->flags2 = value;
      break;
   case dehmobjinfoid_raisestate:
      mi->raisestate = E_GetStateNumForDEHNum(value);
      break;
   case dehmobjinfoid_translucency:
      mi->translucency = value;
      break;
   case dehmobjinfoid_flags3:
      mi->flags3 = value;
      break;
   case dehmobjinfoid_bloodcolor:
      mi->bloodcolor = value;
      break;
   case dehmobjinfoid_droppeditem:
      E_SetDropItem(mi, value);
      break;
   case dehmobjinfoid_fastspeed:
      G_SpeedSetAddThing(mobjInfoIndex, mi->speed, value);
      break;
   default:
      break;
   }
}

// ============================================================
// deh_procThing
// Purpose: Handle DEH Thing block
// Args:    fpin  -- input file stream
//          line  -- current line in file to process
// Returns: void
//
// Ty 8/27/98 - revised to also allow mnemonics for
// bit masks for monster attributes
//
static void deh_procThing(DWFILE *fpin, char *line)
{
   char key[DEH_MAXKEYLEN];
   char inbuffer[DEH_BUFFERMAX];
   int value;      // All deh values are ints or longs
   int indexnum;
   char *strval;

   strncpy(inbuffer, line, DEH_BUFFERMAX);
   deh_LogPrintf("Thing line: '%s'\n", inbuffer);

   // killough 8/98: allow hex numbers in input:
   const int varcount = sscanf(inbuffer, "%" DEH_MAXKEYLEN_FMT "s %i", key, &indexnum);
   deh_LogPrintf("count=%d, Thing %d\n", varcount, indexnum);

   // Note that the mobjinfo[] array is base zero, but object numbers
   // in the dehacked file start with one.  Grumble.
   // haleyjd: not as big an issue with EDF, as it uses a hash lookup
   // --indexnum;  <-- old code

   indexnum = E_GetThingNumForDEHNum(indexnum);

   // now process the stuff
   // Note that for Things we can look up the key and use its offset
   // in the array of key strings as an int offset in the structure

   // get a line until a blank or end of file--it's not
   // blank now because it has our incoming key in it
   while(!fpin->atEof() && *inbuffer && (*inbuffer != ' '))
   {
      if(!fpin->getStr(inbuffer, sizeof(inbuffer)))
         break;

      lfstrip(inbuffer);  // toss the end of line

      // killough 11/98: really bail out on blank lines (break != continue)
      if(!*inbuffer) break;  // bail out with blank line between sections
      if(!deh_GetData(inbuffer, key, &value, &strval)) // returns TRUE if ok
      {
         deh_LogPrintf("Bad data pair in '%s'\n", inbuffer);
         continue;
      }

      const int dehmobjinfoid = E_StrToNumLinear(deh_mobjinfo, DEH_MOBJINFOMAX, key);
      if(dehmobjinfoid != DEH_MOBJINFOMAX)
      {
         if(dehmobjinfoid == dehmobjinfoid_flags)
         {
            if(!value)
            {
               dehacked_flags.mode = DEHFLAGS_MODE1;

               deh_ParseFlags(&dehacked_flags, &strval);

               value = dehacked_flags.results[DEHFLAGS_MODE1];

               // Don't worry about conversion -- simply print values
               deh_LogPrintf("Bits = 0x%08lX = %ld \n", value, value);
            }

            SetMobjInfoValue(indexnum, dehmobjinfoid_flags, value);
         }
         else if(dehmobjinfoid == dehmobjinfoid_flags2)
         {
            // haleyjd 04/09/99: flags2 support
            if(!value)
            {
               dehacked_flags.mode = DEHFLAGS_MODE2;

               deh_ParseFlags(&dehacked_flags, &strval);

               value = dehacked_flags.results[DEHFLAGS_MODE2];

               deh_LogPrintf("Bits2 = 0x%08lX = %ld \n", value, value);
            }

            SetMobjInfoValue(indexnum, dehmobjinfoid_flags2, value);
         }
         else if(dehmobjinfoid == dehmobjinfoid_flags3)
         {
            // haleyjd 02/02/03: flags3 support
            if(!value)
            {
               dehacked_flags.mode = DEHFLAGS_MODE3;

               deh_ParseFlags(&dehacked_flags, &strval);

               value = dehacked_flags.results[DEHFLAGS_MODE3];

               deh_LogPrintf("Bits3 = 0x%08lX = %ld \n", value, value);
            }

            SetMobjInfoValue(indexnum, dehmobjinfoid_flags3, value);
         }
         else if(dehmobjinfoid == dehmobjinfoid_mbf21flags)
         {
            if(!value)
            {
               mobjinfo_t *mi = mobjinfo[indexnum];

               deh_ParseFlagsCombinedRemapped(strval, &dehacked_mbf21flags, dehacked_mbf21mobjflags_remappings);

               deh_LogPrintf("MBF21 Bits = %s \n", strval);

               mi->flags  = dehacked_mbf21flags.results[DEHFLAGS_MODE1];
               mi->flags2 = dehacked_mbf21flags.results[DEHFLAGS_MODE2];
               mi->flags3 = dehacked_mbf21flags.results[DEHFLAGS_MODE3];
               mi->flags4 = dehacked_mbf21flags.results[DEHFLAGS_MODE4];
               mi->flags5 = dehacked_mbf21flags.results[DEHFLAGS_MODE5];
            }

         }
         else
            SetMobjInfoValue(indexnum, dehmobjinfoid, value);

         deh_LogPrintf("Assigned %d to %s(%d) at index %d\n",
                       value, key, indexnum, dehmobjinfoid);
      }
   }
   return;
}

// ====================================================================
// deh_createArgList
// Purpose: Create an argument list object for a frame that doesn't
//          already have one.
// Args:    state -- state to modify
// Returns: void
//
static void deh_createArgList(state_t *state)
{
   if(!state->args)
      state->args = ecalloc(arglist_t *, 1, sizeof(arglist_t));
}

// ====================================================================
// deh_procFrame
// Purpose: Handle DEH Frame block
// Args:    fpin  -- input file stream
//          line  -- current line in file to process
// Returns: void
//
static void deh_procFrame(DWFILE *fpin, char *line)
{
   char key[DEH_MAXKEYLEN];
   char inbuffer[DEH_BUFFERMAX];
   int value;      // All deh values are ints or longs
   int indexnum;

   strncpy(inbuffer,line,DEH_BUFFERMAX);

   // killough 8/98: allow hex numbers in input:
   sscanf(inbuffer,"%" DEH_MAXKEYLEN_FMT "s %i",key, &indexnum);

   // haleyjd: resolve state number through EDF
   indexnum = E_GetStateNumForDEHNum(indexnum);

   deh_LogPrintf("Processing Frame at index %d: %s\n", indexnum, key);

   while(!fpin->atEof() && *inbuffer && (*inbuffer != ' '))
   {
      if(!fpin->getStr(inbuffer, sizeof(inbuffer)))
         break;

      lfstrip(inbuffer);
      if(!*inbuffer)
         break; // killough 11/98

      if(!deh_GetData(inbuffer, key, &value, nullptr)) // returns TRUE if ok
      {
         deh_LogPrintf("Bad data pair in '%s'\n", inbuffer);
         continue;
      }

      // haleyjd 08/09/02: significant reformatting, added new
      // fields

      const int dehstateid = E_StrToNumLinear(deh_state, NUMDEHSTATEIDS, key);
      switch(dehstateid)
      {
      case dehstateid_sprite:  // Sprite number
         deh_LogPrintf(" - sprite = %ld\n", value);
         states[indexnum]->sprite = (spritenum_t)value;
         break;
      case dehstateid_frame:  // Sprite subnumber
         deh_LogPrintf(" - frame = %ld\n", value);
         states[indexnum]->frame = value;
         break;
      case dehstateid_tics:  // Duration
         deh_LogPrintf(" - tics = %ld\n", value);
         states[indexnum]->tics = value;
         states[indexnum]->flags |= STATEFI_VANILLA0TIC;
         break;
      case dehstateid_nextstate:  // Next frame
         deh_LogPrintf(" - nextstate = %ld\n", value);

         // haleyjd: resolve state number through EDF
         states[indexnum]->nextstate = E_GetStateNumForDEHNum(value);

         break;
      case dehstateid_action:  // Codep frame (not set in Frame deh block)
         deh_LogPrintf(" - codep, should not be set in Frame section!\n");
         /* nop */ ;
         break;
      case dehstateid_misc1:  // Unknown 1
         deh_LogPrintf(" - misc1 = %ld\n", value);
         states[indexnum]->misc1 = value;
         break;
      case dehstateid_misc2:  // Unknown 2
         deh_LogPrintf(" - misc2 = %ld\n", value);
         states[indexnum]->misc2 = value;
         break;
      case dehstateid_particleEvent: // Particle event
         // haleyjd 08/09/02: particle event setting
         deh_LogPrintf(" - particle_evt = %ld\n", value);
         states[indexnum]->particle_evt = value;
         break;
      case dehstateid_args1: // Args1
      case dehstateid_args2: // Args2
      case dehstateid_args3: // Args3
      case dehstateid_args4: // Args4
      case dehstateid_args5: // Args5
      case dehstateid_args6: // Args6
      case dehstateid_args7: // Args7
      case dehstateid_args8: // Args8
      {
         const int argIndex = dehstateid - dehstateid_args1;
         deh_LogPrintf(" - args[%d] = %ld\n", argIndex, value);

         deh_createArgList(states[indexnum]);
         E_SetArgFromNumber(states[indexnum]->args, argIndex, value);
      }
         break;
      default:
         deh_LogPrintf("Invalid frame string index for '%s'\n", key);
         break;
      }
   }
}

// ====================================================================
// deh_procPointer
// Purpose: Handle DEH Code pointer block, can use BEX [CODEPTR] instead
// Args:    fpin  -- input file stream
//          line  -- current line in file to process
// Returns: void
//
static void deh_procPointer(DWFILE *fpin, char *line) // done
{
   char key[DEH_MAXKEYLEN];
   char inbuffer[DEH_BUFFERMAX];
   int value;      // All deh values are ints or longs
   int indexnum;
   int i; // looper
   int oldindex; // haleyjd 7/10/03 - preserve for output

   strncpy(inbuffer, line, DEH_BUFFERMAX);
   // NOTE: different format from normal

   // killough 8/98: allow hex numbers in input, fix error case:
   if(sscanf(inbuffer, "%*s %*i (%" DEH_MAXKEYLEN_FMT "s %i)", key, &indexnum) != 2)
   {
      deh_LogPrintf("Bad data pair in '%s'\n", inbuffer);
      return;
   }

   // haleyjd: resolve state num through EDF; preserve old for output
   oldindex = indexnum;
   indexnum = E_GetStateNumForDEHNum(indexnum);

   deh_LogPrintf("Processing Pointer at index %d: %s\n", indexnum, key);

   while(!fpin->atEof() && *inbuffer && (*inbuffer != ' '))
   {
      if(!fpin->getStr(inbuffer, sizeof(inbuffer)))
         break;

      lfstrip(inbuffer);
      if(!*inbuffer)
         break;       // killough 11/98

      if(!deh_GetData(inbuffer, key, &value, nullptr)) // returns TRUE if ok
      {
         deh_LogPrintf("Bad data pair in '%s'\n", inbuffer);
         continue;
      }

      // haleyjd: resolve xref state number through EDF
      value = E_GetStateNumForDEHNum(value);

      if(!strcasecmp(key, deh_state[dehstateid_action])) // Codep frame (not set in Frame deh block)
      {
         states[indexnum]->action = states[value]->oldaction;
         deh_LogPrintf(" - applied %p from codeptr[%ld] to states[%d]\n",
                       states[value]->oldaction, value, indexnum);

         // Write BEX-oriented line to match:

         // haleyjd 03/14/03: It's amazing what you can catch just by
         // reformatting some code -- the below line is COMPLETELY
         // incorrect. Must use NUMBEXPTRS, not NUMSTATES.

         // for(i=0;i<NUMSTATES;i++)

         for(i = 0; i < num_bexptrs; i++)
         {
            if(deh_bexptrs[i].cptr == states[value]->oldaction)
            {
               // haleyjd 07/05/03: use oldindex for proper #
               deh_LogPrintf("BEX [CODEPTR] -> FRAME %d = %s\n",
                             oldindex, deh_bexptrs[i].lookup);
               break;
            }
         }
      }
      else
      {
         deh_LogPrintf("Invalid frame pointer index for '%s' at %ld, xref %p\n",
                       key, value, states[value]->oldaction);
      }
   }
}

// ====================================================================
// deh_procSounds
// Purpose: Handle DEH Sounds block
// Args:    fpin  -- input file stream
//          line  -- current line in file to process
// Returns: void
//
static void deh_procSounds(DWFILE *fpin, char *line)
{
   char key[DEH_MAXKEYLEN];
   char inbuffer[DEH_BUFFERMAX];
   int value;      // All deh values are ints or longs
   int indexnum;
   sfxinfo_t *sfx;  // haleyjd 09/03/03

   strncpy(inbuffer, line, DEH_BUFFERMAX);

   // killough 8/98: allow hex numbers in input:
   sscanf(inbuffer, "%" DEH_MAXKEYLEN_FMT "s %i", key, &indexnum);

   deh_LogPrintf("Processing Sounds at index %d: %s\n", indexnum, key);

   // haleyjd 09/03/03: translate indexnum to sfxinfo_t
   if(!(sfx = E_SoundForDEHNum(indexnum)))
   {
      deh_LogPrintf("Bad sound number %d\n", indexnum);
      return; // haleyjd: bugfix!
   }

   while(!fpin->atEof() && *inbuffer && (*inbuffer != ' '))
   {
      if(!fpin->getStr(inbuffer, sizeof(inbuffer)))
         break;

      lfstrip(inbuffer);
      if(!*inbuffer)
         break;         // killough 11/98

      if(!deh_GetData(inbuffer, key, &value, nullptr)) // returns TRUE if ok
      {
         deh_LogPrintf("Bad data pair in '%s'\n", inbuffer);
         continue;
      }

      const int dehsfxinfoid = E_StrToNumLinear(deh_sfxinfo, NUMDEHSFXINFOIDS, key);
      switch(dehsfxinfoid)
      {
      case dehsfxinfoid_offset:  // Offset
         /* nop */ ;  // we don't know what this is, I don't think
         break;
      case dehsfxinfoid_singularity:  // Zero/One
         sfx->singularity = value;
         break;
      case dehsfxinfoid_priority:  // Value
         sfx->priority = value;
         break;
      case dehsfxinfoid_link:  // Zero 1
         ; // haleyjd: NO!
         // sfx->link = (sfxinfo_t *)value;
         break;
      case dehsfxinfoid_pitch:  // Zero 2
         sfx->pitch = value;
         break;
      case dehsfxinfoid_volume:  // Zero 3
         sfx->volume = value;
         break;
      case dehsfxinfoid_data:  // Zero 4
         ; // haleyjd: NO!
         //sfx->data = (void *)value; // killough 5/3/98: changed cast
         break;
      case dehsfxinfoid_usefulness:  // Neg. One 1
         sfx->usefulness = value;
         break;
      case dehsfxinfoid_lumpnum:  // Neg. One 2
         ; // sf: pointless and no longer works
         //sfx->lumpnum = value;
         break;
      default:
         deh_LogPrintf("Invalid sound string index for '%s'\n", key);
         break;
      }
   }
   return;
}

static constexpr const char *deh_itemsForAmmoNum[NUMAMMO][3] =
{
   // ammotype,     small item,   large item
   { "AmmoClip",    "Clip",       "ClipBox"   },
   { "AmmoShell",   "Shell",      "ShellBox"  },
   { "AmmoCell",    "Cell",       "CellPack"  },
   { "AmmoMissile", "RocketAmmo", "RocketBox" },
};

static constexpr const char *deh_giverNames[NUMWEAPONS] =
{
   "GiverFist",
   "GiverPistol",
   "GiverShotgun",
   "GiverChaingun",
   "GiverRocketLauncher",
   "GiverPlasmaRifle",
   "GiverBFG9000",
   "GiverChainsaw",
   "GiverSuperShotgun"
};

// ====================================================================
// deh_procAmmo
// Purpose: Handle DEH Ammo block
// Args:    fpin  -- input file stream
//          line  -- current line in file to process
// Returns: void
//
static void deh_procAmmo(DWFILE *fpin, char *line)
{
   char key[DEH_MAXKEYLEN];
   char inbuffer[DEH_BUFFERMAX];
   int value;      // All deh values are ints or longs
   int indexnum;
   itemeffect_t *smallitem, *largeitem, *ammotype;

   strncpy(inbuffer, line, DEH_BUFFERMAX);

   // killough 8/98: allow hex numbers in input:
   sscanf(inbuffer, "%" DEH_MAXKEYLEN_FMT "s %i", key, &indexnum);

   deh_LogPrintf("Processing Ammo at index %d: %s\n", indexnum, key);

   if(indexnum < 0 || indexnum >= NUMAMMO)
   {
      deh_LogPrintf("Bad ammo number %d of %d\n", indexnum, NUMAMMO);
      return; // haleyjd 10/08/06: bugfix!
   }

   ammotype  = E_ItemEffectForName(deh_itemsForAmmoNum[indexnum][0]);
   smallitem = E_ItemEffectForName(deh_itemsForAmmoNum[indexnum][1]);
   largeitem = E_ItemEffectForName(deh_itemsForAmmoNum[indexnum][2]);

   while(!fpin->atEof() && *inbuffer && (*inbuffer != ' '))
   {
      if(!fpin->getStr(inbuffer, sizeof(inbuffer)))
         break;

      lfstrip(inbuffer);
      if(!*inbuffer)
         break;       // killough 11/98

      if(!deh_GetData(inbuffer, key, &value, nullptr)) // returns TRUE if ok
      {
         deh_LogPrintf("Bad data pair in '%s'\n", inbuffer);
         continue;
      }

      if(!strcasecmp(key, "Max ammo"))
      {
         // max ammo is now stored in the ammotype effect
         if(ammotype)
         {
            ammotype->setInt("maxamount", value);
            ammotype->setInt("ammo.backpackmaxamount", value*2);
         }
      }
      else if(!strcasecmp(key, "Per ammo"))
      {
         // modify the small pickup item
         if(smallitem)
         {
            smallitem->setInt("amount", value);

            // may also modify dropped amount; original is 1/2 clip, but
            // only bullet clips originally obeyed this
            if(indexnum == am_clip)
               smallitem->setInt("dropamount", value / 2);
         }

         // modify the large pickup item; original behavior was 5 clips of
         // the small item amount
         if(largeitem)
            largeitem->setInt("amount", value*5);

         // modify the ammotype's backpack amount; original behavior was 1 clip
         // of the small item amount
         if(ammotype)
            ammotype->setInt("ammo.backpackamount", value);

         // TODO: This could probably do with being more robust
         for(int wp = 0; wp < NUMWEAPONS; wp++)
         {
            weaponinfo_t *weapon = E_WeaponForDEHNum(wp);
            if(weapon == nullptr || weapon->dehnum >= NUMWEAPONS)
               continue;
            itemeffect_t *giver = E_ItemEffectForName(deh_giverNames[weapon->dehnum]);
            itemeffect_t *given = giver->getMetaTable("ammogiven", nullptr);
            if(given == nullptr)
               continue;
            const char *ammostr = given->getString("type", nullptr);
            if(ammostr == nullptr)
               continue;
            if(E_ItemEffectForName(ammostr) == ammotype)
            {
               given->setInt("ammo.dmstay",   value * 5);
               given->setInt("ammo.coopstay", value * 2);
               given->setInt("ammo.give",     value * 2);
               given->setInt("ammo.dropped",  value);
            }
         }
      }
      else
         deh_LogPrintf("Invalid ammo string index for '%s'\n", key);
   }
}

// ====================================================================
// deh_procWeapon
// Purpose: Handle DEH Weapon block
// Args:    fpin  -- input file stream
//          line  -- current line in file to process
// Returns: void
//
static void deh_procWeapon(DWFILE *fpin, char *line)
{
   char key[DEH_MAXKEYLEN];
   char inbuffer[DEH_BUFFERMAX];
   int value;      // All deh values are ints or longs
   int indexnum;
   char* strval;

   // haleyjd 08/10/02: significant reformatting

   strncpy(inbuffer,line,DEH_BUFFERMAX);

   // killough 8/98: allow hex numbers in input:
   sscanf(inbuffer,"%" DEH_MAXKEYLEN_FMT "s %i",key, &indexnum);
   deh_LogPrintf("Processing Weapon at index %d: %s\n", indexnum, key);

   if(indexnum < 0 || indexnum >= NUMWEAPONS)
   {
      deh_LogPrintf("Bad weapon number %d of %d\n", indexnum, NUMWEAPONS);
      return; // haleyjd 10/08/06: bugfix!
   }

   while(!fpin->atEof() && *inbuffer && (*inbuffer != ' '))
   {
      if(!fpin->getStr(inbuffer, sizeof(inbuffer)))
         break;

      lfstrip(inbuffer);
      if(!*inbuffer)
         break;       // killough 11/98

      if(!deh_GetData(inbuffer, key, &value, &strval)) // returns TRUE if ok
      {
         deh_LogPrintf("Bad data pair in '%s'\n", inbuffer);
         continue;
      }

      weaponinfo_t &weaponinfo = *E_WeaponForDEHNum(indexnum);
      // haleyjd: resolution adjusted for EDF
      const int dehweaponid = E_StrToNumLinear(deh_weapon, NUMDEHWEAPONIDS, key);
      switch(dehweaponid)
      {
      case dehweaponid_ammoType:
      {
         itemeffect_t *giver = E_ItemEffectForName(deh_giverNames[weaponinfo.dehnum]);

         // We want to remake the weapon's giver so that it's appropriate for the new ammo type
         giver->removeMetaTableNR(keyAmmoGiven);
         if(value < 0 || value >= NUMAMMO)
            weaponinfo.ammo = nullptr; // no ammo
         else
         {
            itemeffect_t              *given     = nullptr;
            const itemeffect_t *const  smallitem = E_ItemEffectForName(deh_itemsForAmmoNum[value][1]);
            const int                  amount    = smallitem->getInt(keyAmount, 0);

            weaponinfo.ammo = E_ItemEffectForName(deh_itemsForAmmoNum[value][0]);
            giver->addMetaTable(keyAmmoGiven, new MetaTable("ammogiven"));
            given = giver->getMetaTable(keyAmmoGiven, nullptr);

            given->setString("type", deh_itemsForAmmoNum[value][0]);
            given->setInt("ammo.dmstay",   amount * 5);
            given->setInt("ammo.coopstay", amount * 2);
            given->setInt("ammo.give",     amount * 2);
            given->setInt("ammo.dropped",  amount);
         }
         break;
      }
      case dehweaponid_deselect:
         weaponinfo.upstate = E_GetStateNumForDEHNum(value);
         break;
      case dehweaponid_select:
         weaponinfo.downstate = E_GetStateNumForDEHNum(value);
         break;
      case dehweaponid_bobbing:
         weaponinfo.readystate = E_GetStateNumForDEHNum(value);
         // Apply chainsaw replacement hack by suppressing ready sound
         if(indexnum == wp_chainsaw)
         {
            if(weaponinfo.readystate == E_SafeState(S_SAW))
               weaponinfo.intflags &= ~WIF_SUPPRESSREADYSOUND;
            else
               weaponinfo.intflags |= WIF_SUPPRESSREADYSOUND;
         }
         break;
      case dehweaponid_shooting:
         weaponinfo.atkstate = E_GetStateNumForDEHNum(value);
         break;
      case dehweaponid_firing:
         weaponinfo.flashstate = E_GetStateNumForDEHNum(value);
         break;
      case dehweaponid_ammoPerShot:
         weaponinfo.ammopershot = value;
         // enable ammo per shot value usage for this weapon
         weaponinfo.flags &= ~WPF_DISABLEAPS;
         break;
      case dehweaponid_mbf21flags:
      if(!value)
      {
         deh_ParseFlagsCombinedRemapped(strval, &dehacked_mbf21weaponflags, dehacked_mbf21weaponflags_remappings);
         weaponinfo.flags = dehacked_mbf21weaponflags.results[DEHFLAGS_MODE1];
      }
      break;
      default:
         deh_LogPrintf("Invalid weapon string index for '%s'\n", key);
         break;
      }
   }
   return;
}

// ====================================================================
// deh_procSprite
// Purpose: Dummy - we do not support the DEH Sprite block
// Args:    fpin  -- input file stream
//          line  -- current line in file to process
// Returns: void
//
static void deh_procSprite(DWFILE *fpin, char *line) // Not supported
{
   char key[DEH_MAXKEYLEN];
   char inbuffer[DEH_BUFFERMAX];
   int indexnum;

   // Too little is known about what this is supposed to do, and
   // there are better ways of handling sprite renaming.  Not supported.

   strncpy(inbuffer,line,DEH_BUFFERMAX);

   // killough 8/98: allow hex numbers in input:
   sscanf(inbuffer,"%" DEH_MAXKEYLEN_FMT "s %i",key, &indexnum);
   deh_LogPrintf("Ignoring Sprite offset change at index %d: %s\n",
                 indexnum, key);

   while(!fpin->atEof() && *inbuffer && (*inbuffer != ' '))
   {
      if(!fpin->getStr(inbuffer, sizeof(inbuffer)))
         break;
      lfstrip(inbuffer);
      if(!*inbuffer)
         break;      // killough 11/98

      // ignore line
      deh_LogPrintf("- %s\n", inbuffer);
   }
   return;
}

// ====================================================================
// deh_procPars
// Purpose: Handle BEX extension for PAR times
// Args:    fpin  -- input file stream
//          line  -- current line in file to process
// Returns: void
//
static void deh_procPars(DWFILE *fpin, char *line) // extension
{
   char key[DEH_MAXKEYLEN];
   char inbuffer[DEH_BUFFERMAX];
   int indexnum;
   int episode, level, partime, oldpar;

   // new item, par times
   // usage: After [PARS] Par 0 section identifier, use one or more of these
   // lines:
   //  par 3 5 120
   //  par 14 230
   // The first would make the par for E3M5 be 120 seconds, and the
   // second one makes the par for MAP14 be 230 seconds.  The number
   // of parameters on the line determines which group of par values
   // is being changed.  Error checking is done based on current fixed
   // array sizes of[4][10] and [32]

   strncpy(inbuffer,line,DEH_BUFFERMAX);

   // killough 8/98: allow hex numbers in input:
   sscanf(inbuffer,"%" DEH_MAXKEYLEN_FMT "s %i",key, &indexnum);
   deh_LogPrintf("Processing Par value at index %d: %s\n", indexnum,  key);

   // indexnum is a dummy entry
   while (!fpin->atEof() && *inbuffer && (*inbuffer != ' '))
   {
      if(!fpin->getStr(inbuffer, sizeof(inbuffer)))
         break;

      lfstrip(M_Strlwr(inbuffer)); // lowercase it

      if(!*inbuffer)
         break; // killough 11/98

      if(3 != sscanf(inbuffer,"par %i %i %i",&episode, &level, &partime))
      {
         // not 3
         if(2 != sscanf(inbuffer,"par %i %i",&level, &partime))
         {
            // not 2
            deh_LogPrintf("Invalid par time setting string: %s\n", inbuffer);
         }
         else
         {
            // is 2
            // Ty 07/11/98 - wrong range check, not zero-based
            if(level < 1 || level > 32) // base 0 array (but 1-based parm)
               deh_LogPrintf("Invalid MAPnn value MAP%d\n", level);
            else
            {
               oldpar = cpars[level-1];
               deh_LogPrintf("Changed par time for MAP%02d from %d to %d\n",
                             level, oldpar, partime);
               cpars[level - 1] = partime;
               deh_pars = true;
            }
         }
      }
      else // is 3
      {
         // note that though it's a [4][10] array, the "left" and "top" aren't
         // used, effectively making it a base 1 array.
         // Ty 07/11/98 - level was being checked against max 3 - dumb error
         // Note that episode 4 does not have par times per original design
         // in Ultimate DOOM so that is not supported here.
         if(episode < 1 || episode > 3 || level < 1 || level > 9)
            deh_LogPrintf("Invalid ExMx values E%dM%d\n", episode, level);
         else
         {
            oldpar = pars[episode][level];
            pars[episode][level] = partime;
            deh_LogPrintf("Changed par time for E%dM%d from %d to %d\n",
                          episode, level, oldpar, partime);
            deh_pars = true;
         }
      }
   }
   return;
}

// ====================================================================
// deh_procCheat
// Purpose: Handle DEH Cheat block
// Args:    fpin  -- input file stream
//          line  -- current line in file to process
// Returns: void
//
static void deh_procCheat(DWFILE *fpin, char *line) // done
{
   char  key[DEH_MAXKEYLEN];
   char  inbuffer[DEH_BUFFERMAX];
   int   value;  // All deh values are ints or longs
   char *strval; // pointer to the value area
   char *p;      // utility pointer

   deh_LogPrintf("Processing Cheat: %s\n", line);

   strncpy(inbuffer, line, DEH_BUFFERMAX);

   while(!fpin->atEof() && *inbuffer && (*inbuffer != ' '))
   {
      if(!fpin->getStr(inbuffer, sizeof(inbuffer)))
         break;

      lfstrip(inbuffer);
      if(!*inbuffer)
         break;       // killough 11/98

      if(!deh_GetData(inbuffer, key, &value, &strval)) // returns TRUE if ok
      {
         deh_LogPrintf("Bad data pair in '%s'\n", inbuffer);
         continue;
      }

      // Otherwise we got a (perhaps valid) cheat name, so look up the key in
      // the array
      for(size_t ix = 0; ix < earrlen(deh_cheats); ix++)
      {
         if(!strcasecmp(key, deh_cheats[ix].name)) // found the cheat, ignored case
         {
            // haleyjd: get a reference to the cheat in the main cheat table
            cheat_s &cht = cheat[deh_cheats[ix].cheatnum];

            // replace it but don't overflow it.  Use current length as limit.
            // Ty 03/13/98 - add 0xff code
            // Deal with the fact that the cheats in deh files are extended
            // with character 0xFF to the original cheat length, which we don't do.
            for(size_t iy = 0; strval[iy]; iy++)
               strval[iy] = (strval[iy] == (char)0xff) ? '\0' : strval[iy];

            // Ty 03/14/98 - skip leading spaces
            p = strval;
            while(*p == ' ') ++p;

            // killough 9/12/98: disable cheats which are prefixes of this one
            // haleyjd 08/21/13: talk about overkill...
            if(deh_cheats[ix].cheatnum == CHEAT_IDKFA)
               cheat[CHEAT_IDK].deh_disabled = true;

            // Ty 03/16/98 - change to use a strdup and orphan the original
            // Also has the advantage of allowing length changes.
            cht.cheat = estrdup(p);
            deh_LogPrintf("Assigned new cheat '%s' to cheat '%s'\n", p, deh_cheats[ix].name);
         }
      } // end for

      deh_LogPrintf("- %s\n", inbuffer);

   } // end while
}

// ====================================================================
// deh_procMisc
// Purpose: Handle DEH Misc block
// Args:    fpin  -- input file stream
//          line  -- current line in file to process
// Returns: void
//
static void deh_procMisc(DWFILE *fpin, char *line) // done
{
   char key[DEH_MAXKEYLEN];
   char inbuffer[DEH_BUFFERMAX];
   int  value;      // All deh values are ints or longs
   itemeffect_t *fx;

   strncpy(inbuffer,line,DEH_BUFFERMAX);

   while(!fpin->atEof() && *inbuffer && (*inbuffer != ' '))
   {
      if(!fpin->getStr(inbuffer, sizeof(inbuffer)))
         break;

      lfstrip(inbuffer);
      if(!*inbuffer)
         break;    // killough 11/98

      if(!deh_GetData(inbuffer, key, &value, nullptr)) // returns TRUE if ok
      {
         deh_LogPrintf("Bad data pair in '%s'\n", inbuffer);
         continue;
      }

      // Otherwise it's ok
      deh_LogPrintf("Processing Misc item '%s'\n", key);

      if(!strcasecmp(key, "Initial Health"))
      {
         playerclass_t *pc;
         if((pc = E_PlayerClassForName("DoomMarine")))
            pc->initialhealth = value;
      }
      else if(!strcasecmp(key, "Initial Bullets"))
      {
         playerclass_t *pc;
         if((pc = E_PlayerClassForName("DoomMarine")))
         {
            for(unsigned int i = 0; i < pc->numrebornitems; i++)
            {
               if(!strcasecmp(pc->rebornitems[i].itemname, "AmmoClip"))
               {
                  pc->rebornitems[i].amount = value;
                  if(!value)
                     pc->rebornitems[i].flags |= RBIF_IGNORE;
               }
            }
         }
      }
      else if(!strcasecmp(key, "Max Health"))
      {
         if((fx = E_ItemEffectForName(ITEMNAME_HEALTHBONUS)))
         {
            fx->removeConstString("maxamount");
            fx->setInt("maxamount", value * 2);
         }
         if((fx = E_ItemEffectForName(ITEMNAME_MEDIKIT)))
            fx->setInt("compatmaxamount", value);
         if((fx = E_ItemEffectForName(ITEMNAME_STIMPACK)))
            fx->setInt("compatmaxamount", value);
      }
      else if(!strcasecmp(key, "Max Armor"))
      {
         if((fx = E_ItemEffectForName(ITEMNAME_ARMORBONUS)))
            fx->setInt("maxsaveamount", value);
      }
      else if(!strcasecmp(key, "Green Armor Class"))
      {
         if((fx = E_ItemEffectForName(ITEMNAME_GREENARMOR)))
         {
            fx->setInt("saveamount", value * 100);
            if(value > 1)
            {
               fx->setInt("savefactor",  1);
               fx->setInt("savedivisor", 2);
            }
         }
      }
      else if(!strcasecmp(key, "Blue Armor Class"))  // Blue Armor Class
      {
         if((fx = E_ItemEffectForName(ITEMNAME_BLUEARMOR)))
         {
            fx->setInt("saveamount", value * 100);
            if(value <= 1)
            {
               fx->setInt("savefactor",  1);
               fx->setInt("savedivisor", 3);
            }
         }
      }
      else if(!strcasecmp(key, "Max Soulsphere"))
      {
         if((fx = E_ItemEffectForName(ITEMNAME_SOULSPHERE)))
         {
            fx->removeConstString("maxamount");
            fx->setInt("maxamount", value);
         }
      }
      else if(!strcasecmp(key, "Soulsphere Health"))
      {
         if((fx = E_ItemEffectForName(ITEMNAME_SOULSPHERE)))
         {
            fx->removeConstString("amount");
            fx->setInt("amount", value);
         }
      }
      else if(!strcasecmp(key, "Megasphere Health"))
      {
         if((fx = E_ItemEffectForName(ITEMNAME_MEGASPHERE)))
         {
            fx->removeConstString("amount");
            fx->setInt("amount",    value);
            fx->removeConstString("maxamount");
            fx->setInt("maxamount", value);
         }
      }
      else if(!strcasecmp(key, "God Mode Health"))
      {
         god_health_override = value;
      }
      else if(!strcasecmp(key, "IDFA Armor"))
      {
         if((fx = E_ItemEffectForName(ITEMNAME_IDFAARMOR)))
            fx->setInt("saveamount", value);
      }
      else if(!strcasecmp(key, "IDFA Armor Class"))
      {
         if((fx = E_ItemEffectForName(ITEMNAME_IDFAARMOR)))
         {
            fx->setInt("savefactor", 1);
            fx->setInt("savedivisor", value > 1 ? 2 : 3);
         }
      }
      else if(!strcasecmp(key, "IDKFA Armor"))
         ; //idkfa_armor = value;
      else if(!strcasecmp(key, "IDKFA Armor Class"))
         ; //idkfa_armor_class = value;
      else if(!strcasecmp(key, "BFG Cells/Shot"))
      {
         // haleyjd 08/10/02: propagate to weapon info
         weaponinfo_t &bfginfo = *E_WeaponForDEHNum(wp_bfg);
         bfgcells = bfginfo.ammopershot = value;
      }
      else if(!strcasecmp(key, "Monsters Infight"))
      {
         // FROM CHOCOLATE-DOOM
         // Dehacked: "Monsters infight"
         // This controls whether monsters can harm other monsters of the same species. For example,
         // whether an imp fireball will damage other imps. The value of this in dehacked patches is
         // weird - '202' means off, while '221' means on.
         switch(value)
         {
            case 202:
               deh_species_infighting = false;
               break;
            case 221:
               deh_species_infighting = true;
               break;
            default:
               deh_LogPrintf("Invalid value for 'Monsters Infight': %d\n", value);
               break;
         }
      }
      else
         deh_LogPrintf("Invalid misc item string index for '%s'\n", key);
   }
}

// ====================================================================
// deh_procText
// Purpose: Handle DEH Text block
// Notes:   We look things up in the current information and if found
//          we replace it.  At the same time we write the new and
//          improved BEX syntax to the log file for future use.
// Args:    fpin  -- input file stream
//          line  -- current line in file to process
// Returns: void
//
static void deh_procText(DWFILE *fpin, char *line)
{
   char key[DEH_MAXKEYLEN];
   char inbuffer[DEH_BUFFERMAX*2]; // can't use line -- double size buffer too.
   int i;                          // loop variable
   unsigned int fromlen, tolen;    // as specified on the text block line
   int usedlen;                    // shorter of fromlen and tolen if not matched
   bool found = false;             // to allow early exit once found
   char* line2 = nullptr;             // duplicate line for rerouting
   sfxinfo_t *sfx;

   // Ty 04/11/98 - Included file may have NOTEXT skip flag set
   if(includenotext) // flag to skip included deh-style text
   {
      deh_LogPrintf("Skipped text block because of notext directive\n");

      strcpy(inbuffer,line);

      while(!fpin->atEof() && *inbuffer && (*inbuffer != ' '))
         fpin->getStr(inbuffer, sizeof(inbuffer));  // skip block

      // Ty 05/17/98 - don't care if this fails
      return; // early return
   }

   // killough 8/98: allow hex numbers in input:
   sscanf(line, "%" DEH_MAXKEYLEN_FMT "s %i %i", key, &fromlen, &tolen);

   deh_LogPrintf("Processing Text (key=%s, from=%d, to=%d)\n",
                 key, fromlen, tolen);

   // killough 10/98: fix incorrect usage of feof
   {
      int c;
      unsigned int totlen = 0;

      while(totlen < fromlen + tolen && (c = fpin->getChar()) != EOF)
      {
         // haleyjd 08/20/09: eldritch bug from MBF. Wad files are opened
         // in binary mode, so we must not count 0x0D characters or place
         // them into the inbuffer.
         if(c != '\r')
            inbuffer[totlen++] = c;
      }
      inbuffer[totlen]='\0';
   }

   // if the from and to are 4, this may be a sprite rename.  Check it
   // against the array and process it as such if it matches.  Remember
   // that the original names are (and should remain) uppercase.
   // haleyjd 10/08/06: use deh_spritenames for comparisons to implement
   // proper DeHackEd string replacement logic.
   if(fromlen == 4 && tolen == 4)
   {
      i = 0;
      while (sprnames[i])  // null terminated list in info.c //jff 3/19/98
      {                                                        //check pointer
         if(!strncasecmp(deh_spritenames[i], inbuffer, fromlen))  //not first char
         {
            deh_LogPrintf("Changing name of sprite at index %d from %s to %*s\n",
                          i,sprnames[i], tolen, &inbuffer[fromlen]);

            // Ty 03/18/98 - not using strdup because length is fixed

            // killough 10/98: but it's an array of pointers, so we must
            // use strdup unless we redeclare sprnames and change all else

            // haleyjd 03/11/03: can now use the original
            // sprnames[i] = estrdup(sprnames[i]);

            strncpy(sprnames[i],&inbuffer[fromlen],tolen);
            found = true;
            break;  // only one will match--quit early
         }
         ++i;  // next array element
      }
   }
   if(!found && fromlen < 7 && tolen < 7) // lengths of music and sfx are 6 or shorter
   {
      usedlen = (fromlen < tolen) ? fromlen : tolen;
      if(fromlen != tolen)
      {
         deh_LogPrintf("Warning: Mismatched lengths from=%d, to=%d, used %d\n",
                       fromlen, tolen, usedlen);
      }

      // Try sound effects entries - see sounds.c
      // haleyjd 10/08/06: use sfx mnemonics for comparisons
      // haleyjd 04/13/08: call EDF finder function to eliminate S_sfx
      if((sfx = E_FindSoundForDEH(inbuffer, fromlen)))
      {
         deh_LogPrintf("Changing name of sfx from %s to %*s\n",
                       sfx->name, usedlen, &inbuffer[fromlen]);

         // haleyjd 09/03/03: changed to strncpy
         memset(sfx->name, 0, 9);
         strncpy(sfx->name, &inbuffer[fromlen], usedlen);
         found = true;
      }

      if(!found)  // not yet
      {
         // Try music name entries - see sounds.c
         // haleyjd 10/08/06: use deh_musicnames
         for(i = 1; i < NUMMUSIC; ++i)
         {
            // avoid short prefix erroneous match
            if(strlen(deh_musicnames[i]) != fromlen) continue;
            if(!strncasecmp(deh_musicnames[i],inbuffer,fromlen))
            {
               deh_LogPrintf("Changing name of music from %s to %*s\n",
                             S_music[i].name, usedlen, &inbuffer[fromlen]);

               S_music[i].name = estrdup(&inbuffer[fromlen]);
               found = true;
               break;  // only one matches, quit early
            }
         }
      }  // end !found test
   }

   if(!found) // Nothing we want to handle here--see if strings can deal with it.
   {
      deh_LogPrintf(
         "Checking text area through strings for '%.12s%s' from=%d to=%d\n",
         inbuffer,
         (strlen(inbuffer) > 12) ? "..." : "",
         fromlen, tolen);

      if(fromlen <= strlen(inbuffer))
      {
         line2 = estrdup(&inbuffer[fromlen]);
         inbuffer[fromlen] = '\0';
      }

      deh_procStringSub(nullptr, inbuffer, line2);
   }

   if(line2)
      efree(line2);
}

static void deh_procError(DWFILE *fpin, char *line)
{
   char inbuffer[DEH_BUFFERMAX];

   strncpy(inbuffer, line, DEH_BUFFERMAX);
   deh_LogPrintf("Unmatched Block: '%s'\n", inbuffer);
}

// ============================================================
// deh_procStrings
// Purpose: Handle BEX [STRINGS] extension
// Args:    fpin  -- input file stream
//          line  -- current line in file to process
// Returns: void
//
static void deh_procStrings(DWFILE *fpin, char *line)
{
   char key[DEH_MAXKEYLEN];
   char inbuffer[DEH_BUFFERMAX];
   int  value;          // All deh values are ints or longs
   char *strval = nullptr; // holds the string value of the line
   // holds the final result of the string after concatenation
   static char *holdstring = nullptr;
   static unsigned int maxstrlen = 128; // maximum string length, bumped 128 at
                                        // a time as needed
   bool found = false;  // looking for string continuation

   deh_LogPrintf("Processing extended string substitution\n");

   if(!holdstring)
      holdstring = ecalloc(char *, maxstrlen, sizeof(*holdstring));

   *holdstring = '\0'; // empty string to start with

   strncpy(inbuffer,line,DEH_BUFFERMAX);

   // Ty 04/24/98 - have to allow inbuffer to start with a blank for
   // the continuations of C1TEXT etc.
   while(!fpin->atEof() && *inbuffer)  /* && (*inbuffer != ' ') */
   {
      if(!fpin->getStr(inbuffer, sizeof(inbuffer)))
         break;
      if(*inbuffer == '#')
         continue;  // skip comment lines
      lfstrip(inbuffer);
      if(!*inbuffer)
         break;  // killough 11/98

      if(!*holdstring) // first one--get the key
      {
         if(!deh_GetData(inbuffer, key, &value, &strval)) // returns TRUE if ok
         {
            deh_LogPrintf("Bad data pair in '%s'\n", inbuffer);
            continue;
         }
      }

      while(strlen(holdstring) + strlen(inbuffer) > maxstrlen) // Ty03/29/98 - fix stupid error
      {
         // killough 11/98: allocate enough the first time
         maxstrlen += static_cast<unsigned>(strlen(holdstring) + strlen(inbuffer) - maxstrlen);

         deh_LogPrintf("* Increased buffer from to %d for buffer size %d\n",
                       maxstrlen, (int)strlen(inbuffer));

         holdstring = erealloc(char *, holdstring, maxstrlen*sizeof(*holdstring));
      }
      // concatenate the whole buffer if continuation or the value iffirst
      strcat(holdstring,ptr_lstrip(((*holdstring) ? inbuffer : strval)));
      rstrip(holdstring);
      // delete any trailing blanks past the backslash
      // note that blanks before the backslash will be concatenated
      // but ones at the beginning of the next line will not, allowing
      // indentation in the file to read well without affecting the
      // string itself.
      if(holdstring[strlen(holdstring)-1] == '\\')
      {
         holdstring[strlen(holdstring)-1] = '\0';
         continue; // ready to concatenate
      }
      if(*holdstring) // didn't have a backslash, trap above would catch that
      {
         // go process the current string
         // supply key and not search string
         found = deh_procStringSub(key, nullptr, holdstring);

          if(!found)
          {
             deh_LogPrintf("Invalid string key '%s', substitution skipped.\n",
                           key);
          }

          *holdstring = '\0';  // empty string for the next one
      }
   }

   return;
}

// ====================================================================
// deh_procStringSub
// Purpose: Common string parsing and handling routine for DEH and BEX
// Args:    key       -- place to put the mnemonic for the string if found
//          lookfor   -- original value string to look for
//          newstring -- string to put in its place if found
// Returns: bool: True if string found, false if not
//
// haleyjd 11/02/02: rewritten to replace linear search on string
// table with in-table chained hashing -- table is now in d_dehtbl.c
//
static bool deh_procStringSub(char *key, char *lookfor, char *newstring)
{
   dehstr_t *dehstr = nullptr;

   if(lookfor)
      dehstr = D_GetDEHStr(lookfor);
   else
      dehstr = D_GetBEXStr(key);

   if(!dehstr)
   {
      deh_LogPrintf("Could not find '%.12s'\n", key ? key : lookfor);
      return false;
   }

   char *copyNewStr = estrdup(newstring);

   // Handle embedded \n's in the incoming string, convert to 0x0a's
   char *s, *t;
   for(s=t=copyNewStr; *s; ++s, ++t)
   {
      if (*s == '\\' && (s[1] == 'n' || s[1] == 'N')) //found one
      {
         ++s;
         *t = '\n';  // skip one extra for second character
      }
      else
         *t = *s;
   }
   *t = '\0';  // cap off the target string

   *dehstr->ppstr = copyNewStr; // orphan original string

   if(key)
      deh_LogPrintf("Assigned key %s => '%s'\n", key, newstring);
   else if(lookfor)
   {
      deh_LogPrintf("Changed '%.12s%s' to '%.12s%s' at key %s\n",
                    lookfor, (strlen(lookfor) > 12) ? "..." : "",
                    newstring, (strlen(newstring) > 12) ? "..." :"",
                    dehstr->lookup);

      // must have passed an old style string so show BEX
      deh_LogPrintf("*BEX FORMAT:\n%s=%s\n*END BEX\n",
                    dehstr->lookup, dehReformatStr(newstring));
   }

   return true;
}

//=============================================================
// haleyjd 9/22/99
//
// deh_procHelperThing
//
// Allows handy substitution of any thing for helper dogs.  DEH
// patches are being made frequently for this purpose and it
// requires a complete rewiring of the DOG thing.  I feel this
// is a waste of effort, and so have added this new [HELPER]
// BEX block
//
static void deh_procHelperThing(DWFILE *fpin, char *line)
{
   char key[DEH_MAXKEYLEN];
   char inbuffer[DEH_BUFFERMAX];
   int  value;      // All deh values are ints or longs

   strncpy(inbuffer,line,DEH_BUFFERMAX);
   while(!fpin->atEof() && *inbuffer && (*inbuffer != ' '))
   {
      if(!fpin->getStr(inbuffer, sizeof(inbuffer)))
         break;

      lfstrip(inbuffer);
      if(!*inbuffer)
         break;

      if(!deh_GetData(inbuffer, key, &value, nullptr)) // returns TRUE if ok
      {
         deh_LogPrintf("Bad data pair in '%s'\n", inbuffer);
         continue;
      }

      // Otherwise it's ok
      deh_LogPrintf("Processing Helper Thing item '%s'\n", key);
      deh_LogPrintf("value is %i", (int)value);

      if(!strncasecmp(key, "type", 4))
         HelperThing = E_ThingNumForDEHNum((int)value);
   }
}

//
// deh_procBexSprites
//
// Supports sprite name substitutions without requiring use
// of the DeHackEd Text block
//
static void deh_procBexSprites(DWFILE *fpin, char *line)
{
   char key[DEH_MAXKEYLEN];
   char inbuffer[DEH_BUFFERMAX];
   int  value;    // All deh values are ints or longs
   char *strval;  // holds the string value of the line
   char candidate[5];
   int  rover;

   deh_LogPrintf("Processing sprite name substitution\n");

   strncpy(inbuffer,line,DEH_BUFFERMAX);

   while(!fpin->atEof() && *inbuffer && (*inbuffer != ' '))
   {
      if(!fpin->getStr(inbuffer, sizeof(inbuffer)))
         break;

      if(*inbuffer == '#')
         continue;  // skip comment lines

      lfstrip(inbuffer);
      if(!*inbuffer)
         break;  // killough 11/98

      if(!deh_GetData(inbuffer, key, &value, &strval)) // returns TRUE if ok
      {
         deh_LogPrintf("Bad data pair in '%s'\n", inbuffer);
         continue;
      }

      // do it
      memset(candidate, 0, sizeof(candidate));
      strncpy(candidate, ptr_lstrip(strval), 4);
      if(strlen(candidate) != 4)
      {
         deh_LogPrintf("Bad length for sprite name '%s'\n", candidate);
         continue;
      }

      rover = 0;
      while(deh_spritenames[rover])
      {
         if(!strncasecmp(deh_spritenames[rover], key, 4))
         {
            deh_LogPrintf("Substituting '%s' for sprite '%s'\n",
                          candidate, deh_spritenames[rover]);

            // haleyjd 03/11/03: can now use original due to EDF
            // sprnames[rover] = estrdup(candidate);
            strncpy(sprnames[rover], candidate, 4);
            break;
         }
         rover++;
      }
   }
}

// ditto for sound names
static void deh_procBexSounds(DWFILE *fpin, char *line)
{
   char key[DEH_MAXKEYLEN];
   char inbuffer[DEH_BUFFERMAX];
   int  value;    // All deh values are ints or longs
   char *strval;  // holds the string value of the line
   char candidate[9];
   int  len;
   sfxinfo_t *sfx;

   // haleyjd 09/03/03: rewritten to work with EDF

   deh_LogPrintf("Processing sound name substitution\n");

   strncpy(inbuffer,line,DEH_BUFFERMAX);

   while(!fpin->atEof() && *inbuffer && (*inbuffer != ' '))
   {
      if(!fpin->getStr(inbuffer, sizeof(inbuffer)))
         break;

      if(*inbuffer == '#')
         continue;  // skip comment lines

      lfstrip(inbuffer);
      if(!*inbuffer)
         break;  // killough 11/98

      if(!deh_GetData(inbuffer, key, &value, &strval)) // returns TRUE if ok
      {
         deh_LogPrintf("Bad data pair in '%s'\n", inbuffer);
         continue;
      }

      // do it
      memset(candidate, 0, 9);
      strncpy(candidate, ptr_lstrip(strval), 9);
      len = static_cast<int>(strlen(candidate));
      if(len < 1 || len > 8)
      {
         deh_LogPrintf("Bad length for sound name '%s'\n", candidate);
         continue;
      }

      sfx = E_SoundForName(key);

      if(!sfx)
      {
         deh_LogPrintf("Bad sound mnemonic '%s'\n", key);
         continue;
      }

      deh_LogPrintf("Substituting '%s' for sound '%s'\n",
                    candidate, sfx->mnemonic);

      strncpy(sfx->name, candidate, 9);
   }
}

// ditto for music names
static void deh_procBexMusic(DWFILE *fpin, char *line)
{
   char key[DEH_MAXKEYLEN];
   char inbuffer[DEH_BUFFERMAX];
   int  value;    // All deh values are ints or longs
   char *strval;  // holds the string value of the line
   char candidate[7];
   int  rover, len;

   deh_LogPrintf("Processing music name substitution\n");

   strncpy(inbuffer,line,DEH_BUFFERMAX);

   while(!fpin->atEof() && *inbuffer && (*inbuffer != ' '))
   {
      if(!fpin->getStr(inbuffer, sizeof(inbuffer)))
         break;

      if(*inbuffer == '#')
         continue;  // skip comment lines

      lfstrip(inbuffer);
      if(!*inbuffer)
         break;  // killough 11/98

      if(!deh_GetData(inbuffer, key, &value, &strval)) // returns TRUE if ok
      {
         deh_LogPrintf("Bad data pair in '%s'\n", inbuffer);
         continue;
      }
      // do it
      memset(candidate, 0, 7);
      strncpy(candidate, ptr_lstrip(strval), 6);
      len = static_cast<int>(strlen(candidate));
      if(len < 1 || len > 6)
      {
         deh_LogPrintf("Bad length for music name '%s'\n", candidate);
         continue;
      }

      rover = 1;
      while(deh_musicnames[rover])
      {
         if(!strncasecmp(deh_musicnames[rover], key, 6))
         {
            deh_LogPrintf("Substituting '%s' for music '%s'\n",
                          candidate, deh_musicnames[rover]);

            S_music[rover].name = estrdup(candidate);
            break;
         }
         rover++;
      }
   }
}

// ====================================================================
// General utility function(s)
// ====================================================================

// ====================================================================
// dehReformatStr
// Purpose: Convert a string into a continuous string with embedded
//          linefeeds for "\n" sequences in the source string
// Args:    string -- the string to convert
// Returns: the converted string (converted in a static buffer)
//
static char *dehReformatStr(char *string)
{
   // only processing the changed string, don't need double buffer
   static char buff[DEH_BUFFERMAX];
   char *s, *t;

   s = string;  // source
   t = buff;    // target

   // let's play...
   while(*s)
   {
      if(*s == '\n')
      {
         ++s;
         *t++ = '\\';
         *t++ = 'n';
         *t++ = '\\';
         *t++='\n';
      }
      else
         *t++ = *s++;
   }
   *t = '\0';
   return buff;
}

// ====================================================================
// lfstrip
// Purpose: Strips CR/LF off the end of a string
// Args:    s -- the string to work on
// Returns: void -- the string is modified in place
//
// killough 10/98: only strip at end of line, not entire string

static void lfstrip(char *s)  // strip the \r and/or \n off of a line
{
   char *p = s + strlen(s);
   while(p > s && (*--p == '\r' || *p == '\n'))
      *p = 0;
}

// ====================================================================
// rstrip
// Purpose: Strips trailing blanks off a string
// Args:    s -- the string to work on
// Returns: void -- the string is modified in place
//
static void rstrip(char *s)  // strip trailing whitespace
{
   char *p = s + strlen(s);         // killough 4/4/98: same here
   while(p > s && ectype::isSpace(*--p)) // break on first non-whitespace
      *p='\0';
}

// ====================================================================
// ptr_lstrip
// Purpose: Points past leading whitespace in a string
// Args:    s -- the string to work on
// Returns: char * pointing to the first nonblank character in the
//          string.  The original string is not changed.
//
static char *ptr_lstrip(char *p)  // point past leading whitespace
{
   while(ectype::isSpace(*p))
      p++;
   return p;
}

// ====================================================================
// deh_GetData
// Purpose: Get a key and data pair from a passed string
// Args:    s -- the string to be examined
//          k -- a place to put the key
//          l -- pointer to an integer to store the number
//          strval -- a pointer to the place in s where the number
//                    value comes from.  Pass nullptr to not use this.
// Notes:   Expects a key phrase, optional space, equal sign,
//          optional space and a value, mostly an int. The passed
//          pointer to hold the key must be DEH_MAXKEYLEN in size.

static bool deh_GetData(char *s, char *k, int *l, char **strval)
{
   char *t;                    // current char
   int  val = 0;               // to hold value of pair
   char buffer[DEH_MAXKEYLEN]; // to hold key in progress
   bool okrc = true;           // assume good unless we have problems
   int i;                      // iterator

   memset(buffer, 0, sizeof(buffer));

   for(i = 0, t = s; *t && i < DEH_MAXKEYLEN; ++t, ++i)
   {
      if(*t == '=')
         break;
      buffer[i] = *t; // copy it
   }

   buffer[--i] = '\0';  // terminate the key before the '='

   if(!*t)  // end of string with no equal sign
      okrc = false;
   else
   {
      if(!*++t) // in case "thiskey =" with no value
         okrc = false;

      // we've incremented t
      val = static_cast<int>(strtol(t, nullptr, 0));  // killough 8/9/98: allow hex or octal input
   }

   // go put the results in the passed pointers
   *l = val;  // may be a faked zero

   // if spaces between key and equal sign, strip them
   strcpy(k, ptr_lstrip(buffer));  // could be a zero-length string

   if(strval != nullptr) // pass nullptr if you don't want this back
      *strval = t;       // pointer, has to be somewhere in s,
                         // even if pointing at the zero byte.
   return okrc;
}

//---------------------------------------------------------------------
//
// $Log: d_deh.c,v $
// Revision 1.20  1998/06/01  22:30:38  thldrmn
// fix .acv pointer for new GCC version
//
// Revision 1.19  1998/05/17  09:39:48  thldrmn
// Bug fix to avoid processing last line twice
//
// Revision 1.17  1998/05/04  21:36:21  thldrmn
// commenting, reformatting and savegamename change
//
// Revision 1.16  1998/05/03  22:09:59  killough
// use p_inter.h for extern declarations and fix a pointer cast
//
// Revision 1.15  1998/04/26  14:46:24  thldrmn
// BEX code pointer additions
//
// Revision 1.14  1998/04/24  23:49:35  thldrmn
// Strings continuation fix
//
// Revision 1.13  1998/04/19  01:18:58  killough
// Change deh cheat code handling to use new cheat table
//
// Revision 1.12  1998/04/11  14:47:31  thldrmn
// Added include, fixed pars
//
// Revision 1.11  1998/04/10  06:49:15  killough
// Fix CVS stuff
//
// Revision 1.10  1998/04/09  09:17:00  thldrmn
// Update to text handling
//
// Revision 1.00  1998/04/07  04:43:59  ty
// First time with cvs revision info
//
//---------------------------------------------------------------------
