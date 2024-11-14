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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//--------------------------------------------------------------------------
//
// DESCRIPTION:  
//   New dynamic TerrainTypes system. Inspired heavily by zdoom, but
//   all-original code.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#define NEED_EDF_DEFINITIONS

#include "Confuse/confuse.h"
#include "e_edf.h"
#include "e_lib.h"
#include "e_mod.h"
#include "e_things.h"
#include "e_ttypes.h"

#include "d_gi.h"
#include "d_io.h"
#include "d_dehtbl.h"
#include "d_mod.h"
#include "doomstat.h"
#include "i_system.h"
#include "m_random.h"
#include "p_enemy.h"
#include "p_mobj.h"
#include "p_partcl.h"
#include "p_portal.h"
#include "p_portalcross.h"
#include "p_tick.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_portal.h"
#include "r_state.h"
#include "s_sound.h"
#include "w_wad.h"


//
// Static Variables
//

// Splashes
constexpr int NUMSPLASHCHAINS = 37;
static ETerrainSplash *SplashChains[NUMSPLASHCHAINS];

// Terrain
constexpr int NUMTERRAINCHAINS = 37;
static ETerrain *TerrainChains[NUMTERRAINCHAINS];

// Floors
constexpr int NUMFLOORCHAINS = 37;
static EFloor *FloorChains[NUMFLOORCHAINS];

static int numsplashes;
static int numterrains;
static int numfloors;

//
// libConfuse Stuff
//

// Splash Keywords
constexpr const char ITEM_SPLASH_SMALLCLASS[] = "smallclass";
constexpr const char ITEM_SPLASH_SMALLCLIP[]  = "smallclip";
constexpr const char ITEM_SPLASH_SMALLSOUND[] = "smallsound";
constexpr const char ITEM_SPLASH_BASECLASS[]  = "baseclass";
constexpr const char ITEM_SPLASH_CHUNKCLASS[] = "chunkclass";
constexpr const char ITEM_SPLASH_XVELSHIFT[]  = "chunkxvelshift";
constexpr const char ITEM_SPLASH_YVELSHIFT[]  = "chunkyvelshift";
constexpr const char ITEM_SPLASH_ZVELSHIFT[]  = "chunkzvelshift";
constexpr const char ITEM_SPLASH_BASEZVEL[]   = "chunkbasezvel";
constexpr const char ITEM_SPLASH_SOUND[]      = "sound";

constexpr const char ITEM_SPLASHDELTA_NAME[] = "name";

// Splash Options
#define COMMON_SPLASH_OPTIONS                                  \
      CFG_STR(ITEM_SPLASH_SMALLCLASS, "",        CFGF_NONE),   \
      CFG_INT(ITEM_SPLASH_SMALLCLIP,  0,         CFGF_NONE),   \
      CFG_STR(ITEM_SPLASH_SMALLSOUND, "none",    CFGF_NONE),   \
      CFG_STR(ITEM_SPLASH_BASECLASS,  "",        CFGF_NONE),   \
      CFG_STR(ITEM_SPLASH_CHUNKCLASS, "",        CFGF_NONE),   \
      CFG_INT(ITEM_SPLASH_XVELSHIFT,  -1,        CFGF_NONE),   \
      CFG_INT(ITEM_SPLASH_YVELSHIFT,  -1,        CFGF_NONE),   \
      CFG_INT(ITEM_SPLASH_ZVELSHIFT,  -1,        CFGF_NONE),   \
      CFG_INT(ITEM_SPLASH_BASEZVEL,   0,         CFGF_NONE),   \
      CFG_STR(ITEM_SPLASH_SOUND,      "none",    CFGF_NONE),

cfg_opt_t edf_splash_opts[] =
{
   COMMON_SPLASH_OPTIONS
   CFG_END()
};

cfg_opt_t edf_spldelta_opts[] =
{
   CFG_STR(ITEM_SPLASHDELTA_NAME, nullptr, CFGF_NONE),
   COMMON_SPLASH_OPTIONS
   CFG_END()
};

// Terrain Keywords
constexpr const char ITEM_TERRAIN_SPLASH[]   = "splash";
constexpr const char ITEM_TERRAIN_DMGAMT[]   = "damageamount";
constexpr const char ITEM_TERRAIN_DMGTYPE[]  = "damagetype";
constexpr const char ITEM_TERRAIN_DMGMASK[]  = "damagetimemask";
constexpr const char ITEM_TERRAIN_FOOTCLIP[] = "footclip";
constexpr const char ITEM_TERRAIN_LIQUID[]   = "liquid";
constexpr const char ITEM_TERRAIN_SPALERT[]  = "splashalert";
constexpr const char ITEM_TERRAIN_USECOLS[]  = "useptclcolors";
constexpr const char ITEM_TERRAIN_COL1[]     = "ptclcolor1";
constexpr const char ITEM_TERRAIN_COL2[]     = "ptclcolor2";
constexpr const char ITEM_TERRAIN_MINVER[]   = "minversion";

