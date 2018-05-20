//
// The Eternity Engine
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
// Purpose: EDF Thing Types Module
// Authors: James Haley
//

#define NEED_EDF_DEFINITIONS

#include "z_zone.h"
#include "i_system.h"

#include "Confuse/confuse.h"

#include "acs_intr.h"

#include "autodoom/b_analysis.h"  // IOANCH
#include "d_dehtbl.h"
#include "d_gi.h"
#include "d_io.h"
#include "d_mod.h"
#include "e_dstate.h"
#include "e_edf.h"
#include "e_hash.h"
#include "e_inventory.h"
#include "e_lib.h"
#include "e_metastate.h"
#include "e_mod.h"
#include "e_sound.h"
#include "e_sprite.h"
#include "e_states.h"
#include "e_things.h"
#include "e_weapons.h"
#include "g_game.h"
#include "info.h"
#include "m_cheat.h"
#include "m_qstr.h"
#include "m_qstrkeys.h"
#include "metaapi.h"
#include "metaspawn.h"
#include "p_inter.h"
#include "p_mobjcol.h"
#include "p_partcl.h"
#include "r_defs.h"
#include "r_draw.h"
#include "w_wad.h"
#include "z_auto.h"

// 11/06/11: track generations
static int edf_thing_generation = 1; 

// 7/24/05: This is now global, for efficiency's sake

// The "Unknown" thing type, which is required, has its type
// number resolved in E_CollectThings
int UnknownThingType;

// Thing Keywords

// ID / Type Info
#define ITEM_TNG_DOOMEDNUM    "doomednum"
#define ITEM_TNG_DEHNUM       "dehackednum"
#define ITEM_TNG_COMPATNAME   "compatname"
#define ITEM_TNG_INHERITS     "inherits"
#define ITEM_TNG_BASICTYPE    "basictype"

// States
#define ITEM_TNG_SPAWNSTATE    "spawnstate"
#define ITEM_TNG_SEESTATE      "seestate"
#define ITEM_TNG_PAINSTATE     "painstate"
#define ITEM_TNG_PAINSTATES    "dmg_painstates"
#define ITEM_TNG_PNSTATESADD   "dmg_painstates.add"
#define ITEM_TNG_PNSTATESREM   "dmg_painstates.remove"
#define ITEM_TNG_MELEESTATE    "meleestate"
#define ITEM_TNG_MISSILESTATE  "missilestate"
#define ITEM_TNG_DEATHSTATE    "deathstate"
#define ITEM_TNG_DEATHSTATES   "dmg_deathstates"
#define ITEM_TNG_DTHSTATESADD  "dmg_deathstates.add"
#define ITEM_TNG_DTHSTATESREM  "dmg_deathstates.remove"
#define ITEM_TNG_XDEATHSTATE   "xdeathstate"
#define ITEM_TNG_RAISESTATE    "raisestate"
#define ITEM_TNG_HEALSTATE     "healstate"   // ioanch 20160220
#define ITEM_TNG_CRASHSTATE    "crashstate"
#define ITEM_TNG_ACTIVESTATE   "activestate"
#define ITEM_TNG_INACTIVESTATE "inactivestate"
#define ITEM_TNG_FIRSTDECSTATE "firstdecoratestate"

// DECORATE state block
#define ITEM_TNG_STATES        "states"

// Sounds
#define ITEM_TNG_SEESOUND      "seesound"
#define ITEM_TNG_ATKSOUND      "attacksound"
#define ITEM_TNG_PAINSOUND     "painsound"
#define ITEM_TNG_DEATHSOUND    "deathsound"
#define ITEM_TNG_ACTIVESOUND   "activesound"
#define ITEM_TNG_ACTIVATESND   "activatesound"
#define ITEM_TNG_DEACTIVATESND "deactivatesound"

// Basic Stats
#define ITEM_TNG_SPAWNHEALTH  "spawnhealth"
#define ITEM_TNG_GIBHEALTH    "gibhealth"
#define ITEM_TNG_REACTTIME    "reactiontime"
#define ITEM_TNG_PAINCHANCE   "painchance"
#define ITEM_TNG_SPEED        "speed"
#define ITEM_TNG_FASTSPEED    "fastspeed"
#define ITEM_TNG_RADIUS       "radius"
#define ITEM_TNG_HEIGHT       "height"
#define ITEM_TNG_C3DHEIGHT    "correct_height"
#define ITEM_TNG_MASS         "mass"
#define ITEM_TNG_RESPAWNTIME  "respawntime"
#define ITEM_TNG_RESPCHANCE   "respawnchance"
#define ITEM_TNG_AIMSHIFT     "aimshift"

// Special Spawning
#define ITEM_TNG_COLSPAWN     "collectionspawn"
#define ITEM_TNG_ITEMRESPAT   "itemrespawnat"

// Damage Properties
#define ITEM_TNG_DAMAGE       "damage"
#define ITEM_TNG_DMGSPECIAL   "dmgspecial"
#define ITEM_TNG_DAMAGEFACTOR "damagefactor"
#define ITEM_TNG_TOPDAMAGE    "topdamage"
#define ITEM_TNG_TOPDMGMASK   "topdamagemask"
#define ITEM_TNG_MOD          "mod"
#define ITEM_TNG_OBIT1        "obituary_normal"
#define ITEM_TNG_OBIT2        "obituary_melee"

// Pain/Death Properties
#define ITEM_TNG_BLOODCOLOR   "bloodcolor"
#define ITEM_TNG_NUKESPEC     "nukespecial"
#define ITEM_TNG_DROPTYPE     "droptype"
#define ITEM_TNG_DROPITEM     "dropitem"
#define ITEM_TNG_REMDROPITEM  "dropitem.remove"
#define ITEM_TNG_CLRDROPITEM  "cleardropitems"

// Flags
#define ITEM_TNG_CFLAGS       "cflags"
#define ITEM_TNG_ADDFLAGS     "addflags"
#define ITEM_TNG_REMFLAGS     "remflags"
#define ITEM_TNG_FLAGS        "flags"
#define ITEM_TNG_FLAGS2       "flags2"
#define ITEM_TNG_FLAGS3       "flags3"
#define ITEM_TNG_FLAGS4       "flags4"
#define ITEM_TNG_PARTICLEFX   "particlefx"

// Graphic Properites
#define ITEM_TNG_TRANSLUC     "translucency"
#define ITEM_TNG_TRANMAP      "tranmap"
#define ITEM_TNG_COLOR        "translation"
#define ITEM_TNG_SKINSPRITE   "skinsprite"
#define ITEM_TNG_DEFSPRITE    "defaultsprite"
#define ITEM_TNG_AVELOCITY    "alphavelocity"
#define ITEM_TNG_XSCALE       "xscale"
#define ITEM_TNG_YSCALE       "yscale"

// Title properties
#define ITEM_TNG_TITLE_SUPER     "superclass"
#define ITEM_TNG_TITLE_DOOMEDNUM "doomednum"
#define ITEM_TNG_TITLE_DEHNUM    "dehackednum"

// ACS Spawn Data Sub-Block
#define ITEM_TNG_ACS_SPAWN    "acs_spawndata"
#define ITEM_TNG_ACS_NUM      "num"
#define ITEM_TNG_ACS_MODES    "modes"

// Damage factor multi-value property internal fields
#define ITEM_TNG_DMGF_MODNAME "mod"
#define ITEM_TNG_DMGF_FACTOR  "factor"

// DropItem multi-value property internal fields
#define ITEM_TNG_DROPITEM_ITEM   "item"
#define ITEM_TNG_DROPITEM_CHANCE "chance"
#define ITEM_TNG_DROPITEM_AMOUNT "amount"
#define ITEM_TNG_DROPITEM_TOSS   "toss"

// Collection spawn multi-value property internal fields
#define ITEM_TNG_COLSPAWN_TYPE   "type"
#define ITEM_TNG_COLSPAWN_SP     "spchance"
#define ITEM_TNG_COLSPAWN_COOP   "coopchance"
#define ITEM_TNG_COLSPAWN_DM     "dmchance"

// Thing Delta Keywords
#define ITEM_DELTA_NAME "name"

// Blood Properties
#define ITEM_TNG_BLOODNORM   "bloodtype.normal"
#define ITEM_TNG_BLOODIMPACT "bloodtype.impact"
#define ITEM_TNG_BLOODRIP    "bloodtype.rip"
#define ITEM_TNG_BLOODCRUSH  "bloodtype.crush"

#define ITEM_TNG_BLOODBEHAV  "bloodbehavior"
#define ITEM_TNG_CLRBLOODBEH "clearbloodbehaviors"

#define ITEM_TNG_BB_ACTION   "action"
#define ITEM_TNG_BB_BEHAVIOR "behavior"

// Pickup Property
#define ITEM_TNG_PFX_PICKUPFX  "pickupeffect"
#define ITEM_TNG_PFX_EFFECTS   "effects"
#define ITEM_TNG_PFX_CHANGEWPN "changeweapon"
#define ITEM_TNG_PFX_MSG       "message"
#define ITEM_TNG_PFX_SOUND     "sound"
#define ITEM_TNG_PFX_FLAGS     "flags"

//
// Thing groups
//
#define ITEM_TGROUP_FLAGS "flags"
#define ITEM_TGROUP_TYPES "types"

//
// Field-Specific Data
//

// Particle effects flags

static dehflags_t particlefx[] =
{
   { "ROCKET",         FX_ROCKET },
   { "GRENADE",        FX_GRENADE },
   { "FLIES",          FX_FLIES },
   { "BFG",            FX_BFG },
   { "FLIESONDEATH",   FX_FLIESONDEATH },
   { "DRIP",           FX_DRIP },
   { "REDFOUNTAIN",    FX_REDFOUNTAIN },
   { "GREENFOUNTAIN",  FX_GREENFOUNTAIN },
   { "BLUEFOUNTAIN",   FX_BLUEFOUNTAIN },
   { "YELLOWFOUNTAIN", FX_YELLOWFOUNTAIN },
   { "PURPLEFOUNTAIN", FX_PURPLEFOUNTAIN },
   { "BLACKFOUNTAIN",  FX_BLACKFOUNTAIN },
   { "WHITEFOUNTAIN",  FX_WHITEFOUNTAIN },
   { nullptr,          0 }
};

static dehflagset_t particle_flags =
{
   particlefx,  // flaglist
   0,           // mode
};

// ACS game mode flags

static dehflags_t acs_gamemodes[] =
{
   { "doom",    ACS_MODE_DOOM },
   { "heretic", ACS_MODE_HTIC },
   { "all",     ACS_MODE_ALL  },
   { nullptr,   0             }
};

static dehflagset_t acs_gamemode_flags =
{
   acs_gamemodes, // flaglist
   0,             // mode
};

// table to translate from GameModeInfo->type to acs gamemode flag
static unsigned int gameTypeToACSFlags[NumGameModeTypes] =
{
   ACS_MODE_DOOM, // Game_DOOM,
   ACS_MODE_HTIC, // Game_Heretic
};

// Special damage inflictor types
// currently searched linearly
// matching enum values in p_inter.h

static const char *inflictorTypes[INFLICTOR_NUMTYPES] =
{
   "none",
   "MinotaurCharge",
   "Whirlwind",
};

//
// Basic types for things. These determine a number of alternate "defaults"
// for the thingtype that will make it behave in a typical manner for things
// of its basic type (ie: monster, projectile). The nice thing about basic
// types is that they can change, and things dependent upon them will be 
// automatically updated for new versions of the engine.
//
static const char *BasicTypeNames[] =
{
   "Monster",           // normal walking monster with no fancy features
   "FlyingMonster",     // normal flying monster
   "FriendlyHelper",    // a thing with max options for helping player
   "Projectile",        // normal projectile
   "PlayerProjectile",  // player projectile (activates param linespecs)
   "Seeker",            // homing projectile
   "SolidDecor",        // solid decoration
   "HangingDecor",      // hanging decoration
   "SolidHangingDecor", // solid hanging decoration
   "ShootableDecor",    // shootable decoration
   "Fog",               // fog, like telefog, item fog, or bullet puffs
   "Item",              // collectable item
   "ItemCount",         // collectable item that counts for item %
   "TerrainBase",       // base of a terrain effect
   "TerrainChunk",      // chunk of a terrain effect
   "ControlPoint",      // a control point
   "ControlPointGrav",  // a control point with gravity
};

#define NUMBASICTYPES earrlen(BasicTypeNames)

typedef struct basicttype_s
{
   unsigned int flags;     // goes to: mi->flags
   unsigned int flags2;    //        : mi->flags2
   unsigned int flags3;    //        : mi->flags3
   const char *spawnstate; //        : mi->spawnstate
} basicttype_t;

static basicttype_t BasicThingTypes[] =
{
   // Monster
   {
      static_cast<unsigned>(MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL), // flags
      static_cast<unsigned>(MF2_FOOTCLIP),                       // flags2
      static_cast<unsigned>(MF3_SPACMONSTER|MF3_PASSMOBJ),       // flags3
   },
   // FlyingMonster
   {
      static_cast<unsigned>(MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL|MF_NOGRAVITY|MF_FLOAT), // flags
      0u,                                                       // flags2
      static_cast<unsigned>(MF3_SPACMONSTER|MF3_PASSMOBJ),                             // flags3
   },
   // FriendlyHelper
   { 
      static_cast<unsigned>(MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL|MF_FRIEND),                // flags
      static_cast<unsigned>(MF2_JUMPDOWN|MF2_FOOTCLIP),                                   // flags2
      static_cast<unsigned>(MF3_WINDTHRUST|MF3_SUPERFRIEND|MF3_SPACMONSTER|MF3_PASSMOBJ), // flags3
   },
   // Projectile
   {
      static_cast<unsigned>(MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE), // flags
      static_cast<unsigned>(MF2_NOCROSS),                                      // flags2
      0u,                                               // flags3
   },
   // PlayerProjectile
   {
      static_cast<unsigned>(MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE), // flags
      static_cast<unsigned>(MF2_NOCROSS),                                      // flags2
      static_cast<unsigned>(MF3_SPACMISSILE),                                  // flags3
   },
   // Seeker
   {
      static_cast<unsigned>(MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE), // flags
      static_cast<unsigned>(MF2_NOCROSS|MF2_SEEKERMISSILE),                    // flags2
      0u,                                               // flags3
   },
   // SolidDecor
   {
      static_cast<unsigned>(MF_SOLID), // flags
      0u,       // flags2
      0u,       // flags3
   },
   // HangingDecor
   {
      static_cast<unsigned>(MF_SPAWNCEILING|MF_NOGRAVITY), // flags
      0u,                           // flags2
      0u,                           // flags3
   },
   // SolidHangingDecor
   {
      static_cast<unsigned>(MF_SOLID|MF_SPAWNCEILING|MF_NOGRAVITY), // flags
      0u,                                    // flags2
      0u,                                    // flags3
   },
   // ShootableDecor
   {
      static_cast<unsigned>(MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD), // flags
      0u,                               // flags2
      0u,                               // flags3
   },
   // Fog
   {
      static_cast<unsigned>(MF_NOBLOCKMAP|MF_NOGRAVITY|MF_TRANSLUCENT), // flags
      static_cast<unsigned>(MF2_NOSPLASH),                              // flags2
      0u,                                        // flags3
   },
   // Item
   {
      static_cast<unsigned>(MF_SPECIAL), // flags
      0u,         // flags2
      0u,         // flags3
   },
   // ItemCount
   {
      static_cast<unsigned>(MF_SPECIAL|MF_COUNTITEM), // flags
      0u,                      // flags2
      0u,                      // flags3
   },
   // TerrainBase
   {
      static_cast<unsigned>(MF_NOBLOCKMAP|MF_NOGRAVITY), // flags
      static_cast<unsigned>(MF2_NOSPLASH),               // flags2
      0u,                         // flags3
   },
   // TerrainChunk
   {
      static_cast<unsigned>(MF_NOBLOCKMAP|MF_DROPOFF|MF_MISSILE), // flags
      static_cast<unsigned>(MF2_LOGRAV|MF2_NOSPLASH|MF2_NOCROSS), // flags2
      static_cast<unsigned>(MF3_CANNOTPUSH),                      // flags3
   },
   // ControlPoint
   {
      static_cast<unsigned>(MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY), // flags
      0u,                                     // flags2
      0u,                                     // flags3
      "S_TNT1",                               // spawnstate
   },
   // ControlPointGrav
   {
      0u,                        // flags
      static_cast<unsigned>(MF2_DONTDRAW|MF2_NOSPLASH), // flags2
      0u,                        // flags3
      "S_TNT1",                  // spawnstate
   },
};

