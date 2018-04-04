//
// Copyright (C) 2018 James Haley et al.
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
//----------------------------------------------------------------------------
//
// Purpose: EDF Player Class Module
// Authors: James Haley, Max Waine
//

#define NEED_EDF_DEFINITIONS

#include "z_zone.h"
#include "i_system.h"

#include "Confuse/confuse.h"

#include "d_dehtbl.h"
#include "d_gi.h"
#include "d_io.h"
#include "d_items.h"
#include "d_player.h"
#include "e_lib.h"
#include "e_edf.h"
#include "e_player.h"
#include "e_sprite.h"
#include "e_states.h"
#include "e_things.h"
#include "e_weapons.h"
#include "m_qstr.h"
#include "p_mobj.h"
#include "p_skin.h"
#include "v_misc.h"
#include "v_video.h"

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
#define ITEM_PCLASS_INITIALHEALTH  "initialhealth"
#define ITEM_PCLASS_SPEEDWALK      "speedwalk"
#define ITEM_PCLASS_SPEEDRUN       "speedrun"
#define ITEM_PCLASS_SPEEDSTRAFE    "speedstrafe"
#define ITEM_PCLASS_SPEEDSTRAFERUN "speedstraferun"
#define ITEM_PCLASS_SPEEDTURN      "speedturn"
#define ITEM_PCLASS_SPEEDTURNFAST  "speedturnfast"
#define ITEM_PCLASS_SPEEDTURNSLOW  "speedturnslow"
#define ITEM_PCLASS_SPEEDLOOKSLOW  "speedlookslow"
#define ITEM_PCLASS_SPEEDLOOKFAST  "speedlookfast"
#define ITEM_PCLASS_REBORNITEM     "rebornitem"
#define ITEM_PCLASS_WEAPONSLOT     "weaponslot"

#define ITEM_REBORN_NAME   "name"
#define ITEM_REBORN_AMOUNT "amount"

#define ITEM_WPNSLOT_WPNS  "weapons"
#define ITEM_WPNSLOT_CLEAR "clear"

#define ITEM_DELTA_NAME "name"

static cfg_opt_t edf_wpnslot_opts[] =
{
   CFG_STR(ITEM_WPNSLOT_WPNS,   0, CFGF_LIST),
   CFG_FLAG(ITEM_WPNSLOT_CLEAR, 0, CFGF_NONE),
   CFG_END()
};

static cfg_opt_t edf_reborn_opts[] =
{
   CFG_STR(ITEM_REBORN_NAME,   "", CFGF_NONE),
   CFG_INT(ITEM_REBORN_AMOUNT,  1, CFGF_NONE),
   CFG_END()
};

#define PLAYERCLASS_FIELDS \
   CFG_STR(ITEM_PCLASS_DEFAULTSKIN,   NULL, CFGF_NONE),  \
   CFG_STR(ITEM_PCLASS_THINGTYPE,     NULL, CFGF_NONE),  \
   CFG_STR(ITEM_PCLASS_ALTATTACK,     NULL, CFGF_NONE),  \
   CFG_INT(ITEM_PCLASS_INITIALHEALTH, 100,  CFGF_NONE),  \
                                                         \
   /* speeds */                                          \
   CFG_INT(ITEM_PCLASS_SPEEDWALK,      0x19, CFGF_NONE), \
   CFG_INT(ITEM_PCLASS_SPEEDRUN,       0x32, CFGF_NONE), \
   CFG_INT(ITEM_PCLASS_SPEEDSTRAFE,    0x18, CFGF_NONE), \
   CFG_INT(ITEM_PCLASS_SPEEDSTRAFERUN, 0x28, CFGF_NONE), \
   CFG_INT(ITEM_PCLASS_SPEEDTURN,       640, CFGF_NONE), \
   CFG_INT(ITEM_PCLASS_SPEEDTURNFAST,  1280, CFGF_NONE), \
   CFG_INT(ITEM_PCLASS_SPEEDTURNSLOW,   320, CFGF_NONE), \
   CFG_INT(ITEM_PCLASS_SPEEDLOOKSLOW,   450, CFGF_NONE), \
   CFG_INT(ITEM_PCLASS_SPEEDLOOKFAST,   512, CFGF_NONE), \
                                                         \
   CFG_BOOL(ITEM_PCLASS_DEFAULT, false, CFGF_NONE),      \
                                                         \
   /* reborn inventory items */                          \
   CFG_MVPROP(ITEM_PCLASS_REBORNITEM, edf_reborn_opts, CFGF_MULTI|CFGF_NOCASE), \
                                                                                \
    /* weapon slots */                                                          \
   CFG_SEC(ITEM_PCLASS_WEAPONSLOT,   edf_wpnslot_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NOCASE), \
                                                                                               \
   CFG_END()

