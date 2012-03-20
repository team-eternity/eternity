// Emacs style mode select   -*- C++ -*-
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
//      Weapon sprite animation, weapon objects.
//      Action functions for weapons.
//
// NETCODE_FIXME -- DEMO_FIXME -- WEAPON_FIXME: Weapon changing, prefs,
// etc. need overhaul for all of these. See comments in other modules
// for why and how it needs to be changed. zdoom uses separate events
// outside of ticcmd_t to indicate weapon changes now, and so will not
// face the issue of being limited to 16 weapons.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "doomstat.h"
#include "d_event.h"
#include "d_mod.h"
#include "g_game.h"
#include "m_random.h"
#include "p_enemy.h"
#include "p_inter.h"
#include "p_clipen.h"
#include "p_mapcontext.h"
#include "p_traceengine.h"
#include "p_maputl.h"
#include "p_pspr.h"
#include "p_skin.h"
#include "p_tick.h"
#include "r_main.h"
#include "r_segs.h"
#include "r_things.h"
#include "s_sound.h"
#include "sounds.h"
#include "e_states.h"
#include "e_things.h"
#include "e_sound.h"
#include "d_dehtbl.h"
#include "p_info.h"
#include "d_gi.h"
#include "e_lib.h"
#include "e_args.h"
#include "e_player.h"

int weapon_speed = 6;
int default_weapon_speed = 6;

#define LOWERSPEED   (FRACUNIT*weapon_speed)
#define RAISESPEED   (FRACUNIT*weapon_speed)
#define WEAPONBOTTOM (FRACUNIT*128)
#define WEAPONTOP    (FRACUNIT*32)

#define BFGCELLS bfgcells        /* Ty 03/09/98 externalized in p_inter.c */

extern void P_Thrust(player_t *, angle_t, fixed_t);

// The following array holds the recoil values         // phares
// haleyjd 08/18/08: recoil moved into weaponinfo

// haleyjd 05/21/08:
// This global is only asserted while an action function is being dispatched
// from inside P_SetPsprite. This allows codepointer functions to behave 
// differently if called by Mobj's or by player weapons.

int action_from_pspr;

//=============================================================================
//
// Gun actions
//
// This structure is used to support copying of args between gun and player
// mobj states during execution of gun codepointer actions so that pointers
// designed to work with Mobj's can work seamlessly with guns as well.
//

typedef struct gunaction_s
{
   state_t *s;               // state args were copied to
   arglist_t *args;          // pointer to args object
   struct gunaction_s *next; // next action
} gunaction_t;

static gunaction_t *gunactions;
static gunaction_t *freegunactions;

//
// P_GetGunAction
//
// haleyjd 07/20/09: Keep gunactions on a freelist to avoid a ton of
// allocator noise.
//
static gunaction_t *P_GetGunAction(void)
{
   gunaction_t *ret;

   if(freegunactions)
   {
      ret = freegunactions;
      freegunactions = ret->next;
   }
   else
      ret = ecalloc(gunaction_t *, 1, sizeof(gunaction_t));

   return ret;
}

//
// P_PutGunAction
//
// Put a gunaction onto the freelist.
//
static void P_PutGunAction(gunaction_t *ga)
{
   memset(ga, 0, sizeof(gunaction_t));

   ga->next = freegunactions;
   freegunactions = ga;
}

//
// P_SetupPlayerGunAction
//
// Enables copying of psp->state->args to mo->state->args during psprite
// action function callback so that parameterized pointers work seamlessly.
//
static void P_SetupPlayerGunAction(player_t *player, pspdef_t *psp)
{
   // create a new gunaction
   gunaction_t *ga;
   Mobj *mo = player->mo;

   ga = P_GetGunAction();

   ga->args        = mo->state->args;  // save the original args
   mo->state->args = psp->state->args; // copy from the psprite frame

   action_from_pspr++;

   // save pointer to state
   ga->s = mo->state;

   // put gun action on the stack
   ga->next   = gunactions;
   gunactions = ga;
}

//
// P_FinishPlayerGunAction
//
// Fixes the state args back up after a psprite action call.
//
static void P_FinishPlayerGunAction(void)
{
   gunaction_t *ga;

   // take the current gunaction off the stack
   if(!gunactions)
      I_Error("P_FinishPlayerGunAction: stack underflow\n");

   ga = gunactions;
   gunactions = ga->next;

   // restore saved args object to the state
   ga->s->args = ga->args;

   action_from_pspr--;

   // free the gunaction
   P_PutGunAction(ga);
}

//=============================================================================

//
// P_SetPsprite
//
// haleyjd 03/31/06: Removed static.
//
void P_SetPsprite(player_t *player, int position, statenum_t stnum)
{
   pspdef_t *psp = &player->psprites[position];

   // haleyjd 04/05/07: codepointer rewrite -- use same prototype for
   // all codepointers by getting player and psp from mo->player. This
   // requires stashing the "position" parameter in player_t, however.

   player->curpsprite = position;

   do
   {
      state_t *state;
      
      if(!stnum)
      {
         // object removed itself
         psp->state = NULL;
         break;
      }

      // WEAPON_FIXME: pre-beta bfg must become an alternate weapon under EDF

      // killough 7/19/98: Pre-Beta BFG
      if(stnum == E_StateNumForDEHNum(S_BFG1) && bfgtype == bfg_classic)
         stnum = E_SafeState(S_OLDBFG1); // Skip to alternative weapon frame

      state = states[stnum];
      psp->state = state;
      psp->tics = state->tics;        // could be 0

      if(state->misc1)
      {
         // coordinate set
         psp->sx = state->misc1 << FRACBITS;
         psp->sy = state->misc2 << FRACBITS;
      }

      // Call action routine.
      // Modified handling.
      if(state->action)
      {
         P_SetupPlayerGunAction(player, psp);

         state->action(player->mo);
         
         P_FinishPlayerGunAction();
         
         if(!psp->state)
            break;
      }
      stnum = psp->state->nextstate;
   }
   while(!psp->tics);   // an initial state of 0 could cycle through
}

//
// P_BringUpWeapon
//
// Starts bringing the pending weapon up from the bottom of the screen.
// Uses player.
//
static void P_BringUpWeapon(player_t *player)
{
   statenum_t newstate;
   
   if(player->pendingweapon == wp_nochange)
      player->pendingweapon = player->readyweapon;
   
   // WEAPON_FIXME: weaponup sound must become EDF property of weapons
   if(player->pendingweapon == wp_chainsaw)
      S_StartSound(player->mo, sfx_sawup);
   
   newstate = weaponinfo[player->pendingweapon].upstate;
   
   player->pendingweapon = wp_nochange;
   
   // killough 12/98: prevent pistol from starting visibly at bottom of screen:
   player->psprites[ps_weapon].sy = demo_version >= 203 ? 
      WEAPONBOTTOM+FRACUNIT*2 : WEAPONBOTTOM;
   
   P_SetPsprite(player, ps_weapon, newstate);
}

// weaponinfo are in a stupid order, so let's do next/last in 
// a more proper manner. Come EDF weapon support, this will become
// unnecessary, as we can go by "slots" instead.
//
// mapping: from ordinal to wp_*
//
static int ordinalToWp[NUMWEAPONS] =
{
   wp_fist, wp_chainsaw, wp_pistol, wp_shotgun,
   wp_supershotgun, wp_chaingun, wp_missile, wp_plasma, wp_bfg
};

// reverse mapping: from wp_* to ordinal number
static int wpToOrdinal[NUMWEAPONS] =
{
   0, 2, 3, 5, 6, 7, 8, 1, 4
};

