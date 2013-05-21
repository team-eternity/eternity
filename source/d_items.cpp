// Emacs style mode select -*- C++ -*- vi:ts=3:sw=3:set et:
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
#include "e_states.h"
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
weaponinfo_t    weaponinfo[NUMWEAPONS] =
{
  {
    // fist
    am_noammo,
    S_PUNCHUP,
    S_PUNCHDOWN,
    S_PUNCH,
    S_PUNCH1,
    S_NULL,
    0, false,
    WPF_FLEEMELEE | WPF_NOHITGHOSTS | WPF_SILENCER,
    MOD_FIST,
    10,
  },
  {
    // pistol
    am_clip,
    S_PISTOLUP,
    S_PISTOLDOWN,
    S_PISTOL,
    S_PISTOL1,
    S_PISTOLFLASH,
    1, false,
    WPF_SILENCER,
    MOD_PISTOL,
    10,
  },
  {
    // shotgun
    am_shell,
    S_SGUNUP,
    S_SGUNDOWN,
    S_SGUN,
    S_SGUN1,
    S_SGUNFLASH1,
    1, false,
    WPF_SILENCER,
    MOD_SHOTGUN,
    30,
  },
  {
    // chaingun
    am_clip,
    S_CHAINUP,
    S_CHAINDOWN,
    S_CHAIN,
    S_CHAIN1,
    S_CHAINFLASH1,
    1, false,
    WPF_SILENCER,
    MOD_CHAINGUN,
    10,
  },
  {
    // missile launcher
    am_misl,
    S_MISSILEUP,
    S_MISSILEDOWN,
    S_MISSILE,
    S_MISSILE1,
    S_MISSILEFLASH1,
    1, false,
    WPF_NOAUTOFIRE | WPF_SILENCER,
    MOD_UNKNOWN,
    100,
  },
  {
    // plasma rifle
    am_cell,
    S_PLASMAUP,
    S_PLASMADOWN,
    S_PLASMA,
    S_PLASMA1,
    S_PLASMAFLASH1,
    1, false,
    WPF_SILENCER,
    MOD_UNKNOWN,
    20,
  },
  {
    // bfg 9000
    am_cell,
    S_BFGUP,
    S_BFGDOWN,
    S_BFG,
    S_BFG1,
    S_BFGFLASH1,
    40, false,
    WPF_NOAUTOFIRE | WPF_SILENCER,
    MOD_UNKNOWN,
    100,
  },
  {
    // chainsaw
    am_noammo,
    S_SAWUP,
    S_SAWDOWN,
    S_SAW,
    S_SAW1,
    S_NULL,
    0, false,
    WPF_NOTHRUST | WPF_FLEEMELEE | WPF_NOHITGHOSTS | WPF_SILENCER,
    MOD_CHAINSAW,
    0,
  },
  {
    // super shotgun
    am_shell,
    S_DSGUNUP,
    S_DSGUNDOWN,
    S_DSGUN,
    S_DSGUN1,
    S_DSGUNFLASH1,
    2, false,
    WPF_SILENCER,
    MOD_SSHOTGUN,
    80,
  },
};

//
// haleyjd 07/25/03: temporary hack to resolve weapon states
// until EDF weapon support is in place
// WEAPON_FIXME
//
void D_InitWeaponInfo(void)
{
   int i;

   for(i = 0; i < NUMWEAPONS; ++i)
   {
      weaponinfo[i].atkstate   = E_SafeState(weaponinfo[i].atkstate);
      weaponinfo[i].downstate  = E_SafeState(weaponinfo[i].downstate);
      weaponinfo[i].flashstate = E_SafeState(weaponinfo[i].flashstate);
      weaponinfo[i].readystate = E_SafeState(weaponinfo[i].readystate);
      weaponinfo[i].upstate    = E_SafeState(weaponinfo[i].upstate);
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
