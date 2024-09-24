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

#ifndef E_PLAYER_H__
#define E_PLAYER_H__

// macros
constexpr int NUMEDFSKINCHAINS = 17;
constexpr int NUMWEAPONSLOTS   = 16;

struct player_t;
struct skin_t;
struct weaponslot_t;

extern skin_t *edf_skins[NUMEDFSKINCHAINS];

enum rebornitemflag_e
{
   RBIF_IGNORE = 0x01 // this reborn item has been canceled, ie., by DeHackEd
};

//
// Player class flags
//
enum
{
   PCF_ALWAYSJUMP = 1,   // class is designed to jump, do not allow disabling it
   PCF_CHICKENTWITCH = 2,  // random twitching like Heretic chicken
   PCF_NOBOB = 4, // disable bobbing
};

// default inventory items
struct reborninventory_t
{
   char         *itemname; // EDF itemeffect name
   int           amount;   // amount of item to give when reborn
   unsigned int  flags;    // special flags
};

//
// playerclass_t structure
//
struct playerclass_t
{
   skin_t *defaultskin;  // pointer to default skin
   mobjtype_t type;      // index of mobj type used
   statenum_t altattack; // index of alternate attack state for weapon code
   int initialhealth;    // initial health when reborn
   int maxhealth;        // max health for regular items, HealThing and Gauntlets
   int superhealth;      // max health for superchargers and HealThing
   fixed_t viewheight;   // [XA] view height, relative to player's 'z' position

   // speeds
   fixed_t forwardmove[2];
   fixed_t sidemove[2];
   fixed_t angleturn[3]; // + slow turn
   fixed_t lookspeed[2]; // haleyjd: look speeds (from zdoom)
   fixed_t jumpspeed;

   // original speeds - before turbo is applied.
   fixed_t oforwardmove[2];
   fixed_t osidemove[2];

   fixed_t speedfactor;

   // reborn inventory
   unsigned int       numrebornitems;
   reborninventory_t *rebornitems;

   // weaponslots
   weaponslot_t *weaponslots[NUMWEAPONSLOTS];
   bool          hasslots;

   // flags
   unsigned flags;

   // hashing data
   char mnemonic[129];
   playerclass_t *next;
};

playerclass_t *E_PlayerClassForName(const char *);

void E_VerifyDefaultPlayerClass(void);
bool E_IsPlayerClassThingType(mobjtype_t);
bool E_PlayerInWalkingState(player_t *);
void E_ApplyTurbo(int ts);

bool E_CanJump(const playerclass_t &pclass);
bool E_MayJumpIfOverriden(const playerclass_t &pclass);

// EDF-only stuff
#ifdef NEED_EDF_DEFINITIONS

constexpr const char EDF_SEC_SKIN[] = "skin";
extern cfg_opt_t edf_skin_opts[];

constexpr const char EDF_SEC_PCLASS[] = "playerclass";
constexpr const char EDF_SEC_PDELTA[] = "playerdelta";
extern cfg_opt_t edf_pclass_opts[];
extern cfg_opt_t edf_pdelta_opts[];

void E_ProcessSkins(cfg_t *cfg);
void E_ProcessPlayerClasses(cfg_t *cfg);
void E_ProcessPlayerDeltas(cfg_t *cfg);

void E_ProcessFinalWeaponSlots();

#endif // NEED_EDF_DEFINITIONS

#endif

// EOF

