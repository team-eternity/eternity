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
// The weapon info structure.
//
//-----------------------------------------------------------------------------

// We are referring to sprite numbers.
#include "z_zone.h"
#include "info.h"
#include "d_items.h"
#include "d_gi.h"
#include "d_mod.h"
#include "e_inventory.h"
#include "e_states.h"
#include "e_weapons.h"
#include "sounds.h"
#include "w_wad.h"

// haleyjd 11/28/08: bit of a hack - replace checks on gamemode == commercial
// with tests on this flag to see if we want to be able to use the super 
// shotgun. This may need to change a bit when dynamic weapons are finished.
bool enable_ssg = false;

//
// PSPRITE ACTIONS for weapons.
// This struct controls the weapon animations.
//
// Each entry is:
//  ammo/amunition type
//  upstate
//  downstate
//  readystate
//  atkstate, i.e. attack/fire/hit frame
//  flashstate, muzzle flash
//  haleyjd 08/10/02: added ammopershot field to allow customized
//   ammo usage for any weapon via DeHackEd
//
weaponinfo_t weaponinfo[NUMWEAPONS] =
{
  {
    // fist
    wp_fist,
    "Fist",
    NULL,
    S_PUNCHUP,
    S_PUNCHDOWN,
    S_PUNCH,
    S_PUNCH1,
    S_NULL,
    0, 
    &weaponinfo[wp_chainsaw],
    &weaponinfo[wp_bfg],
    WPF_FLEEMELEE | WPF_NOHITGHOSTS | WPF_SILENCER,
    MOD_FIST,
    10
  },  
  {
    // pistol
    wp_pistol,
    "Pistol",
    NULL,
    S_PISTOLUP,
    S_PISTOLDOWN,
    S_PISTOL,
    S_PISTOL1,
    S_PISTOLFLASH,
    1, 
    &weaponinfo[wp_shotgun],
    &weaponinfo[wp_chainsaw],
    WPF_SILENCER | WPF_HAPTICRECOIL,
    MOD_PISTOL,
    10,
    1, 1,
    0,
    50, 20, 20, 10
  },  
  {
    // shotgun
    wp_shotgun,
    "Shotgun",
    NULL,
    S_SGUNUP,
    S_SGUNDOWN,
    S_SGUN,
    S_SGUN1,
    S_SGUNFLASH1,
    1,
    &weaponinfo[wp_supershotgun],
    &weaponinfo[wp_pistol],
    WPF_SILENCER | WPF_HAPTICRECOIL,
    MOD_SHOTGUN,
    30,
    3, 3,
    0,
    20, 8, 8, 4
  },
  {
    // chaingun
    wp_chaingun,
    "Chaingun",
    NULL,
    S_CHAINUP,
    S_CHAINDOWN,
    S_CHAIN,
    S_CHAIN1,
    S_CHAINFLASH1,
    1,
    &weaponinfo[wp_missile],
    &weaponinfo[wp_supershotgun],
    WPF_SILENCER | WPF_HAPTICRECOIL,
    MOD_CHAINGUN,
    10,
    1, 1,
    0,
    50, 20, 20, 10
  },
  {
    // missile launcher
    wp_missile,
    "MissileLauncher",
    NULL,
    S_MISSILEUP,
    S_MISSILEDOWN,
    S_MISSILE,
    S_MISSILE1,
    S_MISSILEFLASH1,
    1,
    &weaponinfo[wp_plasma],
    &weaponinfo[wp_chaingun],
    WPF_NOAUTOFIRE | WPF_SILENCER | WPF_HAPTICRECOIL,
    MOD_UNKNOWN,
    100,
    6, 8,
    0,
    5, 2, 2, 1
  },
  {
    // plasma rifle
    wp_plasma,
    "PlasmaRifle",
    NULL,
    S_PLASMAUP,
    S_PLASMADOWN,
    S_PLASMA,
    S_PLASMA1,
    S_PLASMAFLASH1,
    1,
    &weaponinfo[wp_bfg],
    &weaponinfo[wp_missile],
    WPF_SILENCER | WPF_HAPTICRECOIL | WPF_NOTSHAREWARE,
    MOD_UNKNOWN,
    20,
    2, 1,
    0,
    100, 40, 40, 20
  },
  {
    // bfg 9000
    wp_bfg,
    "BFG9000",
    NULL,
    S_BFGUP,
    S_BFGDOWN,
    S_BFG,
    S_BFG1,
    S_BFGFLASH1,
    40,
    &weaponinfo[wp_fist],
    &weaponinfo[wp_plasma],
    WPF_NOAUTOFIRE | WPF_SILENCER | WPF_HAPTICRECOIL | WPF_NOTSHAREWARE,
    MOD_UNKNOWN,
    100,
    10, 10,
    0,
    100, 40, 40, 20
  },
  {
    // chainsaw
    wp_chainsaw,
    "Chainsaw",
    NULL,
    S_SAWUP,
    S_SAWDOWN,
    S_SAW,
    S_SAW1,
    S_NULL,
    0,
    &weaponinfo[wp_pistol],
    &weaponinfo[wp_fist],
    WPF_NOTHRUST | WPF_FLEEMELEE | WPF_NOHITGHOSTS | WPF_SILENCER,
    MOD_CHAINSAW,
    0,
    0, 0,
    sfx_sawup
  },
  {
    // super shotgun
    wp_supershotgun,
    "SuperShotgun",
    NULL,
    S_DSGUNUP,
    S_DSGUNDOWN,
    S_DSGUN,
    S_DSGUN1,
    S_DSGUNFLASH1,
    2,
    &weaponinfo[wp_chaingun],
    &weaponinfo[wp_shotgun],
    WPF_SILENCER | WPF_HAPTICRECOIL,
    MOD_SSHOTGUN,
    80,
    5, 5,
    0,
    20, 8, 8, 4
  },  
};

