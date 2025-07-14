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
// Purpose: Generalized sector special system.
// Authors: James Haley, Ioan Chera, Max Waine
//

#include "z_zone.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "d_gi.h"
#include "d_mod.h"
#include "doomstat.h"
#include "ev_sectors.h"
#include "p_info.h"
#include "p_scroll.h"
#include "p_setup.h"
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
// DOOM and Shared Types
//

//
// EV_SectorLightRandomOff
//
// Spawns a LightFlashThinker
// * DOOM: 1
// * Heretic: 1
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
// * Heretic: 2
// * Generalized: (sector->special & 31) == 2
//
static void EV_SectorLightStrobeFast(sector_t *sector)
{
    // strobe fast
    P_SpawnStrobeFlash(sector, FASTDARK, STROBEBRIGHT, 0);
}

//
// EV_SectorLightStrobeSlow
//
// Spawns a StrobeThinker set to SLOWDARK, non-synchronized
// * DOOM: 3
// * Heretic: 3
// * Generalized: (sector->special & 31) == 3
//
static void EV_SectorLightStrobeSlow(sector_t *sector)
{
    // strobe slow
    P_SpawnStrobeFlash(sector, SLOWDARK, STROBEBRIGHT, 0);
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
    P_SpawnStrobeFlash(sector, FASTDARK, STROBEBRIGHT, 0);

    // haleyjd 12/31/08: sector damage conversion
    sector->damage     = 20;
    sector->damagemask = 32;
    sector->damagemod  = MOD_SLIME;
    sector->leakiness  = 5;
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
// * Heretic: 8
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
// * Heretic: 9
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
// * Heretic: 10
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
// * Heretic (EE Extension): 11
//
static void EV_SectorExitSuperDamage(sector_t *sector)
{
    sector->damage       = 20;
    sector->damagemask   = 32;
    sector->damagemod    = MOD_SLIME;
    sector->damageflags |= SDMG_ENDGODMODE | SDMG_EXITLEVEL;
    sector->leakiness    = 256;
}

//
// EV_SectorLightStrobeSlowSync
//
// Spawns a StrobeThinker set to SLOWDARK, synchronized
// * DOOM: 12
// * Heretic: 12
// * Generalized: (sector->special & 31) == 12
//
static void EV_SectorLightStrobeSlowSync(sector_t *sector)
{
    // sync strobe slow
    P_SpawnStrobeFlash(sector, SLOWDARK, STROBEBRIGHT, 1);
}

//
// EV_SectorLightStrobeFastSync
//
// Spawns a StrobeThinker set to FASTDARK, synchronized
// * DOOM: 13
// * Heretic: 13
// * Generalized: (sector->special & 31) == 13
//
static void EV_SectorLightStrobeFastSync(sector_t *sector)
{
    // sync strobe fast
    P_SpawnStrobeFlash(sector, FASTDARK, STROBEBRIGHT, 1);
}

//
// EV_SectorDoorRaiseIn5Mins
//
// Spawns a VerticalDoorThinker set to open-wait-close after 5 minutes.
// * DOOM: 14
// * Heretic: 14
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
    sector->damage     = 20;
    sector->damagemask = 32;
    sector->damagemod  = MOD_SLIME;
    sector->leakiness  = 5;
}

//
// EV_SectorLightFireFlicker
//
// Spawns a FireFlickerThinker.
// * DOOM: 17
// * Heretic (EE Extension): 17
// * Generalized: (sector->special & 31) == 17
//
static void EV_SectorLightFireFlicker(sector_t *sector)
{
    P_SpawnFireFlicker(sector);
}

//
// Heretic Types
//

