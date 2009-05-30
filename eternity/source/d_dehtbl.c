// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2002 James Haley
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
// DeHackEd information tables / hashing
//
// Separated out from d_deh.c to avoid clutter and speed up the
// editing of that file. Routines for internal table hash chaining
// and initialization are also here.
//
//--------------------------------------------------------------------------

#include "z_zone.h"
#include "m_argv.h"
#include "m_fixed.h"
#include "d_io.h"
#include "doomdef.h"
#include "doomtype.h"
#include "dstrings.h"  // to get initial text values
#include "dhticstr.h"  // haleyjd
#include "d_dehtbl.h"
#include "m_queue.h"
#include "d_mod.h"
#include "e_lib.h"

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

char *s_D_DEVSTR    = D_DEVSTR;
char *s_D_CDROM     = D_CDROM;
char *s_PRESSKEY    = PRESSKEY;
char *s_PRESSYN     = PRESSYN;
char *s_QUITMSG     = "";    // sf: optional quitmsg replacement
char *s_LOADNET     = LOADNET;   // PRESSKEY; // killough 4/4/98:
char *s_QLOADNET    = QLOADNET;  // PRESSKEY;
char *s_QSAVESPOT   = QSAVESPOT; // PRESSKEY;
char *s_SAVEDEAD    = SAVEDEAD;  // PRESSKEY; // remove duplicate y/n
char *s_QSPROMPT    = QSPROMPT;  // PRESSYN;
char *s_QLPROMPT    = QLPROMPT;  // PRESSYN;
char *s_NEWGAME     = NEWGAME;   // PRESSKEY;
char *s_NIGHTMARE   = NIGHTMARE; // PRESSYN;
char *s_SWSTRING    = SWSTRING;  // PRESSKEY;
char *s_MSGOFF      = MSGOFF;
char *s_MSGON       = MSGON;
char *s_NETEND      = NETEND;    // PRESSKEY;
char *s_ENDGAME     = ENDGAME;   // PRESSYN; // killough 4/4/98: end
char *s_DOSY        = DOSY;
// joel - detail level strings ditched
char *s_GAMMALVL0   = GAMMALVL0;
char *s_GAMMALVL1   = GAMMALVL1;
char *s_GAMMALVL2   = GAMMALVL2;
char *s_GAMMALVL3   = GAMMALVL3;
char *s_GAMMALVL4   = GAMMALVL4;
char *s_EMPTYSTRING = EMPTYSTRING;
char *s_GOTARMOR    = GOTARMOR;
char *s_GOTMEGA     = GOTMEGA;
char *s_GOTHTHBONUS = GOTHTHBONUS;
char *s_GOTARMBONUS = GOTARMBONUS;
char *s_GOTSTIM     = GOTSTIM;
char *s_GOTMEDINEED = GOTMEDINEED;
char *s_GOTMEDIKIT  = GOTMEDIKIT;
char *s_GOTSUPER    = GOTSUPER;
char *s_GOTBLUECARD = GOTBLUECARD;
char *s_GOTYELWCARD = GOTYELWCARD;
char *s_GOTREDCARD  = GOTREDCARD;
char *s_GOTBLUESKUL = GOTBLUESKUL;
char *s_GOTYELWSKUL = GOTYELWSKUL;
char *s_GOTREDSKULL = GOTREDSKULL;
char *s_GOTINVUL    = GOTINVUL;
char *s_GOTBERSERK  = GOTBERSERK;
char *s_GOTINVIS    = GOTINVIS;
char *s_GOTSUIT     = GOTSUIT;
char *s_GOTMAP      = GOTMAP;
char *s_GOTVISOR    = GOTVISOR;
char *s_GOTMSPHERE  = GOTMSPHERE;
char *s_GOTCLIP     = GOTCLIP;
char *s_GOTCLIPBOX  = GOTCLIPBOX;
char *s_GOTROCKET   = GOTROCKET;
char *s_GOTROCKBOX  = GOTROCKBOX;
char *s_GOTCELL     = GOTCELL;
char *s_GOTCELLBOX  = GOTCELLBOX;
char *s_GOTSHELLS   = GOTSHELLS;
char *s_GOTSHELLBOX = GOTSHELLBOX;
char *s_GOTBACKPACK = GOTBACKPACK;
char *s_GOTBFG9000  = GOTBFG9000;
char *s_GOTCHAINGUN = GOTCHAINGUN;
char *s_GOTCHAINSAW = GOTCHAINSAW;
char *s_GOTLAUNCHER = GOTLAUNCHER;
char *s_GOTPLASMA   = GOTPLASMA;
char *s_GOTSHOTGUN  = GOTSHOTGUN;
char *s_GOTSHOTGUN2 = GOTSHOTGUN2;
char *s_PD_BLUEO    = PD_BLUEO;
char *s_PD_REDO     = PD_REDO;
char *s_PD_YELLOWO  = PD_YELLOWO;
char *s_PD_BLUEK    = PD_BLUEK;
char *s_PD_REDK     = PD_REDK;
char *s_PD_YELLOWK  = PD_YELLOWK;
char *s_PD_BLUEC    = PD_BLUEC;
char *s_PD_REDC     = PD_REDC;
char *s_PD_YELLOWC  = PD_YELLOWC;
char *s_PD_BLUES    = PD_BLUES;
char *s_PD_REDS     = PD_REDS;
char *s_PD_YELLOWS  = PD_YELLOWS;
char *s_PD_ANY      = PD_ANY;
char *s_PD_ALL3     = PD_ALL3;
char *s_PD_ALL6     = PD_ALL6;
char *s_GGSAVED     = GGSAVED;
char *s_HUSTR_MSGU  = HUSTR_MSGU;
char *s_HUSTR_E1M1  = HUSTR_E1M1;
char *s_HUSTR_E1M2  = HUSTR_E1M2;
char *s_HUSTR_E1M3  = HUSTR_E1M3;
char *s_HUSTR_E1M4  = HUSTR_E1M4;
char *s_HUSTR_E1M5  = HUSTR_E1M5;
char *s_HUSTR_E1M6  = HUSTR_E1M6;
char *s_HUSTR_E1M7  = HUSTR_E1M7;
char *s_HUSTR_E1M8  = HUSTR_E1M8;
char *s_HUSTR_E1M9  = HUSTR_E1M9;
char *s_HUSTR_E2M1  = HUSTR_E2M1;
char *s_HUSTR_E2M2  = HUSTR_E2M2;
char *s_HUSTR_E2M3  = HUSTR_E2M3;
char *s_HUSTR_E2M4  = HUSTR_E2M4;
char *s_HUSTR_E2M5  = HUSTR_E2M5;
char *s_HUSTR_E2M6  = HUSTR_E2M6;
char *s_HUSTR_E2M7  = HUSTR_E2M7;
char *s_HUSTR_E2M8  = HUSTR_E2M8;
char *s_HUSTR_E2M9  = HUSTR_E2M9;
char *s_HUSTR_E3M1  = HUSTR_E3M1;
char *s_HUSTR_E3M2  = HUSTR_E3M2;
char *s_HUSTR_E3M3  = HUSTR_E3M3;
char *s_HUSTR_E3M4  = HUSTR_E3M4;
char *s_HUSTR_E3M5  = HUSTR_E3M5;
char *s_HUSTR_E3M6  = HUSTR_E3M6;
char *s_HUSTR_E3M7  = HUSTR_E3M7;
char *s_HUSTR_E3M8  = HUSTR_E3M8;
char *s_HUSTR_E3M9  = HUSTR_E3M9;
char *s_HUSTR_E4M1  = HUSTR_E4M1;
char *s_HUSTR_E4M2  = HUSTR_E4M2;
char *s_HUSTR_E4M3  = HUSTR_E4M3;
char *s_HUSTR_E4M4  = HUSTR_E4M4;
char *s_HUSTR_E4M5  = HUSTR_E4M5;
char *s_HUSTR_E4M6  = HUSTR_E4M6;
char *s_HUSTR_E4M7  = HUSTR_E4M7;
char *s_HUSTR_E4M8  = HUSTR_E4M8;
char *s_HUSTR_E4M9  = HUSTR_E4M9;
char *s_HUSTR_1     = HUSTR_1;
char *s_HUSTR_2     = HUSTR_2;
char *s_HUSTR_3     = HUSTR_3;
char *s_HUSTR_4     = HUSTR_4;
char *s_HUSTR_5     = HUSTR_5;
char *s_HUSTR_6     = HUSTR_6;
char *s_HUSTR_7     = HUSTR_7;
char *s_HUSTR_8     = HUSTR_8;
char *s_HUSTR_9     = HUSTR_9;
char *s_HUSTR_10    = HUSTR_10;
char *s_HUSTR_11    = HUSTR_11;
char *s_HUSTR_12    = HUSTR_12;
char *s_HUSTR_13    = HUSTR_13;
char *s_HUSTR_14    = HUSTR_14;
char *s_HUSTR_15    = HUSTR_15;
char *s_HUSTR_16    = HUSTR_16;
char *s_HUSTR_17    = HUSTR_17;
char *s_HUSTR_18    = HUSTR_18;
char *s_HUSTR_19    = HUSTR_19;
char *s_HUSTR_20    = HUSTR_20;
char *s_HUSTR_21    = HUSTR_21;
char *s_HUSTR_22    = HUSTR_22;
char *s_HUSTR_23    = HUSTR_23;
char *s_HUSTR_24    = HUSTR_24;
char *s_HUSTR_25    = HUSTR_25;
char *s_HUSTR_26    = HUSTR_26;
char *s_HUSTR_27    = HUSTR_27;
char *s_HUSTR_28    = HUSTR_28;
char *s_HUSTR_29    = HUSTR_29;
char *s_HUSTR_30    = HUSTR_30;
char *s_HUSTR_31    = HUSTR_31;
char *s_HUSTR_32    = HUSTR_32;
char *s_PHUSTR_1    = PHUSTR_1;
char *s_PHUSTR_2    = PHUSTR_2;
char *s_PHUSTR_3    = PHUSTR_3;
char *s_PHUSTR_4    = PHUSTR_4;
char *s_PHUSTR_5    = PHUSTR_5;
char *s_PHUSTR_6    = PHUSTR_6;
char *s_PHUSTR_7    = PHUSTR_7;
char *s_PHUSTR_8    = PHUSTR_8;
char *s_PHUSTR_9    = PHUSTR_9;
char *s_PHUSTR_10   = PHUSTR_10;
char *s_PHUSTR_11   = PHUSTR_11;
char *s_PHUSTR_12   = PHUSTR_12;
char *s_PHUSTR_13   = PHUSTR_13;
char *s_PHUSTR_14   = PHUSTR_14;
char *s_PHUSTR_15   = PHUSTR_15;
char *s_PHUSTR_16   = PHUSTR_16;
char *s_PHUSTR_17   = PHUSTR_17;
char *s_PHUSTR_18   = PHUSTR_18;
char *s_PHUSTR_19   = PHUSTR_19;
char *s_PHUSTR_20   = PHUSTR_20;
char *s_PHUSTR_21   = PHUSTR_21;
char *s_PHUSTR_22   = PHUSTR_22;
char *s_PHUSTR_23   = PHUSTR_23;
char *s_PHUSTR_24   = PHUSTR_24;
char *s_PHUSTR_25   = PHUSTR_25;
char *s_PHUSTR_26   = PHUSTR_26;
char *s_PHUSTR_27   = PHUSTR_27;
char *s_PHUSTR_28   = PHUSTR_28;
char *s_PHUSTR_29   = PHUSTR_29;
char *s_PHUSTR_30   = PHUSTR_30;
char *s_PHUSTR_31   = PHUSTR_31;
char *s_PHUSTR_32   = PHUSTR_32;
char *s_THUSTR_1    = THUSTR_1;
char *s_THUSTR_2    = THUSTR_2;
char *s_THUSTR_3    = THUSTR_3;
char *s_THUSTR_4    = THUSTR_4;
char *s_THUSTR_5    = THUSTR_5;
char *s_THUSTR_6    = THUSTR_6;
char *s_THUSTR_7    = THUSTR_7;
char *s_THUSTR_8    = THUSTR_8;
char *s_THUSTR_9    = THUSTR_9;
char *s_THUSTR_10   = THUSTR_10;
char *s_THUSTR_11   = THUSTR_11;
char *s_THUSTR_12   = THUSTR_12;
char *s_THUSTR_13   = THUSTR_13;
char *s_THUSTR_14   = THUSTR_14;
char *s_THUSTR_15   = THUSTR_15;
char *s_THUSTR_16   = THUSTR_16;
char *s_THUSTR_17   = THUSTR_17;
char *s_THUSTR_18   = THUSTR_18;
char *s_THUSTR_19   = THUSTR_19;
char *s_THUSTR_20   = THUSTR_20;
char *s_THUSTR_21   = THUSTR_21;
char *s_THUSTR_22   = THUSTR_22;
char *s_THUSTR_23   = THUSTR_23;
char *s_THUSTR_24   = THUSTR_24;
char *s_THUSTR_25   = THUSTR_25;
char *s_THUSTR_26   = THUSTR_26;
char *s_THUSTR_27   = THUSTR_27;
char *s_THUSTR_28   = THUSTR_28;
char *s_THUSTR_29   = THUSTR_29;
char *s_THUSTR_30   = THUSTR_30;
char *s_THUSTR_31   = THUSTR_31;
char *s_THUSTR_32   = THUSTR_32;
char *s_HUSTR_CHATMACRO1   = HUSTR_CHATMACRO1;
char *s_HUSTR_CHATMACRO2   = HUSTR_CHATMACRO2;
char *s_HUSTR_CHATMACRO3   = HUSTR_CHATMACRO3;
char *s_HUSTR_CHATMACRO4   = HUSTR_CHATMACRO4;
char *s_HUSTR_CHATMACRO5   = HUSTR_CHATMACRO5;
char *s_HUSTR_CHATMACRO6   = HUSTR_CHATMACRO6;
char *s_HUSTR_CHATMACRO7   = HUSTR_CHATMACRO7;
char *s_HUSTR_CHATMACRO8   = HUSTR_CHATMACRO8;
char *s_HUSTR_CHATMACRO9   = HUSTR_CHATMACRO9;
char *s_HUSTR_CHATMACRO0   = HUSTR_CHATMACRO0;
char *s_HUSTR_TALKTOSELF1  = HUSTR_TALKTOSELF1;
char *s_HUSTR_TALKTOSELF2  = HUSTR_TALKTOSELF2;
char *s_HUSTR_TALKTOSELF3  = HUSTR_TALKTOSELF3;
char *s_HUSTR_TALKTOSELF4  = HUSTR_TALKTOSELF4;
char *s_HUSTR_TALKTOSELF5  = HUSTR_TALKTOSELF5;
char *s_HUSTR_MESSAGESENT  = HUSTR_MESSAGESENT;
char *s_HUSTR_PLRGREEN     = HUSTR_PLRGREEN;
char *s_HUSTR_PLRINDIGO    = HUSTR_PLRINDIGO;
char *s_HUSTR_PLRBROWN     = HUSTR_PLRBROWN;
char *s_HUSTR_PLRRED       = HUSTR_PLRRED;
char *s_AMSTR_FOLLOWON     = AMSTR_FOLLOWON;
char *s_AMSTR_FOLLOWOFF    = AMSTR_FOLLOWOFF;
char *s_AMSTR_GRIDON       = AMSTR_GRIDON;
char *s_AMSTR_GRIDOFF      = AMSTR_GRIDOFF;
char *s_AMSTR_MARKEDSPOT   = AMSTR_MARKEDSPOT;
char *s_AMSTR_MARKSCLEARED = AMSTR_MARKSCLEARED;
char *s_STSTR_MUS          = STSTR_MUS;
char *s_STSTR_NOMUS        = STSTR_NOMUS;
char *s_STSTR_DQDON        = STSTR_DQDON;
char *s_STSTR_DQDOFF       = STSTR_DQDOFF;
char *s_STSTR_KFAADDED     = STSTR_KFAADDED;
char *s_STSTR_FAADDED      = STSTR_FAADDED;
char *s_STSTR_NCON         = STSTR_NCON;
char *s_STSTR_NCOFF        = STSTR_NCOFF;
char *s_STSTR_BEHOLD       = STSTR_BEHOLD;
char *s_STSTR_BEHOLDX      = STSTR_BEHOLDX;
char *s_STSTR_CHOPPERS     = STSTR_CHOPPERS;
char *s_STSTR_CLEV         = STSTR_CLEV;
char *s_STSTR_COMPON       = STSTR_COMPON;
char *s_STSTR_COMPOFF      = STSTR_COMPOFF;
char *s_E1TEXT     = E1TEXT;
char *s_E2TEXT     = E2TEXT;
char *s_E3TEXT     = E3TEXT;
char *s_E4TEXT     = E4TEXT;
char *s_C1TEXT     = C1TEXT;
char *s_C2TEXT     = C2TEXT;
char *s_C3TEXT     = C3TEXT;
char *s_C4TEXT     = C4TEXT;
char *s_C5TEXT     = C5TEXT;
char *s_C6TEXT     = C6TEXT;
char *s_P1TEXT     = P1TEXT;
char *s_P2TEXT     = P2TEXT;
char *s_P3TEXT     = P3TEXT;
char *s_P4TEXT     = P4TEXT;
char *s_P5TEXT     = P5TEXT;
char *s_P6TEXT     = P6TEXT;
char *s_T1TEXT     = T1TEXT;
char *s_T2TEXT     = T2TEXT;
char *s_T3TEXT     = T3TEXT;
char *s_T4TEXT     = T4TEXT;
char *s_T5TEXT     = T5TEXT;
char *s_T6TEXT     = T6TEXT;
char *s_CC_ZOMBIE  = CC_ZOMBIE;
char *s_CC_SHOTGUN = CC_SHOTGUN;
char *s_CC_HEAVY   = CC_HEAVY;
char *s_CC_IMP     = CC_IMP;
char *s_CC_DEMON   = CC_DEMON;
char *s_CC_LOST    = CC_LOST;
char *s_CC_CACO    = CC_CACO;
char *s_CC_HELL    = CC_HELL;
char *s_CC_BARON   = CC_BARON;
char *s_CC_ARACH   = CC_ARACH;
char *s_CC_PAIN    = CC_PAIN;
char *s_CC_REVEN   = CC_REVEN;
char *s_CC_MANCU   = CC_MANCU;
char *s_CC_ARCH    = CC_ARCH;
char *s_CC_SPIDER  = CC_SPIDER;
char *s_CC_CYBER   = CC_CYBER;
char *s_CC_HERO    = CC_HERO;
// haleyjd 10/08/06: new demo sequence strings
char *s_TITLEPIC   = TITLEPIC; // for doom
char *s_TITLE      = TITLE;    // for heretic
char *s_DEMO1      = DEMO1;    // demos
char *s_DEMO2      = DEMO2;
char *s_DEMO3      = DEMO3;
char *s_DEMO4      = DEMO4;
char *s_CREDIT     = CREDIT;  // credit/order screens
char *s_ORDER      = ORDER;
char *s_HELP2      = HELP2;
// Ty 03/30/98 - new substitutions for background textures
char *bgflatE1     = "FLOOR4_8"; // end of DOOM Episode 1
char *bgflatE2     = "SFLR6_1";  // end of DOOM Episode 2
char *bgflatE3     = "MFLR8_4";  // end of DOOM Episode 3
char *bgflatE4     = "MFLR8_3";  // end of DOOM Episode 4
char *bgflat06     = "SLIME16";  // DOOM2 after MAP06
char *bgflat11     = "RROCK14";  // DOOM2 after MAP11
char *bgflat20     = "RROCK07";  // DOOM2 after MAP20
char *bgflat30     = "RROCK17";  // DOOM2 after MAP30
char *bgflat15     = "RROCK13";  // DOOM2 going MAP15 to MAP31
char *bgflat31     = "RROCK19";  // DOOM2 going MAP31 to MAP32
char *bgcastcall   = "BOSSBACK"; // Panel behind cast call
char *bgflathE1    = "FLOOR25";  // haleyjd: Heretic episode 1
char *bgflathE2    = "FLATHUH1"; // Heretic episode 2
char *bgflathE3    = "FLTWAWA2"; // Heretic episode 3
char *bgflathE4    = "FLOOR28";  // Heretic episode 4
char *bgflathE5    = "FLOOR08";  // Heretic episode 5

