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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Auto-caching of PLAYPAL lump
//
//-----------------------------------------------------------------------------

#ifndef AUTOPALETTE_H__
#define AUTOPALETTE_H__

#include "doomtype.h"
#include "w_wad.h"

class AutoPalette
{
protected:
   byte *palette;

   // Not copyable.
   AutoPalette(const AutoPalette &other) {} 

public:
   explicit AutoPalette(WadDirectory &dir)
   {
      palette = static_cast<byte *>(dir.cacheLumpName("PLAYPAL", PU_STATIC));
   }

   ~AutoPalette()
   {
      Z_ChangeTag(palette, PU_CACHE);
   }

   byte *get() const { return palette; }
   
   byte operator [] (size_t index) const { return palette[index]; }
};

#endif

// EOF

