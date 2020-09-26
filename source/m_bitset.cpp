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

#include "z_zone.h"
#include "m_bitset.h"

//
// Copy assignment
//
BitSet &BitSet::operator=(const BitSet &other)
{
   if((size + 7) / 8 != (other.size + 7) / 8)
      data = erealloc(byte *, data, (other.size + 7) / 8);
   size = other.size;
   memcpy(data, other.data, (size + 7) / 8);
   return *this;
}

//
// Move assignment
//
BitSet &BitSet::operator=(BitSet &&other)
{
   size = other.size;
   other.size = 0;
   if(data)
      efree(data);
   data = other.data;
   other.data = nullptr;
   return *this;
}
