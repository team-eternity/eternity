//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Dynamic menus -- EDF subsystem.
// Authors: James Haley
//

#include "z_zone.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "d_gi.h"
#include "d_io.h"
#include "d_dehtbl.h"
#include "mn_engin.h"
#include "v_misc.h"
#include "v_video.h"

#define NEED_EDF_DEFINITIONS

#include "Confuse/confuse.h"
#include "e_edf.h"
#include "e_lib.h"
#include "mn_emenu.h"

// menu section keywords

#define ITEM_MENU_ITEM     "item"
#define ITEM_MENU_PREVPAGE "prevpage"
#define ITEM_MENU_NEXTPAGE "nextpage"
#define ITEM_MENU_X        "x"
#define ITEM_MENU_Y        "y"
#define ITEM_MENU_FIRST    "first"
#define ITEM_MENU_FLAGS    "flags"

#define ITEM_MNITEM_TYPE   "type"
#define ITEM_MNITEM_TEXT   "text"
#define ITEM_MNITEM_CMD    "cmd"
#define ITEM_MNITEM_PATCH  "patch"
#define ITEM_MNITEM_FLAGS  "flags"

#define ITEM_MN_EPISODE    "mn_episode"

// menu item options table
static cfg_opt_t mnitem_opts[] =
{
   CFG_STR(ITEM_MNITEM_TYPE,       "info",            CFGF_NONE),
   CFG_STR(ITEM_MNITEM_TEXT,       "",                CFGF_NONE),
   CFG_STR(ITEM_MNITEM_CMD,        nullptr,           CFGF_NONE),
   CFG_STR(ITEM_MNITEM_PATCH,      nullptr,           CFGF_NONE),
   CFG_STR(ITEM_MNITEM_FLAGS,      nullptr,           CFGF_NONE),
   CFG_END()
};

// menu options table
cfg_opt_t edf_menu_opts[] =
{
   CFG_SEC(ITEM_MENU_ITEM,         mnitem_opts,       CFGF_MULTI | CFGF_NOCASE),
   CFG_STR(ITEM_MENU_PREVPAGE,     "",                CFGF_NONE),
   CFG_STR(ITEM_MENU_NEXTPAGE,     "",                CFGF_NONE),
   CFG_INT(ITEM_MENU_X,            200,               CFGF_NONE),
   CFG_INT(ITEM_MENU_Y,            15,                CFGF_NONE),
   CFG_INT(ITEM_MENU_FIRST,        0,                 CFGF_NONE),
   CFG_STR(ITEM_MENU_FLAGS,        nullptr,           CFGF_NONE),
   CFG_END()
};

// menu item type strings (used with E_StrToNumLinear)
static const char *mnitem_types[] =
{
   "gap",
   "runcmd",
   "variable",
   "varnodefault",
   "toggle",
   "title",
   "info",
   "slider",
   "bigslider",
   "automap",
   "binding",
};

#define NUM_MNITEM_TYPES (sizeof(mnitem_types) / sizeof(char *))

// Menu Flag dehflags structure

static dehflags_t mnflagvalues[] =
{
   { "skullmenu",     mf_skullmenu     },
   { "background",    mf_background    },
   { "leftaligned",   mf_leftaligned   },
   { "centeraligned", mf_centeraligned },
   { "emulated",      mf_emulated      },
   { "bigfont",       mf_bigfont       },
   { nullptr,         0                }
};

static dehflagset_t mnflagset = { mnflagvalues, 0 };

// Menu Item Flag dehflags structure

static dehflags_t mnitemflagvalues[] =
{
   { "BIGFONT",  MENUITEM_BIGFONT  },
   { "CENTERED", MENUITEM_CENTERED },
   { "LALIGNED", MENUITEM_LALIGNED },
   { nullptr,    0                 },
};

static dehflagset_t mnitemflagset = { mnitemflagvalues, 0 };

// dynamic menu hash table

#define NUMMENUCHAINS 7
static menu_t *dynaMenuChains[NUMMENUCHAINS];


//
// MN_DynamicMenuForName
//
// Returns a pointer to a dynamic menu given its name.
// Returns nullptr if no such menu exists.
//
menu_t *MN_DynamicMenuForName(const char *name)
{
   menu_t *curmenu = dynaMenuChains[D_HashTableKey(name) % NUMMENUCHAINS];

   while(curmenu && strcasecmp(curmenu->name, name))
      curmenu = curmenu->dynanext;

   return curmenu;
}

