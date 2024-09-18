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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//
// Various structures pertaining to the player.
//
//-----------------------------------------------------------------------------

#ifndef D_PLAYER_H__
#define D_PLAYER_H__

// The player data structure depends on a number
// of other structs: items (internal inventory),
// animation states (closely tied to the sprites
// used to represent them, unfortunately).

#include "p_pspr.h"

// In addition, the player is just a special
// case of the generic moving object/actor.

// Finally, for odd reasons, the player input
// is buffered within the player data struct,
// as commands per game tick.
#include "d_ticcmd.h"

// skins.
// haleyjd: player classes

struct playerclass_t;
struct skin_t;
struct weaponslot_t;

class WeaponCounterTree;

// Inventory item ID is just an integer. The inventory item type can be looked
// up using this.
typedef int inventoryitemid_t;

// Inventory index is an index into a player's inventory_t array. It is NOT
// the same as the item ID. You get from one to the other by using:
//   inventory[index].item
typedef int inventoryindex_t;

// Inventory Slot
// Every slot in an inventory is composed of this structure. It references the
// type of item in the slot via ID number, and tells how many of the item is
// being carried. That's all it does.
struct inventoryslot_t
{
   inventoryitemid_t item;   // The item.
   int               amount; // Amount possessed.
};

// Inventory
// An inventory is an array of inventoryslot structures, allocated at the max
// number of inventory items it is possible to carry (ie. the number of distinct
// inventory items defined).
typedef inventoryslot_t * inventory_t;

//
// Player states.
//
enum
{
   // Playing or camping.
   PST_LIVE,
   // Dead on the ground, view follows killer.
   PST_DEAD,
   // Ready to restart/respawn???
   PST_REBORN
};

typedef int playerstate_t;

//
// Player internal flags, for cheats and debug.
//
enum cheat_t
{
   // No clipping, walk through barriers.
   CF_NOCLIP           = 1,
   // No damage, no health loss.
   CF_GODMODE          = 2,
   // Not really a cheat, just a debug aid.
   CF_NOMOMENTUM       = 4,
   // haleyjd 03/18/03: infinite ammo
   CF_INFAMMO          = 8,
   // haleyjd 12/29/10: immortality cheat
   CF_IMMORTAL         = 0x10,
};


// TODO: Maybe re-add curpos
// The problem is adapting code to handle variable lengths of inventory bars.
struct invbarstate_t {
   bool inventory;  // inventory is currently being viewed?
   int  ArtifactFlash;
};

enum attacktype_e : unsigned int
{
   AT_NONE      = 0,
   AT_PRIMARY   = 1,
   AT_SECONDARY = 2,
   AT_ITEM      = 4, // temporarily ORed in, indicates not to subtract ammo
   AT_UNKNOWN   = 8,

   AT_ALL = (AT_PRIMARY + AT_SECONDARY),
};

struct powerduration_t
{
   int  tics;
   bool infinite;

   inline bool isActive() const { return tics != 0 || infinite; }
   inline bool shouldCount() const { return tics != 0 && !infinite; }
};

//
// Extended player object info: player_t
//
struct player_t
{
   Mobj          *mo;
   playerclass_t *pclass;      // haleyjd 09/27/07: player class
   skin_t        *skin;        // skin
   playerstate_t  playerstate; // live, dead, reborn, etc.
   ticcmd_t       cmd;         // current input

   // Determine POV,
   //  including viewpoint bobbing during movement.
  
   fixed_t        viewz;           // Focal origin above r.z  
   fixed_t        prevviewz;       // haleyjd 01/04/14: previous vewz, for interpolation
   fixed_t        viewheight;      // Base height above floor for viewz.  
   fixed_t        deltaviewheight; // Bob/squat speed.
   fixed_t        bob;             // bounded/scaled total momentum.
   fixed_t        pitch;           // haleyjd 04/03/05: true pitch angle
   fixed_t        prevpitch;       // MaxW 2016/08/02: Prev pitch angle, for iterpolation

