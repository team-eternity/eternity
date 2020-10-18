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
// DeHackEd information tables / hashing
//
// Separated out from d_deh.c to avoid clutter and speed up the
// editing of that file. Routines for internal table hash chaining
// and initialization are also here.
//
//--------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "d_dehtbl.h"
#include "d_io.h"
#include "d_mod.h"
#include "doomdef.h"
#include "doomtype.h"
#include "dhticstr.h"  // haleyjd
#include "dstrings.h"  // to get initial text values
#include "e_lib.h"
#include "info.h"
#include "m_argv.h"
#include "m_fixed.h"
#include "m_queue.h"
#include "sounds.h"

//
// Text Replacement
//

// #include "d_deh.h" -- we don't do that here but we declare the
// variables.  This externalizes everything that there is a string
// set for in the language files.  See d_deh.h for detailed comments,
// original English values etc.  These are set to the macro values,
// which are set by D_ENGLSH.H or D_FRENCH.H(etc).  BEX files are a
// better way of changing these strings globally by language.

// ============================================================
// Any of these can be changed using the bex extensions

static const char *s_D_DEVSTR    = D_DEVSTR;
static const char *s_D_CDROM     = D_CDROM;
static const char *s_PRESSKEY    = PRESSKEY;
static const char *s_PRESSYN     = PRESSYN;
static const char *s_QUITMSG     = "";        // sf: optional quitmsg replacement
static const char *s_LOADNET     = LOADNET;   // PRESSKEY; // killough 4/4/98:
static const char *s_QLOADNET    = QLOADNET;  // PRESSKEY;
static const char *s_QSAVESPOT   = QSAVESPOT; // PRESSKEY;
static const char *s_SAVEDEAD    = SAVEDEAD;  // PRESSKEY; // remove duplicate y/n
const char *s_QSPROMPT    = QSPROMPT;  // PRESSYN;
const char *s_QLPROMPT    = QLPROMPT;  // PRESSYN;
static const char *s_NEWGAME     = NEWGAME;   // PRESSKEY;
static const char *s_NIGHTMARE   = NIGHTMARE; // PRESSYN;
static const char *s_SWSTRING    = SWSTRING;  // PRESSKEY;
static const char *s_MSGOFF      = MSGOFF;
static const char *s_MSGON       = MSGON;
static const char *s_NETEND      = NETEND;    // PRESSKEY;
static const char *s_ENDGAME     = ENDGAME;   // PRESSYN; // killough 4/4/98: end
static const char *s_DOSY        = DOSY;
// joel - detail level strings ditched
static const char *s_GAMMALVL0   = GAMMALVL0;
static const char *s_GAMMALVL1   = GAMMALVL1;
static const char *s_GAMMALVL2   = GAMMALVL2;
static const char *s_GAMMALVL3   = GAMMALVL3;
static const char *s_GAMMALVL4   = GAMMALVL4;
static const char *s_EMPTYSTRING = EMPTYSTRING;
static const char *s_GOTARMOR    = GOTARMOR;
static const char *s_GOTMEGA     = GOTMEGA;
static const char *s_GOTHTHBONUS = GOTHTHBONUS;
static const char *s_GOTARMBONUS = GOTARMBONUS;
static const char *s_GOTSTIM     = GOTSTIM;
static const char *s_GOTMEDINEED = GOTMEDINEED;
static const char *s_GOTMEDIKIT  = GOTMEDIKIT;
static const char *s_GOTSUPER    = GOTSUPER;
static const char *s_GOTBLUECARD = GOTBLUECARD;
static const char *s_GOTYELWCARD = GOTYELWCARD;
static const char *s_GOTREDCARD  = GOTREDCARD;
static const char *s_GOTBLUESKUL = GOTBLUESKUL;
static const char *s_GOTYELWSKUL = GOTYELWSKUL;
static const char *s_GOTREDSKULL = GOTREDSKULL;
static const char *s_GOTINVUL    = GOTINVUL;
static const char *s_GOTBERSERK  = GOTBERSERK;
static const char *s_GOTINVIS    = GOTINVIS;
static const char *s_GOTSUIT     = GOTSUIT;
static const char *s_GOTMAP      = GOTMAP;
static const char *s_GOTVISOR    = GOTVISOR;
static const char *s_GOTMSPHERE  = GOTMSPHERE;
static const char *s_GOTCLIP     = GOTCLIP;
static const char *s_GOTCLIPBOX  = GOTCLIPBOX;
static const char *s_GOTROCKET   = GOTROCKET;
static const char *s_GOTROCKBOX  = GOTROCKBOX;
static const char *s_GOTCELL     = GOTCELL;
static const char *s_GOTCELLBOX  = GOTCELLBOX;
static const char *s_GOTSHELLS   = GOTSHELLS;
static const char *s_GOTSHELLBOX = GOTSHELLBOX;
static const char *s_GOTBACKPACK = GOTBACKPACK;
static const char *s_GOTBFG9000  = GOTBFG9000;
static const char *s_GOTCHAINGUN = GOTCHAINGUN;
static const char *s_GOTCHAINSAW = GOTCHAINSAW;
static const char *s_GOTLAUNCHER = GOTLAUNCHER;
static const char *s_GOTPLASMA   = GOTPLASMA;
static const char *s_GOTSHOTGUN  = GOTSHOTGUN;
static const char *s_GOTSHOTGUN2 = GOTSHOTGUN2;
static const char *s_BETA_BONUS3 = BETA_BONUS3;
static const char *s_BETA_BONUS4 = BETA_BONUS4;
static const char *s_PD_BLUEO    = PD_BLUEO;
static const char *s_PD_REDO     = PD_REDO;
static const char *s_PD_YELLOWO  = PD_YELLOWO;
static const char *s_PD_BLUEK    = PD_BLUEK;
static const char *s_PD_REDK     = PD_REDK;
static const char *s_PD_YELLOWK  = PD_YELLOWK;
static const char *s_PD_BLUEC    = PD_BLUEC;
static const char *s_PD_REDC     = PD_REDC;
static const char *s_PD_YELLOWC  = PD_YELLOWC;
static const char *s_PD_BLUES    = PD_BLUES;
static const char *s_PD_REDS     = PD_REDS;
static const char *s_PD_YELLOWS  = PD_YELLOWS;
static const char *s_PD_ANY      = PD_ANY;
static const char *s_PD_ALL3     = PD_ALL3;
static const char *s_PD_ALL6     = PD_ALL6;
static const char *s_GGSAVED     = GGSAVED;
static const char *s_HUSTR_MSGU  = HUSTR_MSGU;
static const char *s_HUSTR_E1M1  = HUSTR_E1M1;
static const char *s_HUSTR_E1M2  = HUSTR_E1M2;
static const char *s_HUSTR_E1M3  = HUSTR_E1M3;
static const char *s_HUSTR_E1M4  = HUSTR_E1M4;
static const char *s_HUSTR_E1M5  = HUSTR_E1M5;
static const char *s_HUSTR_E1M6  = HUSTR_E1M6;
static const char *s_HUSTR_E1M7  = HUSTR_E1M7;
static const char *s_HUSTR_E1M8  = HUSTR_E1M8;
static const char *s_HUSTR_E1M9  = HUSTR_E1M9;
static const char *s_HUSTR_E2M1  = HUSTR_E2M1;
static const char *s_HUSTR_E2M2  = HUSTR_E2M2;
static const char *s_HUSTR_E2M3  = HUSTR_E2M3;
static const char *s_HUSTR_E2M4  = HUSTR_E2M4;
static const char *s_HUSTR_E2M5  = HUSTR_E2M5;
static const char *s_HUSTR_E2M6  = HUSTR_E2M6;
static const char *s_HUSTR_E2M7  = HUSTR_E2M7;
static const char *s_HUSTR_E2M8  = HUSTR_E2M8;
static const char *s_HUSTR_E2M9  = HUSTR_E2M9;
static const char *s_HUSTR_E3M1  = HUSTR_E3M1;
static const char *s_HUSTR_E3M2  = HUSTR_E3M2;
static const char *s_HUSTR_E3M3  = HUSTR_E3M3;
static const char *s_HUSTR_E3M4  = HUSTR_E3M4;
static const char *s_HUSTR_E3M5  = HUSTR_E3M5;
static const char *s_HUSTR_E3M6  = HUSTR_E3M6;
static const char *s_HUSTR_E3M7  = HUSTR_E3M7;
static const char *s_HUSTR_E3M8  = HUSTR_E3M8;
static const char *s_HUSTR_E3M9  = HUSTR_E3M9;
static const char *s_HUSTR_E4M1  = HUSTR_E4M1;
static const char *s_HUSTR_E4M2  = HUSTR_E4M2;
static const char *s_HUSTR_E4M3  = HUSTR_E4M3;
static const char *s_HUSTR_E4M4  = HUSTR_E4M4;
static const char *s_HUSTR_E4M5  = HUSTR_E4M5;
static const char *s_HUSTR_E4M6  = HUSTR_E4M6;
static const char *s_HUSTR_E4M7  = HUSTR_E4M7;
static const char *s_HUSTR_E4M8  = HUSTR_E4M8;
static const char *s_HUSTR_E4M9  = HUSTR_E4M9;
static const char *s_HUSTR_1     = HUSTR_1;
static const char *s_HUSTR_2     = HUSTR_2;
static const char *s_HUSTR_3     = HUSTR_3;
static const char *s_HUSTR_4     = HUSTR_4;
static const char *s_HUSTR_5     = HUSTR_5;
static const char *s_HUSTR_6     = HUSTR_6;
static const char *s_HUSTR_7     = HUSTR_7;
static const char *s_HUSTR_8     = HUSTR_8;
static const char *s_HUSTR_9     = HUSTR_9;
static const char *s_HUSTR_10    = HUSTR_10;
static const char *s_HUSTR_11    = HUSTR_11;
static const char *s_HUSTR_12    = HUSTR_12;
static const char *s_HUSTR_13    = HUSTR_13;
static const char *s_HUSTR_14    = HUSTR_14;
static const char *s_HUSTR_15    = HUSTR_15;
static const char *s_HUSTR_16    = HUSTR_16;
static const char *s_HUSTR_17    = HUSTR_17;
static const char *s_HUSTR_18    = HUSTR_18;
static const char *s_HUSTR_19    = HUSTR_19;
static const char *s_HUSTR_20    = HUSTR_20;
static const char *s_HUSTR_21    = HUSTR_21;
static const char *s_HUSTR_22    = HUSTR_22;
static const char *s_HUSTR_23    = HUSTR_23;
static const char *s_HUSTR_24    = HUSTR_24;
static const char *s_HUSTR_25    = HUSTR_25;
static const char *s_HUSTR_26    = HUSTR_26;
static const char *s_HUSTR_27    = HUSTR_27;
static const char *s_HUSTR_28    = HUSTR_28;
static const char *s_HUSTR_29    = HUSTR_29;
static const char *s_HUSTR_30    = HUSTR_30;
static const char *s_HUSTR_31    = HUSTR_31;
static const char *s_HUSTR_32    = HUSTR_32;
static const char *s_HUSTR_33    = HUSTR_33;
static const char *s_PHUSTR_1    = PHUSTR_1;
static const char *s_PHUSTR_2    = PHUSTR_2;
static const char *s_PHUSTR_3    = PHUSTR_3;
static const char *s_PHUSTR_4    = PHUSTR_4;
static const char *s_PHUSTR_5    = PHUSTR_5;
static const char *s_PHUSTR_6    = PHUSTR_6;
static const char *s_PHUSTR_7    = PHUSTR_7;
static const char *s_PHUSTR_8    = PHUSTR_8;
static const char *s_PHUSTR_9    = PHUSTR_9;
static const char *s_PHUSTR_10   = PHUSTR_10;
static const char *s_PHUSTR_11   = PHUSTR_11;
static const char *s_PHUSTR_12   = PHUSTR_12;
static const char *s_PHUSTR_13   = PHUSTR_13;
static const char *s_PHUSTR_14   = PHUSTR_14;
static const char *s_PHUSTR_15   = PHUSTR_15;
static const char *s_PHUSTR_16   = PHUSTR_16;
static const char *s_PHUSTR_17   = PHUSTR_17;
static const char *s_PHUSTR_18   = PHUSTR_18;
static const char *s_PHUSTR_19   = PHUSTR_19;
static const char *s_PHUSTR_20   = PHUSTR_20;
static const char *s_PHUSTR_21   = PHUSTR_21;
static const char *s_PHUSTR_22   = PHUSTR_22;
static const char *s_PHUSTR_23   = PHUSTR_23;
static const char *s_PHUSTR_24   = PHUSTR_24;
static const char *s_PHUSTR_25   = PHUSTR_25;
static const char *s_PHUSTR_26   = PHUSTR_26;
static const char *s_PHUSTR_27   = PHUSTR_27;
static const char *s_PHUSTR_28   = PHUSTR_28;
static const char *s_PHUSTR_29   = PHUSTR_29;
static const char *s_PHUSTR_30   = PHUSTR_30;
static const char *s_PHUSTR_31   = PHUSTR_31;
static const char *s_PHUSTR_32   = PHUSTR_32;
static const char *s_THUSTR_1    = THUSTR_1;
static const char *s_THUSTR_2    = THUSTR_2;
static const char *s_THUSTR_3    = THUSTR_3;
static const char *s_THUSTR_4    = THUSTR_4;
static const char *s_THUSTR_5    = THUSTR_5;
static const char *s_THUSTR_6    = THUSTR_6;
static const char *s_THUSTR_7    = THUSTR_7;
static const char *s_THUSTR_8    = THUSTR_8;
static const char *s_THUSTR_9    = THUSTR_9;
static const char *s_THUSTR_10   = THUSTR_10;
static const char *s_THUSTR_11   = THUSTR_11;
static const char *s_THUSTR_12   = THUSTR_12;
static const char *s_THUSTR_13   = THUSTR_13;
static const char *s_THUSTR_14   = THUSTR_14;
static const char *s_THUSTR_15   = THUSTR_15;
static const char *s_THUSTR_16   = THUSTR_16;
static const char *s_THUSTR_17   = THUSTR_17;
static const char *s_THUSTR_18   = THUSTR_18;
static const char *s_THUSTR_19   = THUSTR_19;
static const char *s_THUSTR_20   = THUSTR_20;
static const char *s_THUSTR_21   = THUSTR_21;
static const char *s_THUSTR_22   = THUSTR_22;
static const char *s_THUSTR_23   = THUSTR_23;
static const char *s_THUSTR_24   = THUSTR_24;
static const char *s_THUSTR_25   = THUSTR_25;
static const char *s_THUSTR_26   = THUSTR_26;
static const char *s_THUSTR_27   = THUSTR_27;
static const char *s_THUSTR_28   = THUSTR_28;
static const char *s_THUSTR_29   = THUSTR_29;
static const char *s_THUSTR_30   = THUSTR_30;
static const char *s_THUSTR_31   = THUSTR_31;
static const char *s_THUSTR_32   = THUSTR_32;
static const char *s_HUSTR_CHATMACRO1   = HUSTR_CHATMACRO1;
static const char *s_HUSTR_CHATMACRO2   = HUSTR_CHATMACRO2;
static const char *s_HUSTR_CHATMACRO3   = HUSTR_CHATMACRO3;
static const char *s_HUSTR_CHATMACRO4   = HUSTR_CHATMACRO4;
static const char *s_HUSTR_CHATMACRO5   = HUSTR_CHATMACRO5;
static const char *s_HUSTR_CHATMACRO6   = HUSTR_CHATMACRO6;
static const char *s_HUSTR_CHATMACRO7   = HUSTR_CHATMACRO7;
static const char *s_HUSTR_CHATMACRO8   = HUSTR_CHATMACRO8;
static const char *s_HUSTR_CHATMACRO9   = HUSTR_CHATMACRO9;
static const char *s_HUSTR_CHATMACRO0   = HUSTR_CHATMACRO0;
static const char *s_HUSTR_TALKTOSELF1  = HUSTR_TALKTOSELF1;
static const char *s_HUSTR_TALKTOSELF2  = HUSTR_TALKTOSELF2;
static const char *s_HUSTR_TALKTOSELF3  = HUSTR_TALKTOSELF3;
static const char *s_HUSTR_TALKTOSELF4  = HUSTR_TALKTOSELF4;
static const char *s_HUSTR_TALKTOSELF5  = HUSTR_TALKTOSELF5;
static const char *s_HUSTR_MESSAGESENT  = HUSTR_MESSAGESENT;
static const char *s_HUSTR_PLRGREEN     = HUSTR_PLRGREEN;
static const char *s_HUSTR_PLRINDIGO    = HUSTR_PLRINDIGO;
static const char *s_HUSTR_PLRBROWN     = HUSTR_PLRBROWN;
static const char *s_HUSTR_PLRRED       = HUSTR_PLRRED;
static const char *s_AMSTR_FOLLOWON     = AMSTR_FOLLOWON;
static const char *s_AMSTR_FOLLOWOFF    = AMSTR_FOLLOWOFF;
static const char *s_AMSTR_GRIDON       = AMSTR_GRIDON;
static const char *s_AMSTR_GRIDOFF      = AMSTR_GRIDOFF;
static const char *s_AMSTR_MARKEDSPOT   = AMSTR_MARKEDSPOT;
static const char *s_AMSTR_MARKSCLEARED = AMSTR_MARKSCLEARED;
static const char *s_STSTR_MUS          = STSTR_MUS;
static const char *s_STSTR_NOMUS        = STSTR_NOMUS;
static const char *s_STSTR_DQDON        = STSTR_DQDON;
static const char *s_STSTR_DQDOFF       = STSTR_DQDOFF;
static const char *s_STSTR_KFAADDED     = STSTR_KFAADDED;
static const char *s_STSTR_FAADDED      = STSTR_FAADDED;
static const char *s_STSTR_NCON         = STSTR_NCON;
static const char *s_STSTR_NCOFF        = STSTR_NCOFF;
static const char *s_STSTR_BEHOLD       = STSTR_BEHOLD;
static const char *s_STSTR_BEHOLDX      = STSTR_BEHOLDX;
static const char *s_STSTR_CHOPPERS     = STSTR_CHOPPERS;
static const char *s_STSTR_CLEV         = STSTR_CLEV;
static const char *s_STSTR_COMPON       = STSTR_COMPON;
static const char *s_STSTR_COMPOFF      = STSTR_COMPOFF;
static const char *s_E1TEXT     = E1TEXT;
static const char *s_E2TEXT     = E2TEXT;
static const char *s_E3TEXT     = E3TEXT;
static const char *s_E4TEXT     = E4TEXT;
static const char *s_C1TEXT     = C1TEXT;
static const char *s_C2TEXT     = C2TEXT;
static const char *s_C3TEXT     = C3TEXT;
static const char *s_C4TEXT     = C4TEXT;
static const char *s_C5TEXT     = C5TEXT;
static const char *s_C6TEXT     = C6TEXT;
static const char *s_P1TEXT     = P1TEXT;
static const char *s_P2TEXT     = P2TEXT;
static const char *s_P3TEXT     = P3TEXT;
static const char *s_P4TEXT     = P4TEXT;
static const char *s_P5TEXT     = P5TEXT;
static const char *s_P6TEXT     = P6TEXT;
static const char *s_T1TEXT     = T1TEXT;
static const char *s_T2TEXT     = T2TEXT;
static const char *s_T3TEXT     = T3TEXT;
static const char *s_T4TEXT     = T4TEXT;
static const char *s_T5TEXT     = T5TEXT;
static const char *s_T6TEXT     = T6TEXT;
static const char *s_CC_ZOMBIE  = CC_ZOMBIE;
static const char *s_CC_SHOTGUN = CC_SHOTGUN;
static const char *s_CC_HEAVY   = CC_HEAVY;
static const char *s_CC_IMP     = CC_IMP;
static const char *s_CC_DEMON   = CC_DEMON;
static const char *s_CC_LOST    = CC_LOST;
static const char *s_CC_CACO    = CC_CACO;
static const char *s_CC_HELL    = CC_HELL;
static const char *s_CC_BARON   = CC_BARON;
static const char *s_CC_ARACH   = CC_ARACH;
static const char *s_CC_PAIN    = CC_PAIN;
static const char *s_CC_REVEN   = CC_REVEN;
static const char *s_CC_MANCU   = CC_MANCU;
static const char *s_CC_ARCH    = CC_ARCH;
static const char *s_CC_SPIDER  = CC_SPIDER;
static const char *s_CC_CYBER   = CC_CYBER;
static const char *s_CC_HERO    = CC_HERO;
static const char *s_CC_TITLE   = CC_TITLE;
// haleyjd 10/08/06: new demo sequence strings
static const char *s_TITLEPIC   = TITLEPIC; // for doom
static const char *s_TITLE      = TITLE;    // for heretic
static const char *s_DEMO1      = DEMO1;    // demos
static const char *s_DEMO2      = DEMO2;
static const char *s_DEMO3      = DEMO3;
static const char *s_DEMO4      = DEMO4;
static const char *s_CREDIT     = CREDIT;  // credit/order screens
static const char *s_ORDER      = ORDER;
static const char *s_HELP2      = HELP2;
// Ty 03/30/98 - new substitutions for background textures
static const char *bgflatE1     = "FLOOR4_8"; // end of DOOM Episode 1
static const char *bgflatE2     = "SFLR6_1";  // end of DOOM Episode 2
static const char *bgflatE3     = "MFLR8_4";  // end of DOOM Episode 3
static const char *bgflatE4     = "MFLR8_3";  // end of DOOM Episode 4
static const char *bgflat06     = "SLIME16";  // DOOM2 after MAP06
static const char *bgflat11     = "RROCK14";  // DOOM2 after MAP11
static const char *bgflat20     = "RROCK07";  // DOOM2 after MAP20
static const char *bgflat30     = "RROCK17";  // DOOM2 after MAP30
static const char *bgflat15     = "RROCK13";  // DOOM2 going MAP15 to MAP31
static const char *bgflat31     = "RROCK19";  // DOOM2 going MAP31 to MAP32
static const char *bgcastcall   = "BOSSBACK"; // Panel behind cast call
static const char *bgflathE1    = "FLOOR25";  // haleyjd: Heretic episode 1
static const char *bgflathE2    = "FLATHUH1"; // Heretic episode 2
static const char *bgflathE3    = "FLTWAWA2"; // Heretic episode 3
static const char *bgflathE4    = "FLOOR28";  // Heretic episode 4
static const char *bgflathE5    = "FLOOR08";  // Heretic episode 5

