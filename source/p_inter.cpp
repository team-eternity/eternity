// Emacs style mode select   -*- C++ -*- vi:sw=3 ts=3:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Handling interactions (i.e., collisions).
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

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
#include "e_mod.h"
#include "e_states.h"
#include "e_things.h"
#include "g_dmflag.h"
#include "g_game.h"
#include "hu_frags.h"
#include "hu_stuff.h"
#include "m_random.h"
#include "metaapi.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_tick.h"
#include "p_user.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_segs.h"
#include "s_sound.h"
#include "sounds.h"
#include "v_misc.h"
#include "v_video.h"

// [CG] 09/17/11 Added.
#include "cs_main.h"
#include "cs_ctf.h"
#include "cl_cmd.h"
#include "cl_main.h"
#include "sv_main.h"


#define BONUSADD        6

// Ty 03/07/98 - add deh externals
// Maximums and such were hardcoded values.  Need to externalize those for
// dehacked support (and future flexibility).  Most var names came from the key
// strings used in dehacked.

int initial_health = 100;
int initial_bullets = 50;
int maxhealth = 100; // was MAXHEALTH as a #define, used only in this module
int max_armor = 200;
int green_armor_class = 1;  // these are involved with armortype below
int blue_armor_class = 2;
int max_soul = 200;
int soul_health = 100;
int mega_health = 200;
int god_health = 100;   // these are used in cheats (see st_stuff.c)
int idfa_armor = 200;
int idfa_armor_class = 2;
// not actually used due to pairing of cheat_k and cheat_fa
int idkfa_armor = 200;
int idkfa_armor_class = 2;

int bfgcells = 40;      // used in p_pspr.c
// Ty 03/07/98 - end deh externals

// a weapon is found with two clip loads,
// a big item has five clip loads
int maxammo[NUMAMMO]  = {200, 50, 300, 50};
int clipammo[NUMAMMO] = { 10,  4,  20,  1};

//
// GET STUFF
//

