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
// Purpose: Management of private wad directories for individual level
//  resources.
//
// Authors: James Haley
//

#include "z_zone.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "d_files.h"
#include "d_gi.h"
#include "d_iwad.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "e_hash.h"
#include "g_game.h"
#include "m_dllist.h"
#include "m_utils.h"
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

extern skill_t defaultskill;

//=============================================================================
//
// Structures
//

//
// Managed wad directory structure.
// This adds hashability to a WadDirectory object.
//
class ManagedDirectory : public WadDirectory
{
protected:
   wadlevel_t *levels; // enumerated levels

public:
   DLListItem<ManagedDirectory> links; // links
   char *name;   // name

   ManagedDirectory() : WadDirectory(), levels(nullptr), links(), name(nullptr)
   {
   }
   ~ManagedDirectory();

   static ManagedDirectory *AddManagedDir(const char *filename);
   static ManagedDirectory *DirectoryForName(const char *filename);

   bool openWadFile();
   void enumerateLevels();
   wadlevel_t *findLevel(const char *name);
   const char *getFirstLevelName();

   const char *getName() const { return name; }   
};

//=============================================================================
//
// Static Globals
//

// hash table
static EHashTable<ManagedDirectory, EStringHashKey, 
                  &ManagedDirectory::name, &ManagedDirectory::links> 
                  w_dirhash(31);

//=============================================================================
//
// Implementation
//

//
// ManagedDirectory::AddManagedDir
//
// Adds a new managed wad directory. Returns the new directory object.
//
ManagedDirectory *ManagedDirectory::AddManagedDir(const char *filename)
{
   ManagedDirectory *newdir = nullptr;

   // make sure there isn't one by this name already
   if(w_dirhash.objectForKey(filename))
      return nullptr;

   newdir = new ManagedDirectory;

   newdir->name = estrdup(filename);

   // set type information
   newdir->setType(WadDirectory::MANAGED); // mark as managed

   // add it to the hash table
   w_dirhash.addObject(newdir);

   return newdir;
}

ManagedDirectory *ManagedDirectory::DirectoryForName(const char *filename)
{
   return w_dirhash.objectForKey(filename);
}

//
// ManagedDirectory Destructor
//
ManagedDirectory::~ManagedDirectory()
{
   // close the wad file if it is open
   close();

   // remove managed directory from the hash table
   w_dirhash.removeObject(this);

   // free directory filename
   if(name)
   {
      efree(name); 
      name = nullptr;
   }

   // free list of levels
   if(levels)
   {
      efree(levels);
      levels = nullptr;
   }
}

//
// ManagedDirectory::openWadFile
//
// Tries to open a wad file. Returns true if successful, and false otherwise.
//
bool ManagedDirectory::openWadFile()
{
   bool ret;
   
   if((ret = addNewPrivateFile(name)))
      D_AddFile(name, lumpinfo_t::ns_global, nullptr, 0, DAF_PRIVATE);

   return ret;
}

void ManagedDirectory::enumerateLevels()
{
   // 10/23/10: enumerate all levels in the directory
   levels = W_FindAllMapsInLevelWad(this);
}

wadlevel_t *ManagedDirectory::findLevel(const char *name)
{
   wadlevel_t *retlevel = nullptr;
   wadlevel_t *curlevel = levels;

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

   return retlevel;
}

const char *ManagedDirectory::getFirstLevelName()
{
   return levels[0].header;
}

//=============================================================================
//
// Interface
//

//
// W_AddManagedWad
//
// Adds a managed directory object and opens a wad file inside of it.
// Returns the new waddir if one was created, or else returns nullptr.
//
WadDirectory *W_AddManagedWad(const char *filename)
{
   ManagedDirectory *newdir = nullptr;

   // Haha, yeah right, you wanker :P
   // At least be smart enough to ju4r3z if nothing else.
   if(GameModeInfo->flags & GIF_SHAREWARE)
      return nullptr;

   // create a new managed wad directory object
   if(!(newdir = ManagedDirectory::AddManagedDir(filename)))
      return nullptr;

   // open the wad file in the new directory
   if(!newdir->openWadFile())
   {
      // if failed, delete the new directory object
      delete newdir;
      return nullptr;
   }

   // 10/23/10: enumerate all levels in the directory
   newdir->enumerateLevels();

   // success!
   return newdir;
}

//
// W_CloseManagedWad
//
// Closes a managed wad directory. Returns true if anything was actually done.
//
// FIXME: unused?
//
/*
static bool W_CloseManagedWad(const char *filename)
{
   ManagedDirectory *dir = nullptr;
   bool retcode = false;

   if((dir = ManagedDirectory::DirectoryForName(filename)))
   {
      delete dir;
      retcode = true;
   }

   return retcode;
}
*/