//
// Thing Type Options
//

// title properties
static cfg_opt_t thing_tprops[] =
{
   CFG_STR(ITEM_TNG_TITLE_SUPER,      0, CFGF_NONE),
   CFG_INT(ITEM_TNG_TITLE_DOOMEDNUM, -1, CFGF_NONE),
   CFG_INT(ITEM_TNG_TITLE_DEHNUM,    -1, CFGF_NONE),
   CFG_END()
};

// acs spawndata sub-section
static cfg_opt_t acs_data[] =
{
   CFG_INT(ITEM_TNG_ACS_NUM,   -1,    CFGF_NONE),
   CFG_STR(ITEM_TNG_ACS_MODES, "all", CFGF_NONE),
   CFG_END()
};

static int E_damageFactorCB(cfg_t *cfg, cfg_opt_t *opt, const char *value, 
   void *result);

// damage factor multi-value property options
static cfg_opt_t dmgf_opts[] =
{
   CFG_STR(  ITEM_TNG_DMGF_MODNAME, "Unknown", CFGF_NONE),
   CFG_FLOAT_CB(ITEM_TNG_DMGF_FACTOR, 0.0, CFGF_NONE, E_damageFactorCB),
   CFG_END()
};

// dropitem multi-value property options
static cfg_opt_t dropitem_opts[] =
{
   CFG_STR(ITEM_TNG_DROPITEM_ITEM,   nullptr, CFGF_NONE),
   CFG_INT(ITEM_TNG_DROPITEM_CHANCE, 255,     CFGF_NONE),
   CFG_INT(ITEM_TNG_DROPITEM_AMOUNT, 0,       CFGF_NONE),

   CFG_FLAG(ITEM_TNG_DROPITEM_TOSS,  0,       CFGF_SIGNPREFIX),

   CFG_END()
};

// collectionspawn multi-value property options
static cfg_opt_t colspawn_opts[] =
{
   CFG_STR(ITEM_TNG_COLSPAWN_TYPE, "", CFGF_NONE),
   CFG_INT(ITEM_TNG_COLSPAWN_SP,    0, CFGF_NONE),
   CFG_INT(ITEM_TNG_COLSPAWN_COOP,  0, CFGF_NONE),
   CFG_INT(ITEM_TNG_COLSPAWN_DM,    0, CFGF_NONE),
   CFG_END()
};

// bloodbehavior multi-value property options
static cfg_opt_t bloodbeh_opts[] =
{
   CFG_STR(ITEM_TNG_BB_ACTION,   "", CFGF_NONE), // action
   CFG_STR(ITEM_TNG_BB_BEHAVIOR, "", CFGF_NONE), // behavior
   CFG_END()
};

static cfg_opt_t tngpfx_opts[] =
{
   CFG_STR(ITEM_TNG_PFX_EFFECTS,   0,          CFGF_LIST),
   CFG_STR(ITEM_TNG_PFX_CHANGEWPN, NULL,       CFGF_NONE),
   CFG_STR(ITEM_TNG_PFX_MSG,       NULL,       CFGF_NONE),
   CFG_STR(ITEM_TNG_PFX_SOUND,     NULL,       CFGF_NONE),
   CFG_STR(ITEM_TNG_PFX_FLAGS,     NULL,       CFGF_NONE),

   CFG_END()
};

// translation value-parsing callback
static int E_ColorCB(cfg_t *, cfg_opt_t *, const char *, void *);
static int E_TranMapCB(cfg_t *, cfg_opt_t *, const char *, void *);

#define THINGTYPE_FIELDS \
   CFG_INT(ITEM_TNG_DOOMEDNUM,       -1,            CFGF_NONE), \
   CFG_INT(ITEM_TNG_DEHNUM,          -1,            CFGF_NONE), \
   CFG_STR(ITEM_TNG_COMPATNAME,      "",            CFGF_NONE), \
   CFG_STR(ITEM_TNG_BASICTYPE,       "",            CFGF_NONE), \
   CFG_STR(ITEM_TNG_SPAWNSTATE,      "S_NULL",      CFGF_NONE), \
   CFG_STR(ITEM_TNG_SEESTATE,        "S_NULL",      CFGF_NONE), \
   CFG_STR(ITEM_TNG_PAINSTATE,       "S_NULL",      CFGF_NONE), \
   CFG_STR(ITEM_TNG_PAINSTATES,      0,             CFGF_LIST), \
   CFG_STR(ITEM_TNG_PNSTATESADD,     0,             CFGF_LIST), \
   CFG_STR(ITEM_TNG_PNSTATESREM,     0,             CFGF_LIST), \
   CFG_STR(ITEM_TNG_MELEESTATE,      "S_NULL",      CFGF_NONE), \
   CFG_STR(ITEM_TNG_MISSILESTATE,    "S_NULL",      CFGF_NONE), \
   CFG_STR(ITEM_TNG_DEATHSTATE,      "S_NULL",      CFGF_NONE), \
   CFG_STR(ITEM_TNG_DEATHSTATES,     0,             CFGF_LIST), \
   CFG_STR(ITEM_TNG_DTHSTATESADD,    0,             CFGF_LIST), \
   CFG_STR(ITEM_TNG_DTHSTATESREM,    0,             CFGF_LIST), \
   CFG_STR(ITEM_TNG_XDEATHSTATE,     "S_NULL",      CFGF_NONE), \
   CFG_STR(ITEM_TNG_RAISESTATE,      "S_NULL",      CFGF_NONE), \
   CFG_STR(ITEM_TNG_HEALSTATE,       "S_NULL",      CFGF_NONE), \
   CFG_STR(ITEM_TNG_CRASHSTATE,      "S_NULL",      CFGF_NONE), \
   CFG_STR(ITEM_TNG_ACTIVESTATE,     "S_NULL",      CFGF_NONE), \
   CFG_STR(ITEM_TNG_INACTIVESTATE,   "S_NULL",      CFGF_NONE), \
   CFG_STR(ITEM_TNG_FIRSTDECSTATE,   nullptr,       CFGF_NONE), \
   CFG_STR(ITEM_TNG_STATES,          0,             CFGF_NONE), \
   CFG_STR(ITEM_TNG_SEESOUND,        "none",        CFGF_NONE), \
   CFG_STR(ITEM_TNG_ATKSOUND,        "none",        CFGF_NONE), \
   CFG_STR(ITEM_TNG_PAINSOUND,       "none",        CFGF_NONE), \
   CFG_STR(ITEM_TNG_DEATHSOUND,      "none",        CFGF_NONE), \
   CFG_STR(ITEM_TNG_ACTIVESOUND,     "none",        CFGF_NONE), \
   CFG_STR(ITEM_TNG_ACTIVATESND,     "none",        CFGF_NONE), \
   CFG_STR(ITEM_TNG_DEACTIVATESND,   "none",        CFGF_NONE), \
   CFG_INT(ITEM_TNG_SPAWNHEALTH,     1000,          CFGF_NONE), \
   CFG_INT(ITEM_TNG_GIBHEALTH,       0,             CFGF_NONE), \
   CFG_INT(ITEM_TNG_REACTTIME,       8,             CFGF_NONE), \
   CFG_INT(ITEM_TNG_PAINCHANCE,      0,             CFGF_NONE), \
   CFG_INT(ITEM_TNG_MASS,            100,           CFGF_NONE), \
   CFG_INT(ITEM_TNG_RESPAWNTIME,     (12*35),       CFGF_NONE), \
   CFG_INT(ITEM_TNG_RESPCHANCE,      4,             CFGF_NONE), \
   CFG_INT(ITEM_TNG_AIMSHIFT,        -1,            CFGF_NONE), \
   CFG_INT(ITEM_TNG_DAMAGE,          0,             CFGF_NONE), \
   CFG_STR(ITEM_TNG_DMGSPECIAL,      "NONE",        CFGF_NONE), \
   CFG_INT(ITEM_TNG_TOPDAMAGE,       0,             CFGF_NONE), \
   CFG_INT(ITEM_TNG_TOPDMGMASK,      0,             CFGF_NONE), \
   CFG_STR(ITEM_TNG_MOD,             "Unknown",     CFGF_NONE), \
   CFG_STR(ITEM_TNG_OBIT1,           "NONE",        CFGF_NONE), \
   CFG_STR(ITEM_TNG_OBIT2,           "NONE",        CFGF_NONE), \
   CFG_INT(ITEM_TNG_BLOODCOLOR,      0,             CFGF_NONE), \
   CFG_STR(ITEM_TNG_NUKESPEC,        "NULL",        CFGF_NONE), \
   CFG_STR(ITEM_TNG_DROPTYPE,        "NONE",        CFGF_NONE), \
   CFG_STR(ITEM_TNG_CFLAGS,          "",            CFGF_NONE), \
   CFG_STR(ITEM_TNG_ADDFLAGS,        "",            CFGF_NONE), \
   CFG_STR(ITEM_TNG_REMFLAGS,        "",            CFGF_NONE), \
   CFG_STR(ITEM_TNG_FLAGS,           "",            CFGF_NONE), \
   CFG_STR(ITEM_TNG_FLAGS2,          "",            CFGF_NONE), \
   CFG_STR(ITEM_TNG_FLAGS3,          "",            CFGF_NONE), \
   CFG_STR(ITEM_TNG_FLAGS4,          "",            CFGF_NONE), \
   CFG_STR(ITEM_TNG_PARTICLEFX,      "",            CFGF_NONE), \
   CFG_STR(ITEM_TNG_SKINSPRITE,      "noskin",      CFGF_NONE), \
   CFG_STR(ITEM_TNG_DEFSPRITE,       nullptr,       CFGF_NONE), \
   CFG_SEC(ITEM_TNG_ACS_SPAWN,       acs_data,      CFGF_NOCASE), \
   CFG_STR(ITEM_TNG_REMDROPITEM,     "",            CFGF_MULTI), \
   CFG_STR(ITEM_TNG_ITEMRESPAT,      "",            CFGF_NONE), \
   CFG_FLAG(ITEM_TNG_CLRDROPITEM,    0,             CFGF_NONE), \
   CFG_FLOAT(ITEM_TNG_RADIUS,        20.0f,         CFGF_NONE), \
   CFG_FLOAT(ITEM_TNG_HEIGHT,        16.0f,         CFGF_NONE), \
   CFG_FLOAT(ITEM_TNG_C3DHEIGHT,     0.0f,          CFGF_NONE), \
   CFG_FLOAT(ITEM_TNG_AVELOCITY,     0.0f,          CFGF_NONE), \
   CFG_FLOAT(ITEM_TNG_XSCALE,        1.0f,          CFGF_NONE), \
   CFG_FLOAT(ITEM_TNG_YSCALE,        1.0f,          CFGF_NONE), \
   CFG_INT_CB(ITEM_TNG_SPEED,        0,             CFGF_NONE, E_IntOrFixedCB), \
   CFG_INT_CB(ITEM_TNG_FASTSPEED,    0,             CFGF_NONE, E_IntOrFixedCB), \
   CFG_INT_CB(ITEM_TNG_TRANSLUC,     65536,         CFGF_NONE, E_TranslucCB  ), \
   CFG_INT_CB(ITEM_TNG_COLOR,        0,             CFGF_NONE, E_ColorCB     ), \
   CFG_INT_CB(ITEM_TNG_TRANMAP,     -1,             CFGF_NONE, E_TranMapCB   ), \
   CFG_MVPROP(ITEM_TNG_DAMAGEFACTOR, dmgf_opts,     CFGF_MULTI|CFGF_NOCASE   ), \
   CFG_MVPROP(ITEM_TNG_DROPITEM,     dropitem_opts, CFGF_MULTI|CFGF_NOCASE   ), \
   CFG_MVPROP(ITEM_TNG_COLSPAWN,     colspawn_opts, CFGF_NOCASE              ), \
   CFG_MVPROP(ITEM_TNG_BLOODBEHAV,   bloodbeh_opts, CFGF_MULTI|CFGF_NOCASE   ), \
   CFG_FLAG(ITEM_TNG_CLRBLOODBEH,    0,             CFGF_NONE                ), \
   CFG_STR(ITEM_TNG_BLOODNORM,       "",            CFGF_NONE                ), \
   CFG_STR(ITEM_TNG_BLOODIMPACT,     "",            CFGF_NONE                ), \
   CFG_STR(ITEM_TNG_BLOODRIP,        "",            CFGF_NONE                ), \
   CFG_STR(ITEM_TNG_BLOODCRUSH,      "",            CFGF_NONE                ), \
   CFG_SEC(ITEM_TNG_PFX_PICKUPFX,    tngpfx_opts,   CFGF_NOCASE              ), \
   CFG_END()

