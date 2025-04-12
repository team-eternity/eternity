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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//
// GFS -- Game File Script
//
// This is a radical new way for DOOM editing projects to provide
// file information to Eternity. A single GFS script, when provided
// via the -gfs command-line parameter, will affect the loading of
// an arbitrary number of WADs, DEH/BEX's, EDF files, and console
// scripts.
//
// By James Haley
//
//--------------------------------------------------------------------------

#define NEED_EDF_DEFINITIONS

#include "z_zone.h"

#include "Confuse/confuse.h"

#include "i_system.h"
#include "doomtype.h"
#include "g_gfs.h"
#include "m_utils.h"
#include "e_lib.h"

static gfs_t gfs;

constexpr const char SEC_WADFILE[]  = "wadfile";
constexpr const char SEC_DEHFILE[]  = "dehfile";
constexpr const char SEC_CSCFILE[]  = "cscfile";
constexpr const char SEC_EDFFILE[]  = "edffile";
constexpr const char SEC_IWAD[]     = "iwad";
constexpr const char SEC_BASEPATH[] = "basepath";

static cfg_opt_t gfs_opts[] =
{
   CFG_STR(SEC_WADFILE,   nullptr, CFGF_MULTI),
   CFG_STR(SEC_DEHFILE,   nullptr, CFGF_MULTI),
   CFG_STR(SEC_CSCFILE,   nullptr, CFGF_MULTI),
   CFG_STR(SEC_EDFFILE,   nullptr, CFGF_NONE),
   CFG_STR(SEC_IWAD,      nullptr, CFGF_NONE),
   CFG_STR(SEC_BASEPATH,  nullptr, CFGF_NONE),
   CFG_END()
};

gfs_t *G_LoadGFS(const char *filename)
{
   static bool loaded = false;
   int i;
   cfg_t *cfg;

   // only one GFS can be loaded per session
   if(loaded)
      return nullptr;

   cfg = cfg_init(gfs_opts, CFGF_NOCASE);

   cfg_set_error_function(cfg, E_ErrorCB);

   if(cfg_parse(cfg, filename))
      I_Error("G_LoadGFS: failed to parse GFS file\n");

   // count number of options
   gfs.numwads = cfg_size(cfg, SEC_WADFILE);
   gfs.numdehs = cfg_size(cfg, SEC_DEHFILE);
   gfs.numcsc  = cfg_size(cfg, SEC_CSCFILE);

   if(gfs.numwads)
      gfs.wadnames = ecalloc(char **, gfs.numwads, sizeof(char *));

   if(gfs.numdehs)
      gfs.dehnames = ecalloc(char **, gfs.numdehs, sizeof(char *));

   if(gfs.numcsc)
      gfs.cscnames = ecalloc(char **, gfs.numcsc,  sizeof(char *));

   // load wads, dehs, csc
   for(i = 0; i < gfs.numwads; i++)
   {
      const char *str = cfg_getnstr(cfg, SEC_WADFILE, i);

      gfs.wadnames[i] = estrdup(str);
   }
   for(i = 0; i < gfs.numdehs; i++)
   {
      const char *str = cfg_getnstr(cfg, SEC_DEHFILE, i);
      
      gfs.dehnames[i] = estrdup(str);
   }
   for(i = 0; i < gfs.numcsc; i++)
   {
      const char *str = cfg_getnstr(cfg, SEC_CSCFILE, i);

      gfs.cscnames[i] = estrdup(str);
   }

   // haleyjd 07/05/03: support root EDF specification
   if(cfg_size(cfg, SEC_EDFFILE) >= 1)
   {
      const char *str = cfg_getstr(cfg, SEC_EDFFILE);

      gfs.edf = estrdup(str);

      gfs.hasEDF = true;
   }

   // haleyjd 04/16/03: support iwad specification for end-users
   // (this is not useful to mod authors, UNLESS their mod happens
   // to be an IWAD file ;)
   if(cfg_size(cfg, SEC_IWAD) >= 1)
   {
      const char *str = cfg_getstr(cfg, SEC_IWAD);

      gfs.iwad = estrdup(str);

      gfs.hasIWAD = true;
   }

   // haleyjd 04/16/03: basepath support
   if(cfg_size(cfg, SEC_BASEPATH) >= 1)
   {
      const char *str = cfg_getstr(cfg, SEC_BASEPATH);

      gfs.filepath = estrdup(str);
   }
   else
   {
      gfs.filepath = emalloc(char *, strlen(filename) + 1);
      M_GetFilePath(filename, gfs.filepath, strlen(filename));
   }

   cfg_free(cfg);

   loaded = true;

   return &gfs;
}

void G_FreeGFS(gfs_t *lgfs)
{
   int i;

   // free all filenames, and then free the arrays they were in
   // as well

   for(i = 0; i < lgfs->numwads; i++)
   {
      efree(lgfs->wadnames[i]);
   }
   efree(lgfs->wadnames);

   for(i = 0; i < lgfs->numdehs; i++)
   {
      efree(lgfs->dehnames[i]);
   }
   efree(lgfs->dehnames);

   for(i = 0; i < lgfs->numcsc; i++)
   {
      efree(lgfs->cscnames[i]);
   }
   efree(lgfs->cscnames);

   if(lgfs->edf)
      efree(lgfs->edf);
   lgfs->edf = nullptr;
   lgfs->hasEDF = false;

   if(lgfs->iwad)
      efree(lgfs->iwad);
   lgfs->iwad = nullptr;
   lgfs->hasIWAD = false;

   if(lgfs->filepath)
      efree(lgfs->filepath);
   lgfs->filepath = nullptr;
}

//
// G_GFSCheckIWAD
//
// Checks to see if a GFS IWAD has been loaded.
// Convenience function to avoid rewriting too much of FindIWADFile
// Only valid while the GFS is still loaded; returns nullptr otherwise,
// and when there is no GFS IWAD specified.
//
const char *G_GFSCheckIWAD(void)
{
   if(gfs.hasIWAD)
      return gfs.iwad;
   else
      return nullptr;
}

//
// G_GFSCheckEDF
//
// Like above, but checks if an EDF file has been loaded
//
const char *G_GFSCheckEDF(void)
{
   if(gfs.hasEDF)
      return gfs.edf;
   else
      return nullptr;
}

// EOF