char *startup1     = "";  // blank lines are default and are not
char *startup2     = "";  // printed
char *startup3     = "";
char *startup4     = "";
char *startup5     = "";

// haleyjd: heretic strings
char *s_HHUSTR_E1M1 = HHUSTR_E1M1;
char *s_HHUSTR_E1M2 = HHUSTR_E1M2;
char *s_HHUSTR_E1M3 = HHUSTR_E1M3;
char *s_HHUSTR_E1M4 = HHUSTR_E1M4;
char *s_HHUSTR_E1M5 = HHUSTR_E1M5;
char *s_HHUSTR_E1M6 = HHUSTR_E1M6;
char *s_HHUSTR_E1M7 = HHUSTR_E1M7;
char *s_HHUSTR_E1M8 = HHUSTR_E1M8;
char *s_HHUSTR_E1M9 = HHUSTR_E1M9;
char *s_HHUSTR_E2M1 = HHUSTR_E2M1;
char *s_HHUSTR_E2M2 = HHUSTR_E2M2;
char *s_HHUSTR_E2M3 = HHUSTR_E2M3;
char *s_HHUSTR_E2M4 = HHUSTR_E2M4;
char *s_HHUSTR_E2M5 = HHUSTR_E2M5;
char *s_HHUSTR_E2M6 = HHUSTR_E2M6;
char *s_HHUSTR_E2M7 = HHUSTR_E2M7;
char *s_HHUSTR_E2M8 = HHUSTR_E2M8;
char *s_HHUSTR_E2M9 = HHUSTR_E2M9;
char *s_HHUSTR_E3M1 = HHUSTR_E3M1;
char *s_HHUSTR_E3M2 = HHUSTR_E3M2;
char *s_HHUSTR_E3M3 = HHUSTR_E3M3;
char *s_HHUSTR_E3M4 = HHUSTR_E3M4;
char *s_HHUSTR_E3M5 = HHUSTR_E3M5;
char *s_HHUSTR_E3M6 = HHUSTR_E3M6;
char *s_HHUSTR_E3M7 = HHUSTR_E3M7;
char *s_HHUSTR_E3M8 = HHUSTR_E3M8;
char *s_HHUSTR_E3M9 = HHUSTR_E3M9;
char *s_HHUSTR_E4M1 = HHUSTR_E4M1;
char *s_HHUSTR_E4M2 = HHUSTR_E4M2;
char *s_HHUSTR_E4M3 = HHUSTR_E4M3;
char *s_HHUSTR_E4M4 = HHUSTR_E4M4;
char *s_HHUSTR_E4M5 = HHUSTR_E4M5;
char *s_HHUSTR_E4M6 = HHUSTR_E4M6;
char *s_HHUSTR_E4M7 = HHUSTR_E4M7;
char *s_HHUSTR_E4M8 = HHUSTR_E4M8;
char *s_HHUSTR_E4M9 = HHUSTR_E4M9;
char *s_HHUSTR_E5M1 = HHUSTR_E5M1;
char *s_HHUSTR_E5M2 = HHUSTR_E5M2;
char *s_HHUSTR_E5M3 = HHUSTR_E5M3;
char *s_HHUSTR_E5M4 = HHUSTR_E5M4;
char *s_HHUSTR_E5M5 = HHUSTR_E5M5;
char *s_HHUSTR_E5M6 = HHUSTR_E5M6;
char *s_HHUSTR_E5M7 = HHUSTR_E5M7;
char *s_HHUSTR_E5M8 = HHUSTR_E5M8;
char *s_HHUSTR_E5M9 = HHUSTR_E5M9;
char *s_H1TEXT      = H1TEXT;
char *s_H2TEXT      = H2TEXT;
char *s_H3TEXT      = H3TEXT;
char *s_H4TEXT      = H4TEXT;
char *s_H5TEXT      = H5TEXT;
char *s_HGOTBLUEKEY = HGOTBLUEKEY;
char *s_HGOTYELLOWKEY     = HGOTYELLOWKEY;
char *s_HGOTGREENKEY      = HGOTGREENKEY;
char *s_HITEMHEALTH       = HITEMHEALTH;
char *s_HITEMSHIELD1      = HITEMSHIELD1;
char *s_HITEMSHIELD2      = HITEMSHIELD2;
char *s_HITEMBAGOFHOLDING = HITEMBAGOFHOLDING;
char *s_HITEMSUPERMAP     = HITEMSUPERMAP;
char *s_HPD_GREENO  = HPD_GREENO;
char *s_HPD_GREENK  = HPD_GREENK;
char *s_HAMMOGOLDWAND1 = HAMMOGOLDWAND1;
char *s_HAMMOGOLDWAND2 = HAMMOGOLDWAND2;
char *s_HAMMOMACE1     = HAMMOMACE1;
char *s_HAMMOMACE2     = HAMMOMACE2;
char *s_HAMMOCROSSBOW1 = HAMMOCROSSBOW1;
char *s_HAMMOCROSSBOW2 = HAMMOCROSSBOW2;
char *s_HAMMOBLASTER1  = HAMMOBLASTER1;
char *s_HAMMOBLASTER2  = HAMMOBLASTER2;
char *s_HAMMOSKULLROD1 = HAMMOSKULLROD1;
char *s_HAMMOSKULLROD2 = HAMMOSKULLROD2;
char *s_HAMMOPHOENIXROD1 = HAMMOPHOENIXROD1;
char *s_HAMMOPHOENIXROD2 = HAMMOPHOENIXROD2;

