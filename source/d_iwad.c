// Emacs style mode select -*- C -*- vi:sw=3 ts=3:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2006 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.
//
// DESCRIPTION:
//     Search for and locate an IWAD file, and initialise according
//     to the IWAD type.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "d_iwad.h"
#include "d_main.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_file.h"
#include "m_misc.h"
#include "w_wad.h"


//
// M_FileExists
//
// Check if a file exists
// haleyjd TODO: move to m_misc.c
//
boolean M_FileExists(const char *filename)
{
   FILE *fstream;

   fstream = fopen(filename, "r");

   if(fstream != NULL)
   {
      fclose(fstream);
      return true;
   }
   else
   {
      // If we can't open because the file is a directory, the
      // "file" exists at least!

      return errno == EISDIR;
   }
}


// Array of locations to search for IWAD files
//
// "128 IWAD search directories should be enough for anybody".

#define MAX_IWAD_DIRS 128

static boolean iwad_dirs_built = false;
static char *iwad_dirs[MAX_IWAD_DIRS];
static int num_iwad_dirs = 0;

void AddIWADDir(char *dir)
{
   if(num_iwad_dirs < MAX_IWAD_DIRS)
   {
      iwad_dirs[num_iwad_dirs] = dir;
      ++num_iwad_dirs;
   }
}

//
// SearchDirectoryForIWAD
//
// Search a directory to try to find an IWAD
// Returns the location of the IWAD if found, otherwise NULL.
//
static char *SearchDirectoryForIWAD(char *dir, const char *const *iwads,
                                    int numiwads)
{
   int i;

   for(i = 0; i < numiwads; ++i)
   {
      char *filename;
      const char *iwadname;

      iwadname = iwads[i];

      filename = malloc(strlen(dir) + strlen(iwadname) + 3);

      if(!strcmp(dir, "."))
         strcpy(filename, iwadname);
      else
         sprintf(filename, "%s/%s", dir, iwadname);

      // haleyjd
      M_NormalizeSlashes(filename);

      if(M_FileExists(filename))
         return filename;

      free(filename);
   }

   return NULL;
}

//
// AddDoomWadPath
//
// Add directories from the list in the DOOMWADPATH environment variable.
//
static void AddDoomWadPath(void)
{
   char *doomwadpath;
   char *p;

   // Check the DOOMWADPATH environment variable.

   doomwadpath = getenv("DOOMWADPATH");

   if(doomwadpath == NULL)
      return;

   doomwadpath = strdup(doomwadpath);

   // Add the initial directory

   AddIWADDir(doomwadpath);

   // Split into individual dirs within the list.

   p = doomwadpath;

   for(;;)
   {
      p = strchr(p, ';');

      if(p != NULL)
      {
         // Break at the separator and store the right hand side
         // as another iwad dir

         *p = '\0';
         p += 1;

         AddIWADDir(p);
      }
      else
         break;
   }
}

// haleyjd: I had to separate this stuff into its own module, i_iwad.c;
// our headers do not get along with windows.h at all.
#ifdef _WIN32
extern void CheckUninstallStrings(void);
extern void CheckCollectorsEdition(void);
extern void CheckSteamEdition(void);
extern void CheckDOSDefaults(void);
#endif