//
// EV_SectorHticScrollEastLavaDamage
//
// Augments DOOM's "strobe hurt/death slime"
// * Heretic: 4
//
static void EV_SectorHticScrollEastLavaDamage(sector_t *sector)
{
    P_SpawnStrobeFlash(sector, FASTDARK, STROBEBRIGHT, 0);

    // custom damage parameters:
    sector->damage       = 5;
    sector->damagemask   = 16;
    sector->damagemod    = MOD_LAVA;
    sector->damageflags |= SDMG_TERRAINHIT;

    // heretic current pusher type:
    sector->hticPushType  = SECTOR_HTIC_CURRENT;
    sector->hticPushAngle = 0;
    sector->hticPushForce = 2048 * 28;

    // scrolls to the east:
    Add_Scroller(ScrollThinker::sc_floor, (-FRACUNIT / 2) * 8, 0, -1, eindex(sector - sectors), 0);
}

//
// EV_SectorHticDamageLavaWimpy
//
// Sets up Heretic wimpy lava damage.
// * Heretic: 5
//
static void EV_SectorHticDamageLavaWimpy(sector_t *sector)
{
    sector->damage       = 5;
    sector->damagemask   = 16;
    sector->damagemod    = MOD_LAVA;
    sector->damageflags |= SDMG_TERRAINHIT;
}

//
// EV_SectorHticDamageSludge
//
// Sets up Heretic sludge damage.
// * Heretic: 7
//
static void EV_SectorHticDamageSludge(sector_t *sector)
{
    sector->damage     = 4;
    sector->damagemask = 32;
    sector->damagemod  = MOD_SLIME;
}

//
// EV_SectorHticFrictionLow
//
// Sets up Heretic ice sectors.
// * Heretic: 15
//
static void EV_SectorHticFrictionLow(sector_t *sector)
{
    sector->friction    = 0xf900;
    sector->movefactor  = ORIG_FRICTION_FACTOR >> 2;
    sector->flags      |= SECF_FRICTION; // set friction bit
}

//
// EV_SectorHticDamageLavaHefty
//
// Sets up Heretic hefty lava damage.
// * Heretic: 16
//
static void EV_SectorHticDamageLavaHefty(sector_t *sector)
{
    sector->damage       = 8;
    sector->damagemask   = 16;
    sector->damagemod    = MOD_LAVA;
    sector->damageflags |= SDMG_TERRAINHIT;
}

//
// EV_setupHereticPusher
//
// Setup a Heretic wind or current sector effect.
//
static void EV_setupHereticPusher(sector_t *sector, angle_t angle, int type, int force)
{
    // Heretic push forces table
    static fixed_t pushForces[5] = { 2048 * 5, 2048 * 10, 2048 * 25, 2048 * 30, 2048 * 35 };

    sector->hticPushType  = type;
    sector->hticPushAngle = angle;
    sector->hticPushForce = pushForces[force];
}

//
// EV_setupHereticScroller
//
// Setup a Heretic scrolling effect.
//
static void EV_setupHereticScroller(sector_t *sector, int force)
{
    Add_Scroller(ScrollThinker::sc_floor, (-FRACUNIT / 2) << force, 0, -1, eindex(sector - sectors), 0);
}

//
// EV_SectorHticScrollEast
//
// Sets up scroll east with current.
// * Heretic: 20, 21, 22, 23, 24
//
template<int force>
static void EV_SectorHticScrollEast(sector_t *sector)
{
    EV_setupHereticPusher(sector, 0, SECTOR_HTIC_CURRENT, force);
    EV_setupHereticScroller(sector, force);
}

//
// EV_SectorHticScroll
//
// Sets up Heretic current in directions other than east.
// * Heretic: <ANG90,  0-4> == 25, 26, 27, 28, 29 (North)
//            <ANG270, 0-4> == 30, 31, 32, 33, 34 (South)
//            <ANG180, 0-4> == 35, 36, 37, 38, 39 (West)
//
template<angle_t angle, int force>
static void EV_SectorHticScroll(sector_t *sector)
{
    EV_setupHereticPusher(sector, angle, SECTOR_HTIC_CURRENT, force);
}

