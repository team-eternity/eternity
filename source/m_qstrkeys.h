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
//--------------------------------------------------------------------------
//
// Hash key classes for use of qstrings as hash keys.
//
// By James Haley
//
//-----------------------------------------------------------------------------

#ifndef M_QSTRKEYS_H__
#define M_QSTRKEYS_H__

#include "m_qstr.h"

//
// ENCQStrHashKey
//
// Case-insensitive qstring hash key interface.
//
class ENCQStrHashKey
{
public:
   typedef qstring     basic_type;
   typedef const char *param_type;

   // NB: HashCode must be implemented for both types when they are distinct.

   static unsigned int HashCode(const char *input)
   {
      return qstring::HashCodeStatic(input);
   }

   static unsigned int HashCode(const qstring &qstr)
   {
      return qstr.hashCode();
   }

   static bool Compare(const qstring &first, const char *second)
   {
      return !first.strCaseCmp(second);
   }
};

//
// EQStrHashKey
//
// Case-sensitive version.
//
class EQStrHashKey
{
public:
   typedef qstring     basic_type;
   typedef const char *param_type;

   static unsigned int HashCode(const char *input)
   {
      return qstring::HashCodeCaseStatic(input);
   }

   static unsigned int HashCode(const qstring &qstr)
   {
      return qstr.hashCodeCase();
   }

   static bool Compare(const qstring &first, const char *second)
   {
      return first == second;
   }
};

#endif


