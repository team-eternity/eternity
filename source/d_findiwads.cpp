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
//------------------------------------------------------------------------------
//
// Purpose: Code for automated location of users' IWAD files and important
//  PWADs. Some code is derived from Chocolate Doom, by Simon Howard, used
//  under terms of the GPLv2.
//
// Authors: Simon Howard, James Haley, David Hill
//

#if __cplusplus >= 201703L || _MSC_VER >= 1914
#include "hal/i_platform.h"
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

#include "z_zone.h"

#include "doomstat.h"
#include "d_io.h"
#include "d_iwad.h"
#include "d_main.h"
#include "hal/i_directory.h"
#include "hal/i_picker.h"
#include "hal/i_platform.h"
#include "m_collection.h"
#include "m_qstr.h"
#include "w_levels.h"

#include "steam.h"

//=============================================================================
//
// Code for reading registry keys
//
// Do not define EE_FEATURE_REGISTRYSCAN unless you are using a compatible
// compiler.
//

#ifdef EE_FEATURE_REGISTRYSCAN

#include "Win32/i_registry.h"

// Prefix of uninstall value strings
constexpr const char UNINSTALLER_STRING[] = "\\uninstl.exe /S ";

// clang-format off

// CD Installer Locations
static registry_value_t cdUninstallValues[] = {
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
static registry_value_t collectorsEditionValue = {
    HKEY_LOCAL_MACHINE,
    SOFTWARE_KEY "\\Activision\\DOOM Collector's Edition\\v1.0",
    "INSTALLPATH"
};

// clang-format on

// The path loaded from the CE key has several subdirectories:
static const char *collectorsEditionSubDirs[] = {
    "Doom2",
    "Final Doom",
    "Ultimate Doom",
};

// Order of GOG installation key values below.
enum gogkeys_e
{
    GOG_KEY_DOOM_DOOM_II,
    GOG_KEY_ULTIMATE,
    GOG_KEY_DOOM2,
    GOG_KEY_FINAL,
    GOG_KEY_DOOM3BFG,
    GOG_KEY_SVE,
    GOG_KEY_MAX
};

// GOG.com installation keys
static registry_value_t gogInstallValues[GOG_KEY_MAX + 1] = {
    // Doom + Doom II install
    { HKEY_LOCAL_MACHINE, SOFTWARE_KEY "\\GOG.com\\Games\\1413291984", "PATH"  },

    // Ultimate Doom install
    { HKEY_LOCAL_MACHINE, SOFTWARE_KEY "\\GOG.com\\Games\\1435827232", "PATH"  },

    // Doom II install
    { HKEY_LOCAL_MACHINE, SOFTWARE_KEY "\\GOG.com\\Games\\1435848814", "PATH"  },

    // Final Doom install
    { HKEY_LOCAL_MACHINE, SOFTWARE_KEY "\\GOG.com\\Games\\1435848742", "PATH"  },

    // Doom 3: BFG install
    { HKEY_LOCAL_MACHINE, SOFTWARE_KEY "\\GOG.com\\Games\\1135892318", "PATH"  },

    // Strife: Veteran Edition
    { HKEY_LOCAL_MACHINE, SOFTWARE_KEY "\\GOG.com\\Games\\1432899949", "PATH"  },

    // terminating entry
    { nullptr,            nullptr,                                     nullptr }
};

// The paths loaded from the GOG.com keys have several subdirectories.
// Doom + Doom II, Ultimate Doom, and SVE are stored straight in their top directories.
static const char *gogInstallSubDirs[] = {
    "doom2",
    "TNT",
    "Plutonia",
    "base\\wads",
};

// Master Levels from GOG.com. These are installed with Doom II
static const char *gogMasterLevelsPath = "master\\wads";

// clang-format off

// Hexen 95, from the Towers of Darkness collection.
// Special thanks to GreyGhost for finding the registry keys it creates.
// TODO: There's no code to load this right now, since EE doesn't support
// Hexen as of yet. Consider it ground work for the future.
static registry_value_t hexen95Value = {
    HKEY_LOCAL_MACHINE,
    SOFTWARE_KEY "\\Microsoft\\Windows\\CurrentVersion\\App Paths\\hexen95.exe",
    "Path"
};

// clang-format on

//
// Add any of the CD distribution's paths, as determined from the uninstaller
// registry keys they create.
//
static void D_addUninstallPaths(Collection<qstring> &paths)
{
    registry_value_t *regval    = cdUninstallValues;
    size_t            uninstlen = strlen(UNINSTALLER_STRING);

    while(regval->root)
    {
        qstring str;

        if(I_GetRegistryString(*regval, str))
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

    if(I_GetRegistryString(collectorsEditionValue, str))
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

        if(I_GetRegistryString(*regval, str))
        {
            // Doom + Doom II and SVE are in their root installation paths
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

#endif // defined(EE_FEATURE_REGISTRYSCAN)

struct steamdir_t
{
    int         appid;
    const char *waddir;
};

// Steam install directory subdirs
static const steamdir_t steamInstallSubDirs[] = {
    { DOOM_DOOM_II_STEAM_APPID, "rerelease"     },
    { DOOM2_STEAM_APPID,        "base"          },
    { DOOM2_STEAM_APPID,        "finaldoombase" },
    { FINAL_DOOM_STEAM_APPID,   "base"          },
    { HEXEN_STEAM_APPID,        "base"          },
    { HEXDD_STEAM_APPID,        "base"          },
    { SOSR_STEAM_APPID,         "base"          },
    { DOOM3_BFG_STEAM_APPID,    "base/wads"     },
    { SVE_STEAM_APPID,          nullptr         },
};

// Master Levels
static const steamdir_t steamMasterLevelsSubDirs[] = {
    { DOOM_DOOM_II_STEAM_APPID,  "base/master/wads"       },
    { MASTER_LEVELS_STEAM_APPID, "master/wads"            }, // Special thanks to Gez for getting this installation path.
    { DOOM2_STEAM_APPID,         "masterbase/master/wads" },
};

//
// Add all possible Steam paths if the Steam client path can be found
//
static void D_addSteamPaths(Collection<qstring> &paths)
{
    qstring str;

    for(const int appid : STEAM_APPIDS)
    {
        steamgame_t steamGame;
        if(Steam_FindGame(&steamGame, appid) && Steam_ResolvePath(str, &steamGame))
        {
            for(const steamdir_t &currSubDir : steamInstallSubDirs)
            {
                if(currSubDir.appid != appid)
                    continue;

                qstring &newPath = paths.addNew();

                newPath = str;
                if(currSubDir.waddir)
                {
                    // Strip everything before the first slash off of the string to concatenate
                    newPath.pathConcatenate(currSubDir.waddir);
                }
                else
                    newPath.normalizeSlashes();
            }
        }
    }
}

// Default DOS install paths for IWADs
static const char *dosInstallPaths[] = {                                                //
    "\\doom2",   "\\plutonia", "\\tnt",   "\\doom_se", "\\doom", "\\dooms", "\\doomsw", //
    "\\heretic", "\\hexen",    "\\strife"
};

// Default DOS install paths for add-ons.
// Special thanks to GreyGhost for confirming these:
static const char *masterLevelsDOSPath = "\\master\\wads";

// TODO: not used yet
// static const char *dkotdcDOSPath       = "\\hexendk";

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
static void D_addSubDirectories(Collection<qstring> &paths, const char *base)
{
    const fs::directory_iterator itr(base);
    for(const fs::directory_entry &ent : itr)
    {
        const auto filename = ent.path().filename().generic_u8string();

        if(ent.exists() && ent.is_directory())
            paths.add(qstring(
                reinterpret_cast<const char *>(filename.c_str()))); // C++20_FIXME: Cast to make C++20 builds compile
    }
}

//
// Add default paths.
//
static void D_addDefaultDirectories(Collection<qstring> &paths)
{
    std::error_code ec;
#if EE_CURRENT_PLATFORM == EE_PLATFORM_LINUX
    // Default Linux locations
    paths.addNew() = "/usr/local/share/games/doom";
    paths.addNew() = "/usr/share/games/doom";
    paths.addNew() = "/usr/share/doom";
    paths.addNew() = "/usr/share/games/doom3bfg/base/wads";
#endif

    // add base/game paths
    if(fs::is_directory(basepath, ec))
        D_addSubDirectories(paths, basepath);

    // add user/game paths, if userpath != basepath
    if(strcmp(basepath, userpath) && fs::is_directory(userpath, ec))
        D_addSubDirectories(paths, userpath);

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

    // Steam
    D_addSteamPaths(paths);

#ifdef EE_FEATURE_REGISTRYSCAN
    // Add registry paths

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
    iwadcheck_t version;

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

    char **var = nullptr;

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
            case doom2: // DOOM II
                if(estrempty(gi_path_doom2))
                    var = &gi_path_doom2;
                break;
            case pack_disk: // DOOM II BFG Edition
                if(estrempty(gi_path_bfgdoom2))
                    var = &gi_path_bfgdoom2;
                break;
            case pack_tnt: // Final Doom: TNT - Evilution
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
            default: //
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
        default: //
            break;
        }
        break;
    default: //
        break;
    }

    // If we found a var to set, set it!
    if(var)
        *var = fullpath.duplicate(PU_STATIC);
}

//
// Check a path for any of the standard IWAD (or addon WAD) file names.
//
void D_CheckPathForWADs(const qstring &path)
{
    if(std::error_code ec; !fs::is_directory(path.constPtr(), ec))
    {
        // check to see if this is just a regular .wad file
        const char *dot = path.findSubStrNoCase(".wad");
        if(dot)
            D_determineIWADVersion(path);
        return;
    }

    const fs::directory_iterator itr(path.constPtr());
    for(const fs::directory_entry &ent : itr)
    {
        const qstring filename =
            qstring(reinterpret_cast<const char *>(ent.path().filename().generic_u8string().c_str()))
                .toLower(); // C++20_FIXME: Cast to make C++20 builds compile
        for(int i = 0; i < nstandard_iwads; i++)
        {
            if(filename == standard_iwads[i] + 1)
            {
                // found one.
                // determine if we want to store this path.
                D_determineIWADVersion(qstring(reinterpret_cast<const char *>(
                    ent.path().generic_u8string().c_str()))); // C++20_FIXME: Cast to make C++20 builds compile

                break; // break inner loop
            }
        }

        if(estrempty(gi_path_id24res) && filename == "id24res.wad")
        {
            // C++20_FIXME: Cast to make C++20 builds compile
            gi_path_id24res = estrdup(reinterpret_cast<const char *>(ent.path().generic_u8string().c_str()));
            break;
        }
    }
}

//
// If we found a BFG Edition IWAD, or ID24 res,
// check also for nerve.wad and configure its mission pack path.
//
static void D_checkForNoRest()
{
    const char *const *const indicator_wad_path_ptrs[] = { &gi_path_id24res, &gi_path_bfgdoom2 };

    for(const char *const *const path_ptr : indicator_wad_path_ptrs)
    {
        const char *path = *path_ptr;

        // Need BFG Edition DOOM II IWAD
        if(estrempty(path))
            return;

        // Need no NR4TL path already configured
        if(estrnonempty(w_norestpath))
            return;

        qstring nrvpath;

        nrvpath = path;
        nrvpath.removeFileSpec();

        if(std::error_code ec; fs::is_directory(nrvpath.constPtr(), ec))
        {
            const fs::directory_iterator itr(nrvpath.constPtr());
            for(const fs::directory_entry &ent : itr)
            {
                const qstring filename =
                    qstring(reinterpret_cast<const char *>(ent.path().filename().generic_u8string().c_str())).toLower();
                if(filename == "nerve.wad")
                {
                    nrvpath.pathConcatenate(filename);
                    w_norestpath = nrvpath.duplicate();
                    break;
                }
            }
        }
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
    qstring     str;
    struct stat sbuf;

    // Need no Master Levels path already configured
    if(estrnonempty(w_masterlevelsdirname))
        return;

    // Check for any Steam install path
    for(const int appid : STEAM_APPIDS)
    {
        steamgame_t steamGame;
        if(Steam_FindGame(&steamGame, appid) && Steam_ResolvePath(str, &steamGame))
        {
            for(const steamdir_t &currSubDir : steamMasterLevelsSubDirs)
            {
                if(currSubDir.appid != appid)
                    continue;

                qstring newPath{ str };
                // Strip everything before the first slash off of the string to concatenate
                newPath.pathConcatenate(currSubDir.waddir);

                if(!I_stat(newPath.constPtr(), &sbuf) && S_ISDIR(sbuf.st_mode))
                {
                    w_masterlevelsdirname = newPath.duplicate(PU_STATIC);

                    // Got it.
                    return;
                }
            }
        }
    }

#ifdef EE_FEATURE_REGISTRYSCAN
    // Check for GOG.com Doom II install path
    if(I_GetRegistryString(gogInstallValues[GOG_KEY_DOOM2], str))
    {
        str.pathConcatenate(gogMasterLevelsPath);

        if(!I_stat(str.constPtr(), &sbuf) && S_ISDIR(sbuf.st_mode))
        {
            w_masterlevelsdirname = str.duplicate(PU_STATIC);
            return;
        }
    }
#endif

    // Check for the default DOS path
    str = masterLevelsDOSPath;
    if(!I_stat(str.constPtr(), &sbuf) && S_ISDIR(sbuf.st_mode))
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
    qstring             proto;
    Collection<qstring> paths;

    paths.setPrototype(&proto);

    // Collect all candidate file paths
    D_collectIWADPaths(paths);

    // Check all paths that were found for IWADs
    for(qstring &path : paths)
        D_CheckPathForWADs(path);

    // Check for special WADs
    D_checkForNoRest(); // NR4TL

    // TODO: DKotDC, when Hexen is supported

    // Master Levels detection
    D_findMasterLevels();
}

// EOF