// obituaries
char *s_OB_SUICIDE = OB_SUICIDE;
char *s_OB_FALLING = OB_FALLING;
char *s_OB_CRUSH   = OB_CRUSH;
char *s_OB_LAVA    = OB_LAVA;
char *s_OB_SLIME   = OB_SLIME;
char *s_OB_BARREL  = OB_BARREL;
char *s_OB_SPLASH  = OB_SPLASH;
char *s_OB_COOP    = OB_COOP;
char *s_OB_DEFAULT = OB_DEFAULT;
char *s_OB_R_SPLASH_SELF = OB_R_SPLASH_SELF;
char *s_OB_ROCKET_SELF = OB_ROCKET_SELF;
char *s_OB_BFG11K_SELF = OB_BFG11K_SELF;
char *s_OB_FIST = OB_FIST;
char *s_OB_CHAINSAW = OB_CHAINSAW;
char *s_OB_PISTOL = OB_PISTOL;
char *s_OB_SHOTGUN = OB_SHOTGUN;
char *s_OB_SSHOTGUN = OB_SSHOTGUN;
char *s_OB_CHAINGUN = OB_CHAINGUN;
char *s_OB_ROCKET = OB_ROCKET;
char *s_OB_R_SPLASH = OB_R_SPLASH;
char *s_OB_PLASMA = OB_PLASMA;
char *s_OB_BFG = OB_BFG;
char *s_OB_BFG_SPLASH = OB_BFG_SPLASH;
char *s_OB_BETABFG = OB_BETABFG;
char *s_OB_BFGBURST = OB_BFGBURST;
char *s_OB_GRENADE_SELF = OB_GRENADE_SELF;
char *s_OB_GRENADE = OB_GRENADE;
char *s_OB_TELEFRAG = OB_TELEFRAG;
char *s_OB_QUAKE = OB_QUAKE;