static const char *startup1     = "";  // blank lines are default and are not
static const char *startup2     = "";  // printed
static const char *startup3     = "";
static const char *startup4     = "";
static const char *startup5     = "";

// haleyjd: heretic strings
static const char *s_HHUSTR_E1M1 = HHUSTR_E1M1;
static const char *s_HHUSTR_E1M2 = HHUSTR_E1M2;
static const char *s_HHUSTR_E1M3 = HHUSTR_E1M3;
static const char *s_HHUSTR_E1M4 = HHUSTR_E1M4;
static const char *s_HHUSTR_E1M5 = HHUSTR_E1M5;
static const char *s_HHUSTR_E1M6 = HHUSTR_E1M6;
static const char *s_HHUSTR_E1M7 = HHUSTR_E1M7;
static const char *s_HHUSTR_E1M8 = HHUSTR_E1M8;
static const char *s_HHUSTR_E1M9 = HHUSTR_E1M9;
static const char *s_HHUSTR_E2M1 = HHUSTR_E2M1;
static const char *s_HHUSTR_E2M2 = HHUSTR_E2M2;
static const char *s_HHUSTR_E2M3 = HHUSTR_E2M3;
static const char *s_HHUSTR_E2M4 = HHUSTR_E2M4;
static const char *s_HHUSTR_E2M5 = HHUSTR_E2M5;
static const char *s_HHUSTR_E2M6 = HHUSTR_E2M6;
static const char *s_HHUSTR_E2M7 = HHUSTR_E2M7;
static const char *s_HHUSTR_E2M8 = HHUSTR_E2M8;
static const char *s_HHUSTR_E2M9 = HHUSTR_E2M9;
static const char *s_HHUSTR_E3M1 = HHUSTR_E3M1;
static const char *s_HHUSTR_E3M2 = HHUSTR_E3M2;
static const char *s_HHUSTR_E3M3 = HHUSTR_E3M3;
static const char *s_HHUSTR_E3M4 = HHUSTR_E3M4;
static const char *s_HHUSTR_E3M5 = HHUSTR_E3M5;
static const char *s_HHUSTR_E3M6 = HHUSTR_E3M6;
static const char *s_HHUSTR_E3M7 = HHUSTR_E3M7;
static const char *s_HHUSTR_E3M8 = HHUSTR_E3M8;
static const char *s_HHUSTR_E3M9 = HHUSTR_E3M9;
static const char *s_HHUSTR_E4M1 = HHUSTR_E4M1;
static const char *s_HHUSTR_E4M2 = HHUSTR_E4M2;
static const char *s_HHUSTR_E4M3 = HHUSTR_E4M3;
static const char *s_HHUSTR_E4M4 = HHUSTR_E4M4;
static const char *s_HHUSTR_E4M5 = HHUSTR_E4M5;
static const char *s_HHUSTR_E4M6 = HHUSTR_E4M6;
static const char *s_HHUSTR_E4M7 = HHUSTR_E4M7;
static const char *s_HHUSTR_E4M8 = HHUSTR_E4M8;
static const char *s_HHUSTR_E4M9 = HHUSTR_E4M9;
static const char *s_HHUSTR_E5M1 = HHUSTR_E5M1;
static const char *s_HHUSTR_E5M2 = HHUSTR_E5M2;
static const char *s_HHUSTR_E5M3 = HHUSTR_E5M3;
static const char *s_HHUSTR_E5M4 = HHUSTR_E5M4;
static const char *s_HHUSTR_E5M5 = HHUSTR_E5M5;
static const char *s_HHUSTR_E5M6 = HHUSTR_E5M6;
static const char *s_HHUSTR_E5M7 = HHUSTR_E5M7;
static const char *s_HHUSTR_E5M8 = HHUSTR_E5M8;
static const char *s_HHUSTR_E5M9 = HHUSTR_E5M9;
static const char *s_H1TEXT      = H1TEXT;
static const char *s_H2TEXT      = H2TEXT;
static const char *s_H3TEXT      = H3TEXT;
static const char *s_H4TEXT      = H4TEXT;
static const char *s_H5TEXT      = H5TEXT;
static const char *s_HGOTBLUEKEY = HGOTBLUEKEY;
static const char *s_HGOTYELLOWKEY     = HGOTYELLOWKEY;
static const char *s_HGOTGREENKEY      = HGOTGREENKEY;
static const char *s_HITEMHEALTH       = HITEMHEALTH;
static const char *s_HITEMSHADOWSPHERE = HITEMSHADOWSPHERE;
static const char *s_HITEMQUARTZFLASK  = HITEMQUARTZFLASK;
static const char *s_HITEMWINGSOFWRATH = HITEMWINGSOFWRATH;
static const char *s_HITEMRINGOFINVINCIBILITY = HITEMRINGOFINVINCIBILITY;
static const char *s_HITEMTOMEOFPOWER  = HITEMTOMEOFPOWER;
static const char *s_HITEMMORPHOVUM    = HITEMMORPHOVUM;
static const char *s_HITEMMYSTICURN    = HITEMMYSTICURN;
static const char *s_HITEMTORCH        = HITEMTORCH;
static const char *s_HITEMTIMEBOMB     = HITEMTIMEBOMB;
static const char *s_HITEMTELEPORT     = HITEMTELEPORT;
static const char *s_HITEMSHIELD1      = HITEMSHIELD1;
static const char *s_HITEMSHIELD2      = HITEMSHIELD2;
static const char *s_HITEMBAGOFHOLDING = HITEMBAGOFHOLDING;
static const char *s_HITEMSUPERMAP     = HITEMSUPERMAP;
static const char *s_HPD_GREENO  = HPD_GREENO;
static const char *s_HPD_GREENK  = HPD_GREENK;
static const char *s_HAMMOGOLDWAND1 = HAMMOGOLDWAND1;
static const char *s_HAMMOGOLDWAND2 = HAMMOGOLDWAND2;
static const char *s_HAMMOMACE1     = HAMMOMACE1;
static const char *s_HAMMOMACE2     = HAMMOMACE2;
static const char *s_HAMMOCROSSBOW1 = HAMMOCROSSBOW1;
static const char *s_HAMMOCROSSBOW2 = HAMMOCROSSBOW2;
static const char *s_HAMMOBLASTER1  = HAMMOBLASTER1;
static const char *s_HAMMOBLASTER2  = HAMMOBLASTER2;
static const char *s_HAMMOSKULLROD1 = HAMMOSKULLROD1;
static const char *s_HAMMOSKULLROD2 = HAMMOSKULLROD2;
static const char *s_HAMMOPHOENIXROD1 = HAMMOPHOENIXROD1;
static const char *s_HAMMOPHOENIXROD2 = HAMMOPHOENIXROD2;
static const char *s_HWEAPONMACE       = HWEAPONMACE;
static const char *s_HWEAPONCROSSBOW   = HWEAPONCROSSBOW;
static const char *s_HWEAPONBLASTER    = HWEAPONBLASTER;
static const char *s_HWEAPONSKULLROD   = HWEAPONSKULLROD;
static const char *s_HWEAPONPHOENIXROD = HWEAPONPHOENIXROD;
static const char *s_HWEAPONGAUNTLETS  = HWEAPONGAUNTLETS;
static const char *s_TXT_CHEATGODON         = TXT_CHEATGODON;
static const char *s_TXT_CHEATGODOFF        = TXT_CHEATGODOFF;
static const char *s_TXT_CHEATNOCLIPON      = TXT_CHEATNOCLIPON;
static const char *s_TXT_CHEATNOCLIPOFF     = TXT_CHEATNOCLIPOFF;
static const char *s_TXT_CHEATWEAPONS       = TXT_CHEATWEAPONS;
static const char *s_TXT_CHEATPOWERON       = TXT_CHEATPOWERON;
static const char *s_TXT_CHEATPOWEROFF      = TXT_CHEATPOWEROFF;
static const char *s_TXT_CHEATHEALTH        = TXT_CHEATHEALTH;
static const char *s_TXT_CHEATKEYS          = TXT_CHEATKEYS;
static const char *s_TXT_CHEATARTIFACTS1    = TXT_CHEATARTIFACTS1;
static const char *s_TXT_CHEATARTIFACTS2    = TXT_CHEATARTIFACTS2;
static const char *s_TXT_CHEATARTIFACTS3    = TXT_CHEATARTIFACTS3;
static const char *s_TXT_CHEATARTIFACTSFAIL = TXT_CHEATARTIFACTSFAIL;
static const char *s_TXT_CHEATWARP          = TXT_CHEATWARP;
static const char *s_TXT_CHEATCHICKENON     = TXT_CHEATCHICKENON;
static const char *s_TXT_CHEATCHICKENOFF    = TXT_CHEATCHICKENOFF;
static const char *s_TXT_CHEATMASSACRE      = TXT_CHEATMASSACRE;
static const char *s_TXT_CHEATIDDQD         = TXT_CHEATIDDQD;
static const char *s_TXT_CHEATIDKFA         = TXT_CHEATIDKFA;

