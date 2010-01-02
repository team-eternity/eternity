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
//  Internally used data structures for virtually everything,
//   key definitions, lots of other stuff.
//
//-----------------------------------------------------------------------------

#ifndef __DOOMDEF__
#define __DOOMDEF__

// killough 4/25/98: Make gcc extensions mean nothing on other compilers
// haleyjd 05/22/02: moved to d_keywds.h
#include "d_keywds.h"

// This must come first, since it redefines malloc(), free(), etc. -- killough:
#include "z_zone.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "m_swap.h"
#include "version.h"
#include "d_mod.h"
#include "m_fixed.h" // SoM 2-4-04: ANYRES

// Identify language to use, software localization.
typedef enum {
  english,
  french,
  german,
  unknown
} Language_t;

//
// For resize of screen, at start of game.
//

#define BASE_WIDTH 320

// It is educational but futile to change this
//  scaling e.g. to 2. Drawing of status bar,
//  menues etc. is tied to the scale implied
//  by the graphics.

#define SCREEN_MUL         1
#define INV_ASPECT_RATIO   0.625 /* 0.75, ideally */

// killough 2/8/98: MAX versions for maximum screen sizes
// allows us to avoid the overhead of dynamic allocation
// when multiple screen sizes are supported

// SoM 2-4-04: ANYRES
#define MAX_SCREENWIDTH  2560
#define MAX_SCREENHEIGHT 1600

// SoM: replaced globals with a struct and a single global
typedef struct
{
   // SoM: Not implemented (yet)
   int         bitdepth, pixelsize;

   int         width, height;
   int         pitch;
   fixed_t     widthfrac, heightfrac;
   fixed_t     xscale, yscale;
   fixed_t     xstep, ystep;

   float       xscalef, yscalef;
   float       xstepf, ystepf;
   boolean     scaled; // SoM: should be set when the scale values are

   byte        *screens[5];

   // SoM 1-31-04: This will insure that scaled patches and such are put in the right places
   int x1lookup[321];
   int y1lookup[201];
   int x2lookup[321];
   int y2lookup[201];
} cb_video_t;

extern cb_video_t video;

#define SCREENWIDTH      320
#define SCREENHEIGHT     200

// The maximum number of players, multiplayer/networking.
#define MAXPLAYERS       4

// phares 5/14/98:
// DOOM Editor Numbers (aka doomednum in mobj_t)

#define DEN_PLAYER5 4001
#define DEN_PLAYER6 4002
#define DEN_PLAYER7 4003
#define DEN_PLAYER8 4004

// State updates, number of tics / second.
#define TICRATE          35

// The current state of the game: whether we are playing, gazing
// at the intermission screen, the game final animation, or a demo.

typedef enum {
  GS_LEVEL,
  GS_INTERMISSION,
  GS_FINALE,
  GS_DEMOSCREEN,
  GS_CONSOLE,
  GS_STARTUP
} gamestate_t;

//
// Difficulty/skill settings/filters.
//
// These are Thing flags

// Skill flags.
#define MTF_EASY                1
#define MTF_NORMAL              2
#define MTF_HARD                4
// Deaf monsters/do not react to sound.
#define MTF_AMBUSH              8

// killough 11/98
#define MTF_NOTSINGLE          16
#define MTF_NOTDM              32
#define MTF_NOTCOOP            64
#define MTF_FRIEND            128
#define MTF_RESERVED          256

// haleyjd
#define MTF_DORMANT           512

// sf: sector flags, not me =)
                // kill all sound in sector
#define SF_KILLSOUND          1024
                // kill all sounds due to moving
#define SF_KILLMOVESOUND      2048

// a macro to find out whether to make moving sounds in a sector
#define silentmove(s) ((s)->flags & SECF_KILLMOVESOUND)

typedef enum 
{
  sk_none=-1, //jff 3/24/98 create unpicked skill setting
  sk_baby=0,
  sk_easy,
  sk_medium,
  sk_hard,
  sk_nightmare
} skill_t;

//
// Key cards.
//

typedef enum {
  it_bluecard,
  it_yellowcard,
  it_redcard,
  it_blueskull,
  it_yellowskull,
  it_redskull,
  NUMCARDS
} card_t;

// The defined weapons, including a marker
// indicating user has not changed weapon.
typedef enum {
  wp_fist,
  wp_pistol,
  wp_shotgun,
  wp_chaingun,
  wp_missile,
  wp_plasma,
  wp_bfg,
  wp_chainsaw,
  wp_supershotgun,

  NUMWEAPONS,
  wp_nochange              // No pending weapon change.
} weapontype_t;

