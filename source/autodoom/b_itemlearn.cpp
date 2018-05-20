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

#include "../g_dmflag.h"
#include "../doomstat.h"
#include "../e_inventory.h"
#include "../e_lib.h"
#include "../e_weapons.h"
#include "../metaapi.h"

inline static bool B_checkWasted(int current, int added, int max)
{
   return current + added <= max + added / 3;
}

bool B_CheckArmour(const player_t *pl, const itemeffect_t *effect)
{
   if(!effect)
      return false;

   int hits = effect->getInt("saveamount", -1);
   int savefactor = effect->getInt("savefactor", 1);
   int savedivisor = effect->getInt("savedivisor", 3);
   int maxsaveamount = effect->getInt("maxsaveamount", 0);
   bool additive = !!effect->getInt("additive", 0);
   bool setabsorption = !!effect->getInt("setabsorption", 0);

   if(hits < 0 || !savefactor || !savedivisor)
      return false;

   if(!effect->getInt("alwayspickup", 0) &&
      (pl->armorpoints >= (additive ? maxsaveamount : hits) ||
       (!hits && (!pl->armorfactor || !setabsorption))))
   {
      return false;
   }

   if(additive)
      return B_checkWasted(pl->armorpoints, hits, maxsaveamount);

   // FIXME: potential overflow in insane mods
   return (savedivisor ? hits * savefactor / savedivisor : 0) >
   (pl->armordivisor ?
    pl->armorpoints * pl->armorfactor / pl->armordivisor : 0);
}

bool B_CheckBody(const player_t *pl, const itemeffect_t *effect)
{
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

//
// Checks if a given power should be pursued by bot.
//
bool B_CheckPowerForItem(const player_t *pl, const itemeffect_t *power)
{
   const char *powerStr = power->getString("type", "");
   if(estrempty(powerStr))
      return false;
   int powerNum;
   if((powerNum = E_StrToNumLinear(powerStrings, NUMPOWERS, powerStr)) ==
      NUMPOWERS)
   {
      return false;
   }
   if(!power->getInt("overridesself", 0) && pl->powers[powerNum] > 4 * 32)
      return false;

   int duration = power->getInt("duration", 0);
   bool additiveTime = false;
   if(power->getInt("permanent", 0))
      duration = -1;
   else
   {
      duration *= TICRATE;
      additiveTime = power->getInt("additivetime", 0) != 0;
   }

   static const int expireTime = 5 * TICRATE;
   return pl->powers[powerNum] >= 0 && pl->powers[powerNum] < expireTime;
}

bool B_CheckInventoryItem(const player_t *pl, const itemeffect_t *artifact,
                          int amount)
{
   if(!artifact)
      return false;

   itemeffecttype_t fxtype;
   inventoryitemid_t itemid;
   int amountToGive;
   int maxAmount;
   int fullAmount;

   if(!E_GetInventoryItemDetails(pl, artifact, fxtype, itemid, amountToGive,
                                 maxAmount, fullAmount))
   {
      return false;
   }

   if(fxtype != ITEMFX_ARTIFACT || itemid < 0)
      return false;

   if(amount > 0)
      amountToGive = amount;

   const inventoryslot_t *slot = E_InventorySlotForItemID(pl, itemid);

   inventoryindex_t newSlot = -1;
   if(!slot)
   {
      if((newSlot = E_FindInventorySlot(pl->inventory)) < 0)
         return false;
      slot = &pl->inventory[newSlot];
   }

   if(fullAmount && slot->amount + amountToGive > maxAmount)
      return false;

   return B_checkWasted(slot->amount, amountToGive, maxAmount);
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

   return B_CheckInventoryItem(pl, ammo, num);
}

bool B_CheckAmmoPickup(const player_t *pl, const itemeffect_t *pickup,
                       bool dropped, int dropamount)
{
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

//
// Demo-compatible version
//
static bool B_checkWeaponCompat(const player_t *pl, const itemeffect_t *giver,
                                bool dropped, const Mobj *special)
{
   const weaponinfo_t *wp = E_WeaponForName(giver->getString("weapon", ""));
   const itemeffect_t *ammogiven = nullptr;
   ammogiven = giver->getNextKeyAndTypeEx(ammogiven, "ammogiven");

   const itemeffect_t *ammo;
   int giveammo, dropammo, dmstayammo, coopstayammo;
   if(ammogiven)
   {
      ammo = E_ItemEffectForName(ammogiven->getString("type", ""));

      giveammo = ammogiven->getInt("ammo.give", -1);
      if((dropammo = ammogiven->getInt("ammo.dropped", -1)) < 0)
         dropammo = giveammo;
      if((dmstayammo = ammogiven->getInt("ammo.dmstay", -1)) < 0)
         dmstayammo = giveammo;
      if((coopstayammo = ammogiven->getInt("ammo.coopstay", -1)) < 0)
         coopstayammo = giveammo;
   }
   else
   {
      ammo = nullptr;
      giveammo = dropammo = dmstayammo = coopstayammo = 0;
   }

   if(!E_PlayerOwnsWeapon(pl, wp))
      return true;

   // weapon owned

   if(((dmflags & DM_WEAPONSTAY) && !dropped) || !ammo)
      return false;  // if owned, it won't be picked up. Also ignore if no ammo

   return B_checkAmmo(pl, ammo, dropped ? dropammo : giveammo);
}

//
// Checks that a weapon is worth picking up
//
bool B_CheckWeapon(const player_t *pl, const itemeffect_t *giver, bool dropped,
                   const Mobj *special)
{
   if(demo_version < 401)
      return B_checkWeaponCompat(pl, giver, dropped, special);

   const weaponinfo_t *wp = E_WeaponForName(giver->getString("weapon", ""));
   if(!wp)
      return false;
   if(!E_PlayerOwnsWeapon(pl, wp))
      return true;
   if((dmflags & DM_WEAPONSTAY) && !dropped)
      return false;
   const itemeffect_t *ammogiven = nullptr;
   while((ammogiven = giver->getNextKeyAndTypeEx(ammogiven, "ammogiven")))
   {
      const itemeffect_t *ammo = nullptr;
      int giveammo = 0;
      int dropammo = 0;
      if(ammogiven)
      {
         if(!(ammo = E_ItemEffectForName(ammogiven->getString("type", ""))))
            return false;
         if((giveammo = ammogiven->getInt("ammo.give", -1)) < 0)
            return false;
         if((dropammo = ammogiven->getInt("ammo.dropped", -1)) < 0)
            dropammo = giveammo;
      }
      // at this point, weapon is not staying
      const int amount = dropped ? dropammo : giveammo;
      if(ammo && amount && B_checkAmmo(pl, ammo, amount))
         return true;
   }
   return false;
}

// EOF

