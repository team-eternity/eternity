// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//  Macros to resolve compiler-dependent keywords and language
//  extensions.
//
//-----------------------------------------------------------------------------

#ifndef D_KEYWDS_H__
#define D_KEYWDS_H__

// attribute hints are available only under GNU C
// Note: The idea of E_PRINTF is lifted from q2pro's q_printf macro.
// q2pro is licenced under the GPL v2.0.
#ifndef __GNUC__
#define E_PRINTF(f, a)
#else
#define E_PRINTF(f, a) __attribute__((format(printf, f, a)))
#endif

#if _MSC_VER >= 1400 && defined(_DEBUG) && !defined(EE_NO_SAL)
#include <sal.h>
#if _MSC_VER > 1400
#define E_FORMAT_STRING(p) _Printf_format_string_ p
#else
#define E_FORMAT_STRING(p) __format_string p
#endif
#else
#define E_FORMAT_STRING(p) p
#endif

//
// Non-standard function availability defines
//
// These control the presence of custom code for some common
// non-ANSI libc functions that are in m_misc.c -- in some cases
// its not worth having the code when its already available in 
// the platform's libc
//

// HAVE_ITOA -- define this if your platform has ITOA
// ITOA_NAME -- name of your platform's itoa function
// #define HAVE_ITOA
// #define ITOA_NAME itoa

// known platforms with itoa -- you can add yours here, but it
// isn't absolutely necessary
#ifndef EE_HAVE_ITOA
   #ifdef DJGPP
      #define EE_HAVE_ITOA
      #define ITOA_NAME itoa
   #endif
   #ifdef _MSC_VER
      #define EE_HAVE_ITOA
      #define ITOA_NAME _itoa
   #endif
   #ifdef __MINGW32__
      #define EE_HAVE_ITOA
      #define ITOA_NAME itoa
   #endif
#endif

#ifdef _MSC_VER
#define strcasecmp  stricmp
#define strncasecmp strnicmp
#endif

#endif // D_KEYWDS_H__

// EOF
