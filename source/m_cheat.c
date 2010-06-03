// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#include "doomstat.h"
#include "m_cheat.h"

#include "c_net.h"
#include "c_runcmd.h"
#include "d_deh.h"  // Ty 03/27/98 - externalized strings
#include "d_gi.h"
#include "d_io.h" // SoM 3/14/2002: strncasecmp
#include "dstrings.h"
#include "e_states.h"
#include "g_game.h"
#include "m_argv.h"
#include "p_inter.h"
#include "p_setup.h"
#include "r_data.h"
#include "s_sound.h"
#include "sounds.h"
#include "w_wad.h"

#define plyr (&players[consoleplayer])     /* the console player */

//=============================================================================
//
// CHEAT SEQUENCE PACKAGE
//

static void cheat_mus(const void *);
static void cheat_choppers(const void *);
static void cheat_god(const void *);
static void cheat_infammo(const void *);
static void cheat_fa(const void *);
static void cheat_k(const void *);
static void cheat_kfa(const void *);
static void cheat_noclip(const void *);
static void cheat_pw(const void *);
static void cheat_behold(const void *);
static void cheat_clev(const void *);
static void cheat_mypos(const void *);
static void cheat_comp(const void *);
static void cheat_friction(const void *);
static void cheat_pushers(const void *);
static void cheat_tran(const void *);
static void cheat_massacre(const void *);
static void cheat_ddt(const void *);
static void cheat_hom(const void *);
static void cheat_key(const void *);
static void cheat_keyx(const void *);
static void cheat_keyxx(const void *);
static void cheat_weap(const void *);
static void cheat_weapx(const void *);
static void cheat_ammo(const void *);
static void cheat_ammox(const void *);
static void cheat_nuke(const void *);
static void cheat_one(const void *);

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
// {always, not_dm, not_coop, not_net, not_menu, not_demo, not_deh, beta_only},
// which excludes the cheat during certain modes of play.
//
// The fourth argument is the handler function.
//
// The fifth argument is passed to the handler function if it's non-negative;
// if negative, then its negative indicates the number of extra characters
// expected after the cheat code, which are passed to the handler function
// via a pointer to a buffer (after folding any letters to lowercase).
//

struct cheat_s cheat[] = {
  {"idmus",      "Change music",      always,
   cheat_mus,      -2},

  {"idchoppers", "Chainsaw",          not_net | not_demo,
   cheat_choppers },

  {"iddqd",      "God mode",          not_net | not_demo,
   cheat_god      },

  {"idk",        NULL,                not_net | not_demo | not_deh,
   cheat_k },  // The most controversial cheat code in Doom history!!!

  {"idkfa",      "Ammo & Keys",       not_net | not_demo,
   cheat_kfa },

  {"idfa",       "Ammo",              not_net | not_demo,
   cheat_fa  },

  {"idspispopd", "No Clipping 1",     not_net | not_demo,
   cheat_noclip },

  {"idclip",     "No Clipping 2",     not_net | not_demo,
   cheat_noclip },

  {"idbeholdv",  "Invincibility",     not_net | not_demo,
   cheat_pw,  pw_invulnerability },

  {"idbeholds",  "Berserk",           not_net | not_demo,
   cheat_pw,  pw_strength        },

  {"idbeholdi",  "Invisibility",      not_net | not_demo,  
   cheat_pw,  pw_invisibility    },

  {"idbeholdr",  "Radiation Suit",    not_net | not_demo,
   cheat_pw,  pw_ironfeet        },

  {"idbeholda",  "Auto-map",          not_net | not_demo,
   cheat_pw,  pw_allmap          },

  {"idbeholdl",  "Lite-Amp Goggles",  not_net | not_demo,
   cheat_pw,  pw_infrared        },

  {"idbehold",   "BEHOLD menu",       not_net | not_demo,
   cheat_behold   },

  {"idclev",     "Level Warp",        not_net | not_demo | not_menu,
   cheat_clev,    -2},

  {"idmypos",    "Player Position",   not_net | not_demo,
   cheat_mypos    },

  {"comp",    NULL,                   not_net | not_demo,
   cheat_comp     },     // phares

  {"killem",     NULL,                not_net | not_demo,
   cheat_massacre },     // jff 2/01/98 kill all monsters