constexpr const char ITEM_TERDELTA_NAME[] = "name";

// Terrain Options
cfg_opt_t edf_terrn_opts[] =
{
   CFG_STR(ITEM_TERRAIN_SPLASH,   "",        CFGF_NONE),
   CFG_INT(ITEM_TERRAIN_DMGAMT,   0,         CFGF_NONE),
   CFG_STR(ITEM_TERRAIN_DMGTYPE,  "UNKNOWN", CFGF_NONE),
   CFG_INT(ITEM_TERRAIN_DMGMASK,  0,         CFGF_NONE),
   CFG_INT(ITEM_TERRAIN_FOOTCLIP, 0,         CFGF_NONE),
   CFG_BOOL(ITEM_TERRAIN_LIQUID,  false,     CFGF_NONE),
   CFG_BOOL(ITEM_TERRAIN_SPALERT, false,     CFGF_NONE),
   CFG_BOOL(ITEM_TERRAIN_USECOLS, false,     CFGF_NONE),
   CFG_INT(ITEM_TERRAIN_MINVER,   0,         CFGF_NONE),
   
   CFG_INT_CB(ITEM_TERRAIN_COL1,  0,         CFGF_NONE, E_ColorStrCB),
   CFG_INT_CB(ITEM_TERRAIN_COL2,  0,         CFGF_NONE, E_ColorStrCB),
   
   CFG_END()
};

cfg_opt_t edf_terdelta_opts[] =
{
   CFG_STR(ITEM_TERDELTA_NAME,    nullptr,   CFGF_NONE),

   CFG_STR(ITEM_TERRAIN_SPLASH,   "",        CFGF_NONE),
   CFG_INT(ITEM_TERRAIN_DMGAMT,   0,         CFGF_NONE),
   CFG_STR(ITEM_TERRAIN_DMGTYPE,  "UNKNOWN", CFGF_NONE),
   CFG_INT(ITEM_TERRAIN_DMGMASK,  0,         CFGF_NONE),
   CFG_INT(ITEM_TERRAIN_FOOTCLIP, 0,         CFGF_NONE),
   CFG_BOOL(ITEM_TERRAIN_LIQUID,  false,     CFGF_NONE),
   CFG_BOOL(ITEM_TERRAIN_SPALERT, false,     CFGF_NONE),
   CFG_BOOL(ITEM_TERRAIN_USECOLS, false,     CFGF_NONE),
   CFG_INT(ITEM_TERRAIN_MINVER,   0,         CFGF_NONE),
   
   CFG_INT_CB(ITEM_TERRAIN_COL1,  0,         CFGF_NONE, E_ColorStrCB),
   CFG_INT_CB(ITEM_TERRAIN_COL2,  0,         CFGF_NONE, E_ColorStrCB),
   
   CFG_END()
};

// Floor Keywords
constexpr const char ITEM_FLOOR_FLAT[]    = "flat";
constexpr const char ITEM_FLOOR_TERRAIN[] = "terrain";

// Floor Options
cfg_opt_t edf_floor_opts[] =
{
   CFG_STR(ITEM_FLOOR_FLAT,    "none",   CFGF_NONE),
   CFG_STR(ITEM_FLOOR_TERRAIN, "Solid",  CFGF_NONE),
   CFG_END()
};

//
// E_SplashForName
//
// Returns a terrain splash object for a name.
// Returns nullptr if no such splash exists.
//
static ETerrainSplash *E_SplashForName(const char *name)
{
   int key = D_HashTableKey(name) % NUMSPLASHCHAINS;
   ETerrainSplash *splash = SplashChains[key];

   while(splash && strcasecmp(splash->name, name))
      splash = splash->next;

   return splash;
}

//
// E_AddSplashToHash
//
// Adds a terrain splash object to the hash table.
//
static void E_AddSplashToHash(ETerrainSplash *splash)
{
   int key = D_HashTableKey(splash->name) % NUMSPLASHCHAINS;

   splash->next = SplashChains[key];
   SplashChains[key] = splash;

   // keep track of how many there are
   ++numsplashes;
}

