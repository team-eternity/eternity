// Emacs style mode select -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2014 James Haley et al.
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
// Description: 
//   SMMU/Eternity EMAPINFO Parser
//
//-----------------------------------------------------------------------------

#ifndef XL_EMAPINFO_H__
#define XL_EMAPINFO_H__

class MetaTable;
class WadDirectory;

const MetaTable *XL_EMapInfoForMapName(const char *mapname);
MetaTable *XL_EMapInfoForMapNum(int episode, int map);
void       XL_ParseEMapInfo();
MetaTable *XL_ParseLevelInfo(WadDirectory *dir, int lumpnum);

void XL_BuildInterEMapInfo();

#endif

// EOF

