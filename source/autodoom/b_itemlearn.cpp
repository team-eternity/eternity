// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2015 Ioan Chera
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
#include "../../rapidjson/document.h"

#include "b_itemlearn.h"
#include "b_util.h"
#include "../g_dmflag.h"
#include "../doomstat.h"
#include "../e_inventory.h"
#include "../metaapi.h"
#include "../r_pcheck.h"

#define JSON_HEALTH "health"
#define JSON_ARMORPOINTS "armorpoints"
#define JSON_ARMORTYPE "armortype"
#define JSON_POWERS "powers"
#define JSON_WEAPONOWNED "weaponowned"
#define JSON_ITEMCOUNT "itemcount"
#define JSON_INVENTORY_ITEM "item"
#define JSON_INVENTORY_AMOUNT "amount"
#define JSON_INVENTORY "inventory"

#define JSON_VALUES_DATA "data"
#define JSON_VALUES_PRIOR "prior"

#define JSON_FIND_ITEM(key, valueType) \
    it = json.FindMember(key); \
    if(it == json.MemberEnd() || !it->value.valueType()) \
        goto fallback

//
// Copy constructor
//
PlayerStats::Values::Values(const Values &other) : 
health(other.health),
armorpoints(other.armorpoints), 
armortype(other.armortype),
itemcount(other.itemcount), 
inv_size(other.inv_size)
{
   memcpy(powers, other.powers, sizeof(powers));
   memcpy(weaponowned, other.weaponowned, sizeof(weaponowned));

   inventory = estructalloc(inventoryslot_t, inv_size);
   memcpy(inventory, other.inventory, inv_size * sizeof(*inventory));
}

//
// Move constructor
//
PlayerStats::Values::Values(Values &&other) : 
health(other.health),
armorpoints(other.armorpoints), 
armortype(other.armortype),
itemcount(other.itemcount), 
inventory(other.inventory),
inv_size(other.inv_size)
{
   memcpy(powers, other.powers, sizeof(powers));
   memcpy(weaponowned, other.weaponowned, sizeof(weaponowned));
   
   // move happens here
   other.inventory = nullptr;
   other.inv_size = 0;
}

//
// JSON constructor
//
// Make sure we start with a zero inventory before any fallback/reset
//
PlayerStats::Values::Values(const rapidjson::Value& json, bool maxOutFallback) : inventory(nullptr)
{
    {
        if (!json.IsObject())
            goto fallback;

        auto JSON_FIND_ITEM(JSON_HEALTH, IsInt);
        this->health = it->value.GetInt();

        JSON_FIND_ITEM(JSON_ARMORPOINTS, IsInt);
        this->armorpoints = it->value.GetInt();

        JSON_FIND_ITEM(JSON_ARMORTYPE, IsInt);
        this->armortype = it->value.GetInt();

        JSON_FIND_ITEM(JSON_POWERS, IsArray);
        if (it->value.Capacity() != earrlen(this->powers))
            goto fallback;
        int *pointer = this->powers;
        for (auto ait = it->value.Begin(); ait != it->value.End(); ++ait)
        {
            if (!ait->IsInt())
                goto fallback;
            *pointer++ = ait->GetInt();
        }

        JSON_FIND_ITEM(JSON_WEAPONOWNED, IsArray);
        if (it->value.Capacity() != earrlen(this->weaponowned))
            goto fallback;
        pointer = this->weaponowned;
        for (auto ait = it->value.Begin(); ait != it->value.End(); ++ait)
        {
            if (!ait->IsInt())
                goto fallback;
            *pointer++ = ait->GetInt();
        }

        JSON_FIND_ITEM(JSON_ITEMCOUNT, IsInt);
        this->itemcount = it->value.GetInt();

        JSON_FIND_ITEM(JSON_INVENTORY, IsArray);
        this->inv_size = static_cast<int>(it->value.Capacity());
        this->inventory = estructalloc(inventoryslot_t, this->inv_size);
        inventoryslot_t* sptr = this->inventory;
        for (auto ait = it->value.Begin(); ait != it->value.End(); ++ait)
        {
            if (!ait->IsObject())
                goto fallback;
            auto bit = ait->FindMember(JSON_INVENTORY_ITEM);
            if (bit == ait->MemberEnd() || !bit->value.IsInt())
                goto fallback;
            sptr->item = bit->value.GetInt();
            bit = ait->FindMember(JSON_INVENTORY_AMOUNT);
            if (bit == ait->MemberEnd() || !bit->value.IsInt())
                goto fallback;
            sptr->amount = bit->value.GetInt();
            ++sptr;
        }
    }


    return;
fallback:
    B_Log("Failed reading PlayerStats::Values from JSON!");
    reset(maxOutFallback);
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
   efree(inventory);
   inventory = estructalloc(inventoryslot_t, inv_size = e_maxitemid);
   for(i = 0; i < e_maxitemid; ++i)
   {
      inventory[i].item = i;
      inventory[i].amount = sup;
   }
}

