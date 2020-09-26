//
// The Eternity Engine
// Copyright(C) 2020 James Haley et al.
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
//----------------------------------------------------------------------------
//
// Purpose: Variable length array of single bits.
// Authors: Ioan Chera
//

#ifndef M_BITSET_H_
#define M_BITSET_H_

//
// Bit set
//
class BitSet
{
public:
   //
   // Constructor expects a size
   //
   explicit BitSet(int size) : size(size)
   {
      data = ecalloc(byte *, (size + 7) / 8, 1);
   }

   BitSet &operator=(const BitSet &other);

   //
   // Copy constructor
   //
   BitSet(const BitSet &other)
   {
      *this = other;
   }

   BitSet &operator=(BitSet &&other);

   //
   // Move constructor
   //
   BitSet(BitSet &&other)
   {
      *this = std::move(other);
   }

   //
   // Destructor
   //
   ~BitSet()
   {
      efree(data);
   }

   //
   // Enable flag
   //
   void enable(int index)
   {
      data[index / 8] |= 1 << index % 8;
   }

   //
   // Check if flag is set
   //
   bool get(int index) const
   {
      return !!(data[index / 8] & 1 << index % 8);
   }

private:
   byte *data = nullptr;
   size_t size = 0;
};

#endif

// EOF