// Ty 05/03/98 - externalized
char *savegamename;

// end d_deh.h variable declarations
// ============================================================

// Do this for a lookup--the pointer (loaded above) is 
// cross-referenced to a string key that is the same as the define 
// above.  We will use strdups to set these new values that we read 
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
   { &s_HPD_GREENO,        "HPD_GREENO"        },
   { &s_HPD_GREENK,        "HPD_GREENK"        },
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
};

static int deh_numstrlookup = sizeof(deh_strlookup)/sizeof(dehstr_t);

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

void A_Light0(mobj_t *);
void A_WeaponReady(mobj_t *);
void A_Lower(mobj_t *);
void A_Raise(mobj_t *);
void A_Punch(mobj_t *);
void A_ReFire(mobj_t *);
void A_FirePistol(mobj_t *);
void A_Light1(mobj_t *);
void A_FireShotgun(mobj_t *);
void A_Light2(mobj_t *);
void A_FireShotgun2(mobj_t *);
void A_CheckReload(mobj_t *);
void A_OpenShotgun2(mobj_t *);
void A_LoadShotgun2(mobj_t *);
void A_CloseShotgun2(mobj_t *);
void A_FireCGun(mobj_t *);
void A_GunFlash(mobj_t *);
void A_FireMissile(mobj_t *);
void A_Saw(mobj_t *);
void A_FirePlasma(mobj_t *);
void A_BFGsound(mobj_t *);
void A_FireBFG(mobj_t *);
void A_BFGSpray(mobj_t *);
void A_Explode(mobj_t *);
void A_Pain(mobj_t *);
void A_PlayerScream(mobj_t *);
void A_Fall(mobj_t *);
void A_XScream(mobj_t *);
void A_Look(mobj_t *);
void A_Chase(mobj_t *);
void A_FaceTarget(mobj_t *);
void A_PosAttack(mobj_t *);
void A_Scream(mobj_t *);
void A_SPosAttack(mobj_t *);
void A_VileChase(mobj_t *);
void A_VileStart(mobj_t *);
void A_VileTarget(mobj_t *);
void A_VileAttack(mobj_t *);
void A_StartFire(mobj_t *);
void A_Fire(mobj_t *);
void A_FireCrackle(mobj_t *);
void A_Tracer(mobj_t *);
void A_SkelWhoosh(mobj_t *);
void A_SkelFist(mobj_t *);
void A_SkelMissile(mobj_t *);
void A_FatRaise(mobj_t *);
void A_FatAttack1(mobj_t *);
void A_FatAttack2(mobj_t *);
void A_FatAttack3(mobj_t *);
void A_BossDeath(mobj_t *);
void A_CPosAttack(mobj_t *);
void A_CPosRefire(mobj_t *);
void A_TroopAttack(mobj_t *);
void A_SargAttack(mobj_t *);
void A_HeadAttack(mobj_t *);
void A_BruisAttack(mobj_t *);
void A_SkullAttack(mobj_t *);
void A_Metal(mobj_t *);
void A_SpidRefire(mobj_t *);
void A_BabyMetal(mobj_t *);
void A_BspiAttack(mobj_t *);
void A_Hoof(mobj_t *);
void A_CyberAttack(mobj_t *);
void A_PainAttack(mobj_t *);
void A_PainDie(mobj_t *);
void A_KeenDie(mobj_t *);
void A_BrainPain(mobj_t *);
void A_BrainScream(mobj_t *);
void A_BrainDie(mobj_t *);
void A_BrainAwake(mobj_t *);
void A_BrainSpit(mobj_t *);
void A_SpawnSound(mobj_t *);
void A_SpawnFly(mobj_t *);
void A_BrainExplode(mobj_t *);
void A_Detonate(mobj_t *);        // killough 8/9/98
void A_Mushroom(mobj_t *);        // killough 10/98
void A_Die(mobj_t *);             // killough 11/98
void A_Spawn(mobj_t *);           // killough 11/98
void A_Turn(mobj_t *);            // killough 11/98
void A_Face(mobj_t *);            // killough 11/98
void A_Scratch(mobj_t *);         // killough 11/98
void A_PlaySound(mobj_t *);       // killough 11/98
void A_RandomJump(mobj_t *);      // killough 11/98
void A_LineEffect(mobj_t *);      // killough 11/98
void A_Nailbomb(mobj_t *);

