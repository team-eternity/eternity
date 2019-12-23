//
// The Eternity Engine
// Copyright (C) 2019 James Haley et al.
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
// Purpose: Code for loading data files, particularly PWADs.
//  Base, user, and game path determination and file-finding functions.
//
// Authors: James Haley, Ioan Chera, Max Waine
//

// MaxW: 2019/07/07: Moved over to C++17 filesystem
// haleyjd 08/20/07: POSIX opendir needed for autoload functionality
#if __cplusplus >= 201703L || _MSC_VER >= 1914
#include "hal/i_platform.h"
#if EE_CURRENT_PLATFORM == EE_PLATFORM_MACOSX
#include "hal/i_directory.h"
namespace fs = fsStopgap;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif
#include "z_zone.h"

#include "hal/i_directory.h"
#include "hal/i_platform.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "d_dehtbl.h"
#include "d_files.h"
#include "d_gi.h"
#include "d_io.h"
#include "d_main.h"
#include "doomstat.h"
#include "e_edf.h"
#include "e_fonts.h"
#include "e_weapons.h"
#include "g_gfs.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_utils.h"
#include "p_setup.h"
#include "p_skin.h"
#include "r_data.h"
#include "r_main.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "v_misc.h"
#include "w_formats.h"
#include "w_wad.h"
#include "xl_scripts.h"

// D_FIXME:
static char *D_CheckGameEDF();

// haleyjd 11/09/09: wadfiles made a structure.
// note: needed extern in g_game.c
wfileadd_t *wadfiles;

//=============================================================================
//
// WAD File Loading
//

static int numwadfiles, numwadfiles_alloc;

//
// D_reAllocFiles
//
// haleyjd 12/24/11: resize the wadfiles array
//
static void D_reAllocFiles()
{
   // sf: allocate for +2 for safety
   if(numwadfiles + 2 >= numwadfiles_alloc)
   {
      numwadfiles_alloc = numwadfiles_alloc ? numwadfiles_alloc * 2 : 8;

      wadfiles = erealloc(wfileadd_t *, wadfiles, numwadfiles_alloc * sizeof(*wadfiles));
   }
}

//
// D_AddFile
//
// Rewritten by Lee Killough
//
// killough 11/98: remove limit on number of files
// haleyjd 05/28/10: added f and baseoffset parameters for subfile support.
//
void D_AddFile(const char *file, int li_namespace, FILE *fp, size_t baseoffset,
               dafflags_e addflags)
{
   unsigned int flags;

   D_reAllocFiles();

   memset(&wadfiles[numwadfiles], 0, sizeof(*wadfiles));

   wadfiles[numwadfiles].filename     = estrdup(file);
   wadfiles[numwadfiles].li_namespace = li_namespace;
   wadfiles[numwadfiles].f            = fp;
   wadfiles[numwadfiles].baseoffset   = baseoffset;
   wadfiles[numwadfiles].requiredFmt  = -1;
   
   // haleyjd 10/27/12: setup flags

   // ioanch: check if it's a directory. Do not allow "subfiles" or special
   // addflags
   struct stat sbuf;
   if(addflags == DAF_NONE && !fp &&
      !stat(file, &sbuf) && S_ISDIR(sbuf.st_mode))
   {
      flags = WFA_DIRECTORY_ARCHIVE | WFA_OPENFAILFATAL;
   }
   else
      flags = WFA_ALLOWINEXACTFN | WFA_OPENFAILFATAL | WFA_ALLOWHACKS;

   // adding a subfile?
   if(fp)
   {
      wadfiles[numwadfiles].requiredFmt = W_FORMAT_WAD;
      flags |= (WFA_SUBFILE | WFA_REQUIREFORMAT);
      flags &= ~WFA_ALLOWHACKS; // don't hack subfiles.
   }

   // private directory?
   if(addflags & DAF_PRIVATE)
      flags |= WFA_PRIVATE;

   // haleyjd 10/3/12: must explicitly track what file has been added as the
   // IWAD now. Special thanks to id Software and their BFG Edition fuck-ups!
   if(addflags & DAF_IWAD)
      flags |= WFA_ISIWADFILE;

   // special check for demos - may actually be a lump name.
   if(addflags & DAF_DEMO)
      flags &= ~WFA_OPENFAILFATAL;

   wadfiles[numwadfiles].flags = flags;

   wadfiles[numwadfiles+1].filename = NULL; // sf: always NULL at end

   ++numwadfiles;
}