// obituaries
static const char *s_OB_SUICIDE = OB_SUICIDE;
static const char *s_OB_FALLING = OB_FALLING;
static const char *s_OB_CRUSH   = OB_CRUSH;
static const char *s_OB_LAVA    = OB_LAVA;
static const char *s_OB_SLIME   = OB_SLIME;
static const char *s_OB_BARREL  = OB_BARREL;
static const char *s_OB_SPLASH  = OB_SPLASH;
static const char *s_OB_COOP    = OB_COOP;
static const char *s_OB_DEFAULT = OB_DEFAULT;
static const char *s_OB_R_SPLASH_SELF = OB_R_SPLASH_SELF;
static const char *s_OB_ROCKET_SELF = OB_ROCKET_SELF;
static const char *s_OB_BFG11K_SELF = OB_BFG11K_SELF;
static const char *s_OB_FIST = OB_FIST;
static const char *s_OB_CHAINSAW = OB_CHAINSAW;
static const char *s_OB_PISTOL = OB_PISTOL;
static const char *s_OB_SHOTGUN = OB_SHOTGUN;
static const char *s_OB_SSHOTGUN = OB_SSHOTGUN;
static const char *s_OB_CHAINGUN = OB_CHAINGUN;
static const char *s_OB_ROCKET = OB_ROCKET;
static const char *s_OB_R_SPLASH = OB_R_SPLASH;
static const char *s_OB_PLASMA = OB_PLASMA;
static const char *s_OB_BFG = OB_BFG;
static const char *s_OB_BFG_SPLASH = OB_BFG_SPLASH;
static const char *s_OB_BETABFG = OB_BETABFG;
static const char *s_OB_BFGBURST = OB_BFGBURST;
static const char *s_OB_GRENADE_SELF = OB_GRENADE_SELF;
static const char *s_OB_GRENADE = OB_GRENADE;
static const char *s_OB_TELEFRAG = OB_TELEFRAG;
static const char *s_OB_QUAKE = OB_QUAKE;

// misc new strings
static const char *s_SECRETMESSAGE = SECRETMESSAGE;

// Ty 05/03/98 - externalized
const char *savegamename;

// end d_deh.h variable declarations
// ============================================================

// Do this for a lookup--the pointer (loaded above) is 
// cross-referenced to a string key that is the same as the define 
// above.  We will use estrdups to set these new values that we read 
// from the file, orphaning the original value set above.

