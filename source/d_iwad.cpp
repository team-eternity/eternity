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
// Purpose: Routines for IWAD location, version identification, and loading.
// Split out of d_main.cpp 08/12/12.
//
// Authors: James Haley, Max Waine
//

#include <memory>

#include "z_zone.h"

#include "hal/i_directory.h"
#include "hal/i_picker.h"
#include "hal/i_platform.h"

#include "d_diskfile.h"
#include "d_files.h"
#include "d_io.h"
#include "d_findiwads.h"
#include "d_iwad.h"
#include "d_main.h"
#include "doomstat.h"
#include "g_game.h"
#include "g_gfs.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_collection.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "m_swap.h"
#include "m_utils.h"
#include "p_info.h"
#include "sounds.h"
#include "w_formats.h"
#include "w_levels.h"
#include "w_wad.h"
#include "w_zip.h"
#include "z_auto.h"

// D_FIXME:
extern bool gamepathset;
extern int  gamepathparm;
void        D_InitPaths();
void        D_CheckGameMusic();

bool d_scaniwads; // haleyjd 11/15/12

//=============================================================================
//
// DOOMWADPATH support
//
// haleyjd 12/31/10: A standard evolved later for DOOMWADPATH, in preference to
// use of DOOMWADDIR, which could only specify a single path. DOOMWADPATH is
// like the standard system path, except for wads. When looking for a file on
// the DOOMWADPATH, the paths in the variable will be tried in the order they
// are specified.
//

// doomwadpaths is an array of paths, the decomposition of the DOOMWADPATH
// environment variable
static PODCollection<char *> doomwadpaths;

//
// D_addDoomWadPath
//
// Adds a path to doomwadpaths.
//
static void D_addDoomWadPath(const char *path)
{
    doomwadpaths.add(estrdup(path));
}

// haleyjd 01/17/11: Use a different separator on Windows than on POSIX platforms
#if EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS
static constexpr char DOOMWADPATHSEP = ';';
#else
static constexpr char DOOMWADPATHSEP = ':';
#endif

//
// D_parseDoomWadPath
//
// Looks for the DOOMWADPATH environment variable. If it is defined, then
// doomwadpaths will consist of the components of the decomposed variable.
//
static void D_parseDoomWadPath()
{
    const char *dwp;

    if((dwp = getenv("DOOMWADPATH")))
    {
        char *tempdwp = Z_Strdupa(dwp);
        char *rover   = tempdwp;
        char *currdir = tempdwp;
        int   dirlen  = 0;

        while(*rover)
        {
            // Found the end of a path?
            // Add the string from the prior one (or the beginning) to here as
            // a separate path.
            if(*rover == DOOMWADPATHSEP)
            {
                *rover = '\0'; // replace ; or : with a null terminator
                if(dirlen)
                    D_addDoomWadPath(currdir);
                dirlen  = 0;
                currdir = rover + 1; // start of next path is the next char
            }
            else
                ++dirlen; // Length is tracked so that we don't add any 0-length paths

            ++rover;
        }

        // Add the last path if necessary (it's either the only one, or the final
        // one, which is probably not followed by a semicolon).
        if(dirlen)
            D_addDoomWadPath(currdir);
    }
}

//
// D_FindInDoomWadPath
//
// Looks for a file in each path extracted from DOOMWADPATH in the order the
// paths were defined. A normalized concatenation of the path and the filename
// will be returned which must be freed by the calling code, if the file is
// found. Otherwise nullptr is returned.
//
static bool D_FindInDoomWadPath(qstring &out, const char *filename, const char *extension)
{
    qstring qstr;
    bool    success  = false;
    char   *currext  = nullptr;
    size_t  numpaths = doomwadpaths.getLength();

    for(size_t i = 0; i < numpaths; i++)
    {
        struct stat sbuf;

        qstr = doomwadpaths[i];
        qstr.pathConcatenate(filename);

        // See if the file exists as-is
        if(!I_stat(qstr.constPtr(), &sbuf)) // check for existence
        {
            if(!S_ISDIR(sbuf.st_mode)) // check that it's NOT a directory
            {
                out     = qstr;
                success = true;
                break; // done.
            }
        }

        // See if the file could benefit from having the default extension
        // added to it.
        if(extension && (currext = qstr.bufferAt(qstr.length() - 4)))
        {
            if(strcasecmp(currext, extension)) // Doesn't already have it?
            {
                qstr += extension;

                if(!I_stat(qstr.constPtr(), &sbuf)) // exists?
                {
                    if(!S_ISDIR(sbuf.st_mode)) // not a dir?
                    {
                        out     = qstr;
                        success = true;
                        break; // done.
                    }
                }
            }
        }
    }

    return success;
}

//
// D_GetNumDoomWadPaths
//
// Returns the number of path components loaded from DOOMWADPATH
//
size_t D_GetNumDoomWadPaths()
{
    return doomwadpaths.getLength();
}

//
// D_GetDoomWadPath
//
// Returns the DOOMWADPATH entry at index i.
// Returns nullptr if i is not a valid index.
//
char *D_GetDoomWadPath(size_t i)
{
    if(i >= doomwadpaths.getLength())
        return nullptr;

    return doomwadpaths[i];
}

//=============================================================================
//
// Disk File Handling
//
// haleyjd 05/28/10
//

// known .disk types
enum
{
    DISK_DOOM,
    DISK_DOOM2
};

static bool        havediskfile; // if true, -disk loaded a file
static bool        havediskiwad; // if true, an IWAD was found in the disk file
static const char *diskpwad;     // PWAD name (or substring) to look for
static diskfile_t *diskfile;     // diskfile object (see d_diskfile.c)
static diskwad_t   diskiwad;     // diskwad object for iwad
static int         disktype;     // type of disk file