//
// BuildIWADDirList
//
// Build a list of IWAD files
//
static void BuildIWADDirList(void)
{
   char *doomwaddir;
   char *base_wads;

   if(iwad_dirs_built)
      return;

   // Add DOOMWADDIR if it is in the environment

   doomwaddir = getenv("DOOMWADDIR");

   if(doomwaddir != NULL)
      AddIWADDir(doomwaddir);

   // Add dirs from DOOMWADPATH

   AddDoomWadPath();

#ifdef _WIN32

   // Search the registry and find where IWADs have been installed.

   CheckUninstallStrings();
   CheckCollectorsEdition();
   CheckSteamEdition();
   CheckDOSDefaults();

#endif

#ifdef LINUX

   // Standard places where IWAD files are installed under Unix.

   AddIWADDir("/usr/share/games/doom");
   AddIWADDir("/usr/local/share/games/doom");

#endif

   // [CG] Add base/wads.

   base_wads = calloc(strlen(basepath) + 6, sizeof(char));
   sprintf(base_wads, "%s/wads", basepath);
   M_NormalizeSlashes(base_wads);
   if(!M_PathExists(base_wads))
   {
       if(!M_CreateFolder(base_wads))
       {
           I_Error(
               "Error creating base WAD folder %s: %s.\n", 
               base_wads,
               M_GetFileSystemErrorMessage()
           );
       }
   }
   else if(!M_IsFolder(base_wads))
   {
       I_Error(
           "Base WAD folder %s/wads exists, but is not a folder.\n", basepath
       );
   }
   AddIWADDir(base_wads);

   // Don't run this function again.

   iwad_dirs_built = true;
}

//
// D_FindWADByName
//
// Searches WAD search paths for an WAD with a specific filename.
//
char *D_FindWADByName(char *name)
{
   char *buf;
   int i;
   boolean exists;

   printf("D_FindWADByName: Looking for %s.\n", name);

   // Absolute path?
   if(M_FileExists(name))
   {
      printf("D_FindWADByName: Found %s.\n", name);
      return name;
   }

   BuildIWADDirList();

   // Search through all IWAD paths for a file with the given name.

   for(i = 0; i < num_iwad_dirs; ++i)
   {
      // Construct a string for the full path

      buf = malloc(strlen(iwad_dirs[i]) + strlen(name) + 5);
      sprintf(buf, "%s/%s", iwad_dirs[i], name);

      // haleyjd
      M_NormalizeSlashes(buf);

      if(strlen(iwad_dirs[i]) < 50)
      {
         printf(
            "D_FindWADByName: Checking for %s in %s (%s) [%d].\n",
            name,
            iwad_dirs[i],
            buf,
            strlen(iwad_dirs[i])
         );
      }

      exists = M_FileExists(buf);

      if(exists)
      {
         printf("D_FindWADByName: Found %s.\n", buf);
         return buf;
      }
      else
      {
         if(strlen(iwad_dirs[i]) < 50)
            printf("D_FindWADByName: %s does not exist.\n", buf);
      }

      free(buf);
   }

   // File not found

   return NULL;
}

//
// D_TryFindWADByName
//
// Searches for a WAD by its filename, or passes through the filename
// if not found.
//
char *D_TryFindWADByName(char *filename)
{
   char *result;

   result = D_FindWADByName(filename);

   if(result != NULL)
      return result;
   else
      return filename;
}

#if 0
//
// D_FindIWAD
//
// Checks availability of IWAD files by name.
//
char *D_FindIWAD(const char *const *iwads, int numiwads)
{
   char *result;
   char *iwadfile;
   int iwadparm;
   int i;

   // Check for the -iwad parameter
   iwadparm = M_CheckParm("-iwad");

   if(iwadparm && iwadparm < myargc - 1)
   {
      // Search through IWAD dirs for an IWAD with the given name.

      iwadfile = myargv[iwadparm + 1];

      result = D_FindWADByName(iwadfile);
   }
   else
   {
      // Search through the list and look for an IWAD

      result = NULL;

      BuildIWADDirList();

      for(i = 0; result == NULL && i < num_iwad_dirs; ++i)
         result = SearchDirectoryForIWAD(iwad_dirs[i], iwads, numiwads);
   }

   return result;
}

// Clever hack: Setup can invoke Doom to determine which IWADs are installed.
// Doom searches install paths and exits with the return code being a
// bitmask of the installed IWAD files.

void D_FindInstalledIWADs(iwad_t *iwads)
{
   unsigned int i;
   int result;

   BuildIWADDirList();

   result = 0;

   for(i = 0; i < arrlen(iwads); ++i)
   {
      if (D_FindWADByName(iwads[i].name) != NULL)
      {
         result |= 1 << i;
      }
   }

   exit(result);
}
#endif

// EOF