//
// P_GiveAmmo
//
// Num is the number of clip loads,
// not the individual count (0= 1/2 clip).
// Returns false if the ammo can't be picked up at all
//
bool P_GiveAmmo(player_t *player, ammotype_t ammo, int num)
{
   int oldammo;
   int playernum = player - players;
   int old_pending_weapon = player->pendingweapon;
   
   if(ammo == am_noammo)
      return false;
   
   // haleyjd: Part of the campaign to eliminate unnecessary
   //   I_Error bombouts for things that can be ignored safely
   if((unsigned int)ammo >= NUMAMMO)
   {
      doom_printf(FC_ERROR "P_GiveAmmo: bad type %d\a\n", ammo);
      return false;
   }

   if(player->ammo[ammo] == player->maxammo[ammo])
      return false;

   if(num)
      num *= clipammo[ammo];
   else
      num = clipammo[ammo]/2;

   // give double ammo in trainer mode, you'll need in nightmare
   if(gameskill == sk_baby || gameskill == sk_nightmare)
      num <<= 1;

   oldammo = player->ammo[ammo];
   player->ammo[ammo] += num;
   
   if(player->ammo[ammo] > player->maxammo[ammo])
      player->ammo[ammo] = player->maxammo[ammo];
   
   // If non zero ammo, don't change up weapons, player was lower on purpose.
   if(oldammo)
      return true;

   // We were down to zero, so select a new weapon.
   // Preferences are not user selectable.
   // [CG] Preferences are now user selectable based on PWO.

   // WEAPON_FIXME: ammo reception behaviors

   if(GET_ASOP(playernum) == AMMO_SWITCH_VANILLA ||
      (clientserver &&
       ((dmflags2 & dmf_allow_no_weapon_switch_on_pickup) == 0)))
   {
      switch (ammo)
      {
      case am_clip:
         if(player->readyweapon == wp_fist)
         {
            if(player->weaponowned[wp_chaingun])
               player->pendingweapon = wp_chaingun;
            else
               player->pendingweapon = wp_pistol;
         }
         break;

      case am_shell:
         if(player->readyweapon == wp_fist || player->readyweapon == wp_pistol)
            if(player->weaponowned[wp_shotgun])
               player->pendingweapon = wp_shotgun;
         break;

      case am_cell:
         if(player->readyweapon == wp_fist || player->readyweapon == wp_pistol)
            if(player->weaponowned[wp_plasma])
               player->pendingweapon = wp_plasma;
         break;

      case am_misl:
         if(player->readyweapon == wp_fist)
            if(player->weaponowned[wp_missile])
               player->pendingweapon = wp_missile;
         break;

      default:
         break;
      }
   }
   else if(GET_ASOP(playernum) == AMMO_SWITCH_USE_PWO &&
           (!clientserver || (dmflags2 & dmf_allow_preferred_weapon_order)))
   {
      switch (ammo)
      {
      case am_clip:
         if(player->weaponowned[wp_chaingun] && player->weaponowned[wp_pistol])
         {
            if(CS_WeaponPreferred(playernum, wp_chaingun, wp_pistol))
            {
               if(CS_WeaponPreferred(playernum, wp_chaingun,
                                     player->readyweapon))
               {
                  player->pendingweapon = wp_chaingun;
               }
               else if(CS_WeaponPreferred(playernum, wp_pistol,
                                          player->readyweapon))
               {
                  player->pendingweapon = wp_pistol;
               }
            }
            else if(CS_WeaponPreferred(playernum, wp_pistol,
                                       player->readyweapon))
            {
               player->pendingweapon = wp_pistol;
            }
         }
         else if(player->weaponowned[wp_chaingun])
         {
            if(CS_WeaponPreferred(playernum, wp_chaingun, player->readyweapon))
            {
               player->pendingweapon = wp_chaingun;
            }
         }
         else if(player->weaponowned[wp_pistol])
         {
            if(CS_WeaponPreferred(playernum, wp_pistol, player->readyweapon))
            {
               player->pendingweapon = wp_pistol;
            }
         }
         break;

      case am_shell:
         if(player->weaponowned[wp_supershotgun] &&
            player->weaponowned[wp_shotgun])
         {
            if(CS_WeaponPreferred(playernum, wp_supershotgun, wp_shotgun))
            {
               if(CS_WeaponPreferred(playernum, wp_supershotgun,
                                     player->readyweapon))
               {
                  player->pendingweapon = wp_supershotgun;
               }
               else if(CS_WeaponPreferred(playernum, wp_shotgun,
                                          player->readyweapon))
               {
                  player->pendingweapon = wp_shotgun;
               }
            }
            else if(CS_WeaponPreferred(playernum, wp_shotgun,
                                       player->readyweapon))
            {
               player->pendingweapon = wp_shotgun;
            }
         }
         else if(player->weaponowned[wp_supershotgun])
         {
            if(CS_WeaponPreferred(playernum, wp_supershotgun,
                                  player->readyweapon))
            {
               player->pendingweapon = wp_supershotgun;
            }
         }
         else if(player->weaponowned[wp_plasma])
         {
            if(CS_WeaponPreferred(playernum, wp_plasma, player->readyweapon))
            {
               player->pendingweapon = wp_plasma;
            }
         }
         break;

      case am_cell:
         if(player->weaponowned[wp_bfg] && player->weaponowned[wp_plasma])
         {
            if(CS_WeaponPreferred(playernum, wp_bfg, wp_plasma))
            {
               if(CS_WeaponPreferred(playernum, wp_bfg, player->readyweapon))
               {
                  player->pendingweapon = wp_bfg;
               }
               else if(CS_WeaponPreferred(playernum, wp_plasma,
                                          player->readyweapon))
               {
                  player->pendingweapon = wp_plasma;
               }
            }
            else if(CS_WeaponPreferred(playernum, wp_plasma,
                                       player->readyweapon))
            {
               player->pendingweapon = wp_plasma;
            }
         }
         else if(player->weaponowned[wp_bfg])
         {
            if(CS_WeaponPreferred(playernum, wp_bfg, player->readyweapon))
            {
               player->pendingweapon = wp_bfg;
            }
         }
         else if(player->weaponowned[wp_plasma])
         {
            if(CS_WeaponPreferred(playernum, wp_plasma, player->readyweapon))
            {
               player->pendingweapon = wp_plasma;
            }
         }
         break;

      case am_misl:
         if(player->weaponowned[wp_missile])
         {
            if(CS_WeaponPreferred(playernum, wp_missile, player->readyweapon))
            {
               player->pendingweapon = wp_missile;
            }
         }
         break;

      default:
         break;
      }
   }

   if(CS_SERVER && player->pendingweapon != old_pending_weapon)
      SV_BroadcastPlayerScalarInfo(playernum, ci_pending_weapon);

   return true;
}

