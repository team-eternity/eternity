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
//      Put all global state variables here.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "doomstat.h"

// Language.
Language_t   language = english;

// Set if homebrew PWAD stuff has been added.
bool modifiedgame;

// haleyjd 03/05/09: support using same config for all DOOM game modes
int use_doom_config = false;

//-----------------------------------------------------------------------------

//
// Special factories
//
gameplayopts_t gameplayopts_t::InitialCurrent()
{
   edefstructvar(gameplayopts_t, opts);
   opts.allow_pushers = 1; // PUSH Things              // phares 3/10/98
   opts.autoaim = 1; //sf
   opts.distfriend = 128;
   opts.monster_friction = 1;
   opts.monster_infighting = 1;
   opts.monsters_remember = 1;
   opts.variable_friction = 1;
   return opts;
}
gameplayopts_t gameplayopts_t::InitialDefault()
{
   edefstructvar(gameplayopts_t, opts);
   opts.autoaim = 1;
   opts.distfriend = 128;
   opts.monster_friction = 1;
   opts.monster_infighting = 1;
   opts.monsters_remember = 1;
   return opts;
}

gameplayopts_t g_opts = gameplayopts_t::InitialCurrent();
gameplayopts_t g_default_opts = gameplayopts_t::InitialDefault();

bool in_textmode = true;        // game in graphics mode yet?

// compatibility with old engines (monster behavior, metrics, etc.)
int compatibility, default_compatibility;          // killough 1/31/98
bool vanilla_mode;   // ioanch: store -vanilla in a global flag

int demo_version;           // killough 7/19/98: Boom version of demo
int demo_subversion;        // haleyjd 06/17/01: subversion for betas

// v1.1-like pitched sounds
int pitched_sounds;  // killough 10/98

int general_translucency;    // killough 10/98

bool deh_species_infighting;  // Dehacked setting: from Chocolate-Doom

        // sf: removed beta_emulation

int flashing_hom;     // killough 10/98

int weapon_hotkey_cycling; // killough 10/98

bool cinema_pause = false; // haleyjd 08/22/01

int drawparticles;  // haleyjd 09/28/01
int bloodsplat_particle;
int bulletpuff_particle;
int drawrockettrails;
int drawgrenadetrails;
int drawbfgcloud;

int forceFlipPan; // haleyjd 12/08/01

// haleyjd 9/22/99
int HelperThing = -1; // in P_SpawnMapThing to substitute helper thing

// haleyjd 04/10/03: unified game type variable
gametype_t GameType = gt_single, DefaultGameType = gt_single;

// haleyjd 05/20/04: option to wait at end of game
#ifdef _SDL_VER
int waitAtExit = 0;
#endif

// haleyjd 01/24/07: spawn <!>'s for missing objects?
int p_markunknowns;

//----------------------------------------------------------------------------
//
// $Log: doomstat.c,v $
// Revision 1.5  1998/05/12  12:46:12  phares
// Removed OVER_UNDER code
//
// Revision 1.4  1998/05/05  16:29:01  phares
// Removed RECOIL and OPT_BOBBING defines
//
// Revision 1.3  1998/05/03  23:12:13  killough
// beautify, move most global switch variables here
//
// Revision 1.2  1998/01/26  19:23:10  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:06  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