//
// P_NextWeapon
//
// haleyjd 03/06/09: Yes this will have to be rewritten for EDF weapons,
// but I'm tired of not having it, so here is the temporary version.
//
int P_NextWeapon(player_t *player)
{
   int currentweapon = wpToOrdinal[player->readyweapon];
   int newweapon     = currentweapon;
   int weaptotry;
   int ammototry;

   do
   {
      ++newweapon;

      if(newweapon >= NUMWEAPONS)
         newweapon = 0;

      weaptotry = ordinalToWp[newweapon];
      ammototry = weaponinfo[weaptotry].ammo;
   }
   while((!player->weaponowned[weaptotry] ||
          (ammototry != am_noammo &&
           player->ammo[ammototry] <= 0)) && 
          newweapon != currentweapon);

   newweapon = ordinalToWp[newweapon];

   if(newweapon == player->readyweapon)
      newweapon = wp_nochange;

   return newweapon;
}

//
// P_PrevWeapon
//
// haleyjd 03/06/09: Like the above.
//
int P_PrevWeapon(player_t *player)
{
   int currentweapon = wpToOrdinal[player->readyweapon];
   int newweapon     = currentweapon;
   int weaptotry;
   int ammototry;

   do
   {
      --newweapon;

      if(newweapon < 0)
         newweapon = NUMWEAPONS - 1;

      weaptotry = ordinalToWp[newweapon];
      ammototry = weaponinfo[weaptotry].ammo;
   }
   while((!player->weaponowned[weaptotry] ||
          (ammototry != am_noammo &&
           player->ammo[ammototry] <= 0)) && 
         newweapon != currentweapon);

   newweapon = ordinalToWp[newweapon];

   if(newweapon == player->readyweapon)
      newweapon = wp_nochange;

   return newweapon;
}

// The first set is where the weapon preferences from             // killough,
// default.cfg are stored. These values represent the keys used   // phares
// in DOOM2 to bring up the weapon, i.e. 6 = plasma gun. These    //    |
// are NOT the wp_* constants.                                    //    V

int weapon_preferences[2][NUMWEAPONS+1] =
{
   { 6, 9, 4, 3, 2, 8, 5, 7, 1, 0 },  // !compatibility preferences
   { 6, 9, 4, 3, 2, 8, 5, 7, 1, 0 },  //  compatibility preferences
};

//
// P_SwitchWeapon
//
// Checks current ammo levels and gives you the most preferred weapon with ammo.
// It will not pick the currently raised weapon. When called from P_CheckAmmo 
// this won't matter, because the raised weapon has no ammo anyway. When called
// from G_BuildTiccmd you want to toggle to a different weapon regardless.
//
weapontype_t P_SwitchWeapon(player_t *player)
{
   int *prefer = weapon_preferences[demo_compatibility != 0]; // killough 3/22/98
   weapontype_t currentweapon = player->readyweapon;
   weapontype_t newweapon = currentweapon;
   int i = NUMWEAPONS + 1;   // killough 5/2/98   

   // killough 2/8/98: follow preferences and fix BFG/SSG bugs

   // haleyjd WEAPON_FIXME: makes assumptions about ammo per shot
   // haleyjd WEAPON_FIXME: makes assumptions about ammotypes used by weapons!
   // haleyjd WEAPON_FIXME: shareware-only must become EDF weapon property
   // haleyjd WEAPON_FIXME: must support arbitrary weapons
   // haleyjd WEAPON_FIXME: chainsaw/fist issues

   do
   {
      switch(*prefer++)
      {
      case 1:
         if(!player->powers[pw_strength])  // allow chainsaw override
            break;
      case 0:
         newweapon = wp_fist;
         break;
      case 2:
         if(player->ammo[am_clip])
            newweapon = wp_pistol;
         break;
      case 3:
         if(player->weaponowned[wp_shotgun] && player->ammo[am_shell])
            newweapon = wp_shotgun;
         break;
      case 4:
         if(player->weaponowned[wp_chaingun] && player->ammo[am_clip])
            newweapon = wp_chaingun;
         break;
      case 5:
         if(player->weaponowned[wp_missile] && player->ammo[am_misl])
            newweapon = wp_missile;
         break;
      case 6:
         if(player->weaponowned[wp_plasma] && player->ammo[am_cell] &&
            GameModeInfo->id != shareware)
            newweapon = wp_plasma;
         break;
      case 7:
         if(player->weaponowned[wp_bfg] && GameModeInfo->id != shareware &&
            player->ammo[am_cell] >= (demo_compatibility ? 41 : 40))
            newweapon = wp_bfg;
         break;
      case 8:
         if(player->weaponowned[wp_chainsaw])
            newweapon = wp_chainsaw;
         break;
      case 9:
         if(player->weaponowned[wp_supershotgun] && 
            enable_ssg &&
            player->ammo[am_shell] >= (demo_compatibility ? 3 : 2))
            newweapon = wp_supershotgun;
         break;
      }
   }
   while(newweapon == currentweapon && --i);        // killough 5/2/98

   return newweapon;
}

//
// P_WeaponPreferred
//
// killough 5/2/98: whether consoleplayer prefers weapon w1 over weapon w2.
//
int P_WeaponPreferred(int w1, int w2)
{
  return
    (weapon_preferences[0][0] != ++w2 && (weapon_preferences[0][0] == ++w1 ||
    (weapon_preferences[0][1] !=   w2 && (weapon_preferences[0][1] ==   w1 ||
    (weapon_preferences[0][2] !=   w2 && (weapon_preferences[0][2] ==   w1 ||
    (weapon_preferences[0][3] !=   w2 && (weapon_preferences[0][3] ==   w1 ||
    (weapon_preferences[0][4] !=   w2 && (weapon_preferences[0][4] ==   w1 ||
    (weapon_preferences[0][5] !=   w2 && (weapon_preferences[0][5] ==   w1 ||
    (weapon_preferences[0][6] !=   w2 && (weapon_preferences[0][6] ==   w1 ||
    (weapon_preferences[0][7] !=   w2 && (weapon_preferences[0][7] ==   w1
   ))))))))))))))));
}

//
// P_CheckAmmo
//
// Returns true if there is enough ammo to shoot.
// If not, selects the next weapon to use.
// (only in demo_compatibility mode -- killough 3/22/98)
//
bool P_CheckAmmo(player_t *player)
{
   ammotype_t ammo = weaponinfo[player->readyweapon].ammo;

   // haleyjd 08/10/02: get count from weaponinfo_t
   // (BFGCELLS value is now written into struct by DeHackEd code)
   int count = weaponinfo[player->readyweapon].ammopershot;
      
   // Some do not need ammunition anyway.
   // Return if current ammunition sufficient.
   
   if(ammo == am_noammo || player->ammo[ammo] >= count)
      return true;
   
   // Out of ammo, pick a weapon to change to.
   //
   // killough 3/22/98: for old demos we do the switch here and now;
   // for Boom games we cannot do this, and have different player
   // preferences across demos or networks, so we have to use the
   // G_BuildTiccmd() interface instead of making the switch here.
   
   if(demo_compatibility)
   {
      player->pendingweapon = P_SwitchWeapon(player);      // phares
      // Now set appropriate weapon overlay.
      P_SetPsprite(player,ps_weapon,weaponinfo[player->readyweapon].downstate);
   }
   
#if 0 /* PROBABLY UNSAFE */
   else
      if (demo_version >= 203)  // killough 9/5/98: force switch if out of ammo
         P_SetPsprite(player,ps_weapon,weaponinfo[player->readyweapon].downstate);
#endif
      
   return false;
}

//
// P_SubtractAmmo
//
// haleyjd 03/08/07:
// Subtracts ammo from weapons in a uniform fashion. Unfortunately, this
// operation is complicated by compatibility issues and extra features.
//
void P_SubtractAmmo(player_t *player, int compat_amt)
{
   weapontype_t weapontype = player->readyweapon;
   weaponinfo_t *weapon    = &weaponinfo[weapontype]; 
   ammotype_t   ammotype   = weapon->ammo;

   // WEAPON_FIXME/TODO: comp flag for corruption of player->maxammo by DeHackEd
   if(player->cheats & CF_INFAMMO || 
      (demo_version >= 329 && ammotype >= NUMAMMO))
      return;
   
   player->ammo[ammotype] -= 
      (weapon->enableaps || compat_amt < 0) ? weapon->ammopershot : compat_amt;
}