//
// D_CheckDiskFileParm
//
// haleyjd 05/28/10: Looks for -disk and sets up diskfile loading.
//
static void D_CheckDiskFileParm()
{
    int p;

    if((p = M_CheckParm("-disk")) && p < myargc - 1)
    {
        havediskfile = true;

        // get diskfile name
        const char *fn = myargv[p + 1];

        // have a pwad name as well?
        if(p < myargc - 2 && *(myargv[p + 2]) != '-')
            diskpwad = myargv[p + 2];

        // open the diskfile
        diskfile = D_OpenDiskFile(fn);
    }
}

//
// D_FindDiskFileIWAD
//
// Finds an IWAD in a disk file.
// If this fails, the disk file will be closed.
//
static void D_FindDiskFileIWAD()
{
    diskiwad = D_FindWadInDiskFile(diskfile, "doom");

    if(diskiwad.f)
    {
        havediskiwad = true;

        if(strstr(diskiwad.name, "doom2.wad"))
            disktype = DISK_DOOM2;
        else
            disktype = DISK_DOOM;
    }
    else
    {
        // close it up, we can't use it
        D_CloseDiskFile(diskfile, true);
        diskfile     = nullptr;
        diskpwad     = nullptr;
        havediskfile = false;
        havediskiwad = false;
    }
}

//
// D_LoadDiskFileIWAD
//
// Loads an IWAD from the disk file.
//
static void D_LoadDiskFileIWAD()
{
    if(diskiwad.f)
        D_AddFile(diskiwad.name, lumpinfo_t::ns_global, diskiwad.f, diskiwad.offset, DAF_IWAD);
    else
        I_Error("D_LoadDiskFileIWAD: invalid file pointer\n");
}

//
// D_LoadDiskFilePWAD
//
// Loads a PWAD from the disk file.
//
static void D_LoadDiskFilePWAD()
{
    diskwad_t wad = D_FindWadInDiskFile(diskfile, diskpwad);

    if(wad.f)
    {
        if(!strstr(wad.name, "doom")) // do not add doom[2].wad twice
            D_AddFile(wad.name, lumpinfo_t::ns_global, wad.f, wad.offset, DAF_NONE);
    }
}

//
// D_metaGetLine
//
// Gets a single line of input from the metadata.txt resource.
//
static bool D_metaGetLine(qstring &qstr, const char *input, int *idx)
{
    int i = *idx;

    // if empty at start, we are finished
    if(input[i] == '\0')
        return false;

    qstr.clear();

    while(input[i] != '\n' && input[i] != '\0')
    {
        if(input[i] == '\\' && input[i + 1] == 'n')
        {
            // make \n sequence into a \n character
            ++i;
            qstr += '\n';
        }
        else if(input[i] != '\r')
            qstr += input[i];

        ++i;
    }

    if(input[i] == '\n')
        ++i;

    // write back input position
    *idx = i;

    return true;
}

//
// D_parseMetaData
//
// Parse a metadata resource
//
static void D_parseMetaData(const char *metatext, int mission)
{
    qstring     buffer;
    const char *endtext = nullptr, *levelname = nullptr, *musicname = nullptr;
    int         partime = 0, musicnum = 0, index = 0;
    int         exitreturn = 0, secretlevel = 0, levelnum = 1, linenum = 0;
    const char *intername = "INTERPIC";

    if(::GameModeInfo->missionInfo->id == pack_disk)
        intername = "DMENUPIC";

    // get first line, which is an episode id
    D_metaGetLine(buffer, metatext, &index);

    // get episode name
    if(D_metaGetLine(buffer, metatext, &index))
    {
        if(mission == MD_NONE) // Not when playing as a mission pack
            ::GameModeInfo->versionName = buffer.duplicate(PU_STATIC);
    }

    // get end text
    if(D_metaGetLine(buffer, metatext, &index))
        endtext = buffer.duplicate(PU_STATIC);

    // get next level after secret
    if(D_metaGetLine(buffer, metatext, &index))
        exitreturn = buffer.toInt();

    // skip next line (wad name)
    D_metaGetLine(buffer, metatext, &index);

    // get secret level
    if(D_metaGetLine(buffer, metatext, &index))
        secretlevel = buffer.toInt();

    // get levels
    while(D_metaGetLine(buffer, metatext, &index))
    {
        switch(linenum)
        {
        case 0: // levelname
            levelname = buffer.duplicate(PU_STATIC);
            break;
        case 1: // music number
            musicnum = mus_runnin + buffer.toInt() - 1;

            if(musicnum > ::GameModeInfo->musMin && musicnum < ::GameModeInfo->numMusic)
                musicname = S_music[musicnum].name;
            else
                musicname = "";
            break;
        case 2: // partime (final field)
            partime = buffer.toInt();

            // create a metainfo object for LevelInfo
            P_CreateMetaInfo(levelnum, levelname, partime, musicname, levelnum == secretlevel ? exitreturn : 0,
                             levelnum == exitreturn - 1 ? secretlevel : 0, levelnum == secretlevel - 1,
                             (levelnum == secretlevel - 1) ? endtext : nullptr, mission, intername);
            break;
        }
        ++linenum;

        if(linenum == 3)
        {
            levelnum++;
            linenum = 0;
        }
    }
}

//
// D_DiskMetaData
//
// Handles metadata in the disk file.
//
static void D_DiskMetaData()
{
    qstring     name;
    char       *metatext = nullptr;
    const char *slash    = nullptr;
    int         slen     = 0;
    diskwad_t   wad;

    if(!diskpwad)
        return;

    // find the wad to get the canonical resource name
    wad = D_FindWadInDiskFile(diskfile, diskpwad);

    // return if not found, or if this is metadata for the IWAD
    if(!wad.f || strstr(wad.name, "doom"))
        return;

    // construct the metadata filename
    if(!(slash = strrchr(wad.name, '\\')))
        return;

    slen = eindex(slash - wad.name);
    ++slen;
    name.copy(wad.name, slen);
    name.concat("metadata.txt");

    // load it up
    if((metatext = (char *)(D_CacheDiskFileResource(diskfile, name.constPtr(), true))))
    {
        // parse it
        D_parseMetaData(metatext, MD_NONE);

        // done with metadata resource
        efree(metatext);
    }
}

