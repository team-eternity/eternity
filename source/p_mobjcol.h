// Emacs style mode select   -*- C++ -*- vi:sw=3 ts=3: 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//  Mobj Collections
//  These are used by stuff like Boss Spawn Spots and D'Sparil landings, and
//  evolved out of a generalization of the code Lee originally wrote to remove
//  the boss spot limit in BOOM. The core logic is now based on the generic
//  PODCollection class, however, and this just adds a bit of utility to it.
//
//-----------------------------------------------------------------------------

#ifndef P_MOBJCOL_H__
#define P_MOBJCOL_H__

#include "m_collection.h"

class Mobj;

class MobjCollection : public PODCollection<Mobj *>
{
protected:
   int mobjType;

public:
   MobjCollection() : PODCollection<Mobj *>(), mobjType(-1) {}
   int  getMobjType() const { return mobjType; }
   void setMobjType(int mt) { mobjType = mt;   }

   void collectThings();
};

#endif

// EOF