cfg_opt_t edf_thing_opts[] =
{
   CFG_TPROPS(thing_tprops,       CFGF_NOCASE),
   CFG_STR(ITEM_TNG_INHERITS,  0, CFGF_NONE),
   THINGTYPE_FIELDS
};

cfg_opt_t edf_tdelta_opts[] =
{
   CFG_STR(ITEM_DELTA_NAME, 0, CFGF_NONE),
   THINGTYPE_FIELDS
};


//==============================================================================

static dehflags_t tgroup_kinds[] =
{
   { "PROJECTILEALLIANCE", TGF_PROJECTILEALLIANCE },
   { "DAMAGEIGNORE",       TGF_DAMAGEIGNORE       },
   { "INHERITED",          TGF_INHERITED          },
   { nullptr,              0                      }
};

//
// Thinggroup kinds
//
static dehflagset_t tgroup_kindset =
{
   tgroup_kinds,  // flaglist
   0              // mode
};

//
// Thinggroup options
//
cfg_opt_t edf_tgroup_opts[] =
{
   CFG_STR(ITEM_TGROUP_FLAGS, "", CFGF_NONE),
   CFG_STR(ITEM_TGROUP_TYPES, 0, CFGF_LIST),
   CFG_END()
};

//
// The thing group
//
class ThingGroup : public ZoneObject
{
public:
   explicit ThingGroup(const char *inname) : name(inname), link(), flags()
   {
   }

   qstring name;
   DLListItem<ThingGroup> link;

   unsigned flags;
   PODCollection<int> types;
};

//
// A projectile alliance definition
//
struct thinggrouppair_t
{
   union
   {
      int types[2];
      int64_t key;
   };
   DLListItem<thinggrouppair_t> link;
   unsigned flags;   // use flags from ThingGroup
};

//==============================================================================

//
// Thing Type Hash Lookup Functions
//

// Thing hash tables
// Note: Keep some buffer space between this prime number and the
// number of default mobj types defined, so that users can add a
// number of types without causing significant hash collisions.

// Number of chains
#define NUMTHINGCHAINS 307

// hash by name
static EHashTable<mobjinfo_t, ENCStringHashKey, 
                  &mobjinfo_t::name, &mobjinfo_t::namelinks> thing_namehash(NUMTHINGCHAINS);

// hash by compatname
static EHashTable<mobjinfo_t, ENCStringHashKey,
                  &mobjinfo_t::compatname, &mobjinfo_t::cnamelinks> thing_cnamehash(NUMTHINGCHAINS);

// hash by DeHackEd number
static EHashTable<mobjinfo_t, EIntHashKey,
                  &mobjinfo_t::dehnum, &mobjinfo_t::numlinks> thing_dehhash(NUMTHINGCHAINS);

// Thing group
static EHashTable<ThingGroup, ENCQStrHashKey,
                  &ThingGroup::name, &ThingGroup::link> thinggroup_namehash(53);

static EHashTable<thinggrouppair_t, EInt64HashKey,
     &thinggrouppair_t::key, &thinggrouppair_t::link> thinggrouppairs(NUMTHINGCHAINS);

//
// As with states, things need to store their DeHackEd number now.
// Returns -1 if a thing type is not found. This is used
// extensively by parameterized codepointers.
//
int E_ThingNumForDEHNum(int dehnum)
{
   const mobjinfo_t *info = nullptr;
   int ret = -1;

   if((info = thing_dehhash.objectForKey(dehnum)))
      ret = info->index;

   return ret;
}

//
// As above, but causes a fatal error if a thing type is not found.
//
int E_GetThingNumForDEHNum(int dehnum)
{
   int thingnum = E_ThingNumForDEHNum(dehnum);

   if(thingnum == -1)
      I_Error("E_GetThingNumForDEHNum: invalid deh num %d\n", dehnum);

   return thingnum;
}

//
// As above, but returns the 'Unknown' type if the requested
// one was not found.
//
int E_SafeThingType(int dehnum)
{
   int thingnum = E_ThingNumForDEHNum(dehnum);

   if(thingnum == -1)
      thingnum = UnknownThingType;

   return thingnum;
}

//
// As above, but for names
//
int E_SafeThingName(const char *name)
{
   int thingnum = E_ThingNumForName(name);

   if(thingnum == -1)
      thingnum = UnknownThingType;

   return thingnum;
}

//
// Returns a thing type index given its name. Returns -1
// if a thing type is not found.
//
int E_ThingNumForName(const char *name)
{
   mobjinfo_t *info = nullptr;
   int ret = -1;

   if((info = thing_namehash.objectForKey(name)))
      ret = info->index;

   return ret;
}

//
// As above, but causes a fatal error if the thing type isn't found.
//
int E_GetThingNumForName(const char *name)
{
   int thingnum = E_ThingNumForName(name);

   if(thingnum == -1)
      I_Error("E_GetThingNumForName: bad thing type %s\n", name);

   return thingnum;
}

//
// Get a thingtype index by name, but allowing fallback to the ACS compatibility
// hash if nothing is initially found. This extends ACS mod compatibility with
// ZDoom w/o requiring special handling inside ACS itself.
//
int E_ThingNumForCompatName(const char *name)
{
   int ret = -1;

   if((ret = E_ThingNumForName(name)) < 0)
   {
      mobjinfo_t *info; 
      if((info = thing_cnamehash.objectForKey(name)))
         ret = info->index;
   }

   return ret;
}

// allocation starts at D_MAXINT and works toward 0
static int edf_alloc_thing_dehnum = D_MAXINT;

//
// Automatic allocation of dehacked numbers allows things to be used with
// parameterized codepointers without having had a DeHackEd number explicitly
// assigned to them by the EDF author. This was requested by several users
// after v3.33.02.
//
bool E_AutoAllocThingDEHNum(int thingnum)
{
   int dehnum;
   mobjinfo_t *mi = mobjinfo[thingnum];

#ifdef RANGECHECK
   if(mi->dehnum != -1)
      I_Error("E_AutoAllocThingDEHNum: called for thing with valid dehnum\n");
#endif

   // cannot assign because we're out of dehnums?
   if(edf_alloc_thing_dehnum < 0)
      return false;

   do
   {
      dehnum = edf_alloc_thing_dehnum--;
   } 
   while(dehnum >= 0 && E_ThingNumForDEHNum(dehnum) != -1);

   // ran out while looking for an unused number?
   if(dehnum < 0)
      return false;

   // assign it!
   mi->dehnum = dehnum;
   thing_dehhash.addObject(mi);

   return true;
}

// 
// Processing Functions
//

// Dynamic Reallocation - haleyjd 11/06/11

//
// Function to reallocate the thingtypes array safely.
//
static void E_ReallocThings(int numnewthings)
{
   static int numthingsalloc = 0;

   // only realloc when needed
   if(!numthingsalloc || (NUMMOBJTYPES < numthingsalloc + numnewthings))
   {
      int i;

      // First time, just allocate the requested number of mobjinfo.
      // Afterward:
      // * If the number of mobjinfo requested is small, add 2 times as many
      //   requested, plus a small constant amount.
      // * If the number is large, just add that number.

      if(!numthingsalloc)
         numthingsalloc = numnewthings;
      else if(numnewthings <= 50)
         numthingsalloc += numnewthings * 2 + 32;
      else
         numthingsalloc += numnewthings;

      // reallocate mobjinfo[]
      mobjinfo = erealloc(mobjinfo_t **, mobjinfo, numthingsalloc * sizeof(mobjinfo_t *));

      // set the new mobjinfo pointers to null
      for(i = NUMMOBJTYPES; i < numthingsalloc; i++)
         mobjinfo[i] = nullptr;
   }

   // increment NUMMOBJTYPES
   NUMMOBJTYPES += numnewthings;
}

//
// Pre-creates and hashes by name the thingtypes, for purpose 
// of mutual and forward references.
//
void E_CollectThings(cfg_t *cfg)
{
   unsigned int i;
   unsigned int numthingtypes;     // number of thingtypes defined by the cfg
   unsigned int curnewthing = 0;   // index of current new thingtype being used
   mobjinfo_t  *newMobjInfo = nullptr;
   static bool firsttime = true;

   // get number of thingtypes defined by the cfg
   numthingtypes = cfg_size(cfg, EDF_SEC_THING);

   // echo counts
   E_EDFLogPrintf("\t\t%u thingtypes defined\n", numthingtypes);

   if(numthingtypes)
   {
      unsigned int firstnewthing = 0; // index of first new thingtype
      // allocate mobjinfo_t structures for the new thingtypes
      newMobjInfo = estructalloc(mobjinfo_t, numthingtypes);

      // add space to the mobjinfo array
      curnewthing = firstnewthing = NUMMOBJTYPES;

      E_ReallocThings((int)numthingtypes);
      // IOANCH: reallocate associated bot learn info
      B_UpdateMobjInfoSet(NUMMOBJTYPES);

      // set pointers in mobjinfo[] to the proper structures;
      // also set self-referential index member, and allocate a
      // metatable.
      for(i = firstnewthing; i < (unsigned int)NUMMOBJTYPES; i++)
      {
         mobjinfo[i] = &newMobjInfo[i - firstnewthing];
         mobjinfo[i]->index = i;
         mobjinfo[i]->meta = new MetaTable("mobjinfo");
      }
   }

   // build hash tables
   E_EDFLogPuts("\t\tBuilding thing hash tables\n");
   
   // cycle through the thingtypes defined in the cfg
   for(i = 0; i < numthingtypes; i++)
   {
      cfg_t *thingcfg   = cfg_getnsec(cfg, EDF_SEC_THING, i);
      const char *name  = cfg_title(thingcfg);
      const char *cname = cfg_getstr(thingcfg, ITEM_TNG_COMPATNAME);
      cfg_t *titleprops = nullptr;
      int dehnum = -1;

      // This is a new mobjinfo, whether or not one already exists by this name
      // in the hash table. For subsequent addition of EDF thingtypes at runtime,
      // the hash table semantics of "find newest first" take care of overriding,
      // while not breaking objects that depend on the original definition of
      // the thingtype for inheritance purposes.
      mobjinfo_t *mi = mobjinfo[curnewthing++];

      // initialize name
      mi->name = estrdup(name);

      // add to name hash
      thing_namehash.addObject(mi);

      // check for titleprops definition first
      if(cfg_size(thingcfg, "#title") > 0)
      {
         titleprops = cfg_gettitleprops(thingcfg);
         if(titleprops)
            dehnum = cfg_getint(titleprops, ITEM_TNG_TITLE_DEHNUM);
      }

      // If undefined, check the legacy value inside the section
      if(dehnum == -1)
         dehnum = cfg_getint(thingcfg, ITEM_TNG_DEHNUM);

      // process dehackednum and add thing to dehacked hash table,
      // if appropriate
      if((mi->dehnum = dehnum) >= 0)
         thing_dehhash.addObject(mi);

      // check for compatname
      if(estrnonempty(cname))
      {
         mi->compatname = estrdup(cname);
         thing_cnamehash.addObject(mi);
      }

      // set generation
      mi->generation = edf_thing_generation;
   }

   // first-time-only events
   if(firsttime)
   {
      // check that at least one thingtype was defined
      if(!NUMMOBJTYPES)
         E_EDFLoggedErr(2, "E_CollectThings: no thingtypes defined.\n");

      // verify the existance of the Unknown thing type
      UnknownThingType = E_ThingNumForName("Unknown");
      if(UnknownThingType < 0)
         E_EDFLoggedErr(2, "E_CollectThings: 'Unknown' thingtype must be defined.\n");

      firsttime = false;
   }
}

//
// Does sound name lookup & verification and then stores the resulting
// sound DeHackEd number into *target.
//
static void E_ThingSound(const char *data, const char *fieldname, 
                         int thingnum, int *target)
{
   sfxinfo_t *sfx;

   if((sfx = E_EDFSoundForName(data)) == nullptr)
   {
      // haleyjd 05/31/06: relaxed to warning
      E_EDFLoggedWarning(2, "Warning: thing '%s': invalid %s '%s'\n",
                         mobjinfo[thingnum]->name, fieldname, data);
      sfx = &NullSound;
   }
   
   // haleyjd 03/22/06: support auto-allocation of dehnums where possible
   if(sfx->dehackednum != -1 || E_AutoAllocSoundDEHNum(sfx))
      *target = sfx->dehackednum;
   else
   {
      E_EDFLoggedWarning(2, 
                         "Warning: failed to auto-allocate DeHackEd number "
                         "for sound %s\n", sfx->mnemonic);
      *target = 0;
   }
}

//
// Does frame name lookup & verification and then stores the resulting
// frame index into *target.
//
static void E_ThingFrame(const char *data, const char *fieldname,
                         int thingnum, int *target)
{
   int index;

   if((index = E_StateNumForName(data)) < 0)
   {
      E_EDFLoggedErr(2, "E_ThingFrame: thing '%s': invalid %s '%s'\n",
                     mobjinfo[thingnum]->name, fieldname, data);
   }
   *target = index;
}

//=============================================================================
//
// MetaState Management
//

// Warning: likely to get lost as there's no module for metastates currently!
IMPLEMENT_RTTI_TYPE(MetaState)

//
// Adds a state to the mobjinfo metatable.
//
static void E_AddMetaState(mobjinfo_t *mi, state_t *state, const char *name)
{
   mi->meta->addObject(new MetaState(name, state));
}

//
// Removes a state from the mobjinfo metatable given a metastate pointer.
//
static void E_RemoveMetaStatePtr(mobjinfo_t *mi, MetaState *ms)
{
   mi->meta->removeObject(ms);
   
   delete ms;
}

//
// Removes a state from the mobjinfo metatable given a name under which
// it is keyed into the table. If no such state exists, nothing happens.
//
static void E_RemoveMetaState(mobjinfo_t *mi, const char *name)
{
   MetaState *obj;

   if((obj = mi->meta->getObjectKeyAndTypeEx<MetaState>(name)))
      E_RemoveMetaStatePtr(mi, obj);
}

//
// Gets a state that is stored inside an mobjinfo metatable.
// Returns null if no such object exists.
//
static MetaState *E_GetMetaState(const mobjinfo_t *mi, const char *name)
{
   return mi->meta->getObjectKeyAndTypeEx<MetaState>(name);
}

//
// If state is not the null state, set it as a metastate under the "name"
// key. If state IS the null state, remove any such named metastate from the
// mobjinfo.
//
static void E_SetMetaState(mobjinfo_t *mi, state_t *state, const char *name)
{
   if(state->index != NullStateNum)
   {
      MetaState *ms;
      
      if((ms = mi->meta->getObjectKeyAndTypeEx<MetaState>(name)))
         ms->state = state;
      else
         E_AddMetaState(mi, state, name);
   }
   else
      E_RemoveMetaState(mi, name);      
}