  {"iddt",       "Map cheat",         not_dm,
   cheat_ddt      },     // killough 2/07/98: moved from am_map.c

  {"hom",     NULL,                   always,
   cheat_hom      },     // killough 2/07/98: HOM autodetector

  {"key",     NULL,                   not_net | not_demo, 
   cheat_key   },     // killough 2/16/98: generalized key cheats

  {"keyr",    NULL,                   not_net | not_demo,
   cheat_keyx  },

  {"keyy",    NULL,                   not_net | not_demo,
   cheat_keyx  },

  {"keyb",    NULL,                   not_net | not_demo,
   cheat_keyx  },

  {"keyrc",   NULL,                   not_net | not_demo, 
   cheat_keyxx, it_redcard    },

  {"keyyc",   NULL,                   not_net | not_demo,
   cheat_keyxx, it_yellowcard },

  {"keybc",   NULL,                   not_net | not_demo, 
   cheat_keyxx, it_bluecard   },

  {"keyrs",   NULL,                   not_net | not_demo,
   cheat_keyxx, it_redskull   },

  {"keyys",   NULL,                   not_net | not_demo,
   cheat_keyxx, it_yellowskull},

  {"keybs",   NULL,                   not_net | not_demo,
   cheat_keyxx, it_blueskull  },  // killough 2/16/98: end generalized keys

  {"weap",    NULL,                   not_net | not_demo,
   cheat_weap  },     // killough 2/16/98: generalized weapon cheats

  {"weap",    NULL,                   not_net | not_demo,
   cheat_weapx, -1},

  {"ammo",    NULL,                   not_net | not_demo,
   cheat_ammo  },

  {"ammo",    NULL,                   not_net | not_demo,
   cheat_ammox, -1},  // killough 2/16/98: end generalized weapons

  {"tran",    NULL,                   always,
   cheat_tran  },     // invoke translucency         // phares

  {"ice",     NULL,                   not_net | not_demo,
   cheat_friction   },   // phares 3/10/98: toggle variable friction effects

  {"push",    NULL,                   not_net | not_demo, 
   cheat_pushers    },   // phares 3/10/98: toggle pushers

  {"nuke",    NULL,                   not_net | not_demo,
   cheat_nuke       },   // killough 12/98: disable nukage damage

#ifdef INSTRUMENTED
  {"stat",       NULL,                always,
   cheat_printstats},
#endif

  // haleyjd: total invis cheat -- hideme
  {"hideme", NULL,      not_net | not_demo,
    cheat_pw, pw_totalinvis     },

  // haleyjd: heretic ghost 
  {"ghost",  NULL,      not_net | not_demo,
    cheat_pw, pw_ghost },

  {"infshots", NULL,    not_net | not_demo,
    cheat_infammo },

  {"silence", NULL,     not_net | not_demo,
    cheat_pw, pw_silencer },

  {"iamtheone", NULL,   not_net | not_demo,
    cheat_one },

  {NULL}                 // end-of-list marker
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
   int musnum;
   const char *buf = (const char *)arg;
   
   //jff 3/20/98 note: this cheat allowed in netgame/demorecord
   
   //jff 3/17/98 avoid musnum being negative and crashing
   if(!isdigit(buf[0]) || !isdigit(buf[1]))
      return;

   doom_printf("%s", DEH_String("STSTR_MUS")); // Ty 03/27/98 - externalized
  
