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
//      Cheat sequence checking.
//
// NETCODE_FIXME: Cheats need to work in netgames and demos when enabled 
// by the arbitrator. Requires significant changes, including addition of
// cheat events in the input stream. I also want to add the ability to
// turn cheat detection on/off along with the use of letter keys for
// binding of commands, because these uses interfere with each other.
// It should be possible to have an exclusive cheats mode and an exclusive
// bindings mode.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "a_args.h"
#include "c_net.h"
#include "c_runcmd.h"
#include "d_deh.h"    // Ty 03/27/98 - externalized strings
#include "d_dehtbl.h"
#include "d_gi.h"
#include "d_io.h"     // SoM 3/14/2002: strncasecmp
#include "d_mod.h"
#include "doomstat.h"
#include "dstrings.h"
#include "e_inventory.h"
#include "e_player.h"
#include "e_states.h"
#include "e_weapons.h"
#include "g_game.h"
#include "metaapi.h"
#include "m_argv.h"
#include "m_cheat.h"
#include "p_inter.h"
#include "p_mobj.h"
#include "p_setup.h"
#include "p_user.h"
#include "r_data.h"
#include "s_sound.h"
#include "sounds.h"
#include "w_levels.h"
#include "w_wad.h"
#include "dhticstr.h"

#define plyr (&players[consoleplayer])     /* the console player */

// Various flags
enum
{
   FLAG_ALWAYSCALL = 16    // apply this on top of negative arg
};

static int M_NukeMonsters();

//=============================================================================
//
// CHEAT SEQUENCE PACKAGE
//

// Doom Cheats
static void cheat_mus(const void *);
static void cheat_choppers(const void *);
static void cheat_god(const void *);
static void cheat_fa(const void *);
static void cheat_k(const void *);
static void cheat_kfa(const void *);
static void cheat_noclip(const void *);
static void cheat_behold(const void *);
static void cheat_clev(const void *);
static void cheat_mypos(const void *);
static void cheat_massacre(const void *);
static void cheat_ddt(const void *);
static void cheat_key(const void *);
static void cheat_keyx(const void *);
static void cheat_keyxx(const void *);
static void cheat_weap(const void *);
static void cheat_weapx(const void *);
static void cheat_ammo(const void *);
static void cheat_ammox(const void *);
static void cheat_one(const void *);

// Heretic cheats
static void cheat_hticgod(const void *);
static void cheat_htichealth(const void *);
static void cheat_hticiddqd(const void *);
static void cheat_htickeys(const void *);
static void cheat_htickill(const void *);
static void cheat_hticnoclip(const void *);
static void cheat_hticwarp(const void *);
static void cheat_hticbehold(const void *);
static void cheat_hticgimme(const void *);
static void cheat_rambo(const void *);

// Shared cheats
static void cheat_pw(const void *);
static void cheat_infammo(const void *);
static void cheat_comp(const void *);
static void cheat_friction(const void *);
static void cheat_pushers(const void *);
static void cheat_tran(const void *);
static void cheat_hom(const void *);
static void cheat_nuke(const void *);

#ifdef INSTRUMENTED
static void cheat_printstats(const void *);   // killough 8/23/98
#endif

//=============================================================================
//
// List of cheat codes, functions, and special argument indicators.
//
// The first argument is the cheat code.
//
// The second argument is its DEH name, or NULL if it's not supported by -deh.
//
// The third argument is a combination of the bitmasks:
// { always, not_dm, not_coop, not_net, not_menu, not_demo, not_deh },
// which excludes the cheat during certain modes of play.
//
// The fourth argument is the handler function.
//
// The fifth argument is passed to the handler function if it's non-negative;
// if negative, then its negative indicates the number of extra characters
// expected after the cheat code, which are passed to the handler function
// via a pointer to a buffer (after folding any letters to lowercase).
//