//
// D_MissionMetaData
//
// haleyjd 11/04/12: Load metadata for a mission pack wad file.
//
void D_MissionMetaData(const char *lump, int mission)
{
    int         lumpnum  = wGlobalDir.getNumForName(lump);
    int         lumpsize = wGlobalDir.lumpLength(lumpnum);
    ZAutoBuffer metabuffer(lumpsize + 1, true);
    char       *metatext = metabuffer.getAs<char *>();

    wGlobalDir.readLump(lumpnum, metatext);

    D_parseMetaData(metatext, mission);
}

struct deferredmetadata_t
{
    const char *lumpname;
    int         mission;
};

static deferredmetadata_t d_deferredMetaData;

//
// D_DeferredMissionMetaData
//
// Defer loading of a mission metadata file at startup, so that it can be
// scheduled during W_InitMultipleFiles but not done until after the process
// is complete, since lumps cannot be looked up at that point.
//
void D_DeferredMissionMetaData(const char *lump, int mission)
{
    d_deferredMetaData.lumpname = lump;
    d_deferredMetaData.mission  = mission;
}

//
// D_DoDeferredMissionMetaData
//
// Execute a deferred mission metadata load.
// This is called right after W_InitMultipleFiles.
//
void D_DoDeferredMissionMetaData()
{
    if(d_deferredMetaData.lumpname != nullptr)
        D_MissionMetaData(d_deferredMetaData.lumpname, d_deferredMetaData.mission);
}

//=============================================================================
//
// IWAD Detection / Verification Code
//

int iwad_choice; // haleyjd 03/19/10: remember choice

// variable-for-index lookup for D_DoIWADMenu
static char **iwadVarForNum[NUMPICKIWADS] = {
    &gi_path_doomsw, &gi_path_doomreg,  &gi_path_doomu,  // Doom 1
    &gi_path_doom2,  &gi_path_bfgdoom2,                  // Doom 2
    &gi_path_tnt,    &gi_path_plut,                      // Final Doom
    &gi_path_hacx,                                       // HACX
    &gi_path_hticsw, &gi_path_hticreg,  &gi_path_sosr,   // Heretic
    &gi_path_fdoom,  &gi_path_fdoomu,   &gi_path_freedm, // Freedoom
    &gi_path_rekkr,                                      // Rekkr
};

//
// D_doIWADMenu
//
// Crazy fancy graphical IWAD choosing menu.
// Paths are stored in the system.cfg file, and only the games with paths
// stored can be picked from the menu.
// This feature is only available for SDL builds.
//
static const char *D_doIWADMenu()
{
    const char *iwadToUse = nullptr;

#ifdef _SDL_VER
    bool haveIWADs[NUMPICKIWADS];
    int  choice   = -1;
    bool foundone = false;

    memset(haveIWADs, 0, sizeof(haveIWADs));

    // populate haveIWADs array based on system.cfg variables
    for(int i = 0; i < NUMPICKIWADS; i++)
    {
        const char *path = *iwadVarForNum[i];
        if(path && *path != '\0')
        {
            struct stat sbuf;

            // 07/06/13: Needs to actually exist.
            if(!I_stat(path, &sbuf) && !S_ISDIR(sbuf.st_mode))
            {
                haveIWADs[i] = true;
                foundone     = true;
            }
        }
    }

    if(foundone) // at least one IWAD must be specified!
    {
        startupmsg("D_DoIWADMenu", "Init IWAD choice subsystem.");
        choice = I_Pick_DoPicker(haveIWADs, iwad_choice);
    }

    if(choice >= 0)
    {
        iwad_choice = choice; // 03/19/10: remember selection
        iwadToUse   = *iwadVarForNum[choice];
    }
#endif

    return iwadToUse;
}

// Match modes for iwadpathmatch
enum
{
    MATCH_NONE,
    MATCH_GAME,
    MATCH_IWAD
};

//
// iwadpathmatch
//
// This structure is used for finding an IWAD path variable that is the
// best match for a -game or -iwad string. A predefined priority is imposed
// on the IWAD variables so that the "best" version of the game available
// is chosen.
//
struct iwadpathmatch_t
{
    int         mode;         // Mode for this entry: -game, or -iwad
    const char *name;         // The -game or -iwad substring matched
    char      **iwadpaths[3]; // IWAD path variables to check when this matches,
                              // in order of precedence from greatest to least.
};

// clang-format off

static iwadpathmatch_t iwadMatchers[] = {
    // -game matches:
    { MATCH_GAME, "doom2",     { &gi_path_doom2,    &gi_path_bfgdoom2, &gi_path_fdoom  } },
    { MATCH_GAME, "doom",      { &gi_path_doomu,    &gi_path_doomreg,  &gi_path_doomsw } },
    { MATCH_GAME, "tnt",       { &gi_path_tnt,      nullptr,           nullptr         } },
    { MATCH_GAME, "plutonia",  { &gi_path_plut,     nullptr,           nullptr         } },
    { MATCH_GAME, "hacx",      { &gi_path_hacx,     nullptr,           nullptr         } },
    { MATCH_GAME, "heretic",   { &gi_path_sosr,     &gi_path_hticreg,  &gi_path_hticsw } },

    // -iwad matches
    { MATCH_IWAD, "doom2f",    { &gi_path_doom2,    &gi_path_bfgdoom2, &gi_path_fdoom  } },
    { MATCH_IWAD, "doom2",     { &gi_path_doom2,    &gi_path_bfgdoom2, &gi_path_fdoom  } },
    { MATCH_IWAD, "doomu",     { &gi_path_doomu,    &gi_path_fdoomu,   nullptr         } },
    { MATCH_IWAD, "doom1",     { &gi_path_doomsw,   nullptr,           nullptr         } },
    { MATCH_IWAD, "doom",      { &gi_path_doomu,    &gi_path_doomreg,  &gi_path_fdoomu } },
    { MATCH_IWAD, "tnt",       { &gi_path_tnt,      nullptr,           nullptr         } },
    { MATCH_IWAD, "plutonia",  { &gi_path_plut,     nullptr,           nullptr         } },
    { MATCH_IWAD, "hacx",      { &gi_path_hacx,     nullptr,           nullptr         } },
    { MATCH_IWAD, "heretic1",  { &gi_path_hticsw,   nullptr,           nullptr         } },
    { MATCH_IWAD, "heretic",   { &gi_path_sosr,     &gi_path_hticreg,  nullptr         } },
    { MATCH_IWAD, "freedoom2", { &gi_path_fdoom,    nullptr,           nullptr         } },
    { MATCH_IWAD, "freedoom1", { &gi_path_fdoomu,   nullptr,           nullptr         } },
    { MATCH_IWAD, "freedm",    { &gi_path_freedm,   nullptr,           nullptr         } },
    { MATCH_IWAD, "bfgdoom2",  { &gi_path_bfgdoom2, nullptr,           nullptr,        } },
    { MATCH_IWAD, "rekkr",     { &gi_path_rekkr,    nullptr,           nullptr,        } },

    // Terminating entry
    { MATCH_NONE, nullptr,     { nullptr,           nullptr,           nullptr         } }
};

