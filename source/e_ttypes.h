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
//------------------------------------------------------------------------------
//
// Purpose: New dynamic TerrainTypes system. Inspired heavily by ZDoom, but
// all-original code.
//
// Authors: James Haley
//

#ifndef E_TTYPES_H__
#define E_TTYPES_H__

// Required for: byte, fixed_t
#include "doomtype.h"
#include "m_fixed.h"

struct particle_t;
struct sector_t;
class Mobj;

#ifdef NEED_EDF_DEFINITIONS

constexpr const char EDF_SEC_SPLASH[]      = "splash";
constexpr const char EDF_SEC_TERRAIN[]     = "terrain";
constexpr const char EDF_SEC_FLOOR[]       = "floor";
constexpr const char EDF_SEC_SPLASHDELTA[] = "splashdelta";
constexpr const char EDF_SEC_TERDELTA[]    = "terraindelta";
extern cfg_opt_t     edf_splash_opts[];
extern cfg_opt_t     edf_spldelta_opts[];
extern cfg_opt_t     edf_terrn_opts[];
extern cfg_opt_t     edf_terdelta_opts[];
extern cfg_opt_t     edf_floor_opts[];

void E_ProcessTerrainTypes(cfg_t *cfg);

#endif

struct ETerrainSplash
{
    int  smallclass;      // mobjtype used for small splash
    int  smallclip;       // amount of floorclip to apply to small splash
    char smallsound[129]; // sound to play for small splash

    int     baseclass;      // mobjtype used for normal splash
    int     chunkclass;     // mobjtype used for normal splash chunk
    int     chunkxvelshift; // chunk's x velocity factor
    int     chunkyvelshift; // chunk's y velocity factor
    int     chunkzvelshift; // chunk's z velocity factor
    fixed_t chunkbasezvel;  // base amount of z velocity for chunk
    char    sound[129];     // sound to play for normal splash

    struct ETerrainSplash *next;      // hash link
    char                   name[129]; // hash name
};

struct ETerrain
{
    ETerrainSplash *splash;         // pointer to splash object
    int             damageamount;   // damage amount at each chance to hurt
    int             damagetype;     // MOD to use for damage
    int             damagetimemask; // time mask for damage chances
    fixed_t         footclip;       // footclip amount
    bool            liquid;         // is liquid?
    bool            splashalert;    // normal splash causes P_NoiseAlert?
    bool            usepcolors;     // use particle colors?
    byte            pcolor_1;       // particle color 1
    byte            pcolor_2;       // particle color 2

    int minversion; // minimum demo version for this terrain

    struct ETerrain *next;      // hash link
    char             name[129]; // hash name
};

struct EFloor
{
    char           name[9]; // flat name
    ETerrain      *terrain; // terrain definition
    struct EFloor *next;    // hash link
};

void      E_InitTerrainTypes(void);
ETerrain *E_TerrainForName(const char *name);
ETerrain *E_GetThingFloorType(const Mobj *thing);
ETerrain *E_GetTerrainTypeForPt(fixed_t x, fixed_t y, int pos);
fixed_t   E_SectorFloorClip(sector_t *sector);
bool      E_HitWater(Mobj *thing, const sector_t *sector);
void      E_ExplosionHitWater(Mobj *thing, int damage);
bool      E_StandingOnExactly(const sector_t &sector, const Mobj &thing);
bool      E_HitFloor(Mobj *thing);
bool      E_WouldHitFloorWater(const Mobj &thing);
bool      E_UnderBoomLiquidFakeFloor(const Mobj &thing);
void      E_PtclTerrainHit(particle_t *);

#endif

// EOF

