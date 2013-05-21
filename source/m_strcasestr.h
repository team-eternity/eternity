// Emacs style mode select -*- C++ -*- vi:ts=3:sw=3:set et:
//-----------------------------------------------------------------------------
//
// Copyright (C) 2005-2007 Free Software Foundation, Inc.
// Written by Bruno Haible <bruno@clisp.org>, 2005.
//
// Modifications for Eternity Copyright(C) 2011 James Haley
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
// GNUlib strcasestr implementation for case-insensitive substring detection.
//
//-----------------------------------------------------------------------------

#ifndef M_STRCASESTR_H__
#define M_STRCASESTR_H__

const char *M_StrCaseStr(const char *haystack, const char *needle);
      char *M_StrCaseStr(      char *haystack, const char *needle);

#endif

// EOF
