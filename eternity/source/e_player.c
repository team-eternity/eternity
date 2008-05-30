// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2006 James Haley
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
//----------------------------------------------------------------------------
//
// EDF Player Class Module
//
// By James Haley
//
//----------------------------------------------------------------------------

#define NEED_EDF_DEFINITIONS

#include "z_zone.h"

#include "Confuse/confuse.h"

#include "d_gi.h"
#include "d_io.h"
#include "d_items.h"
#include "p_skin.h"

#include "e_lib.h"
#include "e_edf.h"
#include "e_states.h"
#include "e_things.h"
#include "e_player.h"

//
// Weapon Options
//

#define ITEM_WEAPON_AMMO        "ammo"
#define ITEM_WEAPON_AMMOPERSHOT "ammopershot"
#define ITEM_WEAPON_RECOIL      "recoil"
#define ITEM_WEAPON_UPSTATE     "upstate"
#define ITEM_WEAPON_DOWNSTATE   "downstate"
#define ITEM_WEAPON_READYSTATE  "readystate"
#define ITEM_WEAPON_ATTACKSTATE "attackstate"
#define ITEM_WEAPON_FLASHSTATE  "flashstate"
#define ITEM_WEAPON_UPSOUND     "upsound"
#define ITEM_WEAPON_IDLESOUND   "idlesound"
#define ITEM_WEAPON_FLAGS       "flags"

cfg_opt_t edf_weapon_opts[] =
{
   CFG_STR(ITEM_WEAPON_AMMO,        NULL,       CFGF_NONE), // TODO: default ammo type?
   CFG_INT(ITEM_WEAPON_AMMOPERSHOT, 1,          CFGF_NONE),
   CFG_INT(ITEM_WEAPON_RECOIL,      0,          CFGF_NONE),
   CFG_STR(ITEM_WEAPON_UPSTATE,     "S_NULL",   CFGF_NONE),
   CFG_STR(ITEM_WEAPON_DOWNSTATE,   "S_NULL",   CFGF_NONE),
   CFG_STR(ITEM_WEAPON_READYSTATE,  "S_NULL",   CFGF_NONE),
   CFG_STR(ITEM_WEAPON_ATTACKSTATE, "S_NULL",   CFGF_NONE),
   CFG_STR(ITEM_WEAPON_FLASHSTATE,  "S_NULL",   CFGF_NONE),
   CFG_STR(ITEM_WEAPON_UPSOUND,     "none",     CFGF_NONE),
   CFG_STR(ITEM_WEAPON_IDLESOUND,   "none",     CFGF_NONE),
   CFG_STR(ITEM_WEAPON_FLAGS,       "",         CFGF_NONE),
   
   // TODO: more stuff.

   CFG_END()
};

// haleyjd: weapon flags!
static dehflags_t weapon_flags[] =
{
   { "NOTHRUST",     WPF_NOTHRUST     },
   { "NOHITGHOSTS",  WPF_NOHITGHOSTS  },
   { "NOTSHAREWARE", WPF_NOTSHAREWARE },
   { "COMMERCIAL",   WPF_COMMERCIAL   },
   { "SILENCER",     WPF_SILENCER     },
   { "SILENT",       WPF_SILENT       },
   { "NOAUTOFIRE",   WPF_NOAUTOFIRE   },
   { "FLEEMELEE",    WPF_FLEEMELEE    },
   { NULL,           0 }
};

static dehflagset_t weapon_flagset =
{
   weapon_flags, // flaglist
   0,            // mode: single flags word
};


//
// Player Class and Skin Options
//

#define ITEM_SKINSND_PAIN    "pain"
#define ITEM_SKINSND_DIEHI   "diehi"
#define ITEM_SKINSND_OOF     "oof"
#define ITEM_SKINSND_GIB     "gib"
#define ITEM_SKINSND_PUNCH   "punch"
#define ITEM_SKINSND_RADIO   "radio"
#define ITEM_SKINSND_DIE     "die"
#define ITEM_SKINSND_FALL    "fall"
#define ITEM_SKINSND_FEET    "feet"
#define ITEM_SKINSND_FALLHIT "fallhit"
#define ITEM_SKINSND_PLWDTH  "plwdth"
#define ITEM_SKINSND_NOWAY   "noway"

