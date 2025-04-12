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
// Purpose: Code to deal with disk files.
// Authors: James Haley
//

#ifndef D_DISK_H__
#define D_DISK_H__

//
// diskfile
//
// Represents a specific type of "disk" file format which has 72-byte entries
// in the header for each file, and 2 words of other information including the
// number of files. This is an opaque type, similar to a FILE *.
//
struct diskfile_t
{
   void *opaque;
};

//
// diskwad
//
// This struct is returned from D_FindWadInDiskFile and contains information on a
// logical wad file found in the physical disk file.
//
struct diskwad_t
{
   FILE *f;          // file pointer
   size_t offset;    // offset of wad within physical file
   const char *name; // canonical file name
};

diskfile_t *D_OpenDiskFile(const char *filename);
diskwad_t D_FindWadInDiskFile(diskfile_t *df, const char *filename);
void *D_CacheDiskFileResource(diskfile_t *df, const char *path, bool text);
void D_CloseDiskFile(diskfile_t *df, bool closefile);

#endif

// EOF