//
// W_GetManagedWad
//
// Returns a managed wad directory for the given filename, if such exists.
// Returns nullptr otherwise.
//
WadDirectory *W_GetManagedWad(const char *filename)
{
   return ManagedDirectory::DirectoryForName(filename);
}

//
// W_GetManagedDirFN
//
// If the given WadDirectory is a managed directory, the file name corresponding
// to it will be returned. Otherwise, nullptr is returned.
//
const char *W_GetManagedDirFN(WadDirectory *waddir)
{
   const char *name = nullptr;

   if(waddir->getType() == WadDirectory::MANAGED)
   {
      ManagedDirectory *mdir = static_cast<ManagedDirectory *>(waddir);
      name = mdir->getName();
   }

   return name;
}

//
// W_FindMapInLevelWad
//
// Finds the first MAPxy or ExMy map in the given wad directory, assuming it
// is a single-level wad. Returns nullptr if no such map exists. Note that the
// map is not guaranteed to be valid; code in P_SetupLevel is expected to deal
// with that possibility.
//
static const char *W_FindMapInLevelWad(WadDirectory *dir, bool mapxy)
{
   const char *name = nullptr;
   int          numlumps = dir->getNumLumps();
   lumpinfo_t **lumpinfo = dir->getLumpInfo();

   for(int i = 0; i < numlumps; i++)
   {
      lumpinfo_t *lump = lumpinfo[i];

      if(mapxy)
      {
         if(M_IsMAPxy(lump->name, nullptr))
            name = lump->name;
      }
      else if(M_IsExMy(lump->name, nullptr, nullptr))
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
   const wadlevel_t *firstLevel  = static_cast<const wadlevel_t *>(first);
   const wadlevel_t *secondLevel = static_cast<const wadlevel_t *>(second);

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
   wadlevel_t *levels;
   int numlevels;
   int numlevelsalloc;
   int          numlumps = dir->getNumLumps();
   lumpinfo_t **lumpinfo = dir->getLumpInfo();

   // start out with a small set of levels
   numlevels = 0;
   numlevelsalloc = 8;
   levels = estructalloc(wadlevel_t, numlevelsalloc);

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
// and returns its wadlevel_t object if it is present. Returns nullptr otherwise.
//
wadlevel_t *W_FindLevelInDir(WadDirectory *waddir, const char *name)
{
   wadlevel_t *retlevel = nullptr;

   if(waddir->getType() == WadDirectory::MANAGED)
   {
      // get the managed directory
      auto dir = static_cast<ManagedDirectory *>(waddir);
      retlevel = dir->findLevel(name);
   }

   return retlevel;
}

//=============================================================================
//
// Master Levels
//

// globals
char *w_masterlevelsdirname;
int   inmanageddir;          // non-zero if we are playing a managed dir level

// statics
static mndir_t masterlevelsdir;        // menu file loader directory structure
static bool    masterlevelsenum;       // if true, the folder has been enumerated
static int     masterlevelsskill = -1; // skill level

//
// W_loadMasterLevelWad
//
// Loads a managed wad using the Master Levels directory setting.
//
static WadDirectory *W_loadMasterLevelWad(const char *filename)
{
   char *fullpath = nullptr;
   WadDirectory *dir = nullptr;
   
   if(estrempty(w_masterlevelsdirname))
      return nullptr;

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
   WadDirectory *dir = nullptr;
   const char *mapname = nullptr;

   // Try to load the indicated wad from the Master Levels directory
   if(!(dir = W_loadMasterLevelWad(filename)))
   {
      if(menuactive)
         MN_ErrorMsg("Could not load wad");
      else
         C_Printf(FC_ERROR "Could not load level %s\n", filename);
      return;
   }

   // Find the first map in the wad file
   if(dir->getType() == WadDirectory::MANAGED)
   {
      // if levelname is valid, try to find that particular map
      if(levelname && W_FindLevelInDir(dir, levelname))
         mapname = levelname;
      else
      {
         ManagedDirectory *mdir = static_cast<ManagedDirectory *>(dir);
         mapname = mdir->getFirstLevelName();
      }
   }
   else
      mapname = W_FindMapInLevelWad(dir, !!(GameModeInfo->flags & GIF_MAPXY));

   // none??
   if(estrempty(mapname))
   {
      if(menuactive)
         MN_ErrorMsg("No maps found in wad");
      else
         C_Printf(FC_ERROR "No maps found in wad %s\n", filename);
      return;
   }

   // Just in case. We could get here by the user manually using the
   // w_startlevel command, which doesn't faff about with skills.
   if(masterlevelsskill == -1)
      masterlevelsskill = defaultskill - 1;

   // Got one. Start playing it!
   MN_ClearMenus();
   G_DeferedInitNewFromDir((skill_t)masterlevelsskill, mapname, dir);

   // set inmanageddir - this is even saved in savegames :)
   inmanageddir = MD_MASTERLEVELS;
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
   static const char *exts[1] = { "*.wad" };
   if(masterlevelsenum && !forceRefresh)
      return;

   if(estrempty(w_masterlevelsdirname))
   {
      C_Printf(FC_ERROR "Set master_levels_dir first!\n");
      return;
   }

   if(MN_ReadDirectory(&masterlevelsdir, w_masterlevelsdirname, exts, earrlen(exts), false))
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
// an immediate exit via menu_toggle or menu_previous actions.
//
void W_DoMasterLevels(bool allowexit, int skill)
{
   masterlevelsskill = skill;

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
// No Rest for the Living
//
// 11/04/12: The PC version of NR4TL, released along with DOOM 3: BFG Edition,
// can be configured to load as an additional episode with DOOM 2.
//

// globals
char *w_norestpath;

//
// W_loadNR4TL
//
// Loads No Rest for the Living as a managed wad.
//
static WadDirectory *W_loadNR4TL()
{
   WadDirectory *dir = nullptr;
   
   if(estrempty(w_norestpath))
      return nullptr;

   // make sure it wasn't already opened
   if((dir = W_GetManagedWad(w_norestpath)))
      return dir;

   // otherwise, add it now
   return W_AddManagedWad(w_norestpath);
}

//
// W_DoNR4TLStart
//
// Command handling for starting the NR4TL episode.
//
void W_DoNR4TLStart(int skill)
{
   WadDirectory *dir = nullptr;

   // Try to load the NR4TL wad file
   if(!(dir = W_loadNR4TL()))
   {
      if(menuactive)
         MN_ErrorMsg("Could not load wad");
      else
         C_Printf(FC_ERROR "Could not load %s\n", w_norestpath);
      return;
   }

   // Initialize the mission
   W_InitManagedMission(MD_NR4TL);

   // Start playing it!
   MN_ClearMenus();
   G_DeferedInitNewFromDir((skill_t)(skill), "MAP01", dir);

   // set inmanageddir
   inmanageddir = MD_NR4TL;
}

//=============================================================================
//
// Mission Initialization
//

//
// W_initNR4TL
//
// No Rest for the Living requires some metadata to be loaded in order to get
// the proper level transitions, map names, music tracks, and episode ending
// behavior. The level metadata system in p_info only applies records loaded
// this way to maps when the managed dir mission matches the passed-in value,
// so loading this data does not affect the ordinary IWAD maps or interfere
// with any loaded MapInfo data.
//
static void W_initNR4TL()
{
   static bool firsttime = true;

   // Load metadata
   if(firsttime)
   {
      D_MissionMetaData("ENRVMETA", MD_NR4TL);
      firsttime = false;
   }
}

//
// W_InitManagedMission
//
// haleyjd 11/04/12: Managed directory mission packs need certain init tasks
// performed for them any time they get loaded, whether that happens here or
// from the savegame module.
//
void W_InitManagedMission(int mission)
{
   switch(mission)
   {
   case MD_NR4TL:
      W_initNR4TL();
      break;
   default:
      break;
   }
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
   int skill = -1;
   if(Console.argc >= 1)
      skill = Console.argv[0]->toInt();

   W_DoMasterLevels(true, skill);
}

//
// w_startlevel
// 
// Executed by the menu filebox widget when displaying the Master Levels
// directory listing, in order to load and start the proper map.
//
CONSOLE_COMMAND(w_startlevel, cf_notnet)
{
   const char *levelname = nullptr;

   if(Console.argc < 1)
      return;

   // 10/24/10: support a level name as argument 2
   if(Console.argc >= 2)
      levelname = Console.argv[1]->constPtr();

   W_doMasterLevelsStart(Console.argv[0]->constPtr(), levelname);
}

//
// w_playnorest
//
// Start playing No Rest for the Living
//
CONSOLE_COMMAND(w_playnorest, 0)
{
   int skill = defaultskill - 1;

   if(Console.argc >= 1)
      skill = Console.argv[0]->toInt();

   W_DoNR4TLStart(skill);
}

//
// Utility commands
//
// Yeah, they're here because there's no real other place :P
//

// Write out a lump
CONSOLE_COMMAND(w_writelump, 0)
{
   qstring filename;
   const char *lumpname;

   if(Console.argc < 1)
   {
      C_Puts("Usage: w_writelump lumpname");
      return;
   }

   lumpname = Console.argv[0]->constPtr();
   filename = usergamepath;
   filename.pathConcatenate(lumpname);
   filename.addDefaultExtension(".lmp");

   wGlobalDir.writeLump(lumpname, filename.constPtr());
}

// EOF