// Ammunition types defined.
typedef enum {
  am_clip,    // Pistol / chaingun ammo.
  am_shell,   // Shotgun / double barreled shotgun.
  am_cell,    // Plasma rifle, BFG.
  am_misl,    // Missile launcher.
  
  NUMAMMO,
  am_noammo   // Unlimited for chainsaw / fist.
} ammotype_t;

// Power up artifacts.
typedef enum {
  pw_invulnerability,
  pw_strength,
  pw_invisibility,
  pw_ironfeet,
  pw_allmap,
  pw_infrared,
  pw_totalinvis,  // haleyjd: total invisibility
  pw_ghost,       // haleyjd: heretic ghost
  pw_silencer,    // haleyjd: silencer
  NUMPOWERS
} powertype_t;

// Power up durations (how many seconds till expiration).
typedef enum {
  INVULNTICS  = (30*TICRATE),
  INVISTICS   = (60*TICRATE),
  INFRATICS   = (120*TICRATE),
  IRONTICS    = (60*TICRATE)
} powerduration_t;

// DOOM keyboard definition.
// This is the stuff configured by Setup.Exe.
// Most key data are simple ascii (uppercased).

#define KEYD_TAB        0x09
#define KEYD_ENTER      0x0d
#define KEYD_ESCAPE     0x1b

#define KEYD_SPACEBAR   0x20

#define KEYD_MINUS      0x2d
#define KEYD_EQUALS     0x3d

#define KEYD_BACKSPACE  0x7f

#define KEYD_RCTRL      0x9d //(0x80+0x1d)

#define KEYD_LEFTARROW  0xac
#define KEYD_UPARROW    0xad
#define KEYD_RIGHTARROW 0xae
#define KEYD_DOWNARROW  0xaf

#define KEYD_RSHIFT     0xb6 //(0x80+0x36)
#define KEYD_RALT       0xb8 //(0x80+0x38)
#define KEYD_LALT       KEYD_RALT

#define KEYD_CAPSLOCK   0xba                 // phares 

#define KEYD_F1         0xbb //(0x80+0x3b)
#define KEYD_F2         0xbc //(0x80+0x3c)
#define KEYD_F3         0xbd //(0x80+0x3d)
#define KEYD_F4         0xbe //(0x80+0x3e)
#define KEYD_F5         0xbf //(0x80+0x3f)
#define KEYD_F6         0xc0 //(0x80+0x40)
#define KEYD_F7         0xc1 //(0x80+0x41)
#define KEYD_F8         0xc2 //(0x80+0x42)
#define KEYD_F9         0xc3 //(0x80+0x43)
#define KEYD_F10        0xc4 //(0x80+0x44)

#define KEYD_NUMLOCK    0xc5                 // killough 3/6/98
#define KEYD_SCROLLLOCK 0xc6
#define KEYD_HOME       0xc7
#define KEYD_PAGEUP     0xc9
#define KEYD_END        0xcf
#define KEYD_PAGEDOWN   0xd1
#define KEYD_INSERT     0xd2

#define KEYD_F11        0xd7 //(0x80+0x57)
#define KEYD_F12        0xd8 //(0x80+0x58)

// haleyjd: virtual keys
#define KEYD_MOUSE1     0xe0 //(0x80 + 0x60)
#define KEYD_MOUSE2     0xe1 //(0x80 + 0x61)
#define KEYD_MOUSE3     0xe2 //(0x80 + 0x62)
#define KEYD_JOY1       0xe3 //(0x80 + 0x63)
#define KEYD_JOY2       0xe4 //(0x80 + 0x64)
#define KEYD_JOY3       0xe5 //(0x80 + 0x65)
#define KEYD_JOY4       0xe6 //(0x80 + 0x66)
#define KEYD_JOY5       0xe7 //(0x80 + 0x67)
#define KEYD_JOY6       0xe8 //(0x80 + 0x68)
#define KEYD_JOY7       0xe9 //(0x80 + 0x69)
#define KEYD_JOY8       0xea //(0x80 + 0x6a)
#define KEYD_MWHEELUP   0xeb //(0x80 + 0x6b)
#define KEYD_MWHEELDOWN 0xec //(0x80 + 0x6c)

