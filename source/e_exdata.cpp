// Emacs style mode select -*- C++ -*- vi:ts=3:sw=3:set et:
//----------------------------------------------------------------------------
//
// Copyright(C) 2003 James Haley
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

#include "z_zone.h"
#include "i_system.h"

#define NEED_EDF_DEFINITIONS

#include "Confuse/confuse.h"
#include "e_exdata.h"
#include "e_lib.h"
#include "e_mod.h"
#include "e_things.h"
#include "e_ttypes.h"

#include "d_dehtbl.h" // for dehflags parsing
#include "d_io.h"
#include "doomdef.h"
#include "doomstat.h"
#include "m_qstr.h"
#include "p_info.h"
#include "p_mobj.h"
#include "p_portal.h"
#include "p_spec.h"
#include "r_data.h"
#include "r_main.h"
#include "r_portal.h"
#include "w_wad.h"

// statics

static mapthing_t *EDThings;
static unsigned int numEDMapThings;

#define NUMMTCHAINS 1021
static unsigned int mapthing_chains[NUMMTCHAINS];

static maplinedefext_t *EDLines;
static unsigned int numEDLines;

#define NUMLDCHAINS 1021
static unsigned int linedef_chains[NUMLDCHAINS];

static mapsectorext_t *EDSectors;
static unsigned int numEDSectors;

#define NUMSECCHAINS 1021
static unsigned int sector_chains[NUMSECCHAINS];

// ExtraData section names
#define SEC_MAPTHING "mapthing"
#define SEC_LINEDEF  "linedef"
#define SEC_SECTOR   "sector"

// ExtraData field names
// mapthing fields:
#define FIELD_NUM     "recordnum"
#define FIELD_TID     "tid"
#define FIELD_TYPE    "type"
#define FIELD_OPTIONS "options"
#define FIELD_ARGS    "args"
#define FIELD_HEIGHT  "height"

// linedef fields:
#define FIELD_LINE_NUM       "recordnum"
#define FIELD_LINE_SPECIAL   "special"
#define FIELD_LINE_TAG       "tag"
#define FIELD_LINE_EXTFLAGS  "extflags"
#define FIELD_LINE_ARGS      "args"
#define FIELD_LINE_ID        "id"
#define FIELD_LINE_ALPHA     "alpha"

// sector fields:
#define FIELD_SECTOR_NUM            "recordnum"
#define FIELD_SECTOR_FLAGS          "flags"
#define FIELD_SECTOR_FLAGSADD       "flags.add"
#define FIELD_SECTOR_FLAGSREM       "flags.remove"
#define FIELD_SECTOR_DAMAGE         "damage"
#define FIELD_SECTOR_DAMAGEMASK     "damagemask"
#define FIELD_SECTOR_DAMAGEMOD      "damagemod"
#define FIELD_SECTOR_DAMAGEFLAGS    "damageflags"
#define FIELD_SECTOR_DMGFLAGSADD    "damageflags.add"
#define FIELD_SECTOR_DMGFLAGSREM    "damageflags.remove"
#define FIELD_SECTOR_FLOORTERRAIN   "floorterrain"
#define FIELD_SECTOR_FLOORANGLE     "floorangle"
#define FIELD_SECTOR_FLOOROFFSETX   "flooroffsetx"
#define FIELD_SECTOR_FLOOROFFSETY   "flooroffsety"
#define FIELD_SECTOR_CEILINGTERRAIN "ceilingterrain"
#define FIELD_SECTOR_CEILINGANGLE   "ceilingangle"
#define FIELD_SECTOR_CEILINGOFFSETX "ceilingoffsetx"
#define FIELD_SECTOR_CEILINGOFFSETY "ceilingoffsety"
#define FIELD_SECTOR_TOPMAP         "colormaptop"
#define FIELD_SECTOR_MIDMAP         "colormapmid"
#define FIELD_SECTOR_BOTTOMMAP      "colormapbottom"
#define FIELD_SECTOR_PORTALFLAGS_F  "portalflags.floor"
#define FIELD_SECTOR_PORTALFLAGS_C  "portalflags.ceiling"
#define FIELD_SECTOR_OVERLAYALPHA_F "overlayalpha.floor"
#define FIELD_SECTOR_OVERLAYALPHA_C "overlayalpha.ceiling"

// mapthing options and related data structures

static cfg_opt_t mapthing_opts[] =
{
   CFG_INT(FIELD_NUM,     0,  CFGF_NONE),
   CFG_INT(FIELD_TID,     0,  CFGF_NONE),
   CFG_STR(FIELD_TYPE,    "", CFGF_NONE),
   CFG_STR(FIELD_OPTIONS, "", CFGF_NONE),
   CFG_STR(FIELD_ARGS,    0,  CFGF_LIST),
   CFG_INT(FIELD_HEIGHT,  0,  CFGF_NONE),
   CFG_END()
};

// mapthing flag values and mnemonics

static dehflags_t mapthingflags[] =
{
   { "EASY",      MTF_EASY },
   { "NORMAL",    MTF_NORMAL },
   { "HARD",      MTF_HARD },
   { "AMBUSH",    MTF_AMBUSH },
   { "NOTSINGLE", MTF_NOTSINGLE },
   { "NOTDM",     MTF_NOTDM },
   { "NOTCOOP",   MTF_NOTCOOP },
   { "FRIEND",    MTF_FRIEND },
   { "DORMANT",   MTF_DORMANT },
   { NULL,        0 }
};

static dehflagset_t mt_flagset =
{
   mapthingflags, // flaglist
   0,             // mode
};

// linedef options and related data structures

// line special callback
static int E_LineSpecCB(cfg_t *cfg, cfg_opt_t *opt, const char *value,
                        void *result);

static cfg_opt_t linedef_opts[] =
{
   CFG_INT(FIELD_LINE_NUM,         0, CFGF_NONE),
   CFG_INT_CB(FIELD_LINE_SPECIAL,  0, CFGF_NONE, E_LineSpecCB),
   CFG_INT(FIELD_LINE_TAG,         0, CFGF_NONE),
   CFG_STR(FIELD_LINE_EXTFLAGS,   "", CFGF_NONE),
   CFG_STR(FIELD_LINE_ARGS,        0, CFGF_LIST),
   CFG_INT(FIELD_LINE_ID,         -1, CFGF_NONE),
   CFG_FLOAT(FIELD_LINE_ALPHA,   1.0, CFGF_NONE),
   CFG_END()
};

// linedef extended flag values and mnemonics

static dehflags_t extlineflags[] =
{
   { "CROSS",    EX_ML_CROSS    },
   { "USE",      EX_ML_USE      },
   { "IMPACT",   EX_ML_IMPACT   },
   { "PUSH",     EX_ML_PUSH     },
   { "PLAYER",   EX_ML_PLAYER   },
   { "MONSTER",  EX_ML_MONSTER  },
   { "MISSILE",  EX_ML_MISSILE  },
   { "REPEAT",   EX_ML_REPEAT   },
   { "1SONLY",   EX_ML_1SONLY   },
   { "ADDITIVE", EX_ML_ADDITIVE },
   { "BLOCKALL", EX_ML_BLOCKALL },
   { NULL,       0              }
};

static dehflagset_t ld_flagset =
{
   extlineflags, // flaglist
   0,            // mode
};

// haleyjd 06/26/07: sector options and related data structures

static cfg_opt_t sector_opts[] =
{
   CFG_INT(FIELD_SECTOR_NUM,              0,          CFGF_NONE),
   CFG_STR(FIELD_SECTOR_FLAGS,            "",         CFGF_NONE),
   CFG_STR(FIELD_SECTOR_FLAGSADD,         "",         CFGF_NONE),
   CFG_STR(FIELD_SECTOR_FLAGSREM,         "",         CFGF_NONE),
   CFG_INT(FIELD_SECTOR_DAMAGE,           0,          CFGF_NONE),
   CFG_INT(FIELD_SECTOR_DAMAGEMASK,       0,          CFGF_NONE),
   CFG_STR(FIELD_SECTOR_DAMAGEMOD,        "Unknown",  CFGF_NONE),
   CFG_STR(FIELD_SECTOR_DAMAGEFLAGS,      "",         CFGF_NONE),
   CFG_STR(FIELD_SECTOR_DMGFLAGSADD,      "",         CFGF_NONE),
   CFG_STR(FIELD_SECTOR_DMGFLAGSREM,      "",         CFGF_NONE),
   CFG_STR(FIELD_SECTOR_FLOORTERRAIN,     "@flat",    CFGF_NONE),
   CFG_STR(FIELD_SECTOR_CEILINGTERRAIN,   "@flat",    CFGF_NONE),
   CFG_STR(FIELD_SECTOR_TOPMAP,           "@default", CFGF_NONE),
   CFG_STR(FIELD_SECTOR_MIDMAP,           "@default", CFGF_NONE),
   CFG_STR(FIELD_SECTOR_BOTTOMMAP,        "@default", CFGF_NONE),

   CFG_FLOAT(FIELD_SECTOR_FLOOROFFSETX,   0.0,        CFGF_NONE),
   CFG_FLOAT(FIELD_SECTOR_FLOOROFFSETY,   0.0,        CFGF_NONE),
   CFG_FLOAT(FIELD_SECTOR_CEILINGOFFSETX, 0.0,        CFGF_NONE),
   CFG_FLOAT(FIELD_SECTOR_CEILINGOFFSETY, 0.0,        CFGF_NONE),
   CFG_FLOAT(FIELD_SECTOR_FLOORANGLE,     0.0,        CFGF_NONE),
   CFG_FLOAT(FIELD_SECTOR_CEILINGANGLE,   0.0,        CFGF_NONE),

   // haleyjd 01/08/11: portal properties
   CFG_STR(FIELD_SECTOR_PORTALFLAGS_F,     "",        CFGF_NONE),
   CFG_STR(FIELD_SECTOR_PORTALFLAGS_C,     "",        CFGF_NONE),
   CFG_INT_CB(FIELD_SECTOR_OVERLAYALPHA_F, 255,       CFGF_NONE, E_TranslucCB2),
   CFG_INT_CB(FIELD_SECTOR_OVERLAYALPHA_C, 255,       CFGF_NONE, E_TranslucCB2),

   CFG_END()
};

