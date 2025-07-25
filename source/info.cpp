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
//------------------------------------------------------------------------------
//
// Purpose: Thing frame/state LUT, generated by multigen utility.
//  This one is the original DOOM version, preserved.
//  BOOM changes include commenting and addition of predefined lumps
//  for providing things that aren't in the IWAD without sending a
//  separate must-use wad file around with the EXE.
//
// Authors: James Haley
//

//
//  haleyjd 07/10/03: this file is now mostly extinct, and only
//  contains declarations for the sprnames, states, and mobjinfo
//  array pointers, which are allocated and initialized by EDF.
//  See e_edf.c
//

// haleyjd: removed now-unnecessary headers
#include "z_zone.h"
#include "info.h"
#include "e_things.h"

// ********************************************************************
// Sprite names
// ********************************************************************
// This is the list of sprite 4-character prefixes.  They are searched
// through, with a nullptr entry terminating the list.  In DOOM originally
// this nullptr entry was missing, and coincidentally the next thing in
// memory was the dummy state_t[] entry that started with zero bytes.
// killough 1/17/98: add an explicit nullptr entry.
// NUMSPRITES is an enum from info.h where all these are listed
// as SPR_xxxx

// haleyjd: made dynamic via EDF
char **sprnames;
int    NUMSPRITES = 0;

// ********************************************************************
// Function addresses or Code Pointers
// ********************************************************************
// These function addresses are the Code Pointers that have been
// modified for years by Dehacked enthusiasts.  The new BEX format
// allows more extensive changes (see d_deh.c)

// haleyjd: removed codepointer prototypes, unneeded

// ********************************************************************
// State or "frame" information
// ********************************************************************
// Each of the states, otherwise known as "frames", is outlined
// here.  The data in each element of the array is the way it is
// initialized, with sprite names identified by their enumerator
// value such as SPR_SHTG.  These correlate to the above sprite
// array so don't change them around unless you understand what
// you're doing.
//
// The commented name beginning with S_ at the end of each line
// is there to help figure out where the next-frame pointer is
// pointing.  These are also additionally identified in info.h
// as enumerated values.  From a change-and-recompile point of
// view this is fairly workable, but it adds a lot to the effort
// when trying to change things externally.  See also the d_deh.c
// parts where frame rewiring is done for more details and the
// extended way a BEX file can handle this.

// haleyjd: made dynamic via EDF
// haleyjd 6/18/09: made into pointer-to-pointer
state_t **states    = nullptr;
int       NUMSTATES = 0;

// ********************************************************************
// Object "Thing" definitions
// ********************************************************************
// Now we get to the actual objects and their characteristics.  If
// you've seen Dehacked, much of this is where the Bits are set,
// commented below as "flags", as well as where you wire in which
// frames are the beginning frames for near and far attack, death,
// and such.  Sounds are hooked in here too, as well as how much
// mass, speed and so forth a Thing has.  Everything you ever wanted
// to know...
//
// Like all this other stuff, the MT_* entries are enumerated in info.h
//
// Note that these are all just indices of the elements involved, and
// not real pointers to them.  For example, the player's death sequence
// is S_PLAY_DIE1, which just evaluates to the index in the states[]
// array above, which actually knows what happens then and what the
// sprite looks like, if it makes noise or not, etc.
//
// Additional comments about each of the entries are located in info.h
// next to the mobjinfo_t structure definition.
//
// This goes on for the next 3000+ lines...

// haleyjd: made dynamic via EDF
// haleyjd 11/03/11: made into pointer-to-pointer
mobjinfo_t **mobjinfo     = nullptr;
int          NUMMOBJTYPES = 0;