// haleyjd: start new eternity action functions
void A_SetFlags(mobj_t *);
void A_UnSetFlags(mobj_t *);
void A_BetaSkullAttack(mobj_t *);
void A_StartScript(mobj_t *); // haleyjd 1/25/00: Script wrapper
void A_PlayerStartScript(mobj_t *);
void A_FireGrenade(mobj_t *);
void A_FireCustomBullets(mobj_t *);
void A_FirePlayerMissile(mobj_t *);
void A_CustomPlayerMelee(mobj_t *);
void A_GenTracer(mobj_t *);
void A_BFG11KHit(mobj_t *);
void A_BouncingBFG(mobj_t *);
void A_BFGBurst(mobj_t *);
void A_FireOldBFG(mobj_t *);
void A_KeepChasing(mobj_t *);
void A_Stop(mobj_t *);
void A_PlayerThunk(mobj_t *);
void A_MissileAttack(mobj_t *);
void A_MissileSpread(mobj_t *);
void A_BulletAttack(mobj_t *);
void A_HealthJump(mobj_t *);
void A_CounterJump(mobj_t *);
void A_CounterSwitch(mobj_t *);
void A_SetCounter(mobj_t *);
void A_CopyCounter(mobj_t *);
void A_CounterOp(mobj_t *);
void A_SetTics(mobj_t *);
void A_AproxDistance(mobj_t *);
void A_ShowMessage(mobj_t *);
void A_RandomWalk(mobj_t *);
void A_TargetJump(mobj_t *);
void A_ThingSummon(mobj_t *);
void A_KillChildren(mobj_t *);
void A_WeaponCtrJump(mobj_t *);
void A_WeaponCtrSwitch(mobj_t *);
void A_WeaponSetCtr(mobj_t *);
void A_WeaponCopyCtr(mobj_t *);
void A_WeaponCtrOp(mobj_t *);
void A_AmbientThinker(mobj_t *);
void A_SteamSpawn(mobj_t *);
void A_EjectCasing(mobj_t *);