cheat_s cheat[CHEAT_NUMCHEATS] = 
{
   // DOOM Cheats
   { "idmus",      Game_DOOM, always,   cheat_mus,      -2                  },
   { "idchoppers", Game_DOOM, not_sync, cheat_choppers,  0                  },
   { "iddqd",      Game_DOOM, not_sync, cheat_god,       0                  },
   { "idk",        Game_DOOM, not_sync, cheat_k,         0                  }, // The most controversial cheat code in Doom history!!!
   { "idkfa",      Game_DOOM, not_sync, cheat_kfa,       0                  },
   { "idfa",       Game_DOOM, not_sync, cheat_fa,        0                  },
   { "idspispopd", Game_DOOM, not_sync, cheat_noclip,    0                  },
   { "idclip",     Game_DOOM, not_sync, cheat_noclip,    0                  },
   { "idbeholdv",  Game_DOOM, not_sync, cheat_pw,        pw_invulnerability },
   { "idbeholds",  Game_DOOM, not_sync, cheat_pw,        pw_strength        },
   { "idbeholdi",  Game_DOOM, not_sync, cheat_pw,        pw_invisibility    },
   { "idbeholdr",  Game_DOOM, not_sync, cheat_pw,        pw_ironfeet        },
   { "idbeholda",  Game_DOOM, not_sync, cheat_pw,        pw_allmap          },
   { "idbeholdl",  Game_DOOM, not_sync, cheat_pw,        pw_infrared        },
   { "idbehold",   Game_DOOM, not_sync, cheat_behold,    0                  },
   { "idclev",     Game_DOOM, not_sync, cheat_clev,     -2                  },
   { "idmypos",    Game_DOOM, always,   cheat_mypos,     0                  },
   { "iddt",       Game_DOOM, not_dm,   cheat_ddt,       0                  }, // killough 2/07/98: moved from am_map.c
   { "key",        Game_DOOM, not_sync, cheat_key,       0                  }, // killough 2/16/98: generalized key cheats
   { "keyr",       Game_DOOM, not_sync, cheat_keyx,      0                  },
   { "keyy",       Game_DOOM, not_sync, cheat_keyx,      0                  },
   { "keyb",       Game_DOOM, not_sync, cheat_keyx,      0                  },
   { "keyrc",      Game_DOOM, not_sync, cheat_keyxx,     it_redcard         },
   { "keyyc",      Game_DOOM, not_sync, cheat_keyxx,     it_yellowcard      },
   { "keybc",      Game_DOOM, not_sync, cheat_keyxx,     it_bluecard        },
   { "keyrs",      Game_DOOM, not_sync, cheat_keyxx,     it_redskull        },
   { "keyys",      Game_DOOM, not_sync, cheat_keyxx,     it_yellowskull     },
   { "keybs",      Game_DOOM, not_sync, cheat_keyxx,     it_blueskull       }, // killough 2/16/98: end generalized keys
   { "weap",       Game_DOOM, not_sync, cheat_weap,      0                  }, // killough 2/16/98: generalized weapon cheats
   { "weap",       Game_DOOM, not_sync, cheat_weapx,    -1                  },
   { "ammo",       Game_DOOM, not_sync, cheat_ammo,      0                  },
   { "ammo",       Game_DOOM, not_sync, cheat_ammox,    -1                  }, // killough 2/16/98: end generalized weapons
   { "iamtheone",  Game_DOOM, not_sync, cheat_one,       0                  },
   { "killem",     Game_DOOM, not_sync, cheat_massacre,  0                  }, // jff 2/01/98 kill all monsters

   // Heretic Cheats
   { "quicken",   Game_Heretic, not_sync, cheat_hticgod,     0                  },
   { "ponce",     Game_Heretic, not_sync, cheat_htichealth,  0                  },
   { "iddqd",     Game_Heretic, not_sync, cheat_hticiddqd,   0                  },
   { "skel",      Game_Heretic, not_sync, cheat_htickeys,    0                  },
   { "massacre",  Game_Heretic, not_sync, cheat_htickill,    0                  },
   { "kitty",     Game_Heretic, not_sync, cheat_hticnoclip,  0                  },
   { "engage",    Game_Heretic, not_sync, cheat_hticwarp,   -2                  },
   { "ravmap",    Game_Heretic, not_dm,   cheat_ddt,         0                  },
   { "ravpowerv", Game_Heretic, not_sync, cheat_pw,          pw_invulnerability },
   { "ravpowerg", Game_Heretic, not_sync, cheat_pw,          pw_ghost           },
   { "ravpowera", Game_Heretic, not_sync, cheat_pw,          pw_allmap          },
   { "ravpowert", Game_Heretic, not_sync, cheat_pw,          pw_torch           },
   { "ravpowerf", Game_Heretic, not_sync, cheat_pw,          pw_flight          },
   { "ravpowerr", Game_Heretic, not_sync, cheat_pw,          pw_ironfeet        },
   { "ravpower",  Game_Heretic, not_sync, cheat_hticbehold,  0                  },
   { "gimme",     Game_Heretic, not_sync, cheat_hticgimme, -(2 | FLAG_ALWAYSCALL) },
   { "rambo",     Game_Heretic, not_sync, cheat_rambo,       0                  },

   // Shared Cheats
   { "comp",     -1, not_sync, cheat_comp,     0             }, // phares
   { "hom",      -1, always,   cheat_hom,      0             }, // killough 2/07/98: HOM autodetector
   { "tran",     -1, always,   cheat_tran,     0             }, // invoke translucency - phares
   { "ice",      -1, not_sync, cheat_friction, 0             }, // phares 3/10/98: toggle variable friction effects
   { "push",     -1, not_sync, cheat_pushers,  0             }, // phares 3/10/98: toggle pushers
   { "nuke",     -1, not_sync, cheat_nuke,     0             }, // killough 12/98: disable nukage damage
   { "hideme",   -1, not_sync, cheat_pw,       pw_totalinvis }, // haleyjd: total invis cheat -- hideme
   { "ghost",    -1, not_sync, cheat_pw,       pw_ghost      }, // haleyjd: heretic ghost 
   { "infshots", -1, not_sync, cheat_infammo,  0             },
   { "silence",  -1, not_sync, cheat_pw,       pw_silencer   },

#ifdef INSTRUMENTED
   // Debug Cheats
   { "stat", -1, always, cheat_printstats, 0 },
#endif

   { NULL, -1, 0, NULL, 0 } // end-of-list marker
};

//-----------------------------------------------------------------------------

#ifdef INSTRUMENTED
static void cheat_printstats(const void *arg)    // killough 8/23/98
{
   if(!(printstats = !printstats))
      doom_printf("Memory stats off");
}
#endif

