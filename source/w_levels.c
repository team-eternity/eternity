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
#include "doomdef.h"
#include "w_wad.h"
#include "w_levels.h"
#include "d_gi.h"
#include "m_dllist.h"
#include "e_hash.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "m_misc.h"
#include "mn_files.h"
#include "mn_engin.h"

//=============================================================================
// 
// Externs
//

extern int defaultskill;

void D_AddFile(const char *file, int li_namespace, FILE *fp, size_t baseoffset,
               int privatedir);

void G_DeferedInitNewFromDir(skill_t skill, char *levelname, waddir_t *dir);

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

   // set type information
   newdir->waddir.type = WADDIR_MANAGED; // mark as managed
   newdir->waddir.data = newdir;         // opaque pointer back to parent

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
   boolean ret;
   
   if((ret = W_AddNewPrivateFile(&dir->waddir, dir->name)))
      D_AddFile(dir->name, ns_global, NULL, 0, 1);

   return ret;
}

//=============================================================================
//
// Interface
//

//
// W_AddManagedWad
//
// Adds a managed directory object and opens a wad file inside of it.
// Returns the new waddir if one was created, or else returns NULL.
//
waddir_t *W_AddManagedWad(const char *filename)
{
   manageddir_t *newdir = NULL;

   // create a new managed wad directory object
   if(!(newdir = W_addManagedDir(filename)))
      return NULL;

   // open the wad file in the new directory
   if(!W_openWadFile(newdir))
   {
      // if failed, delete the new directory object
      W_delManagedDir(newdir);
      return NULL;
   }

   // success!
   return &(newdir->waddir);
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

//
// W_GetManagedDirFN
//
// If the given waddir_t is a managed directory, the file name corresponding
// to it will be returned. Otherwise, NULL is returned.
//
const char *W_GetManagedDirFN(waddir_t *waddir)
{
   const char *name = NULL;

   if(waddir->data && waddir->type == WADDIR_MANAGED)
      name = ((manageddir_t *)(waddir->data))->name;

   return name;
}

//=============================================================================
//
// Master Levels
//

// globals
char *w_masterlevelsdirname;
boolean inmasterlevels;          // true if we are playing master levels

// statics
static mndir_t masterlevelsdir;  // menu file loader directory structure
static boolean masterlevelsenum; // if true, the folder has been enumerated

void W_EnumerateMasterLevels(void)
{
   if(masterlevelsenum)
      return;

   if(!w_masterlevelsdirname || !*w_masterlevelsdirname)
   {
      C_Printf(FC_ERROR "Set master_levels_dir first\n");
      return;
   }

   if(MN_ReadDirectory(&masterlevelsdir, w_masterlevelsdirname, "*.wad"))
   {
      C_Printf(FC_ERROR "Could not enumerate Master Levels directory: %s\n",
               errno ? strerror(errno) : "(unknown error)");
      return;
   }
   
   if(masterlevelsdir.numfiles > 0)
      masterlevelsenum = true;
}

waddir_t *W_LoadMasterLevelWad(const char *filename)
{
   char *fullpath = NULL;
   int len = 0;
   waddir_t *dir = NULL;
   
   if(!w_masterlevelsdirname || !*w_masterlevelsdirname)
      return NULL;

   // construct full file path
   len = M_StringAlloca(&fullpath, 2, 2, w_masterlevelsdirname, filename);

   psnprintf(fullpath, len, "%s/%s", w_masterlevelsdirname, filename);

   // make sure it wasn't already opened
   if((dir = W_GetManagedWad(fullpath)))
      return dir;

   // otherwise, add it now
   return W_AddManagedWad(fullpath);
}

//
// W_FindMapInLevelWad
//
// Finds the first MAPxy or ExMy map in the given wad directory, assuming it
// is a single-level wad. Returns NULL if no such map exists. Note that the
// map is not guaranteed to be valid; code in P_SetupLevel is expected to deal
// with that possibility.
//
char *W_FindMapInLevelWad(waddir_t *dir, boolean mapxy)
{
   int i;
   char *name = NULL;

   for(i = 0; i < dir->numlumps; ++i)
   {
      lumpinfo_t *lump = dir->lumpinfo[i];

      if(mapxy)
      {
         if(isMAPxy(lump->name))
            name = lump->name;
      }
      else if(isExMy(lump->name))
         name = lump->name;
   }

   return name;
}

void W_DoMasterLevels(boolean allowexit)
{
   W_EnumerateMasterLevels();

   if(!masterlevelsenum)
   {
      if(menuactive)
         MN_ErrorMsg("Could not list directory");
      return;
   }

   MN_DisplayFileSelector(&masterlevelsdir, 
                          "Select a Master Levels WAD:", 
                          "w_startlevel", true, allowexit);
}

//=============================================================================
//
// File Selection
//

CONSOLE_COMMAND(w_masterlevels, cf_notnet)
{
   W_DoMasterLevels(true);
}

CONSOLE_COMMAND(w_startlevel, cf_notnet|cf_hidden)
{
   waddir_t *dir = NULL;
   char *mapname = NULL;

   if(Console.argc < 1)
      return;

   if(!(dir = W_LoadMasterLevelWad(Console.argv[0])))
   {
      if(menuactive)
         MN_ErrorMsg("Could not load wad");
      else
         C_Printf(FC_ERROR "Could not load level %s\n", Console.argv[0]);
      return;
   }

   mapname = W_FindMapInLevelWad(dir, !!(GameModeInfo->flags & GIF_MAPXY));

   if(!mapname)
   {
      if(menuactive)
         MN_ErrorMsg("No maps found in wad");
      else
         C_Printf(FC_ERROR "No maps found in wad %s\n", Console.argv[0]);
      return;
   }

   MN_ClearMenus();
   G_DeferedInitNewFromDir(defaultskill - 1, mapname, dir);

   inmasterlevels = true;
}

void W_AddCommands(void)
{
   C_AddCommand(w_masterlevels);
   C_AddCommand(w_startlevel);
}

// EOF

