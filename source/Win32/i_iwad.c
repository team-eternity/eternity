// Emacs style mode select   -*- C++ -*- 
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
//   This is Windows-specific code that automatically finds the location
//   of installed IWAD files.  The registry is inspected to find special
//   keys installed by the Windows installers for various CD versions
//   of Doom.  From these keys we can deduce where to find an IWAD.
//
//-----------------------------------------------------------------------------

#ifdef _WIN32

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern void AddIWADDir(char *dir);

typedef struct 
{
   HKEY root;
   char *path;
   char *value;
} registry_value_t;

#define UNINSTALLER_STRING "\\uninstl.exe /S "

// Keys installed by the various CD editions.  These are actually the 
// commands to invoke the uninstaller and look like this:
//
// C:\Program Files\Path\uninstl.exe /S C:\Program Files\Path
//
// With some munging we can find where Doom was installed.

static registry_value_t uninstall_values[] = 
{
   // Ultimate Doom, CD version (Depths of Doom trilogy)

   {
      HKEY_LOCAL_MACHINE, 
      "Software\\Microsoft\\Windows\\CurrentVersion\\"
          "Uninstall\\Ultimate Doom for Windows 95",
      "UninstallString",
   },

   // Doom II, CD version (Depths of Doom trilogy)

   {
      HKEY_LOCAL_MACHINE, 
      "Software\\Microsoft\\Windows\\CurrentVersion\\"
          "Uninstall\\Doom II for Windows 95",
      "UninstallString",
   },

   // Final Doom

   {
      HKEY_LOCAL_MACHINE, 
      "Software\\Microsoft\\Windows\\CurrentVersion\\"
          "Uninstall\\Final Doom for Windows 95",
      "UninstallString",
   },

   // Shareware version

   {
      HKEY_LOCAL_MACHINE, 
      "Software\\Microsoft\\Windows\\CurrentVersion\\"
          "Uninstall\\Doom Shareware for Windows 95",
      "UninstallString",
   },
};

#define num_uninstall_values (sizeof(uninstall_values) / sizeof(registry_value_t))

// Value installed by the Collector's Edition when it is installed

static registry_value_t collectors_edition_value =
{
   HKEY_LOCAL_MACHINE,
   "Software\\Activision\\DOOM Collector's Edition\\v1.0",
   "INSTALLPATH",
};

// Subdirectories of the above install path, where IWADs are installed.

static char *collectors_edition_subdirs[] = 
{
   "Doom2",
   "Final Doom",
   "Ultimate Doom",
};

#define num_collectors_edition_subdirs (sizeof(collectors_edition_subdirs) / sizeof(char *))

// Location where Steam is installed

static registry_value_t steam_install_location =
{
   HKEY_LOCAL_MACHINE,
   "Software\\Valve\\Steam",
   "InstallPath",
};

// Subdirs of the steam install directory where IWADs are found

static char *steam_install_subdirs[] =
{
   "steamapps\\common\\doom 2\\base",
   "steamapps\\common\\final doom\\base",
   "steamapps\\common\\ultimate doom\\base",
   "steamapps\\common\\hexen\\base",
   "steamapps\\common\\heretic shadow of the serpent riders\\base"
};

#define num_steam_install_subdirs (sizeof(steam_install_subdirs) / sizeof(char *))

static char *GetRegistryString(registry_value_t *reg_val)
{
   HKEY key;
   DWORD len;
   DWORD valtype;
   char *result;

   // Open the key (directory where the value is stored)
   
   if(RegOpenKeyEx(reg_val->root, reg_val->path, 0, KEY_READ, &key) 
      != ERROR_SUCCESS)
   {
      return NULL;
   }
   
   // Find the type and length of the string
   
   if(RegQueryValueEx(key, reg_val->value, NULL, &valtype, NULL, &len) 
      != ERROR_SUCCESS)
   {
      return NULL;
   }

   // Only accept strings
   
   if(valtype != REG_SZ)
   {
      return NULL;
   }

   // Allocate a buffer for the value and read the value
   
   result = malloc(len);
   
   if(RegQueryValueEx(key, reg_val->value, NULL, &valtype, (unsigned char *) result, &len) 
      != ERROR_SUCCESS)
   {
      free(result);
      return NULL;
   }

   // Close the key
        
   RegCloseKey(key);

   return result;
}

// Check for the uninstall strings from the CD versions

void CheckUninstallStrings(void)
{
   unsigned int i;
   
   for(i = 0; i < num_uninstall_values; ++i)
   {
      char *val;
      char *path;
      char *unstr;
      
      val = GetRegistryString(&uninstall_values[i]);
      
      if(val == NULL)
      {
         continue;
      }
      
      unstr = strstr(val, UNINSTALLER_STRING);
      
      if(unstr == NULL)
      {
         free(val);
      }
      else
      {
         path = unstr + strlen(UNINSTALLER_STRING);
         
         AddIWADDir(path);
      }
   }
}

// Check for Doom: Collector's Edition

void CheckCollectorsEdition(void)
{
   char *install_path;
   char *subpath;
   unsigned int i;
   
   install_path = GetRegistryString(&collectors_edition_value);
   
   if(install_path == NULL)
   {
      return;
   }
   
   for(i = 0; i < num_collectors_edition_subdirs; ++i)
   {
      subpath = malloc(strlen(install_path)
                       + strlen(collectors_edition_subdirs[i])
                       + 5);

      sprintf(subpath, "%s\\%s", install_path, collectors_edition_subdirs[i]);
      
      AddIWADDir(subpath);
   }

   free(install_path);
}


// Check for Doom downloaded via Steam

void CheckSteamEdition(void)
{
   char *install_path;
   char *subpath;
   size_t i;
   
   install_path = GetRegistryString(&steam_install_location);
   
   if(install_path == NULL)
   {
      return;
   }

   for(i = 0; i < num_steam_install_subdirs; ++i)
   {
      subpath = malloc(strlen(install_path) 
                       + strlen(steam_install_subdirs[i]) + 5);

      sprintf(subpath, "%s\\%s", install_path, steam_install_subdirs[i]);
      
      AddIWADDir(subpath);
   }
}

// Default install directories for DOS Doom

void CheckDOSDefaults(void)
{
   // These are the default install directories used by the deice
   // installer program:
   
   AddIWADDir("/doom2");              // Doom II
   AddIWADDir("/plutonia");           // Final Doom
   AddIWADDir("/tnt");
   AddIWADDir("/doom_se");            // Ultimate Doom
   AddIWADDir("/doom");               // Shareware / Registered Doom
   AddIWADDir("/dooms");              // Shareware versions
   AddIWADDir("/doomsw");

   // haleyjd:
   AddIWADDir("/heretic");            // Heretic
}

#endif

// EOF

