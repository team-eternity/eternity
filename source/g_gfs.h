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
// Purpose: GFS -- Game File Scripts.
//
//  This is a radical new way for DOOM editing projects to provide
//  file information to Eternity. A single GFS script, when provided
//  via the -gfs command-line parameter, will affect the loading of
//  an arbitrary number of WADs, DEH/BEX's, EDF files, and console
//  scripts.
//
// Authors: James Haley
//

#ifndef G_GFS_H__
#define G_GFS_H__

struct gfs_t
{
    char **wadnames;
    char **dehnames;
    char **cscnames;

    char *iwad;
    char *filepath;
    char *edf;

    int  numwads;
    int  numdehs;
    int  numcsc;
    bool hasIWAD;
    bool hasEDF;
};

gfs_t      *G_LoadGFS(const char *filename);
void        G_FreeGFS(gfs_t *lgfs);
const char *G_GFSCheckIWAD(void);
const char *G_GFSCheckEDF(void);

#endif

// EOF