//
// ioanch 20160220: variant with metastate
//
static void E_ThingFrame(const char *data, const char *fieldname,
                         int thingnum, const char *metakey)
{
   int index;
   if((index = E_StateNumForName(data)) < 0)
   {
      E_EDFLoggedErr(2, "E_ThingFrame: thing '%s': invalid %s '%s'\n",
                     mobjinfo[thingnum]->name, fieldname, data);
   }
   else
   {
      E_SetMetaState(mobjinfo[thingnum], states[index], metakey);
   }
}

//=============================================================================
//
// MOD States
//

//
// Constructs the appropriate label name for a metaproperty that
// uses a mod name as a suffix.
// Don't cache the return value.
//
const char *E_ModFieldName(const char *base, const emod_t *mod)
{
   static qstring namebuffer;

   namebuffer.clear() << base << '.' << mod->name;

   return namebuffer.constPtr();
}

//
// Returns the state from the given mobjinfo for the given mod type and
// base label, if such exists. If not, null is returned.
//
state_t *E_StateForMod(const mobjinfo_t *mi, const char *base,
                       const emod_t *mod)
{
   state_t   *ret = nullptr;
   const MetaState *mstate;

   if((mstate = E_GetMetaState(mi, E_ModFieldName(base, mod))))
      ret = mstate->state;

   return ret;
}

//
// Convenience wrapper routine to get the state node for a given
// mod type by number, rather than with a pointer to the damagetype object.
//
state_t *E_StateForModNum(const mobjinfo_t *mi, const char *base, int num)
{
   emod_t  *mod = E_DamageTypeForNum(num);
   state_t *ret = nullptr;

   if(mod->num != 0)
      ret = E_StateForMod(mi, base, mod);

   return ret;
}

//
// Adds a deathstate for a particular dynamic damage type to the given
// mobjinfo. An mobjinfo can only contain one deathstate for each 
// damagetype.
//
static void E_AddDamageTypeState(mobjinfo_t *info, const char *base, 
                                 state_t *state, emod_t *mod)
{
   MetaState *msnode;
   
   // if one exists for this mod already, use it, else create a new one.
   if((msnode = E_GetMetaState(info, E_ModFieldName(base, mod))))
      msnode->state = state;
   else
      E_AddMetaState(info, state, E_ModFieldName(base, mod));
}

//
// Trashes all the states in the given list.
//
static void E_DisposeDamageTypeList(mobjinfo_t *mi, const char *base)
{
   MetaState *state = nullptr;

   // iterate on the metatable to look for metastate_t objects with
   // the base string as the initial part of their name

   while((state = mi->meta->getNextTypeEx(state)))
   {
      if(!strncasecmp(state->getKey(), base, strlen(base)))
      {
         E_RemoveMetaStatePtr(mi, state);

         // must restart search (iterator invalidated)
         state = nullptr;
      }
   }
}

// Modes for E_ProcessDamageTypeStates
enum
{
   E_DTS_MODE_ADD,
   E_DTS_MODE_REMOVE,
   E_DTS_MODE_OVERWRITE,
};

// Fields for E_ProcessDamageTypeStates
enum
{
   E_DTS_FIELD_PAIN,
   E_DTS_FIELD_DEATH,
};

//
// Given the parent cfg object, the name of a list item within it,
// the destination mobjinfo, and the mode to work in (add, remove, or
// overwrite), this routine modifies the mobjinfo's custom damagetype
// state lists appropriately.
//
// The list is a flat list of strings. In add or overwrite mode, every 
// set of two strings is a mod/state pair. In remove mode, all the strings
// are mod types (we don't care about the states in that case).
//
static void E_ProcessDamageTypeStates(cfg_t *cfg, const char *name,
                                      mobjinfo_t *mi, int mode, int field)
{
   const char *base = "";

   switch(field)
   {
   case E_DTS_FIELD_PAIN:
      base = "Pain";
      break;
   case E_DTS_FIELD_DEATH:
      base = "Death";
      break;
   default:
      I_Error("E_ProcessDamageTypeStates: unknown field type\n");
      break;
   }

   // first things first, if we are in overwrite mode, we will dispose of
   // the appropriate list of states if any have been added already.
   if(mode == E_DTS_MODE_OVERWRITE)
      E_DisposeDamageTypeList(mi, base);

   // add / overwrite mode
   if(mode == E_DTS_MODE_ADD || mode == E_DTS_MODE_OVERWRITE)
   {
      int numstrs = (int)(cfg_size(cfg, name)); // get count of strings
      int i;

      if(numstrs % 2) // not divisible by two? ignore last one
         --numstrs;

      if(numstrs <= 0) // woops? nothing to do.
         return;

      for(i = 0; i < numstrs; i += 2)
      {
         const char *modname, *statename;
         int statenum;
         state_t *state;
         emod_t  *mod;

         modname   = cfg_getnstr(cfg, name, i);
         statename = cfg_getnstr(cfg, name, i + 1);

         mod      = E_DamageTypeForName(modname);
         statenum = E_StateNumForName(statename);

         // unknown state? ignore
         if(statenum < 0)
            continue;

         state = states[statenum];

         if(mod->num == 0) // if this is "Unknown", ignore it.
            continue;

         // add it to the thing
         E_AddDamageTypeState(mi, base, state, mod);
      }
   }
   else
   {
      // remove mode
      unsigned int numstrs = cfg_size(cfg, name); // get count
      unsigned int i;

      for(i = 0; i < numstrs; ++i)
      {
         const char *modname = cfg_getnstr(cfg, name, i);
         emod_t *mod = E_DamageTypeForName(modname);

         if(mod->num == 0) // if this is "Unknown", ignore it.
            continue;

         E_RemoveMetaState(mi, E_ModFieldName(base, mod));
      }
   }
}

//
// Returns an mobjinfo_t * if the given mobjinfo inherits from the given type 
// by name. Returns null otherwise. Self-identity is *not* considered 
// inheritance.
//
static mobjinfo_t *E_IsMobjInfoDescendantOf(mobjinfo_t *mi, const char *type)
{
   mobjinfo_t *curmi = mi->parent;
   int targettype = E_ThingNumForName(type);

   while(curmi)
   {
      if(curmi->index == targettype)
         break; // found it

      // walk up the inheritance tree
      curmi = curmi->parent;
   }

   return curmi;
}

//
// Takes a single string containing a :: operator and returns the tokens on
// either side of it. The string passed in src should be mutable.
//
// The pointers will not be modified if an error occurs.
//
void E_SplitTypeAndState(char *src, char **type, char **state)
{
   char *colon1, *colon2;

   colon1 = strchr(src, ':');
   colon2 = strrchr(src, ':');

   if(!colon1 || !colon2 || colon1 == colon2)
      return;

   *colon1 = *colon2 = '\0';

   *type  = src;
   *state = colon2 + 1;
}

//
// Deal with unresolved goto entries in the DECORATE state object.
//
static void E_processDecorateGotos(mobjinfo_t *mi, edecstateout_t *dso)
{
   int i;

   for(i = 0; i < dso->numgotos; ++i)
   {
      mobjinfo_t *type = nullptr;
      state_t *state;
      statenum_t statenum;
      char *statename = nullptr;

      // see if the label contains a colon, and if so, it may be an
      // access to an inherited state
      char *colon = strchr(dso->gotos[i].label, ':');

      if(colon)
      {
         char *typestr = nullptr;

         E_SplitTypeAndState(dso->gotos[i].label, &typestr, &statename);

         if(!(typestr && statename))
         {
            // error, set to null state
            *(dso->gotos[i].nextstate) = NullStateNum;
            continue;
         }

         // if "super" this means find the state in the parent type;
         // otherwise, check if the indicated type inherits from this one
         if(!strcasecmp(typestr, "super") && mi->parent)
            type = mi->parent;
         else 
            type = E_IsMobjInfoDescendantOf(mi, typestr);
      }
      else
      {
         // otherwise this is a name to resolve within the scope of the local
         // thingtype - it may be a state acquired through inheritance, for
         // example, and is thus not defined within the thingtype's state block.
         type = mi;
         statename = dso->gotos[i].label;
      }

      if(!type)
      {
         // error; set to null state and continue
         *(dso->gotos[i].nextstate) = NullStateNum;
         continue;
      }

      // find the state in the proper type
      if(!(state = E_GetStateForMobjInfo(type, statename)))
      {
         // error; set to null state and continue
         *(dso->gotos[i].nextstate) = NullStateNum;
         continue;
      }

      statenum = state->index + dso->gotos[i].offset;

      if(statenum < 0 || statenum >= NUMSTATES)
      {
         // error; set to null state and continue
         *(dso->gotos[i].nextstate) = NullStateNum;
         continue;
      }

      // resolve the goto
      *(dso->gotos[i].nextstate) = statenum;
   }
}

//
// Add all labeled states from a DECORATE state block to the given mobjinfo.
//
static void E_processDecorateStates(mobjinfo_t *mi, edecstateout_t *dso)
{
   int i;

   for(i = 0; i < dso->numstates; ++i)
   {
      int *nativefield;
      
      // first see if this is a native state
      if((nativefield = E_GetNativeStateLoc(mi, dso->states[i].label)))
         *nativefield = dso->states[i].state->index;
      else
      {
         MetaState *msnode;

         // there is not a matching native field, so add the state as a 
         // metastate
         if((msnode = E_GetMetaState(mi, dso->states[i].label)))
            msnode->state = dso->states[i].state;
         else
            E_AddMetaState(mi, dso->states[i].state, dso->states[i].label);
      }
   }
}

//
// Processes killstates (states to be removed) in the DECORATE state block.
//
static void E_processKillStates(mobjinfo_t *mi, edecstateout_t *dso)
{
   int i;

   for(i = 0; i < dso->numkillstates; ++i)
   {
      int *nativefield;

      // first see if this is a native state
      if((nativefield = E_GetNativeStateLoc(mi, dso->killstates[i].killname)))
         *nativefield = NullStateNum;
      else
         E_RemoveMetaState(mi, dso->killstates[i].killname);
   }
}

//
// Processes the DECORATE state list in a thing
//
static void E_ProcessDecorateStateList(mobjinfo_t *mi, const char *str, 
                                       const char *firststate, bool recursive)
{
   edecstateout_t *dso;

   if(!(dso = E_ParseDecorateStates(str, firststate)))
   {
      E_EDFLoggedWarning(2, "Warning: couldn't attach DECORATE states to thing '%s'.\n",
                         mi->name);
      return;
   }

   // first deal with any gotos that still need resolution
   if(dso->numgotos)
      E_processDecorateGotos(mi, dso);

   // add all labeled states from the block to the mobjinfo
   if(dso->numstates && !recursive)
      E_processDecorateStates(mi, dso);

   // deal with kill states
   if(dso->numkillstates && !recursive)
      E_processKillStates(mi, dso);

   // free the DSO object
   E_FreeDSO(dso);
}

//
// A change-over to DECORATE-format states in the default EDFs requires that we not
// drop DECORATE state blocks defined in sections that are displaced via a more recent
// definition during initial EDF processing. A small modification to libConfuse has made
// this possible to achieve. This recursive processing is only necessary when the 
// displaced thingtype definition uses the "firstdecoratestate" mechanism to populate 
// global states with its data.
//
static void E_ProcessDecorateStatesRecursive(cfg_t *thingsec, int thingnum, bool recursive)
{
   cfg_t *displaced;

   // 01/02/12: Process displaced sections recursively first.
   if((displaced = cfg_displaced(thingsec)))
      E_ProcessDecorateStatesRecursive(displaced, thingnum, true);

   // haleyjd 06/22/10: Process DECORATE state block
   if(cfg_size(thingsec, ITEM_TNG_STATES) > 0)
   {
      // 01/01/12: allow use of pre-existing reserved states; they must be
      // defined consecutively in EDF and should be flagged +decorate in order
      // for values inside them to be overridden by the DECORATE state block.
      // If this isn't being done, firststate will be null.
      const char *firststate = cfg_getstr(thingsec, ITEM_TNG_FIRSTDECSTATE);
      const char *tempstr    = cfg_getstr(thingsec, ITEM_TNG_STATES);

      // recursion should process states only if firststate is valid
      if(!recursive || firststate)
         E_ProcessDecorateStateList(mobjinfo[thingnum], tempstr, firststate, recursive);
   }
}

//
// Damage Factors
//
// Damage factors are also stored in the mobjinfo metatable. These floating point 
// properties adjust the amount of damage done to objects by specific damage types.
//

//
// Processes the damage factor objects for a thingtype definition.
//
static void E_ProcessDamageFactors(mobjinfo_t *info, cfg_t *cfg)
{
   unsigned int numfactors = cfg_size(cfg, ITEM_TNG_DAMAGEFACTOR);

   for(unsigned int i = 0; i < numfactors; i++)
   {
      cfg_t  *sec = cfg_getnmvprop(cfg, ITEM_TNG_DAMAGEFACTOR, i);
      emod_t *mod = E_DamageTypeForName(cfg_getstr(sec, ITEM_TNG_DMGF_MODNAME));

      // we don't add damage factors for the unknown damage type
      if(mod->num != 0)
      {
         double df  = cfg_getfloat(sec, ITEM_TNG_DMGF_FACTOR);
         // D_MININT is a special case which makes monster totally ignore damage
         int    dfi = df == D_MININT ? D_MININT : static_cast<int>(M_DoubleToFixed(df));

         info->meta->setInt(E_ModFieldName("damagefactor", mod), dfi);
      }
   }
}

//
// DropItems
//
// Multiple dropitems can now be assigned to thing types.
//

IMPLEMENT_RTTI_TYPE(MetaDropItem)

//
// Clear all dropitems from an mobjinfo's MetaTable
//
static void E_clearDropItems(mobjinfo_t *mi)
{
   MetaDropItem *mdi = nullptr;

   while((mdi = mi->meta->getNextTypeEx(mdi)))
   {
      mi->meta->removeObject(mdi);
      delete mdi;
      mdi = nullptr; // must restart search
   }
}

//
// Find a dropitem for a particular item type
// 
static MetaDropItem *E_findDropItemForType(mobjinfo_t *mi, const char *item)
{
   MetaDropItem *mdi = nullptr;

   while((mdi = mi->meta->getNextTypeEx(mdi)))
   {
      if(!mdi->item.strCaseCmp(item)) // be case insensitive
         return mdi;
   }

   return nullptr;
}

//
// Add a new dropitem to the mobjinfo's MetaTable
//
static void E_addDropItem(mobjinfo_t *mi, const char *item, int chance, 
                          int amount, bool toss)
{
   mi->meta->addObject(new MetaDropItem("dropitem", item, chance, amount, toss));
}

