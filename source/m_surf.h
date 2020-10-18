//
// The Eternity Engine
// Copyright (C) 2020 James Haley et al.
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
// Purpose: Surface structure for floor/ceiling doubles.
// Authors: Ioan Chera
//

#ifndef M_SURF_H_
#define M_SURF_H_

//
// Common enum to get surface index to avoid repeating floor/ceiling etc.
//
enum surf_e
{
   surf_floor,
   surf_ceil,
   surf_NUM
};

inline static const surf_e SURFS[] = { surf_floor, surf_ceil };

inline static surf_e operator!(surf_e surf)
{
   return (surf_e)!(int)surf;
}

//
// More convenient than raw array. Must be POD
//
template<typename T>
union Surfaces
{
   // THESE MUST BE the same order as the surf_e enum
   struct
   {
      T floor;
      T ceiling;
   };
   T items[2];

   Surfaces() = default;
   Surfaces(const T &first, const T &second) : floor(first), ceiling(second)
   {
   }

   T &operator[](int surf)
   {
      return items[surf];
   }
   const T &operator[](int surf) const
   {
      return items[surf];
   }
   template<typename FUNC, typename U>
   void operate(FUNC &&func, const Surfaces<U> &other)
   {
      floor = func(other.floor);
      ceiling = func(other.ceiling);
   }
};

//
// Check if floor is higher or ceiling is lower, respectively viceversa
// Also have template versions
//
template<typename T>
inline static bool isInner(surf_e surf, const T &a, const T &b)
{
   return surf == surf_floor ? a > b : a < b;
}
template<surf_e S, typename T>
inline static bool isInner(const T &a, const T &b)
{
   return S == surf_floor ? a > b : a < b;
}
template<typename T>
inline static bool isOuter(surf_e surf, const T &a, const T &b)
{
   return surf == surf_floor ? a < b : a > b;
}
template<surf_e S, typename T>
inline static bool isOuter(const T &a, const T &b)
{
   return S == surf_floor ? a < b : a > b;
}

#endif

// EOF
