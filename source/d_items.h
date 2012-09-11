// Emacs style mode select   -*- C++ -*- vi:ts=3:sw=3:set et: 
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

//
// haleyjd 09/11/07: weapon flags
//
enum
{
   WPF_NOTHRUST     = 0x00000001, // doesn't thrust Mobj's
   WPF_NOHITGHOSTS  = 0x00000002, // tracer-based weapon can't hit ghosts
   WPF_NOTSHAREWARE = 0x00000004, // not in shareware gamemodes
   WPF_UNUSED       = 0x00000008, // (was WPF_COMMERCIAL, free for use)
   WPF_SILENCER     = 0x00000010, // weapon supports silencer powerup
   WPF_SILENT       = 0x00000020, // weapon is always silent
   WPF_NOAUTOFIRE   = 0x00000040, // weapon won't autofire in A_WeaponReady
   WPF_FLEEMELEE    = 0x00000080, // monsters consider it a melee weapon
   WPF_ALWAYSRECOIL = 0x00000100, // weapon always has recoil
};

// Weapon info: sprite frames, ammunition use.
typedef struct weaponinfo_s
{
   ammotype_t  ammo;
   int         upstate;
   int         downstate;
   int         readystate;
   int         atkstate;
   int         flashstate;
   int         ammopershot; // haleyjd 08/10/02: ammo per shot field
   int         enableaps;   // haleyjd: enables above field, off by default
   
   // haleyjd 09/11/07: new fields in prep. for dynamic weapons
   int         flags;
   int         mod;
   int         recoil;
} weaponinfo_t;

extern weaponinfo_t weaponinfo[NUMWEAPONS];

// haleyjd: temporary hack
void D_InitWeaponInfo(void);

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
