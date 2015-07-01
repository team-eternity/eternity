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
//      Bit set of integers
//
//-----------------------------------------------------------------------------

#ifndef B_INTSET_H_
#define B_INTSET_H_

#include "../m_collection.h"

class IntOSet : public ZoneObject
{
   uint32_t*            m_buffer;
   size_t               m_bufferSize;
   size_t               m_bitCount;
   size_t               m_minIndex;
   size_t               m_maxIndex;

   PODCollection<int>   m_coll;
public:
   static int s_maxSize;

   IntOSet() : m_bitCount(s_maxSize), m_maxIndex(0)
   {
      m_bufferSize = (s_maxSize + 8 * sizeof(*m_buffer) - 1) / (8 * sizeof(*m_buffer));
      m_minIndex = m_bufferSize;
      m_buffer = ecalloc(uint32_t*, m_bufferSize, sizeof(*m_buffer));
   }
   IntOSet(const IntOSet &other) : m_bufferSize(other.m_bufferSize), m_bitCount(other.m_bitCount), m_minIndex(other.m_minIndex), m_maxIndex(other.m_maxIndex), m_coll(other.m_coll)
   {
      m_buffer = emalloc(uint32_t*, m_bufferSize * sizeof(*m_buffer));
      memcpy(m_buffer, other.m_buffer, m_bufferSize * sizeof(*m_buffer));
   }
   IntOSet(IntOSet &&other) : m_buffer(other.m_buffer), m_bufferSize(other.m_bufferSize), m_bitCount(other.m_bitCount), m_minIndex(other.m_minIndex), m_maxIndex(other.m_maxIndex), m_coll(other.m_coll)
   {
      other.m_buffer = nullptr;
   }
   ~IntOSet()
   {
      efree(m_buffer);
   }
   void insert(int val);
   void insert(const int* begin, const int* end);
   size_t count(int val) const
   {
      return (m_buffer[val / (sizeof(*m_buffer) * 8)] >> (val % (sizeof(*m_buffer) * 8))) & 1;
   }
   int* begin() const
   {
      return m_coll.begin();
   }
   int* end() const
   {
      return m_coll.end();
   }
   const int* cbegin() const
   {
      return m_coll.begin();
   }
   const int* cend() const
   {
      return m_coll.end();
   }
   size_t size() const
   {
      return m_coll.getLength();
   }
   bool operator==(const IntOSet& other) const;
   void clear();
   IntOSet& operator=(const IntOSet& other);
   IntOSet& operator=(IntOSet&& other);
};

#endif
// EOF

