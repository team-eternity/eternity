// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//    Code for loading data files, particularly PWADs.
//
//-----------------------------------------------------------------------------

#ifndef D_FILES_H__
#define D_FILES_H__

struct gfs_t;

struct wfileadd_t;
extern wfileadd_t *wadfiles;     // killough 11/98
extern char firstlevel[9];

// WAD Files
void D_AddFile(const char *file, int li_namespace, FILE *fp, size_t baseoffset,
               int privatedir);
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

#endif

// EOF

