//
// The Eternity Engine
// Copyright (C) 2016 James Haley et al.
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
// Purpose: Utility functions
// Authors: James Haley et al.
//

#ifndef M_UTILS_H__
#define M_UTILS_H__

#include "d_keywds.h"
#include "doomtype.h"

// File IO utilities

bool  M_WriteFile(const char *name, void *source, size_t length);
int   M_ReadFile(const char *name, byte **buffer);
char *M_TempFile(const char *s);
char *M_LoadStringFromFile(const char *filename);

// haleyjd: Portable versions of common non-standard C functions, as well as
// some misc string routines that really don't fit anywhere else. Some of these
// default to the platform implementation if its existence is verifiable 
// (see d_keywds.h)
// MaxW: 20151221: M_Strnlen moved from psnprntf.cpp, originally called pstrnlen.

size_t M_Strnlen(const char *s, size_t count); 
char  *M_Strupr(char *string);
char  *M_Strlwr(char *string);
char  *M_Itoa(int value, char *string, int radix);
int    M_CountNumLines(const char *str);
int    M_StringAlloca(char **str, int numstrs, size_t extra, const char *str1, ...);

// Misc file routines
// haleyjd: moved a number of these here from w_wad module.

void   M_GetFilePath(const char *fn, char *base, size_t len); // haleyjd
long   M_FileLength(FILE *f);
void   M_ExtractFileBase(const char *, char *);               // killough
char  *M_AddDefaultExtension(char *, const char *);           // killough 1/18/98
void   M_NormalizeSlashes(char *);                            // killough 11/98
char  *M_SafeFilePath(const char *pbasepath, const char *newcomponent);

int M_PositiveModulo(int op1, int op2);

//
// This one assumes op2 > 0. Needed to avoid calling abs if we guarantee op2 > 0
//
inline static int M_PositiveModPositiveRight(int op1, int op2)
{
   return (op1 % op2 + op2) % op2;
}

bool M_IsExMy(const char *name, int *episode, int *map);
bool M_IsMAPxy(const char *name, int *map);

#endif

// EOF

