//
// The Eternity Engine
// Copyright (C) 2013 James Haley et al.
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
// Purpose: Code for automated location of users' IWAD files and important 
//  PWADs. Some code is derived from Chocolate Doom, by Simon Howard, used 
//  under terms of the GPLv2.
//
// Authors: Simon Howard, James Haley, David Hill
//

#ifdef _MSC_VER
#include "Win32/i_opndir.h"
#else
#include <dirent.h>
#endif

#include "z_zone.h"
#include "z_auto.h"

#include "doomstat.h"
#include "d_io.h"
#include "d_iwad.h"
#include "d_main.h"
#include "hal/i_picker.h"
#include "hal/i_platform.h"
#include "m_collection.h"
#include "m_qstr.h"
#include "w_levels.h"

//=============================================================================
//
// Code for reading registry keys
//
// Do not define EE_FEATURE_REGISTRYSCAN unless you are using a compatible
// compiler.
//

#ifdef EE_FEATURE_REGISTRYSCAN

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

//
// Static data on a registry key to be read.
//
struct registry_value_t
{
   HKEY root;         // registry root (ie. HKEY_LOCAL_MACHINE)
   const char *path;  // registry path
   const char *value; // registry key
};

//
// This class represents an open registry key. The key handle will be closed
// when an instance of this class falls out of scope.
//
class AutoRegKey
{
protected:
   HKEY key;
   bool valid;

public:
   // Constructor. Open the key described by rval.
   explicit AutoRegKey(const registry_value_t &rval) : valid(false)
   {
      if(!RegOpenKeyEx(rval.root, rval.path, 0, KEY_READ, &key))
         valid = true;
   }

   // Disallow copying
   AutoRegKey(const AutoRegKey &) = delete;
   AutoRegKey &operator = (const AutoRegKey &) = delete;

   // Destructor. Close the key handle, if it is valid.
   ~AutoRegKey()
   {
      if(valid)
      {
         RegCloseKey(key);
         key   = nullptr;
         valid = false;
      }
   }

   // Test if the key is open.
   bool operator ! () const { return !valid; }

   // Query the key value using the Win32 API RegQueryValueEx.
   LONG queryValueEx(LPCTSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType,
                     LPBYTE lpData, LPDWORD lpcbData)
   {
      return RegQueryValueEx(key, lpValueName, lpReserved, lpType, lpData, lpcbData);
   }
};

//
// Open the registry key described by regval and retrieve its value, assuming
// it is a REG_SZ (null-terminated string value). If successful, the results
// are stored in qstring "str". Otherwise, the qstring is unmodified.
//
static bool D_getRegistryString(const registry_value_t &regval, qstring &str)
{
   DWORD  len;
   DWORD  valtype;
   LPBYTE result;

   // Open the key (directory where the value is stored)
   AutoRegKey key(regval);
   if(!key)
      return false;
   
   // Find the type and length of the string
   if(key.queryValueEx(regval.value, nullptr, &valtype, nullptr, &len))
      return false;

   // Only accept strings
   if(valtype != REG_SZ)
      return false;

   // Allocate a buffer for the value and read the value
   ZAutoBuffer buffer(len, true);
   result = buffer.getAs<LPBYTE>();

   if(key.queryValueEx(regval.value, nullptr, &valtype, result, &len))
      return false;

   str = reinterpret_cast<char *>(result);

   return true;
}

// Prefix of uninstall value strings
#define UNINSTALLER_STRING "\\uninstl.exe /S "

#if _WIN64
#define SOFTWARE_KEY "Software\\Wow6432Node"
#else
#define SOFTWARE_KEY "Software"
#endif

// CD Installer Locations
static registry_value_t cdUninstallValues[] =
{
   // Ultimate Doom CD version (Depths of Doom Trilogy)
   {
      HKEY_LOCAL_MACHINE,
      SOFTWARE_KEY "\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
      "Ultimate Doom for Windows 95",
      "UninstallString"
   },

   // DOOM II CD version (Depths of Doom Trilogy)
   {
      HKEY_LOCAL_MACHINE,
      SOFTWARE_KEY "\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
      "Doom II for Windows 95",
      "UninstallString"
   },

   // Final Doom
   {
      HKEY_LOCAL_MACHINE,
      SOFTWARE_KEY "\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
      "Final Doom for Windows 95",
      "UninstallString"
   },

   // Shareware version
   {
      HKEY_LOCAL_MACHINE,
      SOFTWARE_KEY "\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
      "Doom Shareware for Windows 95",
      "UninstallString"
   },

   // terminating entry
   { nullptr, nullptr, nullptr }
};

