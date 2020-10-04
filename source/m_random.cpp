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
//      Random number LUT.
//
// 1/19/98 killough: Rewrote random number generator for better randomness,
// while at the same time maintaining demo sync and backward compatibility.
//
// 2/16/98 killough: Made each RNG local to each control-equivalent block,
// to reduce the chances of demo sync problems.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "d_gi.h"
#include "doomstat.h"
#include "m_random.h"
#include "a_small.h"

#ifdef RANDOM_LOG
void M_RandomLog(E_FORMAT_STRING(const char *format), ...)
{
   static FILE *f;
   if(!f)
      f = fopen("randomlog.txt", "wt");
   if(f)
   {
     fprintf(f, "%d:", gametic);
     va_list ap;
     va_start(ap, format);
     vfprintf(f, format, ap);
     va_end(ap);
     fflush(f);
   }
}
#endif

//
// M_Random
// Returns a 0-255 number
//
const unsigned char rndtable[256] =  // 1/19/98 killough -- made const
{
     0,   8, 109, 220, 222, 241, 149, 107,  75, 248, 254, 140,  16,  66,
    74,  21, 211,  47,  80, 242, 154,  27, 205, 128, 161,  89,  77,  36,
    95, 110,  85,  48, 212, 140, 211, 249,  22,  79, 200,  50,  28, 188,
    52, 140, 202, 120,  68, 145,  62,  70, 184, 190,  91, 197, 152, 224,
   149, 104,  25, 178, 252, 182, 202, 182, 141, 197,   4,  81, 181, 242,
   145,  42,  39, 227, 156, 198, 225, 193, 219,  93, 122, 175, 249,   0,
   175, 143,  70, 239,  46, 246, 163,  53, 163, 109, 168, 135,   2, 235,
    25,  92,  20, 145, 138,  77,  69, 166,  78, 176, 173, 212, 166, 113,
    94, 161,  41,  50, 239,  49, 111, 164,  70,  60,   2,  37, 171,  75,
   136, 156,  11,  56,  42, 146, 138, 229,  73, 146,  77,  61,  98, 196,
   135, 106,  63, 197, 195,  86,  96, 203, 113, 101, 170, 247, 181, 113,
    80, 250, 108,   7, 255, 237, 129, 226,  79, 107, 112, 166, 103, 241,
    24, 223, 239, 120, 198,  58,  60,  82, 128,   3, 184,  66, 143, 224,
   145, 224,  81, 206, 163,  45,  63,  90, 168, 114,  59,  33, 159,  95,
    28, 139, 123,  98, 125, 196,  15,  70, 194, 253,  54,  14, 109, 226,
    71,  17, 161,  93, 186,  87, 244, 138,  20,  52, 123, 251,  26,  36,
    17,  46,  52, 231, 232,  76,  31, 221,  84,  37, 216, 165, 212, 106,
   197, 242,  98,  43,  39, 175, 254, 145, 190,  84, 118, 222, 187, 136,
   120, 163, 236, 249
};

