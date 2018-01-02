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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Handling interactions (i.e., collisions).
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "hal/i_gamepads.h"

#include "a_small.h"
#include "am_map.h"
#include "c_io.h"
#include "d_deh.h"     // Ty 03/22/98 - externalized strings
#include "d_dehtbl.h"
#include "d_gi.h"
#include "d_mod.h"
#include "d_player.h"
#include "doomstat.h"
#include "dstrings.h"
#include "e_edf.h"
#include "e_inventory.h"
#include "e_metastate.h"
#include "e_mod.h"
#include "e_states.h"
#include "e_things.h"
#include "ev_specials.h"
#include "g_demolog.h"
#include "g_dmflag.h"
#include "g_game.h"
#include "hu_frags.h"
#include "hu_stuff.h"
#include "m_random.h"
#include "metaapi.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_portalcross.h"
#include "p_skin.h"
#include "p_tick.h"
#include "p_user.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_segs.h"
#include "s_sound.h"
#include "sounds.h"
#include "v_misc.h"
#include "v_video.h"


#define BONUSADD        6

// Ty 03/07/98 - add deh externals
// Maximums and such were hardcoded values.  Need to externalize those for
// dehacked support (and future flexibility).  Most var names came from the key
// strings used in dehacked.

int god_health = 100;   // these are used in cheats (see st_stuff.c)

int bfgcells = 40;      // used in p_pspr.c
// Ty 03/07/98 - end deh externals

//
// GET STUFF
//

//
// P_GiveAmmo
//
// Returns false if the ammo can't be picked up at all
//
bool P_GiveAmmo(player_t *player, itemeffect_t *ammo, int num)
{
   if(!ammo)
      return false;

   // check if needed
   int oldammo   = E_GetItemOwnedAmount(player, ammo);
   int maxamount = E_GetMaxAmountForArtifact(player, ammo);

   if(oldammo == maxamount)
      return false;

   // give double ammo in trainer mode, you'll need in nightmare
   if(gameskill == sk_baby || gameskill == sk_nightmare)
      num <<= 1;

   if(!E_GiveInventoryItem(player, ammo, num))
      return false; // don't need this ammo
   
   // If non zero ammo, don't change up weapons, player was lower on purpose.
   if(oldammo)
      return true;

   // We were down to zero, so select a new weapon.
   // Preferences are not user selectable.
   
   // WEAPON_FIXME: ammo reception behaviors
   // INVENTORY_TODO: hardcoded behaviors for now...
   if(!strcasecmp(ammo->getKey(), "AmmoClip"))
   {
      if(player->readyweapon == wp_fist)
      {
         if(player->weaponowned[wp_chaingun])
            player->pendingweapon = wp_chaingun;
         else
            player->pendingweapon = wp_pistol;
      }
   }
   else if(!strcasecmp(ammo->getKey(), "AmmoShell"))
   {
      if(player->readyweapon == wp_fist || player->readyweapon == wp_pistol)
         if(player->weaponowned[wp_shotgun])
            player->pendingweapon = wp_shotgun;
   }
   else if(!strcasecmp(ammo->getKey(), "AmmoCell"))
   {
      if(player->readyweapon == wp_fist || player->readyweapon == wp_pistol)
         if(player->weaponowned[wp_plasma])
            player->pendingweapon = wp_plasma;
   }
   else if(!strcasecmp(ammo->getKey(), "AmmoMissile"))
   {
      if(player->readyweapon == wp_fist)
         if(player->weaponowned[wp_missile])
            player->pendingweapon = wp_missile;
   }

   return true;
}

//
// P_GiveAmmoPickup
//
// Give the player ammo from an ammoeffect pickup.
//
bool P_GiveAmmoPickup(player_t *player, itemeffect_t *pickup, bool dropped, int dropamount)
{
   if(!pickup)
      return false;

   itemeffect_t *give = E_ItemEffectForName(pickup->getString("ammo", ""));
   int giveamount     = pickup->getInt("amount", 0);

   if(dropped)
   {
      // actor may override dropamount
      if(dropamount)
         giveamount = dropamount;
      else
         giveamount = pickup->getInt("dropamount", giveamount);
   }

   return P_GiveAmmo(player, give, giveamount);
}

//
// P_giveBackpackAmmo
//
// ioanch 20151225: this calls P_GiveAmmo for each ammo type, using the backpack
// amount metatable value. It needs to work this way to have all side effects
// of the called function (double baby/nightmare ammo, weapon switching).
//
static void P_giveBackpackAmmo(player_t *player)
{
   static MetaKeyIndex keyBackpackAmount("ammo.backpackamount");

   size_t numAmmo = E_GetNumAmmoTypes();
   for(size_t i = 0; i < numAmmo; ++i)
   {
      auto ammoType = E_AmmoTypeForIndex(i);
      int giveamount = ammoType->getInt(keyBackpackAmount, 0);
      P_GiveAmmo(player, ammoType, giveamount);
   }
}

//
// Executes a thing's special and zeroes it, like in Hexen. Useful for reusable
// things; they'll only execute their special once.
//
static void P_consumeSpecial(player_t *activator, Mobj *special)
{
   if(special->special)
   {
      EV_ActivateSpecialNum(special->special, special->args, activator->mo);
      special->special = 0;
   }
}

//
// P_GiveWeapon
//
// The weapon name may have a MF_DROPPED flag ored in.
//
static bool P_GiveWeapon(player_t *player, weapontype_t weapon, bool dropped,
   Mobj *special)
{
   bool gaveweapon = false;
   weaponinfo_t *wp = &weaponinfo[weapon];

   if((dmflags & DM_WEAPONSTAY) && !dropped)
   {
      // leave placed weapons forever on net games
      if(player->weaponowned[weapon])
         return false;

      player->bonuscount += BONUSADD;
      player->weaponowned[weapon] = true;
      P_GiveAmmo(player, wp->ammo, (GameType == gt_dm) ? wp->dmstayammo : wp->coopstayammo);
      
      player->pendingweapon = weapon;
      S_StartSound(player->mo, sfx_wpnup); // killough 4/25/98, 12/98
      P_consumeSpecial(player, special); // need to handle it here
      return false;
   }

   // give one clip with a dropped weapon, two clips with a found weapon
   int  amount   = dropped ? wp->dropammo : wp->giveammo;
   bool gaveammo = (wp->ammo ? P_GiveAmmo(player, wp->ammo, amount) : false);
   
   // haleyjd 10/4/11: de-Killoughized
   if(!player->weaponowned[weapon])
   {
      player->pendingweapon = weapon;
      player->weaponowned[weapon] = 1;
      gaveweapon = true;
   }

   return gaveweapon || gaveammo;
}

//
// P_GiveBody
//
// Returns false if the body isn't needed at all
//
bool P_GiveBody(player_t *player, itemeffect_t *effect)
{
   if(!effect)
      return false;

   int amount    = effect->getInt("amount",       0);
   int maxamount = effect->getInt("maxamount",    0);

   // haleyjd 11/14/09: compatibility fix - the DeHackEd maxhealth setting was
   // only supposed to affect health potions, but when Ty replaced the MAXHEALTH
   // define in this module, he replaced all uses of it, including here. We need
   // to handle multiple cases for demo compatibility.
   if(demo_version >= 200 && demo_version < 335)
   {
      // only applies to items that actually have this key added to them by
      // DeHackEd; otherwise, the behavior defined through EDF prevails
      if(effect->hasKey("compatmaxamount"))
         maxamount = effect->getInt("compatmaxamount", 0);
   }

   // if not alwayspickup, and have more health than the max, don't pick it up
   if(!effect->getInt("alwayspickup", 0) && player->health >= maxamount)
      return false;

   // give the health
   if(effect->getInt("sethealth", 0))
      player->health = amount;  // some items set health directly
   else
      player->health += amount; // most items add to health

   // cap to maxamount
   if(player->health > maxamount)
      player->health = maxamount;

   // propagate to player's Mobj
   player->mo->health = player->health;
   return true;
}

//
// EV_DoHealThing
//
// Returns false if the health isn't needed at all
//
bool EV_DoHealThing(Mobj *actor, int amount, int max)
{
   if(actor->health < max)
   {
      actor->health += amount;

      // cap to maxhealth
      if(actor->health > max)
         actor->health = max;


      // propagate to Mobj's player if it exists
      if(actor->player)
         actor->player->health = actor->health;

      return true;
   }
   return false;
}