dehstr_t deh_strlookup[] = 
{
   { &s_D_DEVSTR,    "D_DEVSTR"    },
   { &s_D_CDROM,     "D_CDROM"     },
   { &s_PRESSKEY,    "PRESSKEY"    },
   { &s_PRESSYN,     "PRESSYN"     },
   { &s_QUITMSG,     "QUITMSG"     },
   { &s_LOADNET,     "LOADNET"     },
   { &s_QLOADNET,    "QLOADNET"    },
   { &s_QSAVESPOT,   "QSAVESPOT"   },
   { &s_SAVEDEAD,    "SAVEDEAD"    },
   // cph - disabled to prevent format string attacks
   // {&s_QSPROMPT,  "QSPROMPT"    },
   // {&s_QLPROMPT,  "QLPROMPT"    },
   { &s_NEWGAME,     "NEWGAME"     },
   { &s_NIGHTMARE,   "NIGHTMARE"   },
   { &s_SWSTRING,    "SWSTRING"    },
   { &s_MSGOFF,      "MSGOFF"      },
   { &s_MSGON,       "MSGON"       },
   { &s_NETEND,      "NETEND"      },
   { &s_ENDGAME,     "ENDGAME"     },
   { &s_DOSY,        "DOSY"        },
   // joel - detail level strings ditched
   { &s_GAMMALVL0,   "GAMMALVL0"   },
   { &s_GAMMALVL1,   "GAMMALVL1"   },
   { &s_GAMMALVL2,   "GAMMALVL2"   },
   { &s_GAMMALVL3,   "GAMMALVL3"   },
   { &s_GAMMALVL4,   "GAMMALVL4"   },
   { &s_EMPTYSTRING, "EMPTYSTRING" },
   { &s_GOTARMOR,    "GOTARMOR"    },
   { &s_GOTMEGA,     "GOTMEGA"     },
   { &s_GOTHTHBONUS, "GOTHTHBONUS" },
   { &s_GOTARMBONUS, "GOTARMBONUS" },
   { &s_GOTSTIM,     "GOTSTIM"     },
   { &s_GOTMEDINEED, "GOTMEDINEED" },
   { &s_GOTMEDIKIT,  "GOTMEDIKIT"  },
   { &s_GOTSUPER,    "GOTSUPER"    },
   { &s_GOTBLUECARD, "GOTBLUECARD" },
   { &s_GOTYELWCARD, "GOTYELWCARD" },
   { &s_GOTREDCARD,  "GOTREDCARD"  },
   { &s_GOTBLUESKUL, "GOTBLUESKUL" },
   { &s_GOTYELWSKUL, "GOTYELWSKUL" },
   { &s_GOTREDSKULL, "GOTREDSKULL" },
   { &s_GOTINVUL,    "GOTINVUL"    },
   { &s_GOTBERSERK,  "GOTBERSERK"  },
   { &s_GOTINVIS,    "GOTINVIS"    },
   { &s_GOTSUIT,     "GOTSUIT"     },
   { &s_GOTMAP,      "GOTMAP"      },
   { &s_GOTVISOR,    "GOTVISOR"    },
   { &s_GOTMSPHERE,  "GOTMSPHERE"  },
   { &s_GOTCLIP,     "GOTCLIP"     },
   { &s_GOTCLIPBOX,  "GOTCLIPBOX"  },
   { &s_GOTROCKET,   "GOTROCKET"   },
   { &s_GOTROCKBOX,  "GOTROCKBOX"  },
   { &s_GOTCELL,     "GOTCELL"     },
   { &s_GOTCELLBOX,  "GOTCELLBOX"  },
   { &s_GOTSHELLS,   "GOTSHELLS"   },
   { &s_GOTSHELLBOX, "GOTSHELLBOX" },
   { &s_GOTBACKPACK, "GOTBACKPACK" },
   { &s_GOTBFG9000,  "GOTBFG9000"  },
   { &s_GOTCHAINGUN, "GOTCHAINGUN" },
   { &s_GOTCHAINSAW, "GOTCHAINSAW" },
   { &s_GOTLAUNCHER, "GOTLAUNCHER" },
   { &s_GOTPLASMA,   "GOTPLASMA"   },
   { &s_GOTSHOTGUN,  "GOTSHOTGUN"  },
   { &s_GOTSHOTGUN2, "GOTSHOTGUN2" },
   { &s_BETA_BONUS3, "BETA_BONUS3" },
   { &s_BETA_BONUS4, "BETA_BONUS4" },
   { &s_PD_BLUEO,    "PD_BLUEO"    },
   { &s_PD_REDO,     "PD_REDO"     },
   { &s_PD_YELLOWO,  "PD_YELLOWO"  },
   { &s_PD_BLUEK,    "PD_BLUEK"    },
   { &s_PD_REDK,     "PD_REDK"     },
   { &s_PD_YELLOWK,  "PD_YELLOWK"  },
   { &s_PD_BLUEC,    "PD_BLUEC"    },
   { &s_PD_REDC,     "PD_REDC"     },
   { &s_PD_YELLOWC,  "PD_YELLOWC"  },
   { &s_PD_BLUES,    "PD_BLUES"    },
   { &s_PD_REDS,     "PD_REDS"     },
   { &s_PD_YELLOWS,  "PD_YELLOWS"  },
   { &s_PD_ANY,      "PD_ANY"      },
   { &s_PD_ALL3,     "PD_ALL3"     },
   { &s_PD_ALL6,     "PD_ALL6"     },
   { &s_GGSAVED,     "GGSAVED"     },
   { &s_HUSTR_MSGU,  "HUSTR_MSGU"  },
   { &s_HUSTR_E1M1,  "HUSTR_E1M1"  },
   { &s_HUSTR_E1M2,  "HUSTR_E1M2"  },
   { &s_HUSTR_E1M3,  "HUSTR_E1M3"  },
   { &s_HUSTR_E1M4,  "HUSTR_E1M4"  },
   { &s_HUSTR_E1M5,  "HUSTR_E1M5"  },
   { &s_HUSTR_E1M6,  "HUSTR_E1M6"  },
   { &s_HUSTR_E1M7,  "HUSTR_E1M7"  },
   { &s_HUSTR_E1M8,  "HUSTR_E1M8"  },
   { &s_HUSTR_E1M9,  "HUSTR_E1M9"  },
   { &s_HUSTR_E2M1,  "HUSTR_E2M1"  },
   { &s_HUSTR_E2M2,  "HUSTR_E2M2"  },
   { &s_HUSTR_E2M3,  "HUSTR_E2M3"  },
   { &s_HUSTR_E2M4,  "HUSTR_E2M4"  },
   { &s_HUSTR_E2M5,  "HUSTR_E2M5"  },
   { &s_HUSTR_E2M6,  "HUSTR_E2M6"  },
   { &s_HUSTR_E2M7,  "HUSTR_E2M7"  },
   { &s_HUSTR_E2M8,  "HUSTR_E2M8"  },
   { &s_HUSTR_E2M9,  "HUSTR_E2M9"  },
   { &s_HUSTR_E3M1,  "HUSTR_E3M1"  },
   { &s_HUSTR_E3M2,  "HUSTR_E3M2"  },
   { &s_HUSTR_E3M3,  "HUSTR_E3M3"  },
   { &s_HUSTR_E3M4,  "HUSTR_E3M4"  },
   { &s_HUSTR_E3M5,  "HUSTR_E3M5"  },
   { &s_HUSTR_E3M6,  "HUSTR_E3M6"  },
   { &s_HUSTR_E3M7,  "HUSTR_E3M7"  },
   { &s_HUSTR_E3M8,  "HUSTR_E3M8"  },
   { &s_HUSTR_E3M9,  "HUSTR_E3M9"  },
   { &s_HUSTR_E4M1,  "HUSTR_E4M1"  },
   { &s_HUSTR_E4M2,  "HUSTR_E4M2"  },
   { &s_HUSTR_E4M3,  "HUSTR_E4M3"  },
   { &s_HUSTR_E4M4,  "HUSTR_E4M4"  },
   { &s_HUSTR_E4M5,  "HUSTR_E4M5"  },
   { &s_HUSTR_E4M6,  "HUSTR_E4M6"  },
   { &s_HUSTR_E4M7,  "HUSTR_E4M7"  },
   { &s_HUSTR_E4M8,  "HUSTR_E4M8"  },
   { &s_HUSTR_E4M9,  "HUSTR_E4M9"  },
   { &s_HUSTR_1,     "HUSTR_1"     },
   { &s_HUSTR_2,     "HUSTR_2"     },
   { &s_HUSTR_3,     "HUSTR_3"     },
   { &s_HUSTR_4,     "HUSTR_4"     },
   { &s_HUSTR_5,     "HUSTR_5"     },
   { &s_HUSTR_6,     "HUSTR_6"     },
   { &s_HUSTR_7,     "HUSTR_7"     },
   { &s_HUSTR_8,     "HUSTR_8"     },
   { &s_HUSTR_9,     "HUSTR_9"     },
   { &s_HUSTR_10,    "HUSTR_10"    },
   { &s_HUSTR_11,    "HUSTR_11"    },
   { &s_HUSTR_12,    "HUSTR_12"    },
   { &s_HUSTR_13,    "HUSTR_13"    },
   { &s_HUSTR_14,    "HUSTR_14"    },
   { &s_HUSTR_15,    "HUSTR_15"    },
   { &s_HUSTR_16,    "HUSTR_16"    },
   { &s_HUSTR_17,    "HUSTR_17"    },
   { &s_HUSTR_18,    "HUSTR_18"    },
   { &s_HUSTR_19,    "HUSTR_19"    },
   { &s_HUSTR_20,    "HUSTR_20"    },
   { &s_HUSTR_21,    "HUSTR_21"    },
   { &s_HUSTR_22,    "HUSTR_22"    },
   { &s_HUSTR_23,    "HUSTR_23"    },
   { &s_HUSTR_24,    "HUSTR_24"    },
   { &s_HUSTR_25,    "HUSTR_25"    },
   { &s_HUSTR_26,    "HUSTR_26"    },
   { &s_HUSTR_27,    "HUSTR_27"    },
   { &s_HUSTR_28,    "HUSTR_28"    },
   { &s_HUSTR_29,    "HUSTR_29"    },
   { &s_HUSTR_30,    "HUSTR_30"    },
   { &s_HUSTR_31,    "HUSTR_31"    },
   { &s_HUSTR_32,    "HUSTR_32"    },
   { &s_HUSTR_33,    "HUSTR_33"    },
   { &s_PHUSTR_1,    "PHUSTR_1"    },
   { &s_PHUSTR_2,    "PHUSTR_2"    },
   { &s_PHUSTR_3,    "PHUSTR_3"    },
   { &s_PHUSTR_4,    "PHUSTR_4"    },
   { &s_PHUSTR_5,    "PHUSTR_5"    },
   { &s_PHUSTR_6,    "PHUSTR_6"    },
   { &s_PHUSTR_7,    "PHUSTR_7"    },
   { &s_PHUSTR_8,    "PHUSTR_8"    },
   { &s_PHUSTR_9,    "PHUSTR_9"    },
   { &s_PHUSTR_10,   "PHUSTR_10"   },
   { &s_PHUSTR_11,   "PHUSTR_11"   },
   { &s_PHUSTR_12,   "PHUSTR_12"   },
   { &s_PHUSTR_13,   "PHUSTR_13"   },
   { &s_PHUSTR_14,   "PHUSTR_14"   },
   { &s_PHUSTR_15,   "PHUSTR_15"   },
   { &s_PHUSTR_16,   "PHUSTR_16"   },
   { &s_PHUSTR_17,   "PHUSTR_17"   },
   { &s_PHUSTR_18,   "PHUSTR_18"   },
   { &s_PHUSTR_19,   "PHUSTR_19"   },
   { &s_PHUSTR_20,   "PHUSTR_20"   },
   { &s_PHUSTR_21,   "PHUSTR_21"   },
   { &s_PHUSTR_22,   "PHUSTR_22"   },
   { &s_PHUSTR_23,   "PHUSTR_23"   },
   { &s_PHUSTR_24,   "PHUSTR_24"   },
   { &s_PHUSTR_25,   "PHUSTR_25"   },
   { &s_PHUSTR_26,   "PHUSTR_26"   },
   { &s_PHUSTR_27,   "PHUSTR_27"   },
   { &s_PHUSTR_28,   "PHUSTR_28"   },
   { &s_PHUSTR_29,   "PHUSTR_29"   },
   { &s_PHUSTR_30,   "PHUSTR_30"   },
   { &s_PHUSTR_31,   "PHUSTR_31"   },
   { &s_PHUSTR_32,   "PHUSTR_32"   },
   { &s_THUSTR_1,    "THUSTR_1"    },
   { &s_THUSTR_2,    "THUSTR_2"    },
   { &s_THUSTR_3,    "THUSTR_3"    },
   { &s_THUSTR_4,    "THUSTR_4"    },
   { &s_THUSTR_5,    "THUSTR_5"    },
   { &s_THUSTR_6,    "THUSTR_6"    },
   { &s_THUSTR_7,    "THUSTR_7"    },
   { &s_THUSTR_8,    "THUSTR_8"    },
   { &s_THUSTR_9,    "THUSTR_9"    },
   { &s_THUSTR_10,   "THUSTR_10"   },
   { &s_THUSTR_11,   "THUSTR_11"   },
   { &s_THUSTR_12,   "THUSTR_12"   },
   { &s_THUSTR_13,   "THUSTR_13"   },
   { &s_THUSTR_14,   "THUSTR_14"   },
   { &s_THUSTR_15,   "THUSTR_15"   },
   { &s_THUSTR_16,   "THUSTR_16"   },
   { &s_THUSTR_17,   "THUSTR_17"   },
   { &s_THUSTR_18,   "THUSTR_18"   },
   { &s_THUSTR_19,   "THUSTR_19"   },
   { &s_THUSTR_20,   "THUSTR_20"   },
   { &s_THUSTR_21,   "THUSTR_21"   },
   { &s_THUSTR_22,   "THUSTR_22"   },
   { &s_THUSTR_23,   "THUSTR_23"   },
   { &s_THUSTR_24,   "THUSTR_24"   },
   { &s_THUSTR_25,   "THUSTR_25"   },
   { &s_THUSTR_26,   "THUSTR_26"   },
   { &s_THUSTR_27,   "THUSTR_27"   },
   { &s_THUSTR_28,   "THUSTR_28"   },
   { &s_THUSTR_29,   "THUSTR_29"   },
   { &s_THUSTR_30,   "THUSTR_30"   },
   { &s_THUSTR_31,   "THUSTR_31"   },
   { &s_THUSTR_32,   "THUSTR_32"   },
   { &s_HUSTR_CHATMACRO1,   "HUSTR_CHATMACRO1"   },
   { &s_HUSTR_CHATMACRO2,   "HUSTR_CHATMACRO2"   },
   { &s_HUSTR_CHATMACRO3,   "HUSTR_CHATMACRO3"   },
   { &s_HUSTR_CHATMACRO4,   "HUSTR_CHATMACRO4"   },
   { &s_HUSTR_CHATMACRO5,   "HUSTR_CHATMACRO5"   },
   { &s_HUSTR_CHATMACRO6,   "HUSTR_CHATMACRO6"   },
   { &s_HUSTR_CHATMACRO7,   "HUSTR_CHATMACRO7"   },
   { &s_HUSTR_CHATMACRO8,   "HUSTR_CHATMACRO8"   },
   { &s_HUSTR_CHATMACRO9,   "HUSTR_CHATMACRO9"   },
   { &s_HUSTR_CHATMACRO0,   "HUSTR_CHATMACRO0"   },
   { &s_HUSTR_TALKTOSELF1,  "HUSTR_TALKTOSELF1"  },
   { &s_HUSTR_TALKTOSELF2,  "HUSTR_TALKTOSELF2"  },
   { &s_HUSTR_TALKTOSELF3,  "HUSTR_TALKTOSELF3"  },
   { &s_HUSTR_TALKTOSELF4,  "HUSTR_TALKTOSELF4"  },
   { &s_HUSTR_TALKTOSELF5,  "HUSTR_TALKTOSELF5"  },
   { &s_HUSTR_MESSAGESENT,  "HUSTR_MESSAGESENT"  },
   { &s_HUSTR_PLRGREEN,     "HUSTR_PLRGREEN"     },
   { &s_HUSTR_PLRINDIGO,    "HUSTR_PLRINDIGO"    },
   { &s_HUSTR_PLRBROWN,     "HUSTR_PLRBROWN"     },
   { &s_HUSTR_PLRRED,       "HUSTR_PLRRED"       },
   // player chat keys removed
   { &s_AMSTR_FOLLOWON,     "AMSTR_FOLLOWON"     },
   { &s_AMSTR_FOLLOWOFF,    "AMSTR_FOLLOWOFF"    },
   { &s_AMSTR_GRIDON,       "AMSTR_GRIDON"       },
   { &s_AMSTR_GRIDOFF,      "AMSTR_GRIDOFF"      },
   { &s_AMSTR_MARKEDSPOT,   "AMSTR_MARKEDSPOT"   },
   { &s_AMSTR_MARKSCLEARED, "AMSTR_MARKSCLEARED" },
   { &s_STSTR_MUS,          "STSTR_MUS"          },
   { &s_STSTR_NOMUS,        "STSTR_NOMUS"        },
   { &s_STSTR_DQDON,        "STSTR_DQDON"        },
   { &s_STSTR_DQDOFF,       "STSTR_DQDOFF"       },
   { &s_STSTR_KFAADDED,     "STSTR_KFAADDED"     },
   { &s_STSTR_FAADDED,      "STSTR_FAADDED"      },
   { &s_STSTR_NCON,         "STSTR_NCON"         },
   { &s_STSTR_NCOFF,        "STSTR_NCOFF"        },
   { &s_STSTR_BEHOLD,       "STSTR_BEHOLD"       },
   { &s_STSTR_BEHOLDX,      "STSTR_BEHOLDX"      },
   { &s_STSTR_CHOPPERS,     "STSTR_CHOPPERS"     },
   { &s_STSTR_CLEV,         "STSTR_CLEV"         },
   { &s_STSTR_COMPON,       "STSTR_COMPON"       },
   { &s_STSTR_COMPOFF,      "STSTR_COMPOFF"      },
   { &s_E1TEXT,      "E1TEXT"      },
   { &s_E2TEXT,      "E2TEXT"      },
   { &s_E3TEXT,      "E3TEXT"      },
   { &s_E4TEXT,      "E4TEXT"      },
   { &s_C1TEXT,      "C1TEXT"      },
   { &s_C2TEXT,      "C2TEXT"      },
   { &s_C3TEXT,      "C3TEXT"      },
   { &s_C4TEXT,      "C4TEXT"      },
   { &s_C5TEXT,      "C5TEXT"      },
   { &s_C6TEXT,      "C6TEXT"      },
   { &s_P1TEXT,      "P1TEXT"      },
   { &s_P2TEXT,      "P2TEXT"      },
   { &s_P3TEXT,      "P3TEXT"      },
   { &s_P4TEXT,      "P4TEXT"      },
   { &s_P5TEXT,      "P5TEXT"      },
   { &s_P6TEXT,      "P6TEXT"      },
   { &s_T1TEXT,      "T1TEXT"      },
   { &s_T2TEXT,      "T2TEXT"      },
   { &s_T3TEXT,      "T3TEXT"      },
   { &s_T4TEXT,      "T4TEXT"      },
   { &s_T5TEXT,      "T5TEXT"      },
   { &s_T6TEXT,      "T6TEXT"      },
   { &s_CC_ZOMBIE,   "CC_ZOMBIE"   },
   { &s_CC_SHOTGUN,  "CC_SHOTGUN"  },
   { &s_CC_HEAVY,    "CC_HEAVY"    },
   { &s_CC_IMP,      "CC_IMP"      },
   { &s_CC_DEMON,    "CC_DEMON"    },
   { &s_CC_LOST,     "CC_LOST"     },
   { &s_CC_CACO,     "CC_CACO"     },
   { &s_CC_HELL,     "CC_HELL"     },
   { &s_CC_BARON,    "CC_BARON"    },
   { &s_CC_ARACH,    "CC_ARACH"    },
   { &s_CC_PAIN,     "CC_PAIN"     },
   { &s_CC_REVEN,    "CC_REVEN"    },
   { &s_CC_MANCU,    "CC_MANCU"    },
   { &s_CC_ARCH,     "CC_ARCH"     },
   { &s_CC_SPIDER,   "CC_SPIDER"   },
   { &s_CC_CYBER,    "CC_CYBER"    },
   { &s_CC_HERO,     "CC_HERO"     },
   { &s_CC_TITLE,    "CC_TITLE"    },
   // haleyjd 10/08/06: new demo sequence strings
   { &s_TITLEPIC,    "TITLEPIC"    },
   { &s_TITLE,       "TITLE"       },
   { &s_DEMO1,       "DEMO1"       },
   { &s_DEMO2,       "DEMO2"       },
   { &s_DEMO3,       "DEMO3"       },
   { &s_DEMO4,       "DEMO4"       },
   { &s_CREDIT,      "CREDIT"      },
   { &s_ORDER,       "ORDER"       },
   { &s_HELP2,       "HELP2"       },
   // background flats
   { &bgflatE1,      "BGFLATE1"    },
   { &bgflatE2,      "BGFLATE2"    },
   { &bgflatE3,      "BGFLATE3"    },
   { &bgflatE4,      "BGFLATE4"    },
   { &bgflat06,      "BGFLAT06"    },
   { &bgflat11,      "BGFLAT11"    },
   { &bgflat20,      "BGFLAT20"    },
   { &bgflat30,      "BGFLAT30"    },
   { &bgflat15,      "BGFLAT15"    },
   { &bgflat31,      "BGFLAT31"    },
   { &bgcastcall,    "BGCASTCALL"  },
   // Ty 04/08/98 - added 5 general purpose startup announcement
   // strings for hacker use.
   { &startup1,      "STARTUP1"    },
   { &startup2,      "STARTUP2"    },
   { &startup3,      "STARTUP3"    },
   { &startup4,      "STARTUP4"    },
   { &startup5,      "STARTUP5"    },
   { &savegamename,  "SAVEGAMENAME"},  // Ty 05/03/98
   // haleyjd: Eternity and Heretic strings follow
   { &bgflathE1,     "BGFLATHE1"   },
   { &bgflathE2,     "BGFLATHE2"   },
   { &bgflathE3,     "BGFLATHE3"   },
   { &bgflathE4,     "BGFLATHE4"   },
   { &bgflathE5,     "BGFLATHE5"   },
   { &s_HHUSTR_E1M1, "HHUSTR_E1M1" },
   { &s_HHUSTR_E1M2, "HHUSTR_E1M2" },
   { &s_HHUSTR_E1M3, "HHUSTR_E1M3" },
   { &s_HHUSTR_E1M4, "HHUSTR_E1M4" },
   { &s_HHUSTR_E1M5, "HHUSTR_E1M5" },
   { &s_HHUSTR_E1M6, "HHUSTR_E1M6" },
   { &s_HHUSTR_E1M7, "HHUSTR_E1M7" },
   { &s_HHUSTR_E1M8, "HHUSTR_E1M8" },
   { &s_HHUSTR_E1M9, "HHUSTR_E1M9" },
   { &s_HHUSTR_E2M1, "HHUSTR_E2M1" },
   { &s_HHUSTR_E2M2, "HHUSTR_E2M2" },
   { &s_HHUSTR_E2M3, "HHUSTR_E2M3" },
   { &s_HHUSTR_E2M4, "HHUSTR_E2M4" },
   { &s_HHUSTR_E2M5, "HHUSTR_E2M5" },
   { &s_HHUSTR_E2M6, "HHUSTR_E2M6" },
   { &s_HHUSTR_E2M7, "HHUSTR_E2M7" },
   { &s_HHUSTR_E2M8, "HHUSTR_E2M8" },
   { &s_HHUSTR_E2M9, "HHUSTR_E2M9" },
   { &s_HHUSTR_E3M1, "HHUSTR_E3M1" },
   { &s_HHUSTR_E3M2, "HHUSTR_E3M2" },
   { &s_HHUSTR_E3M3, "HHUSTR_E3M3" },
   { &s_HHUSTR_E3M4, "HHUSTR_E3M4" },
   { &s_HHUSTR_E3M5, "HHUSTR_E3M5" },
   { &s_HHUSTR_E3M6, "HHUSTR_E3M6" },
   { &s_HHUSTR_E3M7, "HHUSTR_E3M7" },
   { &s_HHUSTR_E3M8, "HHUSTR_E3M8" },
   { &s_HHUSTR_E3M9, "HHUSTR_E3M9" },
   { &s_HHUSTR_E4M1, "HHUSTR_E4M1" },
   { &s_HHUSTR_E4M2, "HHUSTR_E4M2" },
   { &s_HHUSTR_E4M3, "HHUSTR_E4M3" },
   { &s_HHUSTR_E4M4, "HHUSTR_E4M4" },
   { &s_HHUSTR_E4M5, "HHUSTR_E4M5" },
   { &s_HHUSTR_E4M6, "HHUSTR_E4M6" },
   { &s_HHUSTR_E4M7, "HHUSTR_E4M7" },
   { &s_HHUSTR_E4M8, "HHUSTR_E4M8" },
   { &s_HHUSTR_E4M9, "HHUSTR_E4M9" },
   { &s_HHUSTR_E5M1, "HHUSTR_E5M1" },
   { &s_HHUSTR_E5M2, "HHUSTR_E5M2" },
   { &s_HHUSTR_E5M3, "HHUSTR_E5M3" },
   { &s_HHUSTR_E5M4, "HHUSTR_E5M4" },
   { &s_HHUSTR_E5M5, "HHUSTR_E5M5" },
   { &s_HHUSTR_E5M6, "HHUSTR_E5M6" },
   { &s_HHUSTR_E5M7, "HHUSTR_E5M7" },
   { &s_HHUSTR_E5M8, "HHUSTR_E5M8" },
   { &s_HHUSTR_E5M9, "HHUSTR_E5M9" },
   { &s_H1TEXT,      "H1TEXT"      },
   { &s_H2TEXT,      "H2TEXT"      },
   { &s_H3TEXT,      "H3TEXT"      },
   { &s_H4TEXT,      "H4TEXT"      },
   { &s_H5TEXT,      "H5TEXT"      },
   { &s_HGOTBLUEKEY,       "HGOTBLUEKEY"       },
   { &s_HGOTYELLOWKEY,     "HGOTYELLOWKEY"     },
   { &s_HGOTGREENKEY,      "HGOTGREENKEY"      },
   { &s_HITEMHEALTH,       "HITEMHEALTH"       },
   { &s_HITEMSHADOWSPHERE, "HITEMSHADOWSPHERE" },
   { &s_HITEMQUARTZFLASK,  "HITEMQUARTZFLASK"  },
   { &s_HITEMWINGSOFWRATH, "HITEMWINGSOFWRATH" },
   { &s_HITEMRINGOFINVINCIBILITY, "HITEMRINGOFINVINCIBILITY" },
   { &s_HITEMTOMEOFPOWER,  "HITEMTOMEOFPOWER"  },
   { &s_HITEMMORPHOVUM,    "HITEMMORPHOVUM"    },
   { &s_HITEMMYSTICURN,    "HITEMMYSTICURN"    },
   { &s_HITEMTORCH,        "HITEMTORCH"        },
   { &s_HITEMTIMEBOMB,     "HITEMTIMEBOMB"     },
   { &s_HITEMTELEPORT,     "HITEMTELEPORT"     },
   { &s_HITEMSHIELD1,      "HITEMSHIELD1"      },
   { &s_HITEMSHIELD2,      "HITEMSHIELD2"      },
   { &s_HITEMBAGOFHOLDING, "HITEMBAGOFHOLDING" },
   { &s_HITEMSUPERMAP,     "HITEMSUPERMAP"     },
   { &s_HAMMOGOLDWAND1,    "HAMMOGOLDWAND1"    },
   { &s_HAMMOGOLDWAND2,    "HAMMOGOLDWAND2"    },
   { &s_HAMMOMACE1,        "HAMMOMACE1"        },
   { &s_HAMMOMACE2,        "HAMMOMACE2"        },
   { &s_HAMMOCROSSBOW1,    "HAMMOCROSSBOW1"    },
   { &s_HAMMOCROSSBOW2,    "HAMMOCROSSBOW2"    },
   { &s_HAMMOBLASTER1,     "HAMMOBLASTER1"     },
   { &s_HAMMOBLASTER2,     "HAMMOBLASTER2"     },
   { &s_HAMMOSKULLROD1,    "HAMMOSKULLROD1"    },
   { &s_HAMMOSKULLROD2,    "HAMMOSKULLROD2"    },
   { &s_HAMMOPHOENIXROD1,  "HAMMOPHOENIXROD1"  },
   { &s_HAMMOPHOENIXROD2,  "HAMMOPHOENIXROD2"  },
   { &s_HWEAPONMACE,          "HWEAPONMACE"       },
   { &s_HWEAPONCROSSBOW,      "HWEAPONCROSSBOW"   },
   { &s_HWEAPONBLASTER,       "HWEAPONBLASTER"    },
   { &s_HWEAPONSKULLROD,      "HWEAPONSKULLROD"   },
   { &s_HWEAPONPHOENIXROD,    "HWEAPONPHOENIXROD" },
   { &s_HWEAPONGAUNTLETS,     "HWEAPONGAUNTLETS"  },
   { &s_HPD_GREENO,        "HPD_GREENO"        },
   { &s_HPD_GREENK,        "HPD_GREENK"        },
   { &s_TXT_CHEATGODON,         "TXT_CHEATGODON"        },
   { &s_TXT_CHEATGODOFF,        "TXT_CHEATGODOFF"        },
   { &s_TXT_CHEATNOCLIPON,      "TXT_CHEATNOCLIPON"      },
   { &s_TXT_CHEATNOCLIPOFF,     "TXT_CHEATNOCLIPOFF"     },
   { &s_TXT_CHEATWEAPONS,       "TXT_CHEATWEAPONS"       },
   { &s_TXT_CHEATPOWERON,       "TXT_CHEATPOWERON"       },
   { &s_TXT_CHEATPOWEROFF,      "TXT_CHEATPOWEROFF"      },
   { &s_TXT_CHEATHEALTH,        "TXT_CHEATHEALTH"        },
   { &s_TXT_CHEATKEYS,          "TXT_CHEATKEYS"          },
   { &s_TXT_CHEATARTIFACTS1,    "TXT_CHEATARTIFACTS1"    },
   { &s_TXT_CHEATARTIFACTS2,    "TXT_CHEATARTIFACTS2"    },
   { &s_TXT_CHEATARTIFACTS3,    "TXT_CHEATARTIFACTS3"    },
   { &s_TXT_CHEATARTIFACTSFAIL, "TXT_CHEATARTIFACTSFAIL" },
   { &s_TXT_CHEATWARP,          "TXT_CHEATWARP"          },
   { &s_TXT_CHEATCHICKENON,     "TXT_CHEATCHICKENON"     },
   { &s_TXT_CHEATCHICKENOFF,    "TXT_CHEATCHICKENOFF"    },
   { &s_TXT_CHEATMASSACRE,      "TXT_CHEATMASSACRE"      },
   { &s_TXT_CHEATIDDQD,         "TXT_CHEATIDDQD"         },
   { &s_TXT_CHEATIDKFA,         "TXT_CHEATIDKFA"         },
   { &s_OB_SUICIDE,        "OB_SUICIDE"        },
   { &s_OB_FALLING,        "OB_FALLING"        },
   { &s_OB_CRUSH,          "OB_CRUSH"          },
   { &s_OB_LAVA,           "OB_LAVA"           },
   { &s_OB_SLIME,          "OB_SLIME"          },
   { &s_OB_BARREL,         "OB_BARREL"         },
   { &s_OB_SPLASH,         "OB_SPLASH"         },
   { &s_OB_COOP,           "OB_COOP"           },
   { &s_OB_DEFAULT,        "OB_DEFAULT"        },
   { &s_OB_R_SPLASH_SELF,  "OB_R_SPLASH_SELF"  },
   { &s_OB_ROCKET_SELF,    "OB_ROCKET_SELF"    },
   { &s_OB_BFG11K_SELF,    "OB_BFG11K_SELF"    },
   { &s_OB_FIST,           "OB_FIST"           },
   { &s_OB_CHAINSAW,       "OB_CHAINSAW"       },
   { &s_OB_PISTOL,         "OB_PISTOL"         },
   { &s_OB_SHOTGUN,        "OB_SHOTGUN"        },
   { &s_OB_SSHOTGUN,       "OB_SSHOTGUN"       },
   { &s_OB_CHAINGUN,       "OB_CHAINGUN"       },
   { &s_OB_ROCKET,         "OB_ROCKET"         },
   { &s_OB_R_SPLASH,       "OB_R_SPLASH"       },
   { &s_OB_PLASMA,         "OB_PLASMA"         },
   { &s_OB_BFG,            "OB_BFG"            },
   { &s_OB_BFG_SPLASH,     "OB_BFG_SPLASH"     },
   { &s_OB_BETABFG,        "OB_BETABFG"        },
   { &s_OB_BFGBURST,       "OB_BFGBURST"       },
   { &s_OB_GRENADE_SELF,   "OB_GRENADE_SELF"   },
   { &s_OB_GRENADE,        "OB_GRENADE"        },
   { &s_OB_TELEFRAG,       "OB_TELEFRAG"       },
   { &s_OB_QUAKE,          "OB_QUAKE"          },
   { &s_SECRETMESSAGE,     "SECRETMESSAGE"     },
};