// haleyjd: key pad
#define KEYD_KP0        0xed
#define KEYD_KP1        0xee
#define KEYD_KP2        0xef
#define KEYD_KP3        0xf0
#define KEYD_KP4        0xf1
#define KEYD_KP5        0xf2
#define KEYD_KP6        0xf3
#define KEYD_KP7        0xf4
#define KEYD_KP8        0xf5
#define KEYD_KP9        0xf6
#define KEYD_KPPERIOD   0xf7
#define KEYD_KPDIVIDE   0xf8
#define KEYD_KPMULTIPLY 0xf9
#define KEYD_KPMINUS    0xfa
#define KEYD_KPPLUS     0xfb
#define KEYD_KPENTER    0xfc
#define KEYD_KPEQUALS   0xfd

// haleyjd: remap delete here
#define KEYD_DEL        0xfe

#define KEYD_PAUSE      0xff

// phares 3/2/98:

// phares 3/2/98

// sf: console key
#define KEYD_ACCGRAVE   '`'

// phares 4/19/98:
// Defines Setup Screen groups that config variables appear in.
// Used when resetting the defaults for every item in a Setup group.

typedef enum {
  ss_none,
  ss_keys,
  ss_weap,
  ss_stat,
  ss_auto,
  ss_enem,
  ss_mess,
  ss_chat,
  ss_gen,       // killough 10/98
  ss_comp,      // killough 10/98
  ss_max
} ss_types;

// phares 3/20/98:
//
// Player friction is variable, based on controlling
// linedefs. More friction can create mud, sludge,
// magnetized floors, etc. Less friction can create ice.

#define MORE_FRICTION_MOMENTUM 15000       // mud factor based on momentum
#define ORIG_FRICTION          0xE800      // original value
#define ORIG_FRICTION_FACTOR   2048        // original value

// sf: some useful macros
//
// isExMy  }
//         } for easy testing of level name format
// isMAPxy }
//

#define isnumchar(c) ( (c) >= '0' && (c) <= '9')
#define isExMy(s) ( (s)[0] == 'E' && (s)[2] == 'M'      \
                && isnumchar((s)[1]) && isnumchar((s)[3]) \
                        && !(s)[4])
#define isMAPxy(s) ( (s)[0] == 'M' && (s)[1] == 'A' && (s)[2] == 'P'   \
                && isnumchar((s)[3]) && isnumchar((s)[4]) && !(s)[5] )  

#define HTIC_GHOST_TRANS 26624

#endif          // __DOOMDEF__

//----------------------------------------------------------------------------
//
// $Log: doomdef.h,v $
// Revision 1.23  1998/05/14  08:02:00  phares
// Added Player Starts 5-8 (4001-4004)
//
// Revision 1.22  1998/05/05  15:34:48  phares
// Documentation and Reformatting changes
//
// Revision 1.21  1998/05/03  22:39:56  killough
// beautification
//
// Revision 1.20  1998/04/27  01:50:51  killough
// Make gcc's __attribute__ mean nothing on other compilers
//
// Revision 1.19  1998/04/22  13:45:23  phares
// Added Setup screen Reset to Defaults
//
// Revision 1.18  1998/03/24  15:59:13  jim
// Added default_skill parameter to config file
//
// Revision 1.17  1998/03/23  15:23:34  phares
// Changed pushers to linedef control
//
// Revision 1.16  1998/03/20  00:29:34  phares
// Changed friction to linedef control
//
// Revision 1.15  1998/03/12  14:28:36  phares
// friction and IDCLIP changes
//
// Revision 1.14  1998/03/09  18:27:16  phares
// Fixed bug in neighboring variable friction sectors
//
// Revision 1.13  1998/03/09  07:08:30  killough
// Add numlock key scancode
//
// Revision 1.12  1998/03/04  21:26:27  phares
// Repaired syntax error (left-over conflict marker)
//
// Revision 1.11  1998/03/04  21:02:16  phares
// Dynamic HELP screen
//
// Revision 1.10  1998/03/02  11:25:52  killough
// Remove now-dead monster_ai mask idea
//
// Revision 1.9  1998/02/24  08:45:32  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.8  1998/02/23  04:15:50  killough
// New monster AI option mask enums
//
// Revision 1.7  1998/02/15  02:48:06  phares
// User-defined keys
//
// Revision 1.6  1998/02/09  02:52:01  killough
// Make SCREENWIDTH/HEIGHT more flexible
//
// Revision 1.5  1998/02/02  13:22:47  killough
// user new version files
//
// Revision 1.4  1998/01/30  18:48:07  phares
// Changed textspeed and textwait to functions
//
// Revision 1.3  1998/01/30  16:09:06  phares
// Faster end-mission text display
//
// Revision 1.2  1998/01/26  19:26:39  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:51  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
