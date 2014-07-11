// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2014 Ioan Chera
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Learn item effects by trying them.
//
//-----------------------------------------------------------------------------

#include "../z_zone.h"

#include "../e_inventory.h"
#include "b_itemlearn.h"


//
// Copy constructor
//
PlayerStats::Values::Values(const Values &other) : health(other.health),
armorpoints(other.armorpoints), armortype(other.armortype),
itemcount(other.itemcount), inv_size(other.inv_size)
{
   memcpy(powers, other.powers, sizeof(powers));
   memcpy(weaponowned, other.weaponowned, sizeof(weaponowned));
   inventory = estructalloc(inventoryslot_t, inv_size);
   memcpy(inventory, other.inventory, inv_size * sizeof(*inventory));
}

//
// Move constructor
//
PlayerStats::Values::Values(Values &&other) : health(other.health),
armorpoints(other.armorpoints), armortype(other.armortype),
itemcount(other.itemcount), inventory(other.inventory),
inv_size(other.inv_size)
{
   memcpy(powers, other.powers, sizeof(powers));
   memcpy(weaponowned, other.weaponowned, sizeof(weaponowned));
   
   // move happens here
   other.inventory = nullptr;
   other.inv_size = 0;
}

//
// PlayerStats::Values::reset
//
// Resets the values
//
void PlayerStats::Values::reset(bool maxOut)
{
   int sup = maxOut ? INT_MAX : 0;
   fixed_t fsup = maxOut ? D_MAXINT : 0;
   health = sup;
   armorpoints = sup;
   armortype = fsup;
   itemcount = sup;
   
   int i;
   for(i = 0; i < NUMPOWERS; ++i)
      powers[i] = sup;
   for(i = 0; i < NUMWEAPONS; ++i)
      weaponowned[i] = sup;
   inventory = estructalloc(inventoryslot_t, inv_size = e_maxitemid);
   for(i = 0; i < e_maxitemid; ++i)
   {
      inventory[i].item = i;
      inventory[i].amount = sup;
   }
}

//
// PlayerStats::reduceByCurrentState
//
// Reduces the stored values to the minimum of player's current
//
void PlayerStats::reduceByCurrentState(const player_t &pl)
{
   if(pl.health < data.health)
      data.health = pl.health;
   if(pl.armorpoints < data.armorpoints)
      data.armorpoints = pl.armorpoints;
   fixed_t plarmor = pl.armordivisor ? FRACUNIT * pl.armorfactor / pl.armordivisor :
   0;
   if(plarmor < data.armortype)
      data.armortype = plarmor;
   int i;
   int plpowers;
   for(i = 0; i < NUMPOWERS; ++i)
   {
      plpowers = pl.powers[i] == -1 ? INT_MAX : pl.powers[i];
      if(plpowers < data.powers[i])
         data.powers[i] = plpowers;
   }
   for(i = 0; i < NUMWEAPONS; ++i)
   {
      if(pl.weaponowned[i] < data.weaponowned[i])
         data.weaponowned[i] = pl.weaponowned[i];
   }
   if(pl.itemcount < data.itemcount)
      data.itemcount = pl.itemcount;
   for(i = 0; i < e_maxitemid; ++i)
   {
      if(pl.inventory[i].item != -1 && pl.inventory[i].amount <
         data.inventory[pl.inventory[i].item].amount)
      {
         data.inventory[pl.inventory[i].item].amount = pl.inventory[i].amount;
      }
   }
}

//
// PlayerStats::getPriorState
//
// Gets the current player state in the 'prior' struct member
//
void PlayerStats::setPriorState(const player_t &pl)
{
   prior.health = pl.health;
   prior.armorpoints = pl.armorpoints;
   prior.armortype = pl.armordivisor ? FRACUNIT * pl.armorfactor / pl.armordivisor :
   0;
   int i;
   for(i = 0; i < NUMPOWERS; ++i)
      prior.powers[i] = pl.powers[i];
   for(i = 0; i < NUMWEAPONS; ++i)
      prior.weaponowned[i] = pl.weaponowned[i];
   prior.itemcount = pl.itemcount;
   for(i = 0; i < e_maxitemid; ++i)
   {
      if(pl.inventory[i].item != -1)
         prior.inventory[pl.inventory[i].item].amount = pl.inventory[i].amount;
   }
}