// haleyjd 10/12/02: Heretic pointers
void A_SpawnGlitter(mobj_t *);
void A_AccelGlitter(mobj_t *);
void A_SpawnAbove(mobj_t *);
void A_MummyAttack(mobj_t *);
void A_MummyAttack2(mobj_t *);
void A_MummySoul(mobj_t *);
void A_HticDrop(mobj_t *);
void A_HticTracer(mobj_t *);
void A_ClinkAttack(mobj_t *);
void A_WizardAtk1(mobj_t *);
void A_WizardAtk2(mobj_t *);
void A_WizardAtk3(mobj_t *);
void A_Srcr2Decide(mobj_t *);
void A_Srcr2Attack(mobj_t *);
void A_BlueSpark(mobj_t *);
void A_GenWizard(mobj_t *);
void A_Sor2DthInit(mobj_t *);
void A_Sor2DthLoop(mobj_t *);
void A_HticExplode(mobj_t *);
void A_HticBossDeath(mobj_t *);
void A_PodPain(mobj_t *);
void A_RemovePod(mobj_t *);
void A_MakePod(mobj_t *);
void A_KnightAttack(mobj_t *);
void A_DripBlood(mobj_t *);
void A_BeastAttack(mobj_t *);
void A_BeastPuff(mobj_t *);
void A_SnakeAttack(mobj_t *);
void A_SnakeAttack2(mobj_t *);
void A_Sor1Chase(mobj_t *);
void A_Sor1Pain(mobj_t *);
void A_Srcr1Attack(mobj_t *);
void A_SorcererRise(mobj_t *);
void A_VolcanoBlast(mobj_t *);
void A_VolcBallImpact(mobj_t *);
void A_MinotaurAtk1(mobj_t *);
void A_MinotaurDecide(mobj_t *);
void A_MinotaurAtk2(mobj_t *);
void A_MinotaurAtk3(mobj_t *);
void A_MinotaurCharge(mobj_t *);
void A_MntrFloorFire(mobj_t *);
void A_LichFire(mobj_t *);
void A_LichWhirlwind(mobj_t *);
void A_LichAttack(mobj_t *);
void A_WhirlwindSeek(mobj_t *);
void A_LichIceImpact(mobj_t *);
void A_LichFireGrow(mobj_t *);
void A_ImpChargeAtk(mobj_t *);
void A_ImpMeleeAtk(mobj_t *);
void A_ImpMissileAtk(mobj_t *);
void A_ImpDeath(mobj_t *);
void A_ImpXDeath1(mobj_t *);
void A_ImpXDeath2(mobj_t *);
void A_ImpExplode(mobj_t *);
void A_PlayerSkull(mobj_t *);
void A_ClearSkin(mobj_t *mo);
void A_PhoenixPuff(mobj_t *mo);

// haleyjd 10/04/08: Hexen pointers
void A_SetInvulnerable(mobj_t *mo);
void A_UnSetInvulnerable(mobj_t *mo);
void A_SetReflective(mobj_t *mo);
void A_UnSetReflective(mobj_t *mo);
void A_PigLook(mobj_t *mo);
void A_PigChase(mobj_t *mo);
void A_PigAttack(mobj_t *mo);
void A_PigPain(mobj_t *mo);
void A_HexenExplode(mobj_t *mo);
void A_SerpentUnHide(mobj_t *mo);
void A_SerpentHide(mobj_t *mo);
void A_SerpentChase(mobj_t *mo);
void A_RaiseFloorClip(mobj_t *mo);
void A_LowerFloorClip(mobj_t *mo);
void A_SerpentHumpDecide(mobj_t *mo);
void A_SerpentWalk(mobj_t *mo);
void A_SerpentCheckForAttack(mobj_t *mo);
void A_SerpentChooseAttack(mobj_t *mo);
void A_SerpentMeleeAttack(mobj_t *mo);
void A_SerpentMissileAttack(mobj_t *mo);
void A_SerpentSpawnGibs(mobj_t *mo);
void A_SubTics(mobj_t *mo);
void A_SerpentHeadCheck(mobj_t *mo);
void A_CentaurAttack(mobj_t *mo);
void A_CentaurAttack2(mobj_t *mo);
void A_DropEquipment(mobj_t *mo);
void A_CentaurDefend(mobj_t *mo);
void A_BishopAttack(mobj_t *mo);
void A_BishopAttack2(mobj_t *mo);
void A_BishopMissileWeave(mobj_t *mo);
void A_BishopDoBlur(mobj_t *mo);
void A_SpawnBlur(mobj_t *mo);
void A_BishopChase(mobj_t *mo);
void A_BishopPainBlur(mobj_t *mo);
void A_DragonInitFlight(mobj_t *mo);
void A_DragonFlight(mobj_t *mo);
void A_DragonFlap(mobj_t *mo);
void A_DragonFX2(mobj_t *mo);
void A_PainCounterBEQ(mobj_t *mo);
void A_DemonAttack1(mobj_t *mo);
void A_DemonAttack2(mobj_t *mo);
void A_DemonDeath(mobj_t *mo);
void A_Demon2Death(mobj_t *mo);
void A_WraithInit(mobj_t *mo);
void A_WraithRaiseInit(mobj_t *mo);
void A_WraithRaise(mobj_t *mo);
void A_WraithMelee(mobj_t *mo);
void A_WraithMissile(mobj_t *mo);
void A_WraithFX2(mobj_t *mo);
void A_WraithFX3(mobj_t *mo);
void A_WraithFX4(mobj_t *mo);
void A_WraithLook(mobj_t *mo);
void A_WraithChase(mobj_t *mo);
void A_EttinAttack(mobj_t *mo);
void A_DropMace(mobj_t *mo);
void A_AffritSpawnRock(mobj_t *mo);
void A_AffritRocks(mobj_t *mo);
void A_SmBounce(mobj_t *mo);
void A_AffritChase(mobj_t *mo);
void A_AffritSplotch(mobj_t *mo);
void A_IceGuyLook(mobj_t *mo);
void A_IceGuyChase(mobj_t *mo);
void A_IceGuyAttack(mobj_t *mo);
void A_IceGuyDie(mobj_t *mo);
void A_IceGuyMissileExplode(mobj_t *mo);
void A_CheckFloor(mobj_t *mo);
void A_FreezeDeath(mobj_t *mo);
void A_IceSetTics(mobj_t *mo);
void A_IceCheckHeadDone(mobj_t *mo);
void A_FreezeDeathChunks(mobj_t *mo);