static void cheat_mus(const void *arg)
{
   const char *buf = (const char *)arg;
   
   //jff 3/20/98 note: this cheat allowed in netgame/demorecord
   
   //jff 3/17/98 avoid musnum being negative and crashing
   if(!ectype::isDigit(buf[0]) || !ectype::isDigit(buf[1]))
      return;

   int musnum = GameModeInfo->MusicCheat(buf);
   if(musnum < 0)
      doom_printf("%s", DEH_String("STSTR_NOMUS")); // Ty 03/27/98 - externalized
   else
   {
      doom_printf("%s", DEH_String("STSTR_MUS")); // Ty 03/27/98 - externalized
      S_ChangeMusicNum(musnum, 1);
      idmusnum = musnum; // jff 3/17/98: remember idmus number for restore
   }
}

// 'choppers' invulnerability & chainsaw
static void cheat_choppers(const void *arg)
{
   weaponinfo_t *chainsaw = E_WeaponForDEHNum(wp_chainsaw);
   if(!chainsaw)
      return; // For whatever reason, the chainsaw isn't defined
   E_GiveWeapon(plyr, chainsaw);
   doom_printf("%s", DEH_String("STSTR_CHOPPERS")); // Ty 03/27/98 - externalized
}

static void cheat_god(const void *arg)
{                                    // 'dqd' cheat for toggleable god mode
   C_RunTextCmd("god");
}

// haleyjd 03/18/03: infinite ammo cheat

static void cheat_infammo(const void *arg)
{
   C_RunTextCmd("infammo");
}

// haleyjd 04/07/03: super cheat

static void cheat_one(const void *arg)
{
   int pwinv = pw_totalinvis;
   int pwsil = pw_silencer;

   if(!(players[consoleplayer].cheats & CF_GODMODE))
      C_RunTextCmd("god");
   if(!(players[consoleplayer].cheats & CF_INFAMMO))
      C_RunTextCmd("infammo");
   cheat_fa(arg);
   if(!players[consoleplayer].powers[pw_totalinvis])
      cheat_pw(&pwinv);
   if(!players[consoleplayer].powers[pw_silencer])
      cheat_pw(&pwsil);
}

static void cheat_fa(const void *arg)
{
   if(!E_PlayerHasBackpack(plyr))
      E_GiveBackpack(plyr);

   itemeffect_t *armor = E_ItemEffectForName(ITEMNAME_IDFAARMOR);
   if(armor)
   {
      plyr->armorpoints  = armor->getInt("saveamount",  0);
      plyr->armorfactor  = armor->getInt("savefactor",  1);
      plyr->armordivisor = armor->getInt("savedivisor", 3);
   }

   E_GiveAllClassWeapons(plyr);

   // give full ammo
   E_GiveAllAmmo(plyr, GAA_MAXAMOUNT);

   doom_printf("%s", DEH_String("STSTR_FAADDED"));
}

static void cheat_k(const void *arg)
{
   // sf: fix multiple 'keys added' messages
   if(E_GiveAllKeys(plyr))
      doom_printf("Keys Added");
}

static void cheat_kfa(const void *arg)
{
  cheat_k(arg);
  cheat_fa(arg);

  doom_printf(STSTR_KFAADDED);
}

static void cheat_noclip(const void *arg)
{
   // Simplified, accepting both "noclip" and "idspispopd".
   C_RunTextCmd("noclip");
}

// 'behold?' power-up cheats (modified for infinite duration -- killough)
static void cheat_pw(const void *arg)
{
   int pw = *(const int *)arg;

   if(plyr->powers[pw])
      plyr->powers[pw] = pw!=pw_strength && pw!=pw_allmap && pw!=pw_silencer;  // killough
   else
   {
      static const int tics[NUMPOWERS] =
      {
         INVULNTICS,
         1,          // strength
         INVISTICS,
         IRONTICS,
         1,          // allmap
         INFRATICS,
         INVISTICS,  // haleyjd: totalinvis
         INVISTICS,  // haleyjd: ghost
         1,          // haleyjd: silencer
         FLIGHTTICS, // haleyjd: flight
         INFRATICS,  // haleyjd: torch
      };
      P_GivePower(plyr, pw, tics[pw], false);
      if(pw != pw_strength && !comp[comp_infcheat])
         plyr->powers[pw] = -1;      // infinite duration -- killough
   }

   // haleyjd: stop flight if necessary
   if(pw == pw_flight && !plyr->powers[pw_flight])
      P_PlayerStopFlight(plyr);
   
   doom_printf("%s", DEH_String("STSTR_BEHOLDX")); // Ty 03/27/98 - externalized
}

// 'behold' power-up menu
static void cheat_behold(const void *arg)
{
   doom_printf("%s", DEH_String("STSTR_BEHOLD")); // Ty 03/27/98 - externalized
}

