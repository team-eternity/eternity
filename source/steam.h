//
// ironwail
// Copyright (C) 2022 A. Drexler
//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Max Waine, et al.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
//------------------------------------------------------------------------------
//
// Purpose: Steam config parsing.
// Authors: A. Drexler, Max Waine
//

#ifndef _STEAM_H_
#define _STEAM_H_

#include "d_io.h"
#include "m_qstr.h"

constexpr int DOOM_DOOM_II_STEAM_APPID = 2280;
constexpr int FINAL_DOOM_STEAM_APPID   = 2290;
constexpr int DOOM2_STEAM_APPID        = 2300;
constexpr int DOOM3_BFG_STEAM_APPID    = 208200;

constexpr int MASTER_LEVELS_STEAM_APPID = 9160;

constexpr int HEXEN_STEAM_APPID   = 2360;
constexpr int HEXDD_STEAM_APPID   = 2370;
constexpr int SOSR_STEAM_APPID    = 2390;

constexpr int SVE_STEAM_APPID = 317040;

constexpr int STEAM_APPIDS[] =
{
   DOOM_DOOM_II_STEAM_APPID,
   FINAL_DOOM_STEAM_APPID,
   DOOM2_STEAM_APPID,
   DOOM3_BFG_STEAM_APPID,
   MASTER_LEVELS_STEAM_APPID,

   HEXEN_STEAM_APPID,
   HEXDD_STEAM_APPID,
   SOSR_STEAM_APPID,

   SVE_STEAM_APPID,
};

struct steamgame_t
{
   int   appid;
   char *subdir;
   char  library[PATH_MAX];
};

bool Steam_GetDir(qstring &dirout);
bool Steam_FindGame(steamgame_t *game, int appid);
bool Steam_ResolvePath(qstring &path, const steamgame_t *game);

#endif // defined(_STEAM_H)