//
// MN_HashDynamicMenu
//
// Adds a dynamic menu to the hash table.
//
static void MN_HashDynamicMenu(menu_t *menu)
{
   unsigned int key = D_HashTableKey(menu->name) % NUMMENUCHAINS;

   menu->dynanext = dynaMenuChains[key];
   dynaMenuChains[key] = menu;
}

//
// MN_CreateDynamicMenu
//
// Creates a dynamic menu and hashes it.
//
static menu_t *MN_CreateDynamicMenu(const char *name)
{
   menu_t *newMenu = ecalloc(menu_t *, 1, sizeof(menu_t));

   // set name
   if(strlen(name) > 32)
      E_EDFLoggedErr(2, "MN_CreateDynamicMenu: mnemonic '%s' is too long\n", name);

   strncpy(newMenu->name, name, 33);

   // hash it
   MN_HashDynamicMenu(newMenu);

   return newMenu;
}


//
// MN_InitDynamicMenu
//
// Sets up the fields of a dynamic menu given all the information about
// it. The items and the menu must have been created and hashed previously.
//
static void MN_InitDynamicMenu(menu_t *newMenu, menuitem_t *items, 
                               const char *prev, const char *next, 
                               int x, int y, int firstitem, unsigned int flags)
{
   // set fields
   newMenu->menuitems = items;
   newMenu->prevpage  = MN_DynamicMenuForName(prev);
   newMenu->nextpage  = MN_DynamicMenuForName(next);
   newMenu->x         = x;
   newMenu->y         = y;
   newMenu->selected  = firstitem;
   newMenu->flags     = flags;

   // if this menu has a previous page...
   if(newMenu->prevpage)
   {
      menu_t *curpage = newMenu;

      // go back through the pages until we find a menu that has no prevpage
      while(curpage->prevpage)
         curpage = curpage->prevpage;

      // the menu we have found is the root page of this menu
      newMenu->rootpage = curpage;
   }
   else // otherwise, set rootpage to self
      newMenu->rootpage = newMenu;
}

//
// MN_CreateMenuItems
//
// Given an EDF menu section, returns an array of menu items.
// Returns nullptr if an error occurs.
//
static menuitem_t *MN_CreateMenuItems(cfg_t *menuSec)
{
   menuitem_t *items;
   unsigned int i;
   unsigned int itemCount = cfg_size(menuSec, ITEM_MENU_ITEM);

   // woops! menus need at least one real item.
   if(itemCount == 0)
      return nullptr;

   // add one to itemCount for the it_end terminator
   items = estructalloc(menuitem_t, itemCount + 1);

   for(i = 0; i < itemCount; ++i)
   {
      const char *tempstr;
      cfg_t *itemSec = cfg_getnsec(menuSec, ITEM_MENU_ITEM, i);

      // process fields

      // set item type
      items[i].type = E_StrToNumLinear(mnitem_types, NUM_MNITEM_TYPES, 
                                       cfg_getstr(itemSec, ITEM_MNITEM_TYPE));
      if(items[i].type == NUM_MNITEM_TYPES)
         items[i].type = it_info; // default to information only

      // set description
      items[i].dyndescription = cfg_getstrdup(itemSec, ITEM_MNITEM_TEXT);
      items[i].description = items[i].dyndescription;

      // set command
      if((tempstr = cfg_getstr(itemSec, ITEM_MNITEM_CMD)))
         items[i].data = items[i].dyndata = estrdup(tempstr);

      // set patch
      if((tempstr = cfg_getstr(itemSec, ITEM_MNITEM_PATCH)))
         items[i].patch = items[i].dynpatch = estrdup(tempstr);

      // set flags
      if((tempstr = cfg_getstr(itemSec, ITEM_MNITEM_FLAGS)))
         items[i].flags = E_ParseFlags(tempstr, &mnitemflagset);
   }

   // initialize terminator
   items[itemCount].type = it_end;

   return items;
}