// zdoom-inspired pointers
void A_JumpIfTargetInLOS(mobj_t *);
void A_SetTranslucent(mobj_t *);
void A_AlertMonsters(mobj_t *);
void A_CheckPlayerDone(mobj_t *);
void A_FadeIn(mobj_t *);
void A_FadeOut(mobj_t *);
void A_PlaySoundEx(mobj_t *mo);

// eternity tc ptrs: TODO: remove these?
void A_FogSpawn(mobj_t *);
void A_FogMove(mobj_t *);

// haleyjd 07/13/03: special death actions for killem cheat
void A_PainNukeSpec(mobj_t *);
void A_SorcNukeSpec(mobj_t *);

// Keyword sets for parameterized codepointers
#define EXTKWDS(a) extern E_Keyword_t kwds_ ## a []
#define KWDS(a)    kwds_ ## a

EXTKWDS(A_FireCustomBullets);
EXTKWDS(A_FirePlayerMissile);
EXTKWDS(A_CustomPlayerMelee);
EXTKWDS(A_PlayerThunk);
EXTKWDS(A_WeaponCtrJump);
EXTKWDS(A_WeaponCtrSwitch);
EXTKWDS(A_WeaponSetCtr);
EXTKWDS(A_WeaponCtrOp);
EXTKWDS(A_PlaySound);
EXTKWDS(A_HticExplode);
EXTKWDS(A_Scratch);
EXTKWDS(A_MissileAttack);
EXTKWDS(A_BulletAttack);
EXTKWDS(A_ThingSummon);
EXTKWDS(A_KillChildren);
EXTKWDS(A_HealthJump);
EXTKWDS(A_CounterJump);
EXTKWDS(A_SetCounter);
EXTKWDS(A_CounterOp);
EXTKWDS(A_SetTics);
EXTKWDS(A_StartScript);
EXTKWDS(A_PlayerStartScript);
EXTKWDS(A_ShowMessage);
EXTKWDS(A_PlaySoundEx);

// haleyjd 03/14/03: moved here, added hashing, eliminated useless
// A_ prefixes on mnemonics

// haleyjd 10/04/08: macros to save my wrists
#define POINTER(name)  { A_ ## name , #name }
#define POINTERK(name)  { A_ ## name , #name , KWDS( A_ ## name ) }

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
  POINTERK(Scratch),
  POINTERK(PlaySound),
  POINTER(RandomJump),
  POINTER(LineEffect),
  
  POINTER(Nailbomb),

  // haleyjd: start new eternity codeptrs
  POINTERK(StartScript),
  POINTERK(PlayerStartScript),
  POINTER(SetFlags),
  POINTER(UnSetFlags),
  POINTER(BetaSkullAttack),
  POINTER(FireGrenade),
  POINTERK(FireCustomBullets),
  POINTERK(FirePlayerMissile),
  POINTERK(CustomPlayerMelee),
  POINTER(GenTracer),
  POINTER(BFG11KHit),
  POINTER(BouncingBFG),
  POINTER(BFGBurst),          
  POINTER(FireOldBFG),        
  POINTER(KeepChasing),       
  POINTER(Stop),              
  POINTERK(PlayerThunk),
  POINTERK(MissileAttack),
  POINTER(MissileSpread),
  POINTERK(BulletAttack),
  POINTERK(HealthJump),
  POINTERK(CounterJump),
  POINTER(CounterSwitch),
  POINTERK(SetCounter),
  POINTER(CopyCounter),
  POINTERK(CounterOp),
  POINTERK(SetTics),
  POINTER(AproxDistance),
  POINTERK(ShowMessage),
  POINTER(RandomWalk),
  POINTER(TargetJump),
  POINTERK(ThingSummon),
  POINTERK(KillChildren),
  POINTERK(WeaponCtrJump),
  POINTERK(WeaponCtrSwitch),
  POINTERK(WeaponSetCtr),
  POINTER(WeaponCopyCtr),
  POINTERK(WeaponCtrOp),
  POINTER(AmbientThinker),
  POINTER(SteamSpawn),
  POINTER(EjectCasing),

  // haleyjd 07/13/03: nuke specials
  POINTER(PainNukeSpec),
  POINTER(SorcNukeSpec),

  // haleyjd: Heretic pointers
  POINTER(SpawnGlitter),
  POINTER(AccelGlitter),      
  POINTER(SpawnAbove),        
  POINTER(MummyAttack),       
  POINTER(MummyAttack2),      
  POINTER(MummySoul),         
  POINTER(HticDrop),          
  POINTER(HticTracer),        
  POINTER(ClinkAttack),       
  POINTER(WizardAtk1),        
  POINTER(WizardAtk2),        
  POINTER(WizardAtk3),        
  POINTER(Srcr2Decide),       
  POINTER(Srcr2Attack),       
  POINTER(BlueSpark),         
  POINTER(GenWizard),         
  POINTER(Sor2DthInit),       
  POINTER(Sor2DthLoop),       
  POINTERK(HticExplode),
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
  POINTER(ClearSkin),
  POINTER(PhoenixPuff),

  // haleyjd 10/04/08: Hexen pointers
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

  // zdoom-inspired pointers
  POINTER(AlertMonsters),     
  POINTER(CheckPlayerDone),   
  POINTER(FadeIn),            
  POINTER(FadeOut),           
  POINTER(JumpIfTargetInLOS), 
  POINTERK(PlaySoundEx),
  POINTER(SetTranslucent),    

  // ETERNITY TC ptrs -- TODO: eliminate these
  POINTER(FogSpawn),
  POINTER(FogMove),

  // This NULL entry must be the last in the list
  {NULL,             "NULL"},  // Ty 05/16/98
};