//
// P_GiveArmor
//
// Returns false if the armor is worse
// than the current armor.
//
bool P_GiveArmor(player_t *player, itemeffect_t *effect)
{
   if(!effect)
      return false;

   int hits        = effect->getInt("saveamount",  0);
   int savefactor  = effect->getInt("savefactor",  1);
   int savedivisor = effect->getInt("savedivisor", 3);

   // check for validity
   if(!hits || !savefactor || !savedivisor)
      return false;

   // check if needed
   if(!(effect->getInt("alwayspickup", 0)) && player->armorpoints >= hits)
      return false; // don't pick up

   // bonuses add to your armor and preserve your current absorption qualities;
   // normal pickups set your armor and override any existing qualities
   if(effect->getInt("bonus", 0))
   {
      int maxsaveamount = effect->getInt("maxsaveamount", 0);
      player->armorpoints += hits;
      if(player->armorpoints > maxsaveamount)
         player->armorpoints = maxsaveamount;

      // bonuses only change the player's armor absorption quality if the player
      // currently has no armor
      if(!player->armorfactor)
      {
         player->armorfactor  = savefactor;
         player->armordivisor = savedivisor;
      }
   }
   else
   {
      player->armorpoints  = hits;
      player->armorfactor  = savefactor;
      player->armordivisor = savedivisor;
   }

   return true;
}

//
// P_GiveCard
//
static void P_GiveCard(player_t *player, itemeffect_t *card, Mobj *special)
{
   if(E_GetItemOwnedAmount(player, card))
      return;

   player->bonuscount = BONUSADD; // INVENTORY_TODO: hard-coded for now
   E_GiveInventoryItem(player, card);

   // Make sure to consume its special if the player needed it, even if it
   // may or may not be removed later.
   P_consumeSpecial(player, special);
}

/*
  pw_invulnerability,
  pw_strength,
  pw_invisibility,
  pw_ironfeet,
  pw_allmap,
  pw_infrared,
  pw_totalinvis,  // haleyjd: total invisibility
  pw_ghost,       // haleyjd: heretic ghost
  pw_silencer,    // haleyjd: silencer
  pw_flight,      // haleyjd: flight
  pw_torch,       // haleyjd: infrared w/flicker
  NUMPOWERS
*/

//
// P_GivePower
//
// Rewritten by Lee Killough
//
bool P_GivePower(player_t *player, int power)
{
   static const int tics[NUMPOWERS] = 
   {
      INVULNTICS,
      1,          // strength 
      INVISTICS,
      IRONTICS, 
      1,          // allmap 
      INFRATICS,
      INVISTICS,  // haleyjd: totalinvis
      INVISTICS,  // haleyjd: ghost 
      1,          // haleyjd: silencer 
      FLIGHTTICS, // haleyjd: flight
      INFRATICS,  // haleyjd: torch
   };

   switch(power)
   {
   case pw_invisibility:
      player->mo->flags |= MF_SHADOW;
      break;
   case pw_allmap:
      if(player->powers[pw_allmap])
         return false;
      break;
   case pw_strength:
      P_GiveBody(player, E_ItemEffectForName(ITEMNAME_BERSERKHEALTH));
      break;
   case pw_totalinvis:   // haleyjd: total invisibility
      player->mo->flags2 |= MF2_DONTDRAW;
      player->mo->flags4 |= MF4_TOTALINVISIBLE;
      break;
   case pw_ghost:        // haleyjd: heretic ghost
      player->mo->flags3 |= MF3_GHOST;
      break;
   case pw_silencer:
      if(player->powers[pw_silencer])
         return false;
      break;
   case pw_flight:       // haleyjd: flight
      if(player->powers[pw_flight] < 0 || player->powers[pw_flight] > 4*32)
         return false;
      P_PlayerStartFlight(player, true);
      break;
   }

   // Unless player has infinite duration cheat, set duration (killough)   
   if(player->powers[power] >= 0)
      player->powers[power] = tics[power];

   return true;
}

//
// P_RavenRespawn
//
// haleyjd 08/17/13: Perform Raven-style item respawning logic.
//
static void P_RavenRespawn(Mobj *special)
{
   // item respawning has to be turned on
   bool willrespawn = ((dmflags & DM_ITEMRESPAWN) == DM_ITEMRESPAWN);

   // Remove special status. Note this exempts the object from the DOOM-style
   // item respawning code in Mobj::remove.
   special->flags &= ~MF_SPECIAL;

   // Super items only respawn if so specified
   if(special->flags3 & MF3_SUPERITEM && !(dmflags & DM_RESPAWNSUPER))
      willrespawn = false; 

   // NOITEMRESP items never respawn.
   if(special->flags3 & MF3_NOITEMRESP)
      willrespawn = false;

   // DROPPED items never respawn.
   if(special->flags & MF_DROPPED)
      willrespawn = false;

   // get states
   state_t *respawn = E_GetStateForMobjInfo(special->info, "Pickup.Respawn");
   state_t *remove  = E_GetStateForMobjInfo(special->info, "Pickup.Remove");

   // Want to respawn, and can respawn?
   if(willrespawn && respawn)
      P_SetMobjState(special, respawn->index);
   else
   {
      // Removing the item. If it has a remove state, set it.
      // Otherwise, the item is removed now.
      if(remove)
         P_SetMobjState(special, remove->index);
      else
         special->remove();
   }
}

//
// P_TouchSpecialThingNew
//
// INVENTORY_FIXME / INVENTORY_TODO: This will become P_TouchSpecialThing
// when it is finished, replacing the below.
//
void P_TouchSpecialThingNew(Mobj *special, Mobj *toucher)
{
   player_t     *player;
   e_pickupfx_t *pickup;
   itemeffect_t *effect;
   bool          pickedup = false;
   bool          dropped  = false;
   const char   *message  = NULL;
   const char   *sound    = NULL;

   fixed_t delta = special->z - toucher->z;
   if(delta > toucher->height || delta < -8*FRACUNIT)
      return; // out of reach

   // haleyjd: don't crash if a monster gets here.
   if(!(player = toucher->player))
      return;

   // Dead thing touching.
   // Can happen with a sliding player corpse.
   if(toucher->health <= 0)
      return;

   // haleyjd 05/11/03: EDF pickups modifications
   if(special->sprite < 0 || special->sprite >= NUMSPRITES)
      return;

   pickup = &pickupfx[special->sprite];
   effect = pickup->effect;

   if(!effect)
      return;

   // set defaults
   message = pickup->message;
   sound   = pickup->sound;
   dropped = ((special->flags & MF_DROPPED) == MF_DROPPED);

   switch(effect->getInt("class", ITEMFX_NONE))
   {
   case ITEMFX_HEALTH:   // Health - heal up the player automatically
      pickedup = P_GiveBody(player, effect);
      if(pickedup && player->health < effect->getInt("amount", 0) * 2)
         message = effect->getString("lowmessage", message);
      break;
   case ITEMFX_ARMOR:    // Armor - give the player some armor
      pickedup = P_GiveArmor(player, effect);
      break;
   case ITEMFX_AMMO:     // Ammo - give the player some ammo
      pickedup = P_GiveAmmoPickup(player, effect, dropped, special->dropamount);
      break;
   case ITEMFX_ARTIFACT: // Artifacts - items which go into the inventory
      pickedup = E_GiveInventoryItem(player, effect);
      break;
   default:
      return;
   }

   // perform post-processing if the item was collected beneficially, or if the
   // pickup is flagged to always be picked up even without benefit.
   if(pickedup || (pickup->flags & PFXF_ALWAYSPICKUP))
   {
      // Remove the object, provided it doesn't stay in multiplayer games
      if(GameType == gt_single || !(pickup->flags & PFXF_LEAVEINMULTI))
      {
         // Award item count
         if(special->flags & MF_COUNTITEM)
            player->itemcount++;

         // Check for item respawning style: DOOM, or Raven
         if(special->flags4 & MF4_RAVENRESPAWN)
            P_RavenRespawn(special);
         else
            special->remove();
      }

      // if picked up for benefit, or not silent when picked up without, do
      // all the "noisy" pickup effects
      if(pickedup || !(pickup->flags & PFXF_SILENTNOBENEFIT))
      {
         // Give message
         if(message)
         {
            // check for BEX string
            if(message[0] == '$')
               message = DEH_String(message + 1);
            player_printf(player, "%s", message);
         }

         // Play pickup sound
         if(sound)
            S_StartSoundName(player->mo, sound);

         // Increment bonuscount
         if(!(pickup->flags & PFXF_NOSCREENFLASH))
            player->bonuscount += BONUSADD;
      }
   }
}