//
// Remove a particular dropitem from the mobjinfo's MetaTable
//
static void E_removeDropItem(mobjinfo_t *mi, const char *item)
{
   MetaDropItem *mdi;
   while((mdi = E_findDropItemForType(mi, item)))
   {
      mi->meta->removeObject(mdi);
      delete mdi;
   }
}

//
// Process dropitem multi-valued property defintions inside a thingtype.
//
static void E_processDropItems(mobjinfo_t *mi, cfg_t *thingsec)
{
   unsigned int numDropItems = cfg_size(thingsec, ITEM_TNG_DROPITEM);

   for(unsigned int i = 0; i < numDropItems; i++)
   {
      cfg_t *prop = cfg_getnmvprop(thingsec, ITEM_TNG_DROPITEM, i);

      E_addDropItem(mi,
         cfg_getstr(prop, ITEM_TNG_DROPITEM_ITEM),
         cfg_getint(prop, ITEM_TNG_DROPITEM_CHANCE),
         cfg_getint(prop, ITEM_TNG_DROPITEM_AMOUNT),
         !!cfg_getflag(prop, ITEM_TNG_DROPITEM_TOSS));
   }
}

//
// Collection Spawn
//
// A thingtype that specifies this will have a global collection created
// for all of its instances in each map. At level start, a thing of the
// type it indicates will (or may, depending on specified chances) spawn
// at one of the spots.
//

IMPLEMENT_RTTI_TYPE(MetaCollectionSpawn)

static void E_processCollectionSpawn(mobjinfo_t *mi, cfg_t *spawn)
{
   const char *type       = cfg_getstr(spawn, ITEM_TNG_COLSPAWN_TYPE);
   int         spchance   = cfg_getint(spawn, ITEM_TNG_COLSPAWN_SP);
   int         coopchance = cfg_getint(spawn, ITEM_TNG_COLSPAWN_COOP);
   int         dmchance   = cfg_getint(spawn, ITEM_TNG_COLSPAWN_DM);

   auto mcs = new MetaCollectionSpawn("collectionspawn", type, 
                                      spchance, coopchance, dmchance);
   mi->meta->addObject(mcs);

   // create the global collection for the spot thingtype, if it hasn't been
   // created already.
   MobjCollections.addCollection(mi->name);
}

//
// Collection item respawning
//
static void E_processItemRespawnAt(mobjinfo_t *mi, const char *name)
{
   if(*name)
   {
      if(E_ThingNumForName(name) < 0)
      {
         E_EDFLoggedWarning(2, 
            "Warning: Unknown thingtype '%s' specified as itemrespawnat for '%s'\n",
            name, mi->name);
      }
      mi->meta->setString("itemrespawnat", name);

      // create a collection for the respawn spot thingtype, if it hasn't been
      // created already.
      MobjCollections.addCollection(name);
   }
   else
      mi->meta->removeStringNR("itemrespawnat");
}

//
// Blood types
//
// These are specified on a SHOOTABLE mobj to override the game's default blood
// types for various types of damaging actions.
//

//
// Proceses a given blood property.
//
static void E_ProcessBlood(int i, cfg_t *cfg, const char *searchedprop)
{
   const char *bloodVal = cfg_getstr(cfg, searchedprop);

   // if empty or set to @default, this blood type definition will be removed.
   if(*bloodVal && strcasecmp(bloodVal, "@default"))
   {
      // "@none" is explicitly reserved in order to disable a specific type of blood
      if(strcasecmp(bloodVal, "@none") && E_ThingNumForName(bloodVal) < 0)
      {
         E_EDFLoggedWarning(2, "Invalid %s '%s' for thingtype '%s'\n", 
                            searchedprop, bloodVal, mobjinfo[i]->name);         
      }
      mobjinfo[i]->meta->addString(searchedprop, bloodVal);
   }
   else
      mobjinfo[i]->meta->removeStringNR(searchedprop);
}

static const char *const keyForBloodAction[NUMBLOODACTIONS] =
{
   ITEM_TNG_BLOODNORM,   // bullet
   ITEM_TNG_BLOODIMPACT, // projectile impact
   ITEM_TNG_BLOODRIP,    // ripper projectile
   ITEM_TNG_BLOODCRUSH,  // crusher blood
};

static const char * gamemodeinfo_t::* defaultForBloodAction[NUMBLOODACTIONS] =
{
   &gamemodeinfo_t::bloodDefaultNormal,
   &gamemodeinfo_t::bloodDefaultImpact,
   &gamemodeinfo_t::bloodDefaultRIP,
   &gamemodeinfo_t::bloodDefaultCrush
};

//
// Get the proper blood type to use for an Mobj in response to the given action.
// Returns -1 if there is not a valid blood type for this action. This may, in the
// case of an @none indicator, mean that no blood is meant to be spawned.
//
int E_BloodTypeForThing(const Mobj *mo, bloodaction_e action)
{
   const char *actionKey   = keyForBloodAction[action];
   const char *defaultType = GameModeInfo->*(defaultForBloodAction[action]);
   const char *typeName    = mo->info->meta->getString(actionKey, defaultType);

   return E_ThingNumForName(typeName);
}

//
// Blood behaviors
//
// These are specified on a blood object to specify how it behaves when spawned
// in response to the different types of actions which spawn blood.
//

IMPLEMENT_RTTI_TYPE(MetaBloodBehavior)

// blood behavior names
static const char *bloodBehaviors[BLOODTYPE_MAX] = 
{
   "DOOM", 
   "HERETIC", 
   "HERETICRIP", 
   "HEXEN",
   "HEXENRIP",
   "STRIFE", 
   "CRUSH",
   "CUSTOM" // NB: reserved for future Aeon usage
};

// blood action names
static const char *bloodActions[NUMBLOODACTIONS] = 
{
   "SHOT",   // bullet
   "IMPACT", // projectile impact
   "RIP",    // ripper projectile
   "CRUSH"   // crusher blood
};

//
// Clear all blood behaviors from an mobjinfo's MetaTable
//
static void E_clearBloodBehaviors(mobjinfo_t *mi)
{
   MetaBloodBehavior *mbb = nullptr;

   while((mbb = mi->meta->getNextTypeEx(mbb)))
   {
      mi->meta->removeObject(mbb);
      delete mbb;
      mbb = nullptr; // must restart search
   }
}

//
// Find a blood behavior for a particular action
// 
static MetaBloodBehavior *E_findBloodBehavior(mobjinfo_t *mi, bloodaction_e action)
{
   MetaBloodBehavior *mbb = nullptr;

   while((mbb = mi->meta->getNextTypeEx(mbb)))
   {
      if(mbb->action == action)
         return mbb;
   }

   return nullptr;
}

//
// Add a new blood behavior to the mobjinfo's MetaTable
//
static void E_addBloodBehavior(mobjinfo_t *mi, bloodaction_e action, bloodtype_e behavior)
{
   MetaBloodBehavior *mbb = nullptr;

   while((mbb = mi->meta->getNextTypeEx(mbb)))
   {
      if(mbb->action == action)
      {
         mbb->behavior = behavior;
         return;
      }
   }

   // none was found, so create a new one
   mi->meta->addObject(new MetaBloodBehavior(ITEM_TNG_BLOODBEHAV, action, behavior));
}

//
// Remove a particular blood behavior from the mobjinfo's MetaTable
//
static void E_removeBloodBehavior(mobjinfo_t *mi, bloodaction_e action)
{
   MetaBloodBehavior *mbb;
   while((mbb = E_findBloodBehavior(mi, action)))
   {
      mi->meta->removeObject(mbb);
      delete mbb;
   }
}

//
// Process bloodbehavior multi-valued property definitions inside a thingtype.
//
static void E_processBloodBehaviors(mobjinfo_t *mi, cfg_t *thingsec)
{
   unsigned int numBloodBehaviors = cfg_size(thingsec, ITEM_TNG_BLOODBEHAV);

   for(unsigned int i = 0; i < numBloodBehaviors; i++)
   {
      cfg_t *prop = cfg_getnmvprop(thingsec, ITEM_TNG_BLOODBEHAV, i);
      const char *action   = cfg_getstr(prop, ITEM_TNG_BB_ACTION);
      const char *behavior = cfg_getstr(prop, ITEM_TNG_BB_BEHAVIOR);

      int actionnum = E_StrToNumLinear(bloodActions, NUMBLOODACTIONS, action);
      if(actionnum == NUMBLOODACTIONS)
      {
         E_EDFLoggedWarning(2, 
            "Warning: Unknown blood action '%s' specified in bloodbehavior for '%s'\n",
            action, mi->name);
         continue;
      }

      int behaviornum = E_StrToNumLinear(bloodBehaviors, BLOODTYPE_MAX, behavior);
      if(behaviornum == BLOODTYPE_MAX)
      {
         // a string such as "none" or "default" will remove the specification
         E_removeBloodBehavior(mi, static_cast<bloodaction_e>(actionnum));
      }
      else
      {
         // set a new specification for this action
         E_addBloodBehavior(mi, static_cast<bloodaction_e>(actionnum),
                                static_cast<bloodtype_e>(behaviornum));
      }
   }
}

//
// Get the blood behavior type for a particular action for this thing type.
//
bloodtype_e E_GetBloodBehaviorForAction(mobjinfo_t *info, bloodaction_e action)
{
   MetaBloodBehavior *mbb = E_findBloodBehavior(info, action);   
   return mbb ? mbb->behavior : GameModeInfo->defBloodBehaviors[action];
}

static inline void E_processThingPickupEffect(mobjinfo_t &mi, cfg_t *thingsec)
{
   const char *str;
   cfg_t *pfx_cfg = cfg_getsec(thingsec, ITEM_TNG_PFX_PICKUPFX);

   if(mi.pickupfx == nullptr)
   {
      mi.pickupfx = estructalloc(e_pickupfx_t, 1);
      // TODO: Is setting name reuqired? Maybe this could be eliminated.
      qstring qname = qstring("_");
      qname += mi.name;
      mi.pickupfx->name = qname.duplicate();
   }
   // EDF_FEATURES_TODO: else efree? i.e. remove all the
   // internal properties of the CFG_SEC that were set beforehand

   e_pickupfx_t &pfx = *mi.pickupfx;

   if((str = cfg_getstr(pfx_cfg, ITEM_TNG_PFX_EFFECTS)))
   {
      if(pfx.numEffects)
         efree(pfx.effects);

      if((pfx.numEffects = cfg_size(pfx_cfg, ITEM_TNG_PFX_EFFECTS)))
      {
         pfx.effects = ecalloc(itemeffect_t **, 1, sizeof(itemeffect_t **));
         for(unsigned int i = 0; i < pfx.numEffects; i++)
         {
            str = cfg_getnstr(pfx_cfg, ITEM_TNG_PFX_EFFECTS, i);
            if(!(pfx.effects[i] = E_ItemEffectForName(str)))
            {
               E_EDFLoggedWarning(2, "Warning: invalid pickup effect: '%s'\n", str);
               return;
            }
         }
      }
   }
   
   if((str = cfg_getstr(pfx_cfg, ITEM_TNG_PFX_CHANGEWPN)))
   {
      if(estrnonempty(str) && !(pfx.changeweapon = E_WeaponForName(str)))
      {
         E_EDFLoggedWarning(2, "Warning: invalid changeweapon '%s' for pickup effect in "
                               "thingtype '%s'\n", str, mi.name);
      }
   }

   if((str = cfg_getstr(pfx_cfg, ITEM_TNG_PFX_MSG)))
   {
      if(pfx.message == nullptr)
         efree(pfx.message);
      pfx.message = estrdup(str);
   }

   if((str = cfg_getstr(pfx_cfg, ITEM_TNG_PFX_SOUND)))
   {
      if(pfx.sound == nullptr)
         efree(pfx.sound);
      pfx.sound = estrdup(str);
   }

   if((str = cfg_getstr(pfx_cfg, ITEM_TNG_PFX_FLAGS)))
      pfx.flags = E_PickupFlagsForStr(str);
}

//
// libConfuse value-parsing callback for the thingtype translation
// field. Can accept an integer value which indicates one of the 14
// builtin player translations, or a lump name, which will be looked
// up within the translations namespace (T_START/T_END), allowing for
// custom sprite translations.
//
static int E_ColorCB(cfg_t *cfg, cfg_opt_t *opt, const char *value,
                     void *result)
{
   int num;
   char *endptr;

   num = static_cast<int>(strtol(value, &endptr, 0));

   // try lump name
   if(*endptr != '\0')
   {
      int tlnum = R_TranslationNumForName(value);

      if(tlnum == -1)
      {
         if(cfg)
            cfg_error(cfg, "bad translation lump '%s'\n", value);
         return -1;
      }
         
      *(int *)result = tlnum;
   }
   else
   {
      *(int *)result = num % TRANSLATIONCOLOURS;
   }

   return 0;
}

//
// Translucency map support
//
static int E_TranMapCB(cfg_t *cfg, cfg_opt_t *opt, const char *value,
                       void *result)
{
   int *target = static_cast<int *>(result);
   if(!strcasecmp(value, "none"))   // accept none
   {
      *target = -1;
      return 0;
   }

   // try lump name
   int trnum = W_CheckNumForName(value);
   if(trnum < 0 || W_LumpLength(trnum) != 65536)
   {
      // Do not error out from this
      E_EDFLoggedWarning(2, "Bad translucency lump '%s'\n", value);
      *target = -1;
      return 0;
   }
   *target = trnum;

   return 0;
}

//
// Damagefactor value callback
//
static int E_damageFactorCB(cfg_t *cfg, cfg_opt_t *opt, const char *value,
   void *result)
{
   double *target = static_cast<double *>(result);
   // D_MININT means immune. Even floating-point is marked with D_MININT.
   *target = !strcasecmp(value, "immune") ? D_MININT : strtod(value, nullptr);
   return 0;
}

//
// Gibhealth Processing
//
// Separated out for global use 01/02/15
//
void E_ThingDefaultGibHealth(mobjinfo_t *mi)
{
   switch(GameModeInfo->defaultGibHealth)
   {
   case GI_GIBFULLHEALTH: // Doom and Strife
      mi->gibhealth = -mi->spawnhealth;
      break;
   case GI_GIBHALFHEALTH: // Heretic and Hexen
      mi->gibhealth = -mi->spawnhealth/2;
      break;
   default:
      break;
   }
}

// Thing type inheritance code -- 01/27/04

// thing_hitlist: keeps track of what thingtypes are initialized
static byte *thing_hitlist = nullptr;

// thing_pstack: used by recursive E_ProcessThing to track inheritance
static int  *thing_pstack  = nullptr;
static int   thing_pindex  = 0;