// Collector's Edition Registry Key
static registry_value_t collectorsEditionValue =
{
   HKEY_LOCAL_MACHINE,
   SOFTWARE_KEY "\\Activision\\DOOM Collector's Edition\\v1.0",
   "INSTALLPATH"
};

// The path loaded from the CE key has several subdirectories:
static const char *collectorsEditionSubDirs[] =
{
   "Doom2",
   "Final Doom",
   "Ultimate Doom",
};

// Order of GOG installation key values below.
enum gogkeys_e
{
   GOG_KEY_ULTIMATE,
   GOG_KEY_DOOM2,
   GOG_KEY_FINAL,
   GOG_KEY_DOOM3BFG,
   GOG_KEY_MAX
};

// GOG.com installation keys
static registry_value_t gogInstallValues[GOG_KEY_MAX+1] =
{
   // Ultimate Doom install
   {
      HKEY_LOCAL_MACHINE,
      SOFTWARE_KEY "\\GOG.com\\Games\\1435827232",
      "PATH"
   },

   // Doom II install
   {
      HKEY_LOCAL_MACHINE,
      SOFTWARE_KEY "\\GOG.com\\Games\\1435848814",
      "PATH"
   },

   // Final Doom install
   {
      HKEY_LOCAL_MACHINE,
      SOFTWARE_KEY "\\GOG.com\\Games\\1435848742",
      "PATH"
   },

   // Doom 3: BFG install
   {
     HKEY_LOCAL_MACHINE,
     SOFTWARE_KEY "\\GOG.com\\Games\\1135892318",
     "PATH"
   },

   // terminating entry
   { nullptr, nullptr, nullptr }
};

// The paths loaded from the GOG.com keys have several subdirectories.
// Ultimate Doom is stored straight in its top directory.
static const char *gogInstallSubDirs[] =
{
   "doom2",
   "TNT",
   "Plutonia",
   "base\\wads",
};

// Master Levels from GOG.com. These are installed with Doom II
static const char *gogMasterLevelsPath = "master\\wads";

// Steam Install Path Registry Key
static registry_value_t steamInstallValue =
{
   HKEY_LOCAL_MACHINE,
   SOFTWARE_KEY "\\Valve\\Steam",
   "InstallPath"
};

// Steam install directory subdirs
// (NB: add "steamapps\\common\\" as a prefix)
static const char *steamInstallSubDirs[] =
{
   "doom 2\\base",
   "final doom\\base",
   "ultimate doom\\base",
   "hexen\\base",
   "hexen deathkings of the dark citadel\\base",
   "heretic shadow of the serpent riders\\base",
   "DOOM 3 BFG Edition\\base\\wads",
};

// Master Levels
// Special thanks to Gez for getting the installation path.
// (NB: as above, add "steamapps\\common\\")
static const char *steamMasterLevelsPath = "Master Levels of Doom\\master\\wads";

// Hexen 95, from the Towers of Darkness collection.
// Special thanks to GreyGhost for finding the registry keys it creates.
// TODO: There's no code to load this right now, since EE doesn't support 
// Hexen as of yet. Consider it ground work for the future.
static registry_value_t hexen95Value =
{
   HKEY_LOCAL_MACHINE,
   SOFTWARE_KEY "\\Microsoft\\Windows\\CurrentVersion\\App Paths\\hexen95.exe",
   "Path"
};

//
// Add any of the CD distribution's paths, as determined from the uninstaller
// registry keys they create.
//
static void D_addUninstallPaths(Collection<qstring> &paths)
{
   registry_value_t *regval = cdUninstallValues;
   size_t uninstlen = strlen(UNINSTALLER_STRING);

   while(regval->root)
   {
      qstring str;

      if(D_getRegistryString(*regval, str))
      {
         size_t uninstpos = str.find(UNINSTALLER_STRING);
         if(uninstpos != qstring::npos)
         {
            // erase from the beginning to the end of the substring
            paths.add(str.erase(0, uninstpos + uninstlen));
         }
      }
      
      ++regval;
   }
}

