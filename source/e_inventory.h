// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 James Haley
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
//    EDF Inventory
//
//-----------------------------------------------------------------------------

#ifndef E_INVENTORY_H__
#define E_INVENTORY_H__

#include "m_dllist.h"
#include "e_hashkeys.h"

// Inventory flags
enum
{
   INVF_AUTOACTIVATE    = 0x00000001,
   INVF_UNDROPPABLE     = 0x00000002,
   INVF_INVBAR          = 0x00000004,
   INVF_HUBPOWER        = 0x00000008,
   INVF_PERSISTENTPOWER = 0x00000010,
   INVF_ALWAYSPICKUP    = 0x00000020,
   INVF_KEEPDEPLETED    = 0x00000040,
   INVF_ADDITIVETIME    = 0x00000080,
   INVF_UNTOSSABLE      = 0x00000100

   // Possible TODOs:
   // INV_BIGPOWERUP  - redundant with flags value
   // INV_IGNORESKILL - would rather have more flexible skill props...
};

//
// inventory_t
//
// This is the primary inventory definition structure.
//
struct inventory_t
{
   // hash links
   DLListItem<inventory_t> namelinks;
   DLListItem<inventory_t> numlinks;

   // basic properties
   int   amount;         // amount given on pickup
   int   maxAmount;      // maximum amount that can be carried
   int   interHubAmount; // amount that persists between hubs or non-hub levels
   char *icon;           // name of icon patch graphic
   char *pickUpMessage;  // message given when picked up (BEX if starts with $)
   char *pickUpSound;    // name of pickup sound, if any
   char *pickUpFlash;    // name of pickup flash actor type
   char *useSound;       // name of use sound, if any
   int   respawnTics;    // length of time it takes to respawn w/item respawn on
   int   giveQuest;      // quest flag # given, if non-zero

   // fields needed for EDF identification and hashing
   ENCStringHashKey name;         // pointer to name
   char             namebuf[129]; // buffer for name (max 128 chars)
   EIntHashKey      idnum;        // ID number
};

#ifdef NEED_EDF_DEFINITIONS

#define EDF_SEC_INVENTORY "inventory"
#define EDF_SEC_INVDELTA  "inventorydelta"

extern cfg_opt_t edf_inv_opts[];
extern cfg_opt_t edf_invdelta_opts[];

void E_ProcessInventoryDefs(cfg_t *cfg);
void E_ProcessInventoryDeltas(cfg_t *cfg);

#endif

#endif

// EOF

