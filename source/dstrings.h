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
// Purpose: DOOM strings, by language.
// Authors: James Haley
//

#ifndef DSTRINGS_H__
#define DSTRINGS_H__

// All important printed strings.
// Language selection (message strings).
// Use -DFRENCH etc.

#ifdef FRENCH
#include "d_french.h"
#else
#include "d_englsh.h"
#endif

// Misc. other strings.
constexpr const char SAVEGAMENAME[] = "doomsav";

//
// File locations,
//  relative to current position.
// Path names are OS-sensitive.
//
constexpr const char DEVMAPS[] = "devmaps";
constexpr const char DEVDATA[] = "devdata";

// Not done in french?

// QuitDOOM messages
static constexpr int NUM_QUITMESSAGES = 22;

extern const char *endmsg[];

#ifndef BOOM_KEY_STRINGS // some files don't have boom-specific things

constexpr const char PD_BLUEC[]      = PD_BLUEK;
constexpr const char PD_REDC[]       = PD_REDK;
constexpr const char PD_YELLOWC[]    = PD_YELLOWK;
constexpr const char PD_BLUES[]      = "You need a blue skull to open this door";
constexpr const char PD_REDS[]       = "You need a red skull to open this door";
constexpr const char PD_YELLOWS[]    = "You need a yellow skull to open this door";
constexpr const char PD_ANY[]        = "Any key will open this door";
constexpr const char PD_ALL3[]       = "You need all three keys to open this door";
constexpr const char PD_ALL6[]       = "You need all six keys to open this door";
constexpr const char STSTR_COMPON[]  = "Compatibility Mode On";  // phares
constexpr const char STSTR_COMPOFF[] = "Compatibility Mode Off"; // phares

#endif

// obituaries
constexpr const char OB_SUICIDE[]       = "suicides";
constexpr const char OB_FALLING[]       = "cratered";
constexpr const char OB_CRUSH[]         = "was squished";
constexpr const char OB_LAVA[]          = "burned to death";
constexpr const char OB_SLIME[]         = "melted";
constexpr const char OB_BARREL[]        = "was blown up";
constexpr const char OB_SPLASH[]        = "died in an explosion";
constexpr const char OB_COOP[]          = "scored a frag for the other team";
constexpr const char OB_DEFAULT[]       = "died";
constexpr const char OB_R_SPLASH_SELF[] = "blew up";
constexpr const char OB_GRENADE_SELF[]  = "lost track of a grenade";
constexpr const char OB_ROCKET_SELF[]   = "should have stood back";
constexpr const char OB_BFG11K_SELF[]   = "used a BFG11k up close";
constexpr const char OB_FIST[]          = "was punched to death";
constexpr const char OB_CHAINSAW[]      = "was mistaken for a tree";
constexpr const char OB_PISTOL[]        = "took a slug to the head";
constexpr const char OB_SHOTGUN[]       = "couldn't duck the buckshot";
constexpr const char OB_SSHOTGUN[]      = "was shown some double barrels";
constexpr const char OB_CHAINGUN[]      = "did the chaingun cha-cha";
constexpr const char OB_ROCKET[]        = "was blown apart by a rocket";
constexpr const char OB_R_SPLASH[]      = "was splattered by a rocket";
constexpr const char OB_PLASMA[]        = "admires the pretty blue stuff";
constexpr const char OB_BFG[]           = "was utterly destroyed by the BFG";
constexpr const char OB_BFG_SPLASH[]    = "saw the green flash";
constexpr const char OB_BETABFG[]       = "got a blast from the past";
constexpr const char OB_BFGBURST[]      = "was caught in a plasma storm";
constexpr const char OB_GRENADE[]       = "tripped on a grenade";
constexpr const char OB_TELEFRAG[]      = "turned inside out";
constexpr const char OB_QUAKE[]         = "got a bad case of the shakes";

// misc new strings
constexpr const char SECRETMESSAGE[] = "You found a secret!";

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
