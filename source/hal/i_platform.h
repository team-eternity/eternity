// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//--------------------------------------------------------------------------
//
// DESCRIPTION:  
//    Platform Defines
//
//-----------------------------------------------------------------------------

#ifndef I_PLATFORM_H__
#define I_PLATFORM_H__

#define EE_PLATFORM_WINDOWS 0
#define EE_PLATFORM_LINUX   1
#define EE_PLATFORM_FREEBSD 2
#define EE_PLATFORM_MACOSX  3
#define EE_PLATFORM_UNKNOWN 4

#if defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__)
#define EE_CURRENT_PLATFORM EE_PLATFORM_WINDOWS
#elif defined(__FreeBSD__)
#define EE_CURRENT_PLATFORM EE_PLATFORM_FREEBSD
#elif defined(__MACOSX__)
#define EE_CURRENT_PLATFORM EE_PLATFORM_MACOSX
#elif defined(LINUX) || defined(__GNUC__)
// Note guesses Linux if LINUX not defined but compiling with GCC and was not
// caught by an earlier check above.
#define EE_CURRENT_PLATFORM EE_PLATFORM_LINUX
#else
#define EE_CURRENT_PLATFORM EE_PLATFORM_UNKNOWN
#endif

#define EE_COMPILER_MSVC    0x00000001
#define EE_COMPILER_CYGWIN  0x00000002
#define EE_COMPILER_MINGW   0x00000004
#define EE_COMPILER_GCC     0x00000008
#define EE_COMPILER_UNKNOWN 0x00000010,

// Mask to match any Windows compiler
#define EE_COMPILER_WINDOWS (EE_COMPILER_MSVC | EE_COMPILER_CYGWIN | EE_COMPILER_MINGW)

// Mask to match any GCC compiler
#define EE_COMPILER_GCCANY  (EE_COMPILER_GCC | EE_COMPILER_CYGWIN | EE_COMPILER_MINGW)

#if defined(_MSC_VER)
#define EE_CURRENT_COMPILER EE_COMPILER_MSVC
#elif defined(__CYGWIN__)
#define EE_CURRENT_COMPILER EE_COMPILER_CYGWIN
#elif defined(__MINGW32__)
#define EE_CURRENT_COMPILER EE_COMPILER_MINGW
#elif defined(__GNUC__)
#define EE_CURRENT_COMPILER EE_COMPILER_GCC
#else
#define EE_CURRENT_COMPILER EE_COMPILER_UNKNOWN
#endif

extern int ee_current_platform;
extern int ee_current_compiler;

#endif

// EOF