//
// MN_ClearDynamicMenu
//
// Clears out a dynamic menu completely to allow it to be overwritten
// by a new menu of the same name.
//
static void MN_ClearDynamicMenu(menu_t *menu)
{
   // first: clear out menu items
   if(menu->menuitems)
   {
      menuitem_t *item = menu->menuitems;
      
      while(item->type != it_end)
      {
         if(item->dyndescription)
            efree(item->dyndescription);
         if(item->dyndata)
            efree(item->dyndata);
         if(item->dynpatch)
            efree(item->dynpatch);

         ++item;
      }
      efree(menu->menuitems);
   }

   // zero out menu data fields (cannot memset, must maintain hash data)
   menu->menuitems = nullptr;
   menu->prevpage  = nullptr;
   menu->nextpage  = nullptr;
   menu->rootpage  = menu; // point to self
   menu->x         = 0;
   menu->y         = 0;
   menu->selected  = 0;
   menu->flags     = 0;
}

//
// MN_ProcessMenu
//
// Processes a single menu section.
//
static void MN_ProcessMenu(menu_t *menu, cfg_t *menuSec)
{
   int x, y, first;
   unsigned int flags;
   const char *flagstr, *prev, *next;
   menuitem_t *items;

   // first: process items
   if(!(items = MN_CreateMenuItems(menuSec)))
      E_EDFLoggedErr(2, "MN_ProcessMenu: menu %s is empty\n", menu->name);

   // process integer items
   x     = cfg_getint(menuSec, ITEM_MENU_X);
   y     = cfg_getint(menuSec, ITEM_MENU_Y);
   first = cfg_getint(menuSec, ITEM_MENU_FIRST);

   // process string items
   prev  = cfg_getstr(menuSec, ITEM_MENU_PREVPAGE);
   next  = cfg_getstr(menuSec, ITEM_MENU_NEXTPAGE);
   if((flagstr = cfg_getstr(menuSec, ITEM_MENU_FLAGS)))
      flags = E_ParseFlags(flagstr, &mnflagset);
   else
      flags = 0;

   MN_InitDynamicMenu(menu, items, prev, next, x, y, first, flags);

   E_EDFLogPrintf("\t\tFinished menu %s\n", menu->name);
}

// global menu overrides
menu_t *mn_episode_override = nullptr;

//
// MN_ProcessMenus
//
// Processes all EDF dynamic menu sections
//
void MN_ProcessMenus(cfg_t *cfg)
{
   unsigned int i, numMenus;
   const char *override_name;

   numMenus = cfg_size(cfg, EDF_SEC_MENU);

   E_EDFLogPrintf("\t* Processing dynamic menus\n"
                  "\t\t%d dynamic menus defined\n", numMenus);
   
   // first: hash all the menus
   // if a menu of the name already exists, it will be cleared out
   for(i = 0; i < numMenus; ++i)
   {
      const char *tempstr;
      cfg_t *menuSec = cfg_getnsec(cfg, EDF_SEC_MENU, i);
      menu_t *menu;

      tempstr = cfg_title(menuSec);

      if(!(menu = MN_DynamicMenuForName(tempstr)))
         MN_CreateDynamicMenu(tempstr);
      else
         MN_ClearDynamicMenu(menu);
   }

   // next: process the menus
   for(i = 0; i < numMenus; ++i)
   {
      const char *tempstr;
      cfg_t *menuSec = cfg_getnsec(cfg, EDF_SEC_MENU, i);
      menu_t *menu;

      tempstr = cfg_title(menuSec);

      if((menu = MN_DynamicMenuForName(tempstr)))
         MN_ProcessMenu(menu, menuSec);
   }

   // now, process menu-related variables

   // allow episode menu override
   if((override_name = cfg_getstr(cfg, ITEM_MN_EPISODE)))
   {
      // not allowed in a shareware gamemode!
      if(GameModeInfo->flags & GIF_SHAREWARE)
      {
         E_EDFLoggedErr(1, "MN_ProcessMenus: can't override episodes " 
                           "in shareware. Register!\n");
      }

      mn_episode_override = MN_DynamicMenuForName(override_name);
   }
}

//
// mn_dynamenu
//
// This console command starts the dynamic menu named in
// Console.argv[0] -- this can even be used within dynamic menus
// to reference other dynamic menus from command-type items.
//
CONSOLE_COMMAND(mn_dynamenu, 0)
{
   menu_t *menu;

   if(Console.argc != 1)
   {
      C_Puts("usage: mn_dynamenu <menu name>");
      return;
   }

   if(!(menu = MN_DynamicMenuForName(Console.argv[0]->constPtr())))
   {
      C_Printf(FC_ERROR "no such menu %s\a\n", Console.argv[0]->constPtr());
      return;
   }

   MN_StartMenu(menu);
}

// EOF