//
// PlayerStats::Values::makeJson
//
// Returns a JSON object from this set of values
//
rapidjson::Value PlayerStats::Values::makeJson(rapidjson::Document::AllocatorType& allocator) const
{
    rapidjson::Value json(rapidjson::kObjectType);

    json.AddMember(JSON_HEALTH, this->health, allocator);
    json.AddMember(JSON_ARMORPOINTS, this->armorpoints, allocator);
    json.AddMember(JSON_ARMORTYPE, this->armortype, allocator);

    rapidjson::Value powersJson(rapidjson::kArrayType);
    for (int power : this->powers)
    {
        powersJson.PushBack(power, allocator);
    }

    json.AddMember(JSON_POWERS, powersJson, allocator);

    rapidjson::Value weaponsJson(rapidjson::kArrayType);
    for (int weapon : weaponowned)
    {
        weaponsJson.PushBack(weapon, allocator);
    }

    json.AddMember(JSON_WEAPONOWNED, weaponsJson, allocator);
    json.AddMember(JSON_ITEMCOUNT, this->itemcount, allocator);

    rapidjson::Value inventoryJson(rapidjson::kArrayType);
    for (int i = 0; i < this->inv_size; ++i)
    {
        rapidjson::Value inventoryItem(rapidjson::kObjectType);

        // Statically cast the fields to int because they might change at any
        // moment into different types from the main code, and the best we can
        // do here is prevent accidental drastic casts from pointers and whatnot.
        inventoryItem.AddMember(JSON_INVENTORY_ITEM, static_cast<int>(this->inventory[i].item), allocator);
        inventoryItem.AddMember(JSON_INVENTORY_AMOUNT, static_cast<int>(this->inventory[i].amount), allocator);

        inventoryJson.PushBack(inventoryItem, allocator);
    }

    json.AddMember(JSON_INVENTORY, inventoryJson, allocator);

    return json;
}