static dehflags_t sectorflags[] =
{
   { "SECRET",        SECF_SECRET        },
   { "FRICTION",      SECF_FRICTION      },
   { "PUSH",          SECF_PUSH          },
   { "KILLSOUND",     SECF_KILLSOUND     },
   { "KILLMOVESOUND", SECF_KILLMOVESOUND },
   { NULL,            0                  }
};

static dehflagset_t sector_flagset =
{
   sectorflags, // flaglist
   0,           // mode
};

static dehflags_t sectordamageflags[] =
{
   { "LEAKYSUIT",  SDMG_LEAKYSUIT  },
   { "IGNORESUIT", SDMG_IGNORESUIT },
   { "ENDGODMODE", SDMG_ENDGODMODE },
   { "EXITLEVEL",  SDMG_EXITLEVEL  },
   { "TERRAINHIT", SDMG_TERRAINHIT },
   { NULL,         0               }
};

static dehflagset_t sectordamage_flagset =
{
   sectordamageflags, // flaglist
   0                  // mode
};

static dehflags_t sectorportalflags[] =
{
   { "DISABLED",     PF_DISABLED     },
   { "NORENDER",     PF_NORENDER     },
   { "NOPASS",       PF_NOPASS       },
   { "BLOCKSOUND",   PF_BLOCKSOUND   },
   { "OVERLAY",      PS_OVERLAY      },
   { "ADDITIVE",     PS_ADDITIVE     },
   { "USEGLOBALTEX", PS_USEGLOBALTEX },
   { NULL,           0               }
};

static dehflagset_t sectorportal_flagset =
{
   sectorportalflags, // flaglist
   0                  // mode
};

//
// Line Special Information
//