//
// D_AddDirectory
//
// haleyjd 12/24/11: add a directory to be loaded as if it's a wad file
//
void D_AddDirectory(const char *dir)
{
   D_reAllocFiles();

   wadfiles[numwadfiles].filename     = estrdup(dir);
   wadfiles[numwadfiles].li_namespace = lumpinfo_t::ns_global; // TODO?
   wadfiles[numwadfiles].f            = NULL;
   wadfiles[numwadfiles].baseoffset   = 0;

   // haleyjd 10/27/12: flags
   wadfiles[numwadfiles].flags = WFA_OPENFAILFATAL | WFA_DIRECTORY_RAW;

   wadfiles[numwadfiles+1].filename = NULL;

   ++numwadfiles;
}

//sf: console command to list loaded files
void D_ListWads()
{
   int i;
   C_Printf(FC_HI "Loaded WADs:\n");

   for(i = 0; i < numwadfiles; i++)
      C_Printf("%s\n", wadfiles[i].filename);
}

//=============================================================================
//
// haleyjd 03/10/03: GFS functions
//

void D_ProcessGFSDeh(gfs_t *gfs)
{
   int i;
   char *filename = NULL;

   for(i = 0; i < gfs->numdehs; ++i)
   {
      if(gfs->filepath)
      {
         filename = M_SafeFilePath(gfs->filepath, gfs->dehnames[i]);
      }
      else
      {
         filename = Z_Strdupa(gfs->dehnames[i]);
         M_NormalizeSlashes(filename);
      }

      if(access(filename, F_OK))
         I_Error("Couldn't open .deh or .bex %s\n", filename);

      D_QueueDEH(filename, 0); // haleyjd: queue it
   }
}

void D_ProcessGFSWads(gfs_t *gfs)
{
   int i;
   char *filename = NULL;

   // haleyjd 09/30/08: don't load GFS wads in shareware gamemodes
   if(GameModeInfo->flags & GIF_SHAREWARE)
   {
      startupmsg("D_ProcessGFSWads", "ignoring GFS wad files");
      return;
   }

   // haleyjd 06/21/04: GFS should mark modified game when wads are added!
   if(gfs->numwads > 0)
      modifiedgame = true;

   for(i = 0; i < gfs->numwads; ++i)
   {
      if(gfs->filepath)
      {
         filename = M_SafeFilePath(gfs->filepath, gfs->wadnames[i]);
      }
      else
      {
         filename = Z_Strdupa(gfs->wadnames[i]);
         M_NormalizeSlashes(filename);
      }

      if(access(filename, F_OK))
         I_Error("Couldn't open WAD file %s\n", filename);

      D_AddFile(filename, lumpinfo_t::ns_global, NULL, 0, DAF_NONE);
   }
}

void D_ProcessGFSCsc(gfs_t *gfs)
{
   int i;
   char *filename = NULL;

   for(i = 0; i < gfs->numcsc; ++i)
   {
      if(gfs->filepath)
      {
         filename = M_SafeFilePath(gfs->filepath, gfs->cscnames[i]);
      }
      else
      {
         filename = Z_Strdupa(gfs->cscnames[i]);
         M_NormalizeSlashes(filename);
      }

      if(access(filename, F_OK))
         I_Error("Couldn't open CSC file %s\n", filename);

      C_RunScriptFromFile(filename);
   }
}

//=============================================================================
//
// Loose File Support Functions 
// 
// These enable drag-and-drop support for Windows and possibly other OSes.
//

