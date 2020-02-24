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
//  Internally used data structures for virtually everything,
//   key definitions, lots of other stuff.
//
//-----------------------------------------------------------------------------

#ifndef DOOMDEF_H__
#define DOOMDEF_H__

// killough 4/25/98: Make gcc extensions mean nothing on other compilers
// haleyjd 05/22/02: moved to d_keywds.h

// This must come first, since it redefines malloc(), free(), etc. -- killough:
// haleyjd: uhh, no. Include it in the files, not in another header.

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

// haleyjd 02/17/14: MAX_SCREENWIDTH/HEIGHT are now only a limit on the highest
// resolution that Eternity will allow to attempt to be set. They are no longer
// used to size any arrays in the renderer.

// SoM 2-4-04: ANYRES
#define MAX_SCREENWIDTH  32767
#define MAX_SCREENHEIGHT 32767

#define SCREENWIDTH      320
#define SCREENHEIGHT     200

// The maximum number of players, multiplayer/networking.
#define MAXPLAYERS       4

// phares 5/14/98:
// DOOM Editor Numbers (aka doomednum in Mobj)

#define DEN_PLAYER5 4001
#define DEN_PLAYER6 4002
#define DEN_PLAYER7 4003
#define DEN_PLAYER8 4004

// State updates, number of tics / second.
#define TICRATE          35

// The current state of the game: whether we are playing, gazing
// at the intermission screen, the game final animation, or a demo.

typedef enum {
  GS_NOSTATE = -1, // haleyjd: for C++ conversion, initial value of oldgamestate
  GS_LEVEL,
  GS_INTERMISSION,
  GS_FINALE,
  GS_DEMOSCREEN,
  GS_CONSOLE,
  GS_STARTUP, // haleyjd: value given to gamestate during startup to avoid thinking it's GS_LEVEL
  GS_LOADING  // haleyjd: value given to gamestate during level load
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

// haleyjd: PSX flags
#define MTF_PSX_NIGHTMARE (32|128)
#define MTF_PSX_SPECTRE   (32|64|128)

// Strife flags
#define MTF_STRIFE_STAND       32
#define MTF_STRIFE_FRIEND      64
#define MTF_STRIFE_TRANSLUCENT 256
#define MTF_STRIFE_MVCIS       512

//
// ioanch 20151218: mapthing_t::extOptions (extended) flags: needed by UDMF
//
enum
{
   // will appear on skill 1 if MTF_EASY is not set
   // will not appear on skill 1 if MTF_EASY is set
   MTF_EX_BABY_TOGGLE = 1,

   // will appear on skill 5 if MTF_HARD is not set
   // will not appear on skill 5 if MTF_HARD is set
   MTF_EX_NIGHTMARE_TOGGLE = 2,
};

// sf: sector flags, not me =)
                // kill all sound in sector
#define SF_KILLSOUND          1024
                // kill all sounds due to moving
#define SF_KILLMOVESOUND      2048

// a macro to find out whether to make moving sounds in a sector
#define silentmove(s) ((s)->flags & SECF_KILLMOVESOUND)

enum skill_t : int
{
  sk_none=-1, //jff 3/24/98 create unpicked skill setting
  sk_baby=0,
  sk_easy,
  sk_medium,
  sk_hard,
  sk_nightmare
};

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
enum 
{
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
};

typedef int weapontype_t;

// Ammunition types defined.
enum 
{
  am_clip,    // Pistol / chaingun ammo.
  am_shell,   // Shotgun / double barreled shotgun.
  am_cell,    // Plasma rifle, BFG.
  am_misl,    // Missile launcher.
  
  NUMAMMO,
  am_noammo   // Unlimited for chainsaw / fist.
};
typedef int ammotype_t;

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
  pw_flight,      // haleyjd: flight
  pw_torch,       // haleyjd: infrared w/flicker
  pw_weaponlevel2, //  MaxW: powered-up weapons (tome of power)
  NUMPOWERS
} powertype_t;

// Power up durations (how many seconds till expiration).
typedef enum {
  INVULNTICS  = (30*TICRATE),
  INVISTICS   = (60*TICRATE),
  INFRATICS   = (120*TICRATE),
  IRONTICS    = (60*TICRATE),
  FLIGHTTICS  = (60*TICRATE),  // flight tics, for Heretic
} powerduration_t;

// DOOM keyboard definition.
// This is the stuff configured by Setup.Exe.
// Most key data are simple ascii (uppercased).

enum keycode_e
{
   KEYD_TAB            = 0x09,
   KEYD_ENTER          = 0x0d,
   KEYD_ESCAPE         = 0x1b,

   KEYD_SPACEBAR       = 0x20,

   KEYD_COMMA          = 0x2c,
   KEYD_MINUS,
   KEYD_PERIOD,