static cfg_opt_t pc_skin_sound_opts[] =
{
   CFG_STR(ITEM_SKINSND_PAIN,    "plpain", CFGF_NONE),
   CFG_STR(ITEM_SKINSND_DIEHI,   "pdiehi", CFGF_NONE),
   CFG_STR(ITEM_SKINSND_OOF,     "oof",    CFGF_NONE),
   CFG_STR(ITEM_SKINSND_GIB,     "slop",   CFGF_NONE),
   CFG_STR(ITEM_SKINSND_PUNCH,   "punch",  CFGF_NONE),
   CFG_STR(ITEM_SKINSND_RADIO,   "radio",  CFGF_NONE),
   CFG_STR(ITEM_SKINSND_DIE,     "pldeth", CFGF_NONE),
   CFG_STR(ITEM_SKINSND_FALL,    "plfall", CFGF_NONE),
   CFG_STR(ITEM_SKINSND_FEET,    "plfeet", CFGF_NONE),
   CFG_STR(ITEM_SKINSND_FALLHIT, "fallht", CFGF_NONE),
   CFG_STR(ITEM_SKINSND_PLWDTH,  "plwdth", CFGF_NONE),
   CFG_STR(ITEM_SKINSND_NOWAY,   "noway",  CFGF_NONE),
   CFG_END()
};

#define ITEM_SKIN_SPRITE  "sprite"
#define ITEM_SKIN_FACES   "faces"
#define ITEM_SKIN_SOUNDS  "sounds"

cfg_opt_t edf_skin_opts[] =
{
   CFG_STR(ITEM_SKIN_SPRITE,  "PLAY",             CFGF_NONE),
   CFG_STR(ITEM_SKIN_FACES,   "STF",              CFGF_NONE),
   CFG_SEC(ITEM_SKIN_SOUNDS,  pc_skin_sound_opts, CFGF_NOCASE),
   CFG_END()
};

#define ITEM_PCLASS_DEFAULT        "default"
#define ITEM_PCLASS_DEFAULTSKIN    "defaultskin"
#define ITEM_PCLASS_THINGTYPE      "thingtype"
#define ITEM_PCLASS_ALTATTACK      "altattackstate"
#define ITEM_PCLASS_SPEEDWALK      "speedwalk"
#define ITEM_PCLASS_SPEEDRUN       "speedrun"
#define ITEM_PCLASS_SPEEDSTRAFE    "speedstrafe"
#define ITEM_PCLASS_SPEEDSTRAFERUN "speedstraferun"
#define ITEM_PCLASS_SPEEDTURN      "speedturn"
#define ITEM_PCLASS_SPEEDTURNFAST  "speedturnfast"
#define ITEM_PCLASS_SPEEDTURNSLOW  "speedturnslow"
#define ITEM_PCLASS_SPEEDLOOKSLOW  "speedlookslow"
#define ITEM_PCLASS_SPEEDLOOKFAST  "speedlookfast"

cfg_opt_t edf_pclass_opts[] =
{
   CFG_STR(ITEM_PCLASS_DEFAULTSKIN, NULL, CFGF_NONE),
   CFG_STR(ITEM_PCLASS_THINGTYPE,   NULL, CFGF_NONE),
   CFG_STR(ITEM_PCLASS_ALTATTACK,   NULL, CFGF_NONE),

   // speeds
   CFG_INT(ITEM_PCLASS_SPEEDWALK,      0x19, CFGF_NONE),
   CFG_INT(ITEM_PCLASS_SPEEDRUN,       0x32, CFGF_NONE),
   CFG_INT(ITEM_PCLASS_SPEEDSTRAFE,    0x18, CFGF_NONE),
   CFG_INT(ITEM_PCLASS_SPEEDSTRAFERUN, 0x28, CFGF_NONE),
   CFG_INT(ITEM_PCLASS_SPEEDTURN,       640, CFGF_NONE),
   CFG_INT(ITEM_PCLASS_SPEEDTURNFAST,  1280, CFGF_NONE),
   CFG_INT(ITEM_PCLASS_SPEEDTURNSLOW,   320, CFGF_NONE),
   CFG_INT(ITEM_PCLASS_SPEEDLOOKSLOW,   450, CFGF_NONE),
   CFG_INT(ITEM_PCLASS_SPEEDLOOKFAST,   512, CFGF_NONE),

   CFG_BOOL(ITEM_PCLASS_DEFAULT, cfg_false, CFGF_NONE),

   CFG_END()
};

