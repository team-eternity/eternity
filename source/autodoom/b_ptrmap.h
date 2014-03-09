// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 Ioan Chera
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

// A and B should be expressed without *
template <typename A, typename B> class PointerMap
{
	struct Unit
	{
		B* pointer;
		size_t index;
	};
	
	Unit* m_buffer;
	size_t m_length;
	size_t m_alloc;
	
public:
	PointerMap() : m_buffer(nullptr), m_length(0), m_alloc(0)
	{
	}
	PointerMap(const PointerMap<A, B> &o) : m_length(o.m_length), m_alloc(o.m_alloc)
	{
		m_buffer = estructalloc(Unit, m_alloc);
		memcpy(m_buffer, o.m_buffer, m_alloc * sizeof(Unit));
	}
	PointerMap(PointerMap<A, B> &&o) : m_length(o.m_length), m_alloc(o.m_alloc), m_buffer(o.m_buffer)
	{
		o.m_buffer = nullptr;
		o.m_length = o.m_alloc = 0;
	}
	~PointerMap()
	{
		efree(m_buffer);
	}
};

#endif /* defined(__EternityEngine__b_ptrmap__) */