//
// EV_SectorHticWind
//
// Sets up Heretic wind effects.
// * Heretic: <0,      0-2> == 40, 41, 42 (East)
//            <ANG90,  0-2> == 43, 44, 45 (North)
//            <ANG270, 0-2> == 46, 47, 48 (South)
//            <ANG180, 0-2> == 49, 50, 51 (West)
//
template<angle_t angle, int force>
static void EV_SectorHticWind(sector_t *sector)
{
    EV_setupHereticPusher(sector, angle, SECTOR_HTIC_WIND, force);
}

//
// Hexen Types
//

//
// EV_SectorHexenLightPhased
//
// Spawn a PhasedLightThinker base 80 and index determined by the sector's
// initial light level taken modulus 64.
// * Hexen: 1
//
static void EV_SectorHexenLightPhased(sector_t *sector)
{
    PhasedLightThinker::Spawn(sector, 80, -1);
}

//
// EV_SectorHexenLightSeqStart
//
// Marks the beginning of a phased light sequence.
//
static void EV_SectorHexenLightSeqStart(sector_t *sector)
{
    sector->flags |= SECF_PHASEDLIGHT;
}

//
// EV_SectorHexenLightSequence
//
// Marks a sector to step to in a phased light sequence. Must alternate with
// the below special.
//
static void EV_SectorHexenLightSequence(sector_t *sector)
{
    sector->flags |= SECF_LIGHTSEQUENCE;
}

//
// EV_SectorHexenLightSeqAlt
//
// Alternates with the above special to create phased light sequences.
//
static void EV_SectorHexenLightSeqAlt(sector_t *sector)
{
    sector->flags |= SECF_LIGHTSEQALT;
}

//
// Strife Sector Types
//

//
// Sets up Strife instant kill flag.
// * Strife: 15
// * EE UDMF: 115
//
static void EV_SectorStrifeInstantKill(sector_t *sector)
{
    sector->flags |= SECF_INSTANTDEATH;
}

//
// PSX Sector Types
//

//
// EV_SectorLightPSXGlowLow
//
// Glows from sector->lightlevel to lowest neighboring light level in
// a cycle (same behavior as DOOM type 8), but at GLOWSPEEDSLOW.
// * PSX: 8
//
static void EV_SectorLightPSXGlowLow(sector_t *sector)
{
    P_SpawnPSXGlowingLight(sector, psxglow_low);
}

//
// EV_SectorLightPSXGlow10
//
// Glows from sector->lightlevel down to 10, and then back up in a cycle.
// * PSX: 200
//
static void EV_SectorLightPSXGlow10(sector_t *sector)
{
    P_SpawnPSXGlowingLight(sector, psxglow_10);
}

//
// EV_SectorLightPSXGlow255
//
// Glows from sector->lightlevel up to 255, and then back down in a cycle.
// * PSX: 201
//
static void EV_SectorLightPSXGlow255(sector_t *sector)
{
    P_SpawnPSXGlowingLight(sector, psxglow_255);
}

//
// EV_SectorLightPSXStrobeHyper
//
// Super-fast strobing light.
// * PSX: 202 (NB: similar to DOOM 64 202)
//
static void EV_SectorLightPSXStrobeHyper(sector_t *sector)
{
    P_SpawnPSXStrobeFlash(sector, 3, true);
}

//
// EV_SectorLightPSXStrobeFast
//
// Fast strobing light.
// * PSX: 204 (NB: similar to DOOM 64 204)
//
static void EV_SectorLightPSXStrobeFast(sector_t *sector)
{
    P_SpawnPSXStrobeFlash(sector, 8, false);
}

//=============================================================================
//
// Sector Special Bindings
//

