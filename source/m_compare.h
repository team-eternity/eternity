//
// Copyright (C) 2013 James Haley et al.
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
//      Comparison templates
//
//-----------------------------------------------------------------------------

#ifndef M_COMPARE_H__
#define M_COMPARE_H__

//
// emin
//
// Return a reference to the lesser of two objects.
//
template<typename T> inline const T &emin(const T &a, const T &b)
{
   return ((b < a) ? b : a);
}

//
// emax
//
// Return a reference to the greater of two objects.
//
template<typename T> inline const T &emax(const T &a, const T &b)
{
   return ((a < b) ? b : a);
}

//
// eclamp
//
// Clamp a value to a given range, inclusive.
//
template<typename T> inline const T &eclamp(const T &a, const T &min, const T &max)
{
   return (a < min ? min : (max < a ? max : a));
}

#endif

// EOF

