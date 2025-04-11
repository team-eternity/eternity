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
//----------------------------------------------------------------------------
//
// Automap colours.
//
// Holds console commands (variables) for map colours
//
// By Simon Howard
//
//----------------------------------------------------------------------------

#include "z_zone.h"
#include "doomdef.h"
#include "c_runcmd.h"

extern int mapcolor_back;    // map background
extern int mapcolor_grid;    // grid lines color
extern int mapcolor_wall;    // normal 1s wall color
extern int mapcolor_fchg;    // line at floor height change color
extern int mapcolor_cchg;    // line at ceiling height change color
extern int mapcolor_clsd;    // line at sector with floor=ceiling color
extern int mapcolor_rkey;    // red key color
extern int mapcolor_bkey;    // blue key color
extern int mapcolor_ykey;    // yellow key color
extern int mapcolor_rdor;    // red door color  (diff from keys to allow option)
extern int mapcolor_bdor;    // blue door color (of enabling one but not other )
extern int mapcolor_ydor;    // yellow door color
extern int mapcolor_tele;    // teleporter line color
extern int mapcolor_secr;    // secret sector boundary color
extern int mapcolor_exit;    // jff 4/23/98 add exit line color
extern int mapcolor_unsn;    // computer map unseen line color
extern int mapcolor_flat;    // line with no floor/ceiling changes
extern int mapcolor_sprt;    // general sprite color
extern int mapcolor_hair;    // crosshair color
extern int mapcolor_sngl;    // single player arrow color
extern int mapcolor_plyr[4]; // colors for player arrows in multiplayer
extern int mapcolor_frnd;    // colors for friends of player
extern int mapcolor_prtl;    // SoM: color for lines not in player portal group

// SoM: map mode. True means the portal groups are overlayed (the group the 
// player is in being displayed in color and the other groups being grayed out 
// and underneath) and false means the map is not modified.
extern int mapportal_overlay;

VARIABLE_INT(mapcolor_back, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_grid, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_wall, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_fchg, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_cchg, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_clsd, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_rkey, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_bkey, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_ykey, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_rdor, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_bdor, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_ydor, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_tele, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_secr, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_exit, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_unsn, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_flat, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_sprt, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_hair, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_sngl, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_frnd, nullptr, 0, 255, nullptr);
VARIABLE_INT(mapcolor_prtl, nullptr, 0, 255, nullptr);
VARIABLE_BOOLEAN(mapportal_overlay, nullptr, yesno);

CONSOLE_VARIABLE(mapcolor_back, mapcolor_back, 0) {}
CONSOLE_VARIABLE(mapcolor_grid, mapcolor_grid, 0) {}
CONSOLE_VARIABLE(mapcolor_wall, mapcolor_wall, 0) {}
CONSOLE_VARIABLE(mapcolor_fchg, mapcolor_fchg, 0) {}
CONSOLE_VARIABLE(mapcolor_cchg, mapcolor_cchg, 0) {}
CONSOLE_VARIABLE(mapcolor_clsd, mapcolor_clsd, 0) {}
CONSOLE_VARIABLE(mapcolor_rkey, mapcolor_rkey, 0) {}
CONSOLE_VARIABLE(mapcolor_bkey, mapcolor_bkey, 0) {}
CONSOLE_VARIABLE(mapcolor_ykey, mapcolor_ykey, 0) {}
CONSOLE_VARIABLE(mapcolor_rdor, mapcolor_rdor, 0) {}
CONSOLE_VARIABLE(mapcolor_bdor, mapcolor_bdor, 0) {}
CONSOLE_VARIABLE(mapcolor_ydor, mapcolor_ydor, 0) {}
CONSOLE_VARIABLE(mapcolor_tele, mapcolor_tele, 0) {}
CONSOLE_VARIABLE(mapcolor_secr, mapcolor_secr, 0) {}
CONSOLE_VARIABLE(mapcolor_exit, mapcolor_exit, 0) {}
CONSOLE_VARIABLE(mapcolor_unsn, mapcolor_unsn, 0) {}
CONSOLE_VARIABLE(mapcolor_flat, mapcolor_flat, 0) {}
CONSOLE_VARIABLE(mapcolor_sprt, mapcolor_sprt, 0) {}
CONSOLE_VARIABLE(mapcolor_hair, mapcolor_hair, 0) {}
CONSOLE_VARIABLE(mapcolor_sngl, mapcolor_sngl, 0) {}
CONSOLE_VARIABLE(mapcolor_frnd, mapcolor_frnd, 0) {}
CONSOLE_VARIABLE(mapcolor_prtl, mapcolor_prtl, 0) {}
CONSOLE_VARIABLE(mapportal_overlay, mapportal_overlay, 0) {}

// EOF