// INVENTORY_FIXME: temporary default ammo type names for each weapon;
// eventually to be specified via EDF
static const char *d_ammoTypesForWeapons[NUMWEAPONS] =
{
   NULL,          // fist
   "AmmoClip",    // pistol
   "AmmoShell",   // shotgun
   "AmmoClip",    // chaingun
   "AmmoMissile", // rocket launcher
   "AmmoCell",    // plasma gun
   "AmmoCell",    // BFG
   NULL,          // chainsaw
   "AmmoShell"    // SSG
};

//
// haleyjd 07/25/03: temporary hack to resolve weapon states
// until EDF weapon support is in place
// WEAPON_FIXME
// INVENTORY_TODO: weapon init
//
void D_InitWeaponInfo()
{
   int i;

   for(i = 0; i < NUMWEAPONS; ++i)
   {
      weaponinfo[i].atkstate   = E_SafeState(weaponinfo[i].atkstate);
      weaponinfo[i].downstate  = E_SafeState(weaponinfo[i].downstate);
      weaponinfo[i].flashstate = E_SafeState(weaponinfo[i].flashstate);
      weaponinfo[i].readystate = E_SafeState(weaponinfo[i].readystate);
      weaponinfo[i].upstate    = E_SafeState(weaponinfo[i].upstate);

      if(d_ammoTypesForWeapons[i])
         weaponinfo[i].ammo = E_ItemEffectForName(d_ammoTypesForWeapons[i]);
   }

   // haleyjd 11/28/08: SSG enable
   if(GameModeInfo->type == Game_DOOM && 
      W_CheckNumForNameNS("SHT2A0", lumpinfo_t::ns_sprites) > 0)
      enable_ssg = true;
}

//----------------------------------------------------------------------------
//
// $Log: d_items.c,v $
// Revision 1.4  1998/05/04  21:34:09  thldrmn
// commenting and reformatting
//
// Revision 1.2  1998/01/26  19:23:03  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:07  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