//
// E_ProcessSplash
//
// Processes a single splash object from the section data held
// in the cfg parameter. If a splash object already exists by
// the mnemonic, that splash will be edited instead of having a
// new one created. This will allow terrain lumps to have additive
// behavior over EDF.
//
static void E_ProcessSplash(cfg_t *const cfg, const bool def)
{
   const char *tempstr;
   ETerrainSplash *newSplash;
   bool newsp = false;

   const auto IS_SET = [cfg, def](const char *const name) -> bool {
      return def || cfg_size(cfg, (name)) > 0;
   };

   // init name and add to hash table
   if(def)
   {
      // definition:
      tempstr = cfg_title(cfg);

      // If one already exists, modify it. Otherwise, allocate a new
      // splash and add it to the splash hash table.
      if(!(newSplash = E_SplashForName(tempstr)))
      {
         newSplash = estructalloc(ETerrainSplash, 1);
         if(strlen(tempstr) >= sizeof(newSplash->name))
         {
            E_EDFLoggedErr(3, "E_ProcessSplash: invalid splash mnemonic '%s'\n",
                           tempstr);
         }
         strncpy(newSplash->name, tempstr, sizeof(newSplash->name));
         E_AddSplashToHash(newSplash);
         newsp = true;
      }
   }
   else
   {
      // delta:
      tempstr = cfg_getstr(cfg, ITEM_SPLASHDELTA_NAME);
      if(!tempstr)
         E_EDFLoggedErr(3, "E_ProcessSplash: splash delta requires name field!\n");

      if(!(newSplash = E_SplashForName(tempstr)))
      {
         E_EDFLoggedWarning(3, "Warning: splash '%s' doesn't exist\n", tempstr);
         return;
      }
   }

   // process smallclass
   if(IS_SET(ITEM_SPLASH_SMALLCLASS))
   {
      tempstr = cfg_getstr(cfg, ITEM_SPLASH_SMALLCLASS);
      newSplash->smallclass = E_ThingNumForName(tempstr);
   }

   // process smallclip
   if(IS_SET(ITEM_SPLASH_SMALLCLIP))
      newSplash->smallclip = cfg_getint(cfg, ITEM_SPLASH_SMALLCLIP) * FRACUNIT;

   // process smallsound
   if(IS_SET(ITEM_SPLASH_SMALLSOUND))
   {
      tempstr = cfg_getstr(cfg, ITEM_SPLASH_SMALLSOUND);
      if(strlen(tempstr) >= sizeof(newSplash->smallsound))
      {
         E_EDFLoggedErr(3, "E_ProcessSplash: invalid sound mnemonic '%s'\n",
                        tempstr);
      }
      strncpy(newSplash->smallsound, tempstr, sizeof(newSplash->smallsound));
   }

   // process baseclass
   if(IS_SET(ITEM_SPLASH_BASECLASS))
   {
      tempstr = cfg_getstr(cfg, ITEM_SPLASH_BASECLASS);
      newSplash->baseclass = E_ThingNumForName(tempstr);
   }
   
   // process chunkclass
   if(IS_SET(ITEM_SPLASH_CHUNKCLASS))
   {
      tempstr = cfg_getstr(cfg, ITEM_SPLASH_CHUNKCLASS);
      newSplash->chunkclass = E_ThingNumForName(tempstr);
   }
   
   // process chunkxvelshift, yvelshift, zvelshift
   if(IS_SET(ITEM_SPLASH_XVELSHIFT))
      newSplash->chunkxvelshift = cfg_getint(cfg, ITEM_SPLASH_XVELSHIFT);
   if(IS_SET(ITEM_SPLASH_YVELSHIFT))
      newSplash->chunkyvelshift = cfg_getint(cfg, ITEM_SPLASH_YVELSHIFT);
   if(IS_SET(ITEM_SPLASH_ZVELSHIFT))
      newSplash->chunkzvelshift = cfg_getint(cfg, ITEM_SPLASH_ZVELSHIFT);
  
   // process chunkbasezvel
   if(IS_SET(ITEM_SPLASH_BASEZVEL))
      newSplash->chunkbasezvel = cfg_getint(cfg, ITEM_SPLASH_BASEZVEL) * FRACUNIT;
   
   // process sound
   if(IS_SET(ITEM_SPLASH_SOUND))
   {
      tempstr = cfg_getstr(cfg, ITEM_SPLASH_SOUND);
      if(strlen(tempstr) >= sizeof(newSplash->sound))
      {
         E_EDFLoggedErr(3, "E_ProcessSplash: invalid sound mnemonic '%s'\n",
                        tempstr);
      }
      strncpy(newSplash->sound, tempstr, sizeof(newSplash->sound));
   }

   E_EDFLogPrintf("\t\t\t%s splash '%s'\n", 
                  newsp ? "Finished" : "Modified", 
                  newSplash->name);
}

static void E_ProcessSplashes(cfg_t *cfg)
{
   unsigned int i;
   unsigned int numSplashes = cfg_size(cfg, EDF_SEC_SPLASH);

   E_EDFLogPrintf("\t\t* Processing splashes\n"
                  "\t\t\t%d splash(es) defined\n", numSplashes);

   for(i = 0; i < numSplashes; ++i)
   {
      cfg_t *splashsec = cfg_getnsec(cfg, EDF_SEC_SPLASH, i);
      E_ProcessSplash(splashsec, true);
   }
}

//
// Processes delta for splashes
//
static void E_processSplashDeltas(cfg_t *cfg)
{
   unsigned numSplashDeltas = cfg_size(cfg, EDF_SEC_SPLASHDELTA);

   E_EDFLogPrintf("\t\t* Processing splash deltas\n"
                  "\t\t\t%u splash delta(s) defined\n", numSplashDeltas);

   for(unsigned i = 0; i < numSplashDeltas; ++i)
   {
      cfg_t *splashdeltasec = cfg_getnsec(cfg, EDF_SEC_SPLASHDELTA, i);
      E_ProcessSplash(splashdeltasec, false);
   }
}