void D_LooseWads()
{
   int i;
   const char *dot;
   char *filename;

   for(i = 1; i < myargc; i++)
   {
      // stop at first param with '-' or '@'
      if(myargv[i][0] == '-' || myargv[i][0] == '@')
         break;

      // get extension (search from right end)
      dot = strrchr(myargv[i], '.');

      // check for extension
      if(!dot)
         continue;

      // allow any supported archive extension
      if(strncasecmp(dot, ".wad", 4) &&
         strncasecmp(dot, ".pke", 4) &&
         strncasecmp(dot, ".pk3", 4) &&
         strncasecmp(dot, ".zip", 4))
         continue;

      // add it
      filename = Z_Strdupa(myargv[i]);
      M_NormalizeSlashes(filename);
      modifiedgame = true;
      D_AddFile(filename, lumpinfo_t::ns_global, NULL, 0, DAF_NONE);
   }
}

void D_LooseDehs()
{
   const char *dot;
   char *filename;

   for(int i = 1; i < myargc; i++)
   {
      // stop at first param with '-' or '@'
      if(myargv[i][0] == '-' || myargv[i][0] == '@')
         break;

      // get extension (search from right end)
      dot = strrchr(myargv[i], '.');

      // check extension
      if(!dot || (strncasecmp(dot, ".deh", 4) &&
                  strncasecmp(dot, ".bex", 4)))
         continue;

      // add it
      filename = Z_Strdupa(myargv[i]);
      M_NormalizeSlashes(filename);
      D_QueueDEH(filename, 0);
   }
}

gfs_t *D_LooseGFS()
{
   const char *dot;

   for(int i = 1; i < myargc; i++)
   {
      // stop at first param with '-' or '@'
      if(myargv[i][0] == '-' || myargv[i][0] == '@')
         break;

      // get extension (search from right end)
      dot = strrchr(myargv[i], '.');

      // check extension
      if(!dot || strncasecmp(dot, ".gfs", 4))
         continue;

      printf("Found loose GFS file %s\n", myargv[i]);

      // process only the first GFS found
      return G_LoadGFS(myargv[i]);
   }

   return NULL;
}

//
// D_LooseDemo
//
// Looks for a loose LMP file on the command line, to support
// drag-and-drop for demos.
//
const char *D_LooseDemo()
{
   const char *dot;
   const char *ret = NULL;

   for(int i = 1; i < myargc; i++)
   {
      // stop at first param with '-' or '@'
      if(myargv[i][0] == '-' || myargv[i][0] == '@')
         break;

      // get extension (search from right end)
      dot = strrchr(myargv[i], '.');

      // check extension
      if(!dot || strncasecmp(dot, ".lmp", 4))
         continue;
      
      ret = myargv[i];
      break; // process only the first demo found
   }

   return ret;
}

//
// D_LooseEDF
//
// Looks for a loose EDF file on the command line, to support
// drag-and-drop.
//
bool D_LooseEDF(char **buffer)
{
   const char *dot;

   for(int i = 1; i < myargc; i++)
   {
      // stop at first param with '-' or '@'
      if(myargv[i][0] == '-' || myargv[i][0] == '@')
         break;

      // get extension (search from right end)
      dot = strrchr(myargv[i], '.');

      // check extension
      if(!dot || strncasecmp(dot, ".edf", 4))
         continue;

      *buffer = Z_Strdupa(myargv[i]);
      return true; // process only the first EDF found
   }

   return false;
}

//=============================================================================
//
// EDF Loading
//

//
// D_LoadEDF
//
// Identifies the root EDF file, and then calls E_ProcessEDF.
//
void D_LoadEDF(gfs_t *gfs)
{
   int i;
   char *edfname = NULL;
   const char *shortname = NULL;

   // command line takes utmost precedence
   if((i = M_CheckParm("-edf")) && i < myargc - 1)
   {
      // command-line EDF file found
      edfname = Z_Strdupa(myargv[i + 1]);
      M_NormalizeSlashes(edfname);
   }
   else if(gfs && (shortname = G_GFSCheckEDF()))
   {
      // GFS specified an EDF file
      // haleyjd 09/10/11: bug fix - don't assume gfs->filepath is valid
      if(gfs->filepath)
         edfname = M_SafeFilePath(gfs->filepath, shortname);
      else
      {
         edfname = Z_Strdupa(shortname);
         M_NormalizeSlashes(edfname);
      }
   }
   else
   {
      // use default
      if(!D_LooseEDF(&edfname)) // check for loose files (drag and drop)
      {
         char *fn;

         // haleyjd 08/20/07: check for root.edf in base/game first
         if((fn = D_CheckGameEDF()))
            edfname = fn;
         else
            edfname = M_SafeFilePath(basepath, "root.edf");

         // disable other game modes' definitions implicitly ONLY
         // when using the default root.edf
         // also, allow command line toggle
         if(!M_CheckParm("-edfenables"))
         {
            if(GameModeInfo->type == Game_Heretic)
               E_EDFSetEnableValue("DOOM", 0);
            else
               E_EDFSetEnableValue("HERETIC", 0);
         }
      }
   }

   E_ProcessEDF(edfname);

   D_InitWeaponInfo();
}

