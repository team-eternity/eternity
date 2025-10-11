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
// Purpose: Internally used data structures for virtually everything,
//  key definitions, lots of other stuff.
//
// Authors: James Haley, Stephen McGranahan, Ioan Chera, Max Waine
//

#ifndef DOOMDEF_H__
#define DOOMDEF_H__

// killough 4/25/98: Make gcc extensions mean nothing on other compilers
// haleyjd 05/22/02: moved to d_keywds.h

// This must come first, since it redefines malloc(), free(), etc. -- killough:
// haleyjd: uhh, no. Include it in the files, not in another header.

// Identify language to use, software localization.
enum Language_t
{
    english,
    french,
    german,
    unknown
};

//
// For resize of screen, at start of game.
//

static constexpr int BASE_WIDTH = 320;

// It is educational but futile to change this
//  scaling e.g. to 2. Drawing of status bar,
//  menues etc. is tied to the scale implied
//  by the graphics.

static constexpr int    SCREEN_MUL       = 1;
static constexpr double INV_ASPECT_RATIO = 0.625; /* 0.75, ideally */

// haleyjd 02/17/14: MAX_SCREENWIDTH/HEIGHT are now only a limit on the highest
// resolution that Eternity will allow to attempt to be set. They are no longer
// used to size any arrays in the renderer.

// SoM 2-4-04: ANYRES
static constexpr int MAX_SCREENWIDTH  = 32767;
static constexpr int MAX_SCREENHEIGHT = 32767;

static constexpr int SCREENWIDTH  = 320;
static constexpr int SCREENHEIGHT = 200;

// The maximum number of players, multiplayer/networking.
static constexpr int MAXPLAYERS = 4;

// phares 5/14/98:
// DOOM Editor Numbers (aka doomednum in Mobj)

static constexpr int DEN_PLAYER5 = 4001;
static constexpr int DEN_PLAYER6 = 4002;
static constexpr int DEN_PLAYER7 = 4003;
static constexpr int DEN_PLAYER8 = 4004;

// State updates, number of tics / second.
static constexpr int TICRATE = 35;

// The current state of the game: whether we are playing, gazing
// at the intermission screen, the game final animation, or a demo.

enum gamestate_t
{
    GS_NOSTATE = -1, // haleyjd: for C++ conversion, initial value of oldgamestate
    GS_LEVEL,
    GS_INTERMISSION,
    GS_FINALE,
    GS_DEMOSCREEN,
    GS_CONSOLE,
    GS_STARTUP, // haleyjd: value given to gamestate during startup to avoid thinking it's GS_LEVEL
    GS_LOADING  // haleyjd: value given to gamestate during level load
};

//
// Difficulty/skill settings/filters.
//
// These are Thing flags

enum
{
    MTF_EASY   = 0x000000001, // Easy skill flag.
    MTF_NORMAL = 0x000000002, // Normal skill flag.
    MTF_HARD   = 0x000000004, // Hard skill flag.
    MTF_AMBUSH = 0x000000008, // Deaf monsters/do not react to sound.

    // killough 11/98
    MTF_NOTSINGLE = 0x000000010,
    MTF_NOTDM     = 0x000000020,
    MTF_NOTCOOP   = 0x000000040,
    MTF_FRIEND    = 0x000000080,
    MTF_RESERVED  = 0x000000100,

    // haleyjd
    MTF_DORMANT = 0x000000200,
};

// haleyjd: PSX flags
enum
{
    MTF_PSX_NIGHTMARE = 32 | 128,
    MTF_PSX_SPECTRE   = 32 | 64 | 128,
};

// Strife flags
enum
{
    MTF_STRIFE_STAND       = 0x000000020, // WARNING: this is already in MTF_EX_STAND
    MTF_STRIFE_FRIEND      = 0x000000040,
    MTF_STRIFE_TRANSLUCENT = 0x000000100,
    MTF_STRIFE_MVCIS       = 0x000000200,
};

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

    // Strife standing monster
    MTF_EX_STAND = 4,
};

// a macro to find out whether to make moving sounds in a sector
#define silentmove(s) ((s)->flags & SECF_KILLMOVESOUND)

enum skill_t : int
{
    sk_none = -1, // jff 3/24/98 create unpicked skill setting
    sk_baby = 0,
    sk_easy,
    sk_medium,
    sk_hard,
    sk_nightmare
};

//
// Key cards.
//

enum card_t
{
    it_bluecard,
    it_yellowcard,
    it_redcard,
    it_blueskull,
    it_yellowskull,
    it_redskull,
    NUMCARDS
};

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
    wp_nochange // No pending weapon change.
};
using weapontype_t = int;

// Ammunition types defined.
enum
{
    am_clip,  // Pistol / chaingun ammo.
    am_shell, // Shotgun / double barreled shotgun.
    am_cell,  // Plasma rifle, BFG.
    am_misl,  // Missile launcher.

    NUMAMMO,
    am_noammo // Unlimited for chainsaw / fist.
};
using ammotype_t = int;

// Power up artifacts.
enum powertype_t
{
    pw_invulnerability,
    pw_strength,
    pw_invisibility,
    pw_ironfeet,
    pw_allmap,
    pw_infrared,
    pw_totalinvis,   // haleyjd: total invisibility
    pw_ghost,        // haleyjd: heretic ghost
    pw_silencer,     // haleyjd: silencer
    pw_flight,       // haleyjd: flight
    pw_torch,        // haleyjd: infrared w/flicker
    pw_weaponlevel2, //  MaxW: powered-up weapons (tome of power)
    NUMPOWERS
};

