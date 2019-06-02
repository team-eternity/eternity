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

#include <typeinfo>
#include "m_fixed.h"

struct divline_t;
struct line_t;
struct pwindow_t;
struct sector_t;
struct seg_t;
struct side_t;
struct v2double_t;
struct v2fixed_t;
struct v3fixed_t;
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
   DebugLogger &operator << (const char *text);

   DebugLogger &operator << (char character);
   DebugLogger &operator << (int number);
   DebugLogger &operator << (unsigned number);
   DebugLogger &operator << (long number);
   DebugLogger &operator << (unsigned long number);
   DebugLogger &operator << (long long number);
   DebugLogger &operator << (double number);

   // Convenience
   DebugLogger &operator >> (fixed_t number);

   // Structural types
   DebugLogger &operator << (const divline_t &divline);
   DebugLogger &operator << (const line_t &line);
   DebugLogger &operator << (const pwindow_t &window);
   DebugLogger &operator << (const side_t &side);
   DebugLogger &operator << (const sector_t &sector);
   DebugLogger &operator << (const seg_t &seg);
   DebugLogger &operator << (const v2double_t &vec);
   DebugLogger &operator << (const v2fixed_t &vec);
   DebugLogger &operator << (const v3fixed_t &vec);
   DebugLogger &operator << (const vertex_t &vertex);

   template<typename T>
   DebugLogger &operator << (const T *item)
   {
      return item ? *this << *item : *this << "null(" << typeid(T).name() << ')';
   }

private:
   enum lastsym
   {
      ls_start,
      ls_string,
      ls_char,
      ls_number
   };

   void checkspace(lastsym newsym);

   lastsym lsym;
};

#endif

#endif

// EOF
