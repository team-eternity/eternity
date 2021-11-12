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
//   Hexen MAPINFO Parser
//
//-----------------------------------------------------------------------------

#ifndef XL_MAPINFO_H__
#define XL_MAPINFO_H__

class MetaTable;

// map keywords enum
enum
{
   // Standard Hexen keywords
   XL_MAPINFO_SKY1,
   XL_MAPINFO_SKY2,
   XL_MAPINFO_DOUBLESKY,
   XL_MAPINFO_LIGHTNING,
   XL_MAPINFO_FADETABLE,
   XL_MAPINFO_CLUSTER,
   XL_MAPINFO_WARPTRANS,
   XL_MAPINFO_NEXT,
   XL_MAPINFO_CDTRACK,

   // Supported ZDoom extensions
   XL_MAPINFO_SECRETNEXT,
   XL_MAPINFO_TITLEPATCH,
   XL_MAPINFO_PAR,
   XL_MAPINFO_MUSIC,
   XL_MAPINFO_NOINTERMISSION,
   XL_MAPINFO_EVENLIGHTING,
   XL_MAPINFO_NOAUTOSEQUENCES,
   XL_MAPINFO_NOJUMP,
   XL_MAPINFO_NOCROUCH,
   XL_MAPINFO_MAP07SPECIAL,
   XL_MAPINFO_BARONSPECIAL,
   XL_MAPINFO_CYBERDEMONSPECIAL,
   XL_MAPINFO_SPIDERMASTERMINDSPECIAL,
   XL_MAPINFO_SPECIALACTION_LOWERFLOOR,
   XL_MAPINFO_SPECIALACTION_EXITLEVEL,
   XL_MAPINFO_NOSOUNDCLIPPING,
   XL_MAPINFO_SUCKTIME,
   XL_MAPINFO_ALLOWMONSTERTELEFRAGS,

   XL_NUMMAPINFO_FIELDS
};

MetaTable *XL_MapInfoForMapName(const char *name);
MetaTable *XL_MapInfoForMapNum(int episode, int map);
void       XL_ParseMapInfo();

#endif

// EOF