//
// P_GiveWeapon
//
// The weapon name may have a MF_DROPPED flag ored in.
//
bool P_GiveWeapon(player_t *player, weapontype_t weapon, bool dropped)
{
   int playernum = player - players;
   bool gaveweapon = false;
   bool gaveammo;
   
   if((dmflags & DM_WEAPONSTAY) && !dropped)
   {
      // leave placed weapons forever on net games
      if(player->weaponowned[weapon])
         return false;

      player->bonuscount += BONUSADD;
      player->weaponowned[weapon] = true;
      
      P_GiveAmmo(player, weaponinfo[weapon].ammo, (DEATHMATCH) ? 5 : 2);

      if(!clientserver) // [CG] Probably needs demo-versioned.
      {
         player->pendingweapon = weapon;
         S_StartSound(player->mo, sfx_wpnup); // killough 4/25/98, 12/98
         return false;
      }
      
      // [CG] Allow players to pick up weapons without switching to them.
      if(GET_WSOP(playernum) == WEAPON_SWITCH_ALWAYS ||
         (clientserver &&
          ((dmflags2 & dmf_allow_no_weapon_switch_on_pickup) == 0)))
      {
         player->pendingweapon = weapon;
      }
      else if(GET_WSOP(playernum) == WEAPON_SWITCH_USE_PWO &&
              (!clientserver || (dmflags2 & dmf_allow_preferred_weapon_order)))
      {
         if(CS_WeaponPreferred(playernum, weapon, player->readyweapon))
         {
            player->pendingweapon = weapon;
            if(CS_SERVER)
               SV_BroadcastPlayerScalarInfo(playernum, ci_pending_weapon);
         }
      }

      // [CG] Allow players to pickup weapons silently (they hear the noise
      //      themselves, but other players do not) if the server is so
      //      configured.
      if(!clientserver ||
         player == &players[consoleplayer] ||
         ((dmflags2 & dmf_silent_weapon_pickup) == 0))
      {
         S_StartSound(player->mo, sfx_wpnup); // killough 4/25/98, 12/98
      }

      // [CG] In traditional Deathmatch, weapon pickup messages are not
      //      printed.  This is not the way c/s Deathmatch works, however, so
      //      we must return true here if in c/s mode..
      if(clientserver)
         return true;
      else
         return false;
   }

   // give one clip with a dropped weapon, two clips with a found weapon
   gaveammo = weaponinfo[weapon].ammo != am_noammo &&
      P_GiveAmmo(player, weaponinfo[weapon].ammo, dropped ? 1 : 2);
   
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
bool P_GiveBody(player_t *player, int num)
{
   int maxhealthtouse;

   // haleyjd 11/14/09: compatibility fix.
   // The DeHackEd maxhealth setting was only supposed to affect health
   // potions, but when Ty replaced the MAXHEALTH define in this module,
   // he replaced all uses of it, including here. We need to handle 
   // multiple cases for demo compatibility.
   if(demo_version >= 200 && demo_version < 335) 
      maxhealthtouse = maxhealth;
   else
      maxhealthtouse = 100;

   if(player->health >= maxhealthtouse)
      return false; // Ty 03/09/98 externalized MAXHEALTH to maxhealth
   player->health += num;
   if(player->health > maxhealthtouse)
      player->health = maxhealthtouse;
   player->mo->health = player->health;
   return true;
}

//
// P_GiveArmor
//
// Returns false if the armor is worse
// than the current armor.
//
bool P_GiveArmor(player_t *player, int armortype, bool htic)
{
   int hits = armortype*100;

   if(player->armorpoints >= hits)
      return false;   // don't pick up

   player->armortype = armortype;
   player->armorpoints = hits;
   
   // haleyjd 10/10/02 -- TODO/FIXME: hack!
   player->hereticarmor = player->hereticarmor || htic;
   
   return true;
}

//
// P_GiveCard
//
void P_GiveCard(player_t *player, card_t card)
{
   if(player->cards[card])
      return;
   player->bonuscount = BONUSADD;
   player->cards[card] = 1;
}

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
      1,          /* strength */
      INVISTICS,
      IRONTICS, 
      1,          /* allmap */
      INFRATICS,
      INVISTICS,  /* haleyjd: totalinvis */
      INVISTICS,  /* haleyjd: ghost */
      1,          /* haleyjd: silencer */
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
      P_GiveBody(player,100);
      break;
   case pw_totalinvis:   // haleyjd: total invisibility
      player->mo->flags2 |= MF2_DONTDRAW;
      break;
   case pw_ghost:        // haleyjd: heretic ghost
      player->mo->flags3 |= MF3_GHOST;
      break;
   case pw_silencer:
      if(player->powers[pw_silencer])
         return false;
      break;
   }

   // Unless player has infinite duration cheat, set duration (killough)
   
   if(player->powers[power] >= 0)
      player->powers[power] = tics[power];
   return true;
}