cfg_opt_t edf_pclass_opts[] =
{
   PLAYERCLASS_FIELDS
};

cfg_opt_t edf_pdelta_opts[] =
{
   CFG_STR(ITEM_DELTA_NAME, 0, CFGF_NONE),
   PLAYERCLASS_FIELDS
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
static void E_DoSkinSound(cfg_t *sndsec, bool def, skin_t *skin, int idx,
                          const char *itemname)
{
   if(IS_SET(sndsec, itemname))
      E_ReplaceString(skin->sounds[idx], cfg_getstrdup(sndsec, itemname));
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
   bool def; // if defining true; if modifying, false

   // skin name is section title
   tempstr = cfg_title(skinsec);

   // if a skin by this name already exists, we must modify it instead of
   // creating a new one.
   if(!(newSkin = E_EDFSkinForName(tempstr)))
   {
      E_EDFLogPrintf("\t\tCreating skin '%s'\n", tempstr);

      newSkin = ecalloc(skin_t *, 1, sizeof(skin_t));

      // set name
      newSkin->skinname = estrdup(tempstr);

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
      tempstr = cfg_getstr(skinsec, ITEM_SKIN_SPRITE);

      // check sprite for validity
      if(E_SpriteNumForName(tempstr) == -1)
      {
         E_EDFLoggedWarning(2, "Warning: skin '%s' references unknown sprite '%s'\n",
                            newSkin->skinname, tempstr);
         tempstr = sprnames[blankSpriteNum];
      }

      // set it
      E_ReplaceString(newSkin->spritename, estrdup(tempstr));

      // sprite has been reset, so reset the sprite number
      newSkin->sprite = E_SpriteNumForName(newSkin->spritename);
   }

   // set faces
   if(IS_SET(skinsec, ITEM_SKIN_FACES))
   {
      E_ReplaceString(newSkin->facename, cfg_getstrdup(skinsec, ITEM_SKIN_FACES));

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
// E_freeRebornItems
//
// Free a player class's default inventory when recreating it.
//
static void E_freeRebornItems(playerclass_t *pc)
{
   if(pc->rebornitems)
   {
      for(unsigned int i = 0; i < pc->numrebornitems; i++)
      {
         if(pc->rebornitems[i].itemname)
            efree(pc->rebornitems[i].itemname);
      }
      efree(pc->rebornitems);
      pc->rebornitems    = NULL;
      pc->numrebornitems = 0;
   }
}

//
// E_processRebornItem
//
// Process a single rebornitem for the player's default inventory.
//
static void E_processRebornItem(cfg_t *item, playerclass_t *pc, unsigned int index)
{
   reborninventory_t *ri = &pc->rebornitems[index];

   ri->itemname = estrdup(cfg_getstr(item, ITEM_REBORN_NAME));
   ri->amount   = cfg_getint(item, ITEM_REBORN_AMOUNT);
   ri->flags    = 0;
}

//
// Free a player class's default weapon slots when recreating it.
//
static void E_freeWeaponSlot(playerclass_t *pc, int slot)
{
   weaponslot_t *&wepslot = pc->weaponslots[slot];

   // Delete any existing weapon slot
   if(wepslot != nullptr)
   {
      BDListItem<weaponslot_t> *prevslot, *currslot = wepslot->links.bdNext;
      do
      {
         prevslot = currslot;
         currslot->remove(&currslot);
         efree(prevslot->bdObject);
      } while(prevslot != currslot);
      wepslot = nullptr;
   }
}

//
// Process a single weaponslot for the player's default inventory.
//
static void E_processWeaponSlot(cfg_t *slot, playerclass_t *pc)
{
   const qstring titlestr = qstring(cfg_title(slot));
   const int slotindex = titlestr.toInt() - 1;
   const int numweapons = cfg_size(slot, ITEM_WPNSLOT_WPNS);


   if(slotindex > NUMWEAPONSLOTS - 1 || slotindex < 0)
   {
      E_EDFLoggedErr(2, "E_processWeaponSlot: Slot number %d in playerclass '%s' "
                        "larger than %d or less than 1\n",
                     slotindex + 1, pc->mnemonic, NUMWEAPONSLOTS);
      return;
   }

   E_freeWeaponSlot(pc, slotindex);

   if(cfg_size(slot, ITEM_WPNSLOT_CLEAR) > 0)
   {
      // No more processing, since the slot has been cleared.
      // Warn the player if they also defined weapons.
      if(numweapons > 0)
      {
         E_EDFLoggedWarning(2, "E_processWeaponSlot: 'clear' found in weaponslot definition "
                               "that contains weapons in playerclass '%s', slot %d; "
                               "'clear' option overrides\n",
                            pc->mnemonic, slotindex + 1);
      }
      return;
   }

   bool *weaponinslot = ecalloc(bool *, NUMWEAPONTYPES, sizeof(bool));
   weaponslot_t             *initslot = estructalloc(weaponslot_t, 1);
   BDListItem<weaponslot_t> &slotlist = initslot->links;
   BDListItem<weaponslot_t>::Init(slotlist);
   slotlist.bdObject = initslot;
   for(int i = numweapons; i --> 0;)
   {
      const char   *weaponname = cfg_getnstr(slot, ITEM_WPNSLOT_WPNS, i);
      weaponinfo_t *weapon     = E_WeaponForName(weaponname);
      if(weapon)
      {
         if(weaponinslot[weapon->id])
         {
            E_EDFLoggedErr(2, "E_processWeaponSlot: Weapon \"%s\" detected multiple times "
                              "in slot %d\n", weaponname, i + 1);
         }
         else
            weaponinslot[weapon->id] = true;

         weaponslot_t *curslot = estructalloc(weaponslot_t, 1);
         curslot->links.bdData = numweapons - (i + 1);
         curslot->weapon       = weapon;
         curslot->links.insert(curslot, slotlist);
      }
      else
         E_EDFLoggedErr(2, "E_processWeaponSlot: Weapon \"%s\" not found\n", weaponname);
   }
   efree(weaponinslot);
   pc->weaponslots[slotindex] = initslot;
}

//
// E_processPlayerClass
//
// Processes a single EDF player class section.
//
static void E_processPlayerClass(cfg_t *pcsec, bool delta)
{
   const char *tempstr;
   playerclass_t *pc;
   bool def;

   if(!delta)
   {
      // get mnemonic from section title
      tempstr = cfg_title(pcsec);
   }
   else if(cfg_size(pcsec, ITEM_DELTA_NAME))
   {
      if(!(tempstr = cfg_getstr(pcsec, ITEM_DELTA_NAME)))
      {
         // get mnemonic from the "name" option
         E_EDFLoggedErr(2, "E_processPlayerClass: playerclass name '%s' not found\n", tempstr);
      }
   }
   else
      E_EDFLoggedErr(2, "E_ProcessWeaponDeltas: playerdelta requires name field\n");
   

   if(!(pc = E_PlayerClassForName(tempstr)))
   {
      // create a new player class
      pc = estructalloc(playerclass_t, 1);

      // verify mnemonic
      if(strlen(tempstr) >= sizeof(pc->mnemonic))
         E_EDFLoggedErr(2, "E_ProcessPlayerClass: invalid mnemonic %s\n", tempstr);

      // set mnemonic and hash it
      strncpy(pc->mnemonic, tempstr, sizeof(pc->mnemonic));
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
      if(!tempstr || (statenum = E_StateNumForName(tempstr)) < 0)
      {
         mobjinfo_t *mi = mobjinfo[pc->type];
         statenum = mi->missilestate;
      }
      
      pc->altattack = statenum;
   }

   // initial health
   if(IS_SET(pcsec, ITEM_PCLASS_INITIALHEALTH))
      pc->initialhealth = cfg_getint(pcsec, ITEM_PCLASS_INITIALHEALTH);

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
      bool tmp = cfg_getbool(pcsec, ITEM_PCLASS_DEFAULT);

      // last player class with default flag set true will become the default
      // player class for the current gamemode.
      if(tmp == true)
         GameModeInfo->defPClassName = pc->mnemonic;
   }

   unsigned int numitems;
   if((numitems = cfg_size(pcsec, ITEM_PCLASS_REBORNITEM)) > 0)
   {
      E_freeRebornItems(pc);
      pc->numrebornitems = numitems;
      pc->rebornitems    = estructalloc(reborninventory_t, numitems);

      for(unsigned int i = 0; i < numitems; i++)
         E_processRebornItem(cfg_getnmvprop(pcsec, ITEM_PCLASS_REBORNITEM, i), pc, i);
   }

   unsigned int numweaponslots;
   if((numweaponslots = cfg_size(pcsec, ITEM_PCLASS_WEAPONSLOT)) > 0)
   {
      pc->hasslots = true;

      for(int i = numweaponslots; i --> 0;)
         E_processWeaponSlot(cfg_getnsec(pcsec, ITEM_PCLASS_WEAPONSLOT, i), pc);
   }
   else
      pc->hasslots = false;
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
      E_processPlayerClass(cfg_getnsec(cfg, EDF_SEC_PCLASS, i), false);

   E_VerifyDefaultPlayerClass();
}

//
// E_ProcessPlayerClasses
//
// Processes all EDF player classes.
//
void E_ProcessPlayerDeltas(cfg_t *cfg)
{
   unsigned int count, i;

   count = cfg_size(cfg, EDF_SEC_PDELTA);

   E_EDFLogPrintf("\t* Processing player deltas\n"
                  "\t\t%d delta(s) defined\n", count);

   for(i = 0; i < count; ++i)
      E_processPlayerClass(cfg_getnsec(cfg, EDF_SEC_PDELTA, i), true);

   E_VerifyDefaultPlayerClass();
}

//==============================================================================
//
// Game Engine Interface for Player Classes
//

//
// E_VerifyDefaultPlayerClass
//
// Called after EDF processing. Verifies that the current gamemode possesses a
// valid default player class. If not, we can't run the game!
//
void E_VerifyDefaultPlayerClass()
{
   if(E_PlayerClassForName(GameModeInfo->defPClassName) == NULL)
   {
      I_Error("E_VerifyDefaultPlayerClass: default playerclass '%s' "
              "does not exist!\n",
              GameModeInfo->defPClassName);
   }
}

//
// Recursively populate a weapon slot with data from a WeaponSlotTree
//
static void E_populateWeaponSlot(BDListItem<weaponslot_t> &slotlist, WeaponSlotNode *node, int &data)
{
   if(node->left)
      E_populateWeaponSlot(slotlist, node->left, data);

   weaponslot_t *curslot = estructalloc(weaponslot_t, 1);
   curslot->links.bdData = data;
   curslot->weapon = node->object;
   curslot->links.insert(curslot, slotlist);
   data++;

   if(node->right)
      E_populateWeaponSlot(slotlist, node->right, data);
}

//
// Creates a weapon slot from a given tree, then assigns it to the appropriate pclass slot
//
static inline void E_createWeaponSlotFromTree(playerclass_t *pc, int slotindex, WeaponSlotTree *slottree)
{
   weaponslot_t *initslot = estructalloc(weaponslot_t, 1);
   BDListItem<weaponslot_t> &slotlist = initslot->links;
   BDListItem<weaponslot_t>::Init(slotlist);
   slotlist.bdObject = initslot;

   int temp = 0;
   E_populateWeaponSlot(slotlist, slottree->root, temp);
   pc->weaponslots[slotindex] = initslot;
}

//
// Recursively place all weapons from the global slot tree into the playerclass weaponslot (in-order traversal)
//
static void E_AddGlobalWeaponsToSlot(WeaponSlotTree *slot, WeaponSlotNode *node, bool *weaponinslot)
{
   if(node == nullptr)
      return;
   if(slot == nullptr)
      slot = new WeaponSlotTree();

   if(node->left)
      E_AddGlobalWeaponsToSlot(slot, node->left, weaponinslot);
   if(weaponinslot[node->object->id] == false)
   {
      slot->insert(node->object->defaultslotrank, node->object);
      weaponinslot[node->object->id] = true; // Not necessary, but for safety purposes
   }
   if(node->right)
      E_AddGlobalWeaponsToSlot(slot, node->right, weaponinslot);
}


//
// Process weapon slots for the last time
//
void E_ProcessFinalWeaponSlots()
{
   for(int i = 0; i < NUMEDFPCLASSCHAINS; ++i)
   {
      playerclass_t *chain = edf_player_classes[i];

      while(chain)
      {
         for(int i = NUMWEAPONSLOTS; i--> 0;)
         {
            bool *weaponinslot = ecalloc(bool *, NUMWEAPONTYPES, sizeof(bool));
            WeaponSlotTree *pclassslottree = nullptr;
            if(chain->weaponslots[i] != nullptr)
            {
               pclassslottree = new WeaponSlotTree();
               auto *slotiterator = E_FirstInSlot(chain->weaponslots[i]);
               int weaponnum = 1;

               while(!slotiterator->isDummy())
               {
                  pclassslottree->insert(weaponnum << FRACBITS, slotiterator->bdObject->weapon);
                  weaponinslot[slotiterator->bdObject->weapon->id] = true;

                  weaponnum++;
                  slotiterator = slotiterator->bdNext;
               }
            }

            if(weaponslots[i])
               E_AddGlobalWeaponsToSlot(pclassslottree, weaponslots[i]->root, weaponinslot);

            if(pclassslottree != nullptr)
            {
               E_freeWeaponSlot(chain, i);
               E_createWeaponSlotFromTree(chain, i, pclassslottree);
               delete pclassslottree;
            }
            efree(weaponinslot);
         }

         chain = chain->next;
      }
   }
}

//
// E_IsPlayerClassThingType
//
// Checks all extant player classes to see if any stakes a claim on the
// indicated thingtype. Linear search over a small set. This is the only
// real, efficient way to see if a thingtype is a potential player.
//
bool E_IsPlayerClassThingType(mobjtype_t motype)
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
bool E_PlayerInWalkingState(player_t *player)
{
   state_t *pstate, *curstate, *seestate;
   int count = 0;

   pstate   = player->mo->state;
   seestate = states[player->mo->info->seestate];

   curstate = seestate;

   do
   {
      if(curstate == pstate) // found a match
         return true;

      curstate = states[curstate->nextstate]; // try next state

      if(++count >= 100) // seen 100 states? get out.
      {
         doom_printf(FC_ERROR "Open player walk sequence detected!\a\n");
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
         
         pc->sidemove[0] = pc->osidemove[0] * ts / 100;
         pc->sidemove[1] = pc->osidemove[1] * ts / 100;
         
         pc = pc->next;
      }
   }
}

// EOF


