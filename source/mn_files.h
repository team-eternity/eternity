//
// Copyright(C) 2019 Simon Howard, James Haley, et al.
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
// Purpose: Menu file selector
//  eg. For selecting a wad to load or demo to play
//
// Authors: Simon Howard, James Haley
//

#ifndef MN_FILES_H__
#define MN_FILES_H__

struct mndir_t
{
   const char *dirpath; // physical file system path of directory
   char **filenames;    // array of file names
   int  numfiles;       // number of files
   int  numfilesalloc;  // number of files allocated
};

extern char *wad_directory;
extern char *mn_wadname;

int MN_ReadDirectory(mndir_t *dir, const char *read_dir, 
                     const char *const *read_wildcards, 
                     size_t numwildcards, bool allowsubdirs);
void MN_ClearDirectory(mndir_t *dir);

void MN_DisplayFileSelector(mndir_t *dir, const char *title, 
                            const char *command, bool dismissOnSelect,
                            bool allowExit);

#endif

// EOF