static struct exlinespec
{
   int16_t special;     // line special number
   const char *name;    // descriptive name
   unsigned int next;   // for hashing
} exlinespecs[] =
{
   // Normal DOOM/BOOM extended specials.
   // Most of these have horrible names, but they won't be
   // used much via ExtraData, so it doesn't matter.
   {   0, "None" },
   {   1, "DR_RaiseDoor_Slow_Mon" },
   {   2, "W1_OpenDoor_Slow" },
   {   3, "W1_CloseDoor_Slow" },
   {   4, "W1_RaiseDoor_Slow" },
   {   5, "W1_Floor_UpLnC_Slow" },
   {   6, "W1_StartCrusher_Fast" },
   {   7, "S1_Stairs_Up8_Slow" },
   {   8, "W1_Stairs_Up8_Slow" },
   {   9, "S1_Donut" },
   {  10, "W1_Plat_Lift_Slow" },
   {  11, "S1_ExitLevel" },
   {  12, "W1_Light_MaxNeighbor" },
   {  13, "W1_Light_Set255" },
   {  14, "S1_Plat_Up32_c0t_Slow" },
   {  15, "S1_Plat_Up24_cTt_Slow" },
   {  16, "W1_Door_CloseWait30" },
   {  17, "W1_Light_Blink" },
   {  18, "S1_Floor_UpNnF_Slow" },
   {  19, "W1_Floor_DnHnF_Slow" },
   {  20, "S1_Plat_UpNnF_c0t_Slow" },
   {  21, "S1_Plat_Lift_Slow" },
   {  22, "W1_Plat_UpNnF_c0t_Slow" },
   {  23, "S1_Floor_DnLnF_Slow" },
   {  24, "G1_Floor_UpLnC_Slow" },
   {  25, "W1_StartCrusher_Slow" },
   {  26, "DR_RaiseLockedDoor_Blue_Slow" },
   {  27, "DR_RaiseLockedDoor_Yellow_Slow" },
   {  28, "DR_RaiseLockedDoor_Red_Slow" },
   {  29, "S1_RaiseDoor_Slow" },
   {  30, "W1_Floor_UpsT_Slow" },
   {  31, "D1_OpenDoor_Slow" },
   {  32, "D1_OpenLockedDoor_Blue_Slow" },
   {  33, "D1_OpenLockedDoor_Red_Slow" },
   {  34, "D1_OpenLockedDoor_Yellow_Slow" },
   {  35, "W1_Light_Set35" },
   {  36, "W1_Floor_DnHnFp8_Fast" },
   {  37, "W1_Floor_DnLnF_cSn_Slow" },
   {  38, "W1_Floor_DnLnF_Slow" },
   {  39, "W1_TeleportToSpot" },
   {  40, "W1_Ceiling_UpHnC_Slow" },
   {  41, "S1_Ceiling_DnF_Fast" },
   {  42, "SR_CloseDoor_Slow" },
   {  43, "SR_Ceiling_DnF_Fast" },
   {  44, "W1_Ceiling_DnFp8_Slow" },
   {  45, "SR_Floor_DnHnF_Slow" },
   {  46, "GR_OpenDoor_Slow" },
   {  47, "G1_Plat_UpNnF_c0t_Slow" },
   {  48, "ScrollLeft" },
   {  49, "S1_StartCrusher_Slow" },
   {  50, "S1_CloseDoor_Slow" },
   {  51, "S1_ExitSecret" },
   {  52, "W1_ExitLevel" },
   {  53, "W1_Plat_LoHiPerpetual" },
   {  54, "W1_StopPlat" },
   {  55, "S1_Floor_UpLnCm8_Slow_Crush" },
   {  56, "W1_Floor_UpLnCm8_Slow_Crush" },
   {  57, "W1_StopCrusher" },
   {  58, "W1_Floor_Up24_Slow" },
   {  59, "W1_Floor_Up24_cSt_Slow" },
   {  60, "SR_Floor_DnLnF_Slow" },
   {  61, "SR_OpenDoor_Slow" },
   {  62, "SR_Plat_Lift_Slow" },
   {  63, "SR_RaiseDoor_Slow" },
   {  64, "SR_Floor_UpLnC_Slow" },
   {  65, "SR_Floor_UpLnCm8_Slow_Crush" },
   {  66, "SR_Plat_Up24_cTt_Slow" },
   {  67, "SR_Plat_Up32_c0t_Slow" },
   {  68, "SR_Plat_UpNnF_c0t_Slow" },
   {  69, "SR_Floor_UpNnF_Slow" },
   {  70, "SR_Floor_DnHnFp8_Fast" },
   {  71, "S1_Floor_DnHnFp8_Fast" },
   {  72, "WR_Ceiling_DnFp8_Slow" },
   {  73, "WR_StartCrusher_Slow" },
   {  74, "WR_StopCrusher" },
   {  75, "WR_CloseDoor_Slow" },
   {  76, "WR_Door_CloseWait30" },
   {  77, "WR_StartCrusher_Fast" },
   {  78, "SR_Floor_cSn" },
   {  79, "WR_Light_Set35" },
   {  80, "WR_Light_MaxNeighbor" },
   {  81, "WR_Light_Set255" },
   {  82, "WR_Floor_DnLnF_Slow" },
   {  83, "WR_Floor_DnHnF_Slow" },
   {  84, "WR_Floor_DnLnF_cSn_Slow" },
   {  85, "ScrollRight" },
   {  86, "WR_OpenDoor_Slow" },
   {  87, "WR_Plat_LoHiPerpetual" },
   {  88, "WR_Plat_Lift_Slow" },
   {  89, "WR_StopPlat" },
   {  90, "WR_RaiseDoor_Slow" },
   {  91, "WR_Floor_UpLnC_Slow" },
   {  92, "WR_Floor_Up24_Slow" },
   {  93, "WR_Floor_Up24_cSt_Slow" },
   {  94, "WR_Floor_UpLnCm8_Slow_Crush" },
   {  95, "WR_Plat_UpNnF_c0t_Slow" },
   {  96, "WR_Floor_UpsT_Slow" },
   {  97, "WR_TeleportToSpot" },
   {  98, "WR_Floor_DnHnFp8_Fast" },
   {  99, "WR_OpenLockedDoor_Blue_Fast" },
   { 100, "W1_Stairs_Up16_Fast" },
   { 101, "S1_Floor_UpLnC_Slow" },
   { 102, "S1_Floor_DnHnF_Slow" },
   { 103, "S1_OpenDoor_Slow" },
   { 104, "W1_Light_MinNeighbor" },
   { 105, "WR_RaiseDoor_Fast" },
   { 106, "WR_OpenDoor_Fast" },
   { 107, "WR_CloseDoor_Fast" },
   { 108, "W1_RaiseDoor_Fast" },
   { 109, "W1_OpenDoor_Fast" },
   { 110, "W1_CloseDoor_Fast" },
   { 111, "S1_RaiseDoor_Fast" },
   { 112, "S1_OpenDoor_Fast" },
   { 113, "S1_CloseDoor_Fast" },
   { 114, "SR_RaiseDoor_Fast" },
   { 115, "SR_OpenDoor_Fast" },
   { 116, "SR_CloseDoor_Fast" },
   { 117, "DR_RaiseDoor_Fast" },
   { 118, "D1_OpenDoor_Fast" },
   { 119, "W1_Floor_UpNnF_Slow" },
   { 120, "WR_Plat_Lift_Fast" },
   { 121, "W1_Plat_Lift_Fast" },
   { 122, "S1_Plat_Lift_Fast" },
   { 123, "SR_Plat_Lift_Fast" },
   { 124, "W1_ExitSecret" },
   { 125, "W1_TeleportToSpot_MonOnly" },
   { 126, "WR_TeleportToSpot_MonOnly" },
   { 127, "S1_Stairs_Up16_Fast" },
   { 128, "WR_Floor_UpNnF_Slow" },
   { 129, "WR_Floor_UpNnF_Fast" },
   { 130, "W1_Floor_UpNnF_Fast" },
   { 131, "S1_Floor_UpNnF_Fast" },
   { 132, "SR_Floor_UpNnF_Fast" },
   { 133, "S1_OpenLockedDoor_Blue_Fast" },
   { 134, "SR_OpenLockedDoor_Red_Fast" },
   { 135, "S1_OpenLockedDoor_Red_Fast" },
   { 136, "SR_OpenLockedDoor_Yellow_Fast" },
   { 137, "S1_OpenLockedDoor_Yellow_Fast" },
   { 138, "SR_Light_Set255" },
   { 139, "SR_Light_Set35" },
   { 140, "S1_Floor_Up512_Slow" },
   { 141, "W1_StartCrusher_Silent_Slow" },
   { 142, "W1_Floor_Up512_Slow" },
   { 143, "W1_Plat_Up24_cTt_Slow" },
   { 144, "W1_Plat_Up32_c0t_Slow" },
   { 145, "W1_Ceiling_DnF_Fast" },
   { 146, "W1_Donut" },
   { 147, "WR_Floor_Up512_Slow" },
   { 148, "WR_Plat_Up24_cTt_Slow" },
   { 149, "WR_Plat_Up32_c0t_Slow" },
   { 150, "WR_StartCrusher_Silent_Slow" },
   { 151, "WR_Ceiling_UpHnC_Slow" },
   { 152, "WR_Ceiling_DnF_Fast" },
   { 153, "W1_Floor_cSt" },
   { 154, "WR_Floor_cSt" },
   { 155, "WR_Donut" },
   { 156, "WR_Light_Blink" },
   { 157, "WR_Light_MinNeighbor" },
   { 158, "S1_Floor_UpsT_Slow" },
   { 159, "S1_Floor_DnLnF_cSn_Slow" },
   { 160, "S1_Floor_Up24_cSt_Slow" },
   { 161, "S1_Floor_Up24_Slow" },
   { 162, "S1_Plat_LoHiPerpetual" },
   { 163, "S1_StopPlat" },
   { 164, "S1_StartCrusher_Fast" },
   { 165, "S1_StartCrusher_Silent_Slow" },
   { 166, "S1_Ceiling_UpHnC_Slow" },
   { 167, "S1_Ceiling_DnFp8_Slow" },
   { 168, "S1_StopCrusher" },
   { 169, "S1_Light_MaxNeighbor" },
   { 170, "S1_Light_Set35" },
   { 171, "S1_Light_Set255" },
   { 172, "S1_Light_Blink" },
   { 173, "S1_Light_MinNeighbor" },
   { 174, "S1_TeleportToSpot" },
   { 175, "S1_Door_CloseWait30" },
   { 176, "SR_Floor_UpsT_Slow" },
   { 177, "SR_Floor_DnLnF_cSn_Slow" },
   { 178, "SR_Floor_Up512_Slow" },
   { 179, "SR_Floor_Up24_cSt_Slow" },
   { 180, "SR_Floor_Up24_Slow" },
   { 181, "SR_Plat_LoHiPerpetual" },
   { 182, "SR_StopPlat" },
   { 183, "SR_StartCrusher_Fast" },
   { 184, "SR_StartCrusher_Slow" },
   { 185, "SR_StartCrusher_Silent_Slow" },
   { 186, "SR_Ceiling_UpHnC_Slow" },
   { 187, "SR_Ceiling_DnFp8_Slow" },
   { 188, "SR_StopCrusher" },
   { 189, "S1_Floor_cSt" },
   { 190, "SR_Floor_cSt" },
   { 191, "SR_Donut" },
   { 192, "SR_Light_MaxNeighbor" },
   { 193, "SR_Light_Blink" },
   { 194, "SR_Light_MinNeighbor" },
   { 195, "SR_TeleportToSpot" },
   { 196, "SR_Door_CloseWait30" },
   { 197, "G1_ExitLevel" },
   { 198, "G1_ExitSecret" },
   { 199, "W1_Ceiling_DnLnC_Slow" },
   { 200, "W1_Ceiling_DnHnF_Slow" },
   { 201, "WR_Ceiling_DnLnC_Slow" },
   { 202, "WR_Ceiling_DnHnF_Slow" },
   { 203, "S1_Ceiling_DnLnC_Slow" },
   { 204, "S1_Ceiling_DnHnF_Slow" },
   { 205, "SR_Ceiling_DnLnC_Slow" },
   { 206, "SR_Ceiling_DnHnF_Slow" },
   { 207, "W1_TeleportToSpot_Orient_Silent" },
   { 208, "WR_TeleportToSpot_Orient_Silent" },
   { 209, "S1_TeleportToSpot_Orient_Silent" },
   { 210, "SR_TeleportToSpot_Orient_Silent" },
   { 211, "SR_Plat_CeilingToggle_Instant" },
   { 212, "WR_Plat_CeilingToggle_Instant" },
   { 213, "TransferLight_Floor" },
   { 214, "ScrollCeiling_Accel" },
   { 215, "ScrollFloor_Accel" },
   { 216, "CarryObjects_Accel" },
   { 217, "ScrollFloorCarryObjects_Accel" },
   { 218, "ScrollWallWithFlat_Accel" },
   { 219, "W1_Floor_DnNnF_Slow" },
   { 220, "WR_Floor_DnNnF_Slow" },
   { 221, "S1_Floor_DnNnF_Slow" },
   { 222, "SR_Floor_DnNnF_Slow" },
   { 223, "TransferFriction" },
   { 224, "TransferWind" },
   { 225, "TransferCurrent" },
   { 226, "TransferPointForce" },
   { 227, "W1_Elevator_NextHi" },
   { 228, "WR_Elevator_NextHi" },
   { 229, "S1_Elevator_NextHi" },
   { 230, "SR_Elevator_NextHi" },
   { 231, "W1_Elevator_NextLo" },
   { 232, "WR_Elevator_NextLo" },
   { 233, "S1_Elevator_NextLo" },
   { 234, "SR_Elevator_NextLo" },
   { 235, "W1_Elevator_Current" },
   { 236, "WR_Elevator_Current" },
   { 237, "S1_Elevator_Current" },
   { 238, "SR_Elevator_Current" },
   { 239, "W1_Floor_cSn" },
   { 240, "WR_Floor_cSn" },
   { 241, "S1_Floor_cSn" },
   { 242, "TransferHeights" },
   { 243, "W1_TeleportToLine" },
   { 244, "WR_TeleportToLine" },
   { 245, "ScrollCeiling_ByHeight" },
   { 246, "ScrollFloor_ByHeight" },
   { 247, "CarryObjects_ByHeight" },
   { 248, "ScrollFloorCarryObjects_ByHeight" },
   { 249, "ScrollWallWithFlat_ByHeight" },
   { 250, "ScrollCeiling" },
   { 251, "ScrollFloor" },
   { 252, "CarryObjects" },
   { 253, "ScrollFloorCarryObjects" },
   { 254, "ScrollWallWithFlat" },
   { 255, "ScrollWallByOffsets" },
   { 256, "WR_Stairs_Up8_Slow" },
   { 257, "WR_Stairs_Up16_Fast" },
   { 258, "SR_Stairs_Up8_Slow" },
   { 259, "SR_Stairs_Up16_Fast" },
   { 260, "Translucent" },
   { 261, "TransferLight_Ceiling" },
   { 262, "W1_TeleportToLine_Reverse" },
   { 263, "WR_TeleportToLine_Reverse" },
   { 264, "W1_TeleportToLine_Reverse_MonOnly" },
   { 265, "WR_TeleportToLine_Reverse_MonOnly" },
   { 266, "W1_TeleportToLine_MonOnly" },
   { 267, "WR_TeleportToLine_MonOnly" },
   { 268, "W1_TeleportToSpot_Silent_MonOnly" },
   { 269, "WR_TeleportToSpot_Silent_MonOnly" },
   { 270, "ExtraDataSpecial" },
   { 271, "TransferSky" },
   { 272, "TransferSkyFlipped" },
   { 273, "WR_StartScript_1S" },
   { 274, "W1_StartScript" },
   { 275, "W1_StartScript_1S" },
   { 276, "SR_StartScript" },
   { 277, "S1_StartScript" },
   { 278, "GR_StartScript" },
   { 279, "G1_StartScript" },
   { 280, "WR_StartScript" },
   { 281, "3DMidTex_MoveWithFloor" },
   { 282, "3DMidTex_MoveWithCeiling" },
   { 283, "Portal_PlaneCeiling" },
   { 284, "Portal_PlaneFloor" },
   { 285, "Portal_PlaneFloorCeiling" },
   { 286, "Portal_HorizonCeiling" },
   { 287, "Portal_HorizonFloor" },
   { 288, "Portal_HorizonFloorCeiling" },
   { 289, "Portal_LineTransfer" },
   { 290, "Portal_SkyboxCeiling" },
   { 291, "Portal_SkyboxFloor" },
   { 292, "Portal_SkyboxFloorCeiling" },
   { 293, "TransferHereticWind" },
   { 294, "TransferHereticCurrent" },
   { 295, "Portal_AnchoredCeiling" },
   { 296, "Portal_AnchoredFloor" },
   { 297, "Portal_AnchoredFloorCeiling" },
   { 298, "Portal_AnchorLine" },
   { 299, "Portal_AnchorLineFloor" },

   // Parameterized specials.
   // These have nice names because their specifics come from ExtraData
   // line arguments, ala Hexen, instead of being hardcoded.
   { 300, "Door_Raise" },
   { 301, "Door_Open" },
   { 302, "Door_Close" },
   { 303, "Door_CloseWaitOpen" },
   { 304, "Door_WaitRaise" },
   { 305, "Door_WaitClose" },
   { 306, "Floor_RaiseToHighest" },
   { 307, "Floor_LowerToHighest" },
   { 308, "Floor_RaiseToLowest" },
   { 309, "Floor_LowerToLowest" },
   { 310, "Floor_RaiseToNearest" },
   { 311, "Floor_LowerToNearest" },
   { 312, "Floor_RaiseToLowestCeiling" },
   { 313, "Floor_LowerToLowestCeiling" },
   { 314, "Floor_RaiseToCeiling" },
   { 315, "Floor_RaiseByTexture" },
   { 316, "Floor_LowerByTexture" },
   { 317, "Floor_RaiseByValue" },
   { 318, "Floor_LowerByValue" },
   { 319, "Floor_MoveToValue" },
   { 320, "Floor_RaiseInstant" },
   { 321, "Floor_LowerInstant" },
   { 322, "Floor_ToCeilingInstant" },
   { 323, "Ceiling_RaiseToHighest" },
   { 324, "Ceiling_ToHighestInstant" },
   { 325, "Ceiling_RaiseToNearest" },
   { 326, "Ceiling_LowerToNearest" },
   { 327, "Ceiling_RaiseToLowest" },
   { 328, "Ceiling_LowerToLowest" },
   { 329, "Ceiling_RaiseToHighestFloor" },
   { 330, "Ceiling_LowerToHighestFloor" },
   { 331, "Ceiling_ToFloorInstant" },
   { 332, "Ceiling_LowerToFloor" },
   { 333, "Ceiling_RaiseByTexture" },
   { 334, "Ceiling_LowerByTexture" },
   { 335, "Ceiling_RaiseByValue" },
   { 336, "Ceiling_LowerByValue" },
   { 337, "Ceiling_MoveToValue" },
   { 338, "Ceiling_RaiseInstant" },
   { 339, "Ceiling_LowerInstant" },
   { 340, "Stairs_BuildUpDoom" },
   { 341, "Stairs_BuildDownDoom" },
   { 342, "Stairs_BuildUpDoomSync" },
   { 343, "Stairs_BuildDownDoomSync" },

   // SoM: two-way portals
   { 344, "Portal_TwowayCeiling" },
   { 345, "Portal_TwowayFloor" },
   { 346, "Portal_TwowayAnchorLine" },
   { 347, "Portal_TwowayAnchorLineFloor" },

   // Polyobjects
   { 348, "Polyobj_StartLine" },
   { 349, "Polyobj_ExplicitLine" },
   { 350, "Polyobj_DoorSlide" },
   { 351, "Polyobj_DoorSwing" },
   { 352, "Polyobj_Move" },
   { 353, "Polyobj_OR_Move" },
   { 354, "Polyobj_RotateRight" },
   { 355, "Polyobj_OR_RotateRight" },
   { 356, "Polyobj_RotateLeft" },
   { 357, "Polyobj_OR_RotateLeft" },

   // SoM: linked portal types
   { 358, "Portal_LinkedCeiling" },
   { 359, "Portal_LinkedFloor" },
   { 360, "Portal_LinkedAnchorLine" },
   { 361, "Portal_LinkedAnchorLineFloor" },

   { 362, "Pillar_Build" },
   { 363, "Pillar_BuildAndCrush" },
   { 364, "Pillar_Open" },
   { 365, "ACS_Execute" },
   { 366, "ACS_Suspend" },
   { 367, "ACS_Terminate" },
   { 368, "Light_RaiseByValue" },
   { 369, "Light_LowerByValue" },
   { 370, "Light_ChangeToValue" },
   { 371, "Light_Fade" },
   { 372, "Light_Glow" },
   { 373, "Light_Flicker" },
   { 374, "Light_Strobe" },
   { 375, "Radius_Quake" },

   { 376, "Portal_LinkedLineToLine" },
   { 377, "Portal_LinkedLineToLineAnchor" },

   { 378, "Line_SetIdentification" },

   // Sector floor/ceiling attachment
   { 379, "Attach_SetCeilingControl" },
   { 380, "Attach_SetFloorControl" },
   { 381, "Attach_FloorToControl" },
   { 382, "Attach_CeilingToControl" },
   { 383, "Attach_MirrorFloorToControl" },
   { 384, "Attach_MirrorCeilingToControl" },

   // Apply tagged portals to frontsector
   { 385, "Portal_ApplyToFrontsector" },

   // Slopes
   { 386, "Slope_FrontsectorFloor" },
   { 387, "Slope_FrontsectorCeiling" },
   { 388, "Slope_FrontsectorFloorAndCeiling" },
   { 389, "Slope_BacksectorFloor" },
   { 390, "Slope_BacksectorCeiling" },
   { 391, "Slope_BacksectorFloorAndCeiling" },
   { 392, "Slope_BackFloorAndFrontCeiling" },
   { 393, "Slope_BackCeilingAndFrontFloor" },

   { 394, "Slope_FrontFloorToTaggedSlope" },
   { 395, "Slope_FrontCeilingToTaggedSlope" },
   { 396, "Slope_FrontFloorAndCeilingToTaggedSlope" },

   // Misc Hexen specials
   { 397, "Floor_Waggle" },
   { 398, "Thing_Spawn"  },
   { 399, "Thing_SpawnNoFog" },
   { 400, "Teleport_EndGame" },

   // ExtraData sector control line
   { 401, "ExtraDataSector" },

   // More misc Hexen specials
   { 402, "Thing_Projectile" },
   { 403, "Thing_ProjectileGravity" },
   { 404, "Thing_Activate" },
   { 405, "Thing_Deactivate" },
};