//
// E_TerrainForName
//
// Returns a terrain object for a name.
// Returns nullptr if no such terrain exists.
//
ETerrain *E_TerrainForName(const char *name)
{
   int key = D_HashTableKey(name) % NUMTERRAINCHAINS;
   ETerrain *terrain = TerrainChains[key];

   while(terrain && strcasecmp(terrain->name, name))
      terrain = terrain->next;

   return terrain;
}

//
// E_AddTerrainToHash
//
// Adds a terrain object to the hash table.
//
static void E_AddTerrainToHash(ETerrain *terrain)
{
   int key = D_HashTableKey(terrain->name) % NUMTERRAINCHAINS;

   terrain->next = TerrainChains[key];
   TerrainChains[key] = terrain;

   // keep track of how many there are
   ++numterrains;
}

static void E_ProcessTerrain(cfg_t *const cfg, const bool def)
{
   const char *tempstr;
   ETerrain *newTerrain;
   bool newtr = false;

   const auto IS_SET = [cfg, def](const char *const name) -> bool {
      return def || cfg_size(cfg, (name)) > 0;
   };

   // init name and add to hash table
   if(def)
   {
      // definition:
      tempstr = cfg_title(cfg);

      // If one already exists, modify it. Otherwise, allocate a new
      // terrain and add it to the terrain hash table.
      if(!(newTerrain = E_TerrainForName(tempstr)))
      {
         newTerrain = estructalloc(ETerrain, 1);
         if(strlen(tempstr) >= sizeof(newTerrain->name))
         {
            E_EDFLoggedErr(3, "E_ProcessTerrain: invalid terrain mnemonic '%s'\n",
                           tempstr);
         }
         strncpy(newTerrain->name, tempstr, sizeof(newTerrain->name));
         E_AddTerrainToHash(newTerrain);
         newtr = true;
      }
   }
   else
   {
      // delta:
      tempstr = cfg_getstr(cfg, ITEM_TERDELTA_NAME);
      if(!tempstr)
      {
         E_EDFLoggedErr(3, 
            "E_ProcessTerrain: terrain delta requires name field!\n");
      }

      if(!(newTerrain = E_TerrainForName(tempstr)))
      {
         E_EDFLoggedWarning(3, "Warning: terrain '%s' doesn't exist\n",
                            tempstr);
         return;
      }
   }

   // process splash -- may be null
   if(IS_SET(ITEM_TERRAIN_SPLASH))
   {
      tempstr = cfg_getstr(cfg, ITEM_TERRAIN_SPLASH);
      newTerrain->splash = E_SplashForName(tempstr);
   }

   // process damageamount
   if(IS_SET(ITEM_TERRAIN_DMGAMT))
      newTerrain->damageamount = cfg_getint(cfg, ITEM_TERRAIN_DMGAMT);

   // process damagetype
   if(IS_SET(ITEM_TERRAIN_DMGTYPE))
   {
      tempstr = cfg_getstr(cfg, ITEM_TERRAIN_DMGTYPE);
      newTerrain->damagetype = E_DamageTypeForName(tempstr)->num;
   }

   // process damagetimemask
   if(IS_SET(ITEM_TERRAIN_DMGMASK))
      newTerrain->damagetimemask = cfg_getint(cfg, ITEM_TERRAIN_DMGMASK);

   // process footclip
   if(IS_SET(ITEM_TERRAIN_FOOTCLIP))
   {
      newTerrain->footclip = 
         cfg_getint(cfg, ITEM_TERRAIN_FOOTCLIP) * FRACUNIT;
   }

   // process liquid
   if(IS_SET(ITEM_TERRAIN_LIQUID))
   {
      newTerrain->liquid = cfg_getbool(cfg, ITEM_TERRAIN_LIQUID);
   }

   // process splashalert
   if(IS_SET(ITEM_TERRAIN_SPALERT))
   {
      newTerrain->splashalert = cfg_getbool(cfg, ITEM_TERRAIN_SPALERT);
   }

   // process usepcolors
   if(IS_SET(ITEM_TERRAIN_USECOLS))
   {
      newTerrain->usepcolors = cfg_getbool(cfg, ITEM_TERRAIN_USECOLS);
   }

   // process particle colors
   if(IS_SET(ITEM_TERRAIN_COL1))
      newTerrain->pcolor_1 = (byte)cfg_getint(cfg, ITEM_TERRAIN_COL1);

   if(IS_SET(ITEM_TERRAIN_COL2))
      newTerrain->pcolor_2 = (byte)cfg_getint(cfg, ITEM_TERRAIN_COL2);

   if(IS_SET(ITEM_TERRAIN_MINVER))
      newTerrain->minversion = cfg_getint(cfg, ITEM_TERRAIN_MINVER);

   // games other than DOOM have always had terrain types
   if(GameModeInfo->type != Game_DOOM)
      newTerrain->minversion = 0;

   if(def)
   {
      E_EDFLogPrintf("\t\t\t%s terrain '%s'\n", 
                     newtr ? "Finished" : "Modified",
                     newTerrain->name);
   }
   else
      E_EDFLogPrintf("\t\t\tApplied terraindelta to terrain '%s'\n", newTerrain->name);
}

