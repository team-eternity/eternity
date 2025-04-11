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
#include "d_items.h"
#include "d_gi.h"
#include "w_wad.h"

// haleyjd 11/28/08: bit of a hack - replace checks on gamemode == commercial
// with tests on this flag to see if we want to be able to use the super
// shotgun. This may need to change a bit when dynamic weapons are finished.
bool enable_ssg = false;


// MaxW: 2018/05/30: This comment (mostly) preserved for historical purposes

//
// haleyjd 07/25/03: temporary hack to resolve weapon states
// until EDF weapon support is in place
//
void D_InitWeaponInfo()
{
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
