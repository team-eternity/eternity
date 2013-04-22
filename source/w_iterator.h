// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2012 James Haley
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
//      Wad Directory Iterators
//
//-----------------------------------------------------------------------------

#ifndef W_ITERATOR_H__
#define W_ITERATOR_H__

#include "w_wad.h"

class WadIterator
{
protected:
   lumpinfo_t *lump;

public:
   virtual void begin(const WadDirectory &dir, int info) = 0;
   lumpinfo_t *current() const { return lump; }
   virtual void next() = 0;
};

class WadNamespaceIterator : public WadIterator
{
protected:
   lumpinfo_t **lumpinfo;
   WadDirectory::namespace_t ns;

public:
   virtual void begin(const WadDirectory &dir, int info)
   {
      ns       = dir.getNamespace(info);
      lumpinfo = dir.getLumpInfo();
      lump     = (ns.numLumps ? lumpinfo[ns.firstLump] : NULL);
   }

   virtual void next()
   {
      if(lump)
      {
         int next = lump->selfindex + 1;
         lump = ((next < ns.firstLump + ns.numLumps) ? lumpinfo[next] : NULL);
      }
   }
};

#endif

// EOF

