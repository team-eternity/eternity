// Emacs style mode select   -*- C++ -*- vi:sw=3 ts=3:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2010 James Haley
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Management of private wad directories for individual level resources
//
//-----------------------------------------------------------------------------

#ifndef W_LEVELS_H__
#define W_LEVELS_H__

class WadDirectory;

struct wadlevel_t
{
   char header[9];    // header lump name
   int  lumpnum;      // lump number, relative to directory
   WadDirectory *dir; // parent directory
};

extern char *w_masterlevelsdirname;
extern bool inmasterlevels;          // true if we are playing master levels

WadDirectory *W_AddManagedWad(const char *filename);
WadDirectory *W_GetManagedWad(const char *filename);
const char   *W_GetManagedDirFN(WadDirectory *waddir);
wadlevel_t   *W_FindAllMapsInLevelWad(WadDirectory *dir);
wadlevel_t   *W_FindLevelInDir(WadDirectory *waddir, const char *name);
void          W_DoMasterLevels(bool allowexit);
void          W_EnumerateMasterLevels(bool forceRefresh);

#endif

// EOF

