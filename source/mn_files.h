// Emacs style mode select -*- C -*-
//---------------------------------------------------------------------------
//
// Copyright(C) 2006 Simon Howard, James Haley
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
// Menu file selector
//
// eg. For selecting a wad to load or demo to play
//
// By Simon Howard
// Revisions by James Haley
//
//---------------------------------------------------------------------------

#ifndef MN_FILES_H__
#define MN_FILES_H__

typedef struct mndir_s
{
   const char *dirpath; // physical file system path of directory
   char **filenames;    // array of file names
   int  numfiles;       // number of files
   int  numfilesalloc;  // number of files allocated
} mndir_t;

extern char *wad_directory;
void MN_File_AddCommands(void);

int MN_ReadDirectory(mndir_t *dir, const char *read_dir, 
                     const char *read_wildcard);
void MN_ClearDirectory(mndir_t *dir);

void MN_DisplayFileSelector(mndir_t *dir, const char *title, 
                            const char *command, boolean dismissOnSelect,
                            boolean allowExit);

#endif

// EOF

