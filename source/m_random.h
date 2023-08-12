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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Random number generator
//    
//-----------------------------------------------------------------------------


#ifndef M_RANDOM_H__
#define M_RANDOM_H__

#include "d_keywds.h"
#include "m_fixed.h"

//#define RANDOM_LOG
#ifdef RANDOM_LOG
void M_RandomLog(E_FORMAT_STRING(const char *format), ...) E_PRINTF(1, 2);
#else
#define M_RandomLog(...)
#endif


// killough 1/19/98: rewritten to use to use a better random number generator
// in the new engine, although the old one is available for compatibility.

// killough 2/16/98:
//
// Make every random number generator local to each control-equivalent block.
// Critical for demo sync. Changing the order of this list breaks all previous
// versions' demos. The random number generators are made local to reduce the
// chances of sync problems. In Doom, if a single random number generator call
// was off, it would mess up all random number generators. This reduces the
// chances of it happening by making each RNG local to a control flow block.
//
// Notes to developers: if you want to reduce your demo sync hassles, follow
// this rule: for each call to P_Random you add, add a new class to the enum
// type below for each block of code which calls P_Random. If two calls to
// P_Random are not in "control-equivalent blocks", i.e. there are any cases
// where one is executed, and the other is not, put them in separate classes.
//
// Keep all current entries in this list the same, and in the order
// indicated by the #'s, because they're critical for preserving demo
// sync. Do not remove entries simply because they become unused later.