//
// cht_levelWarp
//
// haleyjd 08/23/13: Shared logic for warp cheats
//
static bool cht_levelWarp(const char *buf)
{
   char mapname[9];
   int epsd, map, lumpnum;
   bool foundit = false;
   WadDirectory *levelDir = g_dir; // 11/06/12: look in g_dir first
   int mission = inmanageddir;     // save current value of managed mission

   // return silently on nonsense input
   if(buf[0] < '0' || buf[0] > '9' ||
      buf[1] < '0' || buf[1] > '9')
      return false;

   // haleyjd: build mapname
   memset(mapname, 0, sizeof(mapname));

   if(GameModeInfo->flags & GIF_MAPXY)
   {
      epsd = 1; //jff was 0, but espd is 1-based 
      map  = (buf[0] - '0')*10 + buf[1] - '0';

      psnprintf(mapname, sizeof(mapname), "MAP%02d", map);
   }
   else
   {
      epsd = buf[0] - '0';
      map  = buf[1] - '0';

      psnprintf(mapname, sizeof(mapname), "E%dM%d", epsd, map);
   }

   // haleyjd: check mapname for existence and validity as a map
   do
   {
      lumpnum = levelDir->checkNumForName(mapname);

      if(lumpnum != -1 && P_CheckLevel(levelDir, lumpnum) != LEVEL_FORMAT_INVALID)
      {
         foundit = true;
         break; // got one!
      }
   }
   while(levelDir != &wGlobalDir && (levelDir = &wGlobalDir));

   if(!foundit)
   {
      player_printf(plyr, "%s not found or is not a valid map", mapname);
      return false;
   }

   // So be it.

   idmusnum = -1; //jff 3/17/98 revert to normal level music on IDCLEV

   G_DeferedInitNewFromDir(gameskill, mapname, levelDir);
   
   // restore mission if appropriate
   if(levelDir != &wGlobalDir)
      inmanageddir = mission;

   return true;
}

// 'clev' change-level cheat
static void cheat_clev(const void *arg)
{
   if(cht_levelWarp(static_cast<const char *>(arg)))
      player_printf(plyr, "%s", DEH_String("STSTR_CLEV")); // Ty 03/27/98 - externalized

}

// 'mypos' for player position
// killough 2/7/98: simplified using doom_printf and made output more user-friendly
static void cheat_mypos(const void *arg)
{
   player_printf(plyr, "Position (%d,%d,%d)\tAngle %-.0f", 
                 plyr->mo->x / FRACUNIT,
                 plyr->mo->y / FRACUNIT,
                 plyr->mo->z / FRACUNIT,
                 (double)plyr->mo->angle / ANGLE_1);
}

// compatibility cheat
static void cheat_comp(const void *arg)
{
   int i;
   
   // Ty 03/27/98 - externalized
   doom_printf("%s",
      DEH_String((compatibility = !compatibility) ? "STSTR_COMPON"
                                                  : "STSTR_COMPOFF"));

   for(i = 0; i < COMP_TOTAL; i++) // killough 10/98: reset entire vector
      comp[i] = compatibility;
}

// variable friction cheat
static void cheat_friction(const void *arg)
{
   C_RunTextCmd("varfriction /");        //sf
   
   // FIXME: externalize strings
   doom_printf(variable_friction ? "Variable Friction enabled" : 
                                   "Variable Friction disabled" );
}

// Pusher cheat
// phares 3/10/98
static void cheat_pushers(const void *arg)
{
   C_RunTextCmd("pushers /");
   
   // FIXME: externalize strings
   doom_printf(allow_pushers ? "pushers enabled" : "pushers disabled");
}

// translucency cheat
static void cheat_tran(const void *arg)
{
   C_RunTextCmd("r_trans /");    // sf
}

// jff 2/01/98 kill all monsters
static void cheat_massacre(const void *arg)
{
   C_RunTextCmd("nuke");  //sf
}

// killough 2/7/98: move iddt cheat from am_map.c to here
// killough 3/26/98: emulate Doom better
static void cheat_ddt(const void *arg)
{
   extern int ddt_cheating;
   extern bool automapactive;
   if(automapactive)
      ddt_cheating = (ddt_cheating + 1) % 3;
}

// killough 2/7/98: HOM autodetection
static void cheat_hom(const void *arg)
{
   C_RunTextCmd("r_showhom /"); //sf
}

// killough 2/16/98: keycard/skullkey cheat functions
static void cheat_key(const void *arg)
{
   doom_printf("Red, Yellow, Blue");  // Ty 03/27/98 - *not* externalized
}

static void cheat_keyx(const void *arg)
{
   doom_printf("Card, Skull");        // Ty 03/27/98 - *not* externalized
}

static void cheat_keyxx(const void *arg)
{
   int key = *(const int *)arg;
   const char *msg = NULL;

   if(key >= GameModeInfo->numHUDKeys)
      return;

   itemeffect_t *fx;
   if(!(fx = E_ItemEffectForName(GameModeInfo->cardNames[key])))
      return;

   if(!E_GetItemOwnedAmount(plyr, fx))
   {
      E_GiveInventoryItem(plyr, fx);
      msg = "Key Added"; // Ty 03/27/98 - *not* externalized
   }
   else
   {
      E_RemoveInventoryItem(plyr, fx, -1);
      msg = "Key Removed";
   }

   doom_printf("%s", msg);
}

// killough 2/16/98: generalized weapon cheats
static void cheat_weap(const void *arg)
{                                   // Ty 03/27/98 - *not* externalized
   doom_printf(enable_ssg ?           // killough 2/28/98
               "Weapon number 1-9" : "Weapon number 1-8" );
}

