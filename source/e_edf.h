// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2003 James Haley
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
// EDF Core
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef __E_EDF_H__
#define __E_EDF_H__

// sprite variables

extern int blankSpriteNum;

void E_ProcessEDF(const char *filename);
void E_ProcessNewEDF(void);

void E_EDFSetEnableValue(const char *, int); // enables

void E_EDFLogPuts(const char *msg);
void E_EDFLogPrintf(const char *msg, ...);

#ifdef __GNUC__
void E_EDFLoggedErr(int lv, const char *msg, ...) __attribute__((noreturn));
#else
void E_EDFLoggedErr(int lv, const char *msg, ...);
#endif

void E_EDFLoggedWarning(int lv, const char *msg, ...);

#endif

// EOF