#define NUMLINESPECS (sizeof(exlinespecs) / sizeof(struct exlinespec))

// primary ExtraData options table

static cfg_opt_t ed_opts[] =
{
   CFG_SEC(SEC_MAPTHING, mapthing_opts, CFGF_MULTI|CFGF_NOCASE),
   CFG_SEC(SEC_LINEDEF,  linedef_opts,  CFGF_MULTI|CFGF_NOCASE),
   CFG_SEC(SEC_SECTOR,   sector_opts,   CFGF_MULTI|CFGF_NOCASE),
   CFG_END()
};

//=============================================================================
//
// Shared Processing Routines
//

//
// E_ParseArg
//
// Parses a single element of an args list.
//
static void E_ParseArg(const char *str, int *dest)
{
   // currently only integers are supported
   *dest = strtol(str, NULL, 0);
}

//=============================================================================
//
// Begin Mapthing Routines
//

//
// E_EDThingForRecordNum
//
// Returns an index into EDThings for the given record number.
// Returns numEDMapThings if no such record exists.
//
static unsigned int E_EDThingForRecordNum(int recnum)
{
   unsigned int num;
   int key = recnum % NUMMTCHAINS;

   num = mapthing_chains[key];
   while(num != numEDMapThings && EDThings[num].recordnum != recnum)
   {
      num = EDThings[num].next;
   }

   return num;
}

//
// E_ParseTypeField
//
// Parses thing type fields in ExtraData. Allows resolving of
// EDF thingtype mnemonics to their corresponding doomednums.
//
static int E_ParseTypeField(const char *value)
{
   long num;
   int  i;
   char prefix[16];
   const char *colonloc, *strval;
   char *numpos = NULL;

   num = strtol(value, &numpos, 0);

   memset(prefix, 0, 16);
   colonloc = E_ExtractPrefix(value, prefix, 16);

   // If has a colon, or is otherwise not just a number...
   if(colonloc || (numpos && *numpos != '\0'))
   {
      if(colonloc) // allow a thing: prefix for compatibility
         strval = colonloc + 1;
      else
         strval = value; // use input directly

      // translate from EDF mnemonic to doomednum
      i = mobjinfo[E_SafeThingName(strval)]->doomednum;
   }
   else
   {
      // integer value
      i = (int)num;
   }

   // don't return -1, use no-op zero in that case
   // (this'll work even if somebody messed with 'Unknown')
   return (i >= 0 ? i : 0);
}

//
// E_ParseThingArgs
//
// Parses the mapthing args list.
//
static void E_ParseThingArgs(mapthing_t *mte, cfg_t *sec)
{
   unsigned int i, numargs;

   // count number of args given in list
   numargs = cfg_size(sec, FIELD_ARGS);

   // init all args to 0
   for(i = 0; i < NUMMTARGS; ++i)
      mte->args[i] = 0;

   // parse the given args values
   for(i = 0; i < numargs && i < NUMMTARGS; ++i)
   {
      const char *argstr = cfg_getnstr(sec, FIELD_ARGS, i);
      E_ParseArg(argstr, &(mte->args[i]));
   }
}

