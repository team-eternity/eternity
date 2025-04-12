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
// Purpose: Video and rendering related buffers which must allocate and
//  deallocate with screen resolution changes.
//
// Authors: James Haley, Stephen McGranahan
//

#ifndef V_ALLOC_H__
#define V_ALLOC_H__

#include "m_dllist.h"

class VAllocItem
{
public:
   using allocfn_t = void (*)(int w, int h);

protected:
   static DLListItem<VAllocItem> *vAllocList;

   DLListItem<VAllocItem> links;
   allocfn_t allocator;

public:
   explicit VAllocItem(allocfn_t p_allocator) : links(), allocator(p_allocator) 
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