static void cheat_weapx(const void *arg)
{
   const char *buf = (const char *)arg;
   int w = *buf - '1';
   int pwstr = pw_strength;

   if((w == wp_supershotgun && !enable_ssg) ||      // killough 2/28/98
      ((w == wp_bfg || w == wp_plasma) && GameModeInfo->id == shareware))
      return;

   if(w == wp_fist)           // make '1' apply beserker strength toggle
      cheat_pw(&pwstr);
   else if(w > wp_fist && w < NUMWEAPONS)
   {
      weaponinfo_t *weapon = E_WeaponForDEHNum(w);
      if(!E_PlayerOwnsWeapon(plyr, weapon))
      {
         E_GiveWeapon(plyr, weapon);
         doom_printf("Weapon Added");  // Ty 03/27/98 - *not* externalized
      }
      else
      {
         E_RemoveInventoryItem(plyr, weapon->tracker, 1);
         doom_printf("Weapon Removed"); // Ty 03/27/98 - *not* externalized
      }
   }
}

// killough 2/16/98: generalized ammo cheats
static void cheat_ammo(const void *arg)
{
   doom_printf("Ammo 1-4, Backpack");  // Ty 03/27/98 - *not* externalized
}

static const char *cheat_AmmoForNum[NUMAMMO] =
{
   "AmmoClip",
   "AmmoShell",
   "AmmoMissile",
   "AmmoCell"
};

static void cheat_ammox(const void *arg)
{
   const char *buf = (const char *)arg;
   int a = *buf - '1';

   if(*buf == 'b')
   {
      if(!E_PlayerHasBackpack(plyr))
      {
         doom_printf("Backpack Added");
         E_GiveBackpack(plyr);
      }
      else
      {
         doom_printf("Backpack Removed");
         E_RemoveBackpack(plyr);
      }
   }
   else if(a >= 0 && a < NUMAMMO)
   { 
      // haleyjd 10/24/09: altered behavior:
      // * Add ammo if ammo is not at maxammo.
      // * Remove ammo otherwise.
      auto item = E_ItemEffectForName(cheat_AmmoForNum[a]);
      if(!item)
         return;

      int amount    = E_GetItemOwnedAmount(plyr, item);
      int maxamount = E_GetMaxAmountForArtifact(plyr, item);

      if(amount != maxamount)
      {
         E_GiveInventoryItem(plyr, item, maxamount);
         doom_printf("Ammo Added");
      }
      else
      {
         E_RemoveInventoryItem(plyr, item, -1);
         doom_printf("Ammo Removed");
      }
   }
}

static void cheat_nuke(const void *arg)
{
   extern int enable_nuke;
   doom_printf((enable_nuke = !enable_nuke) ? "Nukage Enabled" :
                                              "Nukage Disabled");
}

//=============================================================================
//
// Heretic Cheats
//

//
// Heretic God Mode cheat - quicken
//
static void cheat_hticgod(const void *arg)
{
   plyr->cheats ^= CF_GODMODE;

   if(plyr->cheats & CF_GODMODE)
      player_printf(plyr, "%s", DEH_String("TXT_CHEATGODON"));
   else
      player_printf(plyr, "%s", DEH_String("TXT_CHEATGODOFF"));
}

//
// Heretic No Clip cheat - kitty
//
static void cheat_hticnoclip(const void *arg)
{
   plyr->cheats ^= CF_NOCLIP;

   if(plyr->cheats & CF_NOCLIP)
      player_printf(plyr, "%s", DEH_String("TXT_CHEATNOCLIPON"));
   else
      player_printf(plyr, "%s", DEH_String("TXT_CHEATNOCLIPOFF"));
}

// HTIC_TODO: weapons cheat "rambo"
// HTIC_TODO: tome of power cheat "shazam"

//
// Heretic Full Health cheat - ponce
//
static void cheat_htichealth(const void *arg)
{
   plyr->health = plyr->mo->health = plyr->pclass->initialhealth;
   player_printf(plyr, "%s", DEH_String("TXT_CHEATHEALTH"));
}

//
// Heretic Keys cheat - skel
//
static void cheat_htickeys(const void *arg)
{
   if(E_GiveAllKeys(plyr))
      player_printf(plyr, "%s", DEH_String("TXT_CHEATKEYS"));
}

//
// Heretic Warp cheat - engage
//
static void cheat_hticwarp(const void *arg)
{
   if(cht_levelWarp(static_cast<const char *>(arg)))
      player_printf(plyr, "%s", DEH_String("TXT_CHEATWARP"));
}

// HTIC_TODO: chicken cheat - cockadoodledoo

//
// Heretic Kill-All cheat - massacre
//
static void cheat_htickill(const void *arg)
{
   M_NukeMonsters();
   doom_printf("%s", DEH_String("TXT_CHEATMASSACRE"));
}

// HTIC_TODO: idkfa - remove player's weapons

//
// Heretic IDDQD - kill yourself. Best cheat ever!
//
static void cheat_hticiddqd(const void *arg)
{
   P_DamageMobj(plyr->mo, NULL, plyr->mo, 10000, MOD_UNKNOWN);
   player_printf(plyr, "%s", DEH_String("TXT_CHEATIDDQD"));
}