//
// E_ProcessEDThings
//
// Allocates and processes ExtraData mapthing records.
//
static void E_ProcessEDThings(cfg_t *cfg)
{
   unsigned int i;

   // get the number of mapthing records
   numEDMapThings = cfg_size(cfg, SEC_MAPTHING);

   // if none, we're done
   if(!numEDMapThings)
      return;

   // allocate the mapthing_t structures
   EDThings = (mapthing_t *)(Z_Malloc(numEDMapThings * sizeof(mapthing_t), PU_LEVEL, NULL));

   // initialize the hash chains
   for(i = 0; i < NUMMTCHAINS; ++i)
      mapthing_chains[i] = numEDMapThings;

   // read fields
   for(i = 0; i < numEDMapThings; i++)
   {
      cfg_t *thingsec;
      const char *tempstr;
      int tempint;

      thingsec = cfg_getnsec(cfg, SEC_MAPTHING, i);

      // get the record number
      tempint = EDThings[i].recordnum = cfg_getint(thingsec, FIELD_NUM);

      // guard against duplicate record numbers
      if(E_EDThingForRecordNum(tempint) != numEDMapThings)
         I_Error("E_ProcessEDThings: duplicate record number %d\n", tempint);

      // hash this ExtraData mapthing record by its recordnum field
      tempint = EDThings[i].recordnum % NUMMTCHAINS;
      EDThings[i].next = mapthing_chains[tempint];
      mapthing_chains[tempint] = i;

      // standard fields

      // type
      tempstr = cfg_getstr(thingsec, FIELD_TYPE);
      EDThings[i].type = (int16_t)(E_ParseTypeField(tempstr));

      // it is not allowed to spawn an ExtraData control object via
      // ExtraData, but the error is tolerated by changing it to an
      // "Unknown" thing
      if(EDThings[i].type == ED_CTRL_DOOMEDNUM)
         EDThings[i].type = mobjinfo[UnknownThingType]->doomednum;

      // options
      tempstr = cfg_getstr(thingsec, FIELD_OPTIONS);
      if(*tempstr == '\0')
         EDThings[i].options = 0;
      else
         EDThings[i].options = (int16_t)(E_ParseFlags(tempstr, &mt_flagset));

      // extended fields

      // get TID field
      EDThings[i].tid = (int16_t)cfg_getint(thingsec, FIELD_TID);
      if(EDThings[i].tid < 0)
         EDThings[i].tid = 0; // you cannot specify a reserved TID

      // get args
      E_ParseThingArgs(&EDThings[i], thingsec);

      // get height
      EDThings[i].height = (int16_t)cfg_getint(thingsec, FIELD_HEIGHT);

      // TODO: any other new fields
   }
}

//=============================================================================
//
// Begin Linedef Routines
//

//
// E_EDLineForRecordNum
//
// Returns an index into EDLines for the given record number.
// Returns numEDLines if no such record exists.
//
static unsigned int E_EDLineForRecordNum(int recnum)
{
   unsigned int num;
   int key = recnum % NUMLDCHAINS;

   num = linedef_chains[key];
   while(num != numEDLines && EDLines[num].recordnum != recnum)
   {
      num = EDLines[num].next;
   }

   return num;
}

#define NUMLSPECCHAINS 307
static unsigned int linespec_chains[NUMLSPECCHAINS];

//
// E_InitLineSpecHash
//
// Sets up a threaded hash table through the exlinespecs array to
// avoid untimely name -> number resolution.
//
static void E_InitLineSpecHash(void)
{
   unsigned int i;

   // init all chains
   for(i = 0; i < NUMLSPECCHAINS; ++i)
      linespec_chains[i] = NUMLINESPECS;

   // add all specials to hash table
   for(i = 0; i < NUMLINESPECS; ++i)
   {
      unsigned int key = D_HashTableKey(exlinespecs[i].name) % NUMLSPECCHAINS;
      exlinespecs[i].next = linespec_chains[key];
      linespec_chains[key] = i;
   }
}

//
// E_LineSpecForName
//
// Gets a line special for a name.
//
int16_t E_LineSpecForName(const char *name)
{
   static bool hash_init = false;
   unsigned int key = D_HashTableKey(name) % NUMLSPECCHAINS;
   unsigned int i;

   // initialize line specials hash first time
   if(!hash_init)
   {
      E_InitLineSpecHash();
      hash_init = true;
   }

   i = linespec_chains[key];

   while(i != NUMLINESPECS && strcasecmp(name, exlinespecs[i].name))
      i = exlinespecs[i].next;

   return (i != NUMLINESPECS ? exlinespecs[i].special : 0);
}

//
// E_GenTypeForName
//
// Returns a base generalized line type for a name.
//
static int16_t E_GenTypeForName(const char *name)
{
   int16_t i = 0;
   static const char *names[] =
   {
      "GenFloor", "GenCeiling", "GenDoor", "GenLockedDoor",
      "GenLift", "GenStairs", "GenCrusher", NULL
   };
   static int16_t bases[] =
   {
      GenFloorBase, GenCeilingBase, GenDoorBase, GenLockedBase,
      GenLiftBase, GenStairsBase, GenCrusherBase, 0
   };

   while(names[i] && strcasecmp(name, names[i]))
      ++i;

   return bases[i];
}

//
// E_GenTokenizer
//
// A lexer function for generalized specials.
//
static const char *E_GenTokenizer(const char *text, int *index, qstring *token)
{
   char c;
   int state = 0;

   // if we're already at the end, return NULL
   if(text[*index] == '\0')
      return NULL;

   token->clear();

   while((c = text[*index]) != '\0')
   {
      *index += 1;
      switch(state)
      {
      case 0: // default state
         switch(c)
         {
         case ')':     // ignore closing bracket
         case ' ':
         case '\t':    // ignore whitespace
            continue;
         case '"':
            state = 1; // enter quoted part
            continue;
         case '\'':
            state = 2; // enter quoted part (single quote support)
            continue;
         case '(':     // end of current token
         case ',':
            return token->constPtr();
         default:      // everything else == part of value
            *token += c;
            continue;
         }
      case 1: // in quoted area (double quotes)
         if(c == '"') // end of quoted area
            state = 0;
         else
            *token += c; // everything inside is literal
         continue;
      case 2: // in quoted area (single quotes)
         if(c == '\'') // end of quoted area
            state = 0;
         else
            *token += c; // everything inside is literal
         continue;
      default:
         I_Error("E_GenTokenizer: internal error - undefined lexer state\n");
      }
   }

   // return final token, next call will return NULL
   return token->constPtr();
}

//
// E_BooleanArg
//
// Parses a yes/no generalized type argument.
//
static bool E_BooleanArg(const char *str)
{
   return !strcasecmp(str, "yes");
}

//
// E_SpeedArg
//
// Parses a generalized speed argument.
//
static int16_t E_SpeedArg(const char *str)
{
   int16_t i = 0;
   static const char *speeds[] =
   {
      "slow", "normal", "fast", "turbo", NULL
   };

   while(speeds[i] && strcasecmp(str, speeds[i]))
      ++i;

   return (speeds[i] ? i : 0);
}

//
// E_ChangeArg
//
// Parses a floor/ceiling changer type argument.
//
static int16_t E_ChangeArg(const char *str)
{
   int16_t i = 0;
   static const char *changes[] =
   {
      "none", "zero", "texture", "texturetype", NULL
   };

   while(changes[i] && strcasecmp(str, changes[i]))
      ++i;

   return (changes[i] ? i : 0);
}

//
// E_ModelArg
//
// Parses a floor/ceiling changer model argument.
//
static int16_t E_ModelArg(const char *str)
{
   if(!strcasecmp(str, "numeric"))
      return 1;

   // trigger = 0
   return 0;
}

//
// E_FloorTarget
//
// Parses the target argument for a generalized floor type.
//
static int16_t E_FloorTarget(const char *str)
{
   int16_t i = 0;
   static const char *targs[] =
   {
      "HnF", "LnF", "NnF", "LnC", "C", "sT", "24", "32", NULL
   };

   while(targs[i] && strcasecmp(str, targs[i]))
      ++i;

   return (targs[i] ? i : 0);
}

//
// E_CeilingTarget
//
// Parses the target argument for a generalized ceiling type.
//
static int16_t E_CeilingTarget(const char *str)
{
   int16_t i = 0;
   static const char *targs[] =
   {
      "HnC", "LnC", "NnC", "HnF", "F", "sT", "24", "32", NULL
   };

   while(targs[i] && strcasecmp(str, targs[i]))
      ++i;

   return (targs[i] ? i : 0);
}

//
// E_DirArg
//
// Parses a direction argument.
//
static int16_t E_DirArg(const char *str)
{
   if(!strcasecmp(str, "up"))
      return 1;

   // down = 0
   return 0;
}

//
// E_DoorTypeArg
//
// Parses a generalized door type argument.
//
static int16_t E_DoorTypeArg(const char *str)
{
   if(!strcasecmp(str, "Open"))
      return 1;
   else if(!strcasecmp(str, "CloseWaitOpen"))
      return 2;
   else if(!strcasecmp(str, "Close"))
      return 3;

   // OpenWaitClose = 0
   return 0;
}

//
// E_DoorDelay
//
// Parses a generalized door delay argument.
//
static int16_t E_DoorDelay(const char *str)
{
   if(*str == '4')
      return 1;
   else if(*str == '9')
      return 2;
   else if(!strcasecmp(str, "30"))
      return 3;

   // "1" = 0
   return 0;
}

//
// E_LockedType
//
// Parses a generalized locked door type argument.
//
static int16_t E_LockedType(const char *str)
{
   if(!strcasecmp(str, "Open"))
      return 1;

   // OpenWaitClose = 0
   return 0;
}

//
// E_LockedKey
//
// Parses the key argument for a locked door.
//
static int16_t E_LockedKey(const char *str)
{
   int16_t i = 0;
   static const char *keys[] =
   {
      "Any", "RedCard", "BlueCard", "YellowCard", "RedSkull", "BlueSkull",
      "YellowSkull", "All", NULL
   };

   while(keys[i] && strcasecmp(str, keys[i]))
      ++i;

   return (keys[i] ? i : 0);
}

//
// E_LiftTarget
//
// Parses the target argument for a generalized lift.
//
static int16_t E_LiftTarget(const char *str)
{
   if(!strcasecmp(str, "NnF"))
      return 1;
   else if(!strcasecmp(str, "LnC"))
      return 2;
   else if(!strcasecmp(str, "Perpetual"))
      return 3;

   // LnF = 0
   return 0;
}