// clang-format on

//
// D_IWADPathForGame
//
// haleyjd 12/31/10: Return the best defined IWAD path variable for a
// -game parameter. Returns nullptr if none found.
//
static char *D_IWADPathForGame(const char *game)
{
    iwadpathmatch_t *cur = iwadMatchers;

    while(cur->mode != MATCH_NONE)
    {
        if(cur->mode == MATCH_GAME) // is a -game matcher?
        {
            if(!strcasecmp(cur->name, game)) // should be an exact match
            {
                for(int i = 0; i < 3; i++) // try each path in order
                {
                    if(!cur->iwadpaths[i]) // no more valid paths to try
                        break;

                    if(**(cur->iwadpaths[i]) != '\0')
                        return *(cur->iwadpaths[i]); // got one!
                }
            }
        }
        ++cur; // try the next entry
    }

    return nullptr; // nothing was found
}

//
// D_IWADPathForIWADParam
//
// haleyjd 12/31/10: Return the best defined IWAD path variable for a
// -iwad parameter. Returns nullptr if none found.
//
static char *D_IWADPathForIWADParam(const char *iwad)
{
    iwadpathmatch_t *cur = iwadMatchers;

    // If the name starts with a slash, step forward one
    char *tmpname = Z_Strdupa((*iwad == '/' || *iwad == '\\') ? iwad + 1 : iwad);

    // Truncate at any extension
    char *dotpos = strrchr(tmpname, '.');
    if(dotpos)
        *dotpos = '\0';

    while(cur->mode != MATCH_NONE)
    {
        if(cur->mode == MATCH_IWAD) // is a -iwad matcher?
        {
            if(!strcasecmp(cur->name, tmpname)) // should be an exact match
            {
                for(int i = 0; i < 3; i++) // try each path in order
                {
                    if(!cur->iwadpaths[i]) // no more valid paths to try
                        break;

                    if(**(cur->iwadpaths[i]) != '\0')
                        return *(cur->iwadpaths[i]); // got one!
                }
            }
        }
        ++cur; // try the next entry
    }

    return nullptr; // nothing was found
}

// Inlines for CheckIWAD

inline static bool isMapExMy(const char *const name)
{
    return name[0] == 'E' && name[2] == 'M' && !name[4];
}

inline static bool isMapMAPxy(const char *const name)
{
    return name[0] == 'M' && name[1] == 'A' && name[2] == 'P' && !name[5];
}

inline static bool isCAV(const char *const name)
{
    return name[0] == 'C' && name[1] == 'A' && name[2] == 'V' && !name[7];
}

inline static bool isMC(const char *const name)
{
    return name[0] == 'M' && name[1] == 'C' && !name[3];
}

// haleyjd 10/13/05: special stuff for FreeDOOM :)
bool freedoom = false;

// haleyjd 11/03/12: special stuff for BFG Edition IWADs
bool bfgedition = false;

//
// lumpnamecmp
//
// Compare a possibly non-null-terminated lump name against a static
// character string of no more than 8 characters.
//
static int lumpnamecmp(const char *lumpname, const char *str)
{
    return strncmp(lumpname, str, strlen(str));
}

