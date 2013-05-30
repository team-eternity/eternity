// Emacs style mode select -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 James Haley
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
//   Video and rendering related buffers which must allocate and deallocate
//   with screen resolution changes.
//
//-----------------------------------------------------------------------------

#ifndef V_ALLOC_H__
#define V_ALLOC_H__

#include "m_dllist.h"

class VAllocItem
{
public:
   typedef void (*allocfn_t)(int w, int h);

protected:
   static DLListItem<VAllocItem> *vAllocList;

   DLListItem<VAllocItem> links;
   allocfn_t allocator;

public:
   VAllocItem(allocfn_t p_allocator) : links(), allocator(p_allocator) 
   {
      links.insert(this, &vAllocList);
   }

   static void FreeAllocs();
   static void SetNewMode(int w, int h);
};

#define VALLOCFNNAME(name) VAllocFn_ ## name
#define VALLOCFNSIG(name)  static void VALLOCFNNAME(name) (int, int)
#define VALLOCDECL(name)   static VAllocItem vAllocItem_ ## name (VALLOCFNNAME(name))
#define VALLOCFNDEF(name)  static void VALLOCFNNAME(name) (int w, int h)

#define VALLOCATION(name) \
   VALLOCFNSIG(name);     \
   VALLOCDECL(name);      \
   VALLOCFNDEF(name)

#endif

// EOF