// Doom sector specials
static ev_sectorbinding_t DoomSectorBindings[] = {
    { 1,  EV_SectorLightRandomOff       },
    { 2,  EV_SectorLightStrobeFast      },
    { 3,  EV_SectorLightStrobeSlow      },
    { 4,  EV_SectorLightStrobeHurt      },
    { 5,  EV_SectorDamageHellSlime      },
    { 7,  EV_SectorDamageNukage         },
    { 8,  EV_SectorLightGlow            },
    { 9,  EV_SectorSecret               },
    { 10, EV_SectorDoorCloseIn30        },
    { 11, EV_SectorExitSuperDamage      },
    { 12, EV_SectorLightStrobeSlowSync  },
    { 13, EV_SectorLightStrobeFastSync  },
    { 14, EV_SectorDoorRaiseIn5Mins     },
    { 16, EV_SectorDamageSuperHellSlime },
    { 17, EV_SectorLightFireFlicker     }
};

// clang-format off

// Heretic sector specials
static ev_sectorbinding_t HticSectorBindings[] = {
    {  1, EV_SectorLightRandomOff           },
    {  2, EV_SectorLightStrobeFast          },
    {  3, EV_SectorLightStrobeSlow          },
    {  4, EV_SectorHticScrollEastLavaDamage },
    {  5, EV_SectorHticDamageLavaWimpy      },
    {  7, EV_SectorHticDamageSludge         },
    {  8, EV_SectorLightGlow                },
    {  9, EV_SectorSecret                   },
    { 10, EV_SectorDoorCloseIn30            },
    { 11, EV_SectorExitSuperDamage          },
    { 12, EV_SectorLightStrobeSlowSync      },
    { 13, EV_SectorLightStrobeFastSync      },
    { 14, EV_SectorDoorRaiseIn5Mins         },
    { 15, EV_SectorHticFrictionLow          },
    { 16, EV_SectorHticDamageLavaHefty      },
    { 17, EV_SectorLightFireFlicker         },
    { 20, EV_SectorHticScrollEast<0>        },
    { 21, EV_SectorHticScrollEast<1>        },
    { 22, EV_SectorHticScrollEast<2>        },
    { 23, EV_SectorHticScrollEast<3>        },
    { 24, EV_SectorHticScrollEast<4>        },
    { 25, EV_SectorHticScroll<ANG90,  0>    },
    { 26, EV_SectorHticScroll<ANG90,  1>    },
    { 27, EV_SectorHticScroll<ANG90,  2>    },
    { 28, EV_SectorHticScroll<ANG90,  3>    },
    { 29, EV_SectorHticScroll<ANG90,  4>    },
    { 30, EV_SectorHticScroll<ANG270, 0>    },
    { 31, EV_SectorHticScroll<ANG270, 1>    },
    { 32, EV_SectorHticScroll<ANG270, 2>    },
    { 33, EV_SectorHticScroll<ANG270, 3>    },
    { 34, EV_SectorHticScroll<ANG270, 4>    },
    { 35, EV_SectorHticScroll<ANG180, 0>    },
    { 36, EV_SectorHticScroll<ANG180, 1>    },
    { 37, EV_SectorHticScroll<ANG180, 2>    },
    { 38, EV_SectorHticScroll<ANG180, 3>    },
    { 39, EV_SectorHticScroll<ANG180, 4>    },
    { 40, EV_SectorHticWind<0,      0>      },
    { 41, EV_SectorHticWind<0,      1>      },
    { 42, EV_SectorHticWind<0,      2>      },
    { 43, EV_SectorHticWind<ANG90,  0>      },
    { 44, EV_SectorHticWind<ANG90,  1>      },
    { 45, EV_SectorHticWind<ANG90,  2>      },
    { 46, EV_SectorHticWind<ANG270, 0>      },
    { 47, EV_SectorHticWind<ANG270, 1>      },
    { 48, EV_SectorHticWind<ANG270, 2>      },
    { 49, EV_SectorHticWind<ANG180, 0>      },
    { 50, EV_SectorHticWind<ANG180, 1>      },
    { 51, EV_SectorHticWind<ANG180, 2>      }
};