static size_t deh_numstrlookup = earrlen(deh_strlookup);

// level name tables

const char *deh_newlevel = "NEWLEVEL";

const char *mapnames[] =  // DOOM shareware/registered/retail (Ultimate) names.
{
  "HUSTR_E1M1",
  "HUSTR_E1M2",
  "HUSTR_E1M3",
  "HUSTR_E1M4",
  "HUSTR_E1M5",
  "HUSTR_E1M6",
  "HUSTR_E1M7",
  "HUSTR_E1M8",
  "HUSTR_E1M9",

  "HUSTR_E2M1",
  "HUSTR_E2M2",
  "HUSTR_E2M3",
  "HUSTR_E2M4",
  "HUSTR_E2M5",
  "HUSTR_E2M6",
  "HUSTR_E2M7",
  "HUSTR_E2M8",
  "HUSTR_E2M9",

  "HUSTR_E3M1",
  "HUSTR_E3M2",
  "HUSTR_E3M3",
  "HUSTR_E3M4",
  "HUSTR_E3M5",
  "HUSTR_E3M6",
  "HUSTR_E3M7",
  "HUSTR_E3M8",
  "HUSTR_E3M9",

  "HUSTR_E4M1",
  "HUSTR_E4M2",
  "HUSTR_E4M3",
  "HUSTR_E4M4",
  "HUSTR_E4M5",
  "HUSTR_E4M6",
  "HUSTR_E4M7",
  "HUSTR_E4M8",
  "HUSTR_E4M9",
};

const char *mapnames2[] = // DOOM 2 map names.
{
  "HUSTR_1",
  "HUSTR_2",
  "HUSTR_3",
  "HUSTR_4",
  "HUSTR_5",
  "HUSTR_6",
  "HUSTR_7",
  "HUSTR_8",
  "HUSTR_9",
  "HUSTR_10",
  "HUSTR_11",

  "HUSTR_12",
  "HUSTR_13",
  "HUSTR_14",
  "HUSTR_15",
  "HUSTR_16",
  "HUSTR_17",
  "HUSTR_18",
  "HUSTR_19",
  "HUSTR_20",

  "HUSTR_21",
  "HUSTR_22",
  "HUSTR_23",
  "HUSTR_24",
  "HUSTR_25",
  "HUSTR_26",
  "HUSTR_27",
  "HUSTR_28",
  "HUSTR_29",
  "HUSTR_30",

  "HUSTR_31",
  "HUSTR_32",
  "HUSTR_33"  // For Betray, in BFG Edition IWAD
};

const char *mapnamesp[] = // Plutonia WAD map names.
{
   "PHUSTR_1",
   "PHUSTR_2",
   "PHUSTR_3",
   "PHUSTR_4",
   "PHUSTR_5",
   "PHUSTR_6",
   "PHUSTR_7",
   "PHUSTR_8",
   "PHUSTR_9",
   "PHUSTR_10",
   "PHUSTR_11",

   "PHUSTR_12",
   "PHUSTR_13",
   "PHUSTR_14",
   "PHUSTR_15",
   "PHUSTR_16",
   "PHUSTR_17",
   "PHUSTR_18",
   "PHUSTR_19",
   "PHUSTR_20",

   "PHUSTR_21",
   "PHUSTR_22",
   "PHUSTR_23",
   "PHUSTR_24",
   "PHUSTR_25",
   "PHUSTR_26",
   "PHUSTR_27",
   "PHUSTR_28",
   "PHUSTR_29",
   "PHUSTR_30",

   "PHUSTR_31",
   "PHUSTR_32",
   "HUSTR_33"   // Can be used through BEX
};

const char *mapnamest[] = // TNT WAD map names.
{
   "THUSTR_1",
   "THUSTR_2",
   "THUSTR_3",
   "THUSTR_4",
   "THUSTR_5",
   "THUSTR_6",
   "THUSTR_7",
   "THUSTR_8",
   "THUSTR_9",
   "THUSTR_10",
   "THUSTR_11",

   "THUSTR_12",
   "THUSTR_13",
   "THUSTR_14",
   "THUSTR_15",
   "THUSTR_16",
   "THUSTR_17",
   "THUSTR_18",
   "THUSTR_19",
   "THUSTR_20",

   "THUSTR_21",
   "THUSTR_22",
   "THUSTR_23",
   "THUSTR_24",
   "THUSTR_25",
   "THUSTR_26",
   "THUSTR_27",
   "THUSTR_28",
   "THUSTR_29",
   "THUSTR_30",

   "THUSTR_31",
   "THUSTR_32",
   "HUSTR_33"   // Can be used through BEX
};