//
// P_TouchSpecialThing
//
void P_TouchSpecialThing(Mobj *special, Mobj *toucher)
{
   player_t   *player;
   int        sound;
   const char *message = NULL;
   bool       removeobj = true;
   bool       pickup_fx = true; // haleyjd 04/14/03
   bool       dropped   = false;
   fixed_t    delta = special->z - toucher->z;
   
   // INVENTORY_TODO: transitional logic is in place below until this function
   // can be fully converted to being based on itemeffects
   itemeffect_t *effect = NULL;

   if(delta > toucher->height || delta < -8*FRACUNIT)
      return;        // out of reach

   sound = sfx_itemup;

   // haleyjd: don't crash if a monster gets here.
   if(!(player = toucher->player))
      return;
   
   // Dead thing touching.
   // Can happen with a sliding player corpse.
   if(toucher->health <= 0)
      return;

   // haleyjd 05/11/03: EDF pickups modifications
   if(special->sprite < 0 || special->sprite >= NUMSPRITES)
      return;

   dropped = ((special->flags & MF_DROPPED) == MF_DROPPED);

   // Identify by sprite.
   // INVENTORY_FIXME: apply pickupfx[].effect instead!
   switch(pickupfx[special->sprite].tempeffect)
   {
      // armor
   case PFX_GREENARMOR:
      // INVENTORY_TODO: hardcoded for now
      if(!P_GiveArmor(player, E_ItemEffectForName(ITEMNAME_GREENARMOR)))
         return;
      message = DEH_String("GOTARMOR"); // Ty 03/22/98 - externalized
      break;

   case PFX_BLUEARMOR:
      if(!P_GiveArmor(player, E_ItemEffectForName(ITEMNAME_BLUEARMOR)))
         return;
      message = DEH_String("GOTMEGA"); // Ty 03/22/98 - externalized
      break;

      // bonus items
   case PFX_POTION:
      // INVENTORY_TODO: hardcoded for now
      P_GiveBody(player, E_ItemEffectForName(ITEMNAME_HEALTHBONUS));
      message = DEH_String("GOTHTHBONUS"); // Ty 03/22/98 - externalized
      break;

   case PFX_ARMORBONUS:
      // INVENTORY_TODO: hardcoded for now
      P_GiveArmor(player, E_ItemEffectForName(ITEMNAME_ARMORBONUS));
      message = DEH_String("GOTARMBONUS"); // Ty 03/22/98 - externalized
      break;

      // sf: removed beta items
      
   case PFX_SOULSPHERE:
      // INVENTORY_TODO: hardcoded for now
      P_GiveBody(player, E_ItemEffectForName(ITEMNAME_SOULSPHERE));
      message = DEH_String("GOTSUPER"); // Ty 03/22/98 - externalized
      sound = sfx_getpow;

      break;

   case PFX_MEGASPHERE:
      if(demo_version < 335 && GameModeInfo->id != commercial)
         return;
      // INVENTORY_TODO: hardcoded for now
      P_GiveBody(player, E_ItemEffectForName(ITEMNAME_MEGASPHERE));
      P_GiveArmor(player, E_ItemEffectForName(ITEMNAME_BLUEARMOR));
      message = DEH_String("GOTMSPHERE"); // Ty 03/22/98 - externalized
      sound = sfx_getpow;
      break;

      // cards
      // leave cards for everyone
   case PFX_BLUEKEY:
      // INVENTORY_TODO: hardcoded for now
      effect = E_ItemEffectForName(ARTI_BLUECARD);
      if(!E_GetItemOwnedAmount(player, effect))
         message = DEH_String("GOTBLUECARD"); // Ty 03/22/98 - externalized
      P_GiveCard(player, effect, special);
      removeobj = pickup_fx = (GameType == gt_single);
      break;

   case PFX_YELLOWKEY:
      // INVENTORY_TODO: hardcoded for now
      effect = E_ItemEffectForName(ARTI_YELLOWCARD);
      if(!E_GetItemOwnedAmount(player, effect))
         message = DEH_String("GOTYELWCARD"); // Ty 03/22/98 - externalized
      P_GiveCard(player, effect, special);
      removeobj = pickup_fx = (GameType == gt_single);
      break;

   case PFX_REDKEY:
      // INVENTORY_TODO: hardcoded for now
      effect = E_ItemEffectForName(ARTI_REDCARD);
      if(!E_GetItemOwnedAmount(player, effect))
         message = DEH_String("GOTREDCARD"); // Ty 03/22/98 - externalized
      P_GiveCard(player, effect, special);
      removeobj = pickup_fx = (GameType == gt_single);
      break;
      
   case PFX_BLUESKULL:
      // INVENTORY_TODO: hardcoded for now
      effect = E_ItemEffectForName(ARTI_BLUESKULL);
      if(!E_GetItemOwnedAmount(player, effect))
         message = DEH_String("GOTBLUESKUL"); // Ty 03/22/98 - externalized
      P_GiveCard(player, effect, special);
      removeobj = pickup_fx = (GameType == gt_single);
      break;
      
   case PFX_YELLOWSKULL:
      // INVENTORY_TODO: hardcoded for now
      effect = E_ItemEffectForName(ARTI_YELLOWSKULL);
      if(!E_GetItemOwnedAmount(player, effect))
         message = DEH_String("GOTYELWSKUL"); // Ty 03/22/98 - externalized
      P_GiveCard(player, effect, special);
      removeobj = pickup_fx = (GameType == gt_single);
      break;

   case PFX_REDSKULL:
      // INVENTORY_TODO: hardcoded for now
      effect = E_ItemEffectForName(ARTI_REDSKULL);
      if(!E_GetItemOwnedAmount(player, effect))
         message = DEH_String("GOTREDSKULL"); // Ty 03/22/98 - externalized
      P_GiveCard(player, effect, special);
      removeobj = pickup_fx = (GameType == gt_single);
      break;

   // medikits, heals
   case PFX_STIMPACK:
      // INVENTORY_FIXME: temp hard-coded
      if(!P_GiveBody(player, E_ItemEffectForName(ITEMNAME_STIMPACK)))
         return;
      message = DEH_String("GOTSTIM"); // Ty 03/22/98 - externalized
      break;
      
   case PFX_MEDIKIT:
      // INVENTORY_TODO: hardcoded for now
      effect = E_ItemEffectForName(ITEMNAME_MEDIKIT);
      if(!P_GiveBody(player, effect))
         return;
      // sf: fix medineed 
      // (check for below 25, but medikit gives 25, so always > 25)
      message = DEH_String(player->health < 50 ? "GOTMEDINEED" : "GOTMEDIKIT");
      break;


      // power ups
   case PFX_INVULNSPHERE:
      if(!P_GivePower(player, pw_invulnerability))
         return;
      message = DEH_String("GOTINVUL"); // Ty 03/22/98 - externalized
      sound = sfx_getpow;
      break;

      // WEAPON_FIXME: berserk changes to fist
   case PFX_BERZERKBOX:
      if(!P_GivePower(player, pw_strength))
         return;
      message = DEH_String("GOTBERSERK"); // Ty 03/22/98 - externalized
      if(player->readyweapon != wp_fist)
         // sf: removed beta
         player->pendingweapon = wp_fist;
      sound = sfx_getpow;
      break;

   case PFX_INVISISPHERE:
      if(!P_GivePower(player, pw_invisibility))
         return;
      message = DEH_String("GOTINVIS"); // Ty 03/22/98 - externalized
      sound = sfx_getpow;
      break;

   case PFX_RADSUIT:
      if(!P_GivePower(player, pw_ironfeet))
         return;
      message = DEH_String("GOTSUIT"); // Ty 03/22/98 - externalized
      sound = sfx_getpow;
      break;

   case PFX_ALLMAP:
      if(!P_GivePower(player, pw_allmap))
         return;
      message = DEH_String("GOTMAP"); // Ty 03/22/98 - externalized
      sound = sfx_getpow;
      break;

   case PFX_LIGHTAMP:
      if(!P_GivePower(player, pw_infrared))
         return;
      sound = sfx_getpow;
      message = DEH_String("GOTVISOR"); // Ty 03/22/98 - externalized
      break;

      // ammo
   case PFX_CLIP:
      // INVENTORY_TODO: hardcoded for now
      if(!P_GiveAmmoPickup(player, E_ItemEffectForName("Clip"), dropped, special->dropamount))
         return;
      message = DEH_String("GOTCLIP"); // Ty 03/22/98 - externalized
      break;

   case PFX_CLIPBOX:
      // INVENTORY_TODO: hardcoded for now
      if(!P_GiveAmmoPickup(player, E_ItemEffectForName("ClipBox"), false, 0))
         return;
      message = DEH_String("GOTCLIPBOX"); // Ty 03/22/98 - externalized
      break;

   case PFX_ROCKET:
      // INVENTORY_TODO: hardcoded for now
      if(!P_GiveAmmoPickup(player, E_ItemEffectForName("RocketAmmo"), false, 0))
         return;
      message = DEH_String("GOTROCKET"); // Ty 03/22/98 - externalized
      break;

   case PFX_ROCKETBOX:
      // INVENTORY_TODO: hardcoded for now
      if(!P_GiveAmmoPickup(player, E_ItemEffectForName("RocketBox"), false, 0))
         return;
      message = DEH_String("GOTROCKBOX"); // Ty 03/22/98 - externalized
      break;

   case PFX_CELL:
      // INVENTORY_TODO: hardcoded for now
      if(!P_GiveAmmoPickup(player, E_ItemEffectForName("Cell"), false, 0))
         return;
      message = DEH_String("GOTCELL"); // Ty 03/22/98 - externalized
      break;

   case PFX_CELLPACK:
      // INVENTORY_TODO: hardcoded for now
      if(!P_GiveAmmoPickup(player, E_ItemEffectForName("CellPack"), false, 0))
         return;
      message = DEH_String("GOTCELLBOX"); // Ty 03/22/98 - externalized
      break;
      
   case PFX_SHELL:
      // INVENTORY_TODO: hardcoded for now
      if(!P_GiveAmmoPickup(player, E_ItemEffectForName("Shell"), false, 0))
         return;
      message = DEH_String("GOTSHELLS"); // Ty 03/22/98 - externalized
      break;
      
   case PFX_SHELLBOX:
      // INVENTORY_TODO: hardcoded for now
      if(!P_GiveAmmoPickup(player, E_ItemEffectForName("ShellBox"), false, 0))
         return;
      message = DEH_String("GOTSHELLBOX"); // Ty 03/22/98 - externalized
      break;

   case PFX_BACKPACK:
      // INVENTORY_TODO: hardcoded for now
      if(!E_PlayerHasBackpack(player))
         E_GiveBackpack(player);
      // ioanch 20151225: call from here to handle backpack ammo
      P_giveBackpackAmmo(player);
      message = DEH_String("GOTBACKPACK"); // Ty 03/22/98 - externalized
      break;

      // WEAPON_FIXME: Weapon collection
      // weapons
   case PFX_BFG:
      if(!P_GiveWeapon(player, wp_bfg, false, special))
         return;
      // FIXME: externalize all BFG pickup strings
      message = bfgtype==0 ? DEH_String("GOTBFG9000") // sf
                : bfgtype==1 ? "You got the BFG 2704!"
                : bfgtype==2 ? "You got the BFG 11K!"
                : bfgtype==3 ? "You got the Bouncing BFG!"
                : bfgtype==4 ? "You got the Plasma Burst BFG!"
                : "You got some kind of BFG";
      sound = sfx_wpnup;
      break;

   case PFX_CHAINGUN:
      if(!P_GiveWeapon(player, wp_chaingun, dropped, special))
         return;
      message = DEH_String("GOTCHAINGUN"); // Ty 03/22/98 - externalized
      sound = sfx_wpnup;
      break;

   case PFX_CHAINSAW:
      if(!P_GiveWeapon(player, wp_chainsaw, false, special))
         return;
      message = DEH_String("GOTCHAINSAW"); // Ty 03/22/98 - externalized
      sound = sfx_wpnup;
      break;

   case PFX_LAUNCHER:
      if(!P_GiveWeapon(player, wp_missile, false, special))
         return;
      message = DEH_String("GOTLAUNCHER"); // Ty 03/22/98 - externalized
      sound = sfx_wpnup;
      break;

   case PFX_PLASMA:
      if(!P_GiveWeapon(player, wp_plasma, false, special))
         return;
      message = DEH_String("GOTPLASMA"); // Ty 03/22/98 - externalized
      sound = sfx_wpnup;
      break;

   case PFX_SHOTGUN:
      if(!P_GiveWeapon(player, wp_shotgun, dropped, special))
         return;
      message = DEH_String("GOTSHOTGUN"); // Ty 03/22/98 - externalized
      sound = sfx_wpnup;
      break;

   case PFX_SSG:
      if(!P_GiveWeapon(player, wp_supershotgun, dropped, special))
         return;
      message = DEH_String("GOTSHOTGUN2"); // Ty 03/22/98 - externalized
      sound = sfx_wpnup;
      break;

      // haleyjd 10/10/02: Heretic powerups

   case PFX_HGREENKEY: // green key
      // INVENTORY_TODO: hardcoded for now
      effect = E_ItemEffectForName(ARTI_KEYGREEN);
      if(!E_GetItemOwnedAmount(player, effect))
         message = DEH_String("HGOTGREENKEY");
      P_GiveCard(player, effect, special);
      removeobj = pickup_fx = (GameType == gt_single);
      sound = sfx_keyup;
      break;

   case PFX_HBLUEKEY: // blue key
      // INVENTORY_TODO: hardcoded for now
      effect = E_ItemEffectForName(ARTI_KEYBLUE);
      if(!E_GetItemOwnedAmount(player, effect))
         message = DEH_String("HGOTBLUEKEY");
      P_GiveCard(player, effect, special);
      removeobj = pickup_fx = (GameType == gt_single);
      sound = sfx_keyup;
      break;

   case PFX_HYELLOWKEY: // yellow key
      // INVENTORY_TODO: hardcoded for now
      effect = E_ItemEffectForName(ARTI_KEYYELLOW);
      if(!E_GetItemOwnedAmount(player, effect))
         message = DEH_String("HGOTYELLOWKEY");
      P_GiveCard(player, effect, special);
      removeobj = pickup_fx = (GameType == gt_single);
      sound = sfx_keyup;
      break;

   case PFX_HPOTION: // heretic potion
      if(!P_GiveBody(player, E_ItemEffectForName("CrystalVial")))
         return;
      message = DEH_String("HITEMHEALTH");
      sound = sfx_hitemup;
      break;

   case PFX_SILVERSHIELD: // heretic shield 1
      // INVENTORY_TODO: hardcoded for now
      if(!P_GiveArmor(player, E_ItemEffectForName(ITEMNAME_SILVERSHIELD)))
         return;
      message = DEH_String("HITEMSHIELD1");
      sound = sfx_hitemup;
      break;

   case PFX_ENCHANTEDSHIELD: // heretic shield 2
      // INVENTORY_TODO: hardcoded for now
      if(!P_GiveArmor(player, E_ItemEffectForName(ITEMNAME_ENCHANTEDSHLD)))
         return;
      message = DEH_String("HITEMSHIELD2");
      sound = sfx_hitemup;
      break;

   case PFX_BAGOFHOLDING: // bag of holding
      // HTIC_TODO: bag of holding effects
      message = DEH_String("HITEMBAGOFHOLDING");
      sound = sfx_hitemup;
      break;

   case PFX_HMAP: // map scroll
      if(!P_GivePower(player, pw_allmap))
         return;
      message = DEH_String("HITEMSUPERMAP");
      sound = sfx_hitemup;
      break;
   
      // Heretic Ammo items
   case PFX_GWNDWIMPY:
      // HTIC_TODO: give ammo
      message = DEH_String("HAMMOGOLDWAND1");
      sound = sfx_hitemup;
      break;
   
   case PFX_GWNDHEFTY:
      // HTIC_TODO: give ammo
      message = DEH_String("HAMMOGOLDWAND2");
      sound = sfx_hitemup;
      break;
   
   case PFX_MACEWIMPY:
      // HTIC_TODO: give ammo
      message = DEH_String("HAMMOMACE1");
      sound = sfx_hitemup;
      break;
   
   case PFX_MACEHEFTY:
      // HTIC_TODO: give ammo
      message = DEH_String("HAMMOMACE2");
      sound = sfx_hitemup;
      break;
   
   case PFX_CBOWWIMPY:
      // HTIC_TODO: give ammo
      message = DEH_String("HAMMOCROSSBOW1");
      sound = sfx_hitemup;
      break;
   
   case PFX_CBOWHEFTY:
      // HTIC_TODO: give ammo
      message = DEH_String("HAMMOCROSSBOW2");
      sound = sfx_hitemup;
      break;
   
   case PFX_BLSRWIMPY:
      // HTIC_TODO: give ammo
      message = DEH_String("HAMMOBLASTER1");
      sound = sfx_hitemup;
      break;
   
   case PFX_BLSRHEFTY:
      // HTIC_TODO: give ammo
      message = DEH_String("HAMMOBLASTER2");
      sound = sfx_hitemup;
      break;
   
   case PFX_PHRDWIMPY:
      // HTIC_TODO: give ammo
      message = DEH_String("HAMMOPHOENIXROD1");
      sound = sfx_hitemup;
      break;
   
   case PFX_PHRDHEFTY:
      // HTIC_TODO: give ammo
      message = DEH_String("HAMMOPHOENIXROD2");
      sound = sfx_hitemup;
      break;
   
   case PFX_SKRDWIMPY:
      // HTIC_TODO: give ammo
      message = DEH_String("HAMMOSKULLROD1");
      sound = sfx_hitemup;
      break;
   
   case PFX_SKRDHEFTY:
      // HTIC_TODO: give ammo
      message = DEH_String("HAMMOSKULLROD2");
      sound = sfx_hitemup;
      break;

      // start new Eternity power-ups
   case PFX_TOTALINVIS:
      if(!P_GivePower(player, pw_totalinvis))
         return;
      message = "Total Invisibility!";
      sound = sfx_getpow;
      break;

   default:
      // I_Error("P_SpecialThing: Unknown gettable thing");
      return;      // killough 12/98: suppress error message
   }

   // sf: display message using player_printf
   if(message)
      player_printf(player, "%s", message);

   // haleyjd 07/08/05: rearranged to avoid removing before
   // checking for COUNTITEM flag.
   if(special->flags & MF_COUNTITEM)
      player->itemcount++;

   if(removeobj)
   {
      // this will cover all disappearing items. Non-disappearing ones have
      // their own special cases.
      P_consumeSpecial(player, special);
      if(special->flags4 & MF4_RAVENRESPAWN)
         P_RavenRespawn(special);
      else
         special->remove();
   }

   // haleyjd 07/08/05: inverted condition
   if(pickup_fx)
   {
      player->bonuscount += BONUSADD;
      S_StartSound(player->mo, sound);   // killough 4/25/98, 12/98
   }
}

