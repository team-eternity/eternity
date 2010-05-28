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

#include "z_zone.h"
#include "i_system.h"
#include "m_swap.h"
#include "m_misc.h"
#include "d_diskfile.h"

//
// Internal disk file entry structure
//
typedef struct diskentry_s
{
   char name[64];
   size_t offset;
   size_t length;
} diskentry_t;

//
// Internal diskfile_t structure
//
typedef struct diskfileint_s
{
   FILE *f;              // physical disk file
   size_t numfiles;      // number of files
   diskentry_t *entries; // directory entries
} diskfileint_t;

//
// D_readDiskFileDirectory
//
// Reads the disk file directory structure.
//
static boolean D_readDiskFileDirectory(diskfileint_t *dfi)
{
   size_t i;
   uint32_t temp;

   for(i = 0; i < dfi->numfiles; ++i)
   {
      diskentry_t *ent = &(dfi->entries[i]);

      if(fread(ent->name, sizeof(ent->name), 1, dfi->f) < 1) // filename
         return false;

      // lower-case names
      M_Strlwr(ent->name);

      if(fread(&temp, sizeof(temp), 1, dfi->f) < 1)          // offset
         return false;
      ent->offset = SwapBigULong(temp);

      if(fread(&temp, sizeof(temp), 1, dfi->f) < 1)          // length
         return false;
      ent->length = SwapBigULong(temp);
   }

   return true;
}

//
// D_OpenDiskFile
//
// Opens and reads a disk file.
//
diskfile_t *D_OpenDiskFile(const char *filename)
{
   diskfile_t *df;
   diskfileint_t *dfi;
   uint32_t temp;

   // allocate a diskfile structure and internal fields
   df  = calloc(1, sizeof(diskfile_t));
   dfi = calloc(1, sizeof(diskfileint_t));

   df->opaque = dfi;

   // open the physical file
   if(!(dfi->f = fopen(filename, "rb")))
      I_Error("D_OpenDiskFile: couldn't open file %s\n", filename);

   // read number of files in the directory
   if(fread(&temp, sizeof(temp), 1, dfi->f) < 1)
      I_Error("D_OpenDiskFile: couldn't read number of entries\n");

   // make big-endian file value host-endian
   dfi->numfiles = SwapBigULong(temp);

   // allocate directory
   dfi->entries = calloc(dfi->numfiles, sizeof(diskentry_t));

   // read directory entries
   if(!D_readDiskFileDirectory(dfi))
      I_Error("D_OpenDiskFile: failed reading directory\n");

   return df;
}

//
// D_FindWadInDiskFile
//
// Looks for a file of the given filename in the diskfile structure. If it is
// found, the internal file pointer will be returned, and the offset of the wad
// within the file will be placed in *offset.
//
// Otherwise, NULL is returned and *offset is not modified.
//
FILE *D_FindWadInDiskFile(diskfile_t *df, const char *filename, size_t *offset)
{
   size_t i;
   diskfileint_t *dfi = df->opaque;
   FILE *ret = NULL;
   char *name = strdup(filename);

   // lower-case a copy of the filename for case-insensitive substring comparison
   M_Strlwr(name);

   for(i = 0; i < dfi->numfiles; ++i)
   {
      diskentry_t *entry = &(dfi->entries[i]);

      // if the filename is contained as a substring, and this entry is
      // a .wad file, return it.
      if(strstr(entry->name, ".wad") && strstr(entry->name, name))
      {
         // entry offsets are relative to the end of the header
         *offset = entry->offset + 8 + 72 * dfi->numfiles;
         ret = dfi->f;
         break;
      }
   }

   free(name);

   return ret;
}

//
// D_CloseDiskFile
//
// Destroys a diskfile_t structure. The physical file will not be closed
// unless "closefile" is true, since it may be in use by one or more
// wad subfiles created on top of the physical file.
//
void D_CloseDiskFile(diskfile_t *df, boolean closefile)
{
   diskfileint_t *dfi = df->opaque;

   if(dfi)
   {
      if(dfi->f && closefile)
      {
         fclose(dfi->f);
         dfi->f = NULL;
      }

      // free the entries
      if(dfi->entries)
      {
         free(dfi->entries);
         dfi->entries = NULL;
      }

      // free the internal structure
      free(dfi);
      df->opaque = NULL;
   }

   // free the structure itself
   free(df);
}

// EOF