// The 'Solid' terrain object.
static ETerrain solid;

//
// E_AddSolidTerrain
//
// Adds the default 'Solid' TerrainType to the terrain hash. This
// is done before user TerrainTypes are added, so if the user
// creates a type named Solid, it will override this one by modifying
// its properties. This default type has no special properties.
//
static void E_AddSolidTerrain(void)
{
   static bool solidinit;

   if(!solidinit)
   {
      E_EDFLogPuts("\t\t\tCreating Solid terrain...\n");
      strncpy(solid.name, "Solid", sizeof(solid.name));
      E_AddTerrainToHash(&solid);
      solidinit = true;
      --numterrains; // do not count the solid terrain 
   }
}

static void E_ProcessTerrains(cfg_t *cfg)
{
   unsigned int i;
   unsigned int numTerrains = cfg_size(cfg, EDF_SEC_TERRAIN);

   E_EDFLogPrintf("\t\t* Processing terrain\n"
                  "\t\t\t%d terrain(s) defined\n", numTerrains);

   E_AddSolidTerrain();

   for(i = 0; i < numTerrains; ++i)
   {
      cfg_t *terrainsec = cfg_getnsec(cfg, EDF_SEC_TERRAIN, i);
      E_ProcessTerrain(terrainsec, true);
   }
}

static void E_ProcessTerrainDeltas(cfg_t *cfg)
{
   unsigned int i;
   unsigned int numTerrains = cfg_size(cfg, EDF_SEC_TERDELTA);

   E_EDFLogPrintf("\t\t* Processing terrain deltas\n"
                  "\t\t\t%d terrain delta(s) defined\n", numTerrains);

   for(i = 0; i < numTerrains; ++i)
   {
      cfg_t *terrainsec = cfg_getnsec(cfg, EDF_SEC_TERDELTA, i);
      E_ProcessTerrain(terrainsec, false);
   }
}

//
// E_FloorForName
//
// Returns a floor object for a flat name.
// Returns nullptr if no such floor exists.
//
static EFloor *E_FloorForName(const char *name)
{
   int key = D_HashTableKey(name) % NUMFLOORCHAINS;
   EFloor *floor = FloorChains[key];

   while(floor && strcasecmp(floor->name, name))
      floor = floor->next;

   return floor;
}

//
// E_AddFloorToHash
//
// Adds a floor object to the hash table.
//
static void E_AddFloorToHash(EFloor *floor)
{
   int key = D_HashTableKey(floor->name) % NUMFLOORCHAINS;

   floor->next = FloorChains[key];
   FloorChains[key] = floor;

   // keep track of how many there are
   ++numfloors;
}

//
// E_ProcessFloor
//
// Creates or modifies a floor object.
//
static void E_ProcessFloor(cfg_t *cfg)
{
   const char *tempstr;
   EFloor *newFloor;

   // init flat name and add to hash table
   tempstr = cfg_getstr(cfg, ITEM_FLOOR_FLAT);
   if(strlen(tempstr) > 8)
   {
      E_EDFLoggedErr(3, "E_ProcessFloor: invalid flat name '%s'\n",
                     tempstr);
   }

   // If one already exists, modify it. Otherwise, allocate a new
   // terrain and add it to the terrain hash table.
   if(!(newFloor = E_FloorForName(tempstr)))
   {
      newFloor = ecalloc(EFloor *, 1, sizeof(EFloor));
      strncpy(newFloor->name, tempstr, 9);
      E_AddFloorToHash(newFloor);
   }

   // process terrain
   tempstr = cfg_getstr(cfg, ITEM_FLOOR_TERRAIN);
   if(!(newFloor->terrain = E_TerrainForName(tempstr)))
   {
      E_EDFLoggedWarning(3, "Warning: Flat '%s' uses bad terrain '%s'\n",
                         newFloor->name, tempstr);
      newFloor->terrain = &solid;
   }

   E_EDFLogPrintf("\t\t\tFlat '%s' = Terrain '%s'\n",
                  newFloor->name, newFloor->terrain->name);
}

//
// E_ProcessFloors
//
// Processes all floor sections in the provided cfg object. Also
// parses the deprecated binary TERTYPES lump to add definitions for
// legacy projects.
//
static void E_ProcessFloors(cfg_t *cfg)
{
   unsigned int i;
   unsigned int numFloors = cfg_size(cfg, EDF_SEC_FLOOR);

   E_EDFLogPrintf("\t\t* Processing floors\n"
                  "\t\t\t%d floor(s) defined\n", numFloors);

   for(i = 0; i < numFloors; ++i)
   {
      cfg_t *floorsec = cfg_getnsec(cfg, EDF_SEC_FLOOR, i);
      E_ProcessFloor(floorsec);
   }
}

