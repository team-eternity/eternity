// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 James Haley
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
//    EDF Inventory
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#define NEED_EDF_DEFINITIONS

#include "Confuse/confuse.h"
#include "e_edf.h"
#include "e_lib.h"
#include "e_inventory.h"

#include "d_dehtbl.h" // for dehflags parsing

// basic inventory flag values and mnemonics

static dehflags_t inventoryflags[] =
{
   { "AUTOACTIVATE",    INVF_AUTOACTIVATE    },
   { "UNDROPPABLE",     INVF_UNDROPPABLE     },
   { "INVBAR",          INVF_INVBAR          },
   { "HUBPOWER",        INVF_HUBPOWER        },
   { "PERSISTENTPOWER", INVF_PERSISTENTPOWER },
   { "ALWAYSPICKUP",    INVF_ALWAYSPICKUP    },
   { "KEEPDEPLETED",    INVF_KEEPDEPLETED    },
   { "ADDITIVETIME",    INVF_ADDITIVETIME    },
   { "UNTOSSABLE",      INVF_UNTOSSABLE      },
   { NULL,              0                    }
};

static dehflagset_t inventory_flagset =
{
   inventoryflags, // flaglist
   0,              // mode
};

// Frame section keywords
#define ITEM_INVENTORY_ID             "id"
#define ITEM_INVENTORY_AMOUNT         "amount"
#define ITEM_INVENTORY_MAXAMOUNT      "maxamount"
#define ITEM_INVENTORY_INTERHUBAMOUNT "interhubamount"
#define ITEM_INVENTORY_ICON           "icon"
#define ITEM_INVENTORY_PICKUPMESSAGE  "pickupmessage"
#define ITEM_INVENTORY_PICKUPSOUND    "pickupsound"
#define ITEM_INVENTORY_PICKUPFLASH    "pickupflash"
#define ITEM_INVENTORY_USESOUND       "usesound"
#define ITEM_INVENTORY_RESPAWNTICS    "respawntics"
#define ITEM_INVENTORY_GIVEQUEST      "givequest"
#define ITEM_INVENTORY_FLAGS          "flags"

#define ITEM_DELTA_NAME               "name"

//
// Inventory Options
//

#define INVENTORY_FIELDS \
   CFG_INT(ITEM_INVENTORY_ID,             -1, CFGF_NONE), \
   CFG_INT(ITEM_INVENTORY_AMOUNT,          0, CFGF_NONE), \
   CFG_INT(ITEM_INVENTORY_MAXAMOUNT,       0, CFGF_NONE), \
   CFG_INT(ITEM_INVENTORY_INTERHUBAMOUNT,  0, CFGF_NONE), \
   CFG_STR(ITEM_INVENTORY_ICON,           "", CFGF_NONE), \
   CFG_STR(ITEM_INVENTORY_PICKUPMESSAGE,  "", CFGF_NONE), \
   CFG_STR(ITEM_INVENTORY_PICKUPSOUND,    "", CFGF_NONE), \
   CFG_STR(ITEM_INVENTORY_PICKUPFLASH,    "", CFGF_NONE), \
   CFG_STR(ITEM_INVENTORY_USESOUND,       "", CFGF_NONE), \
   CFG_INT(ITEM_INVENTORY_RESPAWNTICS,     0, CFGF_NONE), \
   CFG_INT(ITEM_INVENTORY_GIVEQUEST,      -1, CFGF_NONE), \
   CFG_STR(ITEM_INVENTORY_FLAGS,          "", CFGF_NONE), \
   CFG_END()

cfg_opt_t edf_inv_opts[] =
{
   INVENTORY_FIELDS
};

cfg_opt_t edf_invdelta_opts[] =
{
   CFG_STR(ITEM_DELTA_NAME, 0, CFGF_NONE),
   INVENTORY_FIELDS
};

// EOF