//
// Makes sure the thing type being inherited from has not already
// been inherited during the current inheritance chain. Returns
// false if the check fails, and true if it succeeds.
//
static bool E_CheckThingInherit(int pnum)
{
   for(int i = 0; i < NUMMOBJTYPES; i++)
   {
      // circular inheritance
      if(thing_pstack[i] == pnum)
         return false;

      // found end of list
      if(thing_pstack[i] == -1)
         break;
   }

   return true;
}

//
// Adds a type number to the inheritance stack.
//
static void E_AddThingToPStack(int num)
{
   // Overflow shouldn't happen since it would require cyclic
   // inheritance as well, but I'll guard against it anyways.
   
   if(thing_pindex >= NUMMOBJTYPES)
      E_EDFLoggedErr(2, "E_AddThingToPStack: max inheritance depth exceeded\n");

   thing_pstack[thing_pindex++] = num;
}

//
// Resets the thingtype inheritance stack, setting all the pstack
// values to -1, and setting pindex back to zero.
//
static void E_ResetThingPStack()
{
   for(int i = 0; i < NUMMOBJTYPES; i++)
      thing_pstack[i] = -1;

   thing_pindex = 0;
}

//
// Copies one thingtype into another.
//
static void E_CopyThing(int num, int pnum)
{
   mobjinfo_t *this_mi;
   DLListItem<mobjinfo_t> namelinks, cnamelinks, numlinks;
   char       *name, *cname;
   int         dehnum;
   MetaTable  *meta;
   int         index;
   int         generation;
   
   this_mi = mobjinfo[num];

   // must save the following fields in the destination thing:
   namelinks  = this_mi->namelinks;
   cnamelinks = this_mi->cnamelinks;
   numlinks   = this_mi->numlinks;
   name       = this_mi->name;
   cname      = this_mi->compatname;
   dehnum     = this_mi->dehnum;
   meta       = this_mi->meta;
   index      = this_mi->index;
   generation = this_mi->generation;
   
   // copy from source to destination
   memcpy(this_mi, mobjinfo[pnum], sizeof(mobjinfo_t));

   // normalize special fields

   // must duplicate obituaries if they exist
   if(this_mi->obituary)
      this_mi->obituary = estrdup(this_mi->obituary);
   if(this_mi->meleeobit)
      this_mi->meleeobit = estrdup(this_mi->meleeobit);

   // copy metatable
   meta->copyTableFrom(mobjinfo[pnum]->meta);

   // restore metatable pointer
   this_mi->meta = meta;

   // must restore name and dehacked num data
   this_mi->namelinks  = namelinks;
   this_mi->cnamelinks = cnamelinks;
   this_mi->numlinks   = numlinks;
   this_mi->name       = name;
   this_mi->compatname = cname;
   this_mi->dehnum     = dehnum;
   this_mi->index      = index;
   this_mi->generation = generation;

   // other fields not inherited:

   // force doomednum of inheriting type to -1
   this_mi->doomednum = -1;
}

struct thingtitleprops_t
{
   const char *superclass;
   int dehackednum;
   int doomednum;
};

//
// Retrieve all the values in the thing's title properties, if such
// are defined.
//
static void E_getThingTitleProps(cfg_t *thingsec, thingtitleprops_t &props, bool def)
{
   cfg_t *titleprops;

   if(def && cfg_size(thingsec, "#title") > 0 && 
      (titleprops = cfg_gettitleprops(thingsec)))
   {
      props.superclass  = cfg_getstr(titleprops, ITEM_TNG_TITLE_SUPER);
      props.dehackednum = cfg_getint(titleprops, ITEM_TNG_TITLE_DEHNUM);
      props.doomednum   = cfg_getint(titleprops, ITEM_TNG_TITLE_DOOMEDNUM);
   }
   else
   {
      props.superclass  = nullptr;
      props.dehackednum = -1;
      props.doomednum   = -1;
   }
}

//
// Get the mobjinfo index for the thing's superclass thingtype.
//
static int E_resolveParentThingType(cfg_t *thingsec, 
                                    const thingtitleprops_t &props)
{
   int pnum = -1;

   // check title props first
   if(props.superclass)
   {
      // "Mobj" is currently a dummy value and means it is just a plain 
      // thing not inheriting from anything else. Maybe in the future it
      // could designate specialized native subclasses of Mobj as well?
      if(strcasecmp(props.superclass, "Mobj"))
         pnum = E_GetThingNumForName(props.superclass);
   }
   else // resolve parent thingtype through legacy "inherits" field
      pnum = E_GetThingNumForName(cfg_getstr(thingsec, ITEM_TNG_INHERITS));

   return pnum;
}

// IS_SET: this macro tests whether or not a particular field should
// be set. When applying deltas, we should not retrieve defaults.
// 01/27/04: Also, if inheritance is taking place, we should not
// retrieve defaults.

#undef  IS_SET
#define IS_SET(name) ((def && !inherits) || cfg_size(thingsec, (name)) > 0)

// Same as above, but also considers if the basictype has set fields in the thing.
// In that case, fields which are affected by basictype don't set defaults unless
// they are explicitly specified.

#define IS_SET_BT(name) \
   ((def && !inherits && !hasbtype) || cfg_size(thingsec, (name)) > 0)

