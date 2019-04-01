//
// The Eternity Engine
// Copyright (C) 2019 James Haley et al.
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
// Purpose: Convert Windows wide strings to UTF-8 and vice versa.
// Authors: Ioan Chera
//

#include <windows.h>
#include "../z_zone.h"
#include "i_winstrings.h"

//
// This is a fallback if wide-to-utf8 fails for any reason
//
static qstring I_wideToASCII(const wchar_t *text)
{
   qstring result(wcslen(text));
   for(const wchar_t *w = text; *w; ++w)
   {
      if(*w >= 256)
         result.Putc('?'); // better to put a placeholder than wrap around and hit a control char.
      else
         result.Putc(static_cast<char>(*w));
   }
   return result;
}

//
// Tries to convert wide char string to utf8. If failing, it will just use ASCII.
//
qstring I_WideToUTF8(const wchar_t *text)
{
   int count = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
   if(!count)
      return I_wideToASCII(text);

   qstring result(count);
   for(int i = 0; i < count; ++i)
      result.Putc(' '); // FIXME: find a neater way to enlarge a qstring

   if(!WideCharToMultiByte(CP_UTF8, 0, text, -1, result.getBuffer(), count, nullptr, nullptr))
      return I_wideToASCII(text);

   return result.Delc(); // reduce the last null terminator
}

// EOF
