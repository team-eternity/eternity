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

#ifndef I_WINSTRINGS_H_
#define I_WINSTRINGS_H_

#include "../m_qstr.h"

//
// Quick wide-string RAII class just to have safe wide-string returns
//
class WideString : public ZoneObject
{
public:
   explicit WideString(int size) : bufferSize(size)
   {
      buffer = ecalloc(wchar_t *, size + 1, sizeof(wchar_t));
   }
   WideString(const WideString &other) = delete;
   WideString(WideString &&other) : buffer(other.buffer), bufferSize(other.bufferSize)
   {
      other.buffer = nullptr;
      other.bufferSize = 0;
   }
   WideString(const char *text);
   ~WideString()
   {
      efree(buffer);
   }

   wchar_t *getBuffer()
   {
      return buffer;
   }

private:
   wchar_t *buffer;
   int bufferSize;
};

qstring I_WideToUTF8(const wchar_t *text);
WideString I_UTF8ToWide(const char *text);

#endif

// EOF