//==============================================================================
// 
// Global Variables
//

// EDF Skins: These are skins created by the EDF player class objects. They will
// be added to the main skin list and fully initialized in p_skin.c along with
// other normal skins.

skin_t *edf_skins[NUMEDFSKINCHAINS];

// Player classes

#define NUMEDFPCLASSCHAINS 17

playerclass_t *edf_player_classes[NUMEDFPCLASSCHAINS];

//==============================================================================
//
// Static Variables
//

// These numbers are used to track the number of definitions processed in order
// to enable last-chance defaults processing.
static int num_edf_skins;
static int num_edf_pclasses;

//==============================================================================
//
// Skins
//

//
// E_AddPlayerClassSkin
//
// Adds a pointer to a skin generated for a player class to EDF skin hash table.
//
static void E_AddPlayerClassSkin(skin_t *skin)
{
   unsigned int key = D_HashTableKey(skin->skinname) % NUMEDFSKINCHAINS;

   skin->ehashnext = edf_skins[key];
   edf_skins[key] = skin;
}

//
// E_EDFSkinForName
//
// Function to retrieve a specific EDF skin for its name. This shouldn't be
// confused with the general skin hash in p_skin.c, which is for the rest of
// the game engine to use.
//
static skin_t *E_EDFSkinForName(const char *name)
{
   unsigned int key = D_HashTableKey(name) % NUMEDFSKINCHAINS;
   skin_t *skin = edf_skins[key];

   while(skin && strcasecmp(name, skin->skinname))
      skin = skin->ehashnext;

   return skin;
}

#define IS_SET(sec, name) (def || cfg_size(sec, name) > 0)

//
// E_DoSkinSound
//
// Sets an EDF skin sound name.
//
static void E_DoSkinSound(cfg_t *sndsec, boolean def, skin_t *skin, int idx,
                          const char *itemname)
{
   if(IS_SET(sndsec, itemname))
   {
      if(skin->sounds[idx])
         free(skin->sounds[idx]);
      skin->sounds[idx] = strdup(cfg_getstr(sndsec, itemname));
   }
}

//
// A table of EDF skin sound names, for lookup by skin sound index number
//
static const char *skin_sound_names[NUMSKINSOUNDS] =
{
   ITEM_SKINSND_PAIN,
   ITEM_SKINSND_DIEHI,
   ITEM_SKINSND_OOF,
   ITEM_SKINSND_GIB,
   ITEM_SKINSND_PUNCH,
   ITEM_SKINSND_RADIO,
   ITEM_SKINSND_DIE,
   ITEM_SKINSND_FALL,
   ITEM_SKINSND_FEET,
   ITEM_SKINSND_FALLHIT,
   ITEM_SKINSND_PLWDTH,
   ITEM_SKINSND_NOWAY,
};