//
// E_ProcessTerrainTypes
//
// Performs all TerrainTypes processing for EDF.
//
void E_ProcessTerrainTypes(cfg_t *cfg)
{
   E_EDFLogPuts("\t* Processing TerrainTypes\n");

   // First, process splashes and their deltas
   E_ProcessSplashes(cfg);
   E_processSplashDeltas(cfg);

   // Second, process terrains
   E_ProcessTerrains(cfg);

   // Third, process terrain deltas
   E_ProcessTerrainDeltas(cfg);

   // Last, process floors
   E_ProcessFloors(cfg);
}

// TerrainTypes lookup array
static ETerrain **TerrainTypes = nullptr;

//
// E_InitTerrainTypes
//
void E_InitTerrainTypes(void)
{
   int numf;

   // if TerrainTypes already exists, free it
   if(TerrainTypes)
      efree(TerrainTypes);

   // allocate the TerrainTypes lookup
   numf = (texturecount + 1);
   TerrainTypes = ecalloc(ETerrain **, numf, sizeof(ETerrain*));

   // initialize all flats to Solid terrain
   for(int i = 0; i < numf; ++i)
      TerrainTypes[i] = &solid;

   // run down each chain of the Floor hash table and assign each
   // Floor object to the proper TerrainType
   for(EFloor *floor : FloorChains)
   {
      while(floor)
      {
         int tnum = R_CheckForFlat(floor->name);

         if(tnum != -1)
            TerrainTypes[tnum] = floor->terrain;

         floor = floor->next;
      }
   }
}

//
// E_GetThingFloorType
//
// Note: this returns the floor type of the thing's subsector
// floorpic, not necessarily the floor the thing is standing on.
// haleyjd 10/16/10: Except that's never been sufficient. So in 
// newer versions return the appropriate floor's type.
//
ETerrain *E_GetThingFloorType(const Mobj *thing)
{
   ETerrain *terrain = nullptr;
   
   if(full_demo_version >= make_full_version(339, 21))
   {
      const msecnode_t *m = nullptr;

      // determine what touched sector the thing is standing on
      fixed_t z = thing->zref.floor;
      for(m = thing->touching_sectorlist; m; m = m->m_tnext)
      {
         // Handle sloped floors a bit differently, using the designated floorsector
         if(m->m_sector->srf.floor.slope && m->m_sector == thing->zref.sector.floor)
            break;
         if(!m->m_sector->srf.floor.slope && z == m->m_sector->srf.floor.height)
            break;
      }

      // if found one that's valid, use that terrain
      if(m)
      {
         if(!(terrain = m->m_sector->srf.floor.terrain))
            terrain = TerrainTypes[m->m_sector->srf.floor.pic];
      }
      else
         terrain = &solid;
   }

   if(!terrain) 
   {
      if(!(terrain = thing->subsector->sector->srf.floor.terrain))
         terrain = TerrainTypes[thing->subsector->sector->srf.floor.pic];
   }

   I_Assert(!!terrain, "Unexpected null terrain\n");
   
   if(demo_version < terrain->minversion || getComp(comp_terrain))
      terrain = &solid;

   return terrain;
}

//
// E_GetTerrainTypeForPt
//
// haleyjd 06/21/02: function to get TerrainType from a point
//
ETerrain *E_GetTerrainTypeForPt(fixed_t x, fixed_t y, int position)
{
   subsector_t *subsec = R_PointInSubsector(x, y);
   sector_t *sector = subsec->sector;
   ETerrain *floorterrain = nullptr, *ceilingterrain = nullptr;

   if(!(floorterrain = sector->srf.floor.terrain))
      floorterrain = TerrainTypes[sector->srf.floor.pic];

   if(!(ceilingterrain = sector->srf.ceiling.terrain))
      ceilingterrain = TerrainTypes[sector->srf.ceiling.pic];

   // can retrieve a TerrainType for either the floor or the
   // ceiling
   switch(position)
   {
   case 0:
      return floorterrain;
   case 1:
      return ceilingterrain;
   default:
      return &solid;
   }
}

//
// E_SectorFloorClip
//
// Returns the amount of floorclip a sector should impart upon
// objects standing inside it.
//
fixed_t E_SectorFloorClip(sector_t *sector)
{
   ETerrain *terrain = nullptr;
   
   // override with sector terrain if one is specified
   if(!(terrain = sector->srf.floor.terrain))
      terrain = TerrainTypes[sector->srf.floor.pic];

   return (demo_version >= terrain->minversion) ? terrain->footclip : 0;
}