//
// P_DropItems
//
// Drop all items as specified in an actor's MetaTable.
// If tossitems is false, only non-toss items are spawned.
// If tossitems is true, only toss items are spawned.
//
void P_DropItems(Mobj *actor, bool tossitems)
{
   MetaTable    *meta = actor->info->meta;
   MetaDropItem *mdi  = NULL;

   // players only drop items if so indicated
   if(actor->player && !(dmflags & DM_PLAYERDROP))
      return;

   while((mdi = meta->getNextTypeEx(mdi)))
   {
      // check if we spawn this sort of item at the present time
      if(mdi->toss != tossitems)
         continue;

      int type = E_SafeThingName(mdi->item.constPtr());

      // if chance is less than 255, do a dice roll for the drop
      if(mdi->chance != 255)
      {
         if(P_Random(pr_hdrop1) > mdi->chance)
            continue;
      }

      // determine z coordinate; if tossing, start at mid-height
      fixed_t z = ONFLOORZ;
      if(mdi->toss)
         z = actor->z + (actor->height / 2);
      
      // spawn the object
      Mobj *item = P_SpawnMobj(actor->x, actor->y, z, type);

      // special versions of items
      item->flags |= MF_DROPPED;

      // the dropitem may override the dropamount of ammo givers
      if(mdi->amount)
         item->dropamount = mdi->amount;

      // if tossing, give it randomized momenta
      if(mdi->toss)
      {
         item->momx = P_SubRandom(pr_hdropmom) << 8;
         item->momy = P_SubRandom(pr_hdropmom) << 8;
         item->momz = (P_Random(pr_hdropmom) << 10) + 5 * FRACUNIT;
      }
   }
}

