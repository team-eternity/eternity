// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2012 James Haley
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
//    Code for loading data files, particularly PWADs.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "d_dehtbl.h"
#include "d_gi.h"
#include "d_io.h"
#include "d_main.h"
#include "doomstat.h"
#include "e_edf.h"
#include "g_gfs.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"
#include "p_setup.h"
#include "p_skin.h"
#include "r_data.h"
#include "r_main.h"
#include "s_sound.h"
#include "v_misc.h"
#include "w_wad.h"
#include "xl_scripts.h"

// D_FIXME:
char *D_CheckGameEDF();

// haleyjd 11/09/09: wadfiles made a structure.
// note: needed extern in g_game.c
wfileadd_t *wadfiles;

// set from iwad: level to start new games from
char firstlevel[9] = "";

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
               int privatedir)
{
   D_reAllocFiles();

   wadfiles[numwadfiles].filename     = estrdup(file);
   wadfiles[numwadfiles].li_namespace = li_namespace;
   wadfiles[numwadfiles].f            = fp;
   wadfiles[numwadfiles].baseoffset   = baseoffset;
   wadfiles[numwadfiles].privatedir   = privatedir;
   wadfiles[numwadfiles].directory    = false;

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
   wadfiles[numwadfiles].privatedir   = 0;
   wadfiles[numwadfiles].directory    = true;

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

      D_AddFile(filename, lumpinfo_t::ns_global, NULL, 0, 0);
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

   for(i = 1; i < myargc; ++i)
   {
      // stop at first param with '-' or '@'
      if(myargv[i][0] == '-' || myargv[i][0] == '@')
         break;

      // get extension (search from right end)
      dot = strrchr(myargv[i], '.');

      // check extension
      if(!dot || strncasecmp(dot, ".wad", 4))
         continue;

      // add it
      filename = Z_Strdupa(myargv[i]);
      M_NormalizeSlashes(filename);
      modifiedgame = true;
      D_AddFile(filename, lumpinfo_t::ns_global, NULL, 0, 0);
   }
}

void D_LooseDehs()
{
   int i;
   const char *dot;
   char *filename;

   for(i = 1; i < myargc; ++i)
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
   int i;
   const char *dot;

   for(i = 1; i < myargc; ++i)
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
   int i;
   const char *dot;
   const char *ret = NULL;

   for(i = 1; i < myargc; ++i)
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
   int i;
   const char *dot;

   for(i = 1; i < myargc; ++i)
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

   // haleyjd FIXME: temporary hacks
   D_InitWeaponInfo();
}

//=============================================================================
//
// SMMU Runtime WAD File Loading
//

// re init everything after loading a wad

static void D_reInitWadfiles()
{
   R_FreeData();
   E_ProcessNewEDF();      // haleyjd 03/24/10: process any new EDF lumps
   XL_ParseHexenScripts(); // haleyjd 03/27/11: process Hexen scripts
   D_ProcessDEHQueue();    // haleyjd 09/12/03: run any queued DEHs
   R_Init();
   P_Init();
}

// FIXME: various parts of this routine need tightening up
void D_NewWadLumps(FILE *handle)
{
   int i, format;
   char wad_firstlevel[9];
   int numlumps = wGlobalDir.getNumLumps();
   lumpinfo_t **lumpinfo = wGlobalDir.getLumpInfo();

   memset(wad_firstlevel, 0, 9);

   for(i = 0; i < numlumps; ++i)
   {
      if(lumpinfo[i]->file != handle)
         continue;

      // haleyjd: changed check for "THINGS" lump to a fullblown
      // P_CheckLevel call -- this should fix some problems with
      // some crappy wads that have partial levels sitting around

      if((format = P_CheckLevel(&wGlobalDir, i)) != LEVEL_FORMAT_INVALID) // a level
      {
         char *name = lumpinfo[i]->name;

         // ignore ones called 'start' as these are checked elsewhere
         if((!*wad_firstlevel && strcmp(name, "START")) ||
            strncmp(name, wad_firstlevel, 8) < 0)
            strncpy(wad_firstlevel, name, 8);

         // haleyjd: move up to the level's last lump
         i += (format == LEVEL_FORMAT_HEXEN ? 11 : 10);
         continue;
      }

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

   if(*wad_firstlevel && (!*firstlevel ||
      strncmp(wad_firstlevel, firstlevel, 8) < 0)) // a new first level?
      strcpy(firstlevel, wad_firstlevel);
}

// add a new .wad file
// returns true if successfully loaded

bool D_AddNewFile(const char *s)
{
   Console.showprompt = false;
   if(wGlobalDir.addNewFile(s))
      return false;
   modifiedgame = true;
   D_AddFile(s, lumpinfo_t::ns_global, NULL, 0, 0);   // add to the list of wads
   C_SetConsole();
   D_reInitWadfiles();
   return true;
}

// EOF