//
// JSON constructor
//
// For each object, if any of the JSONs is invalid, or empty, it will make it
// as new
//
PlayerStats::PlayerStats(const rapidjson::Value& json, bool maxOutFallback) : 
data(B_OptJsonObject(json, JSON_VALUES_DATA), maxOutFallback), 
prior(B_OptJsonObject(json, JSON_VALUES_PRIOR), false)
{
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

//
// PlayerStats::makeJson
//
rapidjson::Value PlayerStats::makeJson(rapidjson::Document::AllocatorType& allocator) const
{
    auto dataJson = data.makeJson(allocator);
    auto priorJson = prior.makeJson(allocator);

    rapidjson::Value json(rapidjson::kObjectType);
    json.AddMember(JSON_VALUES_DATA, dataJson, allocator);
    json.AddMember(JSON_VALUES_PRIOR, priorJson, allocator);
    return json;
}

////////////////////////////////////////////////////////////////////////////////

inline static bool B_checkWasted(int current, int added, int max)
{
   return current + added <= max + added / 3;
}

bool B_CheckArmour(const player_t *pl, const char *effectname)
{
   const itemeffect_t *effect = E_ItemEffectForName(effectname);
   if(!effect)
      return false;

   int hits = effect->getInt("saveamount", 0);
   int savefactor = effect->getInt("savefactor", 1);
   int savedivisor = effect->getInt("savedivisor", 3);

   if(!hits || !savefactor || !savedivisor)
      return false;

   if(!effect->getInt("alwayspickup", 0) && pl->armorpoints >= hits)
      return false;

   if(effect->getInt("bonus", 0))
   {
      int maxsaveamount = effect->getInt("maxsaveamount", 0);
      return B_checkWasted(pl->armorpoints, hits, maxsaveamount);
   }

   // FIXME: potential overflow in insane mods
   return (savedivisor ? hits * savefactor / savedivisor : 0) >
   (pl->armordivisor ?
    pl->armorpoints * pl->armorfactor / pl->armordivisor : 0);
}

bool B_CheckBody(const player_t *pl, const char *effectname)
{
   const itemeffect_t *effect = E_ItemEffectForName(effectname);
   if(!effect)
      return false;

   int amount = effect->getInt("amount", 0);
   int maxamount = effect->getInt("maxamount", 0);
   if(demo_version >= 200 && demo_version < 335)
   {
      if(effect->hasKey("compatmaxamount"))
         maxamount = effect->getInt("compatmaxamount", 0);
   }
   if(!effect->getInt("alwayspickup", 0) && pl->health >= maxamount)
      return false;

   if(effect->getInt("sethealth", 0))
      return pl->health < amount;
   return B_checkWasted(pl->health, amount, maxamount); // allow some

}

bool B_CheckCard(const player_t *pl, const char *effectname)
{
   const itemeffect_t *effect = E_ItemEffectForName(effectname);
   if(!effect)
      return false;

   return !E_GetItemOwnedAmount(pl, effect);
}

bool B_CheckPower(const player_t *pl, int power)
{
   static const int expireTime = 5 * TICRATE;

   switch(power)
   {
      case pw_invulnerability:   // FIXME: only when monsters are nearby
      case pw_invisibility:      // FIXME: only when certain monsters nearby
      case pw_totalinvis:        // FIXME: only when worth going stealth
      case pw_ghost:             // FIXME: only when certain monsters nearby
      case pw_ironfeet:          // FIXME: only when planning to travel slime
      case pw_infrared:          // FIXME: not necessary for bot
      case pw_torch:
         return pl->powers[power] >= 0 && pl->powers[power] < expireTime;
      case pw_allmap:
      case pw_silencer:
         return !pl->powers[power];
      case pw_strength:
         return !pl->powers[power] || B_CheckBody(pl, ITEMNAME_BERSERKHEALTH);
      case pw_flight:            // FIXME: only when planning to fly
         return pl->powers[power] >= 0 && pl->powers[power] <= 4 * 32;
      default:
         return false;
   }
}

static bool E_checkInventoryItem(const player_t *pl,
                                 const itemeffect_t *artifact,
                                 int amount)
{
   if(!artifact)
      return false;

   itemeffecttype_t fxtype;
   inventoryitemid_t itemid;
   int amountToGive;
   int fullAmount;

   E_GetInventoryItemDetails(artifact, fxtype, itemid, amountToGive, fullAmount);

   if(fxtype != ITEMFX_ARTIFACT || itemid < 0)
      return false;

   int maxAmount = E_GetMaxAmountForArtifact(pl, artifact);
   if(amount > 0)
      amountToGive = amount;

   const inventoryslot_t *slot = E_InventorySlotForItemID(pl, itemid);
   int slotAmount = slot ? slot->amount : 0;

   if(fullAmount && slotAmount + amountToGive > maxAmount)
      return false;

   return B_checkWasted(slotAmount, amountToGive, maxAmount);
}

static bool B_checkAmmo(const player_t *pl, const itemeffect_t *ammo, int num)
{
   if(!ammo)
      return false;

   int oldammo = E_GetItemOwnedAmount(pl, ammo);
   int maxamount = E_GetMaxAmountForArtifact(pl, ammo);

   if(oldammo == maxamount)
      return false;

   if(gameskill == sk_baby || gameskill == sk_nightmare)
      num <<= 1;

   return E_checkInventoryItem(pl, ammo, num);
}

bool B_CheckAmmoPickup(const player_t *pl, const char *effectname, bool dropped,
                       int dropamount)
{
   const itemeffect_t *pickup = E_ItemEffectForName(effectname);
   if(!pickup)
      return false;

   const itemeffect_t *give = E_ItemEffectForName(pickup->getString("ammo", ""));
   int giveamount = pickup->getInt("amount", 0);

   if(dropped)
   {
      if(dropamount)
         giveamount = dropamount;
      else
         giveamount = pickup->getInt("dropamount", giveamount);
   }

   return B_checkAmmo(pl, give, giveamount);
}

bool B_CheckBackpack(const player_t *pl)
{
   if(!E_PlayerHasBackpack(pl))
      return true;

   static MetaKeyIndex keyBackpackAmount("ammo.backpackamount");

   size_t numAmmo = E_GetNumAmmoTypes();
   for(size_t i = 0; i < numAmmo; ++i)
   {
      const itemeffect_t *ammoType = E_AmmoTypeForIndex(i);
      int giveamount = ammoType->getInt(keyBackpackAmount, 0);
      if(B_checkAmmo(pl, ammoType, giveamount))
         return true;
   }
   return false;
}

bool B_CheckWeapon(const player_t *pl, weapontype_t weapon, bool dropped)
{
   if(!pl->weaponowned[weapon])
      return true;

   if((dmflags & DM_WEAPONSTAY) && !dropped)
      return false;

   const weaponinfo_t *wp = &weaponinfo[weapon];
   if(!wp->ammo)
      return false;

   int amount = dropped ? wp->dropammo : wp->giveammo;
   return B_checkAmmo(pl, wp->ammo, amount);
}

// EOF

