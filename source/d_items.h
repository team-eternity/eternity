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
//  Items: key cards, artifacts, weapon, ammunition.
//
//-----------------------------------------------------------------------------

#ifndef D_ITEMS_H__
#define D_ITEMS_H__

#include "doomdef.h"
#include "m_dllist.h"

//
// haleyjd 09/11/07: weapon flags
//
enum
{
   WPF_NOTHRUST      = 0x00000001, // doesn't thrust Mobj's
   WPF_NOHITGHOSTS   = 0x00000002, // tracer-based weapon can't hit ghosts
   WPF_NOTSHAREWARE  = 0x00000004, // not in shareware gamemodes
   WPF_UNUSED        = 0x00000008, // (was WPF_COMMERCIAL, free for use)
   WPF_SILENCER      = 0x00000010, // weapon supports silencer powerup
   WPF_SILENT        = 0x00000020, // weapon is always silent
   WPF_NOAUTOFIRE    = 0x00000040, // weapon won't autofire in A_WeaponReady
   WPF_FLEEMELEE     = 0x00000080, // monsters consider it a melee weapon
   WPF_ALWAYSRECOIL  = 0x00000100, // weapon always has recoil
   WPF_HAPTICRECOIL  = 0x00000200, // use recoil-style haptic effect
};

// Weapon info: sprite frames, ammunition use.
struct weaponinfo_t
{
   weapontype_t id;           // haleyjd 06/28/13: weapontype id number
   const char  *name;         // haleyjd 06/29/13: name of weapon

   ammotype_t   ammo;
   int          upstate;
   int          downstate;
   int          readystate;
   int          atkstate;
   int          flashstate;
   int          ammopershot;  // haleyjd 08/10/02: ammo per shot field
   int          enableaps;    // haleyjd: enables above field, off by default
   
   // haleyjd 09/11/07: new fields in prep. for dynamic weapons
   int          flags;
   int          mod;
   int          recoil;
   int          hapticrecoil; // haptic recoil strength, from 1 to 10
   int          haptictime;   // haptic recoil duration, from 1 to 10
   int          upsound;      // sound made when weapon is being brought up

   // EDF hashing
   DLListItem<weaponinfo_t> idlinks;   // hash by id
   DLListItem<weaponinfo_t> namelinks; // hash by name
};

extern weaponinfo_t weaponinfo[NUMWEAPONS];

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