//
// E_LiftDelay
//
// Parses the delay argument for a generalized lift.
//
static int16_t E_LiftDelay(const char *str)
{
   if(*str == '3')
      return 1;
   else if(*str == '5')
      return 2;
   else if(!strcasecmp(str, "10"))
      return 3;

   // "1" = 0
   return 0;
}

//
// E_StepSize
//
// Parses the step size argument for generalized stairs.
//
static int16_t E_StepSize(const char *str)
{
   if(*str == '8')
      return 1;
   else if(!strcasecmp(str, "16"))
      return 2;
   else if(!strcasecmp(str, "24"))
      return 3;

   // "4" = 0
   return 0;
}

//
// E_GenTrigger
//
// Parses the trigger type of a generalized line special.
//
static int16_t E_GenTrigger(const char *str)
{
   int16_t i = 0;
   static const char *trigs[] =
   {
      "W1", "WR", "S1", "SR", "G1", "GR", "D1", "DR", NULL
   };

   while(trigs[i] && strcasecmp(str, trigs[i]))
      ++i;

   return (trigs[i] ? i : 0);
}

// macro for E_ProcessGenSpec

// NEXTTOKEN: calls E_GenTokenizer to get the next token

#define NEXTTOKEN() \
   curtoken = E_GenTokenizer(value, &tok_index, &buffer); \
   if(!curtoken) \
      I_Error("E_ProcessGenSpec: bad generalized line special\n")

//
// E_ProcessGenSpec
//
// Parses a generalized line special string, with the following format:
//
// 'Gen'<type>'('<arg>+')'
//
// <type> = Floor|Ceiling|Door|LockedDoor|Lift|Stairs|Crusher
// <arg>  = <char>+','
//
// Whitespace between args is ignored.
//
// The number and type of arguments are fixed for a given special
// type, and all must be present. This is provided only for completeness,
// and I do not anticipate that it will actually be used, since
// new parameterized specials more or less obsolete the BOOM generalized
// line system. Hence, not a lot of time has been spent making it
// fast or clean.
//
static int16_t E_ProcessGenSpec(const char *value)
{
   qstring buffer;
   const char *curtoken = NULL;
   int t, forc = 0, tok_index = 0;
   int16_t trigger;

   // get special name (starts at beginning, ends at '[')
   // and convert to base trigger type
   NEXTTOKEN();
   trigger = E_GenTypeForName(curtoken);

   switch(trigger)
   {
   case GenFloorBase:
      forc = 1;
      // fall through
   case GenCeilingBase:
      NEXTTOKEN(); // get crushing value
      trigger += (E_BooleanArg(curtoken) << FloorCrushShift);
      NEXTTOKEN(); // get speed
      trigger += (E_SpeedArg(curtoken) << FloorSpeedShift);
      NEXTTOKEN(); // get change type
      trigger += ((t = E_ChangeArg(curtoken)) << FloorChangeShift);
      NEXTTOKEN(); // get change type or monster allowed
      if(t)
         trigger += (E_ModelArg(curtoken) << FloorModelShift);
      else
         trigger += (E_BooleanArg(curtoken) << FloorModelShift);
      NEXTTOKEN(); // get target
      if(forc)
         trigger += (E_FloorTarget(curtoken) << FloorTargetShift);
      else
         trigger += (E_CeilingTarget(curtoken) << FloorTargetShift);
      NEXTTOKEN(); // get direction
      trigger += (E_DirArg(curtoken) << FloorDirectionShift);
      break;
   case GenDoorBase:
      NEXTTOKEN(); // get kind
      trigger += (E_DoorTypeArg(curtoken) << DoorKindShift);
      NEXTTOKEN(); // get speed
      trigger += (E_SpeedArg(curtoken) << DoorSpeedShift);
      NEXTTOKEN(); // get monster
      trigger += (E_BooleanArg(curtoken) << DoorMonsterShift);
      NEXTTOKEN(); // get delay
      trigger += (E_DoorDelay(curtoken) << DoorDelayShift);
      break;
   case GenLockedBase:
      NEXTTOKEN(); // get kind
      trigger += (E_LockedType(curtoken) << LockedKindShift);
      NEXTTOKEN(); // get speed
      trigger += (E_SpeedArg(curtoken) << LockedSpeedShift);
      NEXTTOKEN(); // get key
      trigger += (E_LockedKey(curtoken) << LockedKeyShift);
      NEXTTOKEN(); // get skull/key same
      trigger += (E_BooleanArg(curtoken) << LockedNKeysShift);
      break;
   case GenLiftBase:
      NEXTTOKEN(); // get target
      trigger += (E_LiftTarget(curtoken) << LiftTargetShift);
      NEXTTOKEN(); // get delay
      trigger += (E_LiftDelay(curtoken) << LiftDelayShift);
      NEXTTOKEN(); // get monster
      trigger += (E_BooleanArg(curtoken) << LiftMonsterShift);
      NEXTTOKEN(); // get speed
      trigger += (E_SpeedArg(curtoken) << LiftSpeedShift);
      break;
   case GenStairsBase:
      NEXTTOKEN(); // get direction
      trigger += (E_DirArg(curtoken) << StairDirectionShift);
      NEXTTOKEN(); // get step size
      trigger += (E_StepSize(curtoken) << StairStepShift);
      NEXTTOKEN(); // get ignore texture
      trigger += (E_BooleanArg(curtoken) << StairIgnoreShift);
      NEXTTOKEN(); // get monster
      trigger += (E_BooleanArg(curtoken) << StairMonsterShift);
      NEXTTOKEN(); // get speed
      trigger += (E_SpeedArg(curtoken) << StairSpeedShift);
      break;
   case GenCrusherBase:
      NEXTTOKEN(); // get silent
      trigger += (E_BooleanArg(curtoken) << CrusherSilentShift);
      NEXTTOKEN(); // get monster
      trigger += (E_BooleanArg(curtoken) << CrusherMonsterShift);
      NEXTTOKEN(); // get speed
      trigger += (E_SpeedArg(curtoken) << CrusherSpeedShift);
      break;
   default:
      break;
   }

   // for all: get trigger type
   NEXTTOKEN();
   trigger += (E_GenTrigger(curtoken) << TriggerTypeShift);

   return trigger;
}

#undef NEXTTOKEN

//
// E_LineSpecCB
//
// libConfuse value-parsing callback function for the linedef special
// field. This function can resolve normal and generalized special
// names and accepts special numbers, too.
//
// Normal and parameterized specials are stored in the exlinespecs
// hash table and are resolved via name lookup.
//
// Generalized specials use a functional syntax which is parsed by
// E_ProcessGenSpec and E_GenTokenizer above. It is rather complicated
// and probably not that useful, but is provided for completeness.
//
// Raw special numbers are not checked for validity, but invalid
// names are turned into zero specials. Malformed generalized specials
// will cause errors, but bad argument values are zeroed.
//
static int E_LineSpecCB(cfg_t *cfg, cfg_opt_t *opt, const char *value,
                        void *result)
{
   int  num;
   char *endptr;

   num = strtol(value, &endptr, 0);

   // check if value is a number or not
   if(*endptr != '\0')
   {
      // value is a special name
      const char *bracket_loc = strchr(value, '(');

      // if it has a parenthesis, it's a generalized type
      if(bracket_loc)
         *(int *)result = E_ProcessGenSpec(value);
      else
         *(int *)result = (int)(E_LineSpecForName(value));
   }
   else
   {
      // value is a number
      if(errno == ERANGE)
      {
         if(cfg)
         {
            cfg_error(cfg,
                      "integer value for option '%s' is out of range\n",
                      opt->name);
         }
         return -1;
      }

      *(int *)result = num;
   }

   return 0;
}

//
// E_ParseLineArgs
//
// Parses the linedef args list.
//
static void E_ParseLineArgs(maplinedefext_t *mlde, cfg_t *sec)
{
   unsigned int i, numargs;

   // count number of args given in list
   numargs = cfg_size(sec, FIELD_LINE_ARGS);

   // init all args to 0
   for(i = 0; i < NUMLINEARGS; ++i)
      mlde->args[i] = 0;

   // parse the given args values
   for(i = 0; i < numargs && i < NUMLINEARGS; ++i)
   {
      const char *argstr = cfg_getnstr(sec, FIELD_LINE_ARGS, i);
      E_ParseArg(argstr, &(mlde->args[i]));
   }
}

//
// E_ProcessEDLines
//
// Allocates and processes ExtraData linedef records.
//
static void E_ProcessEDLines(cfg_t *cfg)
{
   unsigned int i;

   // get the number of linedef records
   numEDLines = cfg_size(cfg, SEC_LINEDEF);

   // if none, we're done
   if(!numEDLines)
      return;

   // allocate the maplinedefext_t structures
   EDLines = (maplinedefext_t *)(Z_Malloc(numEDLines * sizeof(maplinedefext_t),
                                          PU_LEVEL, NULL));

   // initialize the hash chains
   for(i = 0; i < NUMLDCHAINS; ++i)
      linedef_chains[i] = numEDLines;

   // read fields
   for(i = 0; i < numEDLines; ++i)
   {
      cfg_t *linesec;
      const char *tempstr;
      int tempint;
      bool tagset = false;

      linesec = cfg_getnsec(cfg, SEC_LINEDEF, i);

      // get the record number
      tempint = EDLines[i].recordnum = cfg_getint(linesec, FIELD_LINE_NUM);

      // guard against duplicate record numbers
      if(E_EDLineForRecordNum(tempint) != numEDLines)
         I_Error("E_ProcessEDLines: duplicate record number %d\n", tempint);

      // hash this ExtraData linedef record by its recordnum field
      tempint = EDLines[i].recordnum % NUMLDCHAINS;
      EDLines[i].next = linedef_chains[tempint];
      linedef_chains[tempint] = i;

      // standard fields

      // special
      EDLines[i].stdfields.special = (int16_t)cfg_getint(linesec, FIELD_LINE_SPECIAL);

      // tag
      EDLines[i].stdfields.tag = (int16_t)cfg_getint(linesec, FIELD_LINE_TAG);
      if(cfg_size(linesec, FIELD_LINE_TAG) > 0)
         tagset = true;

      // extflags
      tempstr = cfg_getstr(linesec, FIELD_LINE_EXTFLAGS);
      if(*tempstr == '\0')
         EDLines[i].extflags = 0;
      else
         EDLines[i].extflags = E_ParseFlags(tempstr, &ld_flagset);

      // args
      E_ParseLineArgs(&EDLines[i], linesec);

      // 03/03/07: line id
      if(!tagset)
         EDLines[i].id = cfg_getint(linesec, FIELD_LINE_ID);
      else
         EDLines[i].id = -1;

      // 11/11/10: alpha
      EDLines[i].alpha = (float)(cfg_getfloat(linesec, FIELD_LINE_ALPHA));
      if(EDLines[i].alpha < 0.0f)
         EDLines[i].alpha = 0.0f;
      else if(EDLines[i].alpha > 1.0f)
         EDLines[i].alpha = 1.0f;

      // TODO: any other new fields
   }
}

