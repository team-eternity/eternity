// Emacs style mode select - *-C++ - *-
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
//      Bit set of integers
//
//-----------------------------------------------------------------------------

#include "../z_zone.h"

#include "b_intset.h"

int IntOSet::s_maxSize = 16;

void IntOSet::insert(int val)
{
   size_t word = val / (sizeof(*m_buffer) * 8);
   int bit = 1 << (val % (sizeof(*m_buffer) * 8));
   if (m_buffer[word] & bit)
      return;
   if (word > m_maxIndex)
      m_maxIndex = word;
   if (word < m_minIndex)
      m_minIndex = word;
   m_buffer[word] |= bit;
   m_coll.add(val);
}

void IntOSet::insert(const int* begin, const int* end)
{
   for (const int* a = begin; a != end; ++a)
      insert(*a);
}

bool IntOSet::operator==(const IntOSet &other) const
{
   if (m_bitCount != other.m_bitCount)
      return false;
   if (m_coll.getLength() != other.m_coll.getLength())
      return false;
   size_t min = m_minIndex < other.m_minIndex ? m_minIndex : other.m_minIndex;
   size_t max = m_maxIndex > other.m_maxIndex ? m_maxIndex : other.m_maxIndex;
   for (size_t i = min; i <= max; ++i)
   {
      if (m_buffer[i] != other.m_buffer[i])
         return false;
   }
   return true;
}

void IntOSet::clear() 
{
   m_coll.resize(0);
   for (size_t i = m_minIndex; i <= m_maxIndex; ++i)
      m_buffer[i] = 0;
   m_minIndex = m_bufferSize;
   m_maxIndex = 0;
}

IntOSet& IntOSet::operator=(const IntOSet& other)
{
   m_bitCount = other.m_bitCount;
   
   m_coll = other.m_coll;
   m_minIndex = other.m_minIndex;
   m_maxIndex = other.m_maxIndex;
   if (m_bufferSize != other.m_bufferSize)
   {
      efree(m_buffer);
      m_bufferSize = other.m_bufferSize;
      m_buffer = emalloc(uint32_t*, m_bufferSize * sizeof(*m_buffer));
   }
   memcpy(m_buffer, other.m_buffer, m_bufferSize * sizeof(*m_buffer));
   return *this;
}
IntOSet& IntOSet::operator=(IntOSet&& other)
{
   m_bitCount = other.m_bitCount;
   m_bufferSize = other.m_bufferSize;
   m_coll = other.m_coll;
   m_minIndex = other.m_minIndex;
   m_maxIndex = other.m_maxIndex;
   efree(m_buffer);
   m_buffer = other.m_buffer;
   other.m_buffer = nullptr;
   return *this;
}

// EOF

