//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Deathmatch Flags
// Authors: James Haley
//

#ifndef G_DMFLAG_H__
#define G_DMFLAG_H__

enum
{
    DM_ITEMRESPAWN   = 0x00000001, // items respawn
    DM_WEAPONSTAY    = 0x00000002, // weapons stay
    DM_BARRELRESPAWN = 0x00000004, // barrels respawn (dm3)
    DM_PLAYERDROP    = 0x00000008, // players drop items (dm3)
    DM_RESPAWNSUPER  = 0x00000010, // respawning super powerups
    DM_INSTAGIB      = 0x00000020, // any damage gibs players
    DM_KEEPITEMS     = 0x00000040, // keep items when reborn
};

// Default dmflags for certain game modes

static constexpr uint32_t DMD_SINGLE      = 0;
static constexpr uint32_t DMD_COOP        = DM_WEAPONSTAY;
static constexpr uint32_t DMD_DEATHMATCH  = DM_WEAPONSTAY;
static constexpr uint32_t DMD_DEATHMATCH2 = DM_ITEMRESPAWN;
static constexpr uint32_t DMD_DEATHMATCH3 = DM_ITEMRESPAWN | DM_WEAPONSTAY | DM_BARRELRESPAWN | DM_PLAYERDROP;

extern unsigned int dmflags;
extern unsigned int default_dmflags;

void G_SetDefaultDMFlags(int dmtype, bool setdefault);

#endif

// EOF