   if(GameModeInfo->id == commercial)
   {
      musnum = mus_runnin + (buf[0]-'0')*10 + buf[1]-'0' - 1;
          
      //jff 4/11/98 prevent IDMUS00 in DOOMII and IDMUS36 or greater
      if(musnum < mus_runnin ||  ((buf[0]-'0')*10 + buf[1]-'0') > 35)
         doom_printf("%s", DEH_String("STSTR_NOMUS")); // Ty 03/27/98 - externalized
      else
      {
         S_ChangeMusicNum(musnum, 1);
         idmusnum = musnum; //jff 3/17/98 remember idmus number for restore
      }
   }
   else if(GameModeInfo->type == Game_Heretic)
   {
      // haleyjd 03/10/03: heretic support
      // use H_Mus_Matrix for easy access
      int episodenum = (buf[0] - '0') - 1;
      int mapnum     = (buf[1] - '0') - 1;

      if(episodenum < 0 || episodenum > 5 || mapnum < 0 || mapnum > 8)
         doom_printf("%s", DEH_String("STSTR_NOMUS"));
      else
      {
         musnum = H_Mus_Matrix[episodenum][mapnum];
         S_ChangeMusicNum(musnum, 1);
         idmusnum = musnum;
      }
   }
   else
   {
      musnum = mus_e1m1 + (buf[0]-'1')*9 + (buf[1]-'1');
          
      //jff 4/11/98 prevent IDMUS0x IDMUSx0 in DOOMI and greater than introa
      if(buf[0] < '1' || buf[1] < '1' || ((buf[0]-'1')*9 + buf[1]-'1') > 31)
         doom_printf("%s", DEH_String("STSTR_NOMUS")); // Ty 03/27/98 - externalized
      else
      {
         S_ChangeMusicNum(musnum, 1);
         idmusnum = musnum; //jff 3/17/98 remember idmus number for restore
      }
   }
}

// 'choppers' invulnerability & chainsaw
static void cheat_choppers(const void *arg)
{
   // WEAPON_FIXME: choppers cheat
   plyr->weaponowned[wp_chainsaw] = true;
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
   int i;
   
   if(!plyr->backpack)
   {
      for(i = 0; i < NUMAMMO; i++)
         plyr->maxammo[i] *= 2;
      plyr->backpack = true;
   }
   
   plyr->armorpoints = idfa_armor;      // Ty 03/09/98 - deh
   plyr->armortype = idfa_armor_class;  // Ty 03/09/98 - deh

   // WEAPON_FIXME: IDFA cheat
   
   // You can't own weapons that aren't in the game - phares 02/27/98
   for(i = 0; i < NUMWEAPONS; i++)
   {
      if(!(((i == wp_plasma || i == wp_bfg) && GameModeInfo->id == shareware) ||
         (i == wp_supershotgun && !enable_ssg)))
         plyr->weaponowned[i] = true;
   }
      
   for(i = 0; i < NUMAMMO; i++)
   {
      if(i != am_cell || GameModeInfo->id != shareware)
         plyr->ammo[i] = plyr->maxammo[i];
   }

   doom_printf("%s", DEH_String("STSTR_FAADDED"));
}

static void cheat_k(const void *arg)
{
   int i, k = 0;

   for(i = 0; i < NUMCARDS; i++)
   {
      if(!plyr->cards[i])     // only print message if at least one key added
      {                       // however, caller may overwrite message anyway
         plyr->cards[i] = true;
         k++; // sf: fix multiple 'keys added' messages
      }
   }

   if(k)
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
      P_GivePower(plyr, pw);
      if(pw != pw_strength && !comp[comp_infcheat])
         plyr->powers[pw] = -1;      // infinite duration -- killough
   }
   
   doom_printf("%s", DEH_String("STSTR_BEHOLDX")); // Ty 03/27/98 - externalized
}

// 'behold' power-up menu
static void cheat_behold(const void *arg)
{
   doom_printf("%s", DEH_String("STSTR_BEHOLD")); // Ty 03/27/98 - externalized
}

// 'clev' change-level cheat
static void cheat_clev(const void *arg)
{
   const char *buf = (const char *)arg;
   char mapname[9];
   int epsd, map, lumpnum;

   // return silently on nonsense input
   if(buf[0] < '0' || buf[0] > '9' ||
      buf[1] < '0' || buf[1] > '9')
      return;

   // haleyjd: build mapname
   memset(mapname, 0, sizeof(mapname));

   if(GameModeInfo->flags & GIF_MAPXY)
   {
      epsd = 1; //jff was 0, but espd is 1-based 
      map = (buf[0] - '0')*10 + buf[1] - '0';

      psnprintf(mapname, sizeof(mapname), "MAP%02d", map);
   }
   else
   {
      epsd = buf[0] - '0';
      map = buf[1] - '0';

      psnprintf(mapname, sizeof(mapname), "E%dM%d", epsd, map);
   }

   // haleyjd: check mapname for existence and validity as a map
   lumpnum = W_CheckNumForName(mapname);

   if(lumpnum == -1 || P_CheckLevel(lumpnum) == LEVEL_FORMAT_INVALID)
   {
      doom_printf("%s not found or is not a valid map", mapname);
      return;
   }
   
   // So be it.

   idmusnum = -1; //jff 3/17/98 revert to normal level music on IDCLEV

   doom_printf("%s", DEH_String("STSTR_CLEV")); // Ty 03/27/98 - externalized

   G_DeferedInitNewNum(gameskill, epsd, map);
}

