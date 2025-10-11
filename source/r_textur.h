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
// Purpose: New texture system header.
// Authors: James Haley, Derek MacDonald
//

#ifndef R_TEXTUR_H__
#define R_TEXTUR_H__

struct texture_t;

// Needed by GameModeInfo
void R_DoomTextureHacks(texture_t *);
void R_Doom2TextureHacks(texture_t *);
void R_HticTextureHacks(texture_t *);

#endif

// EOF

