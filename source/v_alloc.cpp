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
// Authors: James Haley
//

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
        (*cur)->allocator(w, h);
        cur = cur->dllNext;
    }
}

// EOF

