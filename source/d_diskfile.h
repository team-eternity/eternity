// Emacs style mode select   -*- C -*- 
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//    Code to deal with disk files.
//    
//-----------------------------------------------------------------------------

#ifndef D_DISK_H__
#define D_DISK_H__

typedef struct diskfile_s
{
   void *opaque;
} diskfile_t;

diskfile_t *D_OpenDiskFile(const char *filename);
FILE *D_FindWadInDiskFile(diskfile_t *df, const char *filename, size_t *offset);
void D_CloseDiskFile(diskfile_t *df, boolean closefile);

#endif

// EOF

