// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2014 Ioan Chera
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
// DESCRIPTION:
//      Pointer-pointer map
//
//-----------------------------------------------------------------------------

#ifndef __EternityEngine__b_ptrmap__
#define __EternityEngine__b_ptrmap__

#include "../m_collection.h"
#include "../m_dllist.h"

// A and B should be expressed without *
template <typename A, typename B> class PointerMap
{
//	struct Unit
//	{
//		B* pointer;
//		size_t index;
//	};
	
	
	PODCollection<DLListItem<B>> m_pointers;
	
	PODCollection<size_t> m_table;
	
public:
	
};

#endif /* defined(__EternityEngine__b_ptrmap__) */