// Hexen sector specials
static ev_sectorbinding_t HexenSectorBindings[] = {
    {  1, EV_SectorHexenLightPhased         },
    {  2, EV_SectorHexenLightSeqStart       },
    {  3, EV_SectorHexenLightSequence       },
    {  4, EV_SectorHexenLightSeqAlt         },
    {  9, EV_SectorSecret                   },
   // TODO: 26, 27 for stairs
    { 40, EV_SectorHticWind<0,      0>      },
    { 41, EV_SectorHticWind<0,      1>      },
    { 42, EV_SectorHticWind<0,      2>      },
    { 43, EV_SectorHticWind<ANG90,  0>      },
    { 44, EV_SectorHticWind<ANG90,  1>      },
    { 45, EV_SectorHticWind<ANG90,  2>      },
    { 46, EV_SectorHticWind<ANG270, 0>      },
    { 47, EV_SectorHticWind<ANG270, 1>      },
    { 48, EV_SectorHticWind<ANG270, 2>      },
    { 49, EV_SectorHticWind<ANG180, 0>      },
    { 50, EV_SectorHticWind<ANG180, 1>      },
    { 51, EV_SectorHticWind<ANG180, 2>      }
   // TODO: 198 Lightning
   // TODO: 199 Lightning Flash
   // TODO: 200 Sky2
   // TODO: 201-224 current scrollers
   // TODO: ZDoom extensions
};

// clang-format on

// PSX sector types
static ev_sectorbinding_t PSXSectorBindings[] = {
    { 8,   EV_SectorLightPSXGlowLow     },
    { 200, EV_SectorLightPSXGlow10      },
    { 201, EV_SectorLightPSXGlow255     },
    { 202, EV_SectorLightPSXStrobeHyper },
    { 204, EV_SectorLightPSXStrobeFast  }
};

// Sector specials allowed as the low 5 bits of generalized specials
static ev_sectorbinding_t GenBindings[] = {
    { 1,  EV_SectorLightRandomOff      },
    { 2,  EV_SectorLightStrobeFast     },
    { 3,  EV_SectorLightStrobeSlow     },
    { 4,  EV_SectorLightStrobeHurt     },
    { 8,  EV_SectorLightGlow           },
    { 10, EV_SectorDoorCloseIn30       },
    { 12, EV_SectorLightStrobeSlowSync },
    { 13, EV_SectorLightStrobeFastSync },
    { 14, EV_SectorDoorRaiseIn5Mins    },
    { 17, EV_SectorLightFireFlicker    }
};

// clang-format off

