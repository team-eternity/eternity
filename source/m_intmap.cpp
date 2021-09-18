//
// The Eternity Engine
// Copyright (C) 2021 James Haley et al.
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
// Purpose: Read-only contiguous storage [int: [int]] data map
// Authors: Ioan Chera
//

#include <assert.h>
#include "z_zone.h"
#include "m_intmap.h"

//
// Setup
//
void IntListMap::load(const Collection<PODCollection<int>> &source)
{
   size_t size = source.getLength() + 1;
   for(const PODCollection<int> &element : source)
      size += element.getLength();
   efree(mData);
   mData = emalloc(decltype(mData), size * sizeof(*mData));

   size_t pos = source.getLength() + 1;
   int *ptr = mData;
   for(size_t i = 0; i < source.getLength(); ++i)
   {
      *ptr++ = static_cast<int>(pos);
      pos += source[i].getLength();
   }
   *ptr++ = static_cast<int>(pos); // also the final one to determine size
   for(const PODCollection<int> &element : source)
      for(int value : element)
         *ptr++ = value;
}

//
// Gets access to a given list
//
const int *IntListMap::getList(int index, int *length) const
{
   *length = mData[index + 1] - mData[index];
   return mData + mData[index];
}

//
// Load data from two given collections
//
// LAYOUT: num_lists + 1 items
//         then for each list pair, address of second list, followed by first list, then second list
//
void DualIntListMap::load(const Collection<PODCollection<int>> &first,
                          const Collection<PODCollection<int>> &second)
{
   size_t length = first.getLength();
   assert(length == second.getLength());
   size_t size = length + 1;

   for(size_t i = 0; i < length; ++i)
      size += 1 + first[i].getLength() + second[i].getLength();

   efree(mData);
   mData = emalloc(decltype(mData), size * sizeof(*mData));

   size_t pos = length + 1;
   int *ptr = mData;
   for(size_t i = 0; i < length; ++i)
   {
      *ptr++ = static_cast<int>(pos);
      pos += 1 + first[i].getLength() + second[i].getLength();
   }
   *ptr++ = static_cast<int>(pos);
   pos = length + 1;
   for(size_t i = 0; i < length; ++i)
   {
      *ptr++ = static_cast<int>(pos + 1 + first[i].getLength());
      for(int value : first[i])
         *ptr = value;
      for(int value : second[i])
         *ptr = value;
      pos += 1 + first[i].getLength() + second[i].getLength();
   }
}

// EOF
