// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2012 James Haley
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
//----------------------------------------------------------------------------
//
// Aeon Engine - SpiderMonkey JavaScript Engine - Win32 Configuration
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef AE_JSCFGLINUX_H__
#define AE_JSCFGLINUX_H__

#if EE_CURRENT_PLATFORM != EE_PLATFORM_LINUX
#error Wrong platform for ae_jscfglinux.h!
#endif

#ifndef XP_UNIX
#define XP_UNIX
#endif

#ifndef SVR4
#define SVR4
#endif

#ifndef SYSV
#define SYSV
#endif

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#ifndef POSIX_SOURCE
#define POSIX_SOURCE
#endif

#ifndef HAVE_LOCALTIME_R
#define HAVE_LOCALTIME_R
#endif

#ifndef HAVE_VA_COPY
#define HAVE_VA_COPY
#endif

#ifndef VA_COPY
#define VA_COPY va_copy
#endif

#ifndef STATIC_JS_API
#define STATIC_JS_API
#endif

#endif

// EOF