//=============================================================================
//
// SMMU Runtime WAD File Loading
//

//
// D_reInitWadfiles
//
// Re-init everything after loading a wad
//
static void D_reInitWadfiles()
{
   R_FreeData();
   E_ReloadFonts();        // needed because font patches may change without EDF
   E_ProcessNewEDF();      // haleyjd 03/24/10: process any new EDF lumps
   XL_ParseHexenScripts(); // haleyjd 03/27/11: process Hexen scripts
   D_ProcessDEHQueue();    // haleyjd 09/12/03: run any queued DEHs
   C_InitBackdrop();       // update the console background
   R_Init();
   P_Init();
   ST_Init();
}

// FIXME: various parts of this routine need tightening up
void D_NewWadLumps(int source)
{
   int numlumps = wGlobalDir.getNumLumps();
   lumpinfo_t **lumpinfo = wGlobalDir.getLumpInfo();

   for(int i = 0; i < numlumps; i++)
   {
      if(lumpinfo[i]->source != source)
         continue;

      // new sound
      if(!strncmp(lumpinfo[i]->name, "DSCHGUN",8)) // chaingun sound
      {
         S_Chgun();
         continue;
      }
      // haleyjd 03/26/11: sounds are not handled here any more
      // haleyjd 04/10/11: music is not handled here now either

      // skins
      if(!strncmp(lumpinfo[i]->name, "S_SKIN", 6))
      {
         P_ParseSkin(i);
         continue;
      }
   }
}

// add a new .wad file
// returns true if successfully loaded

bool D_AddNewFile(const char *s)
{
   Console.showprompt = false;
   if(!wGlobalDir.addNewFile(s))
      return false;
   modifiedgame = true;
   D_AddFile(s, lumpinfo_t::ns_global, NULL, 0, DAF_NONE); // add to the list of wads
   C_SetConsole();
   D_reInitWadfiles();
   return true;
}

//=============================================================================
//
// Base and Game Path Determination Code
//

// return codes for D_CheckBasePath
enum
{
   BASE_ISGOOD,
   BASE_NOTEXIST,
   BASE_NOTDIR,
   BASE_CANTOPEN,
   BASE_NOTEEBASE,
   BASE_NUMCODES
};

//
// D_CheckBasePath
//
// Checks a provided path to see that it both exists and that it is a directory
// and not a plain file.
//
static int D_CheckBasePath(const qstring &qpath)
{
   int ret = -1;
   qstring str;
   fs::directory_entry path;

   str = qpath;

   // Rub out any ending slashes; stat does not like them.
   str.rstrip('\\');
   str.rstrip('/');


   path = fs::directory_entry(str.constPtr());

   if(path.exists()) // check for existence
   {
      if(path.is_directory()) // check that it's a directory
      {
         int score = 0;

         const fs::directory_iterator itr(path);
         for(const fs::directory_entry ent : itr)
         {
            const qstring filename = qstring(ent.path().filename().generic_u8string().c_str()).toLower();

            if(filename == "startup.wad")
               ++score;
            else if(filename == "root.edf")
               ++score;
            else if(filename == "doom")
               ++score;
         }

         if(score >= 3)
            ret = BASE_ISGOOD;    // Got it.
         else
            ret = BASE_NOTEEBASE; // Doesn't look like EE's base folder.
      }
      else
         ret = BASE_NOTDIR; // S_ISDIR failed
   }
   else
      ret = BASE_NOTEXIST; // stat failed

   return ret;
}

