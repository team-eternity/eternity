//
// The Eternity Engine
// Copyright (C) 2019 James Haley et al.
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
// Purpose: Code for loading data files, particularly PWADs.
//  Base, user, and game path determination and file-finding functions.
//
// Authors: James Haley
//

#ifndef D_FILES_H__
#define D_FILES_H__

struct gfs_t;

struct wfileadd_t;
extern wfileadd_t *wadfiles;     // killough 11/98

// enumeration for D_AddFile flags parameter
enum dafflags_e
{
   DAF_NONE    = 0,          // no special behaviors
   DAF_PRIVATE = 0x00000001, // is a private directory file
   DAF_IWAD    = 0x00000002, // is the IWAD
   DAF_DEMO    = 0x00000004  // is a demo
};

// WAD Files
void D_AddFile(const char *file, int li_namespace, FILE *fp, size_t baseoffset,
               dafflags_e addflags);
void D_AddDirectory(const char *dir);
void D_ListWads();
void D_NewWadLumps(int source);
bool D_AddNewFile(const char *s);

// GFS Scripts
void D_ProcessGFSDeh(gfs_t *gfs);
void D_ProcessGFSWads(gfs_t *gfs);
void D_ProcessGFSCsc(gfs_t *gfs);

// Drag-and-Drop Support
void D_LooseWads();
void D_LooseDehs();
gfs_t *D_LooseGFS();
const char *D_LooseDemo();
bool D_LooseEDF(char **buffer);

// EDF
void D_LoadEDF(gfs_t *gfs);

// Base, User, and Game Paths
extern bool gamepathset;

void D_SetGamePath();
void D_SetBasePath();
void D_SetUserPath();
void D_SetAutoDoomPath();	// IOANCH
void D_CheckGamePathParam();
void D_EnumerateAutoloadDir();
void D_GameAutoloadWads();
void D_GameAutoloadDEH();
void D_GameAutoloadCSC();
void D_CloseAutoloadDir();

char *D_CheckAutoDoomPathFile(const char *name, bool isDir);	// IOANCH

#endif

// EOF

