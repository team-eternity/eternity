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
// Purpose: Read-only contiguous storage [int: [int]] data map
// Authors: Ioan Chera
//

#ifndef M_INTMAP_H_
#define M_INTMAP_H_

#include "m_collection.h"

//
// Base class which holds the common functionality
//
class BaseIntListMap : public ZoneObject
{
public:
   virtual ~BaseIntListMap()
   {
      efree(mData);
   }
protected:
   int *mData = nullptr;
};

//
// Holds [int: [int]] storage. Read only. Contiguous in memory.
//
// Meant to replace Collection<PODCollection<int>> data
//
class IntListMap : public BaseIntListMap
{
public:
   void load(const Collection<PODCollection<int>> &source);
   const int *getList(int index, int *length) const;
};

//
// This one has two lists per item
//
class DualIntListMap : public BaseIntListMap
{
public:
   void load(const Collection<PODCollection<int>> &first,
             const Collection<PODCollection<int>> &second);
   const int *getList(int index, int which, int *length) const;
};

#endif /* M_INTMAP_H_ */

// EOF