//----------------------------------------------------------------------------
//
// $Log: info.c,v $
// Revision 1.44  1998/05/12  12:46:36  phares
// Removed OVER_UNDER code
//
// Revision 1.43  1998/05/12  09:35:07  phares
// Corrected 4001->5001 and 4002->5002 in OVER/UNDER table
//
// Revision 1.42  1998/05/12  08:41:13  jim
// fix decl of endboom
//
// Revision 1.40  1998/05/11  12:21:20  jim
// 4001/2 deconflicted with DosDOOM
//
// Revision 1.39  1998/05/06  11:30:54  jim
// Moved predefined lump writer info->w_wad
//
// Revision 1.38  1998/05/04  21:34:49  thldrmn
// commenting and reformatting
//
// Revision 1.37  1998/05/03  23:23:50  killough
// Fix #includes at the top, nothing else
//
// Revision 1.36  1998/04/29  09:20:37  jim
// New ENDBOOM
//
// Revision 1.35  1998/04/27  02:15:10  killough
// Fix cr_gold declaration, add missing v1.1 lumps
//
// Revision 1.34  1998/04/24  08:08:36  jim
// Make text translate tables lumps
//
// Revision 1.33  1998/04/22  13:45:37  phares
// Added Setup screen Reset to Defaults
//
// Revision 1.32  1998/04/22  06:34:43  killough
// Make WritePredefinedLumpWad endian-independent, remove tabs
//
// Revision 1.31  1998/04/21  23:46:21  jim
// Predefined lump dumper option
//
// Revision 1.30  1998/04/17  00:04:11  jim
// text file changes and new ENDBOOM
//
// Revision 1.29  1998/04/12  22:54:55  phares
// Remaining 3 Setup screens
//
// Revision 1.28  1998/04/06  04:36:51  killough
// Change WATERMAP, add C_START/C_END
//
// Revision 1.27  1998/04/05  10:10:13  jim
// added STCFN096 lump
//
// Revision 1.26  1998/04/03  19:18:46  phares
// Automap Palette work, slot 0 = disable, 247 = BLACK
//
// Revision 1.25  1998/04/02  05:01:49  jim
// Added ENDOOM, BOOM.TXT mods
//
// Revision 1.24  1998/04/01  15:34:30  phares
// Added Automap Setup Screen, fixed Seg Viol in Setup Menus
//
// Revision 1.23  1998/03/31  01:08:26  phares
// Initial Setup screens and Extended HELP screens
//
// Revision 1.22  1998/03/23  18:39:10  jim
// Switch and animation tables now lumps
//
// Revision 1.21  1998/03/23  15:23:54  phares
// Changed pushers to linedef control
//
// Revision 1.20  1998/03/23  03:18:09  killough
// Add WATERMAP colormap lump for underwater viewing
//
// Revision 1.19  1998/03/09  18:30:28  phares
// Added invisible sprite for MT_PUSH
//
// Revision 1.18  1998/03/09  07:15:14  killough
// Remove unnecessary translucency lumps
//
// Revision 1.17  1998/03/04  22:23:04  phares
// Removed BOOMHELP predefined lump
//
// Revision 1.16  1998/03/04  11:52:43  jim
// Add TRAN50 TRAN66 predefined lumps
//
// Revision 1.15  1998/03/03  00:21:54  jim
// Added predefined ENDBETA lump for beta test
//
// Revision 1.13  1998/02/27  11:51:50  jim
// Add predefined lump STTMINUS
//
// Revision 1.11  1998/02/24  08:45:44  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.10  1998/02/24  04:13:37  jim
// Added double keys to status
//
// Revision 1.8  1998/02/23  04:31:13  killough
// Make tranlucency apply realistically
//
// Revision 1.7  1998/02/22  12:51:38  jim
// HUD control on F5, z coord, spacing change
//
// Revision 1.5  1998/02/20  21:56:49  phares
// Preliminarey sprite translucency
//
// Revision 1.4  1998/02/18  00:59:44  jim
// Addition of HUD
//
// Revision 1.3  1998/02/02  13:36:12  killough
// Add predefined lumps
//
// Revision 1.2  1998/01/26  19:23:34  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:56  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