int lastshottic; // killough 3/22/98

//
// P_FireWeapon.
//
static void P_FireWeapon(player_t *player)
{
   statenum_t newstate;
   weaponinfo_t *weapon;
   
   if(!P_CheckAmmo(player))
      return;

   weapon = P_GetReadyWeapon(player);

   P_SetMobjState(player->mo, player->mo->info->missilestate);
   newstate = weapon->atkstate;
   P_SetPsprite(player, ps_weapon, newstate);

   // haleyjd 04/06/03: silencer powerup
   // haleyjd 09/14/07: per-weapon silencer, always silent support
   if(!(weapon->flags & WPF_SILENCER && player->powers[pw_silencer]) &&
      !(weapon->flags & WPF_SILENT)) 
      P_NoiseAlert(player->mo, player->mo);

   lastshottic = gametic;                       // killough 3/22/98
}

//
// P_DropWeapon
//
// Player died, so put the weapon away.
//
void P_DropWeapon(player_t *player)
{
   P_SetPsprite(player, ps_weapon, weaponinfo[player->readyweapon].downstate);
}

//=============================================================================
//
// haleyjd 09/13/07
//
// New functions for dynamic weapons system
//

//
// P_GetReadyWeapon
//
// haleyjd 09/13/07: 
// Retrieves a pointer to the proper weaponinfo_t structure for the 
// readyweapon index stored in the player.
//
// WEAPON_TODO: Will need to change as system evolves.
// PCLASS_FIXME: weapons
//
weaponinfo_t *P_GetReadyWeapon(player_t *player)
{
   return &(weaponinfo[player->readyweapon]);
}

//
// P_GetPlayerWeapon
//
// haleyjd 09/16/07:
// Gets weapon at given index for the given player.
// 
// WEAPON_TODO: must redirect through playerclass lookup
// PCLASS_FIXME: weapons
//
weaponinfo_t *P_GetPlayerWeapon(player_t *player, int index)
{
   // currently there is only one linear weaponinfo
   return &weaponinfo[index];
}

//
// P_WeaponSoundInfo
//
// Plays a sound originating from the player's weapon by sfxinfo_t *
//
static void P_WeaponSoundInfo(Mobj *mo, sfxinfo_t *sound)
{
   int volume = 127;

   if(mo->player && mo->player->powers[pw_silencer] &&
      P_GetReadyWeapon(mo->player)->flags & WPF_SILENCER)
      volume = WEAPON_VOLUME_SILENCED;

   S_StartSfxInfo(mo, sound, volume, ATTN_NORMAL, false, CHAN_AUTO);
}

//
// P_WeaponSound
//
// Plays a sound originating from the player's weapon
//
static void P_WeaponSound(Mobj *mo, int sfx_id)
{
   int volume = 127;

   if(mo->player && mo->player->powers[pw_silencer] &&
      P_GetReadyWeapon(mo->player)->flags & WPF_SILENCER)
      volume = WEAPON_VOLUME_SILENCED;

   S_StartSoundAtVolume(mo, sfx_id, volume, ATTN_NORMAL, CHAN_AUTO);
}

//
// End dynamic weapon systems functions
//
//=============================================================================


//
// A_WeaponReady
//
// The player can fire the weapon
// or change to another weapon at this time.
// Follows after getting weapon up,
// or after previous attack/fire sequence.
//
void A_WeaponReady(Mobj *mo)
{
   player_t *player;
   pspdef_t *psp;

   if(!(player = mo->player))
      return;

   psp = &player->psprites[player->curpsprite];

   // WEAPON_FIXME: chainsaw particulars (idle sound)

   // get out of attack state
   if(mo->state == states[mo->info->missilestate] || 
      mo->state == states[player->pclass->altattack])
   {
      P_SetMobjState(mo, mo->info->spawnstate);
   }

   if(player->readyweapon == wp_chainsaw && 
      psp->state == states[E_SafeState(S_SAW)])
      S_StartSound(player->mo, sfx_sawidl);

   // check for change
   //  if player is dead, put the weapon away
   
   if(player->pendingweapon != wp_nochange || !player->health)
   {
      // change weapon (pending weapon should already be validated)
      statenum_t newstate = weaponinfo[player->readyweapon].downstate;
      P_SetPsprite(player, ps_weapon, newstate);
      return;
   }

   // check for fire
   //  the missile launcher and bfg do not auto fire
   
   if(player->cmd.buttons & BT_ATTACK)
   {
      if(!player->attackdown || 
         !(P_GetReadyWeapon(player)->flags & WPF_NOAUTOFIRE))
      {
         player->attackdown = true;
         P_FireWeapon(player);
         return;
      }
   }
   else
      player->attackdown = false;

   // bob the weapon based on movement speed
   {
      int angle = (128*leveltime) & FINEMASK;
      psp->sx = FRACUNIT + FixedMul(player->bob, finecosine[angle]);
      angle &= FINEANGLES/2-1;
      psp->sy = WEAPONTOP + FixedMul(player->bob, finesine[angle]);
   }
}

//
// A_ReFire
//
// The player can re-fire the weapon
// without lowering it entirely.
//
void A_ReFire(Mobj *mo)
{
   player_t *player = mo->player;

   if(!player)
      return;

   // check for fire
   //  (if a weaponchange is pending, let it go through instead)
   
   if((player->cmd.buttons & BT_ATTACK)
      && player->pendingweapon == wp_nochange && player->health)
   {
      player->refire++;
      P_FireWeapon(player);
   }
   else
   {
      player->refire = 0;
      P_CheckAmmo(player);
   }
}

//
// A_CheckReload
//
// cph 2002/08/08 - In old Doom, P_CheckAmmo would start the weapon 
// lowering immediately. This was lost in Boom when the weapon 
// switching logic was rewritten. But we must tell Doom that we 
// don't need to complete the reload frames for the weapon here. 
// G_BuildTiccmd will set ->pendingweapon for us later on.
//
void A_CheckReload(Mobj *mo)
{
   player_t *player = mo->player;

   if(!player)
      return;

   if(!P_CheckAmmo(player) && demo_version >= 331)
   {
      P_SetPsprite(player, ps_weapon,
                   weaponinfo[player->readyweapon].downstate);
   }
}

//
// A_Lower
//
// Lowers current weapon, and changes weapon at bottom.
//
void A_Lower(Mobj *mo)
{
   player_t *player;
   pspdef_t *psp;

   if(!(player = mo->player))
      return;

   psp = &player->psprites[player->curpsprite];

   // WEAPON_FIXME: LOWERSPEED property of EDF weapons?
   psp->sy += LOWERSPEED;
   
   // Is already down.
   if(psp->sy < WEAPONBOTTOM)
      return;

   // Player is dead.
   if(player->playerstate == PST_DEAD)
   {
      psp->sy = WEAPONBOTTOM;
      return;      // don't bring weapon back up
   }

   // The old weapon has been lowered off the screen,
   // so change the weapon and start raising it
   
   if(!player->health)
   {      // Player is dead, so keep the weapon off screen.
      P_SetPsprite(player,  ps_weapon, NullStateNum);
      return;
   }

   // haleyjd 03/28/10: do not assume pendingweapon is valid
   if(player->pendingweapon < NUMWEAPONS)
      player->readyweapon = player->pendingweapon;

   P_BringUpWeapon(player);
}

//
// A_Raise
//
void A_Raise(Mobj *mo)
{
   statenum_t newstate;
   player_t *player;
   pspdef_t *psp;

   if(!(player = mo->player))
      return;

   psp = &player->psprites[player->curpsprite];

   // WEAPON_FIXME: RAISESPEED property of EDF weapons?
   
   psp->sy -= RAISESPEED;
   
   if(psp->sy > WEAPONTOP)
      return;
   
   psp->sy = WEAPONTOP;
   
   // The weapon has been raised all the way,
   //  so change to the ready state.
   
   newstate = weaponinfo[player->readyweapon].readystate;
   
   P_SetPsprite(player, ps_weapon, newstate);
}

