//-----------------------------------------------------------------------------
//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Portable snprintf.
//  Amongst other things, tries to always guarantee the buffer will be null-
//  terminated, which some implementations do not do.
//
// Authors: James Haley
//

#include "z_zone.h"

#include "psnprntf.h"

#ifdef _WIN32
#if _MSC_VER < 1400 /* not needed for Visual Studio 2008 */
#define vsnprintf _vsnprintf
#endif
#endif

//
// Portable snprintf
//
int psnprintf(char *str, size_t n, E_FORMAT_STRING(const char *format), ...)
{
   va_list args;
   int ret;

   va_start(args, format);
   ret = pvsnprintf(str, n, format, args);
   va_end(args);

   return ret;
}

//
// Portable vsnprintf
//
int pvsnprintf(char *str, size_t nmax, E_FORMAT_STRING(const char *format), va_list ap)
{
   if(nmax < 1)
   {
      return 0;
   }

   // vsnprintf() in Windows (and possibly other OSes) doesn't always
   // append a trailing \0. We have the responsibility of making this
   // safe by writing into a buffer that is one byte shorter ourselves.
   int result = vsnprintf(str, nmax, format, ap);

   // If truncated, change the final char in the buffer to a \0.
   // In Windows, a negative result indicates a truncated buffer.
   if(result < 0 || (size_t)result >= nmax)
   {
      str[nmax - 1] = '\0';
      result = int(nmax - 1);
   }

   return result;
}

// EOF