//
// Looks for the registry key created by the DOOM Collector's Edition installer
// and adds it as an IWAD path if it was found.
//
static void D_addCollectorsEdition(Collection<qstring> &paths)
{
   qstring str;

   if(D_getRegistryString(collectorsEditionValue, str))
   {
      for(size_t i = 0; i < earrlen(collectorsEditionSubDirs); i++)
      {
         qstring &newPath = paths.addNew();

         newPath = str;
         newPath.pathConcatenate(collectorsEditionSubDirs[i]);
      }
   }
}

//
// Looks for registry keys created by the GOG.com installers
// (including Galaxy), and adds them as IWAD paths if found.
//
static void D_addGogPaths(Collection<qstring> &paths)
{
   registry_value_t *regval = gogInstallValues;

   while(regval->root)
   {
      qstring str;

      if(D_getRegistryString(*regval, str))
      {
         // Ultimate Doom is in the root installation path
         paths.add(str);

         for(size_t i = 0; i < earrlen(gogInstallSubDirs); i++)
         {
            qstring &newPath = paths.addNew();

            newPath = str;
            newPath.pathConcatenate(gogInstallSubDirs[i]);
         }
      }

      ++regval;
   }
}

//
// Add all possible Steam paths if the Steam install path is defined in
// the registry
//
static void D_addSteamPaths(Collection<qstring> &paths)
{
   qstring str;

   if(D_getRegistryString(steamInstallValue, str))
   {
      for(size_t i = 0; i < earrlen(steamInstallSubDirs); i++)
      {
         qstring &newPath = paths.addNew();
         
         newPath = str;
         newPath.pathConcatenate("\\steamapps\\common");
         newPath.pathConcatenate(steamInstallSubDirs[i]);
      }
   }
}

#endif

// Default DOS install paths for IWADs
static const char *dosInstallPaths[] =
{
   "\\doom2",
   "\\plutonia",
   "\\tnt",
   "\\doom_se",
   "\\doom",
   "\\dooms",
   "\\doomsw",
   "\\heretic",
   "\\hexen",
   "\\strife"
};

// Default DOS install paths for add-ons.
// Special thanks to GreyGhost for confirming these:
static const char *masterLevelsDOSPath = "\\master\\wads";
static const char *dkotdcDOSPath       = "\\hexendk"; // TODO: not used yet

//
// Add default DOS installation paths
//
static void D_addDOSPaths(Collection<qstring> &paths)
{
   for(size_t i = 0; i < earrlen(dosInstallPaths); i++)
      paths.addNew() = dosInstallPaths[i];
}

//
// Add all components of the DOOMWADPATH environment variable.
//
static void D_addDoomWadPath(Collection<qstring> &paths)
{
   size_t numpaths = D_GetNumDoomWadPaths();

   for(size_t i = 0; i < numpaths; i++)
      paths.addNew() = D_GetDoomWadPath(i);
}

//
// Add the DOOMWADDIR and HOME paths.
//
static void D_addDoomWadDir(Collection<qstring> &paths)
{
   const char *doomWadDir = getenv("DOOMWADDIR");
   const char *homeDir    = getenv("HOME");

   if(estrnonempty(doomWadDir))
      paths.addNew() = doomWadDir;
   if(estrnonempty(homeDir))
      paths.addNew() = homeDir;
}

//
// Add all subdirectories of an open directory
//
static void D_addSubDirectories(Collection<qstring> &paths, DIR *dir,
                                const char *base)
{
   dirent *ent;

   while((ent = readdir(dir)))
   {
      if(!strcmp(ent->d_name, ".") ||
         !strcmp(ent->d_name, ".."))
         continue;
      
      struct stat sbuf;
      qstring fullpath;
      fullpath = base;
      fullpath.pathConcatenate(ent->d_name);

      if(!stat(fullpath.constPtr(), &sbuf))
      {
         if(S_ISDIR(sbuf.st_mode))
            paths.add(fullpath);
      }
   }
}