typedef enum {
   pr_skullfly,                // #0
   pr_damage,                  // #1
   pr_crush,                   // #2
   pr_genlift,                 // #3
   pr_killtics,                // #4
   pr_damagemobj,              // #5
   pr_painchance,              // #6
   pr_lights,                  // #7
   pr_explode,                 // #8
   pr_respawn,                 // #9
   pr_lastlook,                // #10
   pr_spawnthing,              // #11
   pr_spawnpuff,               // #12
   pr_spawnblood,              // #13
   pr_missile,                 // #14
   pr_shadow,                  // #15
   pr_plats,                   // #16
   pr_punch,                   // #17
   pr_punchangle,              // #18
   pr_saw,                     // #19
   pr_plasma,                  // #20
   pr_gunshot,                 // #21
   pr_misfire,                 // #22
   pr_shotgun,                 // #23
   pr_bfg,                     // #24
   pr_slimehurt,               // #25
   pr_dmspawn,                 // #26
   pr_missrange,               // #27
   pr_trywalk,                 // #28
   pr_newchase,                // #29
   pr_newchasedir,             // #30
   pr_see,                     // #31
   pr_facetarget,              // #32
   pr_posattack,               // #33
   pr_sposattack,              // #34
   pr_cposattack,              // #35
   pr_spidrefire,              // #36
   pr_troopattack,             // #37
   pr_sargattack,              // #38
   pr_headattack,              // #39
   pr_bruisattack,             // #40
   pr_tracer,                  // #41
   pr_skelfist,                // #42
   pr_scream,                  // #43
   pr_brainscream,             // #44
   pr_cposrefire,              // #45
   pr_brainexp,                // #46
   pr_spawnfly,                // #47
   pr_misc,                    // #48
   pr_all_in_one,              // #49
   // Start new entries -- add new entries below
   pr_opendoor,                // #50
   pr_targetsearch,            // #51
   pr_friends,                 // #52
   pr_threshold,               // #53
   pr_skiptarget,              // #54
   pr_enemystrafe,             // #55
   pr_avoidcrush,              // #56
   pr_stayonlift,              // #57
   pr_helpfriend,              // #58
   pr_dropoff,                 // #59
   pr_randomjump,              // #60
   pr_defect,                  // #61
   pr_script,                  // #62: FraggleScript
   // End of new entries

   // Start Eternity classes
   pr_minatk1,   // Minotaur attacks
   pr_minatk2,
   pr_minatk3,
   pr_mindist,
   pr_mffire,
   pr_settics,   // SetTics codepointer
   pr_volcano,   // Heretic volcano stuff
   pr_svolcano,  // ditto
   pr_clrattack,
   pr_splash,    // TerrainTypes
   pr_lightning, // lightning flashes
   pr_nextflash,
   pr_cloudpick,
   pr_fogangle,
   pr_fogcount,
   pr_fogfloat,
   pr_floathealth, // floatbobbing seed
   pr_subtics,     // A_SubTics
   pr_centauratk,  // A_CentaurAttack
   pr_dropequip,   // A_DropEquipment
   pr_bishop1,
   pr_steamspawn, // steam spawn codepointer
   pr_mincharge,  // minotaur inflictor special
   pr_reflect,    // missile reflection
   pr_tglitz,     // teleglitter z coord
   pr_bishop2,
   pr_custombullets, // parameterized pointers
   pr_custommisfire,
   pr_custompunch,
   pr_tglit,      // teleglitter spawn
   pr_spawnfloat, // random spawn float z flag
   pr_mumpunch,   // mummy punches
   pr_mumpunch2,
   pr_hdrop1,     // heretic item drops
   pr_hdrop2,
   pr_hdropmom,
   pr_clinkatk,   // clink scratch
   pr_ghostsneak, // random failure to sight ghost player
   pr_wizatk,     // wizard attack
   pr_lookact,    // make seesound instead of active sound
   pr_sorctele1,  // d'sparil stuff
   pr_sorctele2,
   pr_sorfx1xpl,
   pr_soratk1,
   pr_soratk2,
   pr_bluespark,
   pr_podpain,    // pod pain
   pr_makepod,    // pod spawn
   pr_knightat1,  // knight scratch
   pr_knightat2,  // knight projectile choice
   pr_dripblood,  // for A_DripBlood
   pr_beastbite,  // beast bite
   pr_puffy,      // beast ball puff spawn
   pr_sorc1atk,   // sorcerer serpent attack
   pr_monbullets, // BulletAttack ptr
   pr_monmisfire,
   pr_setcounter, // SetCounter ptr
   pr_madmelee,   // Heretic mad fighting after player death
   pr_whirlwind,  // Whirlwind inflictor
   pr_lichmelee,  // Iron Lich attacks
   pr_lichattack,
   pr_whirlseek,  // Whirlwind seeking
   pr_impcharge,  // Imp charge attack
   pr_impmelee,   // Imp melee attack
   pr_impmelee2,  // Leader imp melee
   pr_impcrash,   // Imp crash
   pr_rndwnewdir, // RandomWalk rngs
   pr_rndwmovect,
   pr_rndwspawn,
   pr_weapsetctr, // WeaponSetCtr
   pr_quake,      // T_QuakeThinker
   pr_quakedmg,   // quake damage
   pr_skullpop,   // Heretic skull flying
   pr_centaurdef, // A_CentaurDefend
   pr_bishop3,
   pr_spawnblur,  // A_SpawnBlur
   pr_chaosbite,  // A_DemonAttack1
   pr_wraithm,    // A_WraithMelee
   pr_wraithd,
   pr_wraithfx2,
   pr_wraithfx3,
   pr_wraithfx4a,
   pr_wraithfx4b,
   pr_wraithfx4c,
   pr_ettin,
   pr_affritrock,  // A_AffritSpawnRock
   pr_smbounce,    // A_SmBounce
   pr_affrits,     // A_AffritSplotch
   pr_icelook,     // A_IceGuyLook
   pr_icelook2,
   pr_icechase,    // A_IceGuyChase
   pr_icechase2,
   pr_dragonfx,    // A_DragonFX2
   pr_dropmace,    // A_DropMace
   pr_rip,         // ripper missile damage
   pr_casing,      // A_CasingThrust
   pr_genrefire,   // A_GenRefire
   pr_decjump,     // A_Jump
   pr_decjump2,
   pr_spotspawn,   // For use with MobjCollection::spawnAtRandom
   pr_moverandom,  // For use with MobjCollection::moveToRandom
   pr_ravenblood,  // Raven blood spawning
   pr_ripperblood, // Ripper blood spawning
   pr_rogueblood,  // Strife blood spawning
   pr_drawblood,   // Missile blood-drawing chance
   pr_hexenteleport,  // ioanch 20160329: used by Hexen teleporters
   pr_goldwand,    // A_FireGoldWandPL1
   pr_goldwand2,   // A_FireGoldWandPL2
   pr_skullrod,    // A_FireSkullRodPL1
   pr_blaster,     // A_FireBlasterPL1
   pr_staff,       // A_StaffAttackPL1
   pr_staff2,      // A_StaffAttackPL2
   pr_staffangle,
   pr_gauntlets,   // A_GauntletAttack
   pr_gauntletsangle,
   pr_boltspark,   // A_BoltSpark
   pr_firemace,    // A_FireMacePL1
   pr_phoenixrod2, // A_FirePhoenixPL2

   pr_hereticartiteleport, // A_ArtiTele
   pr_puffblood,   // P_shootThing draw blood when Heretic-like puff is defined
   pr_nailbombshoot,  // A_Nailbomb random damage
   pr_chainwiggle, // ST_HticTicker
   pr_envirospot,
   pr_envirotics,

   pr_spawnexchance,           // [XA] 02/28/2020: A_SpawnEx spawnchance
   pr_seekermissile,           // A_SeekerMissile

   pr_wpnreadysnd,

   pr_enviroticsend,

   pr_mbf21,

   pr_fasttrailchance,         // [XA] 02/22/2020: Fast projectile trail spawn-chance

   NUMPRCLASS                  // MUST be last item in list
} pr_class_t;

