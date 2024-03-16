//----------------------------------------------------------------------------
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//----------------------------------------------------------------------------
//
// EDF Sprites Module
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef E_SPRITE_H__
#define E_SPRITE_H__

#ifdef NEED_EDF_DEFINITIONS

constexpr const char SEC_SPRITE[] = "spritenames";

void    E_ProcessSprites(cfg_t *cfg);

#endif

// For DECORATE states in particular:
bool E_ProcessSingleSprite(const char *sprname);

int E_SpriteNumForName(const char *name);
int E_SpriteNumForDEHNum(const int num);
void E_UpdateSpriteName(const char *oldname, const char *newname, const int newlen);
void E_UpdateAddSpriteNameForNum(const int num, const char* newname, const int newlen, bool forceupdate);

#endif

// EOF