// New cheats, to fill in for Heretic's shortcomings

//
// Heretic Powerup cheat
//
static void cheat_hticbehold(const void *arg)
{
   player_printf(plyr, "inVuln, Ghost, Allmap, Torch, Fly or Rad");
}


static constexpr char const *hartiNames[] =
{
   "ArtiInvulnerability",
   "ArtiInvisibility",
   "ArtiHealth",
   "ArtiSuperHealth",
   "ArtiTomeOfPower",
   "ArtiTorch",
   "ArtiTimeBomb",
   "ArtiEgg",
   "ArtiFly",
   "ArtiTeleport"

};

static constexpr int numHArtifacts = earrlen(hartiNames);

//
// Adapted CheatArtifact1Func from Choco Heretic
//
static void cheat_hticgimme(const void *varg)
{
   auto args = static_cast<const char *>(varg);
   if(!*args)
   {
      player_printf(plyr, DEH_String(TXT_CHEATARTIFACTS1));
      return;
   }
   if(!*(args + 1))
   {
      player_printf(plyr, DEH_String(TXT_CHEATARTIFACTS2));
      return;
   }
   int i;
   int type;
   int count;

   type = args[0] - 'a';
   count = args[1] - '0';
   if(type == 25 && count == 0)
   {                           // All artifacts
      for(i = 0; i < numHArtifacts; i++)
      {
         itemeffect_t *artifact = E_ItemEffectForName(hartiNames[i]);
         if(artifact == nullptr)
            continue;
         if((GameModeInfo->flags & GIF_SHAREWARE) && artifact->getInt("noshareware", 0))
            continue;
         E_GiveInventoryItem(plyr, artifact, E_GetMaxAmountForArtifact(plyr, artifact));
      }
      player_printf(plyr, DEH_String(TXT_CHEATARTIFACTS3));
   }
   else if(type >= 0 && type < numHArtifacts && count > 0 && count < 10)
   {
      itemeffect_t *artifact = E_ItemEffectForName(hartiNames[type]);
      if(artifact == nullptr)
      {
         return;
      }
      if((GameModeInfo->flags & GIF_SHAREWARE) && artifact->getInt("noshareware", 0))
      {
         player_printf(plyr, DEH_String(TXT_CHEATARTIFACTSFAIL));
         return;
      }
      E_GiveInventoryItem(plyr, artifact, count);
      player_printf(plyr, DEH_String(TXT_CHEATARTIFACTS3));
   }
   else
   {                           // Bad input
      player_printf(plyr, DEH_String(TXT_CHEATARTIFACTSFAIL));
   }

}

static void cheat_rambo(const void *arg)
{
   if(!E_PlayerHasBackpack(plyr))
      E_GiveBackpack(plyr);

   itemeffect_t *armor = E_ItemEffectForName(ITEMNAME_RAMBOARMOR);
   if(armor)
   {
      plyr->armorpoints  = armor->getInt("saveamount",  0);
      plyr->armorfactor  = armor->getInt("savefactor",  1);
      plyr->armordivisor = armor->getInt("savedivisor", 3);
   }

   E_GiveAllClassWeapons(plyr);

   // give full ammo
   E_GiveAllAmmo(plyr, GAA_MAXAMOUNT);

   player_printf(plyr, DEH_String(TXT_CHEATWEAPONS));
}

//-----------------------------------------------------------------------------
// 2/7/98: Cheat detection rewritten by Lee Killough, to avoid
// scrambling and to use a more general table-driven approach.
//-----------------------------------------------------------------------------

#define CHEAT_ARGS_MAX 8  /* Maximum number of args at end of cheats */

void M_DoCheat(const char *s)
{
   while(*s)
   {
      M_FindCheats(*s);
      s++;
   }
}

bool M_FindCheats(int key)
{
   static uint64_t sr;
   static char argbuf[CHEAT_ARGS_MAX+1], *arg;
   static int init, argsleft, cht;
   static bool argsAlwaysCall;
   int i, matchedbefore; 
   bool ret;

   // haleyjd: no cheats in menus, at all.
   if(menuactive)
      return false;

   // If we are expecting arguments to a cheat
   // (e.g. idclev), put them in the arg buffer

   if(argsleft)
   {
      *arg++ = ectype::toLower(key);     // store key in arg buffer
      if(!--argsleft || argsAlwaysCall)  // if last key in arg list,
      {
         cheat[cht].func(argbuf);        // process the arg buffer
         if(GameModeInfo->flags & GIF_CHEATSOUND)
            S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_ACTIVATE]);
      }
      return true;                       // affirmative response
   }

   key = ectype::toLower(key) - 'a';
   if(key < 0 || key >= 32)              // ignore most non-alpha cheat letters
   {
      sr = 0;        // clear shift register
      return false;
   }

   if(!init)                             // initialize aux entries of table
   {
      init = 1;

      for(i = 0; cheat[i].cheat; i++)
      {
         uint64_t c=0, m=0;
         const unsigned char *p;

         for(p = (const unsigned char *)cheat[i].cheat; *p; p++)
         {
            unsigned int ikey = ectype::toLower(*p) - 'a'; // convert to 0-31

            if(ikey >= 32)             // ignore most non-alpha cheat letters
               continue;

            c = (c << 5) + ikey;      // shift key into code
            m = (m << 5) + 31;        // shift 1's into mask
         }

         cheat[i].code = c;           // code for this cheat key
         cheat[i].mask = m;           // mask for this cheat key
      }
   }

   sr = (sr << 5) + key;               // shift this key into shift register

   // haleyjd: Oh Lee, you cad.
