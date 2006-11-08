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

#include "z_zone.h"
#include "p_skin.h"

#include "Confuse/confuse.h"

#define NEED_EDF_DEFINITIONS

#include "e_lib.h"
#include "e_edf.h"
#include "e_player.h"

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
};

#define ITEM_SKIN_SPRITE "sprite"
#define ITEM_SKIN_FACES  "faces"
#define ITEM_SKIN_SOUNDS "sounds"

cfg_opt_t edf_skin_opts[] =
{
   CFG_STR(ITEM_SKIN_SPRITE, "PLAY",             CFGF_NONE),
   CFG_STR(ITEM_SKIN_FACES,  "STF",              CFGF_NONE),
   CFG_SEC(ITEM_SKIN_SOUNDS, pc_skin_sound_opts, CFGF_NOCASE),
   CFG_END()
};

#define ITEM_PCLASS_DEFAULTSKIN "defaultskin"

cfg_opt_t edf_pclass_opts[] =
{
   CFG_STR(ITEM_PCLASS_DEFAULTSKIN, NULL, CFGF_NONE),
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

//==============================================================================
//
// Code
//

//
// E_AddPlayerClassSkin
//
// Adds a pointer to a skin generated for a player class to EDF skin hash table.
//
static void E_AddPlayerClassSkin(skin_t *skin)
{
   int key = D_HashTableKey(skin->skinname) % NUMEDFSKINCHAINS;

   skin->hashnext = edf_skins[key];
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
   int key = D_HashTableKey(name) % NUMEDFSKINCHAINS;
   skin_t *chain = edf_skins[key];

   while(chain && strcasecmp(name, chain->skinname))
      chain = chain->hashnext;

   return chain;
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
// E_CreatePlayerSkin
//
// Creates and adds a new EDF 
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

      newSkin = malloc(sizeof(skin_t));
      memset(newSkin, 0, sizeof(skin_t));

      // set name
      newSkin->skinname = strdup(tempstr);

      // type is always player
      newSkin->type = SKIN_PLAYER;

      // add the new skin to the list
      E_AddPlayerClassSkin(newSkin);

      def = true;
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
         E_EDFLogPrintf("\t\tWarning: skin '%s' references invalid sprite '%s'\n",
                        newSkin->skinname, newSkin->spritename);
         
         // substitute player sprite
         newSkin->spritename = sprnames[playerSpriteNum];
      }

      // sprite has been reset, so clear the sprite number
      newSkin->sprite = 0; // handled by skin code
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

      // get sounds from the sounds section
      E_DoSkinSound(sndsec, def, newSkin, sk_plpain, ITEM_SKINSND_PAIN);
      E_DoSkinSound(sndsec, def, newSkin, sk_pdiehi, ITEM_SKINSND_DIEHI);
      E_DoSkinSound(sndsec, def, newSkin, sk_oof,    ITEM_SKINSND_OOF);
      E_DoSkinSound(sndsec, def, newSkin, sk_slop,   ITEM_SKINSND_GIB);
      E_DoSkinSound(sndsec, def, newSkin, sk_punch,  ITEM_SKINSND_PUNCH);
      E_DoSkinSound(sndsec, def, newSkin, sk_radio,  ITEM_SKINSND_RADIO);
      E_DoSkinSound(sndsec, def, newSkin, sk_pldeth, ITEM_SKINSND_DIE);
      E_DoSkinSound(sndsec, def, newSkin, sk_plfall, ITEM_SKINSND_FALL);
      E_DoSkinSound(sndsec, def, newSkin, sk_plfeet, ITEM_SKINSND_FEET);
      E_DoSkinSound(sndsec, def, newSkin, sk_fallht, ITEM_SKINSND_FALLHIT);
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

//
// E_ProcessPlayerClass
//
// Processes a single EDF player class section.
//
static void E_ProcessPlayerClass(cfg_t *pc)
{
   const char *tempstr;

   // get mnemonic (section title)
   tempstr = cfg_title(pc);

   E_EDFLogPrintf("\t\tProcessing class %s\n", tempstr);
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

// EOF