static ev_sectorbinding_t UDMFEternitySectorBindings[] = {
    {   1, EV_SectorHexenLightPhased },
    {   2, EV_SectorHexenLightSeqStart },
    {   3, EV_SectorHexenLightSequence },
    {   4, EV_SectorHexenLightSeqAlt },
    // TODO: 26, 27 for stairs
    {  40, EV_SectorHticWind<0,      0>      },
    {  41, EV_SectorHticWind<0,      1>      },
    {  42, EV_SectorHticWind<0,      2>      },
    {  43, EV_SectorHticWind<ANG90,  0>      },
    {  44, EV_SectorHticWind<ANG90,  1>      },
    {  45, EV_SectorHticWind<ANG90,  2>      },
    {  46, EV_SectorHticWind<ANG270, 0>      },
    {  47, EV_SectorHticWind<ANG270, 1>      },
    {  48, EV_SectorHticWind<ANG270, 2>      },
    {  49, EV_SectorHticWind<ANG180, 0>      },
    {  50, EV_SectorHticWind<ANG180, 1>      },
    {  51, EV_SectorHticWind<ANG180, 2>      },
    {  65, EV_SectorLightRandomOff           },
    {  66, EV_SectorLightStrobeFast          },
    {  67, EV_SectorLightStrobeSlow          },
    {  68, EV_SectorLightStrobeHurt          },
    {  69, EV_SectorDamageHellSlime          },
    {  71, EV_SectorDamageNukage             },
    {  72, EV_SectorLightGlow                },
    {  74, EV_SectorDoorCloseIn30            },
    {  75, EV_SectorExitSuperDamage          },
    {  76, EV_SectorLightStrobeSlowSync      },
    {  77, EV_SectorLightStrobeFastSync      },
    {  78, EV_SectorDoorRaiseIn5Mins         },
    {  79, EV_SectorHticFrictionLow          },
    {  80, EV_SectorDamageSuperHellSlime     },
    {  81, EV_SectorLightFireFlicker         },
    {  82, EV_SectorHticDamageLavaWimpy      },
    {  83, EV_SectorHticDamageLavaHefty      },
    {  84, EV_SectorHticScrollEastLavaDamage },
    {  85, EV_SectorHticDamageSludge         },
    // Need to look for the appropriate specials for this initial block,
    // as some of these may have appropriate functions already there.
    // TODO: 87 Outside Fog
    // TODO: 104 5% Damage + Light On + Off Randomly
    // TODO: 105 Delayed damage weak
    { 115, EV_SectorStrifeInstantKill },
    // TODO: 116 Delayed damage strong
    // TODO: 118 Carry player by tag
    // TODO: 195 Hidden
    // TODO: 196 Healing Sector
    // TODO 197: Outdoor Lightning

    // MaxW: 2016/29/06: This block was not written by me, so this stuff does need
    // new functions.
    // TODO: 198 Lightning
    // TODO: 199 Lightning Flash
    // TODO: 200 Sky2
    // TODO: 201-224 current scrollers
    // TODO: ZDoom extensions

    { 225, EV_SectorHticScrollEast<0>     },
    { 226, EV_SectorHticScrollEast<1>     },
    { 227, EV_SectorHticScrollEast<2>     },
    { 228, EV_SectorHticScrollEast<3>     },
    { 229, EV_SectorHticScrollEast<4>     },
    { 230, EV_SectorHticScroll<ANG90,  0> },
    { 231, EV_SectorHticScroll<ANG90,  1> },
    { 232, EV_SectorHticScroll<ANG90,  2> },
    { 233, EV_SectorHticScroll<ANG90,  3> },
    { 234, EV_SectorHticScroll<ANG90,  4> },
    { 235, EV_SectorHticScroll<ANG270, 0> },
    { 236, EV_SectorHticScroll<ANG270, 1> },
    { 237, EV_SectorHticScroll<ANG270, 2> },
    { 238, EV_SectorHticScroll<ANG270, 3> },
    { 239, EV_SectorHticScroll<ANG270, 4> },
    { 240, EV_SectorHticScroll<ANG180, 0> },
    { 241, EV_SectorHticScroll<ANG180, 1> },
    { 242, EV_SectorHticScroll<ANG180, 2> },
    { 243, EV_SectorHticScroll<ANG180, 3> },
    { 244, EV_SectorHticScroll<ANG180, 4> }
};

// clang-format on

//
// EV_findBinding
//
// Look up a sector binding in a given set. The sets are small so this is just
// a linear search.
//
static ev_sectorbinding_t *EV_findBinding(ev_sectorbinding_t *bindings, size_t numBindings, int special)
{
    // early return for zero special
    if(!special)
        return nullptr;

    for(size_t i = 0; i < numBindings; i++)
    {
        if(bindings[i].special == special)
            return &bindings[i];
    }

    return nullptr;
}

//
// EV_DOOMBindingForSectorSpecial
//
// Look up a DOOM sector special binding.
//
static ev_sectorbinding_t *EV_DOOMBindingForSectorSpecial(int special)
{
    return EV_findBinding(DoomSectorBindings, earrlen(DoomSectorBindings), special);
}

//
// EV_PSXBindingForSectorSpecial
//
// Look up a PSX sector type. Defers to DOOM's lookup if a PSX type is not
// found first.
//
static ev_sectorbinding_t *EV_PSXBindingForSectorSpecial(int special)
{
    ev_sectorbinding_t *binding;

    if(!(binding = EV_findBinding(PSXSectorBindings, earrlen(PSXSectorBindings), special)))
        binding = EV_DOOMBindingForSectorSpecial(special);

    return binding;
}