#if 0
   {signed/*long*/volatile/*double *x,*y;*/static/*const*/int/*double*/i;
    /**/char/*(*)*/*D_DoomExeName/*(int)*/(void)/*?*/;(void/*)^x*/)((
    /*sr|1024*/32767/*|8%key*/&sr)-19891||/*isupper(c*/strcasecmp/*)*/
    ("b"/*"'%2d!"*/"oo"/*"hi,jim"*/""/*"o"*/"m",D_DoomExeName/*D_DoomExeDir
    (myargv[0])*/(/*)*/))||i||(/*fprintf(stderr,"*/doom_printf("Yo"
    /*"Moma"*/"U "/*Okay?*/"mUSt"/*for(you;read;tHis){/_*/" be a "/*MAN! Re-*/
    "member"/*That.*/" TO uSe"/*x++*/" t"/*(x%y)+5*/"HiS "/*"Life"*/
    "cHe"/*"eze"**/"aT"),i/*+--*/++/*;&^*/));}
#endif

   ret = false;
   for(matchedbefore = i = 0; cheat[i].cheat; i++)
   {
      cheat_s &curcht = cheat[i];

      // if match found & allowed
      if((curcht.gametype == -1 || curcht.gametype == GameModeInfo->type) &&
         (sr & curcht.mask) == curcht.code && 
         !(curcht.when & not_dm   && netgame && GameType == gt_dm && !demoplayback) &&
         !(curcht.when & not_coop && netgame && GameType == gt_coop) &&
         !(curcht.when & not_demo && (demorecording || demoplayback)) &&
         !curcht.deh_disabled)
      {
         if(curcht.arg < 0)               // if additional args are required
         {
            cht = i;                      // remember this cheat code
            arg = argbuf;                 // point to start of arg buffer
            argsleft = -curcht.arg & ~FLAG_ALWAYSCALL;       // number of args expected
            memset(arg, 0, argsleft);
            ret = true;                   // responder has eaten key
            if((argsAlwaysCall = !!(-curcht.arg & FLAG_ALWAYSCALL)))
            {
               curcht.func(arg);
               if(GameModeInfo->flags & GIF_CHEATSOUND)
                  S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_ACTIVATE]);
            }
         }
         else if(!matchedbefore)          // allow only one cheat at a time 
         {
            matchedbefore = 1;            // responder has eaten key
            ret = true;
            curcht.func(&(curcht.arg));   // call cheat handler
            if(GameModeInfo->flags & GIF_CHEATSOUND)
               S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_ACTIVATE]);
         }
      }
   }

   return ret;
}

/***************************
           CONSOLE COMMANDS
 ***************************/

/******** command list *********/

// haleyjd 03/18/03: infinite ammo cheat, at long last

CONSOLE_COMMAND(infammo, cf_notnet|cf_level)
{
   int value = 0;
   if(Console.argc)
      sscanf(Console.argv[0]->constPtr(), "%i", &value);
   else
      value = !(players[consoleplayer].cheats & CF_INFAMMO);

   players[consoleplayer].cheats &= ~CF_INFAMMO;
   players[consoleplayer].cheats |= value ? CF_INFAMMO : 0;

   doom_printf(players[consoleplayer].cheats & CF_INFAMMO ?
               "Infinite ammo on" : "Infinite ammo off");
}

CONSOLE_COMMAND(noclip, cf_notnet|cf_level)
{
   int value=0;

   if(Console.argc)
      sscanf(Console.argv[0]->constPtr(), "%i", &value);
   else
      value = !(players[consoleplayer].cheats & CF_NOCLIP);

   players[consoleplayer].cheats &= ~CF_NOCLIP;
   players[consoleplayer].cheats |= value ? CF_NOCLIP : 0;

   doom_printf("%s",
               DEH_String(players[consoleplayer].cheats & CF_NOCLIP ?
               "STSTR_NCON" : "STSTR_NCOFF"));
}

CONSOLE_COMMAND(god, cf_notnet|cf_level)
{
   int value = 0;        // sf: choose to set to 0 or 1 

   if(Console.argc)
      sscanf(Console.argv[0]->constPtr(), "%i", &value);
   else
      value = !(players[consoleplayer].cheats & CF_GODMODE);
   
   players[consoleplayer].cheats &= ~CF_GODMODE;
   players[consoleplayer].cheats |= value ? CF_GODMODE : 0;
   
   if(players[consoleplayer].cheats & CF_GODMODE)
   {
      if (players[consoleplayer].mo)
         players[consoleplayer].mo->health = god_health;  // Ty 03/09/98 - deh
      
      players[consoleplayer].health = god_health;
      doom_printf("%s", DEH_String("STSTR_DQDON")); // Ty 03/27/98 - externalized
   }
   else 
      doom_printf("%s", DEH_String("STSTR_DQDOFF")); // Ty 03/27/98 - externalized
}