//
// P_KillMobj
//
static void P_KillMobj(Mobj *source, Mobj *target, emod_t *mod)
{
   target->flags &= ~(MF_SHOOTABLE|MF_FLOAT|MF_SKULLFLY);
   target->flags2 &= ~MF2_INVULNERABLE; // haleyjd 04/09/99
   
   if(!(target->flags3 & MF3_DEADFLOAT))
      target->flags &= ~MF_NOGRAVITY;

   target->flags |= MF_CORPSE|MF_DROPOFF;
   target->height >>= 2;

   // killough 8/29/98: remove from threaded list
   target->updateThinker();
   
   if(source && source->player)
   {
      // count for intermission
      // killough 7/20/98: don't count friends
      if(!(target->flags & MF_FRIEND))
      {
         if(target->flags & MF_COUNTKILL)
            source->player->killcount++;
      }
      if(target->player)
      {
         source->player->frags[target->player-players]++;
         HU_FragsUpdate();
      }
   }
   else if(GameType == gt_single && (target->flags & MF_COUNTKILL))
   {
      // count all monster deaths,
      // even those caused by other monsters
      // killough 7/20/98: don't count friends
      if(!(target->flags & MF_FRIEND))
         players->killcount++;
   }

   if(target->player)
   {
      // count environment kills against you
      if(!source)
      {
         target->player->frags[target->player-players]++;
         HU_FragsUpdate();
      }

      target->flags  &= ~MF_SOLID;
      P_PlayerStopFlight(target->player);  // haleyjd: stop flying
      
      G_DemoLog("%d\tdeath player %d ", gametic,
         (int)(target->player - players) + 1);
      G_DemoLogStats();
      G_DemoLog("\n");
      
      target->player->prevpitch = target->player->pitch; // MaxW: Stop interpolation jittering
      target->player->playerstate = PST_DEAD;
      P_DropWeapon(target->player);

      if(target->player == &players[consoleplayer] && automapactive)
      {
         if(!demoplayback) // killough 11/98: don't switch out in demos, though
            AM_Stop();    // don't die in auto map; switch view prior to dying
      }
   }

   if(target->health < target->info->gibhealth &&
      target->info->xdeathstate != NullStateNum)
      P_SetMobjState(target, target->info->xdeathstate);
   else
   {
      // haleyjd 06/05/08: damagetype death states
      statenum_t st = target->info->deathstate;
      state_t *state;

      if(mod->num > 0 && (state = E_StateForMod(target->info, "Death", mod)))
         st = state->index;

      P_SetMobjState(target, st);
   }

   // FIXME: make a flag? Also, probably not done in Heretic/Hexen.
   target->tics -= P_Random(pr_killtics) & 3;
   
   if(target->tics < 1)
      target->tics = 1;

   // Drop stuff.
   // This determines the kind of object spawned
   // during the death frame of a thing.
   P_DropItems(target, false);

   if(EV_ActivateSpecialNum(target->special, target->args, target))
      target->special = 0; // Stop special from executing if revived/respawned
}

//
// P_GetDeathMessageString
//
// Retrieves the string to use for a given EDF damagetype.
//
static const char *P_GetDeathMessageString(emod_t *mod, bool self)
{
   const char *str;
   bool isbex;
   const char *ret;

   if(self)
   {
      str   = mod->selfobituary;
      isbex = mod->selfObitIsBexString;
   }
   else
   {
      str   = mod->obituary;
      isbex = mod->obitIsBexString;
   }

   if(isbex)
      ret = DEH_String(str);
   else
      ret = str;

   return ret;
}

//
// P_DeathMessage
//
// Implements obituaries based on the type of damage which killed a player.
//
static void P_DeathMessage(Mobj *source, Mobj *target, Mobj *inflictor, 
                           emod_t *mod)
{
   bool friendly = false;
   const char *message = NULL;

   if(!target->player || !obituaries)
      return;

   if(GameType == gt_coop)
      friendly = true;

   // miscellaneous death types that cannot be determined
   // directly from the source or inflictor without difficulty

   if((source == NULL && inflictor == NULL) || mod->sourceless)
      message = P_GetDeathMessageString(mod, false);

   if(source && !message)
   {
      if(source == target)
      {
         // killed self
         message = P_GetDeathMessageString(mod, true);
      }
      else if(!source->player)
      {
         // monster kills

         switch(mod->num)
         {
         case MOD_HIT: // melee attack
            message = source->info->meleeobit;
            break;
         default: // other attack
            message = source->info->obituary;
            break;
         }

         // if the monster didn't define the proper obit, try the 
         // obit defined by the mod of this attack
         if(!message)
            message = P_GetDeathMessageString(mod, false);
      }
   }

   // print message if any
   if(message)
   {
      doom_printf("%c%s %s", obcolour+128, target->player->name, message);
      return;
   }

   if(source && source->player)
   {
      if(friendly)
      {
         // in coop mode, player kills are bad
         message = DEH_String("OB_COOP");
      }
      else
      {
         // deathmatch deaths
         message = P_GetDeathMessageString(mod, false);
      }
   }

   // use default message
   if(!message)
      message = DEH_String("OB_DEFAULT");

   // print message
   doom_printf("%c%s %s", obcolour+128, target->player->name, message);
}