//=============================================================================
//
// Begin Sector Routines
//

//
// E_EDSectorForRecordNum
//
// Returns an index into EDSectors for the given record number.
// Returns numEDSectors if no such record exists.
//
static unsigned int E_EDSectorForRecordNum(int recnum)
{
   unsigned int num;
   int key = recnum % NUMSECCHAINS;

   num = sector_chains[key];
   while(num != numEDSectors && EDSectors[num].recordnum != recnum)
   {
      num = EDSectors[num].next;
   }

   return num;
}

//
// E_NormalizeFlatAngle
//
// ZDoom decided for us that floors and ceilings should rotate backward with
// respect to DOOM's normal angular coordinate system, so don't blame me for
// the reversal.
//
static double E_NormalizeFlatAngle(double input)
{
   // first account for negative angles
   while(input < 0.0)
      input += 360.0;

   // now account for super angles
   while(input >= 360.0)
      input -= 360.0;

   // now that we're between 0 and 359, reverse the angle...
   input = 360.0 - input;

   // don't return 360 for 0
   if(input == 360.0)
      input = 0.0;

   return input;
}

//
// E_ProcessEDSectors
//
// Allocates and processes ExtraData sector records.
//
static void E_ProcessEDSectors(cfg_t *cfg)
{
   unsigned int i;

   // get the number of sector records
   numEDSectors = cfg_size(cfg, SEC_SECTOR);

   // if none, we're done
   if(!numEDSectors)
      return;

   // allocate the mapsectorext_t structures
   EDSectors = estructalloctag(mapsectorext_t, numEDSectors, PU_LEVEL);

   // initialize the hash chains
   for(i = 0; i < NUMSECCHAINS; ++i)
      sector_chains[i] = numEDSectors;

   // read fields
   for(i = 0; i < numEDSectors; ++i)
   {
      cfg_t *section;
      int tempint;
      double tempdouble;
      const char *tempstr;
      mapsectorext_t *sec = &EDSectors[i];

      section = cfg_getnsec(cfg, SEC_SECTOR, i);

      // get the record number
      tempint = sec->recordnum = cfg_getint(section, FIELD_SECTOR_NUM);

      // guard against duplicate record numbers
      if(E_EDSectorForRecordNum(tempint) != numEDSectors)
         I_Error("E_ProcessEDSectors: duplicate record number %d\n", tempint);

      // hash this ExtraData sector record by its recordnum field
      tempint = sec->recordnum % NUMSECCHAINS;
      sec->next = sector_chains[tempint];
      sector_chains[tempint] = i;

      // extended fields

      // flags
      tempstr = cfg_getstr(section, FIELD_SECTOR_FLAGS); // flags to set
      if(*tempstr != '\0')
      {
         sec->hasflags = true; // flags were set
         sec->flags = E_ParseFlags(tempstr, &sector_flagset);
      }

      tempstr = cfg_getstr(section, FIELD_SECTOR_FLAGSADD); // flags to add
      if(*tempstr != '\0')
         sec->flagsadd = E_ParseFlags(tempstr, &sector_flagset);

      tempstr = cfg_getstr(section, FIELD_SECTOR_FLAGSREM); // flags to remove
      if(*tempstr != '\0')
         sec->flagsrem = E_ParseFlags(tempstr, &sector_flagset);

      // damage properties

      // damage
      sec->damage = cfg_getint(section, FIELD_SECTOR_DAMAGE);

      // damagemask
      sec->damagemask = cfg_getint(section, FIELD_SECTOR_DAMAGEMASK);

      // damagemod
      tempstr = cfg_getstr(section, FIELD_SECTOR_DAMAGEMOD);
      sec->damagemod = E_DamageTypeNumForName(tempstr);

      // damageflags
      tempstr = cfg_getstr(section, FIELD_SECTOR_DAMAGEFLAGS); // flags to set
      if(*tempstr != '\0')
      {
         sec->hasdamageflags = true; // flags were set
         sec->damageflags = E_ParseFlags(tempstr, &sectordamage_flagset);
      }

      tempstr = cfg_getstr(section, FIELD_SECTOR_DMGFLAGSADD); // flags to add
      if(*tempstr != '\0')
         sec->damageflagsadd = E_ParseFlags(tempstr, &sectordamage_flagset);

      tempstr = cfg_getstr(section, FIELD_SECTOR_DMGFLAGSREM); // flags to remove
      if(*tempstr != '\0')
         sec->damageflagsrem = E_ParseFlags(tempstr, &sectordamage_flagset);

      // floor and ceiling offsets
      sec->floor_xoffs   = cfg_getfloat(section, FIELD_SECTOR_FLOOROFFSETX);
      sec->floor_yoffs   = cfg_getfloat(section, FIELD_SECTOR_FLOOROFFSETY);
      sec->ceiling_xoffs = cfg_getfloat(section, FIELD_SECTOR_CEILINGOFFSETX);
      sec->ceiling_yoffs = cfg_getfloat(section, FIELD_SECTOR_CEILINGOFFSETY);

      // floor and ceiling angles
      tempdouble = cfg_getfloat(section, FIELD_SECTOR_FLOORANGLE);
      sec->floorangle = E_NormalizeFlatAngle(tempdouble);

      tempdouble = cfg_getfloat(section, FIELD_SECTOR_CEILINGANGLE);
      sec->ceilingangle = E_NormalizeFlatAngle(tempdouble);

      // sector colormaps
      sec->topmap = sec->midmap = sec->bottommap = -1; // mark as not specified

      tempstr = cfg_getstr(section, FIELD_SECTOR_TOPMAP);
      if(strcasecmp(tempstr, "@default"))
         sec->topmap = R_ColormapNumForName(tempstr);

      tempstr = cfg_getstr(section, FIELD_SECTOR_MIDMAP);
      if(strcasecmp(tempstr, "@default"))
         sec->midmap = R_ColormapNumForName(tempstr);

      tempstr = cfg_getstr(section, FIELD_SECTOR_BOTTOMMAP);
      if(strcasecmp(tempstr, "@default"))
         sec->bottommap = R_ColormapNumForName(tempstr);

      // terrain type overrides
      tempstr = cfg_getstr(section, FIELD_SECTOR_FLOORTERRAIN);
      if(strcasecmp(tempstr, "@flat"))
         sec->floorterrain = E_TerrainForName(tempstr);

      tempstr = cfg_getstr(section, FIELD_SECTOR_CEILINGTERRAIN);
      if(strcasecmp(tempstr, "@flat"))
         sec->ceilingterrain = E_TerrainForName(tempstr);

      tempstr = cfg_getstr(section, FIELD_SECTOR_PORTALFLAGS_F);
      if(*tempstr != '\0')
         sec->f_pflags = E_ParseFlags(tempstr, &sectorportal_flagset);

      tempstr = cfg_getstr(section, FIELD_SECTOR_PORTALFLAGS_C);
      if(*tempstr != '\0')
         sec->c_pflags = E_ParseFlags(tempstr, &sectorportal_flagset);

      tempint = cfg_getint(section, FIELD_SECTOR_OVERLAYALPHA_F);
      if(tempint < 0)
         tempint = 0;
      if(tempint > 255)
         tempint = 255;
      sec->f_alpha = (unsigned int)tempint;

      tempint = cfg_getint(section, FIELD_SECTOR_OVERLAYALPHA_C);
      if(tempint < 0)
         tempint = 0;
      if(tempint > 255)
         tempint = 255;
      sec->c_alpha = (unsigned int)tempint;
   }
}

//=============================================================================
//
// Global Routines
//

//
// E_LoadExtraData
//
// Loads the ExtraData lump for the level, if it has one
//
void E_LoadExtraData(void)
{
   cfg_t *cfg;
   int lumpnum;

   // reset ExtraData variables (allocations are at PU_LEVEL
   // cache level, so anything from any earlier level has been
   // freed)

   EDThings = NULL;
   numEDMapThings = 0;

   EDLines = NULL;
   numEDLines = 0;

   // check to see if the ExtraData lump is defined by MapInfo
   if(!LevelInfo.extraData)
      return;

   // initialize the cfg
   cfg = cfg_init(ed_opts, CFGF_NOCASE);
   cfg_set_error_function(cfg, E_ErrorCB);

   if((lumpnum = W_CheckNumForName(LevelInfo.extraData)) <  0)
      I_Error("E_LoadExtraData: Error finding lump %s\n", LevelInfo.extraData);

   // load and parse the ED lump
   if(cfg_parselump(cfg, LevelInfo.extraData, lumpnum))
      I_Error("E_LoadExtraData: Error parsing lump %s\n", LevelInfo.extraData);

   // processing

   // load mapthings
   E_ProcessEDThings(cfg);

   // load lines
   E_ProcessEDLines(cfg);

   // load sectors
   E_ProcessEDSectors(cfg);

   // free the cfg
   cfg_free(cfg);
}

