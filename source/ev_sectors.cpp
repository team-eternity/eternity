// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2014 James Haley et al.
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Generalized sector special system
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "d_mod.h"
#include "doomstat.h"
#include "ev_sectors.h"
#include "p_spec.h"
#include "r_defs.h"
#include "r_state.h"

//=============================================================================
//
// Static Sector Effects
//
// Effects that apply to sectors at level start.
//

//
// EV_SectorLightRandomOff
//
// Spawns a LightFlashThinker
// * DOOM: 1
// * Generalized: (sector->special & 31) == 1
//
static void EV_SectorLightRandomOff(sector_t *sector)
{
   // random off
   P_SpawnLightFlash(sector);
}

//
// EV_SectorLightStrobeFast
//
// Spawns a StrobeThinker set to FASTDARK, non-synchronized
// * DOOM: 2
// * Generalized: (sector->special & 31) == 2 
//
static void EV_SectorLightStrobeFast(sector_t *sector)
{
   // strobe fast
   P_SpawnStrobeFlash(sector, FASTDARK, 0);
}

//
// EV_SectorLightStrobeSlow
//
// Spawns a StrobeThinker set to SLOWDARK, non-synchronized
// * DOOM: 3
// * Generalized: (sector->special & 31) == 3
//
static void EV_SectorLightStrobeSlow(sector_t *sector)
{
   // strobe slow
   P_SpawnStrobeFlash(sector, SLOWDARK, 0);
}

//
// EV_SectorLightStrobeHurt
//
// Spawns a StrobeThinker set to FASTDARK, non-synchronized, and 
// sets up 20-per-32 damage with leaky suit. Overrides generalized 
// sector damage properties.
// * DOOM: 4
// * Generalized: (sector->special & 31) == 4
//
static void EV_SectorLightStrobeHurt(sector_t *sector)
{
   // strobe fast/death slime
   P_SpawnStrobeFlash(sector, FASTDARK, 0);
   
   // haleyjd 12/31/08: sector damage conversion
   sector->damage       = 20;
   sector->damagemask   = 32;
   sector->damagemod    = MOD_SLIME;
   sector->damageflags |= SDMG_LEAKYSUIT;
}

//
// EV_SectorDamageHellSlime
//
// Sets up 10-per-32 damage.
// * DOOM: 5
//
static void EV_SectorDamageHellSlime(sector_t *sector)
{
   sector->damage     = 10;
   sector->damagemask = 32;
   sector->damagemod  = MOD_SLIME;
}

//
// EV_SectorDamageNukage
//
// Sets up 5-per-32 damage.
// * DOOM: 7
//
static void EV_SectorDamageNukage(sector_t *sector)
{
   sector->damage     = 5;
   sector->damagemask = 32;
   sector->damagemod  = MOD_SLIME;
}

//
// EV_SectorLightGlow
//
// Spawns a GlowThinker.
// * DOOM: 8
// * Generalized: (sector->special & 31) == 8
//
static void EV_SectorLightGlow(sector_t *sector)
{
   // glowing light
   P_SpawnGlowingLight(sector);
}

//
// EV_SectorSecret
//
// Marks the sector as a secret.
// * DOOM: 9
//
static void EV_SectorSecret(sector_t *sector)
{
   if(!(sector->flags & SECF_SECRET))
   {
      ++totalsecret;
      sector->flags |= SECF_SECRET;
   }
}

//
// EV_SectorDoorCloseIn30
//
// Spawns a VerticalDoorThinker set to close in 30 seconds.
// * DOOM: 10
// * Generalized: (sector->special & 31) == 10
//   NB: obliterates any generalized sector properties.
//
static void EV_SectorDoorCloseIn30(sector_t *sector)
{
   P_SpawnDoorCloseIn30(sector);
}

//
// EV_SectorExitSuperDamage
//
// Creates the "end-of-game hell hack" effect used in E1M8.
// * DOOM: 11
//
static void EV_SectorExitSuperDamage(sector_t *sector)
{
   sector->damage       = 20;
   sector->damagemask   = 32;
   sector->damagemod    = MOD_SLIME;
   sector->damageflags |= SDMG_IGNORESUIT|SDMG_ENDGODMODE|SDMG_EXITLEVEL;
}

//
// EV_SectorLightStrobeSlow
//
// Spawns a StrobeThinker set to SLOWDARK, synchronized
// * DOOM: 12
// * Generalized: (sector->special & 31) == 12
//
static void EV_SectorLightStrobeSlowSync(sector_t *sector)
{
   // sync strobe slow
   P_SpawnStrobeFlash(sector, SLOWDARK, 1);
}

//
// EV_SectorLightStrobeFast
//
// Spawns a StrobeThinker set to FASTDARK, synchronized
// * DOOM: 13
// * Generalized: (sector->special & 31) == 13 
//
static void EV_SectorLightStrobeFastSync(sector_t *sector)
{
   // sync strobe fast
   P_SpawnStrobeFlash(sector, FASTDARK, 1);
}

//
// EV_SectorDoorCloseIn30
//
// Spawns a VerticalDoorThinker set to open-wait-close after 5 minutes.
// * DOOM: 14
// * Generalized: (sector->special & 31) == 14
//   NB: obliterates any generalized sector properties.
//
static void EV_SectorDoorRaiseIn5Mins(sector_t *sector)
{
   P_SpawnDoorRaiseIn5Mins(sector);
}

//
// EV_SectorDamageSuperHellSlime
//
// Sets up 20-per-32 damage with LEAKYSUIT property.
// * DOOM: 16
//
static void EV_SectorDamageSuperHellSlime(sector_t *sector)
{
   sector->damage       = 20;
   sector->damagemask   = 32;
   sector->damagemod    = MOD_SLIME;
   sector->damageflags |= SDMG_LEAKYSUIT;
}

//
// EV_SectorLightFireFlicker
//
// Spawns a FireFlickerThinker.
// * DOOM: 17
// * Generalized: (sector->special & 31) == 17
//
static void EV_SectorLightFireFlicker(sector_t *sector)
{
   P_SpawnFireFlicker(sector);
}

// EOF

