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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Management of private wad directories for individual level resources
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "doomtype.h"
#include "w_wad.h"
#include "w_levels.h"
#include "m_dllist.h"
#include "e_hash.h"

//=============================================================================
//
// Structures
//

//
// Managed wad directory structure.
// This adds hashability to a waddir_t structure.
//
typedef struct manageddir_s
{
   mdllistitem_t  links;  // links
   waddir_t       waddir; // directory
   char          *name;   // name
} manageddir_t;

//=============================================================================
//
// Static Globals
//

// hash table
static ehash_t w_dirhash;

//=============================================================================
//
// Implementation
//

// key function for manageddir_t
E_KEYFUNC(manageddir_t, name)

//
// W_addManagedDir
//
// Adds a new managed wad directory. Returns the new directory object.
//
static manageddir_t *W_addManagedDir(const char *filename)
{
   manageddir_t *newdir = NULL;

   // initialize hash table if first time
   if(!w_dirhash.isinit)
      E_StrHashInit(&w_dirhash, 31, E_KEYFUNCNAME(manageddir_t, name), NULL);

   // make sure there isn't one by this name already
   if(E_HashObjectForKey(&w_dirhash, &filename))
      return NULL;

   newdir = calloc(1, sizeof(manageddir_t));

   newdir->name = strdup(filename);

   // add it to the hash table
   E_HashAddObject(&w_dirhash, newdir);

   return newdir;
}

//
// W_delManagedDir
//
// Destroys a managed directory
//
static void W_delManagedDir(manageddir_t *dir)
{
   waddir_t *waddir = &(dir->waddir);

   // close the wad file if it is open
   if(waddir->lumpinfo)
   {
      // free all resources loaded from the wad
      W_FreeDirectoryLumps(waddir);

      if(waddir->lumpinfo[0]->file)
         fclose(waddir->lumpinfo[0]->file);

      // free all lumpinfo_t's allocated for the wad
      W_FreeDirectoryAllocs(waddir);

      // free the private wad directory
      Z_Free(waddir->lumpinfo);

      waddir->lumpinfo = NULL;
   }

   // remove managed directory from the hash table
   E_HashRemoveObject(&w_dirhash, dir);

   // free directory filename
   if(dir->name)
   {
      free(dir->name);
      dir->name = NULL;
   }

   // free directory
   free(dir);
}

//
// W_openWadFile
//
// Tries to open a wad file. Returns true if successful, and false otherwise.
//
static boolean W_openWadFile(manageddir_t *dir)
{
   return !!W_AddNewPrivateFile(&dir->waddir, dir->name);
}

//=============================================================================
//
// Interface
//

//
// W_AddManagedWad
//
// Adds a managed directory object and opens a wad file inside of it.
// Returns true or false to indicate success.
//
boolean W_AddManagedWad(const char *filename)
{
   manageddir_t *newdir = NULL;

   // create a new managed wad directory object
   if(!(newdir = W_addManagedDir(filename)))
      return false;

   // open the wad file in the new directory
   if(!W_openWadFile(newdir))
   {
      // if failed, delete the new directory object
      W_delManagedDir(newdir);
      return false;
   }

   // success!
   return true;
}

//
// W_CloseManagedWad
//
// Closes a managed wad directory. Returns true if anything was actually done.
//
boolean W_CloseManagedWad(const char *filename)
{
   void    *object = NULL;
   boolean retcode = false;

   if((object = E_HashObjectForKey(&w_dirhash, &filename)))
   {
      W_delManagedDir((manageddir_t *)object);
      retcode = true;
   }

   return retcode;
}

//
// W_GetManagedWad
//
// Returns a managed wad directory for the given filename, if such exists.
// Returns NULL otherwise.
//
waddir_t *W_GetManagedWad(const char *filename)
{
   void     *object = NULL;
   waddir_t *waddir = NULL;

   if((object = E_HashObjectForKey(&w_dirhash, &filename)))
      waddir = &(((manageddir_t *)object)->waddir);

   return waddir;
}

// EOF