//
// Add default paths.
//
static void D_addDefaultDirectories(Collection<qstring> &paths)
{
   DIR *dir;

#if EE_CURRENT_PLATFORM == EE_PLATFORM_LINUX
   // Default Linux locations
   paths.addNew() = "/usr/local/share/games/doom";
   paths.addNew() = "/usr/share/games/doom";
   paths.addNew() = "/usr/share/doom";
   paths.addNew() = "/usr/share/games/doom3bfg/base/wads";
#endif

   // add base/game paths
   if((dir = opendir(basepath)))
   {
      D_addSubDirectories(paths, dir, basepath);
      closedir(dir);
   }

   // add user/game paths, if userpath != basepath
   if(strcmp(basepath, userpath) && (dir = opendir(userpath)))
   {
      D_addSubDirectories(paths, dir, userpath);
      closedir(dir);
   }

   paths.addNew() = D_DoomExeDir(); // executable directory
   paths.addNew() = ".";            // current directory
}

//
// Collects all paths under which IWADs might be found.
//
static void D_collectIWADPaths(Collection<qstring> &paths)
{
   // Add DOOMWADDIR and DOOMWADPATH
   D_addDoomWadDir(paths);
   D_addDoomWadPath(paths);

   // Add DOS locations
   D_addDOSPaths(paths);

#ifdef EE_FEATURE_REGISTRYSCAN
   // Add registry paths

   // Steam
   D_addSteamPaths(paths);
   
   // Collector's Edition
   D_addCollectorsEdition(paths);

   // CD version installers
   D_addUninstallPaths(paths);

   // GOG.com installers
   D_addGogPaths(paths);
#endif

   // Add default locations
   D_addDefaultDirectories(paths);
}

//
// Determine what path to populate with this IWAD location.
//
static void D_determineIWADVersion(const qstring &fullpath)
{
   iwadcheck_t   version;

   version.gamemode    = indetermined;
   version.gamemission = none;
   version.hassecrets  = false;
   version.freedoom    = false;
   version.freedm      = false;
   version.bfgedition  = false;
   version.rekkr       = false;
   version.error       = false;
   version.flags       = IWADF_NOERRORS;

   D_CheckIWAD(fullpath.constPtr(), version);

   if(version.error) // Not a WAD, or could not access
      return;

   char **var = NULL;

   switch(version.gamemode)
   {
   case shareware: // DOOM Shareware
      if(estrempty(gi_path_doomsw))
         var = &gi_path_doomsw;
      break;
   case registered: // DOOM Registered
      if(estrempty(gi_path_doomreg))
         var = &gi_path_doomreg;
      break;
   case retail: // The Ultimate DOOM   
      if(version.freedoom)
      { 
         if(estrempty(gi_path_fdoomu)) // Ultimate FreeDoom
            var = &gi_path_fdoomu;
      }
      else if(version.rekkr)
      {
         if(estrempty(gi_path_rekkr)) // Rekkr
            var = &gi_path_rekkr;
      }
      else if(estrempty(gi_path_doomu)) // Ultimate Doom
         var = &gi_path_doomu;
      break;
   case commercial: // DOOM II
      if(version.freedoom)
      {
         if(version.freedm)
         {
            if(estrempty(gi_path_freedm)) // FreeDM
               var = &gi_path_freedm;
         }
         else if(estrempty(gi_path_fdoom)) // FreeDoom            
            var = &gi_path_fdoom;
      }
      else
      {
         switch(version.gamemission)
         {
         case doom2:     // DOOM II
            if(estrempty(gi_path_doom2))
               var = &gi_path_doom2;
            break;
         case pack_disk: // DOOM II BFG Edition
            if(estrempty(gi_path_bfgdoom2))
               var = &gi_path_bfgdoom2;
            break;
         case pack_tnt:  // Final Doom: TNT - Evilution
            if(estrempty(gi_path_tnt))
               var = &gi_path_tnt;
            break;
         case pack_plut: // Final Doom: The Plutonia Experiment
            if(estrempty(gi_path_plut))
               var = &gi_path_plut;
            break;
         case pack_hacx: // HACX: Twitch 'n Kill 1.2
            if(estrempty(gi_path_hacx))
               var = &gi_path_hacx;
            break;
         default:
            break;
         }
      }
      break;
   case hereticsw: // Heretic Shareware
      if(estrempty(gi_path_hticsw)) 
         var = &gi_path_hticsw;
      break;
   case hereticreg: // Heretic Registered
      switch(version.gamemission)
      {
      case heretic: // Registered Version (3 episodes)
         if(estrempty(gi_path_hticreg))
            var = &gi_path_hticreg;
         break;
      case hticsosr: // Heretic: Shadow of the Serpent Riders
         if(estrempty(gi_path_sosr))
            var = &gi_path_sosr;
         break;
      default:
         break;
      }
      break;
   default:
      break;
   }

   // If we found a var to set, set it!
   if(var)
      *var = fullpath.duplicate(PU_STATIC);
}

