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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Resource archive file formats
//
//-----------------------------------------------------------------------------

#ifndef W_FORMATS_H__
#define W_FORMATS_H__

class qstring;

FILE *W_TryOpenFile(qstring &filename, bool allowInexact);

typedef enum
{
   W_FORMAT_WAD,  // id wadlink file (IWAD or PWAD)
   W_FORMAT_ZIP,  // PKZip archive (aka pk3)
   W_FORMAT_FILE, // An ordinary flat physical file
   W_FORMAT_DIR,  // PKE-like directory
   
   W_FORMAT_MAX   // Keep this last
} WResourceFmt;

WResourceFmt W_DetermineFileFormat(FILE *f, long baseoffset);

void W_LumpNameFromFilePath(const char *input, char output[9], int li_namespace);
int  W_NamespaceForFilePath(const char *path);

#endif

// EOF