//
// D_checkIWAD_WAD
//
// Verify a file is indeed tagged as an IWAD
// Scan its lumps for levelnames and return gamemode as indicated
// Detect missing wolf levels in DOOM II
//
// The filename to check is passed in iwadname, the gamemode detected is
// returned in gmode, hassec returns the presence of secret levels
//
// jff 4/19/98 Add routine to test IWAD for validity and determine
// the gamemode from it. Also note if DOOM II, whether secret levels exist
//
// killough 11/98:
// Rewritten to considerably simplify
// Added Final Doom support (thanks to Joel Murdoch)
//
// joel 10/17/98 Final DOOM fix: added gmission
//
static void D_checkIWAD_WAD(FILE *fp, const char *iwadname, iwadcheck_t &version)
{
    int         ud = 0, rg = 0, sw = 0, cm = 0, sc = 0, tnt = 0, plut = 0, hacx = 0, bfg = 0;
    int         raven = 0, sosr = 0;
    filelump_t  lump;
    wadinfo_t   header;
    const char *n = lump.name;

    // read IWAD header
    if(fread(&header, sizeof header, 1, fp) < 1 || strncmp(header.identification, "IWAD", 4))
    {
        // haleyjd 06/06/09: do not error out here, due to some bad tools
        // resetting peoples' IWADs to PWADs. Only error if it is also
        // not a PWAD.
        if(strncmp(header.identification, "PWAD", 4))
        {
            if(version.flags & IWADF_FATALNOTWAD)
                I_Error("IWAD or PWAD tag not present: %s\n", iwadname);

            version.error = true;
            fclose(fp);
            return;
        }
        else if(version.flags & IWADF_FATALNOTWAD)
            usermsg("Warning: IWAD tag not present: %s\n", iwadname);
    }

    fseek(fp, SwapLong(header.infotableofs), SEEK_SET);

    // Determine game mode from levels present
    // Must be a full set for whichever mode is present
    // Lack of wolf-3d levels also detected here

    header.numlumps = SwapLong(header.numlumps);

    for(; header.numlumps; header.numlumps--)
    {
        if(!fread(&lump, sizeof(lump), 1, fp))
            break;

        if(isMapExMy(n))
        {
            if(n[1] == '4')
                ++ud;
            else if(n[1] == '3' || n[1] == '2')
                ++rg;
            else if(n[1] == '1')
                ++sw;
        }
        else if(isMapMAPxy(n))
        {
            ++cm;
            sc += (n[3] == '3' && (n[4] == '1' || n[4] == '2'));
        }
        else if(isCAV(n))
            ++tnt;
        else if(isMC(n))
            ++plut;
        else if(!lumpnamecmp(n, "ADVISOR") || !lumpnamecmp(n, "TINTTAB") || !lumpnamecmp(n, "SNDCURVE"))
        {
            ++raven;
        }
        else if(!lumpnamecmp(n, "EXTENDED"))
            ++sosr;
        else if(!lumpnamecmp(n, "FREEDOOM"))
            version.freedoom = true;
        else if(!lumpnamecmp(n, "FREEDM")) // Needed to discern freedm from freedoom2
            version.freedm = true;
        else if(!lumpnamecmp(n, "HACX-R"))
            ++hacx;
        else if(!lumpnamecmp(n, "M_ACPT") || // haleyjd 11/03/12: BFG Edition
                !lumpnamecmp(n, "M_CAN") || !lumpnamecmp(n, "M_EXITO") || !lumpnamecmp(n, "M_CHG") ||
                !lumpnamecmp(n, "DMENUPIC"))
        {
            ++bfg;
            if(bfg >= 5) // demand all 5 new lumps for safety.
                version.bfgedition = true;
        }
        else if(!lumpnamecmp(n, "REKCREDS"))
            version.rekkr = true;
    }

    fclose(fp);

    version.hassecrets = false;

    // haleyjd 10/09/05: "Raven mode" detection
    if(raven == 3)
    {
        // TODO: Hexen
        version.gamemission = heretic;

        if(rg >= 18)
        {
            // require both E4 and EXTENDED lump for SoSR
            if(sosr && ud >= 9)
                version.gamemission = hticsosr;
            version.gamemode = hereticreg;
        }
        else if(sw >= 9)
        {
            version.gamemode = hereticsw;
        }
        else if(sw == 3)
        {
            // haleyjd 08/10/13: Heretic beta version support
            version.gamemission = hticbeta;
            version.gamemode    = hereticsw;
        }
        else
            version.gamemode = indetermined;
    }
    else
    {
        version.gamemission = doom;

        if(cm >= 30 || (cm && !rg))
        {
            if(version.freedoom)
            {
                // FreeDoom is meant to be Doom II, not TNT
                version.gamemission = doom2;
            }
            else if(version.bfgedition)
            {
                // BFG Edition - Behaves same as XBox 360 disk version
                version.gamemission = pack_disk;
            }
            else
            {
                if(tnt >= 4)
                    version.gamemission = pack_tnt;
                else if(plut >= 8)
                    version.gamemission = pack_plut;
                else if(hacx)
                    version.gamemission = pack_hacx;
                else
                    version.gamemission = doom2;
            }
            version.hassecrets = (sc >= 2) || hacx;
            version.gamemode   = commercial;
        }
        else if(ud >= 9)
            version.gamemode = retail;
        else if(rg >= 18)
            version.gamemode = registered;
        else if(sw >= 9)
            version.gamemode = shareware;
        else
            version.gamemode = indetermined;
    }
}

// Structure storing information on ZIP-format supported missions
struct zipmission_t
{
    const char   *name;
    GameMode_t    mode;
    GameMission_t mission;
};

// Table of ZIP-format supported mission data
static zipmission_t zipMissions[] = {
    { "doom retail",        retail,     doom      },
    { "doom registered",    registered, doom      },
    { "doom shareware",     shareware,  doom      },
    { "doom2 commercial",   commercial, doom2     },
    { "doom2 tnt",          commercial, pack_tnt  },
    { "doom2 plutonia",     commercial, pack_plut },
    { "doom2 hacx",         commercial, pack_hacx },
    { "doom2 bfg",          commercial, pack_disk },
    { "doom2 psx",          commercial, pack_psx  },
    { "heretic sosr",       hereticreg, hticsosr  },
    { "heretic registered", hereticreg, heretic   },
    { "heretic shareware",  hereticsw,  heretic   },
    { "heretic beta",       hereticsw,  hticbeta  },
};

//
// D_checkIWAD_ZIP
//
// Not technically an "IWAD", but we allow use of PKE archives as the main
// game archive, with different semantics - the PKE must explicitly designate
// what game mode/mission it implements within the archive. There is no
// heuristic-based scan over the broad contents as there is for WAD files.
//
static void D_checkIWAD_ZIP(FILE *fp, const char *iwadname, iwadcheck_t &version)
{
    std::unique_ptr<ZipFile> zip(new ZipFile());

    if(!zip->readFromFile(fp))
    {
        if(version.flags & IWADF_FATALNOTWAD)
            I_Error("Could not read ZIP format archive: %s\n", iwadname);
        version.error = true;
        return;
    }

    int infolump;
    if((infolump = zip->findLump("gameversion.txt")) >= 0)
    {
        ZAutoBuffer buf;
        zip->getLump(infolump).read(buf, true);
        auto verName = buf.getAs<const char *>();
        if(verName)
        {
            for(size_t i = 0; i < earrlen(zipMissions); i++)
            {
                zipmission_t &zm = zipMissions[i];
                if(!strcasecmp(zm.name, verName))
                {
                    version.gamemode    = zm.mode;
                    version.gamemission = zm.mission;

                    // keep special case fields consistent
                    if(version.gamemode == commercial)
                        version.hassecrets = true;
                    if(version.gamemission == pack_disk)
                        version.bfgedition = true;

                    return; // successful!
                }
            }
        }
    }

    // Unknown gamemode or mission
    version.gamemission = doom;
    version.gamemode    = indetermined;
}