const char *mapnamesh[] = // haleyjd: heretic map names
{
   "HHUSTR_E1M1",
   "HHUSTR_E1M2",
   "HHUSTR_E1M3",
   "HHUSTR_E1M4",
   "HHUSTR_E1M5",
   "HHUSTR_E1M6",
   "HHUSTR_E1M7",
   "HHUSTR_E1M8",
   "HHUSTR_E1M9",

   "HHUSTR_E2M1",
   "HHUSTR_E2M2",
   "HHUSTR_E2M3",
   "HHUSTR_E2M4",
   "HHUSTR_E2M5",
   "HHUSTR_E2M6",
   "HHUSTR_E2M7",
   "HHUSTR_E2M8",
   "HHUSTR_E2M9",

   "HHUSTR_E3M1",
   "HHUSTR_E3M2",
   "HHUSTR_E3M3",
   "HHUSTR_E3M4",
   "HHUSTR_E3M5",
   "HHUSTR_E3M6",
   "HHUSTR_E3M7",
   "HHUSTR_E3M8",
   "HHUSTR_E3M9",

   "HHUSTR_E4M1",
   "HHUSTR_E4M2",
   "HHUSTR_E4M3",
   "HHUSTR_E4M4",
   "HHUSTR_E4M5",
   "HHUSTR_E4M6",
   "HHUSTR_E4M7",
   "HHUSTR_E4M8",
   "HHUSTR_E4M9",

   "HHUSTR_E5M1",
   "HHUSTR_E5M2",
   "HHUSTR_E5M3",
   "HHUSTR_E5M4",
   "HHUSTR_E5M5",
   "HHUSTR_E5M6",
   "HHUSTR_E5M7",
   "HHUSTR_E5M8",
   "HHUSTR_E5M9",
};


//
// BEX Codepointers
//

// External references to action functions scattered about the code

// This cheese stands alone!
void A_Aeon(actionargs_t *);

void A_Light0(actionargs_t *);
void A_WeaponReady(actionargs_t *);
void A_Lower(actionargs_t *);
void A_Raise(actionargs_t *);
void A_Punch(actionargs_t *);
void A_ReFire(actionargs_t *);
void A_FirePistol(actionargs_t *);
void A_Light1(actionargs_t *);
void A_FireShotgun(actionargs_t *);
void A_Light2(actionargs_t *);
void A_FireShotgun2(actionargs_t *);
void A_CheckReload(actionargs_t *);
void A_OpenShotgun2(actionargs_t *);
void A_LoadShotgun2(actionargs_t *);
void A_CloseShotgun2(actionargs_t *);
void A_FireCGun(actionargs_t *);
void A_GunFlash(actionargs_t *);
void A_FireMissile(actionargs_t *);
void A_Saw(actionargs_t *);
void A_FirePlasma(actionargs_t *);
void A_BFGsound(actionargs_t *);
void A_FireBFG(actionargs_t *);
void A_BFGSpray(actionargs_t *);
void A_Explode(actionargs_t *);
void A_Pain(actionargs_t *);
void A_PlayerScream(actionargs_t *);
void A_RavenPlayerScream(actionargs_t *);
void A_Fall(actionargs_t *);
void A_XScream(actionargs_t *);
void A_Look(actionargs_t *);
void A_Chase(actionargs_t *);
void A_FaceTarget(actionargs_t *);
void A_PosAttack(actionargs_t *);
void A_Scream(actionargs_t *);
void A_SPosAttack(actionargs_t *);
void A_VileChase(actionargs_t *);
void A_VileStart(actionargs_t *);
void A_VileTarget(actionargs_t *);
void A_VileAttack(actionargs_t *);
void A_StartFire(actionargs_t *);
void A_Fire(actionargs_t *);
void A_FireCrackle(actionargs_t *);
void A_Tracer(actionargs_t *);
void A_SkelWhoosh(actionargs_t *);
void A_SkelFist(actionargs_t *);
void A_SkelMissile(actionargs_t *);
void A_FatRaise(actionargs_t *);
void A_FatAttack1(actionargs_t *);
void A_FatAttack2(actionargs_t *);
void A_FatAttack3(actionargs_t *);
void A_BossDeath(actionargs_t *);
void A_CPosAttack(actionargs_t *);
void A_CPosRefire(actionargs_t *);
void A_TroopAttack(actionargs_t *);
void A_SargAttack(actionargs_t *);
void A_HeadAttack(actionargs_t *);
void A_BruisAttack(actionargs_t *);
void A_SkullAttack(actionargs_t *);
void A_Metal(actionargs_t *);
void A_SpidRefire(actionargs_t *);
void A_BabyMetal(actionargs_t *);
void A_BspiAttack(actionargs_t *);
void A_Hoof(actionargs_t *);
void A_CyberAttack(actionargs_t *);
void A_PainAttack(actionargs_t *);
void A_PainDie(actionargs_t *);
void A_KeenDie(actionargs_t *);
void A_BrainPain(actionargs_t *);
void A_BrainScream(actionargs_t *);
void A_BrainDie(actionargs_t *);
void A_BrainAwake(actionargs_t *);
void A_BrainSpit(actionargs_t *);
void A_SpawnSound(actionargs_t *);
void A_SpawnFly(actionargs_t *);
void A_BrainExplode(actionargs_t *);
void A_Detonate(actionargs_t *);        // killough 8/9/98
void A_Mushroom(actionargs_t *);        // killough 10/98
void A_Die(actionargs_t *);             // killough 11/98
void A_Spawn(actionargs_t *);           // killough 11/98
void A_Turn(actionargs_t *);            // killough 11/98
void A_Face(actionargs_t *);            // killough 11/98
void A_Scratch(actionargs_t *);         // killough 11/98
void A_PlaySound(actionargs_t *);       // killough 11/98
void A_RandomJump(actionargs_t *);      // killough 11/98
void A_LineEffect(actionargs_t *);      // killough 11/98
void A_Nailbomb(actionargs_t *);

// haleyjd: start new eternity action functions
void A_SpawnAbove(actionargs_t *);
void A_SpawnGlitter(actionargs_t *);
void A_SpawnEx(actionargs_t *);
void A_SetFlags(actionargs_t *);
void A_UnSetFlags(actionargs_t *);
void A_BetaSkullAttack(actionargs_t *);
void A_StartScript(actionargs_t *); // haleyjd 1/25/00: Script wrapper
void A_StartScriptNamed(actionargs_t *);
void A_PlayerStartScript(actionargs_t *);
void A_GenRefire(actionargs_t *);
void A_FireCustomBullets(actionargs_t *);
void A_FirePlayerMissile(actionargs_t *);
void A_CustomPlayerMelee(actionargs_t *);
void A_GenTracer(actionargs_t *);
void A_BFG11KHit(actionargs_t *);
void A_BouncingBFG(actionargs_t *);
void A_BFGBurst(actionargs_t *);
void A_FireOldBFG(actionargs_t *);
void A_KeepChasing(actionargs_t *);
void A_Stop(actionargs_t *);
void A_PlayerThunk(actionargs_t *);
void A_MissileAttack(actionargs_t *);
void A_MissileSpread(actionargs_t *);
void A_BulletAttack(actionargs_t *);
void A_HealthJump(actionargs_t *);
void A_CounterJump(actionargs_t *);
void A_CounterJumpEx(actionargs_t *);
void A_CounterSwitch(actionargs_t *);
void A_CounterSwitchEx(actionargs_t *);
void A_SetCounter(actionargs_t *);
void A_CopyCounter(actionargs_t *);
void A_CounterOp(actionargs_t *);
void A_CounterDiceRoll(actionargs_t *);
void A_SetTics(actionargs_t *);
void A_AproxDistance(actionargs_t *);
void A_ShowMessage(actionargs_t *);
void A_RandomWalk(actionargs_t *);
void A_TargetJump(actionargs_t *);
void A_ThingSummon(actionargs_t *);
void A_KillChildren(actionargs_t *);
void A_WeaponCtrJump(actionargs_t *);
void A_WeaponCtrJumpEx(actionargs_t *);
void A_WeaponCtrSwitch(actionargs_t *);
void A_WeaponSetCtr(actionargs_t *);
void A_WeaponCopyCtr(actionargs_t *);
void A_WeaponCtrOp(actionargs_t *);
void A_AmbientThinker(actionargs_t *);
void A_SteamSpawn(actionargs_t *);
void A_EjectCasing(actionargs_t *);
void A_CasingThrust(actionargs_t *);
void A_JumpIfNoAmmo(actionargs_t *);
void A_CheckReloadEx(actionargs_t *);
void A_DetonateEx(actionargs_t *);
void A_MushroomEx(actionargs_t *);
void A_HideThing(actionargs_t *);
void A_UnHideThing(actionargs_t *);
void A_RestoreArtifact(actionargs_t *);
void A_RestoreSpecialThing1(actionargs_t *);
void A_RestoreSpecialThing2(actionargs_t *);
void A_SargAttack12(actionargs_t *actionargs);
void A_SelfDestruct(actionargs_t *);
void A_TurnProjectile(actionargs_t *);
void A_SubtractAmmo(actionargs_t *);

// haleyjd 10/12/02: Heretic pointers
void A_SpawnTeleGlitter(actionargs_t *actionargs);
void A_SpawnTeleGlitter2(actionargs_t *);
void A_AccelGlitter(actionargs_t *);
void A_InitKeyGizmo(actionargs_t *actionargs);
void A_MummyAttack(actionargs_t *);
void A_MummyAttack2(actionargs_t *);
void A_MummySoul(actionargs_t *);
void A_HticDrop(actionargs_t *);
void A_HticTracer(actionargs_t *);
void A_MummyFX1Seek(actionargs_t *actionargs);
void A_ContMobjSound(actionargs_t *actionargs);
void A_ESound(actionargs_t *actionargs);
void A_ClinkAttack(actionargs_t *);
void A_GhostOff(actionargs_t *actionargs);
void A_WizardAtk1(actionargs_t *);
void A_WizardAtk2(actionargs_t *);
void A_WizardAtk3(actionargs_t *);
void A_SorZap(actionargs_t *);
void A_SorRise(actionargs_t *);
void A_SorDSph(actionargs_t *);
void A_SorDExp(actionargs_t *);
void A_SorDBon(actionargs_t *);
void A_SorSightSnd(actionargs_t *);
void A_Srcr2Decide(actionargs_t *);
void A_Srcr2Attack(actionargs_t *);
void A_BlueSpark(actionargs_t *);
void A_GenWizard(actionargs_t *);
void A_Sor2DthInit(actionargs_t *);
void A_Sor2DthLoop(actionargs_t *);
void A_HticExplode(actionargs_t *);
void A_HticBossDeath(actionargs_t *);
void A_PodPain(actionargs_t *);
void A_RemovePod(actionargs_t *);
void A_MakePod(actionargs_t *);
void A_KnightAttack(actionargs_t *);
void A_DripBlood(actionargs_t *);
void A_BeastAttack(actionargs_t *);
void A_BeastPuff(actionargs_t *);
void A_SnakeAttack(actionargs_t *);
void A_SnakeAttack2(actionargs_t *);
void A_Sor1Chase(actionargs_t *);
void A_Sor1Pain(actionargs_t *);
void A_Srcr1Attack(actionargs_t *);
void A_SorcererRise(actionargs_t *);
void A_VolcanoSet(actionargs_t *actionargs);
void A_VolcanoBlast(actionargs_t *);
void A_VolcBallImpact(actionargs_t *);
void A_MinotaurAtk1(actionargs_t *);
void A_MinotaurDecide(actionargs_t *);
void A_MinotaurAtk2(actionargs_t *);
void A_MinotaurAtk3(actionargs_t *);
void A_MinotaurCharge(actionargs_t *);
void A_MntrFloorFire(actionargs_t *);
void A_LichFire(actionargs_t *);
void A_LichWhirlwind(actionargs_t *);
void A_LichAttack(actionargs_t *);
void A_WhirlwindSeek(actionargs_t *);
void A_LichIceImpact(actionargs_t *);
void A_LichFireGrow(actionargs_t *);
void A_ImpChargeAtk(actionargs_t *);
void A_ImpMeleeAtk(actionargs_t *);
void A_ImpMissileAtk(actionargs_t *);
void A_ImpDeath(actionargs_t *);
void A_ImpXDeath1(actionargs_t *);
void A_ImpXDeath2(actionargs_t *);
void A_ImpExplode(actionargs_t *);
void A_PlayerSkull(actionargs_t *);
void A_FlameSnd(actionargs_t *actionargs);
void A_ClearSkin(actionargs_t *);
void A_PhoenixPuff(actionargs_t *);
void A_FlameEnd(actionargs_t *);
void A_FloatPuff(actionargs_t *);

// MaxW: 2018/01/02: Heretic weapon pointers
void A_StaffAttackPL1(actionargs_t *);
void A_StaffAttackPL2(actionargs_t *);
void A_FireGoldWandPL1(actionargs_t *);
void A_FireGoldWandPL2(actionargs_t *);
void A_FireMacePL1(actionargs_t *);
void A_MacePL1Check(actionargs_t *);
void A_MaceBallImpact(actionargs_t *);
void A_MaceBallImpact2(actionargs_t *);
void A_FireMacePL2(actionargs_t *);
void A_DeathBallImpact(actionargs_t *);
void A_FireCrossbowPL1(actionargs_t *);
void A_FireCrossbowPL2(actionargs_t *);
void A_BoltSpark(actionargs_t *);
void A_FireBlasterPL1(actionargs_t *);
void A_FireSkullRodPL1(actionargs_t *);
void A_FirePhoenixPL1(actionargs_t *);
void A_InitPhoenixPL2(actionargs_t *);
void A_FirePhoenixPL2(actionargs_t *);
void A_GauntletAttack(actionargs_t *);

// MaxW: 2018/01/02: Heretic artifact use pointers
void A_HticArtiTele(actionargs_t *);
void A_HticSpawnFireBomb(actionargs_t *);

