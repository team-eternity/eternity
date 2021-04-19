// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//----------------------------------------------------------------------------
//
// ExtraData
//
// The be-all, end-all extension to the DOOM map format. Uses the
// libConfuse library like EDF.
// 
// ExtraData can extend mapthings, lines, and sectors with an
// arbitrary number of fields, with data provided in more or less
// any format. The use of a textual input language will forever
// remove any future problems caused by binary format limitations.
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef E_EXDATA_H__
#define E_EXDATA_H__

// Required for:
// * maplinedef_t
// * NUMLINEARGS, etc.
#include "doomdata.h"
#include "r_defs.h"

// defines

// the ExtraData control object has doomednum 5004
constexpr int ED_CTRL_DOOMEDNUM = 5004;

// ExtraData mapthing structure

// haleyjd 03/03/07: usurped by new unified mapthing_t in doomdata.h

// Extended line special flags
// Because these go into a new field used only by parameterized
// line specials, I don't face the issue of running out of line
// flags anytime soon. This provides another full word for
// future expansion.
enum extmlflags_e : unsigned int
{
   EX_ML_CROSS        = 0x00000001, // crossable
   EX_ML_USE          = 0x00000002, // usable
   EX_ML_IMPACT       = 0x00000004, // shootable
   EX_ML_PUSH         = 0x00000008, // reserved for future use
   EX_ML_PLAYER       = 0x00000010, // enabled for players
   EX_ML_MONSTER      = 0x00000020, // enabled for monsters
   EX_ML_MISSILE      = 0x00000040, // enabled for missiles
   EX_ML_REPEAT       = 0x00000080, // can be used multiple times
   EX_ML_1SONLY       = 0x00000100, // only activated from first side
   EX_ML_ADDITIVE     = 0x00000200, // uses additive blending
   EX_ML_BLOCKALL     = 0x00000400, // line blocks everything
   EX_ML_ZONEBOUNDARY = 0x00000800, // line marks a sound zone boundary
   EX_ML_CLIPMIDTEX   = 0x00001000, // line clips midtextures to floor and ceiling height
   EX_ML_3DMTPASSPROJ = 0x00002000, // with ML_3DMIDTEX, makes it pass projectiles
   EX_ML_LOWERPORTAL  = 0x00004000, // extends the floor portal of the back sector
   EX_ML_UPPERPORTAL  = 0x00008000, // extends the ceiling portal of the back sector
   EX_ML_POLYOBJECT   = 0x00010000, // enabled for polyobjects
};

static const unsigned e_edgePortalFlags[surf_NUM] = { EX_ML_LOWERPORTAL, EX_ML_UPPERPORTAL };

// ExtraData line structure

struct maplinedefext_t
{
   // standard fields
   maplinedef_t stdfields;

   // extended fields
   unsigned int extflags;
   int   args[NUMLINEARGS];
   int   id;
   float alpha;

   int portalid;

   // internal fields (used by ExtraData only)
   int recordnum;
   int next;

};

// ExtraData sector structure

struct ETerrain;
class  UDMFSetupSettings;

//
// Sector floor/ceiling component
//
struct mapsurfaceext_t
{
   v2double_t offset;
   v2double_t scale;
   double angle;
   unsigned pflags;
   unsigned alpha;
   int portalid;
   ETerrain *terrain;
};

struct mapsectorext_t
{
   // extended fields
   Surfaces<mapsurfaceext_t> surface;

   unsigned int flags;
   unsigned int flagsadd;
   unsigned int flagsrem;
   int    topmap;
   int    midmap;
   int    bottommap;
   int    damage;
   int    damagemask;
   int    damagemod;
   unsigned int damageflags;
   unsigned int damageflagsadd;
   unsigned int damageflagsrem;

   // internal fields (used by ExtraData only)
   bool hasflags;
   bool hasdamageflags;
   int  recordnum;
   int  next;

};

// Globals

void    E_LoadExtraData(void);
Mobj   *E_SpawnMapThingExt(mapthing_t *mt);
void    E_LoadLineDefExt(line_t *line, bool applySpecial, UDMFSetupSettings &setupSettings);
void    E_GetEDMapThings(mapthing_t **things, int *numthings);
void    E_GetEDLines(maplinedefext_t **lines, int *numlines);
int     E_LineSpecForName(const char *name);
void    E_LoadSectorExt(line_t *line, UDMFSetupSettings &setupSettings);
double  E_NormalizeFlatAngle(double input); // MaxW: 2016/07/11: Needed for UDMF & Sector_SetRotation

#endif

// EOF