//
// E_CreatePlayerSkin
//
// Creates and adds a new EDF player skin
//
static void E_CreatePlayerSkin(cfg_t *skinsec)
{
   skin_t *newSkin;
   const char *tempstr;
   boolean def; // if defining true; if modifying, false

   // skin name is section title
   tempstr = cfg_title(skinsec);

   // if a skin by this name already exists, we must modify it instead of
   // creating a new one.
   if(!(newSkin = E_EDFSkinForName(tempstr)))
   {
      E_EDFLogPrintf("\t\tCreating skin '%s'\n", tempstr);

      newSkin = calloc(1, sizeof(skin_t));

      // set name
      newSkin->skinname = strdup(tempstr);

      // type is always player
      newSkin->type = SKIN_PLAYER;

      // is an EDF-created skin
      newSkin->edfskin = true;

      // add the new skin to the list
      E_AddPlayerClassSkin(newSkin);

      def = true;

      ++num_edf_skins; // keep track of how many we've processed
   }
   else
   {
      E_EDFLogPrintf("\t\tModifying skin '%s'\n", tempstr);
      def = false;
   }

   // set sprite information
   if(IS_SET(skinsec, ITEM_SKIN_SPRITE))
   {
      if(newSkin->spritename)
         free(newSkin->spritename);
      newSkin->spritename = strdup(cfg_getstr(skinsec, ITEM_SKIN_SPRITE));

      // check sprite for validity
      if(E_SpriteNumForName(newSkin->spritename) == NUMSPRITES)
      {
         E_EDFLogPrintf("\t\tWarning: skin '%s' references unknown sprite '%s'\n",
                        newSkin->skinname, newSkin->spritename);
         newSkin->spritename = sprnames[playerSpriteNum];
      }

      // sprite has been reset, so reset the sprite number
      newSkin->sprite = E_SpriteNumForName(newSkin->spritename);
   }

   // set faces
   if(IS_SET(skinsec, ITEM_SKIN_FACES))
   {
      if(newSkin->facename)
         free(newSkin->facename);
      newSkin->facename = strdup(cfg_getstr(skinsec, ITEM_SKIN_FACES));

      // faces have been reset, so clear the face array pointer
      newSkin->faces = NULL; // handled by skin code
   }

   // set sounds if specified
   // (if not, the skin code will fill in some reasonable defaults)
   if(cfg_size(skinsec, ITEM_SKIN_SOUNDS) > 0)
   {
      cfg_t *sndsec = cfg_getsec(skinsec, ITEM_SKIN_SOUNDS);
      int i;

      // get sounds from the sounds section
      for(i = 0; i < NUMSKINSOUNDS; ++i)
         E_DoSkinSound(sndsec, def, newSkin, i, skin_sound_names[i]);
   }
}

//
// E_ProcessSkins
//
// Processes EDF player skin objects for use by player classes.
//
void E_ProcessSkins(cfg_t *cfg)
{
   unsigned int count, i;

   count = cfg_size(cfg, EDF_SEC_SKIN);

   E_EDFLogPrintf("\t* Processing player skins\n"
                  "\t\t%d skin(s) defined\n", count);

   for(i = 0; i < count; ++i)
      E_CreatePlayerSkin(cfg_getnsec(cfg, EDF_SEC_SKIN, i));
}

//==============================================================================
//
// Player Classes
//

//
// E_AddPlayerClass
//
// Adds a player class object to the playerclass hash table.
//
static void E_AddPlayerClass(playerclass_t *pc)
{
   unsigned int key = D_HashTableKey(pc->mnemonic) % NUMEDFPCLASSCHAINS;

   pc->next = edf_player_classes[key];
   edf_player_classes[key] = pc;
}

//
// E_PlayerClassForName
//
// Returns a player class given a name, or NULL if no such class exists.
//
playerclass_t *E_PlayerClassForName(const char *name)
{
   unsigned int key = D_HashTableKey(name) % NUMEDFPCLASSCHAINS;
   playerclass_t *pc = edf_player_classes[key];

   while(pc && strcasecmp(pc->mnemonic, name))
      pc = pc->next;

   return pc;
}