//
// PlayerStats::maximizeByStateDelta
//
// Increases the gain
//
void PlayerStats::maximizeByStateDelta(const player_t &pl)
{
   int delta;
   fixed_t fdelta;
   delta = pl.health - prior.health;
   if(delta > data.health)
      data.health = delta;
   delta = pl.armorpoints - prior.armorpoints;
   if(delta > data.armorpoints)
      data.armorpoints = delta;
   fixed_t plarmor = pl.armordivisor ? FRACUNIT * pl.armorfactor / pl.armordivisor :
   0;
   fdelta = plarmor - prior.armortype;
   if(fdelta > data.armortype)
      data.armortype = fdelta;
   int i, plpowers;
   for(i = 0; i < NUMPOWERS; ++i)
   {
      plpowers = pl.powers[i] == -1 ? INT_MAX : pl.powers[i];
      delta = plpowers - prior.powers[i];
      if(delta > data.powers[i])
         data.powers[i] = delta;
   }
   for(i = 0; i < NUMWEAPONS; ++i)
   {
      delta = pl.weaponowned[i] - prior.weaponowned[i];
      if(delta > data.weaponowned[i])
         data.weaponowned[i] = delta;
   }
   delta = pl.itemcount - prior.itemcount;
   if(delta > data.itemcount)
      data.itemcount = delta;
   for(i = 0; i < e_maxitemid; ++i)
   {
      if(pl.inventory[i].item != -1)
      {
         delta = pl.inventory[i].amount - prior.inventory[pl.inventory[i].item].amount;
         if(delta > data.inventory[pl.inventory[i].item].amount)
            data.inventory[pl.inventory[i].item].amount = delta;
      }
   }
}

//
// PlayerStats::greaterThan
//
// Returns true if one of the stored values is greater than its equivalent
// player value. Used to lower the minimum required stats for items
//
bool PlayerStats::greaterThan(player_t &pl) const
{
   fixed_t plarmor = pl.armordivisor ? FRACUNIT * pl.armorfactor /
   pl.armordivisor : 0;
   
   int i;
   for(i = 0; i < NUMPOWERS; ++i)
      if(pl.powers[i] != -1 && data.powers[i] > pl.powers[i])
         return true;
   for(i = 0; i < NUMWEAPONS; ++i)
      if(data.weaponowned[i] > pl.weaponowned[i])
         return true;
   
   inventoryslot_t *islot;
   for(i = 0; i < e_maxitemid; ++i)
   {
      islot = E_InventorySlotForItemID(&pl, i);
      if((!islot && data.inventory[i].amount > 0) ||
         data.inventory[i].amount > islot->amount)
         return true;
   }
   return data.health > pl.health || data.armorpoints > pl.armorpoints ||
   data.armortype > plarmor || data.itemcount > pl.itemcount;
}

//
// PlayerStats::fillsGap
//
// Returns true if player pl would benefit from this additive PlayerStats
//
bool PlayerStats::fillsGap(player_t &pl, const PlayerStats &cap) const
{
   fixed_t plarmor = pl.armordivisor ? FRACUNIT * pl.armorfactor /
   pl.armordivisor : 0;
   
   if(data.health && pl.health < cap.data.health)
      return true;
   if(data.armorpoints && pl.armorpoints < cap.data.armorpoints)
      return true;
   if(data.armortype && plarmor < cap.data.armortype)
      return true;
   int i;
   int plpower;
   for(i = 0; i < NUMPOWERS; ++i)
   {
      plpower = pl.powers[i] == -1 ? INT_MAX : pl.powers[i];
      if(data.powers[i] && plpower < cap.data.powers[i])
         return true;
   }
   
   for(i = 0; i < NUMWEAPONS; ++i)
      if(data.weaponowned[i] && pl.weaponowned[i] < cap.data.weaponowned[i])
         return true;
   
   if(data.itemcount && pl.itemcount < cap.data.itemcount)
      return true;
   
   inventoryslot_t *islot;
   for(i = 0; i < e_maxitemid; ++i)
   {
      islot = E_InventorySlotForItemID(&pl, i);
      if(data.inventory[i].amount &&
         (!islot || islot->amount < cap.data.inventory[i].amount))
         return true;
   }
   
   return false;
}

//
// PlayerStats::overlaps
//
// Pretty much the reverse of the operation above.
//
bool PlayerStats::overlaps(player_t &pl, const PlayerStats &cap) const
{
   fixed_t plarmor = pl.armordivisor ? FRACUNIT * pl.armorfactor /
   pl.armordivisor : 0;
   
   if(data.health && pl.health > cap.data.health)
      return true;
   if(data.armorpoints && pl.armorpoints > cap.data.armorpoints)
      return true;
   if(data.armortype && plarmor > cap.data.armortype)
      return true;
   int i;
   int plpower;
   for(i = 0; i < NUMPOWERS; ++i)
   {
      plpower = pl.powers[i] == -1 ? INT_MAX : pl.powers[i];
      if(data.powers[i] && plpower > cap.data.powers[i])
         return true;
   }
   
   for(i = 0; i < NUMWEAPONS; ++i)
      if(data.weaponowned[i] && pl.weaponowned[i] > cap.data.weaponowned[i])
         return true;
   
   if(data.itemcount && pl.itemcount > cap.data.itemcount)
      return true;
   
   inventoryslot_t *islot;
   for(i = 0; i < e_maxitemid; ++i)
   {
      islot = E_InventorySlotForItemID(&pl, i);
      if(islot && islot->amount > cap.data.inventory[i].amount)
         return true;
   }
   
   return false;
}

// EOF

