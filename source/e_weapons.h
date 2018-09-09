//
// Copyright (C) 2018 James Haley, Max Waine, et al.
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
// Purpose: Dynamic Weapons System
// Authors: James Haley, Max Waine
//

#ifndef E_WEAPONS_H__
#define E_WEAPONS_H__

#include "d_keywds.h"
#include "m_avltree.h"
#include "m_bdlist.h"

struct weaponinfo_t;
struct cfg_t;

// Global Data

extern int NUMWEAPONTYPES;

extern weapontype_t UnknownWeaponInfo;

#ifdef NEED_EDF_DEFINITIONS

// Section Names
#define EDF_SEC_WEAPONINFO "weaponinfo"
#define EDF_SEC_WPNDELTA   "weapondelta"

// Section Options
extern cfg_opt_t edf_wpninfo_opts[];
extern cfg_opt_t edf_wdelta_opts[];

#endif


// Classes

using SelectOrderTreeBase = AVLTree<int, weaponinfo_t>;
using SelectOrderNode = SelectOrderTreeBase::avlnode_t;

// The class that overrides parts of the base AVL tree used for
// checking selection order. It's used due to its speed of access
// over red-black trees, at the cost of slower mutation times.
class SelectOrderTree : public SelectOrderTreeBase
{
public:
   // Default constructor
   SelectOrderTree() : SelectOrderTreeBase()
   {
   }

   // Parameterized constructor
   SelectOrderTree(int key, weaponinfo_t *object) : SelectOrderTreeBase(key, object)
   {
   }

protected:
   //
   // This version of handleCollision add the new entry in alphabetically,
   // unless a node with the same object is found.
   //
   avlnode_t *handleCollision(avlnode_t *listroot, avlnode_t *toinsert) override
   {
      avlnode_t *curr;
      if(strcasecmp(toinsert->object->name, listroot->object->name) < 0)
      {
         // We need to put the new node at the start of the list.
         weaponinfo_t *object = listroot->object;
         listroot->object = toinsert->object;
         toinsert->object = object;
         toinsert->next = listroot->next;
         listroot->next = toinsert;
         return toinsert;
      }

      for(curr = listroot; curr->next != nullptr && curr != toinsert; curr = curr->next)
      {
         if(strcasecmp(toinsert->object->name, curr->object->name) > 0 &&
            strcasecmp(toinsert->object->name, curr->next->object->name) < 0)
         {
            // We need to put the new node in the middle of the list.
            toinsert->next = curr->next;
            curr->next = toinsert;
            return toinsert;
         }
      }

      if(curr->object == toinsert->object)
      {
         efree(toinsert);
         return nullptr; // Ahh bugger
      }
      else
      {
         curr->next = toinsert;
         return toinsert;
      }
   }
};

using WeaponSlotTree = SelectOrderTree;
using WeaponSlotNode = WeaponSlotTree::avlnode_t;

#define NUMWEAPCOUNTERS 3
using WeaponCounter = int[NUMWEAPCOUNTERS];
using WeaponCounterTreeBase = AVLTree<int, WeaponCounter>;
using WeaponCounterNode = WeaponCounterTreeBase::avlnode_t;

// Tree of weapon counters
class WeaponCounterTree : public WeaponCounterTreeBase
{
public:
   // Default constructor
   WeaponCounterTree() : WeaponCounterTreeBase()
   {
      deleteobjects = true;
   }

   //
   // Set the indexed counter for the player's currently equipped weapon to value
   //
   void setCounter(player_t *player, int index, int value)
   {
      WeaponCounter &counters = getCounters(player->readyweapon->id);
      counters[index] = value;
   }

   //
   // Get counters for a given weapon.
   // If the counters don't exist then create them
   //
   WeaponCounter &getCounters(int weaponid)
   {
      WeaponCounterNode *ctrnode;
      if((ctrnode = find(weaponid)))
         return *ctrnode->object;
      else
      {
         // We didn't find the counter we want, so make a new one
         WeaponCounter &counters = *estructalloc(WeaponCounter, 1);
         insert(weaponid, &counters);
         return counters;
      }
   }

   //
   // Get the index weapon counter
   //
   int *getIndexedCounter(int weaponid, int index)
   {
      WeaponCounter &counter = getCounters(weaponid);
      return &counter[index];
   }

   //
   // Get counter pointer for the player's currently equipped weapon
   //
   static int *getIndexedCounterForPlayer(const player_t *player, int index)
   {
      return player->weaponctrs->getIndexedCounter(player->readyweapon->id, index);
   }
};

// Structures
struct weaponslot_t
{
   weaponinfo_t *weapon;           // weapon in the slot
   uint8_t       slotindex;        // index of the slot
   BDListItem<weaponslot_t> links; // link to next weapon in the same slot
};

extern WeaponSlotTree *weaponslots[];

// Global Functions
weaponinfo_t *E_WeaponForID(const int id);
weaponinfo_t *E_WeaponForName(const char *name);
weaponinfo_t *E_WeaponForDEHNum(const int dehnum);
weapontype_t E_WeaponNumForName(const char *name);

weaponinfo_t *E_FindBestWeapon(const player_t *player);
weaponinfo_t *E_FindBestWeaponUsingAmmo(const player_t *player,
                                        const itemeffect_t *ammo);
int *E_GetIndexedWepCtrForPlayer(const player_t *player, int index);

bool E_WeaponIsCurrentDEHNum(const player_t *player, const int dehnum);

bool E_PlayerOwnsWeapon(const player_t *player, const weaponinfo_t *weapon);
bool E_PlayerOwnsWeaponForDEHNum(const player_t *player, const int dehnum);
bool E_PlayerOwnsWeaponInSlot(const player_t *player, const int slot);

state_t *E_GetStateForWeaponInfo(const weaponinfo_t *wi, const char *label);

bool E_WeaponHasAltFire(const weaponinfo_t *wp);

void E_GiveWeapon(player_t *player, const weaponinfo_t *weapon);

void E_GiveAllClassWeapons(player_t *player);

bool E_IsPoweredVariant(const weaponinfo_t *wp);
bool E_IsPoweredVariantOf(const weaponinfo_t *untomed, const weaponinfo_t *tomed);

BDListItem<weaponslot_t> *E_FirstInSlot(const weaponslot_t *dummyslot);
BDListItem<weaponslot_t> *E_LastInSlot(const weaponslot_t *dummyslot);

weaponslot_t *E_FindEntryForWeaponInSlotIndex(const player_t *player, const weaponinfo_t *wp,
                                              const int slotindex);
weaponslot_t *E_FindFirstWeaponSlot(const player_t *player, const weaponinfo_t *wp);

void E_CollectWeapons(cfg_t *cfg);

void E_ProcessWeaponInfo(cfg_t *cfg);
void E_ProcessWeaponDeltas(cfg_t *cfg);

#endif

// EOF

