// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2006 James Haley
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
//
// Separated DWFILE functions from d_io.h
//
//-----------------------------------------------------------------------------

#ifndef D_DWFILE_H__
#define D_DWFILE_H__

#include "doomtype.h"
#include "d_keywds.h"

typedef struct 
{
   byte *inp, *lump; // Pointer to string or FILE
   long size;
   long origsize;    // for ungetc
   long lumpnum;     // haleyjd 03/08/06: need to save this
} DWFILE;

char  *D_Fgets(char *buf, size_t n, DWFILE *fp);
int    D_Feof(DWFILE *fp);
int    D_Fgetc(DWFILE *fp);
int    D_Ungetc(int c, DWFILE *fp);
void   D_OpenFile(DWFILE *infile, const char *filename, char *mode);
void   D_OpenLump(DWFILE *infile, int lumpnum);
void   D_Fclose(DWFILE *dwfile);
size_t D_Fread(void *dest, size_t size, size_t num, DWFILE *file);
size_t D_FileLength(DWFILE *file);

d_inline static boolean D_IsOpen(DWFILE *dwfile)
{
   return !!(dwfile->inp);
}

d_inline static boolean D_IsLump(DWFILE *dwfile)
{
   return !!(dwfile->lump);
}

#endif

// EOF