// haleyjd 03/14/03: Just because its null-terminated doesn't mean 
// we can index it however the hell we want! See deh_procPointer in
// d_deh.c for a big bug fix.

int num_bexptrs = sizeof(deh_bexptrs) / sizeof(*deh_bexptrs);

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
//
unsigned int D_HashTableKey(const char *str)
{
   const char *c = str;
   unsigned int h = 0;

   if(!str)
      I_Error("D_HashTableKey: cannot hash NULL string!\n");

   // note: this needs to be case insensitive for EDF mnemonics
   while(*c)
   {
      h = 5 * h + toupper(*c);
      ++c;
   }

   return h;
}

//
// BEX String Hash Table
//
// The deh_strs table has two independent hash chains through it,
// one for lookup by mnemonic (BEX style), and one for lookup by
// value (DEH style).
//

// number of strings = 416, 419 = closest greater prime
#define NUMSTRCHAINS 419
static int bexstrhashchains[NUMSTRCHAINS];
static int dehstrhashchains[NUMSTRCHAINS];

static void D_DEHStrHashInit(void)
{
   int i;

   memset(bexstrhashchains, -1, NUMSTRCHAINS*sizeof(int));
   memset(dehstrhashchains, -1, NUMSTRCHAINS*sizeof(int));

   for(i = 0; i < deh_numstrlookup; ++i)
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
   if(bexstrhashchains[key] == -1)
      return NULL;

   dehstr = &deh_strlookup[bexstrhashchains[key]];

   // while string doesn't match lookup, go down hash chain
   while(strcasecmp(string, dehstr->lookup))
   {
      // end of hash chain -- not found
      if(dehstr->bnext == -1)
         return NULL;
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
   if(dehstrhashchains[key] == -1)
      return NULL;

   dehstr = &deh_strlookup[dehstrhashchains[key]];

   // while string doesn't match lookup, go down hash chain
   while(stricmp(dehstr->original, string))
   {
      // end of hash chain -- not found
      if(dehstr->dnext == -1)
         return NULL;
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

   // allow NULL mnemonic to return NULL
   if(mnemonic == NULL)
      return NULL;

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
boolean DEH_StringChanged(const char *mnemonic)
{
   dehstr_t *dehstr;

   if(!(dehstr = D_GetBEXStr(mnemonic)))
      I_Error("DEH_StringChanged: unknown BEX mnemonic %s\n", mnemonic);

   return (dehstr->original != *(dehstr->ppstr));
}

//
// BEX Code Pointer Hash Table
//

#define NUMCPTRCHAINS 257

static int bexcpchains[NUMCPTRCHAINS];

static void D_BEXPtrHashInit(void)
{
   int i;

   memset(bexcpchains, -1, NUMCPTRCHAINS*sizeof(int));

   for(i = 0; i < num_bexptrs; i++)
   {
      unsigned int key;
      
      key = D_HashTableKey(deh_bexptrs[i].lookup) % NUMCPTRCHAINS;

      deh_bexptrs[i].next = bexcpchains[key];
      bexcpchains[key] = i;

      // haleyjd 06/12/08: also add EDF keywords if defined
      if(deh_bexptrs[i].kwds)
         E_AddKeywords((E_Keyword_t *)deh_bexptrs[i].kwds);
   }
}

deh_bexptr *D_GetBexPtr(const char *mnemonic)
{
   unsigned int key;
   deh_bexptr *bexptr;

   // calculate key for mnemonic
   key = D_HashTableKey(mnemonic) % NUMCPTRCHAINS;

   // is chain empty?
   if(bexcpchains[key] == -1)
      return NULL; // doesn't exist

   bexptr = &deh_bexptrs[bexcpchains[key]];

   while(stricmp(mnemonic, bexptr->lookup))
   {
      // end of hash chain?
      if(bexptr->next == -1)
         return NULL; // doesn't exist
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
void D_BuildBEXHashChains(void)
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
// strdup's string values in the sprites and music
// tables for use as unchanging mnemonics. Sounds now
// store their own mnemonics and thus a table for them
// is unnecessary.
//
void D_BuildBEXTables(void)
{
   char *spritestr;
   char *musicstr;
   int i;

   // build sprites, music lookups

   // haleyjd 03/11/03: must be dynamic now
   // 10/17/03: allocate all the names through a single pointer
   spritestr = Z_Calloc(NUMSPRITES, 5, PU_STATIC, NULL);

   deh_spritenames = Z_Malloc((NUMSPRITES+1)*sizeof(char *),PU_STATIC,0);

   for(i = 0; i < NUMSPRITES; ++i)
   {
      deh_spritenames[i] = spritestr + i * 5;
      strncpy(deh_spritenames[i], sprnames[i], 4);
   }
   deh_spritenames[NUMSPRITES] = NULL;

   // 09/07/05: allocate all music names through one pointer
   musicstr = Z_Calloc(NUMMUSIC, 7, PU_STATIC, 0);

   deh_musicnames = Z_Malloc((NUMMUSIC+1)*sizeof(char *), PU_STATIC, 0);

   for(i = 1; i < NUMMUSIC; ++i)
   {
      deh_musicnames[i] = musicstr + i * 7;
      strncpy(deh_musicnames[i], S_music[i].name, 6);
   }
   deh_musicnames[0] = deh_musicnames[NUMMUSIC] = NULL;
}

//
// DeHackEd File Queue
//

// haleyjd: All DeHackEd files and lumps are now queued as they
// are encountered, and then processed in the appropriate order
// all at once. This allows EDF to be extended for loading from
// wad files.

typedef struct dehqueueitem_s
{
   mqueueitem_t mqitem; // this must be first

   char name[PATH_MAX+1];
   int  lumpnum;   
} dehqueueitem_t;

static mqueue_t dehqueue;

void D_DEHQueueInit(void)
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
   dehqueueitem_t *newdq = calloc(1, sizeof(dehqueueitem_t));

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
// Add lump number as third argument, for use when filename==NULL
void ProcessDehFile(char *filename, const char *outfilename, int lump);

// killough 10/98: support -dehout filename
// cph - made const, don't cache results
// haleyjd 09/11/03: moved here from d_main.c
static const char *D_dehout(void)
{
   int p = M_CheckParm("-dehout");

   if(!p)
      p = M_CheckParm("-bexout");
   
   return (p && ++p < myargc) ? myargv[p] : NULL;
}

//
// D_ProcessDEHQueue
//
// Processes all the DeHackEd/BEX files queued during startup, including
// command-line DEHs, GFS DEHs, preincluded DEHs, and in-wad DEHs.
//
void D_ProcessDEHQueue(void)
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
         ProcessDehFile(NULL, D_dehout(), dqitem->lumpnum);
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


