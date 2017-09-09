//
// The Eternity Engine
// Copyright (C) 2017 James Haley et al.
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
// Purpose: ANIMDEFS lump, from Hexen. Replaces ANIMATED.
// Authors: Ioan Chera
//

#ifndef XL_ANIMDEFS_H_
#define XL_ANIMDEFS_H_

#include "m_collection.h"
#include "m_qstr.h"

//
// Animation type (for a definition)
//
enum xlanimtype_t
{
   xlanim_flat,   // flat animation
   xlanim_texture // texture animation
};

//
// Picture definition
//
struct xlpicdef_t
{
   int offset;          // the name of the picture. Will be looked up later.
   bool isRandom;       // the branch of the union below
   union
   {
      int tics;         // if not random, exact tics
      struct
      {
         int ticsmin;   // if random, minimum tics
         int ticsmax;   // if random, maximum tics
      };
   };
};

//
// Hexen specific definition
//
struct XLAnimDef : public ZoneObject
{
   xlanimtype_t type;   // flat or texture
   qstring picname;     // name of start pic
   int index;           // start pic index
   int count;           // number of pic defs

   qstring rangename;   // for RANGE entries
   int rangetics;       // for RANGE entries

   XLAnimDef() : type(), index(), count()
   {
   }
};

extern Collection<XLAnimDef> xldefs;
extern PODCollection<xlpicdef_t> xlpics;

void XL_ParseAnimDefs();

#endif

// EOF