//
// P_TouchSpecialThing
//
void P_TouchSpecialThing(Mobj *special, Mobj *toucher)
{
   player_t   *player;
   int        i, sound;
   const char *message = NULL;
   bool       removeobj = true;
   bool       pickup_fx = true; // haleyjd 04/14/03
   fixed_t    delta = special->z - toucher->z;

   if(serverside && (delta > toucher->height || delta < -8*FRACUNIT))
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

   if(CS_SERVER)
      SV_BroadcastPlayerTouchedSpecial(player - players, special->net_id);

   // Identify by sprite.
   switch(pickupfx[special->sprite])
   {
      // armor
   case PFX_GREENARMOR:
      if(!P_GiveArmor(player, green_armor_class, false))
         return;
      message = DEH_String("GOTARMOR"); // Ty 03/22/98 - externalized
      break;

   case PFX_BLUEARMOR:
      if(!P_GiveArmor(player, blue_armor_class, false))
         return;
      message = DEH_String("GOTMEGA"); // Ty 03/22/98 - externalized
      break;

      // bonus items
   case PFX_POTION:
      // sf: removed beta
      player->health++;               // can go over 100%
      if(player->health > (maxhealth * 2))
         player->health = (maxhealth * 2);
      player->mo->health = player->health;
      message = DEH_String("GOTHTHBONUS"); // Ty 03/22/98 - externalized
      break;

   case PFX_ARMORBONUS:
      // sf: removed beta
      player->armorpoints++;          // can go over 100%
      if(player->armorpoints > max_armor)
         player->armorpoints = max_armor;
      if(!player->armortype)
         player->armortype = green_armor_class;
      message = DEH_String("GOTARMBONUS"); // Ty 03/22/98 - externalized
      break;

      // sf: removed beta items
      
   case PFX_SOULSPHERE:
      player->health += soul_health;
      if(player->health > max_soul)
         player->health = max_soul;
      player->mo->health = player->health;
      message = DEH_String("GOTSUPER"); // Ty 03/22/98 - externalized
      sound = sfx_getpow;
      break;

   case PFX_MEGASPHERE:
      if(demo_version < 335 && GameModeInfo->id != commercial)
         return;
      player->health = mega_health;
      player->mo->health = player->health;
      P_GiveArmor(player,blue_armor_class, false);
      message = DEH_String("GOTMSPHERE"); // Ty 03/22/98 - externalized
      sound = sfx_getpow;
      break;

      // cards
      // leave cards for everyone
   case PFX_BLUEKEY:
      if(!player->cards[it_bluecard])
         message = DEH_String("GOTBLUECARD"); // Ty 03/22/98 - externalized
      P_GiveCard(player, it_bluecard);
      if(clientserver)
         removeobj = pickup_fx = ((dmflags & dmf_leave_keys) == 0);
      else
         removeobj = pickup_fx = (GameType == gt_single);
      break;

   case PFX_YELLOWKEY:
      if(!player->cards[it_yellowcard])
         message = DEH_String("GOTYELWCARD"); // Ty 03/22/98 - externalized
      P_GiveCard(player, it_yellowcard);
      if(clientserver)
         removeobj = pickup_fx = ((dmflags & dmf_leave_keys) == 0);
      else
         removeobj = pickup_fx = (GameType == gt_single);
      break;

   case PFX_REDKEY:
      if(!player->cards[it_redcard])
         message = DEH_String("GOTREDCARD"); // Ty 03/22/98 - externalized
      P_GiveCard(player, it_redcard);
      if(clientserver)
         removeobj = pickup_fx = ((dmflags & dmf_leave_keys) == 0);
      else
         removeobj = pickup_fx = (GameType == gt_single);
      break;
      
   case PFX_BLUESKULL:
      if(!player->cards[it_blueskull])
         message = DEH_String("GOTBLUESKUL"); // Ty 03/22/98 - externalized
      P_GiveCard(player, it_blueskull);
      if(clientserver)
         removeobj = pickup_fx = ((dmflags & dmf_leave_keys) == 0);
      else
         removeobj = pickup_fx = (GameType == gt_single);
      break;

   case PFX_YELLOWSKULL:
      if(!player->cards[it_yellowskull])
         message = DEH_String("GOTYELWSKUL"); // Ty 03/22/98 - externalized
      P_GiveCard(player, it_yellowskull);
      if(clientserver)
         removeobj = pickup_fx = ((dmflags & dmf_leave_keys) == 0);
      else
         removeobj = pickup_fx = (GameType == gt_single);
      break;

   case PFX_REDSKULL:
      if(!player->cards[it_redskull])
         message = DEH_String("GOTREDSKULL"); // Ty 03/22/98 - externalized
      P_GiveCard(player, it_redskull);
      if(clientserver)
         removeobj = pickup_fx = ((dmflags & dmf_leave_keys) == 0);
      else
         removeobj = pickup_fx = (GameType == gt_single);
      break;

   // medikits, heals
   case PFX_STIMPACK:
      if(!P_GiveBody(player, 10))
         return;
      message = DEH_String("GOTSTIM"); // Ty 03/22/98 - externalized
      break;

   case PFX_MEDIKIT:
      if(!P_GiveBody(player, 25))
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
      if(!P_GivePower (player, pw_invisibility))
         return;
      message = DEH_String("GOTINVIS"); // Ty 03/22/98 - externalized
      sound = sfx_getpow;
      break;

   case PFX_RADSUIT:
      if(!P_GivePower(player, pw_ironfeet))
         return;
      // sf:removed beta

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
      // sf:removed beta
      sound = sfx_getpow;
      message = DEH_String("GOTVISOR"); // Ty 03/22/98 - externalized
      break;

      // ammo
   case PFX_CLIP:
      if(special->flags & MF_DROPPED)
      {
         if(!P_GiveAmmo(player,am_clip,0))
            return;
      }
      else
      {
         if(!P_GiveAmmo(player,am_clip,1))
            return;
      }
      message = DEH_String("GOTCLIP"); // Ty 03/22/98 - externalized
      break;

   case PFX_CLIPBOX:
      if(!P_GiveAmmo(player, am_clip,5))
         return;
      message = DEH_String("GOTCLIPBOX"); // Ty 03/22/98 - externalized
      break;

   case PFX_ROCKET:
      if(!P_GiveAmmo(player, am_misl,1))
         return;
      message = DEH_String("GOTROCKET"); // Ty 03/22/98 - externalized
      break;

   case PFX_ROCKETBOX:
      if(!P_GiveAmmo(player, am_misl,5))
         return;
      message = DEH_String("GOTROCKBOX"); // Ty 03/22/98 - externalized
      break;

   case PFX_CELL:
      if(!P_GiveAmmo(player, am_cell,1))
         return;
      message = DEH_String("GOTCELL"); // Ty 03/22/98 - externalized
      break;

   case PFX_CELLPACK:
      if(!P_GiveAmmo(player, am_cell,5))
         return;
      message = DEH_String("GOTCELLBOX"); // Ty 03/22/98 - externalized
      break;

   case PFX_SHELL:
      if(!P_GiveAmmo(player, am_shell,1))
         return;
      message = DEH_String("GOTSHELLS"); // Ty 03/22/98 - externalized
      break;

   case PFX_SHELLBOX:
      if(!P_GiveAmmo(player, am_shell,5))
         return;
      message = DEH_String("GOTSHELLBOX"); // Ty 03/22/98 - externalized
      break;

   case PFX_BACKPACK:
      if(!player->backpack)
      {
         for (i=0 ; i<NUMAMMO ; i++)
            player->maxammo[i] *= 2;
         player->backpack = true;
      }
      // EDF FIXME: needs a ton of work
#if 0
      if(special->flags & MF_DROPPED)
      {
         int i;
         for(i=0 ; i<NUMAMMO ; i++)
         {
            player->ammo[i] +=special->extradata.backpack->ammo[i];
            if(player->ammo[i]>player->maxammo[i])
               player->ammo[i]=player->maxammo[i];
         }
         P_GiveWeapon(player,special->extradata.backpack->weapon,true);
         Z_Free(special->extradata.backpack);
         message = "got player backpack";
      }
      else
#endif
      {
         for(i = 0; i < NUMAMMO; ++i)
            P_GiveAmmo(player, i, 1);
         message = DEH_String("GOTBACKPACK"); // Ty 03/22/98 - externalized
      }

      break;

      // weapons
   case PFX_BFG:
      if(!P_GiveWeapon(player, wp_bfg, false))
         return;
      removeobj = !clientserver || ((dmflags & dmf_leave_weapons) == 0);
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
      if(!P_GiveWeapon(player, wp_chaingun, special->flags & MF_DROPPED))
         return;
      removeobj = special->flags & MF_DROPPED ||
                  ((dmflags & dmf_leave_weapons) == 0);
      message = DEH_String("GOTCHAINGUN"); // Ty 03/22/98 - externalized
      sound = sfx_wpnup;
      break;

   case PFX_CHAINSAW:
      if(!P_GiveWeapon(player, wp_chainsaw, false))
         return;
      removeobj = special->flags & MF_DROPPED ||
                  ((dmflags & dmf_leave_weapons) == 0);
      message = DEH_String("GOTCHAINSAW"); // Ty 03/22/98 - externalized
      sound = sfx_wpnup;
      break;

   case PFX_LAUNCHER:
      if(!P_GiveWeapon(player, wp_missile, false) )
         return;
      removeobj = special->flags & MF_DROPPED ||
                  ((dmflags & dmf_leave_weapons) == 0);
      message = DEH_String("GOTLAUNCHER"); // Ty 03/22/98 - externalized
      sound = sfx_wpnup;
      break;

   case PFX_PLASMA:
      if(!P_GiveWeapon(player, wp_plasma, false))
         return;
      removeobj = special->flags & MF_DROPPED ||
                  ((dmflags & dmf_leave_weapons) == 0);
      message = DEH_String("GOTPLASMA"); // Ty 03/22/98 - externalized
      sound = sfx_wpnup;
      break;

   case PFX_SHOTGUN:
      if(!P_GiveWeapon(player, wp_shotgun, special->flags & MF_DROPPED))
         return;
      removeobj = special->flags & MF_DROPPED ||
                  ((dmflags & dmf_leave_weapons) == 0);
      message = DEH_String("GOTSHOTGUN"); // Ty 03/22/98 - externalized
      sound = sfx_wpnup;
      break;

   case PFX_SSG:
      if(!P_GiveWeapon(player, wp_supershotgun, special->flags & MF_DROPPED))
         return;
      removeobj = special->flags & MF_DROPPED ||
                  ((dmflags & dmf_leave_weapons) == 0);
      message = DEH_String("GOTSHOTGUN2"); // Ty 03/22/98 - externalized
      sound = sfx_wpnup;
      break;

      // haleyjd 10/10/02: Heretic powerups

      // heretic keys: give both card and skull equivalent DOOM keys
   case PFX_HGREENKEY: // green key (red in doom)
      if(!player->cards[it_redcard])
         message = DEH_String("HGOTGREENKEY");
      P_GiveCard(player, it_redcard);
      P_GiveCard(player, it_redskull);
      if(clientserver)
         removeobj = pickup_fx = ((dmflags & dmf_leave_keys) == 0);
      else
         removeobj = pickup_fx = (GameType == gt_single);
      sound = sfx_keyup;
      break;

   case PFX_HBLUEKEY: // blue key (blue in doom)
      if(!player->cards[it_bluecard])
         message = DEH_String("HGOTBLUEKEY");
      P_GiveCard(player, it_bluecard);
      P_GiveCard(player, it_blueskull);
      if(clientserver)
         removeobj = pickup_fx = ((dmflags & dmf_leave_keys) == 0);
      else
         removeobj = pickup_fx = (GameType == gt_single);
      sound = sfx_keyup;
      break;

   case PFX_HYELLOWKEY: // yellow key (yellow in doom)
      if(!player->cards[it_yellowcard])
         message = DEH_String("HGOTYELLOWKEY");
      P_GiveCard(player, it_yellowcard);
      P_GiveCard(player, it_yellowskull);
      if(clientserver)
         removeobj = pickup_fx = ((dmflags & dmf_leave_keys) == 0);
      else
         removeobj = pickup_fx = (GameType == gt_single);
      sound = sfx_keyup;
      break;

   case PFX_HPOTION: // heretic potion
      if(!P_GiveBody(player, 10))
         return;
      message = DEH_String("HITEMHEALTH");
      sound = sfx_hitemup;
      break;

   case PFX_SILVERSHIELD: // heretic shield 1
      if(!P_GiveArmor(player, 1, true))
         return;
      message = DEH_String("HITEMSHIELD1");
      sound = sfx_hitemup;
      break;

   case PFX_ENCHANTEDSHIELD: // heretic shield 2
      if(!P_GiveArmor(player, 2, true))
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

   case PFX_BLUEFLAG:
   case PFX_DROPPEDBLUEFLAG:
      CS_HandleFlagTouch(player, team_color_blue);
      return;

   case PFX_REDFLAG:
   case PFX_DROPPEDREDFLAG:
      CS_HandleFlagTouch(player, team_color_red);
      return;

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

   if(serverside && removeobj)
   {
      if(CS_SERVER)
         SV_BroadcastActorRemoved(special);
      special->removeThinker();
   }

   // haleyjd 07/08/05: inverted condition
   if(pickup_fx)
   {
      player->bonuscount += BONUSADD;
      S_StartSound(player->mo, sound);   // killough 4/25/98, 12/98
   }
}

//
// P_KillMobj
//
void P_KillMobj(Mobj *source, Mobj *target, emod_t *mod)
{
   mobjtype_t item;
   Mobj     *mo;

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
         if(GameType == gt_tdm)
         {
            // [CG] Subtract 1 from the team's score for suicides and team
            //      kills.
            if(source->player == target->player ||
               clients[source->player - players].team ==
               clients[target->player - players].team)
               team_scores[clients[source->player - players].team]--;
            else
               team_scores[clients[source->player - players].team]++;
         }
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
      // [CG] Keep track of deaths.
      clients[target->player - players].death_count++;

      // count environment kills against you
      if(!source)
      {
         target->player->frags[target->player-players]++;

         // [CG] Subtract 1 from the team's score for environment kills.
         if(GameType == gt_tdm)
            team_scores[clients[target->player - players].team]--;

         HU_FragsUpdate();
      }
      // [CG] Drop the flag if the target was holding it.
      if(GameType == gt_ctf)
         CS_DropFlag(target->player - players);

      target->flags &= ~MF_SOLID;
      target->player->playerstate = PST_DEAD;

      // [CG] Only drop the player's weapon if their actor still exists.
      //      This check only occurs in c/s, because it is sync-critical.
      if(!clientserver || target->player->mo)
         P_DropWeapon(target->player);

      if(target->player == &players[consoleplayer] && automapactive)
      {
         if(!demoplayback) // killough 11/98: don't switch out in demos, though
            AM_Stop();    // don't die in auto map; switch view prior to dying
      }
   }

   if(target->health < -target->info->spawnhealth &&
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

   if(target->info->droptype != -1)
   {
      if(target->player)
      {
         // players only drop backpacks if so indicated
         if(!(dmflags & DM_PLAYERDROP))
            return;
      }

      item = target->info->droptype;
   }
   else
      return;

   if(serverside)
   {
      mo = P_SpawnMobj(target->x, target->y, ONFLOORZ, item);
      mo->flags |= MF_DROPPED;    // special versions of items

      if(CS_SERVER)
         SV_BroadcastActorSpawned(mo);
   }

   // EDF FIXME: problematic, needed work to begin with
#if 0
   if(mo->type == MT_MISC24) // put all the players stuff into the
   {                         // backpack
      int a;
      mo->extradata.backpack = (backpack_t *)(Z_Malloc(sizeof(backpack_t), PU_LEVEL, NULL));
      for(a=0; a<NUMAMMO; a++)
         mo->extradata.backpack->ammo[a] = target->player->ammo[a];
      mo->extradata.backpack->weapon = target->player->readyweapon;
      // set the backpack moving slightly faster than the player
      
      // start it moving in a (fairly) random direction
      // i cant be bothered to create a new random number
      // class right now
      // haleyjd: not demo safe, fix later (TODO)
      /*
      mo->momx = target->momx * (gametic-basetic) % 5;
      mo->momy = target->momy * (gametic-basetic+30) % 5;
      */
   }
#endif
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
void P_DeathMessage(Mobj *source, Mobj *target, Mobj *inflictor, 
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
// [CG] 09/17/11: Made some significant modifications to support c/s.
//
void P_DamageMobj(Mobj *target, Mobj *inflictor, Mobj *source, 
                  int damage, int mod)
{
   int armor_damage = 0;
   bool dealt_damage = false;
   bool justhit = false;  // killough 11/98
   emod_t *emod;
   player_t *player;

   // [CG] This function is only run serverside.
   if(!serverside)
      return;
   
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
      double df = meta->getDouble(E_ModFieldName("damagefactor", emod), 1.0);
         
      damage = (int)(damage * df);
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
         linkoffset_t *link;

         if(inflictor->groupid == target->groupid ||
            !(link = P_GetLinkOffset(inflictor->groupid, target->groupid)))
         {
            ang = P_PointToAngle (inflictor->x, inflictor->y, 
                                   target->x, target->y);
         }
         else
         {
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

      // [CG] Handle friendly damage.
      if(source && source->player && source->player != player)
      {
         if(friendly_damage_percentage < 100 && (GameType == gt_coop || (
            CS_TEAMS_ENABLED &&
            clients[target->player - players].team ==
            clients[source->player - players].team)))
         {
            damage *= cs_settings->friendly_damage_percentage;
            damage /= 100;
         }
      }

      // end of game hell hack
      if(target->subsector->sector->special == 11 && damage >= target->health)
         damage = target->health - 1;

      // Below certain threshold,
      // ignore damage in GOD mode, or with INVUL power.
      // killough 3/26/98: make god mode 100% god mode in non-compat mode

      if((damage < 1000 || (!comp[comp_god] && player->cheats&CF_GODMODE)) &&
         (player->cheats&CF_GODMODE || player->powers[pw_invulnerability]))
         return;

      if(player->armortype)
      {
         // haleyjd 10/10/02: heretic armor -- its better! :P
         // HTIC_FIXME
         if(player->hereticarmor)
            armor_damage = player->armortype == 1 ? damage/2 : (damage*3)/4;
         else
            armor_damage = player->armortype == 1 ? damage/3 : damage/2;
         
         if(player->armorpoints <= armor_damage)
         {
            // armor is used up
            armor_damage = player->armorpoints;
            player->armortype = 0;
            // haleyjd 10/10/02
            player->hereticarmor = false;
         }
         player->armorpoints -= armor_damage;
         damage -= armor_damage;
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

#if 0
      // killough 11/98: 
      // This is unused -- perhaps it was designed for
      // a hand-connected input device or VR helmet,
      // to pinch the player when they're hurt :)

      // haleyjd: this was for the "CyberMan" 3D mouse ^_^
      
      {
         int temp = damage < 100 ? damage : 100;
         
         if(player == &players[consoleplayer])
            I_Tactile(40,10,40+temp*2);
      }
#endif
      
   }

   // do the damage
   if(!(target->flags4 & MF4_NODAMAGE) || damage >= 10000)
   {
      target->health -= damage;
      dealt_damage = true;
   }

   // check for death
   if(target->health <= 0)
   {
      if(dealt_damage && CS_SERVER)
      {
         SV_BroadcastActorDamaged(target, inflictor, source, damage,
                                  armor_damage, mod, true, false);
      }

      // death messages for players
      if(player)
      {
         // haleyjd 12/29/10: immortality cheat
         // [CG] Disabled in c/s.
         if(!clientserver && (player->cheats & CF_IMMORTAL))
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
      if(CS_SERVER)
         SV_BroadcastActorKilled(target, inflictor, source, damage, mod);
   }
   else
   {
      // [CG] Broke out P_HandleDamagedActor from here.
      justhit = P_HandleDamagedActor(target, source, damage, mod, emod);

      if(dealt_damage && CS_SERVER)
      {
         SV_BroadcastActorDamaged(
            target, inflictor, source, damage, armor_damage, mod, false,
            justhit
         );
      }
   }
}

// [CG] Broke out of P_DamageMobj.
bool P_HandleDamagedActor(Mobj *target, Mobj *source, int damage, int mod,
                          emod_t *emod)
{
   bool justhit = false;  // killough 11/98
   bool bossignore;       // haleyjd

   // haleyjd: special death hacks: if we got here, we didn't really die
   target->intflags &= ~(MIF_DIEDFALLING|MIF_WIMPYDEATH);

   // killough 9/7/98: keep track of targets so that friends can help friends
   if(demo_version >= 203)
   {
      // If target is a player, set player's target to source,
      // so that a friend can tell who's hurting a player
      if(serverside || cl_handling_damaged_actor)
      {
         if(target->player)
         {
            P_SetTarget<Mobj>(&target->target, source);
            if(CS_SERVER)
               SV_BroadcastActorTarget(target, CS_AT_TARGET);
         }
      }
      
      // killough 9/8/98:
      // If target's health is less than 50%, move it to the front of its list.
      // This will slightly increase the chances that enemies will choose to
      // "finish it off", but its main purpose is to alert friends of danger.

      if(target->health * 2 < target->info->spawnhealth)
      {
         Thinker *cap = 
            &thinkerclasscap[target->flags & MF_FRIEND ? 
                             th_friends : th_enemies];
         (target->cprev->cnext = target->cnext)->cprev = target->cprev;
         (target->cnext = cap->cnext)->cprev = target;
         (target->cprev = cap)->cnext = target;
      }
   }

   if((cl_handling_damaged_actor && cl_handling_damaged_actor_and_justhit) ||
      (P_Random(pr_painchance) < target->info->painchance &&
      !(target->flags & MF_SKULLFLY)))
   { 
      //killough 11/98: see below
      if(demo_version >= 203)
         justhit = true;
      else
         target->flags |= MF_JUSTHIT;    // fight back!

      // haleyjd 10/06/99: remove pain for zero-damage projectiles
      // FIXME: this needs a comp flag
      if(damage > 0 || demo_version < 329)
      {
         statenum_t st = target->info->painstate;
         state_t *state = NULL;

         // haleyjd  06/05/08: check for special damagetype painstate
         if(mod > 0 && (state = E_StateForMod(target->info, "Pain", emod)))
            st = state->index;

         P_SetMobjState(target, st);
      }
   }
   
   target->reactiontime = 0;           // we're awake now...

   // killough 9/9/98: cleaned up, made more consistent:
   // haleyjd 11/24/02: added MF3_DMGIGNORED and MF3_BOSSIGNORE flags

   // EDF_FIXME: replace BOSSIGNORE with generalized infighting controls.
   // BOSSIGNORE flag will be made obsolete.

   // haleyjd: set bossignore
   if(source && (source->type != target->type) &&
      (source->flags3 & target->flags3 & MF3_BOSSIGNORE))
   {
      // ignore if friendliness matches
      bossignore = !((source->flags ^ target->flags) & MF_FRIEND);
   }
   else
      bossignore = false;

   // Set target based on the following criteria:
   // * Damage is sourced and source is not self.
   // * Source damage is not ignored via MF3_DMGIGNORED.
   // * Threshold is expired, or target has no threshold via MF3_NOTHRESHOLD.
   // * Things are not friends, or monster infighting is enabled.
   // * Things are not both friends while target is not a SUPERFRIEND

   // TODO: add fine-grained infighting control as metadata
   
   if(source && source != target                                     // source checks
      && !(source->flags3 & MF3_DMGIGNORED)                          // not ignored?
      && !bossignore                                                 // EDF_FIXME!
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
         if(serverside || cl_handling_damaged_actor)
         {
            P_SetTarget<Mobj>(&target->lastenemy, target->target);
            if(CS_SERVER)
               SV_BroadcastActorTarget(target, CS_AT_LASTENEMY);
         }
      }

      if(serverside || cl_handling_damaged_actor)
      {
         P_SetTarget<Mobj>(&target->target, source);       // killough 11/98
         if(CS_SERVER)
            SV_BroadcastActorTarget(target, CS_AT_TARGET);
      }

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

   return justhit;
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
         Mobj *fog;
         
         if(serverside)
         {
            fog = P_SpawnMobj(prevx, prevy, 
                                   prevz + GameModeInfo->teleFogHeight,
                                   GameModeInfo->teleFogType);
            if(CS_SERVER)
               SV_BroadcastActorSpawned(fog);
            S_StartSound(fog, GameModeInfo->teleSound);

            fog = P_SpawnMobj(x, y, z + GameModeInfo->teleFogHeight,
                              GameModeInfo->teleFogType);
            if(CS_SERVER)
               SV_BroadcastActorSpawned(fog);
            S_StartSound(fog, GameModeInfo->teleSound);
         }

         // put the thing into its spawnstate and keep it still
         P_SetMobjState(mo, mo->info->spawnstate);
         mo->z = mo->floorz;
         mo->momx = mo->momy = mo->momz = 0;

         return;
      }
   }
}

#ifndef EE_NO_SMALL_SUPPORT
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