// Weapons now recoil, amount depending on the weapon.              // phares
//                                                                  //   |
// The P_SetPsprite call in each of the weapon firing routines      //   V
// was moved here so the recoil could be synched with the
// muzzle flash, rather than the pressing of the trigger.
// The BFG delay caused this to be necessary.
//
// WEAPON_FIXME: "adder" parameter is not reliable for EDF editing
//
static void A_FireSomething(player_t* player, int adder)
{
   P_SetPsprite(player, ps_flash,
      weaponinfo[player->readyweapon].flashstate+adder);
   
   // killough 3/27/98: prevent recoil in no-clipping mode
   if(!(player->mo->flags & MF_NOCLIP))
   {
      // haleyjd 08/18/08: added ALWAYSRECOIL weapon flag; recoil in weaponinfo
      if(weaponinfo[player->readyweapon].flags & WPF_ALWAYSRECOIL ||
         (weapon_recoil && (demo_version >= 203 || !compatibility)))
      {
         P_Thrust(player, ANG180 + player->mo->angle,
                  2048*weaponinfo[player->readyweapon].recoil); // phares
      }
   }
}

//
// A_GunFlash
//

void A_GunFlash(Mobj *mo)
{
   player_t *player = mo->player;

   if(!player)
      return;

   P_SetMobjState(mo, player->pclass->altattack);
   
   A_FireSomething(player, 0);                               // phares
}

//=============================================================================
//
// Autoaiming
//

//
// P_doAutoAim
//
// Does basic autoaiming logic and returns the slope value calculated by
// P_AimLineAttack.
//
static fixed_t P_doAutoAim(TracerContext *tc, Mobj *mo, angle_t angle, fixed_t distance)
{
   if(demo_version >= 203)
   {
      // autoaiming is disabled?
      if(full_demo_version > make_full_version(340, 15) && !autoaim && mo->player)
         return P_PlayerPitchSlope(mo->player);

      // killough 8/2/98: make autoaiming prefer enemies
      fixed_t slope = trace->aimLineAttack(mo, angle, distance, MF_FRIEND, tc);
      if(tc->linetarget)
         return slope;
   }
   
   return trace->aimLineAttack(mo, angle, distance, 0, tc);
}

/*
   // killough 7/19/98: autoaiming was not in original beta
   // sf: made a multiplayer option
   if(autoaim)
   {
      // killough 8/2/98: prefer autoaiming at enemies
      int mask = demo_version < 203 ? 0 : MF_FRIEND;
      do
      {
         slope = P_AimLineAttack(source, an, 16*64*FRACUNIT, mask);
         if(!clip.linetarget)
            slope = P_AimLineAttack(source, an += 1<<26, 16*64*FRACUNIT, mask);
         if(!clip.linetarget)
            slope = P_AimLineAttack(source, an -= 2<<26, 16*64*FRACUNIT, mask);
         if(!clip.linetarget)
         {
            an = source->angle;
            // haleyjd: use true slope angle
            slope = P_PlayerPitchSlope(source->player);
         }
      }
      while(mask && (mask=0, !clip.linetarget));  // killough 8/2/98
   }
   else
   {
      // haleyjd: use true slope angle
      slope = P_PlayerPitchSlope(source->player);
   }
*/

//=============================================================================
//
// WEAPON ATTACKS
//

// haleyjd 09/24/00: Infamous DeHackEd Problem Fix
//   Its been known for years that setting weapons to use am_noammo
//   that normally use non-infinite ammo types causes your max shells
//   to reduce, now I know why:
//
//   player->ammo[weaponinfo[player->readyweapon].ammo]--;
//
//   If weaponinfo[].ammo evaluates to am_noammo, then it is equal
//   to NUMAMMO+1. In the player struct we find:
//
//   int ammo[NUMAMMO];
//   int maxammo[NUMAMMO];
//
//   It is indexing 2 past the end of ammo into maxammo, and the
//   second ammo type just happens to be shells. How funny, eh?
//   All the functions below are fixed to check for this, optioned 
//   with demo compatibility.

//
// A_Punch
//
void A_Punch(Mobj *mo)
{
   angle_t angle;
   fixed_t slope;
   int damage = (P_Random(pr_punch) % 10 + 1) << 1;
   player_t *player  = mo->player;

   if(!player)
      return;
   
   // WEAPON_FIXME: berserk and/or other damage multipliers -> EDF weapon property?
   // WEAPON_FIXME: tracer damage, range, etc possible weapon properties?

   if(player->powers[pw_strength])
      damage *= 10;
   
   angle = mo->angle;

   // haleyjd 08/05/04: use new function
   angle += P_SubRandom(pr_punchangle) << 18;

   TracerContext *tc = trace->getContext();
   slope = P_doAutoAim(tc, mo, angle, MELEERANGE);

   trace->lineAttack(mo, angle, MELEERANGE, slope, damage);

   if(!tc->linetarget)
   {
      tc->done();
      return;
   }

   P_WeaponSound(mo, GameModeInfo->playerSounds[sk_punch]);

   // turn to face target
   mo->angle = P_PointToAngle(mo->x, mo->y, tc->linetarget->x, tc->linetarget->y);
   tc->done();
}

//
// A_Saw
//
void A_Saw(Mobj *mo)
{
   fixed_t slope;
   int damage = 2 * (P_Random(pr_saw) % 10 + 1);
   angle_t angle     = mo->angle;
   
   // haleyjd 08/05/04: use new function
   angle += P_SubRandom(pr_saw) << 18;

   // Use meleerange + 1 so that the puff doesn't skip the flash
   TracerContext *tc = trace->getContext();
   slope = P_doAutoAim(tc, mo, angle, MELEERANGE + 1);

   trace->lineAttack(mo, angle, MELEERANGE+1, slope, damage);
   
   if(!tc->linetarget)
   {
      P_WeaponSound(mo, sfx_sawful);
      tc->done();
      return;
   }

   P_WeaponSound(mo, sfx_sawhit);
   
   // turn to face target
   angle = P_PointToAngle(mo->x, mo->y, tc->linetarget->x, tc->linetarget->y);

   if(angle - mo->angle > ANG180)
   {
      if(angle - mo->angle < -ANG90/20)
         mo->angle = angle + ANG90/21;
      else
         mo->angle -= ANG90/20;
   }
   else
   {
      if(angle - mo->angle > ANG90/20)
         mo->angle = angle - ANG90/21;
      else
         mo->angle += ANG90/20;
   }

   mo->flags |= MF_JUSTATTACKED;
   
   tc->done();
}

//
// A_FireMissile
//
void A_FireMissile(Mobj *mo)
{
   player_t *player = mo->player;

   if(!player)
      return;

   P_SubtractAmmo(player, 1);

   P_SpawnPlayerMissile(player->mo, E_SafeThingType(MT_ROCKET));
}

//
// A_FireBFG
//

#define BFGBOUNCE 16

void A_FireBFG(Mobj *actor)
{
   Mobj *mo;
   player_t *player = actor->player;

   if(!player)
      return;

   P_SubtractAmmo(player, BFGCELLS);
   
   mo = P_SpawnPlayerMissile(actor, E_SafeThingType(MT_BFG));
   mo->extradata.bfgcount = BFGBOUNCE;   // for bouncing bfg - redundant
}