// haleyjd 10/04/08: Hexen pointers
#if 0
void A_SetInvulnerable(Mobj *mo);
void A_UnSetInvulnerable(Mobj *mo);
void A_SetReflective(Mobj *mo);
void A_UnSetReflective(Mobj *mo);
void A_PigLook(Mobj *mo);
void A_PigChase(Mobj *mo);
void A_PigAttack(Mobj *mo);
void A_PigPain(Mobj *mo);
void A_HexenExplode(Mobj *mo);
void A_SerpentUnHide(Mobj *mo);
void A_SerpentHide(Mobj *mo);
void A_SerpentChase(Mobj *mo);
void A_RaiseFloorClip(Mobj *mo);
void A_LowerFloorClip(Mobj *mo);
void A_SerpentHumpDecide(Mobj *mo);
void A_SerpentWalk(Mobj *mo);
void A_SerpentCheckForAttack(Mobj *mo);
void A_SerpentChooseAttack(Mobj *mo);
void A_SerpentMeleeAttack(Mobj *mo);
void A_SerpentMissileAttack(Mobj *mo);
void A_SerpentSpawnGibs(Mobj *mo);
void A_SubTics(Mobj *mo);
void A_SerpentHeadCheck(Mobj *mo);
void A_CentaurAttack(Mobj *mo);
void A_CentaurAttack2(Mobj *mo);
void A_DropEquipment(Mobj *mo);
void A_CentaurDefend(Mobj *mo);
void A_BishopAttack(Mobj *mo);
void A_BishopAttack2(Mobj *mo);
void A_BishopMissileWeave(Mobj *mo);
void A_BishopDoBlur(Mobj *mo);
void A_SpawnBlur(Mobj *mo);
void A_BishopChase(Mobj *mo);
void A_BishopPainBlur(Mobj *mo);
void A_DragonInitFlight(Mobj *mo);
void A_DragonFlight(Mobj *mo);
void A_DragonFlap(Mobj *mo);
void A_DragonFX2(Mobj *mo);
void A_PainCounterBEQ(Mobj *mo);
void A_DemonAttack1(Mobj *mo);
void A_DemonAttack2(Mobj *mo);
void A_DemonDeath(Mobj *mo);
void A_Demon2Death(Mobj *mo);
void A_WraithInit(Mobj *mo);
void A_WraithRaiseInit(Mobj *mo);
void A_WraithRaise(Mobj *mo);
void A_WraithMelee(Mobj *mo);
void A_WraithMissile(Mobj *mo);
void A_WraithFX2(Mobj *mo);
void A_WraithFX3(Mobj *mo);
void A_WraithFX4(Mobj *mo);
void A_WraithLook(Mobj *mo);
void A_WraithChase(Mobj *mo);
void A_EttinAttack(Mobj *mo);
void A_DropMace(Mobj *mo);
void A_AffritSpawnRock(Mobj *mo);
void A_AffritRocks(Mobj *mo);
void A_SmBounce(Mobj *mo);
void A_AffritChase(Mobj *mo);
void A_AffritSplotch(Mobj *mo);
void A_IceGuyLook(Mobj *mo);
void A_IceGuyChase(Mobj *mo);
void A_IceGuyAttack(Mobj *mo);
void A_IceGuyDie(Mobj *mo);
void A_IceGuyMissileExplode(Mobj *mo);
void A_CheckFloor(Mobj *mo);
void A_FreezeDeath(Mobj *mo);
void A_IceSetTics(Mobj *mo);
void A_IceCheckHeadDone(Mobj *mo);
void A_FreezeDeathChunks(Mobj *mo);
#endif

// zdoom-inspired pointers
void A_JumpIfTargetInLOS(actionargs_t *);
void A_SetTranslucent(actionargs_t *);
void A_AlertMonsters(actionargs_t *);
void A_CheckPlayerDone(actionargs_t *);
void A_FadeIn(actionargs_t *);
void A_FadeOut(actionargs_t *);
void A_PlaySoundEx(actionargs_t *);
void A_SetSpecial(actionargs_t *);
void A_Jump(actionargs_t *);
void A_SeekerMissile(actionargs_t *);

// eternity tc ptrs: TODO: remove these?
void A_FogSpawn(actionargs_t *);
void A_FogMove(actionargs_t *);

// haleyjd 07/13/03: special death actions for killem cheat
void A_PainNukeSpec(actionargs_t *);
void A_SorcNukeSpec(actionargs_t *);

// haleyjd 03/14/03: moved here, added hashing, eliminated useless
// A_ prefixes on mnemonics

// haleyjd 10/04/08: macros to save my wrists
#define POINTER(name)  { A_ ## name , #name , -1 }

deh_bexptr deh_bexptrs[] =
{
   POINTER(Light0),
   POINTER(WeaponReady),
   POINTER(Lower),
   POINTER(Raise),
   POINTER(Punch),
   POINTER(ReFire),
   POINTER(FirePistol),
   POINTER(Light1),
   POINTER(FireShotgun),
   POINTER(Light2),
   POINTER(FireShotgun2),
   POINTER(CheckReload),
   POINTER(OpenShotgun2),
   POINTER(LoadShotgun2),
   POINTER(CloseShotgun2),
   POINTER(FireCGun),
   POINTER(GunFlash),
   POINTER(FireMissile),
   POINTER(Saw),
   POINTER(FirePlasma),
   POINTER(BFGsound),
   POINTER(FireBFG),
   POINTER(BFGSpray),
   POINTER(Explode),
   POINTER(Pain),
   POINTER(PlayerScream),
   POINTER(RavenPlayerScream),
   POINTER(Fall),
   POINTER(XScream),
   POINTER(Look),
   POINTER(Chase),
   POINTER(FaceTarget),
   POINTER(PosAttack),
   POINTER(Scream),
   POINTER(SPosAttack),
   POINTER(VileChase),
   POINTER(VileStart),
   POINTER(VileTarget),
   POINTER(VileAttack),
   POINTER(StartFire),
   POINTER(Fire),
   POINTER(FireCrackle),
   POINTER(Tracer),
   POINTER(SkelWhoosh),
   POINTER(SkelFist),
   POINTER(SkelMissile),
   POINTER(FatRaise),
   POINTER(FatAttack1),
   POINTER(FatAttack2),
   POINTER(FatAttack3),
   POINTER(BossDeath),
   POINTER(CPosAttack),
   POINTER(CPosRefire),
   POINTER(TroopAttack),
   POINTER(SargAttack),
   POINTER(HeadAttack),
   POINTER(BruisAttack),
   POINTER(SkullAttack),
   POINTER(Metal),
   POINTER(SpidRefire),
   POINTER(BabyMetal),
   POINTER(BspiAttack),
   POINTER(Hoof),
   POINTER(CyberAttack),
   POINTER(PainAttack),
   POINTER(PainDie),
   POINTER(KeenDie),
   POINTER(BrainPain),
   POINTER(BrainScream),
   POINTER(BrainDie),
   POINTER(BrainAwake),
   POINTER(BrainSpit),
   POINTER(SpawnSound),
   POINTER(SpawnFly),
   POINTER(BrainExplode),
  
   // killough 8/9/98 - 11/98: MBF Pointers  
   POINTER(Detonate),
   POINTER(Mushroom),
   POINTER(Die),
   POINTER(Spawn),
   POINTER(Turn),
   POINTER(Face),
   POINTER(Scratch),
   POINTER(PlaySound),
   POINTER(RandomJump),
   POINTER(LineEffect),

   POINTER(Nailbomb),

   // haleyjd: start new eternity codeptrs
   POINTER(SpawnAbove),
   POINTER(SpawnGlitter),
   POINTER(SpawnEx),
   POINTER(StartScript),
   POINTER(StartScriptNamed),
   POINTER(PlayerStartScript),
   POINTER(SetFlags),
   POINTER(UnSetFlags),
   POINTER(BetaSkullAttack),
   POINTER(GenRefire),
   POINTER(FireCustomBullets),
   POINTER(FirePlayerMissile),
   POINTER(CustomPlayerMelee),
   POINTER(GenTracer),
   POINTER(BFG11KHit),
   POINTER(BouncingBFG),
   POINTER(BFGBurst),
   POINTER(FireOldBFG),
   POINTER(KeepChasing),
   POINTER(Stop),
   POINTER(PlayerThunk),
   POINTER(MissileAttack),
   POINTER(MissileSpread),
   POINTER(BulletAttack),
   POINTER(HealthJump),
   POINTER(CounterJump),
   POINTER(CounterJumpEx),
   POINTER(CounterSwitch),
   POINTER(CounterSwitchEx),
   POINTER(SetCounter),
   POINTER(CopyCounter),
   POINTER(CounterOp),
   POINTER(CounterDiceRoll),
   POINTER(SetTics),
   POINTER(AproxDistance),
   POINTER(ShowMessage),
   POINTER(RandomWalk),
   POINTER(TargetJump),
   POINTER(ThingSummon),
   POINTER(KillChildren),
   POINTER(WeaponCtrJump),
   POINTER(WeaponCtrJumpEx),
   POINTER(WeaponCtrSwitch),
   POINTER(WeaponSetCtr),
   POINTER(WeaponCopyCtr),
   POINTER(WeaponCtrOp),
   POINTER(AmbientThinker),
   POINTER(SteamSpawn),
   POINTER(EjectCasing),
   POINTER(CasingThrust),
   POINTER(JumpIfNoAmmo),
   POINTER(CheckReloadEx),
   POINTER(DetonateEx),
   POINTER(MushroomEx),
   POINTER(HideThing),
   POINTER(UnHideThing),
   POINTER(RestoreArtifact),
   POINTER(RestoreSpecialThing1),
   POINTER(RestoreSpecialThing2),
   POINTER(SargAttack12),
   POINTER(SelfDestruct),
   POINTER(TurnProjectile),
   POINTER(SubtractAmmo),

   // haleyjd 07/13/03: nuke specials
   POINTER(PainNukeSpec),
   POINTER(SorcNukeSpec),

   // haleyjd: Heretic pointers
   POINTER(SpawnTeleGlitter),
   POINTER(SpawnTeleGlitter2),
   POINTER(AccelGlitter),
   POINTER(InitKeyGizmo),
   POINTER(MummyAttack),
   POINTER(MummyAttack2),
   POINTER(MummySoul),
   POINTER(HticDrop),
   POINTER(HticTracer),
   POINTER(MummyFX1Seek),
   POINTER(ContMobjSound),
   POINTER(ESound),
   POINTER(ClinkAttack),
   POINTER(GhostOff),
   POINTER(WizardAtk1),
   POINTER(WizardAtk2),
   POINTER(WizardAtk3),
   POINTER(SorZap),
   POINTER(SorRise),
   POINTER(SorDSph),
   POINTER(SorDExp),
   POINTER(SorDBon),
   POINTER(SorSightSnd),
   POINTER(Srcr2Decide),
   POINTER(Srcr2Attack),
   POINTER(BlueSpark),
   POINTER(GenWizard),
   POINTER(Sor2DthInit),
   POINTER(Sor2DthLoop),
   POINTER(HticExplode),
   POINTER(HticBossDeath),
   POINTER(PodPain),
   POINTER(RemovePod),
   POINTER(MakePod),
   POINTER(KnightAttack),
   POINTER(DripBlood),
   POINTER(BeastAttack),
   POINTER(BeastPuff),
   POINTER(SnakeAttack),
   POINTER(SnakeAttack2),
   POINTER(Sor1Chase),
   POINTER(Sor1Pain),
   POINTER(Srcr1Attack),
   POINTER(SorcererRise),
   POINTER(VolcanoSet),
   POINTER(VolcanoBlast),
   POINTER(VolcBallImpact),
   POINTER(MinotaurAtk1),
   POINTER(MinotaurDecide),
   POINTER(MinotaurAtk2),
   POINTER(MinotaurAtk3),
   POINTER(MinotaurCharge),
   POINTER(MntrFloorFire),
   POINTER(LichFire),
   POINTER(LichWhirlwind),
   POINTER(LichAttack),
   POINTER(WhirlwindSeek),
   POINTER(LichIceImpact),
   POINTER(LichFireGrow),
   POINTER(ImpChargeAtk),
   POINTER(ImpMeleeAtk),
   POINTER(ImpMissileAtk),
   POINTER(ImpDeath),
   POINTER(ImpXDeath1),
   POINTER(ImpXDeath2),
   POINTER(ImpExplode),
   POINTER(PlayerSkull),
   POINTER(FlameSnd),
   POINTER(ClearSkin),
   POINTER(PhoenixPuff),
   POINTER(FlameEnd),
   POINTER(FloatPuff),

   // MaxW: 2018/01/02: Heretic weapon pointers
   POINTER(StaffAttackPL1),
   POINTER(StaffAttackPL2),
   POINTER(FireGoldWandPL1),
   POINTER(FireGoldWandPL2),
   POINTER(FireMacePL1),
   POINTER(MacePL1Check),
   POINTER(MaceBallImpact),
   POINTER(MaceBallImpact2),
   POINTER(FireMacePL2),
   POINTER(DeathBallImpact),
   POINTER(FireCrossbowPL1),
   POINTER(FireCrossbowPL2),
   POINTER(BoltSpark),
   POINTER(FireBlasterPL1),
   POINTER(FireSkullRodPL1),
   POINTER(FirePhoenixPL1),
   POINTER(InitPhoenixPL2),
   POINTER(FirePhoenixPL2),
   POINTER(GauntletAttack),

   // MaxW: 2018/01/02: Heretic artifact use pointers
   POINTER(HticArtiTele),
   POINTER(HticSpawnFireBomb),

   // haleyjd 10/04/08: Hexen pointers
#if 0
   POINTER(SetInvulnerable),
   POINTER(UnSetInvulnerable),
   POINTER(SetReflective),
   POINTER(UnSetReflective),
   POINTER(PigLook),
   POINTER(PigChase),
   POINTER(PigAttack),
   POINTER(PigPain),
   POINTER(HexenExplode),
   POINTER(SerpentUnHide),
   POINTER(SerpentHide),
   POINTER(SerpentChase),
   POINTER(RaiseFloorClip),
   POINTER(LowerFloorClip),
   POINTER(SerpentHumpDecide),
   POINTER(SerpentWalk),
   POINTER(SerpentCheckForAttack),
   POINTER(SerpentChooseAttack),
   POINTER(SerpentMeleeAttack),
   POINTER(SerpentMissileAttack),
   POINTER(SerpentSpawnGibs),
   POINTER(SubTics),
   POINTER(SerpentHeadCheck),
   POINTER(CentaurAttack),
   POINTER(CentaurAttack2),
   POINTER(DropEquipment),
   POINTER(CentaurDefend),
   POINTER(BishopAttack),
   POINTER(BishopAttack2),
   POINTER(BishopMissileWeave),
   POINTER(BishopDoBlur),
   POINTER(SpawnBlur),
   POINTER(BishopChase),
   POINTER(BishopPainBlur),
   POINTER(DragonInitFlight),
   POINTER(DragonFlight),
   POINTER(DragonFlap),
   POINTER(DragonFX2),
   POINTER(PainCounterBEQ),
   POINTER(DemonAttack1),
   POINTER(DemonAttack2),
   POINTER(DemonDeath),
   POINTER(Demon2Death),
   POINTER(WraithInit),
   POINTER(WraithRaiseInit),
   POINTER(WraithRaise),
   POINTER(WraithMelee),
   POINTER(WraithMissile),
   POINTER(WraithFX2),
   POINTER(WraithFX3),
   POINTER(WraithFX4),
   POINTER(WraithLook),
   POINTER(WraithChase),
   POINTER(EttinAttack),
   POINTER(DropMace),
   POINTER(AffritSpawnRock),
   POINTER(AffritRocks),
   POINTER(SmBounce),
   POINTER(AffritChase),
   POINTER(AffritSplotch),
   POINTER(IceGuyLook),
   POINTER(IceGuyChase),
   POINTER(IceGuyAttack),
   POINTER(IceGuyDie),
   POINTER(IceGuyMissileExplode),
   POINTER(CheckFloor),
   POINTER(FreezeDeath),
   POINTER(IceSetTics),
   POINTER(IceCheckHeadDone),
   POINTER(FreezeDeathChunks),
#endif

   // zdoom-inspired pointers
   POINTER(AlertMonsters),
   POINTER(CheckPlayerDone),
   POINTER(FadeIn),
   POINTER(FadeOut),
   POINTER(JumpIfTargetInLOS),
   POINTER(PlaySoundEx),
   POINTER(SetSpecial),
   POINTER(SetTranslucent),
   POINTER(Jump),
   POINTER(SeekerMissile),

   // ETERNITY TC ptrs -- TODO: eliminate these
   POINTER(FogSpawn),
   POINTER(FogMove),

   // This nullptr entry must be the last in the list
   { nullptr, "NULL" } // Ty 05/16/98
};