// basepath sources
enum
{
   BASE_CMDLINE,
   BASE_ENVIRON,
   BASE_HOMEDIR,
   BASE_INSTALL,
   BASE_WORKING,
   BASE_EXEDIR,
   BASE_BASEPARENT, // for user dir only
   BASE_NUMBASE
};

//
// D_SetBasePath
//
// haleyjd 11/23/06: Sets the path to the "base" folder, where Eternity stores
// all of its data.
//
void D_SetBasePath()
{
   int p, res = BASE_NOTEXIST, source = BASE_NUMBASE;
   const char *s;
   qstring basedir;

   // Priority:
   // 1. Command-line argument "-base"
   // 2. Environment variable "ETERNITYBASE"
   // 3. OS-specific install directory.
   // 4. /base under DoomExeDir
   // 5. /base under working directory

   // check command-line
   if((p = M_CheckParm("-base")) && p < myargc - 1)
   {
      basedir = myargv[p + 1];

      if((res = D_CheckBasePath(basedir)) == BASE_ISGOOD)
         source = BASE_CMDLINE;
   }

   // check environment
   if(res != BASE_ISGOOD && (s = getenv("ETERNITYBASE")))
   {
      basedir = s;

      if((res = D_CheckBasePath(basedir)) == BASE_ISGOOD)
         source = BASE_ENVIRON;
   }

   // check OS-specific install dir
   if(res != BASE_ISGOOD && (s = I_PlatformInstallDirectory()))
   {
      basedir = s;

      if((res = D_CheckBasePath(basedir)) == BASE_ISGOOD)
         source = BASE_INSTALL;
   }

   // check exe dir
   if(res != BASE_ISGOOD)
   {
      basedir = D_DoomExeDir();
      basedir.pathConcatenate("/base");

      if((res = D_CheckBasePath(basedir)) == BASE_ISGOOD)
         source = BASE_EXEDIR;
   }

   // check working dir
   if(res != BASE_ISGOOD)
   {
      basedir = "./base";

      if((res = D_CheckBasePath(basedir)) == BASE_ISGOOD)
         source = BASE_WORKING;
      else
      {
         // final straw.
         static const char *errmsgs[BASE_NUMCODES] =
         {
            "is FUBAR", // ???
            "does not exist",
            "is not a directory",
            "cannot be opened",
            "is not an Eternity base path"
         };

         I_Error("D_SetBasePath: base path %s.\n", errmsgs[res]);
      }
   }

   basedir.normalizeSlashes();
   basepath = basedir.duplicate();

   switch(source)
   {
   case BASE_CMDLINE:
      s = "by command line";
      break;
   case BASE_ENVIRON:
      s = "by environment";
      break;
   case BASE_INSTALL:
      s = "to install directory";
      break;
   case BASE_WORKING:
      s = "to working directory";
      break;
   case BASE_EXEDIR:
      s = "to executable directory";
      break;
   default:
      s = "to God only knows what"; // ???
      break;
   }

   printf("Base path set %s.\n", s);
}

#if EE_CURRENT_PLATFORM == EE_PLATFORM_LINUX
static const char *const userdirs[] =
{
   "/doom",
   "/doom2",
   "/hacx",
   "/heretic",
   "/plutonia",
   "/shots",
   "/tnt",
};
#endif

//
// D_CheckUserPath
//
// Checks a provided path to see that it both exists and that it is a directory
// and not a plain file.
//
static int D_CheckUserPath(const qstring &qpath)
{
   int ret = -1;
   qstring str;
   fs::directory_entry path;

   str = qpath;

   // Rub out any ending slashes; stat does not like them.
   str.rstrip('\\');
   str.rstrip('/');

   path = fs::directory_entry(str.constPtr());

   if(path.exists()) // check for existence
   {
      if(path.is_directory()) // check that it's a directory
      {
         int score = 0;

         const fs::directory_iterator itr(path);
         for(const fs::directory_entry ent : itr)
         {
            const qstring filename = qstring(ent.path().filename().generic_u8string().c_str()).toLower();

            if(filename == "doom")
               ++score;
            else if(filename == "shots")
               ++score;
         }

         if(score >= 2)
            ret = BASE_ISGOOD;    // Got it.
         else
            ret = BASE_NOTEEBASE; // Doesn't look like EE's base folder.
      }
      else
         ret = BASE_NOTDIR; // S_ISDIR failed
   }
   else
      ret = BASE_NOTEXIST; // stat failed

   return ret;
}