//
// D_CheckIWAD
//
// Check the format and contents of a candidate IWAD file and return the
// detected game mode and mission properties in the iwadcheck_t structure.
// Dispatches to subroutines above for supported archive formats.
//
void D_CheckIWAD(const char *iwadname, iwadcheck_t &version)
{
    FILE *fp;

    if(!(fp = I_fopen(iwadname, "rb")))
    {
        if(version.flags & IWADF_FATALNOTOPEN)
        {
            I_Error("Can't open IWAD: %s (%s)\n", iwadname, errno ? strerror(errno) : "unknown error");
        }
        version.error = true;
        return;
    }

    WResourceFmt fmt = W_DetermineFileFormat(fp, 0);
    switch(fmt)
    {
    case W_FORMAT_WAD: // WAD file
        D_checkIWAD_WAD(fp, iwadname, version);
        break;
    case W_FORMAT_ZIP: // ZIP file
        D_checkIWAD_ZIP(fp, iwadname, version);
        break;
    default: // Unknown
        if(version.flags & IWADF_FATALNOTWAD)
            I_Error("Unknown archive format: %s\n", iwadname);
        version.error = true;
        fclose(fp);
        break;
    }
}

//
// WadFileStatus
//
// jff 4/19/98 Add routine to check a pathname for existence as
// a file or directory. If neither append .wad and check if it
// exists as a file then. Else return non-existent.
//
static bool WadFileStatus(qstring &filename, bool *isdir)
{
    struct stat sbuf;
    size_t      i = filename.length();

    *isdir = false; // default is directory to false
    if(i == 0)      // if path nullptr or empty, doesn't exist
        return false;

    if(!I_stat(filename.constPtr(), &sbuf)) // check for existence
    {
        *isdir = S_ISDIR(sbuf.st_mode); // if it does, set whether a dir or not
        return true;                    // return does exist
    }

    if(i >= 4)
    {
        if(filename.find(".wad", i - 4) != qstring::npos)
            return false; // if already ends in .wad, not found
    }

    filename.concat(".wad"); // try it with .wad added

    if(!I_stat(filename.constPtr(), &sbuf)) // if it exists then
    {
        if(S_ISDIR(sbuf.st_mode)) // but is a dir, then say we didn't find it
            return false;
        return true; // otherwise return file found, w/ .wad added
    }
    filename.truncate(i); // remove .wad
    return false;         // and report doesn't exist
}

// jff 4/19/98 list of standard IWAD names
const char *const standard_iwads[] = {
    // Official IWADs
    "/doom2.wad",    // DOOM II
    "/doom2f.wad",   // DOOM II, French Version
    "/plutonia.wad", // Final DOOM: Plutonia
    "/tnt.wad",      // Final DOOM: TNT
    "/doom.wad",     // Registered/Ultimate DOOM
    "/doomu.wad",    // CPhipps - allow doomu.wad
    "/doom1.wad",    // Shareware DOOM
    "/heretic.wad",  // Heretic  -- haleyjd 10/10/05
    "/heretic1.wad", // Shareware Heretic

    // Unofficial IWADs
    "/freedoom2.wad", // Freedoom Phase 2         -- haleyjd 01/11/14
    "/freedoom1.wad", // Freedoom "Demo"/Phase 1  -- haleyjd 03/07/10
    "/freedoom.wad",  // Freedoom                 -- haleyjd 01/31/03 (deprecated)
    "/freedoomu.wad", // "Ultimate" Freedoom      -- haleyjd 03/07/10 (deprecated)
    "/freedm.wad",    // FreeDM IWAD              -- haleyjd 08/28/11
    "/hacx.wad",      // HACX standalone version  -- haleyjd 08/19/09
    "/bfgdoom.wad",   // BFG Edition UDoom IWAD   -- haleyjd 11/03/12
    "/bfgdoom2.wad",  // BFG Edition DOOM2 IWAD   -- haleyjd 11/03/12
    "/rekkrsa.wad",   // Rekkr standalone version -- MaxW: 2018/07/15
};

int nstandard_iwads = earrlen(standard_iwads);

