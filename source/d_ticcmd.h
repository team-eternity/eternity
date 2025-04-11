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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//   System specific interface stuff.
//
//-----------------------------------------------------------------------------

#ifndef D_TICCMD_H__
#define D_TICCMD_H__

#include "doomtype.h"

// NETCODE_FIXME: ticcmd_t lives here. It's the structure used to hold
// all player input that can currently be transmitted across the network.
// It is NOT sufficient for the following reasons:
// 1) It cannot efficiently transmit console cmd/cvar changes
// 2) It cannot efficiently transmit player chat (and never has)
// 3) Weapon changes and special events are packed into buttons using
//    special bit codes. This is hackish and artificially limiting, and
//    will create severe problems for the future generalized weapon
//    system.
// 4) There is no packing currently, so the entire ticcmd_t structure
//    is sent every tic, sometimes multiple times. This is horribly
//    inefficient and is hampering the addition of more inputs such as
//    flying, swimming, jumping, inventory use, etc.
//
// DEMO_FIXME: Warning -- changes to ticcmd_t must be reflected in the
// code that reads and writes demos as well.
// ticcmds are built in G_BuildTiccmd or by G_ReadDemoTiccmd
// ticcmds are transmitted over the network by code in d_net.c
//

// haleyjd 10/16/07: ticcmd_t must be packed

#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(push, 1)
#endif

// The data sampled per tick (single player)
// and transmitted to other peers (multiplayer).
// Mainly movements/button commands per game tick,
// plus a checksum for internal state consistency.
struct ticcmd_t
{
   int8_t  forwardmove; // *2048 for move
   int8_t  sidemove;    // *2048 for move
   int8_t  fly;         // haleyjd: flight
   int16_t look;        // haleyjd: <<16 for look delta
   int16_t angleturn;   // <<16 for angle delta
   int16_t consistency; // checks for net game
   byte    chatchar;
   byte    buttons;
   byte    actions;
   uint16_t itemID;     // MaxW: ID of used inventory item (+ 1)
   uint16_t weaponID;   // MaxW: ID of weapon to be made pending (+ 1)
   uint8_t  slotIndex;  // MaxW: Index of slot to switch to (if weaponID != 0)
};

#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(pop)
#endif

#endif

//----------------------------------------------------------------------------
//
// $Log: d_ticcmd.h,v $
// Revision 1.2  1998/01/26  19:26:36  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:08  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