//
// Check a path for any of the standard IWAD file names.
//
static void D_checkPathForIWADs(const qstring &path)
{
   DIR    *dir;
   dirent *ent;

   if(!(dir = opendir(path.constPtr())))
   {
      // check to see if this is just a regular .wad file
      const char *dot = path.findSubStrNoCase(".wad");
      if(dot)
         D_determineIWADVersion(path);
      return;
   }

   while((ent = readdir(dir)))
   {
      for(int i = 0; i < nstandard_iwads; i++)
      {
         if(!strcasecmp(ent->d_name, standard_iwads[i] + 1))
         {
            // found one.
            qstring fullpath = path;
            fullpath.pathConcatenate(ent->d_name);

            // determine if we want to store this path.
            D_determineIWADVersion(fullpath);

            break; // break inner loop
         }
      }
   }

   closedir(dir);
}

//
// If we found a BFG Edition IWAD, check also for nerve.wad and configure its
// mission pack path.
//
static void D_checkForNoRest()
{
   // Need BFG Edition DOOM II IWAD
   if(estrempty(gi_path_bfgdoom2))
      return;

   // Need no NR4TL path already configured
   if(estrnonempty(w_norestpath))
      return;

   DIR    *dir;
   qstring nrvpath;

   nrvpath = gi_path_bfgdoom2;
   nrvpath.removeFileSpec();

   if((dir = opendir(nrvpath.constPtr())))
   {
      dirent *ent;

      while((ent = readdir(dir)))
      {
         if(!strcasecmp(ent->d_name, "nerve.wad"))
         {
            nrvpath.pathConcatenate(ent->d_name);
            w_norestpath = nrvpath.duplicate();
            break;
         }
      }

      closedir(dir);
   }
}

//
// There are three different places we can find the Master Levels; the default
// DOS install path, and, if we have registry scanning enabled, the Steam
// install directory and the GOG.com Doom II installation. The order of
// preference is Steam > GOG.com > DOS install.
//
static void D_findMasterLevels()
{
   qstring str;
   struct stat sbuf;

   // Need no Master Levels path already configured
   if(estrnonempty(w_masterlevelsdirname))
      return;

#ifdef EE_FEATURE_REGISTRYSCAN
   // Check for the Steam install path
   if(D_getRegistryString(steamInstallValue, str))
   {
      str.pathConcatenate("\\steamapps\\common");
      str.pathConcatenate(steamMasterLevelsPath);

      if(!stat(str.constPtr(), &sbuf) && S_ISDIR(sbuf.st_mode))
      {
         w_masterlevelsdirname = str.duplicate(PU_STATIC);

         // Got it.
         return;
      }
   }

   // Check for GOG.com Doom II install path
   if(D_getRegistryString(gogInstallValues[GOG_KEY_DOOM2], str))
   {
      str.pathConcatenate(gogMasterLevelsPath);

      if(!stat(str.constPtr(), &sbuf) && S_ISDIR(sbuf.st_mode))
      {
         w_masterlevelsdirname = str.duplicate(PU_STATIC);
         return;
      }
   }
#endif

   // Check for the default DOS path
   str = masterLevelsDOSPath;
   if(!stat(str.constPtr(), &sbuf) && S_ISDIR(sbuf.st_mode))
      w_masterlevelsdirname = str.duplicate(PU_STATIC);
}

//=============================================================================
//
// Global Interface
//

//
// Tries to find IWADs in all of the common locations, and then uses any files
// found to preconfigure the system.cfg IWAD paths.
//
void D_FindIWADs()
{
   qstring proto;
   Collection<qstring> paths;

   paths.setPrototype(&proto);

   // Collect all candidate file paths
   D_collectIWADPaths(paths);

   // Check all paths that were found for IWADs
   for(qstring &path : paths)
      D_checkPathForIWADs(path);

   // Check for special WADs
   D_checkForNoRest(); // NR4TL

   // TODO: DKotDC, when Hexen is supported

   // Master Levels detection
   D_findMasterLevels();
}

// EOF

