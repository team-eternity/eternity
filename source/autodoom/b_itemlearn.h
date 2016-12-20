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

#ifndef __EternityEngine__b_itemlearn__
#define __EternityEngine__b_itemlearn__

#include "../../rapidjson/document.h"
#include "../d_player.h"
#include "../e_inventory.h"

//
// PlayerStatDiff
//
// Used to get the difference of player stats after bot picks up an item
//
class PlayerStats : public ZoneObject
{
   //
   // Values
   //
   // Internal class that holds the values
   //
   class Values : public ZoneObject
   {
   public:
      int health;
      int armorpoints;
      fixed_t armortype;
      int powers[NUMPOWERS];
      int weaponowned[NUMWEAPONS];
      int itemcount;
      inventory_t inventory;
      int inv_size;
      
      Values(bool maxOut) : inventory(nullptr)
      {
         reset(maxOut);
      }
      Values(const Values &other);
      Values(Values &&other);
      Values(const rapidjson::Value& json, bool maxOutFallback);
      //
      // Destructor
      //
      ~Values()
      {
         efree(inventory);
      }
      
      void reset(bool maxOut);

      rapidjson::Value makeJson(rapidjson::Document::AllocatorType& allocator) const;
   };
   
   Values data;
   Values prior;
   
public:
   //
   // Constructor
   //
   PlayerStats(bool maxOut) : data(maxOut), prior(false)
   {
   }
   PlayerStats(const rapidjson::Value& json, bool maxOutFallback);
   
   void reduceByCurrentState(const player_t &pl);
   void setPriorState(const player_t &pl);
   void maximizeByStateDelta(const player_t &pl);
   
   bool greaterThan(player_t &pl) const;
   bool fillsGap(player_t &pl, const PlayerStats &cap) const;
   bool overlaps(player_t &pl, const PlayerStats &cap) const;

   rapidjson::Value makeJson(rapidjson::Document::AllocatorType& allocator) const;
   
   //
   // reset
   //
   // Sets the values to the initial ones
   //
   void reset(bool maxOut)
   {
      data.reset(maxOut);
   }
};

bool B_CheckArmour(const player_t *pl, const char *effectname);
bool B_CheckBody(const player_t *pl, const char *effectname);
bool B_CheckCard(const player_t *pl, const char *effectname);
bool B_CheckPower(const player_t *pl, int power);
bool B_CheckAmmoPickup(const player_t *pl, const char *effectname, bool dropped,
                       int dropamount);
bool B_CheckBackpack(const player_t *pl);
bool B_CheckWeapon(const player_t *pl, weapontype_t weapon, bool dropped);

#endif /* defined(__EternityEngine__b_itemlearn__) */
