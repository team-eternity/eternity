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
#include "../metaapi.h"

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
#warning Need to update this to new weapon system
#if 0
   if(!pl->weaponowned[weapon])
      return true;

   if((dmflags & DM_WEAPONSTAY) && !dropped)
      return false;

   const weaponinfo_t *wp = &weaponinfo[weapon];
   if(!wp->ammo)
      return false;

   int amount = dropped ? wp->dropammo : wp->giveammo;
   return B_checkAmmo(pl, wp->ammo, amount);
#endif
   return false;
}

// EOF

