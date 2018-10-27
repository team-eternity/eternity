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

#if defined(_DEBUG) && !defined(NDEBUG)

#include "z_zone.h"

#include "doomstat.h"
#include "m_debug.h"
#include "m_fixed.h"
#include "r_defs.h"
#include "r_main.h"

//
// Constructor shall produce gametic and frameid
//
DebugLogger::DebugLogger()
{
   *this << gametic << ':' << frameid << ' ';
}

//
// Destructor shall put an "enter". You can thus create temporaries which will write a full line.
//
DebugLogger::~DebugLogger()
{
   *this << '\n';
}

//==================================================================================================
//
// FUNDAMENTAL TYPES
//
//==================================================================================================
const DebugLogger &DebugLogger::operator << (const char *text) const
{
   fputs(text, stdout);
   return *this;
}
const DebugLogger &DebugLogger::operator << (char character) const
{
   putchar(character);
   return *this;
}
const DebugLogger &DebugLogger::operator << (int number) const
{
   printf("%d", number);
   return *this;
}
const DebugLogger &DebugLogger::operator << (unsigned number) const
{
   printf("%u", number);
   return *this;
}
const DebugLogger &DebugLogger::operator << (double number) const
{
   printf("%.16g", number);
   return *this;
}

//==================================================================================================
//
// DERIVED TYPES
//
//==================================================================================================
const DebugLogger &DebugLogger::operator << (const vertex_t &vertex) const
{
   return *this << "v(" << M_FixedToDouble(vertex.x) << ' ' << M_FixedToDouble(vertex.y) << ')';
}

#endif
// EOF