//
// D_findIWADFile
//
// Search in all the usual places until an IWAD is found.
//
// The global baseiwad contains either a full IWAD file specification
// or a directory to look for an IWAD in, or the name of the IWAD desired.
//
// The global standard_iwads lists the standard IWAD names
//
// The result of search is returned in baseiwad, or set blank if none found
//
// IWAD search algorithm:
//
// Set customiwad blank
// If -iwad present set baseiwad to normalized path from -iwad parameter
//  If baseiwad is an existing file, thats it
//  If baseiwad is an existing dir, try appending all standard iwads
//  If haven't found it, and no : or / is in baseiwad,
//   append .wad if missing and set customiwad to baseiwad
//
// Look in . for customiwad if set, else all standard iwads
//
// Look in DoomExeDir. for customiwad if set, else all standard iwads
//
// If $DOOMWADDIR is an existing file
//  If customiwad is not set, thats it
//  else replace filename with customiwad, if exists thats it
// If $DOOMWADDIR is existing dir, try customiwad if set, else standard iwads
//
// If $HOME is an existing file
//  If customiwad is not set, thats it
//  else replace filename with customiwad, if exists thats it
// If $HOME is an existing dir, try customiwad if set, else standard iwads
//
// IWAD not found
//
// jff 4/19/98 Add routine to search for a standard or custom IWAD in one
// of the standard places. Returns a blank string if not found.
//
// killough 11/98: simplified, removed error-prone cut-n-pasted code
//
static void D_findIWADFile(qstring &iwad)
{
    static const char *envvars[] = { "DOOMWADDIR", "HOME" };
    qstring            customiwad;
    qstring            gameiwad;
    bool               isdir    = false;
    const char        *basename = nullptr;

    // haleyjd 01/01/11: support for DOOMWADPATH
    D_parseDoomWadPath();

    // haleyjd 11/15/12: if so marked, scan for IWADs. This is a one-time
    // only operation unless the user resets the value of d_scaniwads.
    // This will populate as many of the gi_path_* IWADs and w_* mission
    // packs as can be found amongst likely locations. User settings are
    // never overwritten by this process.
    if(d_scaniwads)
    {
        D_FindIWADs();
        d_scaniwads = false;
    }

    // jff 3/24/98 get -iwad parm if specified else use .
    int iwadparm;
    if((iwadparm = M_CheckParm("-iwad")) && iwadparm < myargc - 1)
        basename = myargv[iwadparm + 1];
    else
        basename = G_GFSCheckIWAD(); // haleyjd 04/16/03: GFS support

    // haleyjd 08/19/07: if -game was used and neither -iwad nor a GFS iwad
    // specification was used, start off by trying base/game/game.wad
    if(gamepathset && !basename)
    {
        gameiwad = basegamepath;
        gameiwad.pathConcatenate(myargv[gamepathparm]);
        gameiwad.addDefaultExtension(".wad");

        if(!access(gameiwad.constPtr(), R_OK)) // only if the file exists do we try to use it.
            basename = gameiwad.constPtr();
        else
        {
            // haleyjd 12/31/10: base/game/game.wad doesn't exist;
            // try matching against appropriate configured IWAD path(s)
            char *cfgpath = D_IWADPathForGame(myargv[gamepathparm]);
            if(cfgpath && !access(cfgpath, R_OK))
                basename = cfgpath;
        }
    }

    // jff 3/24/98 get -iwad parm if specified else use .
    if(basename)
    {
        iwad = basename;
        iwad.normalizeSlashes();

        if(WadFileStatus(iwad, &isdir))
        {
            if(!isdir)
                return;
            else
            {
                for(int i = 0; i < nstandard_iwads; i++)
                {
                    size_t n = iwad.length();
                    iwad.concat(standard_iwads[i]);
                    if(WadFileStatus(iwad, &isdir) && !isdir)
                        return;
                    iwad.truncate(n); // reset iwad length to former
                }
            }
        }
        else if(!iwad.strChr(':') && !iwad.strChr('/') && !iwad.strChr('\\'))
        {
            customiwad << "/" << iwad;
            customiwad.addDefaultExtension(".wad");
            customiwad.normalizeSlashes();
        }
    }
    else if(!gamepathset) // try wad picker
    {
        const char *name = D_doIWADMenu();
        if(name && *name)
        {
            iwad = name;
            iwad.normalizeSlashes();
            return;
        }
    }

    for(int j = 0; j < (gamepathset ? 3 : 2); j++)
    {
        switch(j)
        {
        case 0: //
            iwad = ".";
            break;
        case 1: //
            iwad = D_DoomExeDir();
            break;
        case 2: // haleyjd: try basegamepath too when -game was used
            iwad = basegamepath;
            break;
        }

        iwad.normalizeSlashes();

        // sf: only show 'looking in' for devparm
        if(devparm)
            printf("Looking in %s\n", iwad.constPtr()); // killough 8/8/98

        if(customiwad.length())
        {
            iwad.concat(customiwad);
            if(WadFileStatus(iwad, &isdir) && !isdir)
                return;
        }
        else
        {
            for(int i = 0; i < nstandard_iwads; i++)
            {
                size_t n = iwad.length();
                iwad.concat(standard_iwads[i]);
                if(WadFileStatus(iwad, &isdir) && !isdir)
                    return;
                iwad.truncate(n); // reset iwad length to former
            }
        }
    }

    // haleyjd 12/31/10: Try finding a match amongst configured IWAD paths
    if(customiwad.length())
    {
        char *cfgpath = D_IWADPathForIWADParam(customiwad.constPtr());
        if(cfgpath && !access(cfgpath, R_OK))
        {
            iwad = cfgpath;
            return;
        }
    }

    if(doomwadpaths.getLength()) // If at least one path is specified...
    {
        if(customiwad.length()) // -iwad was used with a file name?
        {
            if(D_FindInDoomWadPath(iwad, customiwad.constPtr(), ".wad"))
                return;
        }
        else
        {
            // Try all the standard iwad names in the normal order
            for(int i = 0; i < nstandard_iwads; i++)
            {
                if(D_FindInDoomWadPath(iwad, standard_iwads[i], ".wad"))
                    return;
            }
        }
    }

    for(size_t i = 0; i < earrlen(envvars); i++)
    {
        char *p;

        if((p = getenv(envvars[i])))
        {
            iwad = p;
            iwad.normalizeSlashes();
            if(WadFileStatus(iwad, &isdir))
            {
                if(!isdir)
                {
                    size_t slashPos;

                    if(!customiwad.length())
                    {
                        printf("Looking for %s\n", iwad.constPtr());
                        return; // killough 8/8/98
                    }
                    else if((slashPos = iwad.findLastOf('/')) != qstring::npos ||
                            (slashPos = iwad.findLastOf('\\')) != qstring::npos)
                    {
                        iwad.truncate(slashPos);
                        iwad.concat(customiwad);
                        printf("Looking for %s\n", iwad.constPtr()); // killough 8/8/98
                        if(WadFileStatus(iwad, &isdir) && !isdir)
                            return;
                    }
                }
                else
                {
                    if(devparm)                                     // sf: devparm only
                        printf("Looking in %s\n", iwad.constPtr()); // killough 8/8/98
                    if(customiwad.length())
                    {
                        iwad.concat(customiwad);
                        if(WadFileStatus(iwad, &isdir) && !isdir)
                            return;
                    }
                    else
                    {
                        for(int wn = 0; wn < nstandard_iwads; wn++)
                        {
                            size_t n = iwad.length();
                            iwad.concat(standard_iwads[wn]);
                            if(WadFileStatus(iwad, &isdir) && !isdir)
                                return;
                            iwad.truncate(n); // reset iwad length to former
                        }
                    } // end else (!*customiwad)
                } // end else (isdir)
            } // end if(WadFileStatus(...))
        } // end if((p = getenv(...)))
    } // end for

    iwad = "";
}