//=============================================================================
//
// Special damage type code -- see codepointer table below.
//

typedef struct dmgspecdata_s
{
   Mobj *source;
   Mobj *target;
   int     damage;
} dmgspecdata_t;

//
// P_MinotaurChargeHit
//
// Special damage action for Maulotaurs slamming into things.
//
static bool P_MinotaurChargeHit(dmgspecdata_t *dmgspec)
{
   Mobj *source = dmgspec->source;
   Mobj *target = dmgspec->target;

   // only when charging
   if(source->flags & MF_SKULLFLY)
   {
      angle_t angle;
      fixed_t thrust;
      
      // SoM: TODO figure out if linked portals needs to worry about this. It 
      // looks like target might not always be source->target
      angle = P_PointToAngle(source->x, source->y, target->x, target->y);
      thrust = 16*FRACUNIT + (P_Random(pr_mincharge) << 10);

      P_ThrustMobj(target, angle, thrust);
      P_DamageMobj(target, NULL, NULL, 
                   ((P_Random(pr_mincharge) & 7) + 1) * 6, 
                   MOD_UNKNOWN);
      
      if(target->player)
         target->reactiontime = 14 + (P_Random(pr_mincharge) & 7);

      return true; // return early from P_DamageMobj
   }

   return false; // just normal damage
}

//
// P_TouchWhirlwind
//
// Called when an Iron Lich whirlwind hits something. Does damage
// and may toss the target around violently.
//
static bool P_TouchWhirlwind(dmgspecdata_t *dmgspec)
{
   Mobj *target = dmgspec->target;
   
   // toss the target around
   
   target->angle += P_SubRandom(pr_whirlwind) << 20;
   target->momx  += P_SubRandom(pr_whirlwind) << 10;   
   target->momy  += P_SubRandom(pr_whirlwind) << 10;

   // z momentum -- Bosses will not be tossed up.

   if((leveltime & 16) && !(target->flags2 & MF2_BOSS))
   {
      int randVal = P_Random(pr_whirlwind);

      if(randVal > 160)
         randVal = 160;
      
      target->momz += randVal << 10;
      
      if(target->momz > 12*FRACUNIT)
         target->momz = 12*FRACUNIT;
   }
   
   // do a small amount of damage (it adds up fast)
   if(!(leveltime & 7))
      P_DamageMobj(target, NULL, NULL, 3, MOD_UNKNOWN);

   return true; // always return from P_DamageMobj
}

//
// haleyjd: Damage Special codepointer lookup table
//
// mobjinfo::dmgspecial is an index into this table. The index is checked for
// validity during EDF processing. If the special returns true, P_DamageMobj
// returns immediately, assuming that the special did its own damage. If it
// returns false, P_DamageMobj continues, and the damage field of the 
// dmgspecdata_t structure is used to possibly modify the damage that will be
// done.
//

typedef bool (*dmgspecial_t)(dmgspecdata_t *);

static dmgspecial_t DamageSpecials[INFLICTOR_NUMTYPES] =
{
   NULL,                // none
   P_MinotaurChargeHit, // MinotaurCharge
   P_TouchWhirlwind,    // Whirlwind
};

//
// P_AdjustDamageType
//
// haleyjd 10/12/09: Handles special cases for damage types.
//
static int P_AdjustDamageType(Mobj *source, Mobj *inflictor, int mod)
{
   int newmod = mod;

   // inflictor-based adjustments

   if(inflictor)
   {
      // haleyjd 06/05/08: special adjustments to mod type based on inflictor
      // thingtype flags (I'd rather not do this at all, but it's necessary
      // since the FIREDAMAGE flag has been around forever).
      if(inflictor->flags3 & MF3_FIREDAMAGE)
         newmod = MOD_FIRE;
   }

   // source-based adjustments

   if(source)
   {
      // players
      if(source->player && mod == MOD_PLAYERMISC)
      {
         weaponinfo_t *weapon = P_GetReadyWeapon(source->player);

         // redirect based on weapon mod
         newmod = weapon->mod;
      }
   }

   return newmod;
}