// Power up durations (how many seconds till expiration).
enum powerduration_e
{
    INVULNTICS = (30 * TICRATE),
    INVISTICS  = (60 * TICRATE),
    INFRATICS  = (120 * TICRATE),
    IRONTICS   = (60 * TICRATE),
    FLIGHTTICS = (60 * TICRATE), // flight tics, for Heretic
};

// DOOM keyboard definition.
// This is the stuff configured by Setup.Exe.
// Most key data are simple ascii (uppercased).

enum keycode_e
{
    KEYD_TAB    = 0x09,
    KEYD_ENTER  = 0x0d,
    KEYD_ESCAPE = 0x1b,

    KEYD_SPACEBAR = 0x20,

    KEYD_COMMA = 0x2c,
    KEYD_MINUS,
    KEYD_PERIOD,

    KEYD_EQUALS = 0x3d,

    KEYD_ACCGRAVE = 0x60,

    KEYD_BACKSPACE = 0x7f,

    // FIXME: The values for these two might need adjusting
    KEYD_NONUSBACKSLASH = 0x80,
    KEYD_NONUSHASH,

    KEYD_RCTRL = 0x9d,

    KEYD_LEFTARROW = 0xac,
    KEYD_UPARROW,
    KEYD_RIGHTARROW,
    KEYD_DOWNARROW,

    KEYD_RSHIFT = 0xb6,
    KEYD_RALT   = 0xb8,
    KEYD_LALT   = KEYD_RALT,

    KEYD_CAPSLOCK = 0xba, // phares

    KEYD_F1 = 0xbb,
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
    KEYD_END      = 0xcf,
    KEYD_PAGEDOWN = 0xd1,
    KEYD_INSERT   = 0xd2,

    KEYD_F11 = 0xd7,
    KEYD_F12,

    // haleyjd: virtual keys for mouse
    KEYD_MOUSE1 = 0xe0,
    KEYD_MOUSE2,
    KEYD_MOUSE3,
    KEYD_MOUSE4,
    KEYD_MOUSE5,
    KEYD_MOUSE6,
    KEYD_MOUSE7,
    KEYD_MOUSE8,
    KEYD_MWHEELUP,
    KEYD_MWHEELDOWN,

    KEYD_KP0 = 0xed,
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
    KEYD_JOY_BASE = 0x100,
    KEYD_JOY_A    = KEYD_JOY_BASE,
    KEYD_JOY_B,
    KEYD_JOY_X,
    KEYD_JOY_Y,
    KEYD_JOY_BACK,
    KEYD_JOY_GUIDE,
    KEYD_JOY_START,
    KEYD_JOY_STICK_LEFT,
    KEYD_JOY_STICK_RIGHT,
    KEYD_JOY_SHOULDER_LEFT,
    KEYD_JOY_SHOULDER_RIGHT,
    KEYD_JOY_DPAD_UP,
    KEYD_JOY_DPAD_DOWN,
    KEYD_JOY_DPAD_LEFT,
    KEYD_JOY_DPAD_RIGHT,
    KEYD_JOY_MISC1, // Xbox Series X share, PS5 microphone, Switch Pro capture button
    KEYD_JOY_MISC2,
    KEYD_JOY_MISC3,
    KEYD_JOY_MISC4,
    KEYD_JOY_MISC5,
    KEYD_JOY_TOUCHPAD,
    KEYD_JOY_21,
    KEYD_JOY_22,
    KEYD_JOY_23,

    // axis activation events
    KEYD_AXIS_BASE,
    KEYD_AXIS_LEFT_X = KEYD_AXIS_BASE,
    KEYD_AXIS_LEFT_Y,
    KEYD_AXIS_RIGHT_X,
    KEYD_AXIS_RIGHT_Y,
    KEYD_AXIS_TRIGGER_LEFT,
    KEYD_AXIS_TRIGGER_RIGHT,
    KEYD_AXISON07,
    KEYD_AXISON08,

    NUMKEYS
};

// phares 4/19/98:
// Defines Setup Screen groups that config variables appear in.
// Used when resetting the defaults for every item in a Setup group.

enum ss_types
{
    ss_none,
    ss_keys,
    ss_weap,
    ss_stat,
    ss_auto,
    ss_enem,
    ss_mess,
    ss_chat,
    ss_gen,  // killough 10/98
    ss_comp, // killough 10/98
    ss_max
};

// phares 3/20/98:
//
// Player friction is variable, based on controlling
// linedefs. More friction can create mud, sludge,
// magnetized floors, etc. Less friction can create ice.

static constexpr int MORE_FRICTION_MOMENTUM = 15000;  // mud factor based on momentum
static constexpr int ORIG_FRICTION          = 0xE800; // original value
static constexpr int ORIG_FRICTION_FACTOR   = 2048;   // original value
static constexpr int FRICTION_FLY           = 0xEB00; // haleyjd: Raven flying player

// haleyjd 06/05/12: flying
static constexpr int FLIGHT_CENTER      = -8;
static constexpr int FLIGHT_IMPULSE_AMT = 5;

// sf: some useful macros
//
// isExMy  }
//         } for easy testing of level name format
// isMAPxy }
//

inline constexpr bool isnumchar(const char c)
{
    return c >= '0' && c <= '9';
}

static constexpr int HTIC_GHOST_TRANS = 26624;

#endif // __DOOMDEF__

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