// The random number generator's state.
struct rng_t
{
   unsigned int  seed[NUMPRCLASS];      // Each block's random seed
   int rndindex, prndindex;             // For compatibility support
};

extern rng_t rng;                      // The rng's state

extern unsigned int rngseed;          // The starting seed (not part of state)

// Returns a number from 0 to 255,
#define M_Random() P_Random(pr_misc)

// As M_Random, but used by the play simulation.
int P_Random(pr_class_t);

// haleyjd: function to get a random near zero
int P_SubRandom(pr_class_t);

// haleyjd: function to get a random within a given range
int P_RangeRandom(pr_class_t pr_class, int min, int max);

#define M_RangeRandom(min, max) P_RangeRandom(pr_misc, (min), (max))

int M_VHereticPRandom(pr_class_t pr_class);

// haleyjd 03/16/09: extended random functions
unsigned int P_RandomEx(pr_class_t);
unsigned int P_RangeRandomEx(pr_class_t, unsigned int, unsigned int);
int P_SubRandomEx(pr_class_t pr_class, unsigned max);

#define M_RandomEx() P_RandomEx(pr_misc)
#define M_RangeRandomEx(min, max) P_RangeRandomEx(pr_misc, (min), (max))

// Fix randoms for demos.
void M_ClearRandom(void);

// [XA] Common random formulas used by codepointers
int P_RandomHitscanAngle(pr_class_t pr_class, fixed_t spread);
int P_RandomHitscanSlope(pr_class_t pr_class, fixed_t spread);

#endif

//----------------------------------------------------------------------------
//
// $Log: m_random.h,v $
// Revision 1.9  1998/05/01  14:20:31  killough
// beautification
//
// Revision 1.8  1998/03/31  10:43:07  killough
// Fix (supposed) RNG problems, add new demo_insurance
//
// Revision 1.7  1998/03/28  17:56:02  killough
// Improve RNG by adding external seed
//
// Revision 1.6  1998/03/09  07:16:39  killough
// Remove unused pr_class (don't do this after 1st release)
//
// Revision 1.5  1998/03/02  11:37:47  killough
// fix misspelling in comment
//
// Revision 1.4  1998/02/23  04:42:01  killough
// Add pr_atracer type
//
// Revision 1.3  1998/02/17  05:40:12  killough
// Make RNGs local to each calling block, for demo sync
//
// Revision 1.2  1998/01/26  19:27:14  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:58  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
