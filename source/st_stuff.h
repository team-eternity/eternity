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
//      Status bar code.
//      Does the face/direction indicator animatin.
//      Does palette indicators as well (red pain/berserk, bright pickup)
//
//-----------------------------------------------------------------------------

#ifndef ST_STUFF_H__
#define ST_STUFF_H__

#include "doomtype.h"
#include "d_event.h"
#include "r_defs.h"

struct patch_t;

// Size of statusbar.
// Now sensitive for scaling.

#define ST_HEIGHT (32*SCREEN_MUL)
#define ST_WIDTH  SCREENWIDTH
#define ST_Y      (SCREENHEIGHT - ST_HEIGHT)

//
// STATUS BAR
//

// Called by main loop.
bool ST_Responder(const event_t* ev);

// Called by main loop.
void ST_Ticker(void);

// Called by main loop.
void ST_Drawer(bool fullscreen);

// Called when the console player is spawned on each level.
void ST_Start(void);

// Called by startup code.
void ST_Init(void);

void ST_CacheFaces(patch_t **faces, const char *facename);

// Others
void ST_DrawSmallHereticNumber(int val, int x, int y, bool fullscreen);
bool ST_IsHUDLike();

// haleyjd 10/12/03: structure for gamemode-independent status bar interface

struct stbarfns_t
{
   // data
   int  height;

   // function pointers
   void (*Ticker)(void);   // tic processing
   void (*Drawer)(void);   // drawing
   void (*FSDrawer)(void); // fullscreen drawer
   void (*Start)(void);    // reinit
   void (*Init)(void);     // initialize at startup   
};

extern stbarfns_t DoomStatusBar;
extern stbarfns_t HticStatusBar;
extern stbarfns_t StrfStatusBar;

// States for status bar code.
typedef enum
{
  AutomapState,
  FirstPersonState
} st_stateenum_t;

// killough 5/2/98: moved from m_misc.c:

extern int  health_red;     // health amount less than which status is red
extern int  health_yellow;  // health amount less than which status is yellow
extern int  health_green;   // health amount above is blue, below is green
extern int  armor_red;      // armor amount less than which status is red
extern int  armor_yellow;   // armor amount less than which status is yellow
extern int  armor_green;    // armor amount above is blue, below is green
extern bool armor_byclass;  // reflect armor class with blue vs green
extern int  ammo_red;       // ammo percent less than which status is red
extern int  ammo_yellow;    // ammo percent less is yellow more green
extern int  sts_always_red; // status numbers do not change colors
extern int  sts_pct_always_gray;  // status percents do not change colors
extern int  sts_traditional_keys; // display keys the traditional way
extern int  st_fsalpha;     // haleyjd 02/27/10: fullscreen hud alpha

// Number of status faces.
#define ST_NUMPAINFACES         5
#define ST_NUMSTRAIGHTFACES     3
#define ST_NUMTURNFACES         2
#define ST_NUMSPECIALFACES      3

#define ST_FACESTRIDE \
          (ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES)

#define ST_NUMEXTRAFACES        2

#define ST_NUMFACES \
          (ST_FACESTRIDE*ST_NUMPAINFACES+ST_NUMEXTRAFACES)

extern patch_t *default_faces[ST_NUMFACES];

#endif

//----------------------------------------------------------------------------
//
// $Log: st_stuff.h,v $
// Revision 1.4  1998/05/03  22:50:55  killough
// beautification, move external declarations, remove cheats
//
// Revision 1.3  1998/04/19  01:10:39  killough
// Generalize cheat engine to add deh support
//
// Revision 1.2  1998/01/26  19:27:56  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:04  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