//
// D_SetUserPath
//
// haleyjd 11/23/06: Sets the path to the "user" folder, where Eternity stores
// all of its data.
//
void D_SetUserPath()
{
   int p, res = BASE_NOTEXIST, source = BASE_NUMBASE;
   const char *s;
   qstring userdir;

   // Priority:
   // 1. Command-line argument "-user"
   // 2. Environment variable "ETERNITYUSER"
   // 3. OS-specific home directory.
   // 4. /user under DoomExeDir
   // 5. /user under working directory
   // 6. basepath/../user
   // 7. use basepath itself.

   // check command-line
   if((p = M_CheckParm("-user")) && p < myargc - 1)
   {
      userdir = myargv[p + 1];

      if((res = D_CheckUserPath(userdir)) == BASE_ISGOOD)
         source = BASE_CMDLINE;
   }

   // check environment
   if(res != BASE_ISGOOD && (s = getenv("ETERNITYUSER")))
   {
      userdir = s;

      if((res = D_CheckUserPath(userdir)) == BASE_ISGOOD)
         source = BASE_ENVIRON;
   }

   // check OS-specific home dir
#if EE_CURRENT_PLATFORM == EE_PLATFORM_LINUX
   if(res != BASE_ISGOOD)
   {
      qstring tmp;

      // Under Linux (and POSIX generally) use $XDG_CONFIG_HOME/eternity/user.
      if((s = getenv("XDG_CONFIG_HOME")) && *s)
      {
         userdir = s;
      }
      // But fall back to $HOME/.config/eternity/user.
      else if((s = getenv("HOME")))
      {
         userdir = s;
         I_CreateDirectory(userdir.pathConcatenate("/.config"));
      }

      if(s)
      {
         // Try to create this directory and populate it with needed directories.
         I_CreateDirectory(userdir.pathConcatenate("/eternity"));
         if(I_CreateDirectory(userdir.pathConcatenate("/user")))
         {
            for(size_t i = 0; i != earrlen(userdirs); i++)
               I_CreateDirectory((tmp = userdir).pathConcatenate(userdirs[i]));
         }

         if((res = D_CheckUserPath(userdir)) == BASE_ISGOOD)
            source = BASE_HOMEDIR;
      }
   }
#endif

   // check exe dir
   if(res != BASE_ISGOOD)
   {
      userdir = D_DoomExeDir();
      userdir.pathConcatenate("/user");

      if((res = D_CheckUserPath(userdir)) == BASE_ISGOOD)
         source = BASE_EXEDIR;
   }

   // check working dir
   if(res != BASE_ISGOOD)
   {
      userdir = "./user";

      if((res = D_CheckUserPath(userdir)) == BASE_ISGOOD)
         source = BASE_WORKING;
   }

   // try /user under the base path's immediate parent directory
   if(res != BASE_ISGOOD)
   {
      userdir = basepath;
      userdir.pathConcatenate("/../user");

      if((res = D_CheckUserPath(userdir)) == BASE_ISGOOD)
         source = BASE_BASEPARENT;
   }

   // last straw: use base path - may not work as it is not guaranteed to be a
   // writable location
   if(res != BASE_ISGOOD)
      userdir = basepath;

   userdir.normalizeSlashes();
   userpath = userdir.duplicate();

   switch(source)
   {
   case BASE_CMDLINE:
      s = "by command line";
      break;
   case BASE_ENVIRON:
      s = "by environment";
      break;
   case BASE_HOMEDIR:
      s = "to home directory";
      break;
   case BASE_WORKING:
      s = "to working directory";
      break;
   case BASE_EXEDIR:
      s = "to executable directory";
      break;
   case BASE_BASEPARENT:
      s = "to basepath/../user";
      break;
   default:
      s = "to base directory (warning: writes may fail!)"; // ???
      break;
   }

   printf("User path set %s.\n", s);
}

