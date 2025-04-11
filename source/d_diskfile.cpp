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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//    Code to deal with disk files.
//    
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "m_swap.h"
#include "m_utils.h"
#include "d_diskfile.h"

//
// Internal disk file entry structure
//
struct diskentry_t
{
   char name[64];
   size_t offset;
   size_t length;
};

//
// Internal diskfile_t structure
//
struct diskfileint_t
{
   FILE *f;              // physical disk file
   size_t numfiles;      // number of files
   diskentry_t *entries; // directory entries
};

//
// D_readDiskFileDirectory
//
// Reads the disk file directory structure.
//
static bool D_readDiskFileDirectory(diskfileint_t *dfi)
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

      // adjust offset from file to account for header
      ent->offset += 8 + 72 * dfi->numfiles;

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
   df  = estructalloc(diskfile_t,    1);
   dfi = estructalloc(diskfileint_t, 1);

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
   dfi->entries = estructalloc(diskentry_t, dfi->numfiles);

   // read directory entries
   if(!D_readDiskFileDirectory(dfi))
      I_Error("D_OpenDiskFile: failed reading directory\n");

   return df;
}

//
// D_FindWadInDiskFile
//
// Looks for a file of the given filename in the diskfile structure. If it is
// found, the diskwad_t returned will have a valid file pointer and a positive
// offset member. Otherwise, both will be zero.
//
diskwad_t D_FindWadInDiskFile(diskfile_t *df, const char *filename)
{
   size_t i;
   diskwad_t wad;
   diskfileint_t *dfi = static_cast<diskfileint_t *>(df->opaque);
   char *name = estrdup(filename);

   memset(&wad, 0, sizeof(wad));

   // lower-case a copy of the filename for case-insensitive substring comparison
   M_Strlwr(name);

   for(i = 0; i < dfi->numfiles; ++i)
   {
      diskentry_t *entry = &(dfi->entries[i]);

      // if the filename is contained as a substring, and this entry is
      // a .wad file, return it.
      if(strstr(entry->name, ".wad") && strstr(entry->name, name))
      {
         wad.offset = entry->offset;
         wad.f      = dfi->f;
         wad.name   = entry->name;         
         break;
      }
   }

   efree(name);

   return wad;
}

//
// D_CacheDiskFileResource
//
// Loads a resource from the disk file given the absolute path name.
// Returns an allocated buffer holding the data, or nullptr if the path
// does not exist. If "text" is set to true, the buffer will be allocated
// at size+1 and null-terminated, so that it can be treated as a C string.
//
void *D_CacheDiskFileResource(diskfile_t *df, const char *path, bool text)
{
   size_t i, len;
   diskfileint_t *dfi = static_cast<diskfileint_t *>(df->opaque);
   diskentry_t *entry = nullptr;
   void *buffer;

   // find the resource
   for(i = 0; i < dfi->numfiles; ++i)
   {
      if(!strcasecmp(dfi->entries[i].name, path))
      {
         entry = &(dfi->entries[i]);
         break;
      }
   }

   // return if not found
   if(!entry)
      return nullptr;

   len = entry->length;

   // increment the buffer length by one if we are loading a text resource
   if(text)
      len++;

   // allocate a buffer, read the resource, and return it
   buffer = ecalloc(void *, 1, len);

   if(fseek(dfi->f, static_cast<long>(entry->offset), SEEK_SET))
      I_Error("D_CacheDiskFileResource: can't seek to resource %s\n", entry->name);

   if(fread(buffer, entry->length, 1, dfi->f) < 1)
      I_Error("D_CacheDiskFileResource: can't read resource %s\n", entry->name);

   return buffer;
}

//
// D_CloseDiskFile
//
// Destroys a diskfile_t structure. The physical file will not be closed
// unless "closefile" is true, since it may be in use by one or more
// wad subfiles created on top of the physical file.
//
void D_CloseDiskFile(diskfile_t *df, bool closefile)
{
   diskfileint_t *dfi = static_cast<diskfileint_t *>(df->opaque);

   if(dfi)
   {
      if(dfi->f && closefile)
      {
         fclose(dfi->f);
         dfi->f = nullptr;
      }

      // free the entries
      if(dfi->entries)
      {
         efree(dfi->entries);
         dfi->entries = nullptr;
      }

      // free the internal structure
      efree(dfi);
      df->opaque = nullptr;
   }

   // free the structure itself
   efree(df);
}

// EOF