CONSOLE_COMMAND(buddha, cf_notnet|cf_level)
{
   int value = 0;
   if(Console.argc)
      sscanf(Console.argv[0]->constPtr(), "%i", &value);
   else
      value = !(players[consoleplayer].cheats & CF_IMMORTAL);

   players[consoleplayer].cheats &= ~CF_IMMORTAL;
   players[consoleplayer].cheats |= value ? CF_IMMORTAL : 0;

   doom_printf(players[consoleplayer].cheats & CF_IMMORTAL ?
               "Immortality on" : "Immortality off");
}

CONSOLE_COMMAND(fly, cf_notnet|cf_level)
{
   player_t *p = &players[consoleplayer];

   if(!(p->powers[pw_flight]))
   {
      p->powers[pw_flight] = -1;
      P_PlayerStartFlight(p, true);
   }
   else
   {
      p->powers[pw_flight] = 0;
      P_PlayerStopFlight(p);
   }

   doom_printf(p->powers[pw_flight] ? "Flight on" : "Flight off");
}

extern void A_Fall(actionargs_t *);
extern void A_PainDie(actionargs_t *);

// haleyjd 07/13/03: special actions for killem cheat

//
// Pain Elemental -- spawn lost souls now so they get killed
//
void A_PainNukeSpec(actionargs_t *actionargs)
{
   A_PainDie(actionargs);  // killough 2/8/98
   P_SetMobjState(actionargs->actor, E_SafeState(S_PAIN_DIE6));
}

//
// D'Sparil (first form) -- don't spawn second form
//
void A_SorcNukeSpec(actionargs_t *actionargs)
{
   A_Fall(actionargs);
   P_SetMobjStateNF(actionargs->actor, E_SafeState(S_SRCR1_DIE17));
}

//
// M_NukeMonsters
//
// jff 02/01/98 'em' cheat - kill all monsters
// partially taken from Chi's .46 port
//
// killough 2/7/98: cleaned up code and changed to use doom_printf;
//    fixed lost soul bug (LSs left behind when PEs are killed)
// killough 7/20/98: kill friendly monsters only if no others to kill
// haleyjd 01/10/02: reformatted some code
//
static int M_NukeMonsters()
{   
   int killcount = 0;
   Thinker *th = &thinkercap;
   int mask = MF_FRIEND;
      
   do
   {
      while((th = th->next) != &thinkercap)
      {
         Mobj *mo;     // haleyjd: use pointer to clean up code
         mobjinfo_t *mi;

         if(!(mo = thinker_cast<Mobj *>(th)))
            continue;

         mi = mobjinfo[mo->type];

         if(!(mo->flags & mask) && // killough 7/20/98
            (mo->flags & MF_COUNTKILL || mo->flags3 & MF3_KILLABLE))
         {
            // killough 3/6/98: kill even if PE is dead
            if(mo->health > 0)
            {
               killcount++;
               P_DamageMobj(mo, NULL, NULL, 10000, MOD_UNKNOWN);
            }

            // haleyjd: made behavior customizable
            if(mi->nukespec)
            {
               actionargs_t actionargs;

               actionargs.actiontype = actionargs_t::MOBJFRAME;
               actionargs.actor      = mo;
               actionargs.args       = NULL;
               actionargs.pspr       = NULL;

               mi->nukespec(&actionargs);
            }
         }
      }
   }
   while(!killcount && mask ? mask = 0, 1 : 0);  // killough 7/20/98

   return killcount;
}

CONSOLE_NETCMD(nuke, cf_server|cf_level, netcmd_nuke)
{
   int kills = M_NukeMonsters();

   // killough 3/22/98: make more intelligent about plural
   // Ty 03/27/98 - string(s) *not* externalized
   doom_printf("%d Monster%s Killed", kills,  (kills == 1) ? "" : "s");
}

//
// TODO: Perhaps make it so you can run cheats in console by name, like idkfa and such?
// It would be useful for users who want to bind cheats to a single button.
//
CONSOLE_COMMAND(GIVEARSENAL, cf_notnet|cf_level)
{
   switch(GameModeInfo->type)
   {
   case Game_DOOM:
      cheat_fa(nullptr);
      break;
   case Game_Heretic:
      cheat_rambo(nullptr);
      break;
   default:
      // TODO: I dunno
      break;
   }
}

CONSOLE_COMMAND(GIVEKEYS, cf_notnet|cf_level)
{
   cheat_k(nullptr);
}

//----------------------------------------------------------------------------
//
// $Log: m_cheat.c,v $
// Revision 1.7  1998/05/12  12:47:00  phares
// Removed OVER UNDER code
//
// Revision 1.6  1998/05/07  01:08:11  killough
// Make TNTAMMO ammo ordering more natural
//
// Revision 1.5  1998/05/03  22:10:53  killough
// Cheat engine, moved from st_stuff
//
// Revision 1.4  1998/05/01  14:38:06  killough
// beautification
//
// Revision 1.3  1998/02/09  03:03:05  killough
// Rendered obsolete by st_stuff.c
//
// Revision 1.2  1998/01/26  19:23:44  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:58  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
