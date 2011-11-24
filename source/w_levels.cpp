// Emacs style mode select   -*- C++ -*-
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

#include "c_io.h"
#include "c_runcmd.h"
#include "d_gi.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "e_hash.h"
#include "m_dllist.h"
#include "m_misc.h"
#include "mn_engin.h"
#include "mn_files.h"
#include "p_setup.h"
#include "v_misc.h"
#include "v_video.h"
#include "w_levels.h"
#include "w_wad.h"

//=============================================================================
// 
// Externs
//

extern int defaultskill;

void D_AddFile(const char *file, int li_namespace, FILE *fp, size_t baseoffset,
               int privatedir);

void G_DeferedInitNewFromDir(skill_t skill, const char *levelname, WadDirectory *dir);

//=============================================================================
//
// Structures
//

//
// Managed wad directory structure.
// This adds hashability to a WadDirectory object.
//
struct manageddir_t
{
   DLListItem<manageddir_t> links; // links
   char                    *name;  // name

   WadDirectory  waddir; // directory
   wadlevel_t   *levels; // enumerated levels
};

//=============================================================================
//
// Static Globals
//

// hash table
static EHashTable<manageddir_t, EStringHashKey> w_dirhash(&manageddir_t::name,
                                                          &manageddir_t::links);

//=============================================================================
//
// Implementation
//

//
// W_addManagedDir
//
// Adds a new managed wad directory. Returns the new directory object.
//
static manageddir_t *W_addManagedDir(const char *filename)
{
   manageddir_t *newdir = NULL;

   // initialize hash table if first time
   if(!w_dirhash.isInitialized())
      w_dirhash.Initialize(31);

   // make sure there isn't one by this name already
   if(w_dirhash.objectForKey(filename))
      return NULL;

   newdir = ecalloc(manageddir_t *, 1, sizeof(manageddir_t));

   newdir->name = estrdup(filename);

   // set type information
   newdir->waddir.SetType(WadDirectory::MANAGED); // mark as managed
   newdir->waddir.SetData(newdir); // opaque pointer back to parent

   // add it to the hash table
   w_dirhash.addObject(newdir);

   return newdir;
}

//
// W_delManagedDir
//
// Destroys a managed directory
//
static void W_delManagedDir(manageddir_t *dir)
{
   WadDirectory &waddir = dir->waddir;

   // close the wad file if it is open
   waddir.Close();

   // remove managed directory from the hash table
   w_dirhash.removeObject(dir);

   // free directory filename
   if(dir->name)
   {
      efree(dir->name); 
      dir->name = NULL;
   }

   // free list of levels
   if(dir->levels)
   {
      efree(dir->levels);
      dir->levels = NULL;
   }

   // free directory
   efree(dir);
}