//
// Generalized code to process the data for a single thing type
// structure. Doubles as code for thingtype and thingdelta.
//
void E_ProcessThing(int i, cfg_t *thingsec, cfg_t *pcfg, bool def)
{
   double tempfloat;
   int tempint;
   const char *tempstr;
   bool inherits = false;
   bool cflags   = false;
   bool hasbtype = false;
   thingtitleprops_t titleprops;

   // if thingsec is null, we are in the situation of inheriting from a thing
   // that was processed in a previous EDF generation, so no processing is
   // required; return immediately.
   if(!thingsec)
      return; 

   // Retrieve title properties
   E_getThingTitleProps(thingsec, titleprops, def);

   // 01/27/04: added inheritance -- not in deltas
   if(def)
   {
      int pnum = -1;

      // if this thingtype is already processed via recursion due to
      // inheritance, don't process it again
      if(thing_hitlist[i])
         return;

      if(titleprops.superclass || cfg_size(thingsec, ITEM_TNG_INHERITS) > 0)
         pnum = E_resolveParentThingType(thingsec, titleprops);
      
      if(pnum >= 0)
      {
         cfg_t *parent_tngsec;
         int pnum = E_resolveParentThingType(thingsec, titleprops);

         // check against cyclic inheritance
         if(!E_CheckThingInherit(pnum))
         {
            E_EDFLoggedErr(2, 
               "E_ProcessThing: cyclic inheritance detected in thing '%s'\n",
               mobjinfo[i]->name);
         }
         
         // add to inheritance stack
         E_AddThingToPStack(pnum);

         // process parent recursively
         // 12/12/2011: must use cfg_gettsec; note can return null
         parent_tngsec = cfg_gettsec(pcfg, EDF_SEC_THING, mobjinfo[pnum]->name);
         E_ProcessThing(pnum, parent_tngsec, pcfg, true);
         
         // copy parent to this thing
         E_CopyThing(i, pnum);

         // haleyjd 06/19/09: keep track of parent explicitly
         mobjinfo[i]->parent = mobjinfo[pnum];
         
         // we inherit, so treat defaults as no value
         inherits = true;
      }
      else
         mobjinfo[i]->parent = nullptr; // 6/19/09: no parent.

      // mark this thingtype as processed
      thing_hitlist[i] = 1;
   }

   // haleyjd 07/05/06: process basictype
   // Note that when basictype is present, the default handling of all fields
   // affected by the basictype will be changed.
   if(IS_SET(ITEM_TNG_BASICTYPE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_BASICTYPE);
      tempint = E_StrToNumLinear(BasicTypeNames, NUMBASICTYPES, tempstr);
      if(tempint != NUMBASICTYPES)
      {
         basicttype_t *basicType = &BasicThingTypes[tempint];

         mobjinfo[i]->flags  = basicType->flags;
         mobjinfo[i]->flags2 = basicType->flags2;
         mobjinfo[i]->flags3 = basicType->flags3;

         if(basicType->spawnstate &&
            (tempint = E_StateNumForName(basicType->spawnstate)) >= 0)
            mobjinfo[i]->spawnstate = tempint;
         else if(!inherits) // don't init to default if the thingtype inherits
            mobjinfo[i]->spawnstate = NullStateNum;

         hasbtype = true; // mobjinfo has a basictype set
      }
   }

   // process doomednum
   // haleyjd 09/30/12: allow preferential definition by title properties
   if(titleprops.doomednum != -1)
      mobjinfo[i]->doomednum = titleprops.doomednum;
   else if(IS_SET(ITEM_TNG_DOOMEDNUM))
      mobjinfo[i]->doomednum = cfg_getint(thingsec, ITEM_TNG_DOOMEDNUM);

   // ******************************** STATES ********************************

   // process spawnstate
   if(IS_SET_BT(ITEM_TNG_SPAWNSTATE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_SPAWNSTATE);
      E_ThingFrame(tempstr, ITEM_TNG_SPAWNSTATE, i, 
                   &(mobjinfo[i]->spawnstate));
   }

   // process seestate
   if(IS_SET(ITEM_TNG_SEESTATE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_SEESTATE);
      E_ThingFrame(tempstr, ITEM_TNG_SEESTATE, i,
                   &(mobjinfo[i]->seestate));
   }

   // process painstate
   if(IS_SET(ITEM_TNG_PAINSTATE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_PAINSTATE);
      E_ThingFrame(tempstr, ITEM_TNG_PAINSTATE, i,
                   &(mobjinfo[i]->painstate));
   }

   // process meleestate
   if(IS_SET(ITEM_TNG_MELEESTATE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_MELEESTATE);
      E_ThingFrame(tempstr, ITEM_TNG_MELEESTATE, i,
                   &(mobjinfo[i]->meleestate));
   }

   // process missilestate
   if(IS_SET(ITEM_TNG_MISSILESTATE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_MISSILESTATE);
      E_ThingFrame(tempstr, ITEM_TNG_MISSILESTATE, i,
                   &(mobjinfo[i]->missilestate));
   }

   // process deathstate
   if(IS_SET(ITEM_TNG_DEATHSTATE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_DEATHSTATE);
      E_ThingFrame(tempstr, ITEM_TNG_DEATHSTATE, i,
                   &(mobjinfo[i]->deathstate));
   }

   // process xdeathstate
   if(IS_SET(ITEM_TNG_XDEATHSTATE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_XDEATHSTATE);
      E_ThingFrame(tempstr, ITEM_TNG_XDEATHSTATE, i,
                   &(mobjinfo[i]->xdeathstate));
   }

   // process raisestate
   if(IS_SET(ITEM_TNG_RAISESTATE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_RAISESTATE);
      E_ThingFrame(tempstr, ITEM_TNG_RAISESTATE, i,
                   &(mobjinfo[i]->raisestate));
   }

   // ioanch 20160220: process healstate
   if(cfg_size(thingsec, ITEM_TNG_HEALSTATE) > 0)
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_HEALSTATE);
      E_ThingFrame(tempstr, ITEM_TNG_HEALSTATE, i, METASTATE_HEAL);
   }

   // 08/07/04: process crashstate
   if(IS_SET(ITEM_TNG_CRASHSTATE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_CRASHSTATE);
      E_ThingFrame(tempstr, ITEM_TNG_CRASHSTATE, i,
                   &(mobjinfo[i]->crashstate));
   }

   // 03/19/11: process active/inactive states
   if(IS_SET(ITEM_TNG_ACTIVESTATE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_ACTIVESTATE);
      E_ThingFrame(tempstr, ITEM_TNG_ACTIVESTATE, i, 
                   &(mobjinfo[i]->activestate));
   }

   if(IS_SET(ITEM_TNG_INACTIVESTATE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_INACTIVESTATE);
      E_ThingFrame(tempstr, ITEM_TNG_INACTIVESTATE, i,
                   &(mobjinfo[i]->inactivestate));
   }

   // ******************************** SOUNDS ********************************
   
   // process seesound
   if(IS_SET(ITEM_TNG_SEESOUND))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_SEESOUND);
      E_ThingSound(tempstr, ITEM_TNG_SEESOUND, i,
                   &(mobjinfo[i]->seesound));
   }

   // process attacksound
   if(IS_SET(ITEM_TNG_ATKSOUND))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_ATKSOUND);
      E_ThingSound(tempstr, ITEM_TNG_ATKSOUND, i,
                   &(mobjinfo[i]->attacksound));
   }

   // process painsound
   if(IS_SET(ITEM_TNG_PAINSOUND))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_PAINSOUND);
      E_ThingSound(tempstr, ITEM_TNG_PAINSOUND, i,
                   &(mobjinfo[i]->painsound));
   }

   // process deathsound
   if(IS_SET(ITEM_TNG_DEATHSOUND))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_DEATHSOUND);
      E_ThingSound(tempstr, ITEM_TNG_DEATHSOUND, i,
                   &(mobjinfo[i]->deathsound));
   }

   // process activesound
   if(IS_SET(ITEM_TNG_ACTIVESOUND))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_ACTIVESOUND);
      E_ThingSound(tempstr, ITEM_TNG_ACTIVESOUND, i,
                   &(mobjinfo[i]->activesound));
   }

   // 3/19/11: process activatesound/deactivatesound
   if(IS_SET(ITEM_TNG_ACTIVATESND))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_ACTIVATESND);
      E_ThingSound(tempstr, ITEM_TNG_ACTIVATESND, i,
                   &(mobjinfo[i]->activatesound));
   }

   if(IS_SET(ITEM_TNG_DEACTIVATESND))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_DEACTIVATESND);
      E_ThingSound(tempstr, ITEM_TNG_DEACTIVATESND, i,
                   &(mobjinfo[i]->deactivatesound));
   }

   // ******************************* METRICS ********************************

   // process spawnhealth
   bool setspawnhealth = false;
   if(IS_SET(ITEM_TNG_SPAWNHEALTH))
   {
      mobjinfo[i]->spawnhealth = cfg_getint(thingsec, ITEM_TNG_SPAWNHEALTH);
      setspawnhealth = true;
   }

   // process gibhealth
   if(IS_SET(ITEM_TNG_GIBHEALTH) || setspawnhealth)
   {
      // if spawnhealth was set and we're going to inherit gibhealth, or
      // get the EDF default for gibhealth, use the default behavior instead.
      if(setspawnhealth && !cfg_size(thingsec, ITEM_TNG_GIBHEALTH))
      {
         E_ThingDefaultGibHealth(mobjinfo[i]);
      }
      else
         mobjinfo[i]->gibhealth = cfg_getint(thingsec, ITEM_TNG_GIBHEALTH);
   }

   // process reactiontime
   if(IS_SET(ITEM_TNG_REACTTIME))
      mobjinfo[i]->reactiontime = cfg_getint(thingsec, ITEM_TNG_REACTTIME);

   // process painchance
   if(IS_SET(ITEM_TNG_PAINCHANCE))
      mobjinfo[i]->painchance = cfg_getint(thingsec, ITEM_TNG_PAINCHANCE);

   // process speed
   if(IS_SET(ITEM_TNG_SPEED))
      mobjinfo[i]->speed = cfg_getint(thingsec, ITEM_TNG_SPEED);

   // 07/13/03: process fastspeed
   // get the fastspeed and, if non-zero, add the thing
   // to the speedset list in g_game.c

   if(IS_SET(ITEM_TNG_FASTSPEED))
   {
      tempint = cfg_getint(thingsec, ITEM_TNG_FASTSPEED);         
      if(tempint)
         G_SpeedSetAddThing(i, mobjinfo[i]->speed, tempint);
   }

   // process radius
   if(IS_SET(ITEM_TNG_RADIUS))
   {
      tempfloat = cfg_getfloat(thingsec, ITEM_TNG_RADIUS);
      mobjinfo[i]->radius = (int)(tempfloat * FRACUNIT);
   }

   // process height
   if(IS_SET(ITEM_TNG_HEIGHT))
   {
      tempfloat = cfg_getfloat(thingsec, ITEM_TNG_HEIGHT);
      mobjinfo[i]->height = (int)(tempfloat * FRACUNIT);
   }

   // 07/06/05: process correct 3D thing height
   if(IS_SET(ITEM_TNG_C3DHEIGHT))
   {
      tempfloat = cfg_getfloat(thingsec, ITEM_TNG_C3DHEIGHT);
      mobjinfo[i]->c3dheight = (int)(tempfloat * FRACUNIT);
   }

   // process mass
   if(IS_SET(ITEM_TNG_MASS))
      mobjinfo[i]->mass = cfg_getint(thingsec, ITEM_TNG_MASS);

   // 09/23/09: respawn properties
   if(IS_SET(ITEM_TNG_RESPAWNTIME))
      mobjinfo[i]->respawntime = cfg_getint(thingsec, ITEM_TNG_RESPAWNTIME);

   if(IS_SET(ITEM_TNG_RESPCHANCE))
      mobjinfo[i]->respawnchance = cfg_getint(thingsec, ITEM_TNG_RESPCHANCE);

   // aim shift
   if(cfg_size(thingsec, ITEM_TNG_AIMSHIFT) > 0)
      mobjinfo[i]->meta->setInt("aimshift", cfg_getint(thingsec, ITEM_TNG_AIMSHIFT));

   // process damage
   if(IS_SET(ITEM_TNG_DAMAGE))
      mobjinfo[i]->damage = cfg_getint(thingsec, ITEM_TNG_DAMAGE);

   // 09/22/06: process topdamage 
   if(IS_SET(ITEM_TNG_TOPDAMAGE))
      mobjinfo[i]->topdamage = cfg_getint(thingsec, ITEM_TNG_TOPDAMAGE);

   // 09/23/06: process topdamagemask
   if(IS_SET(ITEM_TNG_TOPDMGMASK))
      mobjinfo[i]->topdamagemask = cfg_getint(thingsec, ITEM_TNG_TOPDMGMASK);

   // process translucency
   if(IS_SET(ITEM_TNG_TRANSLUC))
      mobjinfo[i]->translucency = cfg_getint(thingsec, ITEM_TNG_TRANSLUC);

   // process translucency map
   if(IS_SET(ITEM_TNG_TRANMAP))
      mobjinfo[i]->tranmap = cfg_getint(thingsec, ITEM_TNG_TRANMAP);

   // process bloodcolor
   if(IS_SET(ITEM_TNG_BLOODCOLOR))
      mobjinfo[i]->bloodcolor = cfg_getint(thingsec, ITEM_TNG_BLOODCOLOR);

   // 05/23/08: process alphavelocity
   if(IS_SET(ITEM_TNG_AVELOCITY))
   {
      tempfloat = cfg_getfloat(thingsec, ITEM_TNG_AVELOCITY);
      mobjinfo[i]->alphavelocity = (fixed_t)(tempfloat * FRACUNIT);
   }

   // 11/22/09: scaling properties
   if(IS_SET(ITEM_TNG_XSCALE))
      mobjinfo[i]->xscale = (float)cfg_getfloat(thingsec, ITEM_TNG_XSCALE);

   if(IS_SET(ITEM_TNG_YSCALE))
      mobjinfo[i]->yscale = (float)cfg_getfloat(thingsec, ITEM_TNG_YSCALE);

   // ********************************* FLAGS ********************************

   // 02/19/04: process combined flags first
   if(IS_SET_BT(ITEM_TNG_CFLAGS))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_CFLAGS);
      if(*tempstr == '\0')
      {
         mobjinfo[i]->flags = mobjinfo[i]->flags2 = mobjinfo[i]->flags3 = 0;
      }
      else
      {
         unsigned int *results = deh_ParseFlagsCombined(tempstr);

         mobjinfo[i]->flags  = results[0];
         mobjinfo[i]->flags2 = results[1];
         mobjinfo[i]->flags3 = results[2];
         mobjinfo[i]->flags4 = results[3];
         
         cflags = true; // values were set from cflags
      }
   }

   if(!cflags) // skip if cflags are defined
   {
      // process flags
      if(IS_SET_BT(ITEM_TNG_FLAGS))
      {
         tempstr = cfg_getstr(thingsec, ITEM_TNG_FLAGS);
         if(*tempstr == '\0')
            mobjinfo[i]->flags = 0;
         else
            mobjinfo[i]->flags = deh_ParseFlagsSingle(tempstr, DEHFLAGS_MODE1);
      }
      
      // process flags2
      if(IS_SET_BT(ITEM_TNG_FLAGS2))
      {
         tempstr = cfg_getstr(thingsec, ITEM_TNG_FLAGS2);
         if(*tempstr == '\0')
            mobjinfo[i]->flags2 = 0;
         else
            mobjinfo[i]->flags2 = deh_ParseFlagsSingle(tempstr, DEHFLAGS_MODE2);
      }

      // process flags3
      if(IS_SET_BT(ITEM_TNG_FLAGS3))
      {
         tempstr = cfg_getstr(thingsec, ITEM_TNG_FLAGS3);
         if(*tempstr == '\0')
            mobjinfo[i]->flags3 = 0;
         else
            mobjinfo[i]->flags3 = deh_ParseFlagsSingle(tempstr, DEHFLAGS_MODE3);
      }

      // process flags4
      if(IS_SET(ITEM_TNG_FLAGS4))
      {
         tempstr = cfg_getstr(thingsec, ITEM_TNG_FLAGS4);
         if(*tempstr == '\0')
            mobjinfo[i]->flags4 = 0;
         else
            mobjinfo[i]->flags4 = deh_ParseFlagsSingle(tempstr, DEHFLAGS_MODE4);
      }
   }

   // process addflags and remflags modifiers

   if(cfg_size(thingsec, ITEM_TNG_ADDFLAGS) > 0)
   {
      unsigned int *results;

      tempstr = cfg_getstr(thingsec, ITEM_TNG_ADDFLAGS);
         
      results = deh_ParseFlagsCombined(tempstr);

      mobjinfo[i]->flags  |= results[0];
      mobjinfo[i]->flags2 |= results[1];
      mobjinfo[i]->flags3 |= results[2];
      mobjinfo[i]->flags4 |= results[3];
   }

   if(cfg_size(thingsec, ITEM_TNG_REMFLAGS) > 0)
   {
      unsigned int *results;

      tempstr = cfg_getstr(thingsec, ITEM_TNG_REMFLAGS);

      results = deh_ParseFlagsCombined(tempstr);

      mobjinfo[i]->flags  &= ~(results[0]);
      mobjinfo[i]->flags2 &= ~(results[1]);
      mobjinfo[i]->flags3 &= ~(results[2]);
      mobjinfo[i]->flags4 &= ~(results[3]);
   }

   // 07/13/03: process nukespecial
   if(IS_SET(ITEM_TNG_NUKESPEC))
   {
      deh_bexptr *dp;

      tempstr = cfg_getstr(thingsec, ITEM_TNG_NUKESPEC);
      
      if(!(dp = D_GetBexPtr(tempstr)))
      {
         E_EDFLoggedErr(2, 
            "E_ProcessThing: thing '%s': bad nukespecial '%s'\n",
            mobjinfo[i]->name, tempstr);
      }
      
      if(dp->cptr != nullptr)
         mobjinfo[i]->nukespec = dp->cptr;
   }

   // 07/13/03: process particlefx
   if(IS_SET(ITEM_TNG_PARTICLEFX))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_PARTICLEFX);
      if(*tempstr == '\0')
         mobjinfo[i]->particlefx = 0;
      else
         mobjinfo[i]->particlefx = E_ParseFlags(tempstr, &particle_flags);
   }

   // *************************** ITEM PROPERTIES ****************************

   // 08/06/13: check for cleardropitems flag
   if(cfg_size(thingsec, ITEM_TNG_CLRDROPITEM) > 0)
      E_clearDropItems(mobjinfo[i]);

   // 08/06/13: check for dropitem.remove statements
   unsigned int numRem;
   if((numRem = cfg_size(thingsec, ITEM_TNG_REMDROPITEM)) > 0)
   {
      for(unsigned int i = 0; i < numRem; i++)
      {
         const char *item = cfg_getnstr(thingsec, ITEM_TNG_REMDROPITEM, i);
         E_removeDropItem(mobjinfo[i], item);
      }
   }

   // 07/13/03: process droptype (deprecated in favor of dropitem, but will
   // never be removed as it's good shorthand for DOOM-style item drops)
   if(IS_SET(ITEM_TNG_DROPTYPE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_DROPTYPE);
      if(strcasecmp(tempstr, "NONE"))
         E_addDropItem(mobjinfo[i], tempstr, 255, 0, false);
   }

   // 08/06/13: process dropitems
   E_processDropItems(mobjinfo[i], thingsec);

   // 08/15/13: process collection spawn
   if(cfg_size(thingsec, ITEM_TNG_COLSPAWN) > 0)
      E_processCollectionSpawn(mobjinfo[i], cfg_getmvprop(thingsec, ITEM_TNG_COLSPAWN));

   // 08/22/13: item respawn at collection
   if(IS_SET(ITEM_TNG_ITEMRESPAT))
      E_processItemRespawnAt(mobjinfo[i], cfg_getstr(thingsec, ITEM_TNG_ITEMRESPAT));

   // ************************************************************************

   // 07/13/03: process mod
   if(IS_SET(ITEM_TNG_MOD))
   {
      emod_t *mod;
      char *endpos = nullptr;
      tempstr = cfg_getstr(thingsec, ITEM_TNG_MOD);

      tempint = static_cast<int>(strtol(tempstr, &endpos, 0));
      
      if(endpos && *endpos == '\0')
         mod = E_DamageTypeForNum(tempint);  // it is a number
      else
         mod = E_DamageTypeForName(tempstr); // it is a name

      mobjinfo[i]->mod = mod->num; // mobjinfo stores the numeric key
   }

   // 07/13/03: process obituaries
   if(IS_SET(ITEM_TNG_OBIT1))
   {
      // if this is a delta or the thing type inherited obits
      // from its parent, we need to free any old obituary
      if((!def || inherits) && mobjinfo[i]->obituary)
         efree(mobjinfo[i]->obituary);

      tempstr = cfg_getstr(thingsec, ITEM_TNG_OBIT1);
      if(strcasecmp(tempstr, "NONE"))
         mobjinfo[i]->obituary = estrdup(tempstr);
      else
         mobjinfo[i]->obituary = nullptr;
   }

   if(IS_SET(ITEM_TNG_OBIT2))
   {
      // if this is a delta or the thing type inherited obits
      // from its parent, we need to free any old obituary
      if((!def || inherits) && mobjinfo[i]->meleeobit)
         efree(mobjinfo[i]->meleeobit);

      tempstr = cfg_getstr(thingsec, ITEM_TNG_OBIT2);
      if(strcasecmp(tempstr, "NONE"))
         mobjinfo[i]->meleeobit = estrdup(tempstr);
      else
         mobjinfo[i]->meleeobit = nullptr;
   }

   // 01/12/04: process translation
   if(IS_SET(ITEM_TNG_COLOR))
      mobjinfo[i]->colour = cfg_getint(thingsec, ITEM_TNG_COLOR);

   // 08/01/04: process dmgspecial
   if(IS_SET(ITEM_TNG_DMGSPECIAL))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_DMGSPECIAL);
      
      // find the proper dmgspecial number (linear search)
      tempint = E_StrToNumLinear(inflictorTypes, INFLICTOR_NUMTYPES, tempstr);

      if(tempint == INFLICTOR_NUMTYPES)
      {
         E_EDFLoggedErr(2, 
            "E_ProcessThing: thing '%s': invalid dmgspecial '%s'\n",
            mobjinfo[i]->name, tempstr);
      }

      mobjinfo[i]->dmgspecial = tempint;
   }

   // 09/26/04: process alternate sprite
   if(IS_SET(ITEM_TNG_SKINSPRITE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_SKINSPRITE);
      mobjinfo[i]->altsprite = E_SpriteNumForName(tempstr);
   }

   // 06/11/08: process defaultsprite (for skin handling)
   if(IS_SET(ITEM_TNG_DEFSPRITE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_DEFSPRITE);

      if(tempstr)
         mobjinfo[i]->defsprite = E_SpriteNumForName(tempstr);
      else
         mobjinfo[i]->defsprite = -1;
   }


   // 06/05/08: process custom-damage painstates
   if(IS_SET(ITEM_TNG_PAINSTATES))
   {
      E_ProcessDamageTypeStates(thingsec, ITEM_TNG_PAINSTATES, mobjinfo[i],
                                E_DTS_MODE_OVERWRITE, E_DTS_FIELD_PAIN);
   }
   if(IS_SET(ITEM_TNG_PNSTATESADD))
   {
      E_ProcessDamageTypeStates(thingsec, ITEM_TNG_PNSTATESADD, mobjinfo[i],
                                E_DTS_MODE_ADD, E_DTS_FIELD_PAIN);
   }
   if(IS_SET(ITEM_TNG_PNSTATESREM))
   {
      E_ProcessDamageTypeStates(thingsec, ITEM_TNG_PNSTATESREM, mobjinfo[i],
                                E_DTS_MODE_REMOVE, E_DTS_FIELD_PAIN);
   }

   // 06/05/08: process custom-damage deathstates
   if(IS_SET(ITEM_TNG_DEATHSTATES))
   {
      E_ProcessDamageTypeStates(thingsec, ITEM_TNG_DEATHSTATES, mobjinfo[i],
                                E_DTS_MODE_OVERWRITE, E_DTS_FIELD_DEATH);
   }
   if(IS_SET(ITEM_TNG_DTHSTATESADD))
   {
      E_ProcessDamageTypeStates(thingsec, ITEM_TNG_DTHSTATESADD, mobjinfo[i],
                                E_DTS_MODE_ADD, E_DTS_FIELD_DEATH);
   }
   if(IS_SET(ITEM_TNG_DTHSTATESREM))
   {
      E_ProcessDamageTypeStates(thingsec, ITEM_TNG_DTHSTATESREM, mobjinfo[i],
                                E_DTS_MODE_REMOVE, E_DTS_FIELD_DEATH);
   }

   // 10/11/09: process damagefactors
   E_ProcessDamageFactors(mobjinfo[i], thingsec);

   // MaxW: 20150620: process blood types and behavior
   if(IS_SET(ITEM_TNG_BLOODNORM))
      E_ProcessBlood(i, thingsec, ITEM_TNG_BLOODNORM);

   if(IS_SET(ITEM_TNG_BLOODIMPACT))
      E_ProcessBlood(i, thingsec, ITEM_TNG_BLOODIMPACT);

   if(IS_SET(ITEM_TNG_BLOODRIP))
      E_ProcessBlood(i, thingsec, ITEM_TNG_BLOODRIP);

   if(IS_SET(ITEM_TNG_BLOODCRUSH))
      E_ProcessBlood(i, thingsec, ITEM_TNG_BLOODCRUSH);

   // check for clear blood behaviors flag
   if(cfg_size(thingsec, ITEM_TNG_CLRBLOODBEH) > 0)
      E_clearBloodBehaviors(mobjinfo[i]);

   // process blood behaviors
   E_processBloodBehaviors(mobjinfo[i], thingsec);

   // 01/17/07: process acs_spawndata
   if(cfg_size(thingsec, ITEM_TNG_ACS_SPAWN) > 0)
   {
      cfg_t *acs_sec = cfg_getsec(thingsec, ITEM_TNG_ACS_SPAWN);

      // get ACS spawn number
      tempint = cfg_getint(acs_sec, ITEM_TNG_ACS_NUM);

      // rangecheck number
      if(tempint >= ACS_NUM_THINGTYPES)
      {
         E_EDFLoggedWarning(2, "Warning: invalid ACS spawn number %d\n", tempint);
      }
      else if(tempint >= 0) // negative numbers mean no spawn number
      {
         unsigned int flags;

         // get mode flags
         tempstr = cfg_getstr(acs_sec, ITEM_TNG_ACS_MODES);

         flags = E_ParseFlags(tempstr, &acs_gamemode_flags);

         if(flags & gameTypeToACSFlags[GameModeInfo->type])
            ACS_thingtypes[tempint] = i;
      }
   }

   // Process DECORATE state block
   E_ProcessDecorateStatesRecursive(thingsec, i, false);
}