// haleyjd 03/14/03: Just because its null-terminated doesn't mean 
// we can index it however the hell we want! See deh_procPointer in
// d_deh.c for a big bug fix.

int num_bexptrs = earrlen(deh_bexptrs);

//
// Shared Hash functions
//

//
// D_HashTableKey
//
// Fairly standard key computation -- this is used for multiple 
// tables so there's not much use trying to make it perfect. It'll 
// save time anyways.
// 08/28/03: vastly simplified, is now similar to SGI's STL hash
// 08/23/13: rewritten to use sdbm hash algorithm
//
unsigned int D_HashTableKey(const char *str)
{
   auto ustr = reinterpret_cast<const unsigned char *>(str);
   int c;
   unsigned int h = 0;

#ifdef RANGECHECK
   if(!ustr)
      I_Error("D_HashTableKey: cannot hash NULL string!\n");
#endif

   // note: this needs to be case insensitive for EDF mnemonics
   while((c = *ustr++))
      h = ectype::toUpper(c) + (h << 6) + (h << 16) - h;

   return h;
}

//
// D_HashTableKeyCase
//
// haleyjd 09/09/09: as above, but case-sensitive
//
unsigned int D_HashTableKeyCase(const char *str)
{
   auto ustr = reinterpret_cast<const unsigned char *>(str);
   int c;
   unsigned int h = 0;

#ifdef RANGECHECK
   if(!ustr)
      I_Error("D_HashTableKeyCase: cannot hash NULL string!\n");
#endif

   while((c = *ustr++))
      h = c + (h << 6) + (h << 16) - h;

   return h;
}

//
// BEX String Hash Table
//
// The deh_strs table has two independent hash chains through it,
// one for lookup by mnemonic (BEX style), and one for lookup by
// value (DEH style).
//

#define NUMSTRCHAINS (earrlen(deh_strlookup) + 61)
static size_t bexstrhashchains[NUMSTRCHAINS];
static size_t dehstrhashchains[NUMSTRCHAINS];

static void D_DEHStrHashInit()
{
   for(size_t i = 0; i < NUMSTRCHAINS; i++)
   {
      bexstrhashchains[i] = deh_numstrlookup;
      dehstrhashchains[i] = deh_numstrlookup;
   }

   for(size_t i = 0; i < deh_numstrlookup; i++)
   {
      unsigned int bkey, dkey;

      // key for bex mnemonic
      bkey = D_HashTableKey(deh_strlookup[i].lookup) % NUMSTRCHAINS;

      // key for actual string value
      dkey = D_HashTableKey(*(deh_strlookup[i].ppstr)) % NUMSTRCHAINS;

      deh_strlookup[i].bnext = bexstrhashchains[bkey];      
      bexstrhashchains[bkey] = i;

      deh_strlookup[i].dnext = dehstrhashchains[dkey];
      dehstrhashchains[dkey] = i;

      // haleyjd 10/08/06: save pointer to original string
      deh_strlookup[i].original = *(deh_strlookup[i].ppstr);
   }
}

//
// D_GetBEXStr
//
// Finds the entry in the table above given a BEX mnemonic to
// look for
//
dehstr_t *D_GetBEXStr(const char *string)
{
   unsigned int key;
   dehstr_t *dehstr;

   // compute key for string
   key = D_HashTableKey(string) % NUMSTRCHAINS;

   // hash chain empty -- not found
   if(bexstrhashchains[key] == deh_numstrlookup)
      return nullptr;

   dehstr = &deh_strlookup[bexstrhashchains[key]];

   // while string doesn't match lookup, go down hash chain
   while(strcasecmp(string, dehstr->lookup))
   {
      // end of hash chain -- not found
      if(dehstr->bnext == deh_numstrlookup)
         return nullptr;
      else
         dehstr = &deh_strlookup[dehstr->bnext];
   }

   return dehstr;
}

//
// D_GetDEHStr
//
// Finds the entry in the table above given the actual string value to look for.
// haleyjd 10/08/06: altered to always use original value of string for 
// comparisons in order to support proper DeHackEd string replacement logic.
//
dehstr_t *D_GetDEHStr(const char *string)
{
   unsigned int key;
   dehstr_t *dehstr;

   // compute key for string
   key = D_HashTableKey(string) % NUMSTRCHAINS;

   // hash chain empty -- not found
   if(dehstrhashchains[key] == deh_numstrlookup)
      return nullptr;

   dehstr = &deh_strlookup[dehstrhashchains[key]];

   // while string doesn't match lookup, go down hash chain
   while(strcasecmp(dehstr->original, string))
   {
      // end of hash chain -- not found
      if(dehstr->dnext == deh_numstrlookup)
         return nullptr;
      else
         dehstr = &deh_strlookup[dehstr->dnext];
   }

   return dehstr;
}

//
// DEH_String
//
// haleyjd 10/08/06: Inspired by Chocolate Doom. Allows the game engine to get
// DEH/BEX strings by their DEH/BEX mnemonic instead of through a global string
// variable. The eventual goal is to have everything use this and to eliminate
// the global variables altogether.
//
const char *DEH_String(const char *mnemonic)
{
   dehstr_t *dehstr;

   // allow nullptr mnemonic to return nullptr
   if(mnemonic == nullptr)
      return nullptr;

   // 05/31/08: modified to return mnemonic on unknown string
   if(!(dehstr = D_GetBEXStr(mnemonic)))
      return mnemonic;

   return *(dehstr->ppstr);
}

//
// DEH_StringChanged
//
// haleyjd 10/08/06: Similar to the above, this function reports on whether or
// not a string has been edited by DEH/BEX.
//
bool DEH_StringChanged(const char *mnemonic)
{
   dehstr_t *dehstr;

   if(!(dehstr = D_GetBEXStr(mnemonic)))
      I_Error("DEH_StringChanged: unknown BEX mnemonic %s\n", mnemonic);

   return (dehstr->original != *(dehstr->ppstr));
}

//
// DEH_ReplaceString
//
// haleyjd 04/29/13: Allows replacing a dehstr at runtime.
// newstr should be a static string, or one you allocated storage for
// yourself.
//
void DEH_ReplaceString(const char *mnemonic, const char *newstr)
{
   dehstr_t *dehstr;

   if(!(dehstr = D_GetBEXStr(mnemonic)))
      return;

   *dehstr->ppstr = newstr;
}

//
// BEX Code Pointer Hash Table
//

#define NUMCPTRCHAINS (earrlen(deh_bexptrs) + 35)

static int bexcpchains[NUMCPTRCHAINS];

static void D_BEXPtrHashInit()
{
   int i;

   for(i = 0; i < static_cast<int>(NUMCPTRCHAINS); i++)
      bexcpchains[i] = -1;

   for(i = 0; i < num_bexptrs; i++)
   {
      unsigned int key;
      
      key = D_HashTableKey(deh_bexptrs[i].lookup) % NUMCPTRCHAINS;

      deh_bexptrs[i].next = bexcpchains[key];
      bexcpchains[key] = i;
   }
}

deh_bexptr *D_GetBexPtr(const char *mnemonic)
{
   unsigned int key;
   deh_bexptr *bexptr;

   // 6/19/09: allow optional A_ prefix in all contexts.
   if(strlen(mnemonic) > 2 && !strncasecmp(mnemonic, "A_", 2))
      mnemonic += 2;

   // calculate key for mnemonic
   key = D_HashTableKey(mnemonic) % NUMCPTRCHAINS;

   // is chain empty?
   if(bexcpchains[key] == -1)
      return nullptr; // doesn't exist

   bexptr = &deh_bexptrs[bexcpchains[key]];

   while(strcasecmp(mnemonic, bexptr->lookup))
   {
      // end of hash chain?
      if(bexptr->next == -1)
         return nullptr; // doesn't exist
      else
         bexptr = &deh_bexptrs[bexptr->next];
   }

   return bexptr;
}

//
// D_BuildBEXHashChains
//
// haleyjd 03/14/03: with the addition of EDF, it has become necessary
// to separate internal table hash chain building from the dynamic
// sprites/music/sounds lookup table building below. This function
// must be called before E_ProcessEDF, whereas the one below must
// be called afterward.
//
void D_BuildBEXHashChains()
{
   // build string hash chains
   D_DEHStrHashInit();

   // bex codepointer table
   D_BEXPtrHashInit();
}

// haleyjd: support for BEX SPRITES, SOUNDS, and MUSIC
char **deh_spritenames;
char **deh_musicnames;

//
// D_BuildBEXTables
//
// estrdup's string values in the sprites and music
// tables for use as unchanging mnemonics. Sounds now
// store their own mnemonics and thus a table for them
// is unnecessary.
//
void D_BuildBEXTables()
{
   char *spritestr;
   char *musicstr;
   int i;

   // build sprites, music lookups

   // haleyjd 03/11/03: must be dynamic now
   // 10/17/03: allocate all the names through a single pointer
   spritestr = ecalloctag(char *, NUMSPRITES, 5, PU_STATIC, nullptr);

   deh_spritenames = emalloctag(char **, (NUMSPRITES+1)*sizeof(char *),PU_STATIC, nullptr);

   for(i = 0; i < NUMSPRITES; ++i)
   {
      deh_spritenames[i] = spritestr + i * 5;
      strncpy(deh_spritenames[i], sprnames[i], 4);
   }
   deh_spritenames[NUMSPRITES] = nullptr;

   // 09/07/05: allocate all music names through one pointer
   musicstr = ecalloctag(char *, NUMMUSIC, 7, PU_STATIC, nullptr);

   deh_musicnames = emalloctag(char **, (NUMMUSIC+1)*sizeof(char *), PU_STATIC, nullptr);

   for(i = 1; i < NUMMUSIC; ++i)
   {
      deh_musicnames[i] = musicstr + i * 7;
      strncpy(deh_musicnames[i], S_music[i].name, 6);
   }
   deh_musicnames[0] = deh_musicnames[NUMMUSIC] = nullptr;
}

//
// DeHackEd File Queue
//

// haleyjd: All DeHackEd files and lumps are now queued as they
// are encountered, and then processed in the appropriate order
// all at once. This allows EDF to be extended for loading from
// wad files.

struct dehqueueitem_t
{
   mqueueitem_t mqitem; // this must be first

   char name[PATH_MAX+1];
   int  lumpnum;
};

static mqueue_t dehqueue;

void D_DEHQueueInit()
{
   M_QueueInit(&dehqueue);
}

//
// D_QueueDEH
//
// Adds a dehqueue_t to the DeHackEd file/lump queue, so that all
// DeHackEd files/lumps can be parsed at once in the proper order.
//
void D_QueueDEH(const char *filename, int lumpnum)
{
   // allocate a new dehqueue_t
   dehqueueitem_t *newdq = ecalloc(dehqueueitem_t *, 1, sizeof(dehqueueitem_t));

   // if filename is valid, this is a file DEH
   if(filename)
   {
      strncpy(newdq->name, filename, PATH_MAX+1);
      newdq->lumpnum = -1;
   }
   else
   {
      // otherwise, this is a wad dehacked lump
      newdq->lumpnum = lumpnum;
   }

   M_QueueInsert(&(newdq->mqitem), &dehqueue);
}

// DeHackEd support - Ty 03/09/97
// killough 10/98:
// Add lump number as third argument, for use when filename==nullptr
void ProcessDehFile(const char *filename, const char *outfilename, int lump);

// killough 10/98: support -dehout filename
// cph - made const, don't cache results
// haleyjd 09/11/03: moved here from d_main.c
static const char *D_dehout()
{
   int p = M_CheckParm("-dehout");

   if(!p)
      p = M_CheckParm("-bexout");
   
   return (p && ++p < myargc) ? myargv[p] : nullptr;
}

//
// D_ProcessDEHQueue
//
// Processes all the DeHackEd/BEX files queued during startup, including
// command-line DEHs, GFS DEHs, preincluded DEHs, and in-wad DEHs.
//
void D_ProcessDEHQueue()
{
   // Start at the head node and process each DeHackEd -- the queue
   // has preserved the proper processing order.

   mqueueitem_t *rover;

   while((rover = M_QueueIterator(&dehqueue)))
   {
      dehqueueitem_t *dqitem = (dehqueueitem_t *)rover;

      // if lumpnum != -1, this is a wad dehacked lump, otherwise 
      // it's a file
      if(dqitem->lumpnum != -1)
      {
         ProcessDehFile(nullptr, D_dehout(), dqitem->lumpnum);
      }
      else
      {
         ProcessDehFile(dqitem->name, D_dehout(), 0);
      }
   }

   // free the elements and reset the queue
   M_QueueFree(&dehqueue);
}

// EOF


