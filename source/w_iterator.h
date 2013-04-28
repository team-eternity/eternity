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

//
// WadIterator
//
// Abstract base class for wad directory iterators.
// begin/current/next system inspired by KLKENUMX from TrayMenu, by Wei Ke.
//
class WadIterator
{
protected:
   lumpinfo_t *lump;

public:
   WadIterator() : lump(NULL) {}

   lumpinfo_t *current() const { return lump; }
   lumpinfo_t *end()     const { return NULL; }

   // virtuals
   virtual lumpinfo_t *begin() = 0;
   virtual lumpinfo_t *next()  = 0;

   // operators for alternative more STL-like syntax
   lumpinfo_t *operator * () const { return lump; }
   const WadIterator &operator ++ () { next(); return *this; }
};

//
// WadNamespaceIterator
//
// Iterates through all lumps in a particular namespace within the provided
// WadDirectory.
//
class WadNamespaceIterator : public WadIterator
{
protected:
   WadDirectory::namespace_t ns;
   lumpinfo_t **lumpinfo;
   
public:
   // Provide the WadDirectory instance and namespace to iterate.
   WadNamespaceIterator(const WadDirectory &dir, int li_namespace)
      : WadIterator()
   {
      ns       = dir.getNamespace(li_namespace);
      lumpinfo = dir.getLumpInfo();
   }

   // Iteration will begin at the first lump in that namespace. If the
   // namespace is empty, NULL will be returned immediately.
   virtual lumpinfo_t *begin()
   {
      return (lump = (ns.numLumps ? lumpinfo[ns.firstLump] : NULL));
   }

   // Step to the next lump in the namespace, if one exists. Returns NULL once
   // the end of the namespace has been reached.
   virtual lumpinfo_t *next()
   {
      if(lump)
      {
         int next = lump->selfindex + 1;
         lump = ((next < ns.firstLump + ns.numLumps) ? lumpinfo[next] : NULL);
      }
      return lump;
   }

   // Allow access to namespace properties
   int getFirstLump() const { return ns.firstLump; }
   int getNumLumps()  const { return ns.numLumps;  }
};

//
// WadChainIterator
//
// Iterates through the lumps on a hash chain.
//
class WadChainIterator : public WadIterator
{
protected:
   const char  *lumpname;
   lumpinfo_t  *chain;
   lumpinfo_t **lumpinfo;

public:
   WadChainIterator(const WadDirectory &dir, const char *name)
      : WadIterator(), lumpname(name)
   {
      chain    = dir.getLumpNameChain(name);
      lumpinfo = dir.getLumpInfo();
   }

   // Iteration will begin at the first lump on the hash chain for
   // the name that was passed into the constructor. Returns NULL
   // if the hash chain is empty.
   virtual lumpinfo_t *begin()
   {
      int idx = chain->namehash.index;
      return (lump = (idx >= 0 ? lumpinfo[idx] : NULL));
   }

   // Step to the next lump on the hash chain. Returns NULL when the
   // end of the chain has been reached.
   virtual lumpinfo_t *next()
   {
      if(lump)
      {
         int next = lump->namehash.next;
         lump = (next >= 0 ? lumpinfo[next] : NULL);
      }
      return lump;
   }

   // Test if the current lump in the iteration sequence has the same name
   // that was passed into the constructor, and optionally, is within an
   // indicated namespace. If no namespace is passed in, namespace will not
   // be considered relevant.
   bool testLump(int li_namespace = -1) const
   {
      return (lump &&
              !strcasecmp(lump->name, lumpname) &&
              (li_namespace == -1 || lump->li_namespace == li_namespace));
   }
};

#endif

// EOF