//
// A_FireOldBFG
//
// This function emulates Doom's Pre-Beta BFG
// By Lee Killough 6/6/98, 7/11/98, 7/19/98, 8/20/98
//
// This code may not be used in other mods without appropriate credit given.
// Code leeches will be telefragged.
//
void A_FireOldBFG(Mobj *mo)
{
   static int type1 = -1;
   static int type2 = -1;
   int type;
   player_t *player = mo->player;

   if(!player)
      return;

   if(type1 == -1)
   {
      type1 = E_SafeThingType(MT_PLASMA1);
      type2 = E_SafeThingType(MT_PLASMA2);
   }

   type = type1;

   // PCLASS_FIXME: second attack state
   
   // sf: make sure the player is in firing frame, or it looks silly
   if(demo_version > 300)
      P_SetMobjState(mo, player->pclass->altattack);
   
   // WEAPON_FIXME: recoil for classic BFG

   if(weapon_recoil && !(mo->flags & MF_NOCLIP))
      P_Thrust(player, ANG180 + mo->angle,
               512*weaponinfo[wp_plasma].recoil);

   // WEAPON_FIXME: ammopershot for classic BFG

   if((demo_version < 329 ||
       weaponinfo[player->readyweapon].ammo < NUMAMMO) &&
      !(player->cheats & CF_INFAMMO)) // haleyjd
   {
      player->ammo[weaponinfo[player->readyweapon].ammo]--;
   }

   if(LevelInfo.useFullBright) // haleyjd
      player->extralight = 2;

   do
   {
      Mobj *th;
      angle_t an = mo->angle;
      angle_t an1 = ((P_Random(pr_bfg) & 127) - 64) * (ANG90 / 768) + an;
      angle_t an2 = ((P_Random(pr_bfg) & 127) - 64) * (ANG90 / 640) + ANG90;
      extern int autoaim;
      fixed_t slope;

      if(autoaim)
      {
         TracerContext *tc = trace->getContext();
         
         // killough 8/2/98: make autoaiming prefer enemies
         int mask = MF_FRIEND;
         do
         {
            slope = trace->aimLineAttack(mo, an, 16*64*FRACUNIT, mask, tc);
            if(!tc->linetarget)
               slope = trace->aimLineAttack(mo, an += 1<<26, 16*64*FRACUNIT, mask, tc);
            if(!tc->linetarget)
               slope = trace->aimLineAttack(mo, an -= 2<<26, 16*64*FRACUNIT, mask, tc);
            if(!tc->linetarget) // sf: looking up/down
            {
               slope = finetangent[(ANG90-player->pitch)>>ANGLETOFINESHIFT];
               an = mo->angle;
            }
         }
         while(mask && (mask=0, !tc->linetarget));     // killough 8/2/98
         tc->done();
         
         an1 += an - mo->angle;
         // sf: despite killough's infinite wisdom.. even
         // he is prone to mistakes. seems negative numbers
         // won't survive a bitshift!
         if(slope < 0 && demo_version >= 303)
            an2 -= tantoangle[-slope >> DBITS];
         else
            an2 += tantoangle[slope >> DBITS];
      }
      else
      {
         slope = finetangent[(ANG90-player->pitch)>>ANGLETOFINESHIFT];
         if(slope < 0 && demo_version >= 303)
            an2 -= tantoangle[-slope >> DBITS];
         else
            an2 += tantoangle[slope >> DBITS];
      }

      th = P_SpawnMobj(mo->x, mo->y,
                       mo->z + 62*FRACUNIT - player->psprites[ps_weapon].sy,
                       type);

      P_SetTarget<Mobj>(&th->target, mo);
      th->angle = an1;
      th->momx = finecosine[an1>>ANGLETOFINESHIFT] * 25;
      th->momy = finesine[an1>>ANGLETOFINESHIFT] * 25;
      th->momz = finetangent[an2>>ANGLETOFINESHIFT] * 25;
      P_CheckMissileSpawn(th);
   }
   while((type != type2) && (type = type2)); //killough: obfuscated!
}

//
// A_FirePlasma
//
void A_FirePlasma(Mobj *mo)
{
   player_t *player = mo->player;

   if(!player)
      return;

   P_SubtractAmmo(player, 1);

   A_FireSomething(player, P_Random(pr_plasma) & 1);
   
   // sf: removed beta
   P_SpawnPlayerMissile(mo, E_SafeThingType(MT_PLASMA));
}


//
// P_BulletSlope
//
// Sets a slope so a near miss is at approximately
// the height of the intended target
//
void P_BulletSlope(Mobj *mo, TracerContext *tc)
{
   angle_t an = mo->angle;    // see which target is to be aimed at
   
   // killough 8/2/98: make autoaiming prefer enemies
   int mask = demo_version < 203 ? 0 : MF_FRIEND;

   // haleyjd 08/09/11: allow autoaim disable
   if(full_demo_version > make_full_version(340, 15) && !autoaim && mo->player)
   {
      tc->bulletslope = P_PlayerPitchSlope(mo->player);
      return;
   }
   
   do
   {
      tc->bulletslope = trace->aimLineAttack(mo, an, 16*64*FRACUNIT, mask, tc);
      if(!tc->linetarget)
         tc->bulletslope = trace->aimLineAttack(mo, an += 1<<26, 16*64*FRACUNIT, mask, tc);
      if(!tc->linetarget)
         tc->bulletslope = trace->aimLineAttack(mo, an -= 2<<26, 16*64*FRACUNIT, mask, tc);
   }
   while (mask && (mask=0, !tc->linetarget));  // killough 8/2/98
}

//
// P_GunShot
//
void P_GunShot(Mobj *mo, fixed_t slope, bool accurate)
{
   int damage = 5 * (P_Random(pr_gunshot) % 3 + 1);
   angle_t angle = mo->angle;
   
   if(!accurate)
      angle += P_SubRandom(pr_misfire) << 18;
   
   trace->lineAttack(mo, angle, MISSILERANGE, slope, damage);
}

//
// A_FirePistol
//
void A_FirePistol(Mobj *mo)
{
   player_t *player = mo->player;

   if(!player)
      return;

   P_WeaponSound(mo, sfx_pistol);
   
   // PCLASS_FIXME: attack state two

   P_SetMobjState(mo, player->pclass->altattack);

   P_SubtractAmmo(player, 1);
   
   A_FireSomething(player, 0); // phares
   
   TracerContext *tc = trace->getContext();
   P_BulletSlope(mo, tc);
   
   P_GunShot(mo, tc->bulletslope, !player->refire);
   tc->done();
}

//
// A_FireShotgun
//
void A_FireShotgun(Mobj *mo)
{
   int i;
   player_t *player = mo->player;

   if(!player)
      return;

   // PCLASS_FIXME: second attack state

   P_WeaponSound(mo, sfx_shotgn);
   P_SetMobjState(mo, player->pclass->altattack);
   
   P_SubtractAmmo(player, 1);
   
   A_FireSomething(player, 0); // phares
   
   TracerContext *tc = trace->getContext();
   P_BulletSlope(mo, tc);
   
   for(i = 0; i < 7; ++i)
      P_GunShot(mo, tc->bulletslope, false);

   tc->done();
}

//
// A_FireShotgun2
//
void A_FireShotgun2(Mobj *mo)
{
   int i;
   player_t *player = mo->player;

   if(!player)
      return;

   // PCLASS_FIXME: secondary attack state
   
   P_WeaponSound(mo, sfx_dshtgn);
   P_SetMobjState(mo, player->pclass->altattack);

   P_SubtractAmmo(player, 2);
   
   A_FireSomething(player, 0); // phares
   
   TracerContext *tc = trace->getContext();
   P_BulletSlope(mo, tc);
   
   for(i = 0; i < 20; ++i)
   {
      int damage = 5 * (P_Random(pr_shotgun) % 3 + 1);
      angle_t angle = mo->angle;

      angle += P_SubRandom(pr_shotgun) << 19;
      
      trace->lineAttack(mo, angle, MISSILERANGE, tc->bulletslope +
                        (P_SubRandom(pr_shotgun) << 5), damage);
   }
   tc->done();
}

// haleyjd 04/05/07: moved all SSG codepointers here

void A_OpenShotgun2(Mobj *mo)
{
   P_WeaponSound(mo, sfx_dbopn);
}

void A_LoadShotgun2(Mobj *mo)
{
   P_WeaponSound(mo, sfx_dbload);
}

void A_CloseShotgun2(Mobj *mo)
{
   P_WeaponSound(mo, sfx_dbcls);
   A_ReFire(mo);
}

