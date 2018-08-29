// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2018 Ioan Chera
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
//      Vocabulary and natural language processing
//
//-----------------------------------------------------------------------------

#include "../z_zone.h"
#include "b_vocabulary.h"
#include "../m_ctype.h"
#include "../m_qstr.h"

//
// Adds spaces in a CamelCase string
//
void VOC_AddSpaces(qstring &camel)
{
   size_t len = camel.length();
   for(size_t i = 0; i < len; ++i)
      if(camel[i] >= 'A' && camel[i] <= 'Z' && i)
      {
         camel.insert(" ", i++);
         ++len;
      }
}

//
// Returns a or an depending on noun
//
const char *VOC_IndefiniteArticle(const char *noun)
{
   char s = ectype::toLower(*noun);
   switch(s)
   {
      case 'a':
      case 'e':
      case 'i':
      case 'o':
      case 'u':
         return "an";
      default:
         return "a";
   }
}
