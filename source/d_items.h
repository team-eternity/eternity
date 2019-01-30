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
//  Items: key cards, artifacts, weapon, ammunition.
//
//-----------------------------------------------------------------------------

#ifndef D_ITEMS_H__
#define D_ITEMS_H__

#include "doomdef.h"
#include "m_dllist.h"
#include "m_fixed.h"

using itemeffect_t = class MetaTable;
class qstring;

//
// haleyjd 09/11/07: weapon flags
//
enum wepflags_e : unsigned int
{
   WPF_NOTHRUST       = 0x00000001, // doesn't thrust Mobj's
   WPF_NOHITGHOSTS    = 0x00000002, // tracer-based weapon can't hit ghosts
   WPF_NOTSHAREWARE   = 0x00000004, // not in shareware gamemodes
   WPF_DISABLEAPS     = 0x00000008, // disables ammo-per-shot field
   WPF_SILENCEABLE    = 0x00000010, // weapon supports silencer powerup
   WPF_SILENT         = 0x00000020, // weapon is always silent
   WPF_NOAUTOFIRE     = 0x00000040, // weapon won't autofire in A_WeaponReady
   WPF_FLEEMELEE      = 0x00000080, // monsters consider it a melee weapon
   WPF_ALWAYSRECOIL   = 0x00000100, // weapon always has recoil
   WPF_HAPTICRECOIL   = 0x00000200, // use recoil-style haptic effect
   WPF_READYSNDHALF   = 0x00000400, // readysound has 50% chance to play
   WPF_AUTOSWITCHFROM = 0x00000800, // switches away if ammo for a wep w/o this is picked up
   WPF_POWEREDUP      = 0x00001000, // powered up weapon (tomed weapons in Heretic)
   WPF_FORCETOREADY   = 0x00002000, // force to readystate on receiving/losing pw_weaponlevel2
};

//
// MaxW: 2018/04/04: internal weapon flags
//
enum wepinternalflags_e : uint8_t
{
   WIF_HASSORTORDER = 0x01, // has had a sort order assigned to it
   WIF_INGLOBALSLOT = 0x02, // has been placed in the global weaponslots
};

// Weapon info: sprite frames, ammunition use.
struct weaponinfo_t
{
   weapontype_t id;           // haleyjd 06/28/13: weapontype id number
   const char  *name;         // haleyjd 06/29/13: name of weapon
   int          dehnum;       // MaxW: 2017/12/30: DeHackEd number for fast access & compat.

   MetaTable   *ammo;         // haleyjd 08/05/13: ammo artifact type
   int          upstate;
   int          downstate;
   int          readystate;
   int          atkstate;
   int          flashstate;
   int          holdstate;    // MaxW: 2018/01/01: state jumped to if fire is held
   int          ammopershot;  // haleyjd 08/10/02: ammo per shot field

   MetaTable   *ammo_alt;         // Alt ammo artifact type
   int          atkstate_alt;     // Alt attack state
   int          flashstate_alt;   // Alt flash state
   int          holdstate_alt;    // Alt hold state
   int          ammopershot_alt;  // Alt ammo per shot

   int           defaultslotindex;
   fixed_t       defaultslotrank;

   int           sortorder;   // sort order (lower is higher priority)
   weaponinfo_t *sisterWeapon; // sister weapon (e.g.: tomed variant, Strife-style "alt-fire")

   // haleyjd 09/11/07: new fields in prep. for dynamic weapons
   unsigned int flags;
   uint8_t      intflags;
   int          mod;
   fixed_t      recoil;
   int          hapticrecoil; // haptic recoil strength, from 1 to 10
   int          haptictime;   // haptic recoil duration, from 1 to 10
   const char  *upsound;      // sound made when weapon is being brought up
   const char  *readysound;   // sound made when weapon is ready

   int fullscreenoffset;   // how much to vertically offset a weapon on fullscreen (for Heretic)

   itemeffect_t *tracker;     // tracker artifact for weapon

   // EDF hashing
   DLListItem<weaponinfo_t> idlinks;   // hash by id
   DLListItem<weaponinfo_t> namelinks; // hash by name
   DLListItem<weaponinfo_t> dehlinks;  // hash by dehnum

   int   generation;   // EDF generation number

   MetaTable *meta; // metatable

   weaponinfo_t *parent; // inheritance chain for DECORATE-like semantics where required
};

// haleyjd: temporary hack
void D_InitWeaponInfo();

extern bool enable_ssg;

#endif

//----------------------------------------------------------------------------
//
// $Log: d_items.h,v $
// Revision 1.3  1998/05/04  21:34:12  thldrmn
// commenting and reformatting
//
// Revision 1.2  1998/01/26  19:26:26  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:07  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
