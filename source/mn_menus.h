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
// Purpose: Menus.
// Authors: James Haley, Ioan Chera, Max Waine
//

#ifndef MN_MENUS_H__
#define MN_MENUS_H__

void MN_InitMenus();
void MN_LinkClassicMenus(int link);

void MN_UpdateGamepadMenus();

void MN_DoomNewGame();
void MN_Doom2NewGame();

extern menu_t menu_main;
extern menu_t menu_main_doom2;
extern menu_t menu_newgame;

extern int mn_classic_menus;

// menu aspect ratio settings enum
enum
{
   AR_LEGACY,
   AR_5TO4,
   AR_4TO3,
   AR_3TO2,
   AR_16TO10,
   AR_5TO3,
   AR_WSVGA,  // 128:75 (1024x600 mode)
   AR_16TO9,
   AR_21TO9,
   AR_NUMASPECTRATIOS
};

// menu screen types
enum
{
   MN_WINDOWED,
   MN_FULLSCREEN_DESKTOP,
   MN_FULLSCREEN,
   MN_NUMSCREENTYPES
};

extern int mn_favaspectratio;
extern int mn_favscreentype;

#endif

// EOF

