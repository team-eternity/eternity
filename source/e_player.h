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

#ifndef E_PLAYER_H__
#define E_PLAYER_H__

// macros
#define NUMEDFSKINCHAINS 17

struct skin_t;

extern skin_t *edf_skins[NUMEDFSKINCHAINS];

//
// playerclass_t structure
//
struct playerclass_t
{
   skin_t *defaultskin;  // pointer to default skin
   mobjtype_t type;      // index of mobj type used
   statenum_t altattack; // index of alternate attack state for weapon code

   // speeds
   fixed_t forwardmove[2];
   fixed_t sidemove[2];
   fixed_t angleturn[3]; // + slow turn
   fixed_t lookspeed[2]; // haleyjd: look speeds (from zdoom)

   // original speeds - before turbo is applied.
   fixed_t oforwardmove[2];
   fixed_t osidemove[2];

   // hashing data
   char mnemonic[33];
   playerclass_t *next;
};

playerclass_t *E_PlayerClassForName(const char *);

void E_VerifyDefaultPlayerClass(void);
bool E_IsPlayerClassThingType(mobjtype_t);
bool E_PlayerInWalkingState(player_t *);
void E_ApplyTurbo(int ts);

// Inventory

// Inventory subclass enumeration

typedef enum
{
   INV_GENERIC, // not subclassed - a no-op inventory item
} inventoryclass_e;

// inventory flags
enum
{
   INVF_QUIET         = 0x00000001, // makes no sound on pickup
   INVF_AUTOACTIVATE  = 0x00000002, // automatically used on pickup
   INVF_UNDROPPABLE   = 0x00000004, // cannot be dropped from inventory
   INVF_INVBAR        = 0x00000008, // is displayed in inventory bar
   INVF_HUBPOWER      = 0x00000010, // kept between levels of a hub
   INVF_INTERHUBSTRIP = 0x00000020, // removed on hub or level transfer
   INVF_ALWAYSPICKUP  = 0x00000040, // item is always picked up
   INVF_BIGPOWERUP    = 0x00000080, // same as MF3_SUPERITEM
   INVF_KEEPDEPLETED  = 0x00000100, // icon remains even when used up
};

typedef struct inventoryitem_s
{
   inventoryclass_e invclass; // specific type of this inventory item

   // basic inventory properties - shared by all inventory items
   int amount;           // amount given on pick-up
   int maxamount;        // maximum amount that can be in the inventory
   const char *icon;     // icon graphic name
   const char *pmessage; // pickup message
   const char *psound;   // pickup sound
   int flashtype;        // pickup flash thingtype
   const char *usesound; // sound when used
   int respawntics;      // time til item respawns
   int givequest;        // strife quest item number

} inventoryitem_t;

// EDF-only stuff
#ifdef NEED_EDF_DEFINITIONS

#define EDF_SEC_SKIN "skin"
extern cfg_opt_t edf_skin_opts[];

#define EDF_SEC_PCLASS "playerclass"
extern cfg_opt_t edf_pclass_opts[];

void E_ProcessSkins(cfg_t *cfg);
void E_ProcessPlayerClasses(cfg_t *cfg);

#endif // NEED_EDF_DEFINITIONS

#endif

// EOF