//
// P_DamageMobj
//
// Damages both enemies and players
// "inflictor" is the thing that caused the damage
//  creature or missile, can be NULL (slime, etc)
// "source" is the thing to target after taking damage
//  creature or NULL
// Source and inflictor are the same for melee attacks.
// Source can be NULL for slime, barrel explosions
// and other environmental stuff.
//
// haleyjd 07/13/03: added method of death flag
//
void P_DamageMobj(Mobj *target, Mobj *inflictor, Mobj *source, 
                  int damage, int mod)
{
   emod_t *emod;
   player_t *player;
   bool justhit = false;  // killough 11/98
   bool speciesignore;       // haleyjd
   
   // killough 8/31/98: allow bouncers to take damage
   if(!(target->flags & (MF_SHOOTABLE | MF_BOUNCES)))
      return; // shouldn't happen...
   
   if(target->health <= 0)
      return;
   
   // haleyjd: 
   // Invulnerability -- telestomp can still kill to avoid getting stuck
   // Dormancy -- things are invulnerable until they are awakened
   // No Friend Damage -- some things aren't hurt by friends
   // Invuln-Charge -- skullflying objects won't take damage (for Maulotaur)
   if(damage < 10000)
   {
      if(target->flags2 & (MF2_INVULNERABLE | MF2_DORMANT))
         return;

      if(target->flags3 & MF3_NOFRIENDDMG && source && source != target &&
         source->flags & MF_FRIEND)
         return;

      if(target->flags & MF_SKULLFLY && target->flags3 & MF3_INVULNCHARGE)
         return;
   }

   // a dormant thing being destroyed gets restored to normal first
   if(target->flags2 & MF2_DORMANT)
   {
      target->flags2 &= ~MF2_DORMANT;
      target->tics = 1;
   }

   if(target->flags & MF_SKULLFLY)
      target->momx = target->momy = target->momz = 0;

   player = target->player;
   if(player && gameskill == sk_baby)
      damage >>= 1;   // take half damage in trainer mode

   // haleyjd 08/01/04: dmgspecial -- special inflictor types
   if(inflictor && inflictor->info->dmgspecial)
   {
      dmgspecdata_t dmgspec;

      dmgspec.source = inflictor;
      dmgspec.target = target;
      dmgspec.damage = damage;

      // if the handler returns true, damage was taken care of by
      // the handler, otherwise, we go on as normal
      if(DamageSpecials[inflictor->info->dmgspecial](&dmgspec))
         return;

      // damage may be modified by the handler
      damage = dmgspec.damage;
   }

   // haleyjd 10/12/09: do all adjustments to mod now, and look up emod_t once
   mod  = P_AdjustDamageType(source, inflictor, mod);
   emod = E_DamageTypeForNum(mod);

   // haleyjd 10/12/09: damage factors
   if(mod != MOD_UNKNOWN)
   {
      MetaTable *meta = target->info->meta;
      int df = meta->getInt(emod->dfKeyIndex, FRACUNIT);

      // Special case: D_MININT is absolute immunity.
      if(df == D_MININT)
         return;

      // Only apply if not FRACUNIT, due to the chance this might alter
      // the compatibility characteristics of extreme DEH/BEX damage.
      if(df != FRACUNIT)
         damage = (damage * df) / FRACUNIT;
   }

   // Some close combat weapons should not
   // inflict thrust and push the victim out of reach,
   // thus kick away unless using the chainsaw.

   if(inflictor && !(target->flags & MF_NOCLIP) &&
      (!source || !source->player ||
       !(P_GetReadyWeapon(source->player)->flags & WPF_NOTHRUST)) &&
      !(inflictor->flags3 & MF3_NODMGTHRUST)) // haleyjd 11/14/02
   {
      // haleyjd: thrust factor differs for Heretic
      int16_t tf = GameModeInfo->thrustFactor;

      // SoM: restructured a bit
      fixed_t thrust = damage*(FRACUNIT>>3)*tf/target->info->mass;
#ifdef R_LINKEDPORTALS
      unsigned ang;

      {
         if(inflictor->groupid == target->groupid)
         {
            ang = P_PointToAngle (inflictor->x, inflictor->y, 
                                   target->x, target->y);
         }
         else
         {
            auto link = P_GetLinkOffset(target->groupid, inflictor->groupid);
            ang = P_PointToAngle(inflictor->x, inflictor->y, 
                                  target->x + link->x, target->y + link->y);
         }
      }
#else
      unsigned ang = P_PointToAngle (inflictor->x, inflictor->y,
                                      target->x, target->y);
#endif

      // make fall forwards sometimes
      if(damage < 40 && damage > target->health
         && target->z - inflictor->z > 64*FRACUNIT
         && P_Random(pr_damagemobj) & 1)
      {
         ang += ANG180;
         thrust *= 4;
      }

      P_ThrustMobj(target, ang, thrust);
      
      // killough 11/98: thrust objects hanging off ledges
      if(target->intflags & MIF_FALLING && target->gear >= MAXGEAR)
         target->gear = 0;
   }

   // player specific
   if(player)
   {
      // haleyjd 07/10/09: instagib
      if(source && dmflags & DM_INSTAGIB)
         damage = 10000;

      // end of game hell hack
      // ioanch 20160116: portal aware
      if(P_ExtremeSectorAtPoint(target, false)->damageflags & SDMG_EXITLEVEL &&
         damage >= target->health)
      {
         damage = target->health - 1;
      }

      // Below certain threshold,
      // ignore damage in GOD mode, or with INVUL power.
      // killough 3/26/98: make god mode 100% god mode in non-compat mode

      if((damage < 1000 || (!comp[comp_god] && player->cheats&CF_GODMODE)) &&
         (player->cheats&CF_GODMODE || player->powers[pw_invulnerability]))
         return;

      if(player->armorfactor && player->armordivisor)
      {
         int saved = damage * player->armorfactor / player->armordivisor;
         
         if(player->armorpoints <= saved)
         {
            // armor is used up
            saved = player->armorpoints;
            player->armorfactor = player->armordivisor = 0;
         }
         player->armorpoints -= saved;
         damage -= saved;
      }

      player->health -= damage;       // mirror mobj health here for Dave
      if(player->health < 0)
         player->health = 0;
      
      P_SetPlayerAttacker(player, source);
      player->damagecount += damage;  // add damage after armor / invuln
      
      if(player->damagecount > 100)
         player->damagecount = 100;  // teleport stomp does 10k points...

      // haleyjd 10/14/09: we do not allow negative damagecount
      if(player->damagecount < 0)
         player->damagecount = 0;

      // haleyjd 06/08/13: reimplement support for tactile damage feedback :)
      if(player == &players[consoleplayer])
         I_StartHaptic(HALHapticInterface::EFFECT_DAMAGE, player->damagecount, 300);      
   }

   // do the damage
   if(!(target->flags4 & MF4_NODAMAGE) || damage >= 10000)
      target->health -= damage;

   // check for death
   if(target->health <= 0)
   {
      // death messages for players
      if(player)
      {
         // haleyjd 12/29/10: immortality cheat
         if(player->cheats & CF_IMMORTAL)
         {
            player->mo->health = 1;
            player->health = 1;
            if(target != player->mo)
               target->health = 1;
            // some extra effects for fun :P
            player->bonuscount = player->damagecount;
            player->damagecount = 0;
            doom_printf("Your god has saved you!");
            return;
         }
         else
            P_DeathMessage(source, target, inflictor, emod);
      }

      // haleyjd 09/29/07: wimpy death?
      if(damage <= 10)
         target->intflags |= MIF_WIMPYDEATH;

      P_KillMobj(source, target, emod);
      return;
   }

   // haleyjd: special death hacks: if we got here, we didn't really die
    target->intflags &= ~(MIF_DIEDFALLING|MIF_WIMPYDEATH);

   // killough 9/7/98: keep track of targets so that friends can help friends
   if(demo_version >= 203)
   {
      // If target is a player, set player's target to source,
      // so that a friend can tell who's hurting a player
      if(player)
         P_SetTarget<Mobj>(&target->target, source);
      
      // killough 9/8/98:
      // If target's health is less than 50%, move it to the front of its list.
      // This will slightly increase the chances that enemies will choose to
      // "finish it off", but its main purpose is to alert friends of danger.

      if(target->health * 2 < target->getModifiedSpawnHealth())
      {
         Thinker *cap = 
            &thinkerclasscap[target->flags & MF_FRIEND ? 
                             th_friends : th_enemies];
         (target->cprev->cnext = target->cnext)->cprev = target->cprev;
         (target->cnext = cap->cnext)->cprev = target;
         (target->cprev = cap)->cnext = target;
      }
   }

   if(P_Random(pr_painchance) < target->info->painchance &&
      !(target->flags & MF_SKULLFLY))
   { 
      //killough 11/98: see below
      if(demo_version >= 203)
         justhit = true;
      else
         target->flags |= MF_JUSTHIT;    // fight back!

      statenum_t st = target->info->painstate;
      state_t *state = NULL;

      // haleyjd  06/05/08: check for special damagetype painstate
      if(mod > 0 && (state = E_StateForMod(target->info, "Pain", emod)))
         st = state->index;

      P_SetMobjState(target, st);
   }
   
   target->reactiontime = 0;           // we're awake now...

   // killough 9/9/98: cleaned up, made more consistent:
   // haleyjd 11/24/02: added MF3_DMGIGNORED and MF3_BOSSIGNORE flags

   // BOSSIGNORE flag is deprecated, use thinggroup with DAMAGEIGNORE instead

   // haleyjd: set bossignore
   if(source && ((source->type != target->type &&
                  (source->flags3 & target->flags3 & MF3_BOSSIGNORE ||
                   E_ThingPairValid(source->type, target->type, TGF_DAMAGEIGNORE))) ||
                 (source->type == target->type && source->flags4 & MF4_NOSPECIESINFIGHT)))
   {
      // ignore if friendliness matches
      speciesignore = !((source->flags ^ target->flags) & MF_FRIEND);
   }
   else
      speciesignore = false;

   // Set target based on the following criteria:
   // * Damage is sourced and source is not self.
   // * Source damage is not ignored via MF3_DMGIGNORED.
   // * Threshold is expired, or target has no threshold via MF3_NOTHRESHOLD.
   // * Things are not friends, or monster infighting is enabled.
   // * Things are not both friends while target is not a SUPERFRIEND

   // TODO: add fine-grained infighting control as metadata
   
   if(source && source != target                                     // source checks
      && !(source->flags3 & MF3_DMGIGNORED)                          // not ignored?
      && !speciesignore                                              // species not fighting
      && (!target->threshold || (target->flags3 & MF3_NOTHRESHOLD))  // threshold?
      && ((source->flags ^ target->flags) & MF_FRIEND ||             // friendliness?
           monster_infighting || demo_version < 203)
      && !(source->flags & target->flags & MF_FRIEND &&              // superfriend?
           target->flags3 & MF3_SUPERFRIEND))
   {
      // if not intent on another player, chase after this one
      //
      // killough 2/15/98: remember last enemy, to prevent
      // sleeping early; 2/21/98: Place priority on players
      // killough 9/9/98: cleaned up, made more consistent:

      if(!target->lastenemy || target->lastenemy->health <= 0 ||
         (demo_version < 203 ? !target->lastenemy->player :
          !((target->flags ^ target->lastenemy->flags) & MF_FRIEND) &&
             target->target != source)) // remember last enemy - killough
      {
         P_SetTarget<Mobj>(&target->lastenemy, target->target);
      }

      P_SetTarget<Mobj>(&target->target, source);       // killough 11/98
      target->threshold = BASETHRESHOLD;

      if(target->state == states[target->info->spawnstate] && 
         target->info->seestate != NullStateNum)
      {
         P_SetMobjState(target, target->info->seestate);
      }
   }

   // haleyjd 01/15/06: Fix for demo comp problem introduced in MBF
   // For MBF and above, set MF_JUSTHIT here
   // killough 11/98: Don't attack a friend, unless hit by that friend.
   if(!demo_compatibility && justhit && 
      (target->target == source || !target->target ||
       !(target->flags & target->target->flags & MF_FRIEND)))
   {
      target->flags |= MF_JUSTHIT;    // fight back!
   }
}