//
// D_loadResourceWad
//
// haleyjd 03/10/03: moved eternity.wad loading to this function
//
static void D_loadResourceWad()
{
    char  *filestr = nullptr;
    size_t len     = M_StringAlloca(&filestr, 1, 20, basegamepath);

    psnprintf(filestr, len, "%s/eternity.pke", basegamepath);

    // haleyjd 08/19/07: if not found, fall back to base/doom/eternity.pke
    if(access(filestr, R_OK))
        psnprintf(filestr, len, "%s/doom/eternity.pke", basepath);

    M_NormalizeSlashes(filestr);
    D_AddFile(filestr, lumpinfo_t::ns_global, nullptr, 0, DAF_NONE);

    modifiedgame = false; // reset, ignoring smmu.wad etc.
}

//
// D_identifyDisk
//
// haleyjd 05/31/10: IdentifyVersion subroutine for dealing with disk files.
//
static void D_identifyDisk()
{
    GameMode_t    gamemode;
    GameMission_t gamemission;

    printf("IWAD found: %s\n", diskiwad.name);

    // haleyjd: hardcoded for now
    if(disktype == DISK_DOOM2)
    {
        gamemode      = commercial;
        gamemission   = pack_disk;
        haswolflevels = true;
    }
    else
    {
        gamemode    = retail;
        gamemission = doom;
    }

    // setup gameModeInfo
    D_SetGameModeInfo(gamemode, gamemission);

    // haleyjd: load metadata from diskfile
    D_DiskMetaData();

    // set and display version name
    D_SetGameName(nullptr);

    // initialize game/data paths
    D_InitPaths();

    // haleyjd 03/10/03: add eternity.wad before the IWAD, at request of
    // fraggle -- this allows better compatibility with new IWADs
    D_loadResourceWad();

    // load disk IWAD
    D_LoadDiskFileIWAD();

    // haleyjd: load disk file pwad here, if one was specified
    if(diskpwad)
        D_LoadDiskFilePWAD();

    // 12/24/11: check for game folder hi-def music
    D_CheckGameMusic();

    // done with the diskfile structure
    D_CloseDiskFile(diskfile, false);
    diskfile = nullptr;
}

//
// D_identifyIWAD
//
// haleyjd 05/31/10: IdentifyVersion subroutine for dealing with normal IWADs.
//
static void D_identifyIWAD()
{
    qstring iwad;

    D_findIWADFile(iwad);

#if EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS
    if(iwad.empty())
    {
        extern bool    I_TryIWADSearchAgain();
        extern qstring I_OpenWindowsDirectory();

        qstring dirpath{};

        do
        {
            dirpath = I_OpenWindowsDirectory();
            if(dirpath.length())
            {
                D_CheckPathForWADs(dirpath);
                D_findIWADFile(iwad);
            }
        }
        while(iwad.empty() && !dirpath.empty() && I_TryIWADSearchAgain());
    }
#endif

    if(iwad.length())
    {
        iwadcheck_t version;

        printf("IWAD found: %s\n", iwad.constPtr()); // jff 4/20/98 print only if found

        version.gamemode    = indetermined;
        version.gamemission = none;
        version.hassecrets  = false;
        version.freedoom    = false;
        version.freedm      = false;
        version.bfgedition  = false;
        version.error       = false;
        version.flags       = IWADF_FATALNOTOPEN | IWADF_FATALNOTWAD;

        // joel 10/16/98 gamemission added
        D_CheckIWAD(iwad.constPtr(), version);

        // propagate some info to globals
        haswolflevels = version.hassecrets;
        freedoom      = version.freedoom;
        bfgedition    = version.bfgedition;

        // setup GameModeInfo
        D_SetGameModeInfo(version.gamemode, version.gamemission);

        // set and display version name
        D_SetGameName(iwad.constPtr());

        // initialize game/data paths
        D_InitPaths();

        // haleyjd 03/10/03: add eternity.wad before the IWAD, at request of
        // fraggle -- this allows better compatibility with new IWADs
        D_loadResourceWad();

        D_AddFile(iwad.constPtr(), lumpinfo_t::ns_global, nullptr, 0, DAF_IWAD);

        // 12/24/11: check for game folder hi-def music
        D_CheckGameMusic();
    }
    else
    {
        // haleyjd 08/20/07: improved error message for n00bs
        I_Error("\nIWAD not found!\n"
                "To specify an IWAD, try one of the following:\n"
                "* Configure IWAD file paths in user/system.cfg\n"
                "* Use -iwad\n"
                "* Set the DOOMWADDIR or DOOMWADPATH environment variables.\n"
                "* Place an IWAD in the working directory.\n"
                "* Place an IWAD file under the appropriate game folder of\n"
                "  the base directory and use the -game parameter.\n");
    }
}

//
// D_IdentifyVersion
//
// Set the location of the defaults file and the savegame root
// Locate and validate an IWAD file
// Determine gamemode from the IWAD
//
// supports IWADs with custom names. Also allows the -iwad parameter to
// specify which iwad is being searched for if several exist in one dir.
// The -iwad parm may specify:
//
// 1) a specific pathname, which must exist (.wad optional)
// 2) or a directory, which must contain a standard IWAD,
// 3) or a filename, which must be found in one of the standard places:
//   a) current dir,
//   b) exe dir
//   c) $DOOMWADDIR
//   d) or $HOME
//
// jff 4/19/98 rewritten to use a more advanced search algorithm
//
void D_IdentifyVersion()
{
    // haleyjd 05/28/10: check for -disk parameter
    D_CheckDiskFileParm();

    // if we loaded one, try finding an IWAD in it
    if(havediskfile)
        D_FindDiskFileIWAD();

    // locate the IWAD and determine game mode from it
    if(havediskiwad)
        D_identifyDisk();
    else
        D_identifyIWAD();
}

// EOF