// 'mypos' for player position
// killough 2/7/98: simplified using doom_printf and made output more user-friendly
static void cheat_mypos(const void *arg)
{
   doom_printf("Position (%d,%d,%d)\tAngle %-.0f", 
               players[consoleplayer].mo->x >> FRACBITS,
               players[consoleplayer].mo->y >> FRACBITS,
               players[consoleplayer].mo->z >> FRACBITS,
               players[consoleplayer].mo->angle * (90.0/ANG90));
}

// compatibility cheat
static void cheat_comp(const void *arg)
{
   int i;
   
   // Ty 03/27/98 - externalized
   doom_printf("%s",
      DEH_String((compatibility = !compatibility) ? "STSTR_COMPON"
                                                  : "STSTR_COMPOFF"));

   for(i = 0; i < COMP_TOTAL; ++i) // killough 10/98: reset entire vector
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
   extern boolean automapactive;
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

   doom_printf((plyr->cards[key] = !plyr->cards[key]) ? 
     "Key Added" : "Key Removed");  // Ty 03/27/98 - *not* externalized
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

   // WEAPON_FIXME: weap cheat

   if((w == wp_supershotgun && !enable_ssg) ||      // killough 2/28/98
      ((w == wp_bfg || w == wp_plasma) && GameModeInfo->id == shareware))
      return;

   if(w == wp_fist)           // make '1' apply beserker strength toggle
      cheat_pw(&pwstr);
   else
   {
      if(w >= 0 && w < NUMWEAPONS)
      {
         if((plyr->weaponowned[w] = !plyr->weaponowned[w]))
            doom_printf("Weapon Added");  // Ty 03/27/98 - *not* externalized
         else 
         {
            int P_SwitchWeapon(player_t *player);
            
            doom_printf("Weapon Removed"); // Ty 03/27/98 - *not* externalized
            if(w == plyr->readyweapon)     // maybe switch if weapon removed
               plyr->pendingweapon = P_SwitchWeapon(plyr);
         }
      }
   }
}

// killough 2/16/98: generalized ammo cheats
static void cheat_ammo(const void *arg)
{
   doom_printf("Ammo 1-4, Backpack");  // Ty 03/27/98 - *not* externalized
}