//
// E_PtclTerrainHit
//
// Executes particle terrain hits.
//
void E_PtclTerrainHit(particle_t *p)
{
   ETerrain *terrain = nullptr;
   ETerrainSplash *splash = nullptr;
   Mobj *mo = nullptr;
   fixed_t x, y, z;
   sector_t *sector = nullptr;

   // particles could never hit terrain before v3.33
   if(demo_version < 333 || getComp(comp_terrain))
      return;

   // no particle hits during netgames or demos;
   // this is necessary because particles are not only client
   // specific, but they also use M_Random
   if(netgame || demoplayback || demorecording)
      return;

   sector = p->subsector->sector;

   // override with sector terrain if one is specified
   if(!(terrain = sector->srf.floor.terrain))
      terrain = TerrainTypes[sector->srf.floor.pic];

   // some terrains didn't exist before a certain version
   if(demo_version < terrain->minversion)
      return;

   if(!(splash = terrain->splash))
      return;

   x = p->x;
   y = p->y;
   z = p->z;

   // low mass splash -- always when possible.
   if(splash->smallclass != -1)
   {
      mo = P_SpawnMobj(x, y, z, splash->smallclass);
      mo->floorclip += splash->smallclip;      
   }
   else if(splash->baseclass != -1)
   {
      // spawn only a splash base otherwise
      mo = P_SpawnMobj(x, y, z, splash->baseclass);
   }
   
   if(mo)
      S_StartSoundName(mo, splash->smallsound);
}

//
// E_TerrainHit
//
// Executes mobj terrain effects.
// ioanch 20160116: also use "sector" to change the group ID if needed
//
static void E_TerrainHit(const ETerrain *terrain, Mobj *thing, fixed_t z, const sector_t *sector, 
                         fixed_t splashAlpha)
{
   ETerrainSplash *splash = terrain->splash;
   Mobj *mo = nullptr;
   bool lowmass = (thing->info->mass < 10);   

   if(!splash)
      return;

   // ioanch 20160116: portal aware
   const linkoffset_t *link = P_GetLinkOffset(thing->groupid, sector->groupid);
   fixed_t tx = thing->x + link->x;
   fixed_t ty = thing->y + link->y;

   auto reduceAlpha = [&mo, splashAlpha]()
      {
         if(mo->translucency > splashAlpha)
            mo->translucency = splashAlpha;
      };

   // low mass splash?
   // note: small splash didn't exist before version 3.33
   if(demo_version >= 333 && lowmass && splash->smallclass != -1)
   {
      mo = P_SpawnMobj(tx, ty, z, splash->smallclass);
      mo->floorclip += splash->smallclip;
      reduceAlpha();
   }
   else
   {
      if(splash->baseclass != -1)
      {
         mo = P_SpawnMobj(tx, ty, z, splash->baseclass);
         reduceAlpha();
      }

      if(splash->chunkclass != -1)
      {
         mo = P_SpawnMobj(tx, ty, z, splash->chunkclass);
         reduceAlpha();
         P_SetTarget<Mobj>(&mo->target, thing);
         
         if(splash->chunkxvelshift != -1)
            mo->momx = P_SubRandom(pr_splash) << splash->chunkxvelshift;
         if(splash->chunkyvelshift != -1)
            mo->momy = P_SubRandom(pr_splash) << splash->chunkyvelshift;
         mo->momz = splash->chunkbasezvel;
         if(splash->chunkzvelshift != -1)
            mo->momz += P_Random(pr_splash) << splash->chunkzvelshift;
      }

      // some terrains may awaken enemies when hit
      if(!lowmass && terrain->splashalert && thing->player)
         P_NoiseAlert(thing, thing);
   }

   // make a sound
   // use the splash object as the origin if possible, 
   // else the thing that hit the terrain
   S_StartSoundName(mo ? mo : thing, 
                    lowmass ? splash->smallsound : splash->sound);
}

//
// Get the information for the thing's floor terrain. Also gets the resulting z value
//
static const ETerrain &E_getFloorTerrain(const Mobj &thing, const sector_t &sector, v2fixed_t pos,
                                         fixed_t *z, fixed_t *splashAlpha)
{
   // override with sector terrain if one is specified
   const ETerrain *terrain = sector.srf.floor.terrain;
   if(!terrain)
      terrain = TerrainTypes[sector.srf.floor.pic];

   // no TerrainTypes in old demos or if comp enabled
   if(demo_version < terrain->minversion || getComp(comp_terrain))
      terrain = &solid;

   // some things don't cause splashes
   if(thing.flags2 & MF2_NOSPLASH || thing.flags2 & MF2_FLOATBOB)
      terrain = &solid;

   if(z)
   {
      if(sector.heightsec != -1)
         *z = sectors[sector.heightsec].srf.floor.getZAt(pos);
      else if(P_IsLiquidOverlaylinkedPortal(sector.srf.floor))
         *z = P_PortalZ(sector.srf.floor, pos);
      else 
         *z = sector.srf.floor.getZAt(pos);
   }
   if(splashAlpha)
   {
      // Take the alpha from the overlay, if available
      if(sector.heightsec == -1 && P_IsLiquidOverlaylinkedPortal(sector.srf.floor))
         *splashAlpha = ((sector.srf.floor.pflags >> PO_OPACITYSHIFT & 0xff) << 8) + 255;
      else
         *splashAlpha = FRACUNIT;
   }
   return *terrain;
}