//
// Converts a class to a string
//
static const char *M_prcString(pr_class_t pr_class)
{
#define CASE(a) case a: return #a
   switch(pr_class)
   {
         CASE(pr_skullfly);                // #1
         CASE(pr_damage);                  // #2
         CASE(pr_crush);                   // #3
         CASE(pr_genlift);                 // #4
         CASE(pr_killtics);                // #5
         CASE(pr_damagemobj);              // #6
         CASE(pr_painchance);              // #7
         CASE(pr_lights);                  // #8
         CASE(pr_explode);                 // #9
         CASE(pr_respawn);                 // #10
         CASE(pr_lastlook);                // #11
         CASE(pr_spawnthing);              // #12
         CASE(pr_spawnpuff);               // #13
         CASE(pr_spawnblood);              // #14
         CASE(pr_missile);                 // #15
         CASE(pr_shadow);                  // #16
         CASE(pr_plats);                   // #17
         CASE(pr_punch);                   // #18
         CASE(pr_punchangle);              // #19
         CASE(pr_saw);                     // #20
         CASE(pr_plasma);                  // #21
         CASE(pr_gunshot);                 // #22
         CASE(pr_misfire);                 // #23
         CASE(pr_shotgun);                 // #24
         CASE(pr_bfg);                     // #25
         CASE(pr_slimehurt);               // #26
         CASE(pr_dmspawn);                 // #27
         CASE(pr_missrange);               // #28
         CASE(pr_trywalk);                 // #29
         CASE(pr_newchase);                // #30
         CASE(pr_newchasedir);             // #31
         CASE(pr_see);                     // #32
         CASE(pr_facetarget);              // #33
         CASE(pr_posattack);               // #34
         CASE(pr_sposattack);              // #35
         CASE(pr_cposattack);              // #36
         CASE(pr_spidrefire);              // #37
         CASE(pr_troopattack);             // #38
         CASE(pr_sargattack);              // #39
         CASE(pr_headattack);              // #40
         CASE(pr_bruisattack);             // #41
         CASE(pr_tracer);                  // #42
         CASE(pr_skelfist);                // #43
         CASE(pr_scream);                  // #44
         CASE(pr_brainscream);             // #45
         CASE(pr_cposrefire);              // #46
         CASE(pr_brainexp);                // #47
         CASE(pr_spawnfly);                // #48
         CASE(pr_misc);                    // #49
         CASE(pr_all_in_one);              // #50
         CASE(pr_opendoor);                // #51
         CASE(pr_targetsearch);            // #52
         CASE(pr_friends);                 // #53
         CASE(pr_threshold);               // #54
         CASE(pr_skiptarget);              // #55
         CASE(pr_enemystrafe);             // #56
         CASE(pr_avoidcrush);              // #57
         CASE(pr_stayonlift);              // #58
         CASE(pr_helpfriend);              // #59
         CASE(pr_dropoff);                 // #60
         CASE(pr_randomjump);              // #61
         CASE(pr_defect);                  // #62
         CASE(pr_script);                  // #63: FraggleScript
         CASE(pr_minatk1);   // Minotaur attacks
         CASE(pr_minatk2);
         CASE(pr_minatk3);
         CASE(pr_mindist);
         CASE(pr_mffire);
         CASE(pr_settics);   // SetTics codepointer
         CASE(pr_volcano);   // Heretic volcano stuff
         CASE(pr_svolcano);  // ditto
         CASE(pr_clrattack);
         CASE(pr_splash);    // TerrainTypes
         CASE(pr_lightning); // lightning flashes
         CASE(pr_nextflash);
         CASE(pr_cloudpick);
         CASE(pr_fogangle);
         CASE(pr_fogcount);
         CASE(pr_fogfloat);
         CASE(pr_floathealth); // floatbobbing seed
         CASE(pr_subtics);     // A_SubTics
         CASE(pr_centauratk);  // A_CentaurAttack
         CASE(pr_dropequip);   // A_DropEquipment
         CASE(pr_bishop1);
         CASE(pr_steamspawn); // steam spawn codepointer
         CASE(pr_mincharge);  // minotaur inflictor special
         CASE(pr_reflect);    // missile reflection
         CASE(pr_tglitz);     // teleglitter z coord
         CASE(pr_bishop2);
         CASE(pr_custombullets); // parameterized pointers
         CASE(pr_custommisfire);
         CASE(pr_custompunch);
         CASE(pr_tglit);      // teleglitter spawn
         CASE(pr_spawnfloat); // random spawn float z flag
         CASE(pr_mumpunch);   // mummy punches
         CASE(pr_mumpunch2);
         CASE(pr_hdrop1);     // heretic item drops
         CASE(pr_hdrop2);
         CASE(pr_hdropmom);
         CASE(pr_clinkatk);   // clink scratch
         CASE(pr_ghostsneak); // random failure to sight ghost player
         CASE(pr_wizatk);     // wizard attack
         CASE(pr_lookact);    // make seesound instead of active sound
         CASE(pr_sorctele1);  // d'sparil stuff
         CASE(pr_sorctele2);
         CASE(pr_sorfx1xpl);
         CASE(pr_soratk1);
         CASE(pr_soratk2);
         CASE(pr_bluespark);
         CASE(pr_podpain);    // pod pain
         CASE(pr_makepod);    // pod spawn
         CASE(pr_knightat1);  // knight scratch
         CASE(pr_knightat2);  // knight projectile choice
         CASE(pr_dripblood);  // for A_DripBlood
         CASE(pr_beastbite);  // beast bite
         CASE(pr_puffy);      // beast ball puff spawn
         CASE(pr_sorc1atk);   // sorcerer serpent attack
         CASE(pr_monbullets); // BulletAttack ptr
         CASE(pr_monmisfire);
         CASE(pr_setcounter); // SetCounter ptr
         CASE(pr_madmelee);   // Heretic mad fighting after player death
         CASE(pr_whirlwind);  // Whirlwind inflictor
         CASE(pr_lichmelee);  // Iron Lich attacks
         CASE(pr_lichattack);
         CASE(pr_whirlseek);  // Whirlwind seeking
         CASE(pr_impcharge);  // Imp charge attack
         CASE(pr_impmelee);   // Imp melee attack
         CASE(pr_impmelee2);  // Leader imp melee
         CASE(pr_impcrash);   // Imp crash
         CASE(pr_rndwnewdir); // RandomWalk rngs
         CASE(pr_rndwmovect);
         CASE(pr_rndwspawn);
         CASE(pr_weapsetctr); // WeaponSetCtr
         CASE(pr_quake);      // T_QuakeThinker
         CASE(pr_quakedmg);   // quake damage
         CASE(pr_skullpop);   // Heretic skull flying
         CASE(pr_centaurdef); // A_CentaurDefend
         CASE(pr_bishop3);
         CASE(pr_spawnblur);  // A_SpawnBlur
         CASE(pr_chaosbite);  // A_DemonAttack1
         CASE(pr_wraithm);    // A_WraithMelee
         CASE(pr_wraithd);
         CASE(pr_wraithfx2);
         CASE(pr_wraithfx3);
         CASE(pr_wraithfx4a);
         CASE(pr_wraithfx4b);
         CASE(pr_wraithfx4c);
         CASE(pr_ettin);
         CASE(pr_affritrock);  // A_AffritSpawnRock
         CASE(pr_smbounce);    // A_SmBounce
         CASE(pr_affrits);     // A_AffritSplotch
         CASE(pr_icelook);     // A_IceGuyLook
         CASE(pr_icelook2);
         CASE(pr_icechase);    // A_IceGuyChase
         CASE(pr_icechase2);
         CASE(pr_dragonfx);    // A_DragonFX2
         CASE(pr_dropmace);    // A_DropMace
         CASE(pr_rip);         // ripper missile damage
         CASE(pr_casing);      // A_CasingThrust
         CASE(pr_genrefire);   // A_GenRefire
         CASE(pr_decjump);     // A_Jump
         CASE(pr_decjump2);
         CASE(pr_spotspawn);   // For use with MobjCollection::spawnAtRandom
         CASE(pr_moverandom);  // For use with MobjCollection::moveToRandom
         CASE(pr_ravenblood);  // Raven blood spawning
         CASE(pr_ripperblood); // Ripper blood spawning
         CASE(pr_rogueblood);  // Strife blood spawning
         CASE(pr_drawblood);   // Missile blood-drawing chance
         CASE(pr_hexenteleport);  // ioanch 20160329: used by Hexen teleporters
         CASE(pr_goldwand);    // A_FireGoldWandPL1
         CASE(pr_goldwand2);   // A_FireGoldWandPL2
         CASE(pr_skullrod);    // A_FireSkullRodPL1
         CASE(pr_blaster);     // A_FireBlasterPL1
         CASE(pr_staff);       // A_StaffAttackPL1
         CASE(pr_staff2);      // A_StaffAttackPL2
         CASE(pr_staffangle);
         CASE(pr_gauntlets);   // A_GauntletAttack
         CASE(pr_gauntletsangle);
         CASE(pr_boltspark);   // A_BoltSpark
         CASE(pr_firemace);    // A_FireMacePL1
         CASE(pr_phoenixrod2); // A_FirePhoenixPL2
         CASE(pr_hereticartiteleport); // A_ArtiTele
         CASE(pr_puffblood);   // P_shootThing draw blood when Heretic-like puff is defined
         CASE(pr_nailbombshoot);  // A_Nailbomb random damage
         CASE(pr_chainwiggle);
         CASE(pr_envirospot);
         CASE(pr_envirotics);
         CASE(pr_enviroticsend);
      default:
         return "unknown";
   }
#undef CASE
}