//
// A_FireCGun
//
void A_FireCGun(Mobj *mo)
{
   player_t *player;
   pspdef_t *psp;

   if(!(player = mo->player))
      return;

   psp = &player->psprites[player->curpsprite];

   P_WeaponSound(mo, sfx_chgun);

   if(!player->ammo[weaponinfo[player->readyweapon].ammo])
      return;
   
   // sf: removed beta

   P_SetMobjState(mo, player->pclass->altattack);
   
   P_SubtractAmmo(player, 1);

   // haleyjd 08/28/03: this is not safe for DeHackEd/EDF, so it
   // needs some modification to be safer
   // haleyjd WEAPON_FIXME: hackish and dangerous for EDF, needs fix.
   if(demo_version < 331 || 
      (psp->state->index >= E_StateNumForDEHNum(S_CHAIN1) &&
       psp->state->index < E_StateNumForDEHNum(S_CHAIN3)))
   {      
      // phares
      A_FireSomething(player, psp->state->index - states[E_SafeState(S_CHAIN1)]->index);
   }
   else
      A_FireSomething(player, 0); // new default behavior
   
   TracerContext *tc = trace->getContext();
   P_BulletSlope(mo, tc);
  
   P_GunShot(mo, tc->bulletslope, !player->refire);
   tc->done();
}

void A_Light0(Mobj *mo)
{
   if(!mo->player)
      return;

   mo->player->extralight = 0;
}

void A_Light1(Mobj *mo)
{
   if(!mo->player)
      return;

   if(LevelInfo.useFullBright) // haleyjd
      mo->player->extralight = 1;
}

void A_Light2(Mobj *mo)
{
   if(!mo->player)
      return;

   if(LevelInfo.useFullBright) // haleyjd
      mo->player->extralight = 2;
}

void A_BouncingBFG(Mobj *mo);
void A_BFG11KHit(Mobj *mo);
void A_BFGBurst(Mobj *mo); // haleyjd

//
// A_BFGSpray
//
// Spawn a BFG explosion on every monster in view
//
void A_BFGSpray(Mobj *mo)
{
   int i;

   // WEAPON_FIXME: BFG type stuff
   switch(bfgtype)
   {
   case bfg_11k:
      A_BFG11KHit(mo);
      return;
   case bfg_bouncing:
      A_BouncingBFG(mo);
      return;
   case bfg_burst:
      A_BFGBurst(mo);
      return;
   default:
      break;
   }
   
   TracerContext *tc = trace->getContext();
   for(i = 0; i < 40; i++)  // offset angles from its attack angle
   {
      int j, damage;
      angle_t an = mo->angle - ANG90/2 + ANG90/40*i;
      
      // mo->target is the originator (player) of the missile
      
      // killough 8/2/98: make autoaiming prefer enemies
      if(demo_version < 203 || 
         (trace->aimLineAttack(mo->target, an, 16*64*FRACUNIT, MF_FRIEND, tc), 
         !tc->linetarget))
         trace->aimLineAttack(mo->target, an, 16*64*FRACUNIT, 0, tc);
      
      if(!tc->linetarget)
         continue;
      
      P_SpawnMobj(tc->linetarget->x, tc->linetarget->y,
                  tc->linetarget->z + (tc->linetarget->height>>2), 
                  E_SafeThingType(MT_EXTRABFG));
      
      for(damage = j = 0; j < 15; j++)
         damage += (P_Random(pr_bfg)&7) + 1;
      
      P_DamageMobj(tc->linetarget, mo->target, mo->target, damage,
                   MOD_BFG_SPLASH);
   }
   tc->done();
}

//
// A_BouncingBFG
//
// haleyjd: The bouncing BFG from SMMU, but fixed to work better.
//
void A_BouncingBFG(Mobj *mo)
{
   int i;
   Mobj *newmo;
   
   if(!mo->extradata.bfgcount)
      return;
   
   TracerContext *tc = trace->getContext();
   
   for(i = 0 ; i < 40 ; i++)  // offset angles from its attack angle
   {
      angle_t an2, an = (ANG360/40)*i;
      int dist;
      
      trace->aimLineAttack(mo, an, 16*64*FRACUNIT,0, tc);
      
      // haleyjd: track last target with mo->tracer, don't fire
      // at same target more than one time in a row
      if(!tc->linetarget || (mo->tracer && mo->tracer == tc->linetarget))
         continue;
      if(an/6 == mo->angle/6) continue;
      
      // don't aim for shooter, or for friends of shooter
      if(tc->linetarget == mo->target ||
         (tc->linetarget->flags & mo->target->flags & MF_FRIEND))
         continue; 
      
      P_SpawnMobj(tc->linetarget->x, tc->linetarget->y,
                  tc->linetarget->z + (tc->linetarget->height>>2),
                  E_SafeThingType(MT_EXTRABFG));

      // spawn new bfg      
      // haleyjd: can't use P_SpawnMissile here
      newmo = P_SpawnMobj(mo->x, mo->y, mo->z, E_SafeThingType(MT_BFG));
      S_StartSound(newmo, newmo->info->seesound);
      P_SetTarget<Mobj>(&newmo->target, mo->target); // pass on the player
      an2 = P_PointToAngle(newmo->x, newmo->y, tc->linetarget->x, tc->linetarget->y);
      newmo->angle = an2;
      
      an2 >>= ANGLETOFINESHIFT;
      newmo->momx = FixedMul(newmo->info->speed, finecosine[an2]);
      newmo->momy = FixedMul(newmo->info->speed, finesine[an2]);

      dist = P_AproxDistance(tc->linetarget->x - newmo->x, 
                             tc->linetarget->y - newmo->y);
      dist = dist / newmo->info->speed;
      
      if(dist < 1)
         dist = 1;
      
      newmo->momz = 
         (tc->linetarget->z + (tc->linetarget->height>>1) - newmo->z) / dist;

      newmo->extradata.bfgcount = mo->extradata.bfgcount - 1; // count down
      P_SetTarget<Mobj>(&newmo->tracer, tc->linetarget); // haleyjd: track target

      P_CheckMissileSpawn(newmo);

      mo->removeThinker(); // remove the old one

      break; //only spawn 1
   }
   
   tc->done();
}

//
// A_BFG11KHit
//
// Explosion pointer for SMMU BFG11k.
//
void A_BFG11KHit(Mobj *mo)
{
   int i = 0;
   int j, damage;
   int origdist;

   if(!mo->target)
      return;
   
   // check the originator and hurt them if too close
   origdist = P_AproxDistance(mo->x - getTargetX(mo), mo->y - getTargetY(mo));
   
   if(origdist < 96*FRACUNIT)
   {
      // decide on damage
      // damage decreases with distance
      for(damage = j = 0; j < 48 - (origdist/(FRACUNIT*2)); j++)
         damage += (P_Random(pr_bfg)&7) + 1;
      
      // flash
      P_SpawnMobj(mo->target->x, mo->target->y,
                  mo->target->z + (mo->target->height>>2), 
                  E_SafeThingType(MT_EXTRABFG));
      
      P_DamageMobj(mo->target, mo, mo->target, damage, 
                   MOD_BFG11K_SPLASH);
   }
   
   // now check everyone else
   TracerContext *tc = trace->getContext();
   
   for(i = 0 ; i < 40 ; i++)  // offset angles from its attack angle
   {
      angle_t an = (ANG360/40)*i;
      
      // mo->target is the originator (player) of the missile
      
      trace->aimLineAttack(mo, an, 16*64*FRACUNIT, 0, tc);
      
      if(!tc->linetarget) continue;
      if(tc->linetarget == mo->target)
         continue;
      
      // decide on damage
      for(damage = j = 0; j < 20; j++)
         damage += (P_Random(pr_bfg)&7) + 1;
      
      // dumbass flash
      P_SpawnMobj(tc->linetarget->x, tc->linetarget->y,
                  tc->linetarget->z + (tc->linetarget->height>>2), 
                  E_SafeThingType(MT_EXTRABFG));
      
      P_DamageMobj(tc->linetarget, mo->target, mo->target, damage,
                   MOD_BFG_SPLASH);
   }
   
   tc->done();
}

