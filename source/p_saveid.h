//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Ioan Chera, et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Archiving named objects by their name and not index,
//  for forward compatibility.
// Authors: Ioan Chera
//

#ifndef P_SAVEID_H_
#define P_SAVEID_H_

#include "info.h"

class SaveArchive;

// NOTE: all functions here begin with Archive_ instead of P_, since they're not really influential
// on playsim.

void Archive_Colormap(SaveArchive &arc, int &colormap);
void Archive_ColorTranslation(SaveArchive &arc, int &colour);
void Archive_Flat(SaveArchive &arc, int &flat);
void Archive_Flat(SaveArchive &arc, int16_t &flat);
void Archive_MobjState_Save(SaveArchive &arc, const state_t &state);
state_t &Archive_MobjState_Load(SaveArchive &arc);
void Archive_MobjType(SaveArchive &arc, mobjtype_t &type);
void Archive_PlayerClass(SaveArchive &arc, playerclass_t *&pclass);
void Archive_PSpriteState_Save(SaveArchive &arc, const state_t *state);
state_t *Archive_PSpriteState_Load(SaveArchive &arc);
void Archive_Skin(SaveArchive &arc, skin_t *&skin);
void Archive_SpriteNum(SaveArchive &arc, spritenum_t &sprite);
void Archive_Texture(SaveArchive &arc, int &texture);
void Archive_Texture(SaveArchive &arc, int16_t &texture);
void Archive_TranslucencyMap(SaveArchive &arc, int &tranmap);

#endif

// EOF