//
// EV_HereticBindingForSectorSpecial
//
// Look up a Heretic sector special binding.
//
static ev_sectorbinding_t *EV_HereticBindingForSectorSpecial(int special)
{
    return EV_findBinding(HticSectorBindings, earrlen(HticSectorBindings), special);
}

//
// EV_HexenBindingForSectorSpecial
//
// Look up a Hexen sector special binding.
//
static ev_sectorbinding_t *EV_HexenBindingForSectorSpecial(int special)
{
    return EV_findBinding(HexenSectorBindings, earrlen(HexenSectorBindings), special);
}

//
// EV_GenBindingForSectorSpecial
//
// Find the "lighting" special for a generalized sector.
//
static ev_sectorbinding_t *EV_GenBindingForSectorSpecial(int special)
{
    return EV_findBinding(GenBindings, earrlen(GenBindings), special & LIGHT_MASK);
}

//
// EV_UDMFEternityBindingForSectorSpecial
//
// Look up a UDMF "Eternity" namespace sector special binding.
//
static ev_sectorbinding_t *EV_UDMFEternityBindingForSectorSpecial(int special)
{
    return EV_findBinding(UDMFEternitySectorBindings, earrlen(UDMFEternitySectorBindings), special);
}

//
// EV_BindingForSectorSpecial
//
// Gets the binding for a given special depending on the level format and
// gamemode.
//
static ev_sectorbinding_t *EV_BindingForSectorSpecial(int special)
{
    ev_sectorbinding_t *binding = nullptr;

    switch(LevelInfo.mapFormat)
    {
    case LEVEL_FORMAT_UDMF_ETERNITY: //
        binding = EV_UDMFEternityBindingForSectorSpecial(special);
        break;
    case LEVEL_FORMAT_HEXEN: //
        binding = EV_HexenBindingForSectorSpecial(special);
        break;
    case LEVEL_FORMAT_PSX: //
        binding = EV_PSXBindingForSectorSpecial(special);
        break;
    default:
        switch(LevelInfo.levelType)
        {
        case LI_TYPE_DOOM: //
            binding = EV_DOOMBindingForSectorSpecial(special);
            break;
        case LI_TYPE_HERETIC: //
            binding = EV_HereticBindingForSectorSpecial(special);
            break;
        default:   //
            break; // others TODO
        }
        break;
    }

    return binding;
}

//=============================================================================
//
// BOOM Generalized Sectors
//

//
// EV_IsGenSectorSpecial
//
// Test if a sector special is generalized
//
static bool EV_IsGenSectorSpecial(int special)
{
    // UDMF (based on ZDoom's) sector specials
    if(LevelInfo.mapFormat == LEVEL_FORMAT_UDMF_ETERNITY)
        return (special > UDMF_SEC_MASK); // from 256 up
    if(LevelInfo.mapFormat == LEVEL_FORMAT_DOOM && LevelInfo.levelType == LI_TYPE_DOOM)
        return (special > LIGHT_MASK);

    return false;
}

//
// EV_setGeneralizedSectorFlags
//
// Pull out the flag bits from a BOOM generalized sector special and set them
// as sector flags.
//
static void EV_setGeneralizedSectorFlags(sector_t *sector)
{
    // generalized sectors don't exist in old demos
    if(demo_version < 200)
        return;

    // haleyjd 12/28/08: convert BOOM generalized sector types into sector flags
    //         12/31/08: convert BOOM generalized damage

    // convert special bits into flags (correspondence is direct by design)
    int16_t special = sector->special;
    if(LevelInfo.mapFormat == LEVEL_FORMAT_UDMF_ETERNITY)
        special >>= UDMF_BOOM_SHIFT; // to get the flags, we need to move bits

    sector->flags |= (special & GENSECTOFLAGSMASK) >> SECRET_SHIFT;
}