rng_t rng;     // the random number state

unsigned int rngseed = 1993;   // killough 3/26/98: The seed

int P_Random(pr_class_t pr_class)
{
   // killough 2/16/98:  We always update both sets of random number
   // generators, to ensure repeatability if the demo_compatibility
   // flag is changed while the program is running. Changing the
   // demo_compatibility flag does not change the sequences generated,
   // only which one is selected from.
   //
   // All of this RNG stuff is tricky as far as demo sync goes --
   // it's like playing with explosives :) Lee

   if(pr_class != pr_misc)
   {
      M_RandomLog("%d\n", pr_class);
   }

   int compat; 

   unsigned int boom;

   // killough 3/31/98:
   // If demo sync insurance is not requested, use
   // much more unstable method by putting everything
   // except pr_misc into pr_all_in_one

   pr_class_t prc = pr_class;
   compat = pr_class == pr_misc ?     // sf: moved here
      (rng.prndindex = (rng.prndindex + 1) & 255) :
      (rng.rndindex  = (rng.rndindex  + 1) & 255);

   if(pr_class != pr_misc && !demo_insurance)      // killough 3/31/98
      pr_class = pr_all_in_one;

   boom = rng.seed[pr_class];

   // killough 3/26/98: add pr_class*2 to addend

   rng.seed[pr_class] = boom * 1664525ul + 221297ul + pr_class*2;

   if(demo_compatibility)
   {
      if(pr_class != pr_misc)
         printf("%s: %d: %d\n", M_prcString(prc), gametic, rndtable[compat]);
      return rndtable[compat];
   }

   boom >>= 20;

   // killough 3/30/98: use gametic-levelstarttic to shuffle RNG
   // killough 3/31/98: but only if demo insurance requested,
   // since it's unnecessary for random shuffling otherwise
   // killough 9/29/98: but use basetic now instead of levelstarttic

   if(demo_insurance)
      boom += (gametic-basetic)*7;

   return boom & 255;
}

