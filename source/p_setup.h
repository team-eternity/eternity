// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//   Setup a game, startup stuff.
//
//-----------------------------------------------------------------------------

#ifndef P_SETUP_H__
#define P_SETUP_H__

#include "doomdef.h"  // for skill_t
#include "doomtype.h" // for byte
#include "m_fixed.h"  // for fixed_t

class  Mobj;
struct seg_t;

// haleyjd 10/03/05: let P_CheckLevel determine the map format
enum
{
   LEVEL_FORMAT_INVALID,
   LEVEL_FORMAT_DOOM,
   LEVEL_FORMAT_HEXEN,
   LEVEL_FORMAT_PSX,
   LEVEL_FORMAT_DOOM64,
   // LEVEL_FORMAT_UDMF_[NAMESPACE] is the format for UDMF namespaces that need their own definition
   LEVEL_FORMAT_UDMF_ETERNITY,
};

class WadDirectory;
// IOANCH 20151213: modify P_CheckLevel to support one extra parameter
struct maplumpindex_t;
 // haleyjd: now used in d_main.c
int P_CheckLevel(const WadDirectory *dir, int lumpnum, 
                 maplumpindex_t *mgla = nullptr, bool *udmf = nullptr);

void P_SetupLevel(WadDirectory *dir, const char *mapname, int playermask, skill_t skill);
void P_Init();                   // Called by startup code.
void P_InitThingLists();

// IOANCH 20151210: made these global so they can be accessed from e_udmf
struct line_t;
struct linkdata_t;
struct mapthing_t;
struct sector_t;
struct side_t;
void P_SetupLevelError(const char *msg, const char *levelname);
void P_InitSector(sector_t *ss);
void P_InitLineDef(line_t *ld);
void P_PostProcessLineFlags(line_t *ld);
void P_SetupSidedefTextures(side_t &sd, const char *bottomTexture, 
                            const char *midTexture, const char *topTexture);
bool P_CheckThingDoomBan(int16_t type);
void P_ConvertHereticThing(mapthing_t *mthing);
void P_ConvertDoomExtendedSpawnNum(mapthing_t *mthing);

extern byte     *rejectmatrix;   // for fast sight rejection

// killough 3/1/98: change blockmap from "short" to "long" offsets:
extern int     *blockmaplump;    // offsets in blockmap are from here
extern int     *blockmap;
extern int      bmapwidth;
extern int      bmapheight;      // in mapblocks
extern fixed_t  bmaporgx;
extern fixed_t  bmaporgy;        // origin of block map
extern Mobj   **blocklinks;      // for thing chains
extern byte    *portalmap;       // haleyjd: for fast linked portal checks
extern bool     skipblstart;     // MaxW: Skip initial blocklist short

// haleyjd 05/17/13: portalmap flags
enum
{
   PMF_LINE    = 0x01, // block contains one or more line portals
   PMF_FLOOR   = 0x02, // block contains one or more floor portals
   PMF_CEILING = 0x04  // block contains one or more ceiling portals
};

extern bool     newlevel;
extern char     levelmapname[10];

#if 0
typedef struct                          // Standard OLO stuff, put in WADs
{       
  unsigned char header[3];                 // Header
  unsigned char space1;
  unsigned char extend;
  unsigned char space2;
  
  // Standard
  unsigned char levelwarp;
  unsigned char lastlevel;
  unsigned char deathmatch;
  unsigned char skill_level;
  unsigned char nomonsters;
  unsigned char respawn;
  unsigned char fast;

  unsigned char levelname[32][32];
} olo_t;

extern olo_t olo;
extern int olo_loaded;
#endif

struct seg_t;

#endif

//----------------------------------------------------------------------------
//
// $Log: p_setup.h,v $
// Revision 1.3  1998/05/03  23:03:31  killough
// beautification, add external declarations for blockmap
//
// Revision 1.2  1998/01/26  19:27:28  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:08  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