static void EV_applyGeneralizedDamage(sector_t *sector, bool udmf)
{
    // Apply slime damage UNLESS the MBF21 insta-death bit is set, which changes rules
    // convert damage
    int  damagetype = (sector->special >> (udmf ? UDMF_BOOM_SHIFT : 0) & DAMAGE_MASK) >> DAMAGE_SHIFT;
    bool instadeath = mbf21_demo && sector->flags & SECF_INSTANTDEATH;
    // Don't just make a new nukage type with GOD_BREACH_DAMAGE, because most subtypes work
    // differently

    switch(damagetype)
    {
    case 1:
        if(instadeath)
            sector->damageflags |= SDMG_IGNORESUIT;
        else
            EV_SectorDamageNukage(sector); // 5 per 32 tics
        break;
    case 2:
        if(instadeath)
            sector->damageflags |= SDMG_INSTAEXITNORMAL | SDMG_IGNORESUIT;
        else
            EV_SectorDamageHellSlime(sector); // 10 per 32 tics
        break;
    case 3:
        if(instadeath)
            sector->damageflags |= SDMG_INSTAEXITSECRET | SDMG_IGNORESUIT;
        else
            EV_SectorDamageSuperHellSlime(sector); // 20 per 32 tics w/LEAKYSUIT
        break;
    default: //
        break;
    }
}

//
// EV_initGeneralizedSector
//
// Called to initialize a generalized sector. The types considered generalized
// may differ based on the map format or gamemode.
//
static void EV_initGeneralizedSector(sector_t *sector)
{
    // generalized sectors don't exist in old demos (they would have caused the
    // game to bomb out anyway).
    if(demo_version < 200)
    {
        sector->special = 0;
        return;
    }

    // UDMF format handled right here
    if(LevelInfo.mapFormat == LEVEL_FORMAT_UDMF_ETERNITY)
    {
        EV_applyGeneralizedDamage(sector, true);

        // mask it by the smallest 8 bits
        auto binding = EV_UDMFEternityBindingForSectorSpecial(sector->special & UDMF_SEC_MASK);
        if(binding)
            binding->apply(sector);
        return;
    }

    EV_applyGeneralizedDamage(sector, false);

    // apply "light" specials (some are allowed that are not lighting specials)
    auto binding = EV_GenBindingForSectorSpecial(sector->special);
    if(binding)
        binding->apply(sector);
}

//=============================================================================
//
// Sector Special Spawning
//

//
// EV_SpawnSectorSpecials
//
// Called from P_SpawnSpecials during level initialization.
//
void EV_SpawnSectorSpecials()
{
    for(int i = 0; i < numsectors; i++)
    {
        auto sector = &sectors[i];
        bool isgen  = EV_IsGenSectorSpecial(sector->special);

        if(isgen)
            EV_setGeneralizedSectorFlags(sector);

        // count generalized secrets
        if(sector->flags & SECF_SECRET)
            ++totalsecret;

        if(!sector->special)
            continue;

        if(isgen)
            EV_initGeneralizedSector(sector);
        else
        {
            auto binding = EV_BindingForSectorSpecial(sector->special);
            if(binding)
                binding->apply(sector);
        }
    }
}

//=============================================================================
//
// Development Commands
//

//
// ev_mapsectorspecs
//
// List out all the sector specials in use on the current map
//
CONSOLE_COMMAND(ev_mapsectorspecs, cf_level)
{
    for(int i = 0; i < numsectors; i++)
    {
        sector_t *sec = &sectors[i];

        if(!sec->special)
            continue;

        bool isgen = EV_IsGenSectorSpecial(sec->special);
        auto bind  = EV_BindingForSectorSpecial(sec->special);

        C_Printf("%5d: %5d %3d (%s type)\n", i, sec->special, sec->tag, (isgen || bind) ? "known" : "unknown");
    }
}

// EOF

