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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//------------------------------------------------------------------------------
//
// Purpose: Management of private wad directories for individual level
//  resources.
//
// Authors: James Haley
//

#ifndef W_LEVELS_H__
#define W_LEVELS_H__

class WadDirectory;

struct wadlevel_t
{
    char          header[9]; // header lump name
    int           lumpnum;   // lump number, relative to directory
    WadDirectory *dir;       // parent directory
};

extern char *w_masterlevelsdirname;
extern char *w_norestpath;
extern int   inmanageddir; // non-zero if we are playing a managed dir level

// managed directory level/mission types
enum
{
    MD_NONE,         // not in a managed directory
    MD_MASTERLEVELS, // Master Levels
    MD_NR4TL,        // No Rest for the Living
    MD_OTHER         // Hell if I Know
};

WadDirectory *W_AddManagedWad(const char *filename);
WadDirectory *W_GetManagedWad(const char *filename);
const char   *W_GetManagedDirFN(WadDirectory *waddir);
wadlevel_t   *W_FindAllMapsInLevelWad(WadDirectory *dir);
wadlevel_t   *W_FindLevelInDir(WadDirectory *waddir, const char *name);
void          W_DoMasterLevels(bool allowexit, int skill);
void          W_EnumerateMasterLevels(bool forceRefresh);
void          W_DoNR4TLStart(int skill);
void          W_InitManagedMission(int mission);

#endif

// EOF

