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

// haleyjd 03/21/10
enum
{
   DWF_FILE,
   DWF_LUMP,
   DWF_DATA,
   DWF_NUMTYPES
};

struct DWFILE
{
   int type;
   byte *inp, *lump, *data; // Pointer to lump, FILE, or data
   int size;
   int origsize;            // for ungetc
   int lumpnum;             // haleyjd 03/08/06: need to save this
};

char  *D_Fgets(char *buf, size_t n, DWFILE *fp);
int    D_Feof(DWFILE *fp);
int    D_Fgetc(DWFILE *fp);
int    D_Ungetc(int c, DWFILE *fp);
void   D_OpenFile(DWFILE *infile, const char *filename, const char *mode);
void   D_OpenLump(DWFILE *infile, int lumpnum);
void   D_Fclose(DWFILE *dwfile);
size_t D_Fread(void *dest, size_t size, size_t num, DWFILE *file);
size_t D_FileLength(DWFILE *file);

inline static bool D_IsOpen(DWFILE *dwfile)
{
   return !!(dwfile->inp);
}

inline static bool D_IsLump(DWFILE *dwfile)
{
   return !!(dwfile->lump);
}

inline static bool D_IsData(DWFILE *dwfile)
{
   return !!(dwfile->data);
}

#endif

// EOF