//
// P_Whistle
//
// haleyjd 01/11/04:
// Inspired in part by Lee Killough's changelog, this allows the
// a friend to be teleported to the player's location.
//
void P_Whistle(Mobj *actor, int mobjtype)
{
   Mobj *mo;
   Thinker *th;
   fixed_t prevx, prevy, prevz, prestep, x, y, z;
   angle_t an;

   // look for a friend of the indicated type
   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      if(!(mo = thinker_cast<Mobj *>(th)))
         continue;

      // must be friendly, alive, and of the right type
      if(!(mo->flags & MF_FRIEND) || mo->health <= 0 ||
         mo->type != mobjtype)
         continue;

      prevx = mo->x;
      prevy = mo->y;
      prevz = mo->z;

      // pick a location a bit in front of the player
      an = actor->angle >> ANGLETOFINESHIFT;
      prestep = 4*FRACUNIT + 3*(actor->info->radius + mo->info->radius)/2;

      x = actor->x + FixedMul(prestep, finecosine[an]);
      y = actor->y + FixedMul(prestep, finesine[an]);
      z = actor->z;

      // don't cross "solid" lines
      if(Check_Sides(actor, x, y))
         return;

      // try the teleport
      // 06/06/05: use strict teleport now
      if(P_TeleportMoveStrict(mo, x, y, false))
      {
         Mobj *fog = P_SpawnMobj(prevx, prevy, 
                                   prevz + GameModeInfo->teleFogHeight,
                                   E_SafeThingName(GameModeInfo->teleFogType));
         S_StartSound(fog, GameModeInfo->teleSound);

         fog = P_SpawnMobj(x, y, z + GameModeInfo->teleFogHeight,
                           E_SafeThingName(GameModeInfo->teleFogType));
         S_StartSound(fog, GameModeInfo->teleSound);

         // put the thing into its spawnstate and keep it still
         P_SetMobjState(mo, mo->info->spawnstate);
         mo->z = mo->floorz;
         mo->momx = mo->momy = mo->momz = 0;
         mo->backupPosition();

         return;
      }
   }
}

//==============================================================================
//
// Archvile resurrection handlers. Also used by Thing_Raise.
//

//
// P_ThingIsCorpse
//
// ioanch 20160221
// Check if thing can be raised by an archvile. Doesn't check space.
//
bool P_ThingIsCorpse(const Mobj *mobj)
{
   return mobj->flags & MF_CORPSE && mobj->tics == -1 && 
      mobj->info->raisestate != NullStateNum;
}

//
// P_CheckCorpseRaiseSpace
//
// ioanch 20160221
// Does all the checks that there's actually room for this corpse to be raised.
// Assumes P_ThingIsCorpse returned true.
//
// It also has side effects, like in Doom: resets speed to 0.
//
// Returns true if there is space.
//
bool P_CheckCorpseRaiseSpace(Mobj *corpse)
{
   corpse->momx = corpse->momy = 0;
   bool check;
   if(comp[comp_vile])
   {
      corpse->height <<= 2;
      
      // haleyjd 11/11/04: this is also broken by Lee's change to
      // PIT_CheckThing when not in demo_compatibility.
      if(demo_version >= 331)
         corpse->flags |= MF_SOLID;

      check = P_CheckPosition(corpse, corpse->x, corpse->y);

      if(demo_version >= 331)
         corpse->flags &= ~MF_SOLID;
      
      corpse->height >>= 2;
   }
   else
   {
      int height,radius;
      
      height = corpse->height; // save temporarily
      radius = corpse->radius; // save temporarily
      corpse->height = P_ThingInfoHeight(corpse->info);
      corpse->radius = corpse->info->radius;
      corpse->flags |= MF_SOLID;
      check = P_CheckPosition(corpse,corpse->x,corpse->y);
      corpse->height = height; // restore
      corpse->radius = radius; // restore
      corpse->flags &= ~MF_SOLID;
   }

   return check;
}

//
// P_RaiseCorpse
//
// ioanch 20160221
// Resurrects a dead monster. Assumes the previous two functions returned true
//
void P_RaiseCorpse(Mobj *corpse, const Mobj *raiser)
{
   S_StartSound(corpse, sfx_slop);
   const mobjinfo_t *info = corpse->info;

   // haleyjd 09/26/04: need to restore monster skins here
   // in case they were cleared by the thing being crushed
   if(info->altsprite != -1)
      corpse->skin = P_GetMonsterSkin(info->altsprite);

   P_SetMobjState(corpse, info->raisestate);

   if(comp[comp_vile])
      corpse->height <<= 2;                        // phares
   else                                               //   V
   {
      // fix Ghost bug
      corpse->height = P_ThingInfoHeight(info);
      corpse->radius = info->radius;
   }                                                  // phares

   // killough 7/18/98: 
   // friendliness is transferred from AV to raised corpse
   // ioanch 20160221: if there's no raiser, don't change friendliness
   if(raiser)
   {
      corpse->flags = 
         (info->flags & ~MF_FRIEND) | (raiser->flags & MF_FRIEND);
   }
   else
   {
      // else reuse the old friend flag.
      corpse->flags = (info->flags & ~MF_FRIEND) | (corpse->flags & MF_FRIEND);
   }

   // clear ephemeral MIF flags that may persist from previous death
   corpse->intflags &= ~MIF_CLEARRAISED;

   corpse->health = corpse->getModifiedSpawnHealth();
   P_SetTarget<Mobj>(&corpse->target, NULL);  // killough 11/98

   if(demo_version >= 203)
   {         // kilough 9/9/98
      P_SetTarget<Mobj>(&corpse->lastenemy, NULL);
      corpse->flags &= ~MF_JUSTHIT;
   }

   // killough 8/29/98: add to appropriate thread
   corpse->updateThinker();
}

#if 0
//
// Small natives
//

//
// sm_thingkill
//
// Implements ThingKill(tid, damagetype = 0, mod = 0)
//
static cell AMX_NATIVE_CALL sm_thingkill(AMX *amx, cell *params)
{
   SmallContext_t *context = SM_GetContextForAMX(amx);
   Mobj *rover = NULL;

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   while((rover = P_FindMobjFromTID(params[1], rover, 
                                    context->invocationData.trigger)))
   {
      int damage;
      
      switch(params[2])
      {
      case 1: // telefrag damage
         damage = 10000;
         break;
      default: // damage for health
         damage = rover->health;
         break;
      }
      
      P_DamageMobj(rover, NULL, NULL, damage, MOD_UNKNOWN);
   }

   return 0;
}

//
// sm_thinghurt
//
// Implements ThingHurt(tid, damage, mod = 0, inflictor = 0, source = 0)
//
static cell AMX_NATIVE_CALL sm_thinghurt(AMX *amx, cell *params)
{
   SmallContext_t *context = SM_GetContextForAMX(amx);
   Mobj *rover = NULL;
   Mobj *inflictor = NULL;
   Mobj *source = NULL;

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   if(params[4] != 0)
   {
      inflictor = P_FindMobjFromTID(params[4], inflictor, 
                                    context->invocationData.trigger);
   }

   if(params[5] != 0)
   {
      source = P_FindMobjFromTID(params[5], source, 
                                 context->invocationData.trigger);
   }

   while((rover = P_FindMobjFromTID(params[1], rover, context->invocationData.trigger)))
   {
      P_DamageMobj(rover, inflictor, source, params[2], params[3]);
   }

   return 0;
}

//
// sm_thinghate
//
// Implements ThingHate(object, target)
//
static cell AMX_NATIVE_CALL sm_thinghate(AMX *amx, cell *params)
{
   SmallContext_t *context = SM_GetContextForAMX(amx);
   Mobj *obj = NULL, *targ = NULL;

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   if(params[2] != 0)
      targ = P_FindMobjFromTID(params[2], targ, context->invocationData.trigger);

   while((obj = P_FindMobjFromTID(params[1], obj, context->invocationData.trigger)))
   {
      P_SetTarget<Mobj>(&(obj->target), targ);
   }

   return 0;
}

AMX_NATIVE_INFO pinter_Natives[] =
{
   { "_ThingKill", sm_thingkill },
   { "_ThingHurt", sm_thinghurt },
   { "_ThingHate", sm_thinghate },
   { NULL, NULL }
};
#endif

//----------------------------------------------------------------------------
//
// $Log: p_inter.c,v $
// Revision 1.10  1998/05/03  23:09:29  killough
// beautification, fix #includes, move some global vars here
//
// Revision 1.9  1998/04/27  01:54:43  killough
// Prevent pickup sounds from silencing player weapons
//
// Revision 1.8  1998/03/28  17:58:27  killough
// Fix spawn telefrag bug
//
// Revision 1.7  1998/03/28  05:32:41  jim
// Text enabling changes for DEH
//
// Revision 1.6  1998/03/23  03:25:44  killough
// Fix weapon pickup sounds in spy mode
//
// Revision 1.5  1998/03/10  07:15:10  jim
// Initial DEH support added, minus text
//
// Revision 1.4  1998/02/23  04:44:33  killough
// Make monsters smarter
//
// Revision 1.3  1998/02/17  06:00:54  killough
// Save last enemy, change RNG calling sequence
//
// Revision 1.2  1998/01/26  19:24:05  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:59  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

