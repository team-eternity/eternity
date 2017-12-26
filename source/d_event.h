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
// Event-handling structures.
//    
//-----------------------------------------------------------------------------

#ifndef D_EVENT_H__
#define D_EVENT_H__

//
// Event handling.
//

// Input event types.
enum evtype_t : int
{
  ev_keydown,
  ev_keyup,
  ev_mouse,
  ev_joystick,
  ev_text
};

// Event structure.
struct event_t
{
  evtype_t  type;
  int       data1;     // keys / mouse/joystick buttons
  double    data2;     // mouse/joystick x move
  double    data3;     // mouse/joystick y move
  bool      repeat;    // true if this input is a repeat
};
 
typedef enum
{
  ga_nothing,
  ga_loadlevel,
  ga_newgame,
  ga_loadgame,
  ga_savegame,
  ga_playdemo,
  ga_completed,
  ga_victory,
  ga_worlddone,
  ga_screenshot
} gameaction_t;

//
// Button/action code definitions.
//
typedef enum
{
  // Press "Fire".
  BT_ATTACK       = 1,

  // Use button, to open doors, activate switches.
  BT_USE          = 2,

  // Flag: game events, not really buttons.
  BT_SPECIAL      = 128,
  //  BT_SPECIALMASK  = 3,   killough 9/29/98: unused now
    
  // Flag, weapon change pending.
  // If true, the next 4 bits hold weapon num.
  BT_CHANGE       = 4,

  // The 4bit weapon mask and shift, convenience.
  // ioanch 20160426: added demo compat weapon mask
  BT_WEAPONMASK_OLD = (8+16+32),
  BT_WEAPONMASK   = (8+16+32+64), // extended to pick up SSG        // phares
  BT_WEAPONSHIFT  = 3,

  // Pause the game.
  BTS_PAUSE       = 1,

  // Save the game at each console.
  BTS_SAVEGAME    = 2,

  // Savegame slot numbers occupy the second byte of buttons.    
  BTS_SAVEMASK    = (4+8+16),
  BTS_SAVESHIFT   = 2,
  
} buttoncode_t;

//
// Player action code definitions (jumping etc) -- joek 12/22/07
//
typedef enum
{
   // Bouncy bouncy jump jump
   AC_JUMP       = 1,
} actioncode_t;

//
// GLOBAL VARIABLES
//

extern gameaction_t gameaction;

#endif

//----------------------------------------------------------------------------
//
// $Log: d_event.h,v $
// Revision 1.4  1998/05/05  19:55:53  phares
// Formatting and Doc changes
//
// Revision 1.3  1998/02/15  02:48:04  phares
// User-defined keys
//
// Revision 1.2  1998/01/26  19:26:23  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:52  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