//
// E_ProcessPlayerClass
//
// Processes a single EDF player class section.
//
static void E_ProcessPlayerClass(cfg_t *pcsec)
{
   const char *tempstr;
   playerclass_t *pc;
   boolean def;

   // get mnemonic from section title
   tempstr = cfg_title(pcsec);

   // verify mnemonic
   if(strlen(tempstr) > 32)
      E_EDFLoggedErr(2, "E_ProcessPlayerClass: invalid mnemonic %s\n", tempstr);

   if(!(pc = E_PlayerClassForName(tempstr)))
   {
      // create a new player class
      pc = calloc(1, sizeof(playerclass_t));

      // set mnemonic and hash it
      strncpy(pc->mnemonic, tempstr, 33);
      E_AddPlayerClass(pc);

      E_EDFLogPrintf("\t\tCreating player class %s\n", pc->mnemonic);

      def = true;

      ++num_edf_pclasses; // keep track of how many we've processed
   }
   else
   {
      // edit an existing class
      E_EDFLogPrintf("\t\tModifying player class %s\n", pc->mnemonic);
      def = false;
   }

   // default skin name
   if(IS_SET(pcsec, ITEM_PCLASS_DEFAULTSKIN))
   {
      tempstr = cfg_getstr(pcsec, ITEM_PCLASS_DEFAULTSKIN);

      // possible error: must specify a default skin!
      if(!tempstr)
      {
         E_EDFLoggedErr(2, "E_ProcessPlayerClass: missing required defaultskin "
                           "for player class %s\n", pc->mnemonic);
      }

      // possible error 2: skin specified MUST exist
      if(!(pc->defaultskin = E_EDFSkinForName(tempstr)))
      {
         E_EDFLoggedErr(2, "E_ProcessPlayerClass: invalid defaultskin '%s' "
                           "for player class %s\n", tempstr, pc->mnemonic);
      }
   }

   // mobj type
   if(IS_SET(pcsec, ITEM_PCLASS_THINGTYPE))
   {
      tempstr = cfg_getstr(pcsec, ITEM_PCLASS_THINGTYPE);

      // thing type must be specified
      if(!tempstr)
      {
         E_EDFLoggedErr(2, "E_ProcessPlayerClass: missing required thingtype "
                           "for player class %s\n", pc->mnemonic);
      }

      // thing type must exist
      pc->type = E_GetThingNumForName(tempstr);
   }

   // altattack state
   if(IS_SET(pcsec, ITEM_PCLASS_ALTATTACK))
   {
      statenum_t statenum = 0;
      tempstr = cfg_getstr(pcsec, ITEM_PCLASS_ALTATTACK);

      // altattackstate should be specified, but if it's not, use the 
      // thing type's normal missilestate.
      if(!tempstr || (statenum = E_StateNumForName(tempstr)) == NUMSTATES)
      {
         mobjinfo_t *mi = &mobjinfo[pc->type];
         statenum = mi->missilestate;
      }
      
      pc->altattack = statenum;
   }

   // process player speed fields

   if(IS_SET(pcsec, ITEM_PCLASS_SPEEDWALK))
      pc->forwardmove[0] = cfg_getint(pcsec, ITEM_PCLASS_SPEEDWALK);

   if(IS_SET(pcsec, ITEM_PCLASS_SPEEDRUN))
      pc->forwardmove[1] = cfg_getint(pcsec, ITEM_PCLASS_SPEEDRUN);

   if(IS_SET(pcsec, ITEM_PCLASS_SPEEDSTRAFE))
      pc->sidemove[0] = cfg_getint(pcsec, ITEM_PCLASS_SPEEDSTRAFE);

   if(IS_SET(pcsec, ITEM_PCLASS_SPEEDSTRAFERUN))
      pc->sidemove[1] = cfg_getint(pcsec, ITEM_PCLASS_SPEEDSTRAFERUN);

   if(IS_SET(pcsec, ITEM_PCLASS_SPEEDTURN))
      pc->angleturn[0] = cfg_getint(pcsec, ITEM_PCLASS_SPEEDTURN);

   if(IS_SET(pcsec, ITEM_PCLASS_SPEEDTURNFAST))
      pc->angleturn[1] = cfg_getint(pcsec, ITEM_PCLASS_SPEEDTURNFAST);

   if(IS_SET(pcsec, ITEM_PCLASS_SPEEDTURNSLOW))
      pc->angleturn[2] = cfg_getint(pcsec, ITEM_PCLASS_SPEEDTURNSLOW);

   if(IS_SET(pcsec, ITEM_PCLASS_SPEEDLOOKSLOW))
      pc->lookspeed[0] = cfg_getint(pcsec, ITEM_PCLASS_SPEEDLOOKSLOW);

   if(IS_SET(pcsec, ITEM_PCLASS_SPEEDLOOKFAST))
      pc->lookspeed[1] = cfg_getint(pcsec, ITEM_PCLASS_SPEEDLOOKFAST);

   // copy speeds to original speeds
   memcpy(pc->oforwardmove, pc->forwardmove, 2 * sizeof(fixed_t));
   memcpy(pc->osidemove,    pc->sidemove,    2 * sizeof(fixed_t));

   // default flag
   if(IS_SET(pcsec, ITEM_PCLASS_DEFAULT))
   {
      cfg_bool_t tmp = cfg_getbool(pcsec, ITEM_PCLASS_DEFAULT);

      // last player class with default flag set true will become the default
      // player class for the current gamemode.
      if(tmp == cfg_true)
         GameModeInfo->defPClassName = pc->mnemonic;
   }
}

