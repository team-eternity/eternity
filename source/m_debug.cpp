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
#include "p_maputl.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_state.h"

//
// Constructor shall produce gametic and frameid
//
DebugLogger::DebugLogger() : lsym(ls_start)
{
   *this << gametic << ':' << frameid;
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
DebugLogger &DebugLogger::operator << (const char *text)
{
   checkspace(ls_string);
   fputs(text, stdout);
   return *this;
}
DebugLogger &DebugLogger::operator << (char character)
{
   checkspace(ls_char);
   putchar(character);
   return *this;
}
DebugLogger &DebugLogger::operator << (int number)
{
   checkspace(ls_number);
   printf("%d", number);
   return *this;
}
DebugLogger &DebugLogger::operator << (unsigned number)
{
   checkspace(ls_number);
   printf("%u", number);
   return *this;
}
DebugLogger &DebugLogger::operator << (long number)
{
   checkspace(ls_number);
   printf("%ld", number);
   return *this;
}
DebugLogger &DebugLogger::operator << (double number)
{
   checkspace(ls_number);
   printf("%.16g", number);
   return *this;
}

//==================================================================================================
//
// DERIVED TYPES
//
//==================================================================================================
DebugLogger &DebugLogger::operator >> (fixed_t number)
{
   return *this << M_FixedToDouble(number);
}
DebugLogger &DebugLogger::operator << (const divline_t &line)
{
   return *this << "divline(" >> line.x >> line.y >> line.dx >> line.dy << ')';
}
DebugLogger &DebugLogger::operator << (const line_t &line)
{
   return *this << "line" << (&line - lines) << '(' << line.v1 << line.v2 << ')';
}
DebugLogger &DebugLogger::operator << (const sector_t &sector)
{
   return *this << "sector" << (&sector - sectors);
}
DebugLogger &DebugLogger::operator << (const seg_t &seg)
{
   return *this << "seg" << (&seg - segs) << '(' << seg.v1 << seg.v2 << seg.offset << seg.sidedef
   << seg.linedef << seg.frontsector << seg.backsector << "len(" << seg.len << "))";
}
DebugLogger &DebugLogger::operator << (const side_t &side)
{
   return *this << "side" << (&side - sides) << '(' >> side.textureoffset >> side.rowoffset
   << "top(" << side.toptexture << ") bot(" << side.bottomtexture << ") mid(" << side.midtexture
   << ") " << side.sector << "spec(" << side.special << "))";
}

DebugLogger &DebugLogger::operator << (const v2fixed_t &vec)
{
   return *this << "v2fixed(" >> vec.x >> vec.y << ")";
}

DebugLogger &DebugLogger::operator << (const vertex_t &vertex)
{
   return *this << "v" << (&vertex - vertexes) << '(' >> vertex.x >> vertex.y << ')';
}

//
// Checks if to add space
//
// SPACING RULE: never at the beginning. Then add it, unless it's:
// - number after string
// - number after character
// - character after string
// - character after character
// - character after number
//
void DebugLogger::checkspace(lastsym newsym)
{
   lastsym sym = lsym;
   lsym = newsym;
   switch(sym)
   {
      case ls_start:
         return;  // never add space at beginning
      case ls_char:
      case ls_string:
         if(newsym != ls_number && newsym != ls_char)
            putchar(' ');
         return;
      case ls_number:
         if(newsym != ls_char)
            putchar(' ');
         return;
      default:
         putchar(' ');
         return;
   }
}

#endif
// EOF
