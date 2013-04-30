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

#include "z_zone.h"
#include "v_alloc.h"

// Global list of all VAllocItem instances
DLListItem<VAllocItem> *VAllocItem::vAllocList;

//
// VAllocItem::FreeAllocs
//
// Frees all VAllocItem instances' allocations.
//
void VAllocItem::FreeAllocs()
{
   Z_FreeTags(PU_VALLOC, PU_VALLOC);
}

//
// VAllocItem::SetNewMode
//
// Invokes the allocation method of all VAllocItem instances.
//
void VAllocItem::SetNewMode(int w, int h)
{
   DLListItem<VAllocItem> *cur = vAllocList;

   while(cur)
   {
      cur->dllObject->allocator(w, h);
      cur = cur->dllNext;
   }
}


// EOF