//
// A_BFGBurst
//
// This implements the projectile burst that was featured
// in Cephaler's unreleased MagDoom port. Much thanks to my
// long lost friend for giving me the source to his port back
// when he stopped working on it. This is a tribute to his bold
// spirit ^_^
//
void A_BFGBurst(Mobj *mo)
{
   int a;
   angle_t an = 0;
   Mobj *th;
   static int plasmaType = -1;
   
   if(plasmaType == -1)
      plasmaType = E_SafeThingType(MT_PLASMA3);

   for(a = 0; a < 40; a++)
   {
      an += ANG90 / 10;

      th = P_SpawnMobj(mo->x, mo->y, mo->z, plasmaType);
      P_SetTarget<Mobj>(&th->target, mo->target);

      th->angle = an;
      th->momx = finecosine[an >> ANGLETOFINESHIFT] << 4;
      th->momy = finesine[an >> ANGLETOFINESHIFT] << 4;
      th->momz = FRACUNIT * ((16 - P_Random(pr_bfg)) >> 5);

      P_CheckMissileSpawn(th);      
   }
}

//
// A_BFGsound
//
void A_BFGsound(Mobj *mo)
{
   P_WeaponSound(mo, sfx_bfg);
}

//
// P_SetupPsprites
//
// Called at start of level for each player.
//
void P_SetupPsprites(player_t *player)
{
   int i;
   
   // remove all psprites
   for(i = 0; i < NUMPSPRITES; ++i)
      player->psprites[i].state = NULL;
   
   // spawn the gun
   player->pendingweapon = player->readyweapon;
   P_BringUpWeapon(player);
}

//
// P_MovePsprites
//
// Called every tic by player thinking routine.
//
void P_MovePsprites(player_t *player)
{
   pspdef_t *psp = player->psprites;
   int i;

   // a null state means not active
   // drop tic count and possibly change state
   // a -1 tic count never changes
   
   for(i = 0; i < NUMPSPRITES; ++i, ++psp)
   {
      if(psp->state && psp->tics != -1 && !--psp->tics)
         P_SetPsprite(player, i, psp->state->nextstate);
   }
   
   player->psprites[ps_flash].sx = player->psprites[ps_weapon].sx;
   player->psprites[ps_flash].sy = player->psprites[ps_weapon].sy;
}

//===============================
//
// New Eternity Weapon Functions
//
//===============================

// FIXME/TODO: get rid of this?
void A_FireGrenade(Mobj *mo)
{
}

static const char *kwds_A_FireCustomBullets[] =
{
   "{DUMMY}",           // 0
   "always",            // 1
   "first",             // 2
   "never",             // 3
   "ssg",               // 4
   "monster",           // 5
};

static argkeywd_t fcbkwds =
{
   kwds_A_FireCustomBullets, 
   sizeof(kwds_A_FireCustomBullets) / sizeof(const char *)
};

//
// A_FireCustomBullets
//
// A parameterized player bullet weapon code pointer
// Parameters:
// args[0] : sound (dehacked num)
// args[1] : accuracy (always, first fire only, never, ssg, monster)
// args[2] : number of bullets to fire
// args[3] : damage factor of bullets
// args[4] : damage modulus of bullets
//
void A_FireCustomBullets(Mobj *mo)
{
   int i, accurate, numbullets, damage, dmgmod;
   sfxinfo_t *sfx;
   player_t *player;
   pspdef_t *psp;

   if(!(player = mo->player))
      return;

   psp = &player->psprites[player->curpsprite];

   sfx        = E_ArgAsSound(psp->state->args, 0);
   accurate   = E_ArgAsKwd(psp->state->args, 1, &fcbkwds, 0);
   numbullets = E_ArgAsInt(psp->state->args, 2, 0);
   damage     = E_ArgAsInt(psp->state->args, 3, 0);
   dmgmod     = E_ArgAsInt(psp->state->args, 4, 0);

   if(!accurate)
      accurate = 1;

   if(dmgmod < 1)
      dmgmod = 1;
   else if(dmgmod > 256)
      dmgmod = 256;

   P_WeaponSoundInfo(mo, sfx);

   P_SetMobjState(mo, player->pclass->altattack);

   // subtract ammo amount
   if(weaponinfo[player->readyweapon].ammo < NUMAMMO &&
      !(player->cheats & CF_INFAMMO))
   {
      // now settable in weapon, not needed as a parameter here
      P_SubtractAmmo(player, -1);
   }

   A_FireSomething(player, 0);
   
   TracerContext *tc = trace->getContext();
   P_BulletSlope(mo, tc);

   // loop on numbullets
   for(i = 0; i < numbullets; ++i)
   {
      int dmg = damage * (P_Random(pr_custombullets)%dmgmod + 1);
      angle_t angle = mo->angle;
      fixed_t slope = tc->bulletslope; // haleyjd 01/03/07: bug fix
      
      if(accurate <= 3 || accurate == 5)
      {
         // if never accurate, monster accurate, or accurate only on 
         // refire and player is refiring, add some to the angle
         if(accurate == 3 || accurate == 5 || 
            (accurate == 2 && player->refire))
         {
            int aimshift = ((accurate == 5) ? 20 : 18);
            angle += P_SubRandom(pr_custommisfire) << aimshift;
         }

         trace->lineAttack(mo, angle, MISSILERANGE, slope, dmg);
      }
      else if(accurate == 4) // ssg spread
      {
         angle += P_SubRandom(pr_custommisfire) << 19;
         slope += P_SubRandom(pr_custommisfire) << 5;

         trace->lineAttack(mo, angle, MISSILERANGE, slope, dmg);
      }
   }
   tc->done();
}

static const char *kwds_A_FirePlayerMissile[] =
{
   "normal",           //  0
   "homing",           //  1
};

static argkeywd_t seekkwds =
{
   kwds_A_FirePlayerMissile,
   sizeof(kwds_A_FirePlayerMissile) / sizeof(const char *)
};

//
// A_FirePlayerMissile
//
// A parameterized code pointer function for custom missiles
// Parameters:
// args[0] : thing type to shoot
// args[1] : whether or not to home at current autoaim target
//           (missile requires homing maintenance pointers, however)
//
void A_FirePlayerMissile(Mobj *actor)
{
   int thingnum;
   Mobj *mo;
   bool seek;
   player_t *player;
   pspdef_t *psp;

   if(!actor->player)
      return;

   player = actor->player;
   psp    = &player->psprites[player->curpsprite];

   thingnum = E_ArgAsThingNumG0(psp->state->args, 0);
   seek     = !!E_ArgAsKwd(psp->state->args, 1, &seekkwds, 0);

   // validate thingtype
   if(thingnum < 0 || thingnum == NUMMOBJTYPES)
      return;

   // decrement ammo if appropriate
   if(weaponinfo[player->readyweapon].ammo < NUMAMMO &&
     !(player->cheats & CF_INFAMMO))
   {
      P_SubtractAmmo(player, -1);
   }

   mo = P_SpawnPlayerMissile(actor, thingnum);

   if(mo && seek)
   {
      TracerContext *tc = trace->getContext();
      P_BulletSlope(actor, tc);

      if(tc->linetarget)
         P_SetTarget<Mobj>(&mo->tracer, tc->linetarget);
         
      tc->done();
   }
}

const char *kwds_A_CustomPlayerMelee[] =
{
   "{DUMMY}",          //  0
   "none",             //  1
   "punch",            //  2
   "chainsaw",         //  3
};

static argkeywd_t cpmkwds =
{
   kwds_A_CustomPlayerMelee,
   sizeof(kwds_A_CustomPlayerMelee) / sizeof(const char *)
};

