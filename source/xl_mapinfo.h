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

#include "m_dllist.h"
#include "m_qstr.h"
#include "z_zone.h"

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

   XL_NUMMAPINFO_FIELDS
};

//
// XLMapInfo
//
// Represents a Hexen MAPINFO entry. Data here will populate Eternity's global
// LevelInfo structure when there is no EMAPINFO for the same map. Unlike Hexen
// we will parse all MAPINFO lumps, but the newest definition for a map is the
// one that wins. Definitions are cumulative between lumps, but not additive 
// to each other when they apply to the same map.
//
class XLMapInfo : public ZoneObject
{
public:
   qstring map;
   qstring name;
   qstring sky1;
   qstring sky2;
   int     sky1delta;
   int     sky2delta;
   bool    doublesky;
   bool    lightning;
   qstring fadetable;
   int     cluster;
   int     warptrans;
   qstring next;
   int     cdtrack;
   qstring secretnext;
   qstring titlepatch;
   int     par;
   qstring music;
   bool    nointermission;
   bool    evenlighting;
   bool    noautosequences;

   bool    setfields[XL_NUMMAPINFO_FIELDS];

   DLListItem<XLMapInfo> links; // hash links
};

XLMapInfo *XL_MapInfoForMapName(const char *name);
XLMapInfo *XL_MapInfoForMapNum(int episode, int map);
void       XL_ParseMapInfo();

#endif

// EOF