// haleyjd 8/18/07: if true, the game path has been set
bool gamepathset;

// haleyjd 8/19/07: index of the game name as specified on the command line
int gamepathparm;

//
// D_VerifyGamePath
//
// haleyjd 02/05/12: Verifies one of the two gamepaths (under /base and /user)
//
static int D_VerifyGamePath(const char *path)
{
   int ret;
   struct stat sbuf;

   if(!stat(path, &sbuf)) // check for existence
   {
      if(S_ISDIR(sbuf.st_mode)) // check that it's a directory
         ret = BASE_ISGOOD;
      else
         ret = BASE_NOTDIR;
   }
   else
      ret = BASE_NOTEXIST;

   return ret;
}

//
// D_CheckGamePathParam
//
// haleyjd 08/18/07: This function checks for the -game command-line parameter.
// If it is set, then its value is saved and gamepathset is asserted.
//
void D_CheckGamePathParam()
{
   int p;

   if((p = M_CheckParm("-game")) && p < myargc - 1)
   {
      int gameresult, ugameresult;
      char *gamedir  = M_SafeFilePath(basepath, myargv[p + 1]);
      char *ugamedir = M_SafeFilePath(userpath, myargv[p + 1]);
      
      gamepathparm = p + 1;

      gameresult  = D_VerifyGamePath(gamedir);
      ugameresult = D_VerifyGamePath(ugamedir);

      if(gameresult == BASE_ISGOOD && ugameresult == BASE_ISGOOD)
      {
         basegamepath = estrdup(gamedir);
         usergamepath = estrdup(ugamedir);
         gamepathset = true;
      }
      else if(gameresult != BASE_ISGOOD)
      {
         switch(gameresult)
         {
         case BASE_NOTDIR:
            I_Error("Game path %s is not a directory.\n", gamedir);
            break;
         case BASE_NOTEXIST:
            I_Error("Game path %s does not exist.\n", gamedir);
            break;
         }
      }
      else
      {
         switch(ugameresult)
         {
         case BASE_NOTDIR:
            I_Error("Game path %s is not a directory.\n", ugamedir);
            break;
         case BASE_NOTEXIST:
            I_Error("Game path %s does not exist.\n", ugamedir);
            break;
         }
      }
   }
}

//
// D_SetGamePath
//
// haleyjd 11/23/06: Sets the game path under the base path when the gamemode has
// been determined by the IWAD in use.
//
void D_SetGamePath()
{
   const char *mstr = GameModeInfo->missionInfo->gamePathName;
   char *gamedir    = M_SafeFilePath(basepath, mstr);
   char *ugamedir   = M_SafeFilePath(userpath, mstr);
   int gameresult, ugameresult;

   gameresult  = D_VerifyGamePath(gamedir);
   ugameresult = D_VerifyGamePath(ugamedir);

   if(gameresult == BASE_ISGOOD && ugameresult == BASE_ISGOOD)
   {
      basegamepath = estrdup(gamedir);
      usergamepath = estrdup(ugamedir);
   }
   else if(gameresult != BASE_ISGOOD)
   {
      switch(gameresult)
      {
      case BASE_NOTDIR:
         I_Error("Game path %s is not a directory.\n", gamedir);
         break;
      case BASE_NOTEXIST:
         I_Error("Game path %s does not exist.\n", gamedir);
         break;
      }
   }
   else
   {
      switch(ugameresult)
      {
      case BASE_NOTDIR:
         I_Error("Game path %s is not a directory.\n", ugamedir);
         break;
      case BASE_NOTEXIST:
         I_Error("Game path %s does not exist.\n", ugamedir);
         break;
      }
   }
}

//
// D_CheckGamePathFile
//
// Check for a file or directory in the user or base gamepath, preferring the
// former over the latter when it exists. Returns the path of the file to use,
// or NULL if neither location has that file.
//
static char *D_CheckGamePathFile(const char *name, bool isDir)
{
   struct stat sbuf;   

   // check for existence under user/<game>
   char *fullpath = M_SafeFilePath(usergamepath, name);
   if(!stat(fullpath, &sbuf)) 
   {
      // check that it is or is not a directory as requested
      if(S_ISDIR(sbuf.st_mode) == isDir) 
         return fullpath;
   }

   // check for existence under base/<game>
   fullpath = M_SafeFilePath(basegamepath, name);
   if(!stat(fullpath, &sbuf))
   {      
      if(S_ISDIR(sbuf.st_mode) == isDir)
         return fullpath;
   }

   // not found, or not a file or directory as expected
   return NULL;
}