//
// E_SpawnMapThingExt
//
// Called by P_SpawnMapThing when an ExtraData control point
// (doomednum 5004) is encountered.  This function recursively
// calls P_SpawnMapThing with the new mapthing data from the
// corresponding ExtraData record.
//
Mobj *E_SpawnMapThingExt(mapthing_t *mt)
{
   unsigned int edThingIdx;
   mapthing_t *edthing;

   // The record number is stored in the control thing's options field.
   // Check to see if the record exists, and that ExtraData is loaded.
   if(!LevelInfo.extraData || numEDMapThings == 0 ||
      (edThingIdx = E_EDThingForRecordNum((uint16_t)(mt->options))) == numEDMapThings)
   {
      // spawn an Unknown thing
      return P_SpawnMobj(mt->x << FRACBITS, mt->y << FRACBITS, ONFLOORZ,
                         UnknownThingType);
   }

   // get a pointer to the proper ExtraData mapthing record
   edthing = &EDThings[edThingIdx];

   // propagate the control object's x, y, and angle fields to the ED object
   edthing->x     = mt->x;
   edthing->y     = mt->y;
   edthing->angle = mt->angle;

   // spawn the thing normally;
   // return the spawned object back through P_SpawnMapThing

   return P_SpawnMapThing(edthing);
}

//
// E_LoadLineDefExt
//
// Called from P_LoadLines for lines with the ED_LINE_SPECIAL after they
// have been initialized normally. Normal fields will be altered and
// extended fields will be set in the linedef.
//
void E_LoadLineDefExt(line_t *line, bool applySpecial)
{
   unsigned int edLineIdx;
   maplinedefext_t *edline;

   // ExtraData record number is stored in line tag
   if(!LevelInfo.extraData || numEDLines == 0 ||
      (edLineIdx = E_EDLineForRecordNum((uint16_t)(line->tag))) == numEDLines)
   {
      // if no ExtraData or no such record, zero special and clear tag,
      // and we're finished here.
      line->special = 0;
      line->tag = -1;
      return;
   }

   // get a pointer to the proper ExtraData line record
   edline = &EDLines[edLineIdx];

   if(applySpecial)
   {
      // apply standard fields to the line
      line->special = edline->stdfields.special;
      line->tag     = edline->stdfields.tag;
   }

   // apply extended fields to the line

   // extended special flags
   line->extflags = edline->extflags;

   // args
   memcpy(line->args, edline->args, 5*sizeof(int));

   // 03/03/07: id
   if(edline->id != -1) // haleyjd: only use this field when it is specified
      line->tag = edline->id;

   // 11/11/10: alpha
   line->alpha = edline->alpha;
}

//
// E_LoadSectorExt
//
void E_LoadSectorExt(line_t *line)
{
   unsigned int    edSectorIdx;
   mapsectorext_t *edsector;
   sector_t       *sector;

   // ExtraData must be loaded
   if(!LevelInfo.extraData || numEDSectors == 0)
   {
      line->tag = 0;
      return;
   }

   sector = line->frontsector;

   // The ExtraData record number is the line's tag; the line's frontsector is the
   // sector to adjust.
   if((edSectorIdx = E_EDSectorForRecordNum(line->tag)) == numEDSectors)
   {
      line->tag = 0;
      return;
   }

   // get a pointer to the proper ExtraData sector record
   edsector = &EDSectors[edSectorIdx];

   // apply extended fields to the sector

   // sector flags
   if(edsector->hasflags) // if flags-to-set were specified, set them.
      sector->flags = edsector->flags;

   sector->flags |=   edsector->flagsadd;  // add any flags-to-add
   sector->flags &= ~(edsector->flagsrem); // remove any flags-to-remove

   // damage fields
   sector->damage      = edsector->damage;
   sector->damagemask  = edsector->damagemask;
   sector->damagemod   = edsector->damagemod;

   if(edsector->hasdamageflags) // if flags-to-set were specified, set them.
      sector->damageflags = edsector->damageflags;

   sector->damageflags |=   edsector->damageflagsadd;  // add any flags-to-add
   sector->damageflags &= ~(edsector->damageflagsrem); // remove any flags-to-remove


   // flat offsets
   sector->floor_xoffs   = M_DoubleToFixed(edsector->floor_xoffs);
   sector->floor_yoffs   = M_DoubleToFixed(edsector->floor_yoffs);
   sector->ceiling_xoffs = M_DoubleToFixed(edsector->ceiling_xoffs);
   sector->ceiling_yoffs = M_DoubleToFixed(edsector->ceiling_yoffs);

   // flat angles
   sector->floorbaseangle   = (float)(edsector->floorangle   * PI / 180.0f);
   sector->ceilingbaseangle = (float)(edsector->ceilingangle * PI / 180.0f);

   // colormaps
   if(edsector->topmap >= 0)
      sector->topmap    = edsector->topmap;
   if(edsector->midmap >= 0)
      sector->midmap    = edsector->midmap;
   if(edsector->bottommap >= 0)
      sector->bottommap = edsector->bottommap;

   // terrain overrides
   sector->floorterrain   = edsector->floorterrain;
   sector->ceilingterrain = edsector->ceilingterrain;

   // per-sector portal properties
   sector->f_pflags = (edsector->f_pflags | (edsector->f_alpha << PO_OPACITYSHIFT));
   sector->c_pflags = (edsector->c_pflags | (edsector->c_alpha << PO_OPACITYSHIFT));

   if(sector->f_portal)
      P_CheckFPortalState(sector);
   if(sector->c_portal)
      P_CheckCPortalState(sector);

   // TODO: more?

   // clear the line tag
   line->tag = 0;
}

//
// E_IsParamSpecial
//
// Tests if a given line special is parameterized.
//
bool E_IsParamSpecial(int16_t special)
{
   // no param line specs in old demos
   if(demo_version < 333)
      return false;

   switch(special)
   {
   case 300: // Door_Raise
   case 301: // Door_Open
   case 302: // Door_Close
   case 303: // Door_CloseWaitOpen
   case 304: // Door_WaitRaise
   case 305: // Door_WaitClose
   case 306: // Floor_RaiseToHighest
   case 307: // Floor_LowerToHighest
   case 308: // Floor_RaiseToLowest
   case 309: // Floor_LowerToLowest
   case 310: // Floor_RaiseToNearest
   case 311: // Floor_LowerToNearest
   case 312: // Floor_RaiseToLowestCeiling
   case 313: // Floor_LowerToLowestCeiling
   case 314: // Floor_RaiseToCeiling
   case 315: // Floor_RaiseByTexture
   case 316: // Floor_LowerByTexture
   case 317: // Floor_RaiseByValue
   case 318: // Floor_LowerByValue
   case 319: // Floor_MoveToValue
   case 320: // Floor_RaiseInstant
   case 321: // Floor_LowerInstant
   case 322: // Floor_ToCeilingInstant
   case 323: // Ceiling_RaiseToHighest
   case 324: // Ceiling_ToHighestInstant
   case 325: // Ceiling_RaiseToNearest
   case 326: // Ceiling_LowerToNearest
   case 327: // Ceiling_RaiseToLowest
   case 328: // Ceiling_LowerToLowest
   case 329: // Ceiling_RaiseToHighestFloor
   case 330: // Ceiling_LowerToHighestFloor
   case 331: // Ceiling_ToFloorInstant
   case 332: // Ceiling_LowerToFloor
   case 333: // Ceiling_RaiseByTexture
   case 334: // Ceiling_LowerByTexture
   case 335: // Ceiling_RaiseByValue
   case 336: // Ceiling_LowerByValue
   case 337: // Ceiling_MoveToValue
   case 338: // Ceiling_RaiseInstant
   case 339: // Ceiling_LowerInstant
   case 340: // Stairs_BuildUpDoom
   case 341: // Stairs_BuildDownDoom
   case 342: // Stairs_BuildUpDoomSync
   case 343: // Stairs_BuildDownDoomSync
   case 350: // Polyobj_DoorSlide
   case 351: // Polyobj_DoorSwing
   case 352: // Polyobj_Move
   case 353: // Polyobj_OR_Move
   case 354: // Polyobj_RotateRight
   case 355: // Polyobj_OR_RotateRight
   case 356: // Polyobj_RotateLeft
   case 357: // Polyobj_OR_RotateLeft
   case 362: // Pillar_Build
   case 363: // Pillar_BuildAndCrush
   case 364: // Pillar_Open
   case 365: // ACS_Execute
   case 366: // ACS_Suspend
   case 367: // ACS_Terminate
   case 368: // Light_RaiseByValue
   case 369: // Light_LowerByValue
   case 370: // Light_ChangeToValue
   case 371: // Light_Fade
   case 372: // Light_Glow
   case 373: // Light_Flicker
   case 374: // Light_Strobe
   case 375: // Radius_Quake
   case 378: // Line_SetIdentification
   case 397: // Floor_Waggle
   case 398: // Thing_Spawn
   case 399: // Thing_SpawnNoFog
   case 400: // Teleport_EndGame
   case 402: // Thing_Projectile
   case 403: // Thing_ProjectileGravity
   case 404: // Thing_Activate
   case 405: // Thing_Deactivate
      return true;
   default:
      return false;
   }
}

void E_GetEDMapThings(mapthing_t **things, int *numthings)
{
   *things = EDThings;
   *numthings = numEDMapThings;
}

void E_GetEDLines(maplinedefext_t **lines, int *numlines)
{
   *lines = EDLines;
   *numlines = numEDLines;
}

// EOF
