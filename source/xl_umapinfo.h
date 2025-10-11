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
//------------------------------------------------------------------------------
//
// Purpose: UMAPINFO lump, invented by Graf Zahl for cross-port support.
// Authors: Ioan Chera
//

#ifndef XL_UMAPINFO_H_
#define XL_UMAPINFO_H_

class MetaTable;

enum
{
    // Use the minimum extreme values for special meanings
    XL_UMAPINFO_SPECVAL_CLEAR = INT_MIN,
    XL_UMAPINFO_SPECVAL_FALSE,
    XL_UMAPINFO_SPECVAL_TRUE,
    XL_UMAPINFO_SPECVAL_NOT_SET = INT_MAX // this is the default when querying the meta table.
};

MetaTable *XL_UMapInfoForMapName(const char *mapname);
void       XL_ParseUMapInfo();
void       XL_BuildInterUMapInfo();
void       XL_BuildUMapInfoEpisodes();

#endif

// EOF