//
// A_CustomPlayerMelee
//
// A parameterized melee attack codepointer function
// Parameters:
// args[0] : damage factor
// args[1] : damage modulus
// args[2] : berzerk multiplier
// args[3] : angle deflection type (none, punch, chainsaw)
// args[4] : sound to make (dehacked number)
//
void A_CustomPlayerMelee(Mobj *mo)
{
   angle_t angle;
   fixed_t slope;
   int damage, dmgfactor, dmgmod, berzerkmul, deftype;
   sfxinfo_t *sfx;
   player_t *player;
   pspdef_t *psp;

   if(!(player = mo->player))
      return;

   psp = &player->psprites[player->curpsprite];

   dmgfactor  = E_ArgAsInt(psp->state->args, 0, 0);
   dmgmod     = E_ArgAsInt(psp->state->args, 1, 0);
   berzerkmul = E_ArgAsInt(psp->state->args, 2, 0);
   deftype    = E_ArgAsKwd(psp->state->args, 3, &cpmkwds, 0);
   sfx        = E_ArgAsSound(psp->state->args, 4);

   // adjust parameters

   if(dmgmod < 1)
      dmgmod = 1;
   else if(dmgmod > 256)
      dmgmod = 256;

   // calculate damage
   damage = dmgfactor * ((P_Random(pr_custompunch)%dmgmod) + 1);

   // apply berzerk multiplier
   if(player->powers[pw_strength])
      damage *= berzerkmul;

   // decrement ammo if appropriate
   if(weaponinfo[player->readyweapon].ammo < NUMAMMO &&
      !(player->cheats & CF_INFAMMO))
   {
      P_SubtractAmmo(player, -1);
   }
   
   angle = player->mo->angle;
   
   if(deftype == 2 || deftype == 3)
      angle += P_SubRandom(pr_custompunch) << 18;
   
   TracerContext *tc = trace->getContext();
   slope = P_doAutoAim(tc, mo, angle, MELEERANGE);

   // WEAPON_FIXME: does this pointer fail to set the player into an attack state?
   // WEAPON_FIXME: check ALL new weapon pointers for this problem.
   
   trace->lineAttack(mo, angle, MELEERANGE, slope, damage);
   
   if(!tc->linetarget)
   {
      // assume they want sawful on miss if sawhit specified
      if(sfx && sfx->dehackednum == sfx_sawhit)
         P_WeaponSound(mo, sfx_sawful);
      tc->done();
      return;
   }

   // start sound
   P_WeaponSoundInfo(mo, sfx);
   
   // turn to face target   
   player->mo->angle = P_PointToAngle(mo->x, mo->y,
                                       tc->linetarget->x, tc->linetarget->y);

   // apply chainsaw deflection if selected
   if(deftype == 3)
   {
      if(angle - mo->angle > ANG180)
      {
         if(angle - mo->angle < -ANG90/20)
            mo->angle = angle + ANG90/21;
         else
            mo->angle -= ANG90/20;
      }
      else
      {
         if(angle - mo->angle > ANG90/20)
            mo->angle = angle - ANG90/21;
         else
            mo->angle += ANG90/20;
      }
      
      mo->flags |= MF_JUSTATTACKED;
   }
   
   tc->done();
}

static const char *kwds_A_PlayerThunk1[] =
{
   "noface", // 0
   "face",   // 1
};

static argkeywd_t facekwds = { kwds_A_PlayerThunk1, 2 };

static const char *kwds_A_PlayerThunk2[] =
{
   "attacker",  // 0
   "aimtarget", // 1
};

static argkeywd_t targetkwds = { kwds_A_PlayerThunk2, 2};

static const char *kwds_A_PlayerThunk3[] =
{
   "nouseammo", // 0
   "useammo",   // 1
};

static argkeywd_t ammokwds = { kwds_A_PlayerThunk3, 2 };

//
// A_PlayerThunk
//
// Allows the player's weapons to use a certain class of monster
// codepointers that are deemed safe (these have the BPF_PTHUNK
// flag in the deh_bexptrs array).
// Parameters:
// args[0] : index of codepointer to call (is validated)
// args[1] : bool, 0 disables the FaceTarget cp for the player
// args[2] : dehacked num of a state to put the player in temporarily
// args[3] : bool, 1 == set player's target to autoaim target
// args[4] : bool, 1 == use ammo on current weapon if attack succeeds
//
void A_PlayerThunk(Mobj *mo)
{
   bool face;
   bool settarget;
   bool useammo;
   int cptrnum, statenum;
   state_t *oldstate = 0;
   Mobj *oldtarget = NULL, *localtarget = NULL;
   player_t *player;
   pspdef_t *psp;

   if(!(player = mo->player))
      return;

   psp    = &player->psprites[player->curpsprite];

   cptrnum   =   E_ArgAsBexptr(psp->state->args, 0);
   face      = !!E_ArgAsKwd(psp->state->args, 1, &facekwds, 0);
   statenum  =   E_ArgAsStateNumG0(psp->state->args, 2, NULL);
   settarget = !!E_ArgAsKwd(psp->state->args, 3, &targetkwds, 0);
   useammo   = !!E_ArgAsKwd(psp->state->args, 4, &ammokwds, 0);

   // validate codepointer index
   if(cptrnum < 0)
      return;

   // set player's target to thing being autoaimed at if this
   // behavior is requested.
   if(settarget)
   {
      // record old target
      oldtarget = mo->target;
      
      TracerContext *tc = trace->getContext();
      P_BulletSlope(mo, tc);
      
      if(tc->linetarget)
      {
         P_SetTarget<Mobj>(&(mo->target), tc->linetarget);
         localtarget = tc->linetarget;
         tc->done();
      }
      else
      {
         tc->done();
         return;
      }
   }

   // possibly disable the FaceTarget pointer using MIF_NOFACE
   if(!face)
      mo->intflags |= MIF_NOFACE;

   // If a state has been provided, place the player into it. This
   // allows use of parameterized codepointers.
   if(statenum >= 0 && statenum < NUMSTATES)
   {
      oldstate = mo->state;
      mo->state = states[statenum];
   }

   // if ammo should be used, subtract it now
   if(useammo)
   {
      if(weaponinfo[player->readyweapon].ammo < NUMAMMO &&
         !(player->cheats & CF_INFAMMO))
      {
         P_SubtractAmmo(player, -1);
      }
   }

   // execute the codepointer
   deh_bexptrs[cptrnum].cptr(mo);

   // remove MIF_NOFACE
   mo->intflags &= ~MIF_NOFACE;

   // restore player's old target if a new one was found & set
   if(settarget && localtarget)
   {
      P_SetTarget<Mobj>(&(mo->target), oldtarget);
   }

   // put player back into his normal state
   if(statenum >= 0 && statenum < NUMSTATES)
   {
      mo->state = oldstate;
   }
}

//----------------------------------------------------------------------------
//
// $Log: p_pspr.c,v $
// Revision 1.13  1998/05/07  00:53:36  killough
// Remove dependence on order of evaluation
//
// Revision 1.12  1998/05/05  16:29:17  phares
// Removed RECOIL and OPT_BOBBING defines
//
// Revision 1.11  1998/05/03  22:35:21  killough
// Fix weapons switch bug again, beautification, headers
//
// Revision 1.10  1998/04/29  10:01:55  killough
// Fix buggy weapons switch code
//
// Revision 1.9  1998/03/28  18:01:38  killough
// Prevent weapon recoil in no-clipping mode
//
// Revision 1.8  1998/03/23  03:28:29  killough
// Move weapons changes to G_BuildTiccmd()
//
// Revision 1.7  1998/03/10  07:14:47  jim
// Initial DEH support added, minus text
//
// Revision 1.6  1998/02/24  08:46:27  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.5  1998/02/17  05:59:41  killough
// Use new RNG calling sequence
//
// Revision 1.4  1998/02/15  02:47:54  phares
// User-defined keys
//
// Revision 1.3  1998/02/09  03:06:15  killough
// Add player weapon preference options
//
// Revision 1.2  1998/01/26  19:24:18  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:00  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