//
// D_CheckGameEDF
//
// Looks for an optional root.edf file in base/game
//
static char *D_CheckGameEDF()
{
   return D_CheckGamePathFile("root.edf", false);
}

//
// D_CheckGameMusic
//
// haleyjd 12/24/11: Looks for an optional music directory in base/game,
// provided that s_hidefmusic is enabled.
//
void D_CheckGameMusic()
{
   if(s_hidefmusic)
   {
      char *music_dir = D_CheckGamePathFile("music", true);
      if(music_dir)
         D_AddDirectory(music_dir); // add as if it's a wad file
   }
}

//=============================================================================
//
// Gamepath Autoload Directory Handling
//

// haleyjd 08/20/07: gamepath autload directory structure
static fs::path autoloads;
static qstring               autoload_dirname;

//
// D_EnumerateAutoloadDir
//
// haleyjd 08/20/07: this function enumerates the base/game/autoload directory.
//
void D_EnumerateAutoloadDir()
{
   if(autoloads.empty() && !M_CheckParm("-noload")) // don't do if -noload is used
   {
      char *autoDir;

      if((autoDir = D_CheckGamePathFile("autoload", true)))
      {
         autoload_dirname = autoDir;
         autoloads = fs::path(autoload_dirname.constPtr());
      }
   }
}

//
// D_GameAutoloadWads
//
// Loads all wad files in the gamepath autoload directory.
//
void D_GameAutoloadWads()
{
   char *fn = nullptr;

   if(!autoloads.empty())
   {
      // haleyjd 09/30/08: not in shareware gamemodes, otherwise having any wads
      // in your base/game/autoload directory will make shareware unplayable
      if(GameModeInfo->flags & GIF_SHAREWARE)
      {
         startupmsg("D_GameAutoloadWads", "shareware; ignoring gamepath autoload wad files");
         return;
      }

      const fs::directory_iterator itr(autoloads);
      for(const fs::directory_entry ent : itr)
      {
         if(ent.path().extension() == ".wad")
         {
            fn = M_SafeFilePath(autoload_dirname.constPtr(),
                                ent.path().filename().generic_u8string().c_str());
            D_AddFile(fn, lumpinfo_t::ns_global, nullptr, 0, DAF_NONE);
         }
      }
   }
}

//
// D_GameAutoloadDEH
//
// Queues all deh/bex files in the gamepath autoload directory.
//
void D_GameAutoloadDEH()
{
   char *fn = nullptr;

   if(!autoloads.empty())
   {
      const fs::directory_iterator itr(autoloads);
      for(const fs::directory_entry ent : itr)
      {

         if(const fs::path extension = ent.path().extension();
            extension == ".deh" || extension == ".bex")
         {
            fn = M_SafeFilePath(autoload_dirname.constPtr(),
                                ent.path().filename().generic_u8string().c_str());
            D_QueueDEH(fn, 0);
         }
      }
   }
}

//
// D_GameAutoloadCSC
//
// Runs all console scripts in the base/game/autoload directory.
//
void D_GameAutoloadCSC()
{
   char *fn = nullptr;

   if(!autoloads.empty())
   {
      const fs::directory_iterator itr(autoloads);
      for(const fs::directory_entry ent : itr)
      {
         if(ent.path().extension() == ".csc")
         {
            fn = M_SafeFilePath(autoload_dirname.constPtr(),
                                ent.path().filename().generic_u8string().c_str());
            C_RunScriptFromFile(fn);
         }
      }
   }
}

//
// D_CloseAutoloadDir
//
// haleyjd 08/20/07: closes the base/game/autoload directory.
//
void D_CloseAutoloadDir()
{
   if(!autoloads.empty())
       autoloads = fs::path();
   autoload_dirname.freeBuffer();
}

// EOF

