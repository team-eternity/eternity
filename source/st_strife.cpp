//
// Copyright (C) 2020 James Haley, Max Waine, et al.
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
//----------------------------------------------------------------------------
//
// Purpose: Strife Status Bar
// Authors: Samuel Villarreal, James Haley, Max Waine
//

#include "z_zone.h"
#include "doomstat.h"
#include "d_dehtbl.h"
#include "e_inventory.h"
#include "hu_inventory.h"
#include "metaapi.h"
#include "r_patch.h"
#include "v_patchfmt.h"
#include "v_video.h"
#include "st_stuff.h"

//
// Defines
//
#define ST_SBARHEIGHT 32
#define plyr (&players[displayplayer])

//
// Static variables
//

// cached patches
static patch_t *greeninvnums[10];  // green  inventory numbers
static patch_t *greenpercentage;   // green  percentage symbol
static patch_t *yellowinvnums[10]; // yellow inventory numbers
static patch_t *yellowpercentage;  // yellow percentage symbol

static patch_t *PatchINVCURS;
static patch_t *PatchINVBACK;
static patch_t *PatchINVTOP;

static patch_t *PatchINVPOP;
static patch_t *PatchINVPOP2;
static patch_t *PatchINVPBAK;
static patch_t *PatchINVPBAK2;

// SVE_TODO: INVB_WL, INVB_WR, INVPOPF1, INVPOPF2


//
// Initializes the Heretic status bar:
// * Caches most patch graphics used throughout
//
static void ST_StrfInit()
{

   // load green font
   for(int i = 0; i < 10; ++i)
   {
      char lumpnameg[9];
      char lumpnamey[9];

      memset(lumpnameg, 0, 9);
      sprintf(lumpnameg, "INVFONG%d", i);
      memset(lumpnamey, 0, 9);
      sprintf(lumpnamey, "INVFONY%d", i);

      efree(greeninvnums[i]);
      greeninvnums[i]  = PatchLoader::CacheName(wGlobalDir, lumpnameg, PU_STATIC);
      efree(yellowinvnums[i]);
      yellowinvnums[i] = PatchLoader::CacheName(wGlobalDir, lumpnamey, PU_STATIC);
   }
   efree(greenpercentage);
   greenpercentage  = PatchLoader::CacheName(wGlobalDir, "INVFONG%", PU_STATIC);
   efree(yellowpercentage);
   yellowpercentage = PatchLoader::CacheName(wGlobalDir, "INVFONY%", PU_STATIC);

   PatchINVCURS  = PatchLoader::CacheName(wGlobalDir, "INVCURS",  PU_CACHE);
   PatchINVBACK  = PatchLoader::CacheName(wGlobalDir, "INVBACK",  PU_CACHE);
   PatchINVTOP   = PatchLoader::CacheName(wGlobalDir, "INVTOP",   PU_CACHE);
   PatchINVPOP   = PatchLoader::CacheName(wGlobalDir, "INVPOP",   PU_CACHE);
   PatchINVPOP2  = PatchLoader::CacheName(wGlobalDir, "INVPOP2",  PU_CACHE);
   PatchINVPBAK  = PatchLoader::CacheName(wGlobalDir, "INVPBAK",  PU_CACHE);
   PatchINVPBAK2 = PatchLoader::CacheName(wGlobalDir, "INVPBAK2", PU_CACHE);
}

static void ST_StrfStart()
{
}

//
// Processing code for Strife status bar
//
static void ST_StrfTicker()
{

}

//
// Draws the Strife status bar
//
static void ST_StrfDrawer()
{
   V_DrawPatch(0, SCREENHEIGHT - ST_SBARHEIGHT, &subscreen43, PatchINVBACK);
   V_DrawPatch(0, SCREENHEIGHT - ST_SBARHEIGHT - PatchINVTOP->height, &subscreen43, PatchINVTOP);
}

//
// Draws the Strife fullscreen hud/status information.
//
static void ST_StrfFSDrawer()
{

}


//
// Status Bar Object for GameModeInfo
//
stbarfns_t StrfStatusBar =
{
   ST_SBARHEIGHT,

   ST_StrfTicker,
   ST_StrfDrawer,
   ST_StrfFSDrawer,
   ST_StrfStart,
   ST_StrfInit,
};

// EOF