//
// E_ProcessPlayerClasses
//
// Processes all EDF player classes.
//
void E_ProcessPlayerClasses(cfg_t *cfg)
{
   unsigned int count, i;

   count = cfg_size(cfg, EDF_SEC_PCLASS);

   E_EDFLogPrintf("\t* Processing player classes\n"
                  "\t\t%d class(es) defined\n", count);

   for(i = 0; i < count; ++i)
      E_ProcessPlayerClass(cfg_getnsec(cfg, EDF_SEC_PCLASS, i));
}

//
// E_NeedDefaultPlayerData
//
// Returns true if EDF needs to perform last-chance defaults parsing for
// player.edf. This is true in the event that the number of ANY of the
// sections processed in this file is zero (at least one of each is
// required).
//
boolean E_NeedDefaultPlayerData(void)
{
   return !(num_edf_skins && num_edf_pclasses);
}

//
// Game Engine Interface
//

//
// E_VerifyDefaultPlayerClass
//
// Called after EDF processing. Verifies that the current gamemode possesses a
// valid default player class. If not, we can't run the game!
//
void E_VerifyDefaultPlayerClass(void)
{
   if(E_PlayerClassForName(GameModeInfo->defPClassName) == NULL)
   {
      I_Error("E_VerifyDefaultPlayerClass: default playerclass '%s' "
              "does not exist!\n",
              GameModeInfo->defPClassName);
   }
}

//
// E_IsPlayerClassThingType
//
// Checks all extant player classes to see if any stakes a claim on the
// indicated thingtype. Linear search over a small set. This is the only
// real, efficient way to see if a thingtype is a potential player.
//
boolean E_IsPlayerClassThingType(mobjtype_t motype)
{
   int i;

   // run down all hash chains
   for(i = 0; i < NUMEDFPCLASSCHAINS; ++i)
   {
      playerclass_t *chain = edf_player_classes[i];

      while(chain)
      {
         if(chain->type == motype) // found a match
            return true;

         chain = chain->next;
      }
   }

   return false;
}

//
// E_PlayerInWalkingState
//
// This function is necessitated by the code in P_XYMovement that puts the
// player back in his spawnstate when his momentum has expired. The previous
// code there was a gross hack that made three assumptions:
//
// 1. The player's root walking frame was S_PLAY_RUN1.
// 2. The player's walking frames were sequentially defined.
// 3. The player had exactly 4 walking frames.
//
// None of these are safe under EDF, nor under player classes. Instead we
// have to walk the state sequence stemming from the player's
// mobjinfo::seestate and compare the player's frame against every frame
// within that cycle. The frame cycle should always be relatively short
// (4 frames or so), so this isn't going to be a time-consuming operation.
// The walking frame cycle had better be closed, or this may go haywire.
//
boolean E_PlayerInWalkingState(player_t *player)
{
   state_t *pstate, *curstate, *seestate;
   int count = 0;

   pstate   = player->mo->state;
   seestate = &states[player->mo->info->seestate];

   curstate = seestate;

   do
   {
      if(curstate == pstate) // found a match
         return true;

      curstate = &states[curstate->nextstate]; // try next state

      if(++count >= 100) // seen 100 states? get out.
      {
         doom_printf(FC_ERROR "open player walk sequence detected\a\n");
         break;
      }
   }
   while(curstate != seestate); // terminate when it loops

   return false;
}

//
// E_ApplyTurbo
//
// Applies the turbo scale to all player classes.
//
void E_ApplyTurbo(int ts)
{
   int i;
   playerclass_t *pc;

   // run down all hash chains
   for(i = 0; i < NUMEDFPCLASSCHAINS; ++i)
   {
      pc = edf_player_classes[i];

      while(pc)
      {
         pc->forwardmove[0] = pc->oforwardmove[0] * ts / 100;
         pc->forwardmove[1] = pc->oforwardmove[1] * ts / 100;
         pc->sidemove[0]    =    pc->osidemove[0] * ts / 100;
         pc->sidemove[1]    =    pc->osidemove[1] * ts / 100;
         pc = pc->next;
      }
   }
}

// EOF