   KEYD_EQUALS         = 0x3d,
   
   KEYD_ACCGRAVE       = 0x60,

   KEYD_BACKSPACE      = 0x7f,

   // FIXME: The values for these two might need adjusting
   KEYD_NONUSBACKSLASH = 0x80,
   KEYD_NONUSHASH,

   KEYD_RCTRL          = 0x9d,

   KEYD_LEFTARROW      = 0xac,
   KEYD_UPARROW,
   KEYD_RIGHTARROW,
   KEYD_DOWNARROW,

   KEYD_RSHIFT         = 0xb6,
   KEYD_RALT           = 0xb8,
   KEYD_LALT           = KEYD_RALT,

   KEYD_CAPSLOCK       = 0xba, // phares 

   KEYD_F1             = 0xbb,
   KEYD_F2,
   KEYD_F3,
   KEYD_F4,
   KEYD_F5,
   KEYD_F6,
   KEYD_F7,
   KEYD_F8,
   KEYD_F9,
   KEYD_F10,
   KEYD_PRINTSCREEN,
   KEYD_NUMLOCK,
   KEYD_SCROLLLOCK,
   KEYD_HOME,   
   KEYD_PAGEUP,
   KEYD_END            = 0xcf,
   KEYD_PAGEDOWN       = 0xd1,
   KEYD_INSERT         = 0xd2,

   KEYD_F11            = 0xd7,
   KEYD_F12,

   // haleyjd: virtual keys for mouse
   KEYD_MOUSE1         = 0xe0,
   KEYD_MOUSE2,
   KEYD_MOUSE3,
   KEYD_MOUSE4,
   KEYD_MOUSE5,
   KEYD_MOUSE6,
   KEYD_MOUSE7,
   KEYD_MOUSE8,
   KEYD_MWHEELUP,
   KEYD_MWHEELDOWN,

   KEYD_KP0            = 0xed,
   KEYD_KP1,
   KEYD_KP2,
   KEYD_KP3,
   KEYD_KP4,
   KEYD_KP5,
   KEYD_KP6,
   KEYD_KP7,
   KEYD_KP8,
   KEYD_KP9,
   KEYD_KPPERIOD,
   KEYD_KPDIVIDE,
   KEYD_KPMULTIPLY,
   KEYD_KPMINUS,
   KEYD_KPPLUS,
   KEYD_KPENTER,
   KEYD_KPEQUALS,
   KEYD_DEL,
   KEYD_PAUSE, // 0xff

   // virtual key codes for gamepad buttons
   KEYD_JOY01          = 0x100,
   KEYD_JOY02,
   KEYD_JOY03,
   KEYD_JOY04,
   KEYD_JOY05,
   KEYD_JOY06,
   KEYD_JOY07,
   KEYD_JOY08,
   KEYD_JOY09,
   KEYD_JOY10,
   KEYD_JOY11,
   KEYD_JOY12,
   KEYD_JOY13,
   KEYD_JOY14,
   KEYD_JOY15,
   KEYD_JOY16,

   // joystick hats
   KEYD_HAT1RIGHT,
   KEYD_HAT1UP,
   KEYD_HAT1LEFT,
   KEYD_HAT1DOWN,
   KEYD_HAT2RIGHT,
   KEYD_HAT2UP,
   KEYD_HAT2LEFT,
   KEYD_HAT2DOWN,
   KEYD_HAT3RIGHT,
   KEYD_HAT3UP,
   KEYD_HAT3LEFT,
   KEYD_HAT3DOWN,
   KEYD_HAT4RIGHT,
   KEYD_HAT4UP,
   KEYD_HAT4LEFT,
   KEYD_HAT4DOWN,

   // axis activation events
   KEYD_AXISON01,
   KEYD_AXISON02,
   KEYD_AXISON03,
   KEYD_AXISON04,
   KEYD_AXISON05,
   KEYD_AXISON06,
   KEYD_AXISON07,
   KEYD_AXISON08,

   NUMKEYS
};

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
#define FRICTION_FLY           0xEB00      // haleyjd: Raven flying player

// haleyjd 06/05/12: flying
#define FLIGHT_CENTER      -8
#define FLIGHT_IMPULSE_AMT  5

// sf: some useful macros
//
// isExMy  }
//         } for easy testing of level name format
// isMAPxy }
//

#define isnumchar(c) ((c) >= '0' && (c) <= '9')
#define isExMy(s)                                       \
   ((s)[0] == 'E' && (s)[2] == 'M' &&                   \
    isnumchar((s)[1]) && isnumchar((s)[3]) && !(s)[4])
#define isMAPxy(s)                                      \
   ((s)[0] == 'M' && (s)[1] == 'A' && (s)[2] == 'P' &&  \
    isnumchar((s)[3]) && isnumchar((s)[4]) && !(s)[5])

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