   // killough 10/98: used for realistic bobbing (i.e. not simply overall speed)
   // mo->momx and mo->momy represent true momenta experienced by player.
   // This only represents the thrust that the player applies himself.
   // This avoids anomolies with such things as Boom ice and conveyors.
   fixed_t        momx, momy;      // killough 10/98

   int            health;       // This is only used between levels
   int            armorpoints;
   int            armorfactor;  // haleyjd 07/29/13: numerator for armor save calculation
   int            armordivisor; // haleyjd 07/29/13: denominator for armor save calculation

   // Power ups. invinc and invis are tic counters.
   powerduration_t powers[NUMPOWERS];
  
   // Frags, kills of other players.
   int            frags[MAXPLAYERS];
   int            totalfrags;
   
   weaponinfo_t  *readyweapon;
   weaponinfo_t  *pendingweapon; // Is nullptr if not changing.

   weaponslot_t  *readyweaponslot;
   weaponslot_t  *pendingweaponslot; // Is nullptr if not changing.

   // MaxW: 2018/01/02: Changed from `int weaponctrs[NUMWEAPONS][3]`
   WeaponCounterTree *weaponctrs; // haleyjd 03/31/06

   int            extralight;    // So gun flashes light up areas.
   
   attacktype_e   attackdown; // True if button down last tic.
   int            usedown;

   int            cheats;      // Bit flags, for cheats and debug.

   int            refire;      // Refired shots are less accurate.

   // For chicken
   int            chickenTics;   // player is a chicken if > 0
   int            headThrust;    // chicken peck countdown

   // For intermission stats.
   int            killcount;
   int            itemcount;
   int            secretcount;
   bool           didsecret;    // True if secret level has been done.
  
   // For screen flashing (red or bright).
   int            damagecount;
   int            bonuscount;
   int            fixedcolormap; // Current PLAYPAL, for pain etc.
   int            newtorch;      // haleyjd 08/31/13: change torch level?
   int            torchdelta;    // haleyjd 08/31/13: amount to change torch level

   Mobj          *attacker;      // Who did damage (nullptr for floors/ceilings).

   int            colormap;      // colorshift for player sprites

   // Overlay view sprites (gun, etc).
   pspdef_t       psprites[NUMPSPRITES];
  
   int            quake;         // If > 0, player is experiencing an earthquake
   int            jumptime;      // If > 0, player can't jump again yet
   int            flyheight;     // haleyjd 06/05/12: flying

   // Inventory
   inventory_t      inventory;   // haleyjd 07/06/13: player's inventory
   inventoryindex_t inv_ptr;     // MaxW: 2017/12/28: Player's currently selected item
   invbarstate_t    invbarstate; // MaxW: 2017/12/28: player's inventory bar state

   // Player name
   char           name[20];
};


//
// INTERMISSION
// Structure passed e.g. to WI_Start(wb)
//
struct wbplayerstruct_t
{
   bool        in;     // whether the player is in game

   // Player stats, kills, collected items etc.
   int         skills;
   int         sitems;
   int         ssecret;
   int         stime;
   int         frags[4];
   int         score;  // current score on entry, modified on return
};

struct wbstartstruct_t
{
   int         epsd;   // episode # (0-2)
   int         nextEpisode; // next episode (0-based) in case of custom level info

   // if true, splash the secret level
   bool        didsecret;

   // haleyjd: if player is going to secret map
   bool        gotosecret;

   // previous and next levels, origin 0
   int         last;
   int         next;
   bool        nextexplicit; // true if next was set by g_destmap

   // Explicit level-info stuff
   const char *li_lastlevelname;
   const char *li_nextlevelname;
   const char *li_lastlevelpic;
   const char *li_nextlevelpic;
   const char *li_lastexitpic;
   const char *li_nextenterpic;

   int         maxkills;
   int         maxitems;
   int         maxsecret;
   int         maxfrags;

   // the par time
   int         partime;

   // index of this player in game
   int         pnum;

   wbplayerstruct_t    plyr[MAXPLAYERS];
};


#endif

//----------------------------------------------------------------------------
//
// $Log: d_player.h,v $
// Revision 1.3  1998/05/04  21:34:15  thldrmn
// commenting and reformatting
//
// Revision 1.2  1998/01/26  19:26:31  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:07  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