//
// W_openWadFile
//
// Tries to open a wad file. Returns true if successful, and false otherwise.
//
static bool W_openWadFile(manageddir_t *dir)
{
   bool ret;
   
   if((ret = !!dir->waddir.AddNewPrivateFile(dir->name)))
      D_AddFile(dir->name, lumpinfo_t::ns_global, NULL, 0, 1);

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
WadDirectory *W_AddManagedWad(const char *filename)
{
   manageddir_t *newdir = NULL;

   // Haha, yeah right, you wanker :P
   // At least be smart enough to ju4r3z if nothing else.
   if(GameModeInfo->flags & GIF_SHAREWARE)
      return NULL;

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

   // 10/23/10: enumerate all levels in the directory
   newdir->levels = W_FindAllMapsInLevelWad(&newdir->waddir);

   // success!
   return &(newdir->waddir);
}

//
// W_CloseManagedWad
//
// Closes a managed wad directory. Returns true if anything was actually done.
//
bool W_CloseManagedWad(const char *filename)
{
   manageddir_t *dir = NULL;
   bool retcode = false;

   if((dir = w_dirhash.objectForKey(filename)))
   {
      W_delManagedDir(dir);
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
WadDirectory *W_GetManagedWad(const char *filename)
{
   manageddir_t *dir = NULL;
   WadDirectory *waddir = NULL;

   if((dir = w_dirhash.objectForKey(filename)))
      waddir = &(dir->waddir);

   return waddir;
}

//
// W_GetManagedDirFN
//
// If the given WadDirectory is a managed directory, the file name corresponding
// to it will be returned. Otherwise, NULL is returned.
//
const char *W_GetManagedDirFN(WadDirectory *waddir)
{
   const char *name = NULL;
   void *data = waddir->GetData();

   if(data && waddir->GetType() == WadDirectory::MANAGED)
      name = ((manageddir_t *)data)->name;

   return name;
}

//
// W_FindMapInLevelWad
//
// Finds the first MAPxy or ExMy map in the given wad directory, assuming it
// is a single-level wad. Returns NULL if no such map exists. Note that the
// map is not guaranteed to be valid; code in P_SetupLevel is expected to deal
// with that possibility.
//
char *W_FindMapInLevelWad(WadDirectory *dir, bool mapxy)
{
   int i;
   char *name = NULL;
   int          numlumps = dir->GetNumLumps();
   lumpinfo_t **lumpinfo = dir->GetLumpInfo();

   for(i = 0; i < numlumps; ++i)
   {
      lumpinfo_t *lump = lumpinfo[i];

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

//
// W_sortLevels
//
// Static qsort callback for W_FindAllMapsInLevelWad
//
static int W_sortLevels(const void *first, const void *second)
{
   wadlevel_t *firstLevel  = (wadlevel_t *)first;
   wadlevel_t *secondLevel = (wadlevel_t *)second;

   return strncasecmp(firstLevel->header, secondLevel->header, 9);
}

//
// W_FindAllMapsInLevelWad
//
// haleyjd 10/23/10: Finds all valid maps in a wad directory and returns them
// as a sorted set of wadlevel_t's.
//
wadlevel_t *W_FindAllMapsInLevelWad(WadDirectory *dir)
{
   int i, format;
   wadlevel_t *levels = NULL;
   int numlevels;
   int numlevelsalloc;
   int          numlumps = dir->GetNumLumps();
   lumpinfo_t **lumpinfo = dir->GetLumpInfo();

   // start out with a small set of levels
   numlevels = 0;
   numlevelsalloc = 8;
   levels = ecalloc(wadlevel_t *, numlevelsalloc, sizeof(wadlevel_t));

   // find all the lumps
   for(i = 0; i < numlumps; i++)
   {
      if((format = P_CheckLevel(dir, i)) != LEVEL_FORMAT_INVALID)
      {
         // grow the array if needed, leaving one at the end
         if(numlevels + 1 >= numlevelsalloc)
         {
            numlevelsalloc *= 2;
            levels = erealloc(wadlevel_t *, levels, numlevelsalloc * sizeof(wadlevel_t));
         }
         memset(&levels[numlevels], 0, sizeof(wadlevel_t));
         levels[numlevels].dir = dir;
         levels[numlevels].lumpnum = i;
         strncpy(levels[numlevels].header, lumpinfo[i]->name, 9);
         ++numlevels;

         // skip past the level's directory entries
         i += (format == LEVEL_FORMAT_HEXEN ? 11 : 10);
      }
   }

   // sort the levels if necessary
   if(numlevels > 1)
      qsort(levels, numlevels, sizeof(wadlevel_t), W_sortLevels);

   // set the entry at numlevels to all zeroes as a terminator
   memset(&levels[numlevels], 0, sizeof(wadlevel_t));

   return levels;
}

//
// W_FindLevelInDir
//
// haleyjd 10/23/2010: Looks for a level by the given name in the wad directory
// and returns its wadlevel_t object if it is present. Returns NULL otherwise.
//
wadlevel_t *W_FindLevelInDir(WadDirectory *waddir, const char *name)
{
   wadlevel_t *retlevel = NULL, *curlevel = NULL;
   void *data = waddir->GetData();

   if(data && waddir->GetType() == WadDirectory::MANAGED)
   {
      // get the managed directory
      manageddir_t *dir = (manageddir_t *)data;
      curlevel = dir->levels;

      // loop on the levels list so long as the header names are valid
      while(*curlevel->header)
      {
         if(!strncasecmp(curlevel->header, name, 9))
         {
            // found it
            retlevel = curlevel;
            break;
         }
         // step to the next level
         ++curlevel;
      }
   }

   return retlevel;
}

//=============================================================================
//
// Master Levels
//

// globals
char *w_masterlevelsdirname;
bool inmasterlevels;            // true if we are playing master levels

// statics
static mndir_t masterlevelsdir; // menu file loader directory structure
static bool masterlevelsenum;   // if true, the folder has been enumerated

//
// W_loadMasterLevelWad
//
// Loads a managed wad using the Master Levels directory setting.
//
static WadDirectory *W_loadMasterLevelWad(const char *filename)
{
   char *fullpath = NULL;
   int len = 0;
   WadDirectory *dir = NULL;
   
   if(!w_masterlevelsdirname || !*w_masterlevelsdirname)
      return NULL;

   // construct full file path
   fullpath = M_SafeFilePath(w_masterlevelsdirname, filename);

   // make sure it wasn't already opened
   if((dir = W_GetManagedWad(fullpath)))
      return dir;

   // otherwise, add it now
   return W_AddManagedWad(fullpath);
}

//
// W_doMasterLevelsStart
//
// Command handling for starting a level from the Master Levels wad selection
// menu. Executed by w_startlevel console command.
//
static void W_doMasterLevelsStart(const char *filename, const char *levelname)
{
   WadDirectory *dir = NULL;
   const char *mapname = NULL;

   // Try to load the indicated wad from the Master Levels directory
   if(!(dir = W_loadMasterLevelWad(filename)))
   {
      if(menuactive)
         MN_ErrorMsg("Could not load wad");
      else
         C_Printf(FC_ERROR "Could not load level %s\n", filename);
      return;
   }

   void *data = dir->GetData();

   // Find the first map in the wad file
   if(data && dir->GetType() == WadDirectory::MANAGED)
   {
      // if levelname is valid, try to find that particular map
      if(levelname && W_FindLevelInDir(dir, levelname))
         mapname = levelname;
      else
         mapname = ((manageddir_t *)data)->levels[0].header;
   }
   else
      mapname = W_FindMapInLevelWad(dir, !!(GameModeInfo->flags & GIF_MAPXY));

   // none??
   if(!mapname || !*mapname)
   {
      if(menuactive)
         MN_ErrorMsg("No maps found in wad");
      else
         C_Printf(FC_ERROR "No maps found in wad %s\n", filename);
      return;
   }

   // Got one. Start playing it!
   MN_ClearMenus();
   G_DeferedInitNewFromDir((skill_t)(defaultskill - 1), mapname, dir);

   // set inmasterlevels - this is even saved in savegames :)
   inmasterlevels = true;
}

//
// W_EnumerateMasterLevels
//
// Enumerates the Master Levels directory. Call this at least once so that
// opendir/readdir are not required every time the file dialog menu widget
// is opened.
//
// Set forceRefresh to true to cause the listing to be updated even if it
// was previously cached. This is called from the console variable handler
// for master_levels_dir when the value is successfully changed.
//
void W_EnumerateMasterLevels(bool forceRefresh)
{
   if(masterlevelsenum && !forceRefresh)
      return;

   if(!w_masterlevelsdirname || !*w_masterlevelsdirname)
   {
      C_Printf(FC_ERROR "Set master_levels_dir first!\n");
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

//
// W_DoMasterLevels
//
// Command handling for displaying the Master Levels menu widget.
// If allowexit is false, the menu filebox widget will not allow
// an exit via menu_toggle or menu_previous actions. This is required
// when bringing the menu back up after the intermission, because
// otherwise the player would get stuck (this is done in G_WorldDone
// if "inmasterlevels" is true).
//
void W_DoMasterLevels(bool allowexit)
{
   W_EnumerateMasterLevels(false);

   if(!masterlevelsenum)
   {
      if(menuactive)
         MN_ErrorMsg("Could not list directory");
      else
         C_Printf(FC_ERROR "w_masterlevels: could not list directory\n");
      return;
   }

   MN_DisplayFileSelector(&masterlevelsdir, 
                          "Select a Master Levels WAD:", 
                          "w_startlevel", true, allowexit);
}

//=============================================================================
//
// Console Commands
//

//
// w_masterlevels
//
// Shows the Master Levels menu, assuming master_levels_dir is properly 
// configured.
//
CONSOLE_COMMAND(w_masterlevels, cf_notnet)
{
   W_DoMasterLevels(true);
}

//
// w_startlevel
// 
// Executed by the menu filebox widget when displaying the Master Levels
// directory listing, in order to load and start the proper map.
//
CONSOLE_COMMAND(w_startlevel, cf_notnet)
{
   const char *levelname = NULL;

   if(Console.argc < 1)
      return;

   // 10/24/10: support a level name as argument 2
   if(Console.argc >= 2)
      levelname = Console.argv[1]->constPtr();

   W_doMasterLevelsStart(Console.argv[0]->constPtr(), levelname);
}

//
// W_AddCommands
//
// Adds all managed wad directory and Master Levels commands. Note that the
// master_levels_dir cvar is in g_cmd along with the IWAD settings, because it
// needs to use some of the same code they use for path verification.
//
void W_AddCommands(void)
{
   C_AddCommand(w_masterlevels);
   C_AddCommand(w_startlevel);
}

// EOF