//
// E_HitWater
//
// Called when a thing hits a floor or passes a deep water plane.
//
bool E_HitWater(Mobj *thing, const sector_t *sector)
{
   fixed_t z;
   fixed_t splashAlpha;
   const ETerrain &terrain = E_getFloorTerrain(*thing, *sector, {thing->x, thing->y}, &z,
                                               &splashAlpha);

   // ioanch 20160116: also use "sector" as a parameter in case it's in another group
   E_TerrainHit(&terrain, thing, z, sector, splashAlpha);

   return terrain.liquid;
}

//
// If called from an explosion, hit ground water if close enough
//
void E_ExplosionHitWater(Mobj *thing, int damage)
{
   // Vanilla Heretic didn't really imply impact splash, only explosion, so avoid creating 2
   // splashes at once in vHeretic
   if(vanilla_heretic && thing->flags & MF_MISSILE && !(thing->flags2 & MF2_NOSPLASH) &&
      E_WouldHitFloorWater(*thing))
   {
      return;
   }
   // VANILLA_HERETIC: explosion infinite height

   bool hit = false;
   if(vanilla_heretic)
      hit = true;

   const sector_t *overlaySplashSector = nullptr;

   if(!hit)
   {
      fixed_t refheight;
      const sector_t &sector = *thing->subsector->sector;
      if(sector.heightsec != -1)
         refheight = sectors[sector.heightsec].srf.floor.getZAt(thing->x, thing->y);
      else
      {
         v2fixed_t pos = { thing->x, thing->y };
         bool foundLiquidOverlay = false;
         for(int prot = 0; prot < SECTOR_PORTAL_LOOP_PROTECTION; ++prot)
         {
            const sector_t *currentSector = R_PointInSubsector(pos)->sector;
            if((currentSector->srf.floor.pflags & (PS_PASSABLE | PS_OVERLAY)) == PS_PASSABLE)
            {
               pos.x += currentSector->srf.floor.portal->data.link.delta.x;
               pos.y += currentSector->srf.floor.portal->data.link.delta.y;
            }
            else if(P_IsLiquidOverlaylinkedPortal(currentSector->srf.floor))
            {
               refheight = P_PortalZ(currentSector->srf.floor, pos);
               foundLiquidOverlay = true;
               overlaySplashSector = currentSector;
            }
            else
               break;
         }
         if(!foundLiquidOverlay)
            refheight = thing->zref.secfloor;
      }

      if(thing->z <= refheight + damage * FRACUNIT)
         hit = true;
   }
   if(hit)
   {
      E_HitWater(thing, overlaySplashSector ? overlaySplashSector :
                 P_ExtremeSectorAtPoint(thing, surf_floor));
   }
}

//
// Check if thing is standing on given sector, compatible with slopes and non-slopes
// NOTE: this checks _exact_ floor equality, not <=
//
bool E_StandingOnExactly(const sector_t &sector, const Mobj &thing)
{
   if(!sector.srf.floor.slope && thing.z == sector.srf.floor.height)
      return true;
   // Different handling for sloped floors
   if(sector.srf.floor.slope && thing.z <= thing.zref.floor && thing.zref.sector.floor == &sector)
      return true;
   return false;
}

//
// E_HitFloor
//
// Called when a thing hits a floor.
//
bool E_HitFloor(Mobj *thing)
{
   msecnode_t  *m = nullptr;

   // determine what touched sector the thing is standing on
   for(m = thing->touching_sectorlist; m; m = m->m_tnext)
      if(E_StandingOnExactly(*m->m_sector, *thing))
         break;

   // not on a floor or dealing with deep water, return solid
   // deep water splashes are handled in P_MobjThinker now
   if(!m || m->m_sector->heightsec != -1)         
      return false;

   return E_HitWater(thing, m->m_sector);
}

//
// Checks if E_HitFloor would happen without actually triggering a splash. Needed for certain things
// whose behavior changes when hitting liquids
//
bool E_WouldHitFloorWater(const Mobj &thing)
{
   const msecnode_t *m;
   for(m = thing.touching_sectorlist; m; m = m->m_tnext)
      if(E_StandingOnExactly(*m->m_sector, thing))
         break;

   // NOTE: same conditions as E_HitFloor
   if(!m || m->m_sector->heightsec != -1)
      return false;

   const sector_t &sector = *m->m_sector;
   const ETerrain &terrain = E_getFloorTerrain(thing, sector, v2fixed_t(), nullptr, nullptr);
   return terrain.liquid;
}

//
// Check if under a BOOM liquid fake floor that would splash liquid. Needed for certain Heretic 
// things.
//
bool E_UnderBoomLiquidFakeFloor(const Mobj &thing)
{
   const sector_t &sector = *thing.subsector->sector;
   if(sector.heightsec == -1)
      return false;

   fixed_t z;
   const ETerrain &terrain = E_getFloorTerrain(thing, sector, { thing.x, thing.y }, &z, nullptr);
   return z > thing.z && terrain.liquid;
}

// EOF

