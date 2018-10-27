//
// The Eternity Engine
// Copyright (C) 2018 James Haley et al.
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
// Purpose: Helper functions for logging and debugging. Should be disabled in release builds.
// Authors: Ioan Chera
//

#ifndef M_DEBUG_H_
#define M_DEBUG_H_

#if defined(_DEBUG) && !defined(NDEBUG)

struct vertex_t;

//
// Debug logging facility. Uses C++ style for easier integration of new types than printf.
//
class DebugLogger
{
public:
   DebugLogger();
   ~DebugLogger();

   // Fundamental types
   const DebugLogger &operator << (const char *text) const;

   const DebugLogger &operator << (char character) const;
   const DebugLogger &operator << (int number) const;
   const DebugLogger &operator << (unsigned number) const;
   const DebugLogger &operator << (double number) const;

   // Structural types
   const DebugLogger &operator << (const vertex_t &vertex) const;
};

#endif

#endif

// EOF
