//
// Copyright (C) 2022 A. Drexler
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

#ifndef _STEAM_H_
#define _STEAM_H_

#ifdef EE_FEATURE_REGISTRYSCAN

#include "d_io.h"
#include "m_qstr.h"

#define DOOM2_STEAM_APPID 2300

typedef enum {
   STEAM_VERSION_ORIGINAL,
   STEAM_VERSION_REMASTERED,
} steamversion_t;

typedef struct steamgame_s {
   int   appid;
   char *subdir;
   char  library[PATH_MAX];
} steamgame_t;

bool           Steam_GetDir(qstring &dirout);
bool           Steam_FindGame(steamgame_t *game, int appid);
bool           Steam_ResolvePath(qstring &path, const steamgame_t *game);

#endif // defined(EE_FEATURE_REGISTRYSCAN)

#endif // defined(_STEAM_H)