//
// Resolves and loads all information for the mobjinfo_t structures.
//
void E_ProcessThings(cfg_t *cfg)
{
   unsigned int i, numthings;
   static bool firsttime = true;

   E_EDFLogPuts("\t* Processing thing data\n");

   numthings = cfg_size(cfg, EDF_SEC_THING);

   // allocate inheritance stack and hitlist
   thing_hitlist = ecalloc(byte *, NUMMOBJTYPES, sizeof(byte));
   thing_pstack  = ecalloc(int  *, NUMMOBJTYPES, sizeof(int));

   // add all things from previous generations to the processed hit list
   for(i = 0; i < (unsigned int)NUMMOBJTYPES; i++)
   {
      if(mobjinfo[i]->generation != edf_thing_generation)
         thing_hitlist[i] = 1;
   }

   // 01/17/07: first time, initialize ACS thingtypes array
   if(firsttime)
   {
     for(i = 0; i < ACS_NUM_THINGTYPES; i++)
        ACS_thingtypes[i] = UnknownThingType;
   }

   for(i = 0; i < numthings; i++)
   {
      cfg_t *thingsec  = cfg_getnsec(cfg, EDF_SEC_THING, i);
      const char *name = cfg_title(thingsec);
      int thingnum     = E_ThingNumForName(name);

      // reset the inheritance stack
      E_ResetThingPStack();

      // add this thing to the stack
      E_AddThingToPStack(thingnum);

      E_ProcessThing(thingnum, thingsec, cfg, true);
      
      E_EDFLogPrintf("\t\tFinished thingtype %s (#%d)\n", 
                     mobjinfo[thingnum]->name, thingnum);
   }

   // free tables
   efree(thing_hitlist);
   efree(thing_pstack);

   // increment generation count
   ++edf_thing_generation;
}

//
// True if mobjtype low is a descendant of high.
//
static bool E_mobjTypeIsDescendantOf(mobjtype_t low, mobjtype_t high)
{
   for(const mobjinfo_t *info = mobjinfo[low]; info; info = info->parent)
      if(info->index == high)
         return true;
   return false;
}

//
// Process thing group definitions
//
void E_ProcessThingGroups(cfg_t *cfg)
{
   unsigned numgroups = cfg_size(cfg, EDF_SEC_THINGGROUP);
   ThingGroup *group;

   // Have a visited list
   ZAutoBuffer zvisited(NUMMOBJTYPES, true);
   bool *visited = zvisited.getAs<bool *>();

   for(unsigned i = 0; i < numgroups; ++i)
   {
      cfg_t *gsec = cfg_getnsec(cfg, EDF_SEC_THINGGROUP, i);
      const char *name = cfg_title(gsec);

      group = thinggroup_namehash.objectForKey(name);
      if(!group)
      {
         E_EDFLogPrintf("\t\tCreating thing group '%s'\n", name);
         group = new ThingGroup(name);
         thinggroup_namehash.addObject(group);
      }
      else
         E_EDFLogPrintf("\t\tModifying thing group '%s'\n", name);

      const char *tempstr = cfg_getstr(gsec, ITEM_TGROUP_FLAGS);
      if(estrnonempty(tempstr))
         group->flags = E_ParseFlags(tempstr, &tgroup_kindset);

      unsigned numtypes = cfg_size(gsec, ITEM_TGROUP_TYPES);
      if(numtypes)
      {
         group->types.clear();   // clear it even if thinggroup redefined.
         for(unsigned j = 0; j < numtypes; ++j)
         {
            tempstr = cfg_getnstr(gsec, ITEM_TGROUP_TYPES, j);
            int type = E_ThingNumForName(tempstr);
            if(type != -1 && !visited[type])
            {
               visited[type] = true;
               group->types.add(type);
               if(group->flags & TGF_INHERITED)
               {
                  // Also check children
                  for(int k = 0; k < NUMMOBJTYPES; ++k)
                  {
                     if(visited[k] || !E_mobjTypeIsDescendantOf(k, type))
                        continue;

                     visited[k] = true;
                     group->types.add(k);
                  }
               }
            }
            else if(!visited[type]) // don't scream if visited
            {
               E_EDFLoggedWarning(2, "Warning: unknown type '%s' for group '%s'\n",
                                  tempstr, name);
            }
         }
      }
   }

   // Now process them
   thinggrouppair_t *proj;
   while((proj = thinggrouppairs.tableIterator((thinggrouppair_t*)nullptr)))
   {
      thinggrouppairs.removeObject(proj);
      efree(proj);
   }

   group = nullptr;
   static const unsigned operationalFlags = TGF_PROJECTILEALLIANCE | TGF_DAMAGEIGNORE;
   while((group = thinggroup_namehash.tableIterator(group)))
   {
      // Currently only these are supported
      if(!(group->flags & operationalFlags))
         continue;
      // Setup relation
      for(int entry : group->types)
      {
         for(int other : group->types)
         {
            if(other <= entry)
               continue;
            int64_t key = (int64_t)entry | (int64_t)other << 32;
            proj = thinggrouppairs.objectForKey(key);
            if (!proj)
            {
               proj = estructalloc(thinggrouppair_t, 1);
               proj->types[0] = entry;
               proj->types[1] = other;
               thinggrouppairs.addObject(proj);
            }
            proj->flags |= group->flags & operationalFlags;
         }
      }
   }
}

//
// Returns that two monsters are allies
//
bool E_ThingPairValid(mobjtype_t t1, mobjtype_t t2, unsigned flags)
{
   if(t2 < t1)
   {
      mobjtype_t aux = t1;
      t1 = t2;
      t2 = aux;
   }
   const thinggrouppair_t *pair = thinggrouppairs.objectForKey((int64_t)t1 | (int64_t)t2 << 32);
   return pair && pair->flags & flags;
}

//
// Process a single thingtype's or thingdelta's pickupeffect
// this cannot be done during first pass thingtype processing.
//
static inline void E_processThingPickup(cfg_t *sec, const char *thingname)
{
   int thingnum = E_ThingNumForName(thingname);
   if(cfg_size(sec, ITEM_TNG_PFX_PICKUPFX) > 0)
      E_processThingPickupEffect(*mobjinfo[thingnum], sec);

   // TODO: Delete this if not needed.
   /*if(cfg_size(sec, ITEM_TNG_PICKUPEFFECT))
   {
      const char *tempstr = cfg_getstr(sec, ITEM_TNG_PICKUPEFFECT);
      if(!(mobjinfo[thingnum]->pickupfx = E_PickupFXForName(tempstr)))
      {
         E_EDFLoggedWarning(2, "Invalid pickupeffect '%s' in %s '%s'\n",
                           tempstr, def ? "thingtype" : "thingdelta", thingname);
      }
   }*/
}

//
// Process pickupeffects within thingtypes.
//
void E_ProcessThingPickups(cfg_t *cfg)
{
   unsigned int i, numthings = cfg_size(cfg, EDF_SEC_THING);;
   for(i = 0; i < numthings; i++)
   {
      cfg_t *thingsec = cfg_getnsec(cfg, EDF_SEC_THING, i);
      const char *name = cfg_title(thingsec);
      E_processThingPickup(thingsec, name);
   }
}

//
// Does processing for thingdelta sections, which allow cascading
// editing of existing things. The thingdelta shares most of its
// fields and processing code with the thingtype section.
//
void E_ProcessThingDeltas(cfg_t *cfg)
{
   int i, numdeltas;

   E_EDFLogPuts("\t* Processing thing deltas\n");

   numdeltas = cfg_size(cfg, EDF_SEC_TNGDELTA);

   E_EDFLogPrintf("\t\t%d thingdelta(s) defined\n", numdeltas);

   for(i = 0; i < numdeltas; i++)
   {
      const char *tempstr;
      int mobjType;
      cfg_t *deltasec = cfg_getnsec(cfg, EDF_SEC_TNGDELTA, i);

      // get thingtype to edit
      if(!cfg_size(deltasec, ITEM_DELTA_NAME))
         E_EDFLoggedErr(2, "E_ProcessThingDeltas: thingdelta requires name field\n");

      tempstr = cfg_getstr(deltasec, ITEM_DELTA_NAME);
      mobjType = E_GetThingNumForName(tempstr);

      E_ProcessThing(mobjType, deltasec, cfg, false);
      E_processThingPickup(deltasec, tempstr);

      E_EDFLogPrintf("\t\tApplied thingdelta #%d to %s(#%d)\n",
                     i, mobjinfo[mobjType]->name, mobjType);
   }
}

//
// Post-processing routine; sets things' unspecified default sprites to the
// sprite in the thing's spawnstate.
//
void E_SetThingDefaultSprites()
{
   for(int i = 0; i < NUMMOBJTYPES; ++i)
   {
      if(mobjinfo[i]->defsprite == -1)
         mobjinfo[i]->defsprite = states[mobjinfo[i]->spawnstate]->sprite;
   }
}

//=============================================================================
//
// State finding routines for thing types
//
// These provide some glue between native and metastate fields.
//

//
// DECORATE state labels that correspond to native states
//
static const char *nativeStateLabels[] =
{
   "Spawn",
   "See",
   "Melee",
   "Missile",
   "Pain",
   "Death",
   "XDeath",
   "Raise",
   "Crash",
   "Active",
   "Inactive"
};

//
// Matching enumeration for above names
//
enum nstatetypes_e
{
   NSTATE_SPAWN,
   NSTATE_SEE,
   NSTATE_MELEE,
   NSTATE_MISSILE,
   NSTATE_PAIN,
   NSTATE_DEATH,
   NSTATE_XDEATH,
   NSTATE_RAISE,
   NSTATE_CRASH,
   NSTATE_ACTIVE,
   NSTATE_INACTIVE
};

#define NUMNATIVESTATES earrlen(nativeStateLabels)

//
// Returns a pointer to an mobjinfo's native state field if the given name
// is a match for that field's corresponding DECORATE label. Returns null
// if the name is not a match for a native state field.
//
int *E_GetNativeStateLoc(mobjinfo_t *mi, const char *label)
{
   int nativenum = E_StrToNumLinear(nativeStateLabels, NUMNATIVESTATES, label);
   int *ret = nullptr;

   switch(nativenum)
   {
   case NSTATE_SPAWN:    ret = &mi->spawnstate;    break;
   case NSTATE_SEE:      ret = &mi->seestate;      break;
   case NSTATE_MELEE:    ret = &mi->meleestate;    break;
   case NSTATE_MISSILE:  ret = &mi->missilestate;  break;
   case NSTATE_PAIN:     ret = &mi->painstate;     break;
   case NSTATE_DEATH:    ret = &mi->deathstate;    break;
   case NSTATE_XDEATH:   ret = &mi->xdeathstate;   break;
   case NSTATE_RAISE:    ret = &mi->raisestate;    break;
   case NSTATE_CRASH:    ret = &mi->crashstate;    break;
   case NSTATE_ACTIVE:   ret = &mi->activestate;   break;
   case NSTATE_INACTIVE: ret = &mi->inactivestate; break;
   default:
      break;
   }

   return ret;
}

//
// Retrieves any state for an mobjinfo, either native or metastate.
// Returns null if no such state can be found. Note that the null state is
// not considered a valid state.
//
state_t *E_GetStateForMobjInfo(mobjinfo_t *mi, const char *label)
{
   MetaState *ms;
   state_t *ret = nullptr;
   int *nativefield = nullptr;

   // check metastates
   if((ms = E_GetMetaState(mi, label)))
      ret = ms->state;
   else if((nativefield = E_GetNativeStateLoc(mi, label)))
   {
      // only if not S_NULL
      if(*nativefield != NullStateNum)
         ret = states[*nativefield];
   }

   return ret;
}

//
// Convenience routine to call the above given an Mobj.
//
state_t *E_GetStateForMobj(Mobj *mo, const char *label)
{
   return E_GetStateForMobjInfo(mo->info, label);
}

// EOF