static void cheat_ammox(const void *arg)
{
   const char *buf = (const char *)arg;
   int a = *buf - '1';

   if(*buf == 'b')
   {
      if((plyr->backpack = !plyr->backpack))
      {
         doom_printf("Backpack Added");
         
         for(a = 0; a < NUMAMMO; ++a)
            plyr->maxammo[a] <<= 1;
      }
      else
      {
         doom_printf("Backpack removed");

         for(a = 0; a < NUMAMMO; ++a)
         {
            if(plyr->ammo[a] > (plyr->maxammo[a] >>= 1))
               plyr->ammo[a] = plyr->maxammo[a];
         }
      }
   }
   else if(a >= 0 && a < NUMAMMO)
   { 
      // killough 5/5/98: switch plasma and rockets for now -- KLUDGE 
      a = (a == am_cell ? am_misl : a == am_misl ? am_cell : a);  // HACK

      // haleyjd 10/24/09: altered behavior:
      // * Add ammo if ammo is not at maxammo.
      // * Remove ammo otherwise.
      if(plyr->ammo[a] != plyr->maxammo[a])
      {
         plyr->ammo[a] = plyr->maxammo[a];
         doom_printf("Ammo Added");
      }
      else
      {
         plyr->ammo[a] = 0;
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

//-----------------------------------------------------------------------------
// 2/7/98: Cheat detection rewritten by Lee Killough, to avoid
// scrambling and to use a more general table-driven approach.
//-----------------------------------------------------------------------------

#define CHEAT_ARGS_MAX 8  /* Maximum number of args at end of cheats */

void M_DoCheat(char *s)
{
   while(*s)
   {
      M_FindCheats(*s);
      s++;
   }
}

boolean M_FindCheats(int key)
{
   static uint64_t sr;
   static char argbuf[CHEAT_ARGS_MAX+1], *arg;
   static int init, argsleft, cht;
   int i, ret, matchedbefore;

   // If we are expecting arguments to a cheat
   // (e.g. idclev), put them in the arg buffer

   if(argsleft)
   {
      *arg++ = tolower(key);             // store key in arg buffer
      if(!--argsleft)                    // if last key in arg list,
         cheat[cht].func(argbuf);        // process the arg buffer
      return 1;                          // affirmative response
   }

   key = tolower(key) - 'a';
   if(key < 0 || key >= 32)              // ignore most non-alpha cheat letters
   {
      sr = 0;        // clear shift register
      return 0;
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
            unsigned key = tolower(*p) - 'a'; // convert to 0-31

            if(key >= 32)             // ignore most non-alpha cheat letters
               continue;

            c = (c << 5) + key;       // shift key into code
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

   for(matchedbefore = ret = i = 0; cheat[i].cheat; i++)
   {
      if((sr & cheat[i].mask) == cheat[i].code &&  // if match found & allowed
         !(cheat[i].when & not_dm   && netgame && GameType == gt_dm && !demoplayback) &&
         !(cheat[i].when & not_coop && netgame && GameType == gt_coop) &&
         !(cheat[i].when & not_demo && (demorecording || demoplayback)) &&
         !(cheat[i].when & not_menu && menuactive) &&
         !(cheat[i].when & not_deh  && cheat[i].deh_modified))
      {
         if(cheat[i].arg < 0)               // if additional args are required
         {
            cht = i;                        // remember this cheat code
            arg = argbuf;                   // point to start of arg buffer
            argsleft = -cheat[i].arg;       // number of args expected
            ret = 1;                        // responder has eaten key
         }
         else if(!matchedbefore)            // allow only one cheat at a time 
         {
            matchedbefore = ret = 1;        // responder has eaten key
            cheat[i].func(&(cheat[i].arg)); // call cheat handler
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
      sscanf(Console.argv[0], "%i", &value);
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
      sscanf(Console.argv[0], "%i", &value);
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
      sscanf(Console.argv[0], "%i", &value);
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

extern void A_Fall(mobj_t *);
extern void A_PainDie(mobj_t *);

// haleyjd 07/13/03: special actions for killem cheat

//
// Pain Elemental -- spawn lost souls now so they get killed
//
void A_PainNukeSpec(mobj_t *actor)
{
   A_PainDie(actor);  // killough 2/8/98
   P_SetMobjState(actor, E_SafeState(S_PAIN_DIE6));
}

//
// D'Sparil (first form) -- don't spawn second form
//
void A_SorcNukeSpec(mobj_t *actor)
{
   A_Fall(actor);
   P_SetMobjStateNF(actor, E_SafeState(S_SRCR1_DIE17));
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
static void M_NukeMonsters(void)
{   
   int killcount = 0;
   thinker_t *currentthinker = &thinkercap;
   int mask = MF_FRIEND;
      
   do
   {
      while((currentthinker = currentthinker->next) != &thinkercap)
      {
         mobj_t *mo;     // haleyjd: use pointer to clean up code
         mobjinfo_t *mi;

         if(currentthinker->function != P_MobjThinker)
            continue;

         mo = (mobj_t *)currentthinker;
         mi = &mobjinfo[mo->type];

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
               mi->nukespec(mo);
         }
      }
   }
   while(!killcount && mask ? mask = 0, 1 : 0);  // killough 7/20/98
   
   // killough 3/22/98: make more intelligent about plural
   // Ty 03/27/98 - string(s) *not* externalized
   doom_printf("%d Monster%s Killed", killcount,  (killcount == 1) ? "" : "s");
}

CONSOLE_NETCMD(nuke, cf_server|cf_level, netcmd_nuke)
{
   M_NukeMonsters();
}

void Cheat_AddCommands()
{
   C_AddCommand(infammo);
   C_AddCommand(god);
   C_AddCommand(noclip);
   C_AddCommand(nuke);
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