//
// P_RandomEx
//
// haleyjd 03/16/09: Extended random function for 32-bit integer range.
// Does not shift off 20 bits or AND with 255. Should not be called from
// code expecting the results from the normal random function, or anything
// involving demo compatibility.
//
unsigned int P_RandomEx(pr_class_t pr_class)
{
   unsigned int boom;
   
   if(pr_class != pr_misc && !demo_insurance)
      pr_class = pr_all_in_one;

   boom = rng.seed[pr_class];

   rng.seed[pr_class] = boom * 1664525ul + 221297ul + pr_class*2;

   // ioanch 20160801: needed for better randomness
   boom = ((boom << 24) | (boom >> 8));
   boom += (gametic - basetic) * 7;
   
   return boom;
}

//
// P_SubRandom
//
// haleyjd 08/05/04: This function eliminates the need to use
// temporary variables everywhere in order to subtract two random
// values without incurring order of evaluation problems.
//
int P_SubRandom(pr_class_t pr_class)
{
   int temp = P_Random(pr_class);

   return (temp - P_Random(pr_class));
}

//
// P_RangeRandom
//
// haleyjd 05/31/06: Returns a random number within a given range.
//
int P_RangeRandom(pr_class_t pr_class, int min, int max)
{
   return (P_Random(pr_class) % (max - min + 1)) + min;
}

//
// Heretic demo compatibility switch
//
int M_VHereticPRandom(pr_class_t pr_class)
{
   return vanilla_heretic ? P_Random(pr_class) : M_Random();
}

//
// P_RangeRandomEx
//
// haleyjd 03/16/09: as above, but works for large ranges.
//
unsigned int P_RangeRandomEx(pr_class_t pr_class, 
                             unsigned int min, unsigned int max)
{
   return (P_RandomEx(pr_class) % (max - min + 1)) + min;
}

//
// ioanch: triangular (like P_SubRandom) random with maximum value, based on
// P_RandomEx
//
int P_SubRandomEx(pr_class_t pr_class, unsigned max)
{
   max++; // max has to be 1 more than the supplied arg to function as expected
   const int temp = P_RandomEx(pr_class) % max;
   return temp - static_cast<int>(P_RandomEx(pr_class) % max);
}


//
// Initialize all the seeds
//
// This initialization method is critical to maintaining demo sync.
// Each seed is initialized according to its class, so if new classes
// are added they must be added to end of pr_class_t list. killough
//
void M_ClearRandom()
{
   unsigned int seed = rngseed * 2 + 1; // add 3/26/98: add rngseed
   for(unsigned int &currseed : rng.seed)         // go through each pr_class and set
      currseed = seed *= 69069ul;        // each starting seed differently
   rng.prndindex = rng.rndindex = 0;     // clear two compatibility indices
}

#if 0
static cell AMX_NATIVE_CALL sm_random(AMX *amx, cell *params)
{
   return P_Random(pr_script);
}

//
// sm_mrandom
//
// Implements M_Random()
// 
// This is strictly for non-demo-sync-critical stuff such as
// particle effects.
//
static cell AMX_NATIVE_CALL sm_mrandom(AMX *amx, cell *params)
{
   return M_Random();
}

AMX_NATIVE_INFO random_Natives[] =
{
   { "_P_Random", sm_random  },
   { "_M_Random", sm_mrandom },
   { nullptr, nullptr }
};
#endif

//----------------------------------------------------------------------------
//
// $Log: m_random.c,v $
// Revision 1.6  1998/05/03  23:13:18  killough
// Fix #include
//
// Revision 1.5  1998/03/31  10:43:05  killough
// Fix (supposed) RNG problems, add new demo_insurance
//
// Revision 1.4  1998/03/28  17:56:05  killough
// Improve RNG by adding external seed
//
// Revision 1.3  1998/02/17  05:40:08  killough
// Make RNGs local to each calling block, for demo sync
//
// Revision 1.2  1998/01/26  19:23:51  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:58  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
