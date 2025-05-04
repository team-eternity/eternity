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
// Purpose: ExtraData.
//
//  The be-all, end-all extension to the DOOM map format. Uses the
//  libConfuse library like EDF.
//
//  ExtraData can extend mapthings, lines, and sectors with an
//  arbitrary number of fields, with data provided in more or less
//  any format. The use of a textual input language will forever
//  remove any future problems caused by binary format limitations.
//
// Authors: James Haley, Stephen McGranahan, Ioan Chera, Max Waine
//

#include "z_zone.h"
#include "i_system.h"

#define NEED_EDF_DEFINITIONS

#include "Confuse/confuse.h"
#include "e_exdata.h"
#include "e_lib.h"
#include "e_mod.h"
#include "e_things.h"
#include "e_ttypes.h"
#include "e_udmf.h"

#include "d_dehtbl.h" // for dehflags parsing
#include "d_io.h"
#include "doomdef.h"
#include "doomstat.h"
#include "ev_specials.h"
#include "m_qstr.h"
#include "m_utils.h"
#include "p_info.h"
#include "p_mobj.h"
#include "p_portal.h"
#include "p_spec.h"
#include "r_data.h"
#include "r_main.h"
#include "r_portal.h"
#include "r_state.h"
#include "v_misc.h"
#include "w_wad.h"

// statics

static mapthing_t  *EDThings;
static unsigned int numEDMapThings;

constexpr int       NUMMTCHAINS = 1021;
static unsigned int mapthing_chains[NUMMTCHAINS];

static maplinedefext_t *EDLines;
static unsigned int     numEDLines;

constexpr int       NUMLDCHAINS = 1021;
static unsigned int linedef_chains[NUMLDCHAINS];

static mapsectorext_t *EDSectors;
static unsigned int    numEDSectors;

constexpr int       NUMSECCHAINS = 1021;
static unsigned int sector_chains[NUMSECCHAINS];

// ExtraData section names
constexpr const char SEC_MAPTHING[] = "mapthing";
constexpr const char SEC_LINEDEF[]  = "linedef";
constexpr const char SEC_SECTOR[]   = "sector";

// ExtraData field names
// mapthing fields:
constexpr const char FIELD_NUM[]     = "recordnum";
constexpr const char FIELD_TID[]     = "tid";
constexpr const char FIELD_TYPE[]    = "type";
constexpr const char FIELD_OPTIONS[] = "options";
constexpr const char FIELD_ARGS[]    = "args";
constexpr const char FIELD_HEIGHT[]  = "height";
constexpr const char FIELD_SPECIAL[] = "special";

// linedef fields:
constexpr const char FIELD_LINE_NUM[]      = "recordnum";
constexpr const char FIELD_LINE_PORTALID[] = "portalid";
constexpr const char FIELD_LINE_SPECIAL[]  = "special";
constexpr const char FIELD_LINE_TAG[]      = "tag";
constexpr const char FIELD_LINE_EXTFLAGS[] = "extflags";
constexpr const char FIELD_LINE_ARGS[]     = "args";
constexpr const char FIELD_LINE_ID[]       = "id";
constexpr const char FIELD_LINE_ALPHA[]    = "alpha";

// sector fields:
constexpr const char FIELD_SECTOR_NUM[]            = "recordnum";
constexpr const char FIELD_SECTOR_FLAGS[]          = "flags";
constexpr const char FIELD_SECTOR_FLAGSADD[]       = "flags.add";
constexpr const char FIELD_SECTOR_FLAGSREM[]       = "flags.remove";
constexpr const char FIELD_SECTOR_DAMAGE[]         = "damage";
constexpr const char FIELD_SECTOR_DAMAGEMASK[]     = "damagemask";
constexpr const char FIELD_SECTOR_DAMAGEMOD[]      = "damagemod";
constexpr const char FIELD_SECTOR_DAMAGEFLAGS[]    = "damageflags";
constexpr const char FIELD_SECTOR_DMGFLAGSADD[]    = "damageflags.add";
constexpr const char FIELD_SECTOR_DMGFLAGSREM[]    = "damageflags.remove";
constexpr const char FIELD_SECTOR_FLOORTERRAIN[]   = "floorterrain";
constexpr const char FIELD_SECTOR_FLOORANGLE[]     = "floorangle";
constexpr const char FIELD_SECTOR_FLOOROFFSETX[]   = "flooroffsetx";
constexpr const char FIELD_SECTOR_FLOOROFFSETY[]   = "flooroffsety";
constexpr const char FIELD_SECTOR_FLOORSCALEX[]    = "floorscalex";
constexpr const char FIELD_SECTOR_FLOORSCALEY[]    = "floorscaley";
constexpr const char FIELD_SECTOR_CEILINGTERRAIN[] = "ceilingterrain";
constexpr const char FIELD_SECTOR_CEILINGANGLE[]   = "ceilingangle";
constexpr const char FIELD_SECTOR_CEILINGOFFSETX[] = "ceilingoffsetx";
constexpr const char FIELD_SECTOR_CEILINGOFFSETY[] = "ceilingoffsety";
constexpr const char FIELD_SECTOR_CEILINGSCALEX[]  = "ceilingscalex";
constexpr const char FIELD_SECTOR_CEILINGSCALEY[]  = "ceilingscaley";
constexpr const char FIELD_SECTOR_TOPMAP[]         = "colormaptop";
constexpr const char FIELD_SECTOR_MIDMAP[]         = "colormapmid";
constexpr const char FIELD_SECTOR_BOTTOMMAP[]      = "colormapbottom";
constexpr const char FIELD_SECTOR_PORTALFLAGS_F[]  = "portalflags.floor";
constexpr const char FIELD_SECTOR_PORTALFLAGS_C[]  = "portalflags.ceiling";
constexpr const char FIELD_SECTOR_OVERLAYALPHA_F[] = "overlayalpha.floor";
constexpr const char FIELD_SECTOR_OVERLAYALPHA_C[] = "overlayalpha.ceiling";
constexpr const char FIELD_SECTOR_PORTALID_F[]     = "portalid.floor";
constexpr const char FIELD_SECTOR_PORTALID_C[]     = "portalid.ceiling";

// mapthing options and related data structures

// line special callback
static int E_LineSpecCB(cfg_t *cfg, cfg_opt_t *opt, const char *value, void *result);

// clang-format off

static cfg_opt_t mapthing_opts[] =
{
   CFG_INT(FIELD_NUM,     0,       CFGF_NONE),
   CFG_INT(FIELD_TID,     0,       CFGF_NONE),
   CFG_STR(FIELD_TYPE,    "",      CFGF_NONE),
   CFG_STR(FIELD_OPTIONS, "",      CFGF_NONE),
   CFG_STR(FIELD_ARGS,    nullptr, CFGF_LIST),
   CFG_INT(FIELD_HEIGHT,  0,       CFGF_NONE),
   CFG_INT_CB(FIELD_SPECIAL, 0,    CFGF_NONE, E_LineSpecCB),
   CFG_END()
};

// clang-format on

// mapthing flag values and mnemonics

static dehflags_t mapthingflags[] = {
    { "EASY",      MTF_EASY      },
    { "NORMAL",    MTF_NORMAL    },
    { "HARD",      MTF_HARD      },
    { "AMBUSH",    MTF_AMBUSH    },
    { "NOTSINGLE", MTF_NOTSINGLE },
    { "NOTDM",     MTF_NOTDM     },
    { "NOTCOOP",   MTF_NOTCOOP   },
    { "FRIEND",    MTF_FRIEND    },
    { "DORMANT",   MTF_DORMANT   },
    { nullptr,     0             }
};

static dehflagset_t mt_flagset = {
    mapthingflags, // flaglist
    0,             // mode
};

//
// Apparently it's safe to mix known with unknown flags, so we're going to use both basic and
// extended flags from the same flag string
//
static dehflags_t mapthingflags_ex[] = {
    { "STANDING", MTF_EX_STAND },
    { nullptr,    0            }
};
static dehflagset_t mt_flagset_ex = {
    mapthingflags_ex,
    0,
};

// linedef options and related data structures

// clang-format off

static cfg_opt_t linedef_opts[] =
{
   CFG_INT(FIELD_LINE_NUM,         0, CFGF_NONE),
   CFG_INT_CB(FIELD_LINE_SPECIAL,  0, CFGF_NONE, E_LineSpecCB),
   CFG_INT(FIELD_LINE_TAG,         0, CFGF_NONE),
   CFG_STR(FIELD_LINE_EXTFLAGS,   "", CFGF_NONE),
   CFG_STR(FIELD_LINE_ARGS,   nullptr, CFGF_LIST),
   CFG_INT(FIELD_LINE_ID,         -1, CFGF_NONE),
   CFG_FLOAT(FIELD_LINE_ALPHA,   1.0, CFGF_NONE), 
   CFG_INT(FIELD_LINE_PORTALID,    0, CFGF_NONE),
   CFG_END()
};

// clang-format on

// linedef extended flag values and mnemonics

static dehflags_t extlineflags[] = {
    { "CROSS",        EX_ML_CROSS        },
    { "USE",          EX_ML_USE          },
    { "IMPACT",       EX_ML_IMPACT       },
    { "PUSH",         EX_ML_PUSH         },
    { "PLAYER",       EX_ML_PLAYER       },
    { "MONSTER",      EX_ML_MONSTER      },
    { "MISSILE",      EX_ML_MISSILE      },
    { "REPEAT",       EX_ML_REPEAT       },
    { "1SONLY",       EX_ML_1SONLY       },
    { "ADDITIVE",     EX_ML_ADDITIVE     },
    { "BLOCKALL",     EX_ML_BLOCKALL     },
    { "ZONEBOUNDARY", EX_ML_ZONEBOUNDARY },
    { "CLIPMIDTEX",   EX_ML_CLIPMIDTEX   },
    { "LOWERPORTAL",  EX_ML_LOWERPORTAL  },
    { "UPPERPORTAL",  EX_ML_UPPERPORTAL  },
    { "POLYOBJECT",   EX_ML_POLYOBJECT   },
    { nullptr,        0                  }
};

static dehflagset_t ld_flagset = {
    extlineflags, // flaglist
    0,            // mode
};

// haleyjd 06/26/07: sector options and related data structures

// clang-format off

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
   CFG_FLOAT(FIELD_SECTOR_FLOORSCALEX,    1.0,        CFGF_NONE),
   CFG_FLOAT(FIELD_SECTOR_FLOORSCALEY,    1.0,        CFGF_NONE),
   CFG_FLOAT(FIELD_SECTOR_CEILINGSCALEX,  1.0,        CFGF_NONE),
   CFG_FLOAT(FIELD_SECTOR_CEILINGSCALEY,  1.0,        CFGF_NONE),
   CFG_FLOAT(FIELD_SECTOR_FLOORANGLE,     0.0,        CFGF_NONE),
   CFG_FLOAT(FIELD_SECTOR_CEILINGANGLE,   0.0,        CFGF_NONE),

   // haleyjd 01/08/11: portal properties
   CFG_STR(FIELD_SECTOR_PORTALFLAGS_F,     "",        CFGF_NONE),
   CFG_STR(FIELD_SECTOR_PORTALFLAGS_C,     "",        CFGF_NONE),
   CFG_INT_CB(FIELD_SECTOR_OVERLAYALPHA_F, 255,       CFGF_NONE, E_TranslucCB2),
   CFG_INT_CB(FIELD_SECTOR_OVERLAYALPHA_C, 255,       CFGF_NONE, E_TranslucCB2),
   CFG_INT(FIELD_SECTOR_PORTALID_F,        0,         CFGF_NONE),
   CFG_INT(FIELD_SECTOR_PORTALID_C,        0,         CFGF_NONE),
   
   CFG_END()
};

// clang-format on

static dehflags_t sectorflags[] = {
    { "SECRET",        SECF_SECRET        },
    { "FRICTION",      SECF_FRICTION      },
    { "PUSH",          SECF_PUSH          },
    { "KILLSOUND",     SECF_KILLSOUND     },
    { "KILLMOVESOUND", SECF_KILLMOVESOUND },
    { "PHASEDLIGHT",   SECF_PHASEDLIGHT   },
    { "LIGHTSEQUENCE", SECF_LIGHTSEQUENCE },
    { "LIGHTSEQALT",   SECF_LIGHTSEQALT   },
    { nullptr,         0                  }
};

static dehflagset_t sector_flagset = {
    sectorflags, // flaglist
    0,           // mode
};

static dehflags_t sectordamageflags[] = {
    { "LEAKYSUIT",  SDMG_LEAKYSUIT  },
    { "IGNORESUIT", SDMG_IGNORESUIT },
    { "ENDGODMODE", SDMG_ENDGODMODE },
    { "EXITLEVEL",  SDMG_EXITLEVEL  },
    { "TERRAINHIT", SDMG_TERRAINHIT },
    { nullptr,      0               }
};

static dehflagset_t sectordamage_flagset = {
    sectordamageflags, // flaglist
    0                  // mode
};

static dehflags_t sectorportalflags[] = {
    { "DISABLED",       PF_DISABLED       },
    { "NORENDER",       PF_NORENDER       },
    { "NOPASS",         PF_NOPASS         },
    { "BLOCKSOUND",     PF_BLOCKSOUND     },
    { "OVERLAY",        PS_OVERLAY        },
    { "ADDITIVE",       PS_ADDITIVE       },
    { "USEGLOBALTEX",   PS_USEGLOBALTEX   },
    { "ATTACHEDPORTAL", PF_ATTACHEDPORTAL },
    { nullptr,          0                 }
};

static dehflagset_t sectorportal_flagset = {
    sectorportalflags, // flaglist
    0                  // mode
};

// primary ExtraData options table

// clang-format off

static cfg_opt_t ed_opts[] =
{
   CFG_SEC(SEC_MAPTHING, mapthing_opts, CFGF_MULTI|CFGF_NOCASE),
   CFG_SEC(SEC_LINEDEF,  linedef_opts,  CFGF_MULTI|CFGF_NOCASE),
   CFG_SEC(SEC_SECTOR,   sector_opts,   CFGF_MULTI|CFGF_NOCASE),
   CFG_END()
};

// clang-format on

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
    *dest = static_cast<int>(strtol(str, nullptr, 0));
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
    int          key = M_PositiveModulo(recnum, NUMMTCHAINS);

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
    long        num;
    int         i;
    char        prefix[16];
    const char *colonloc;
    char       *numpos = nullptr;

    num = strtol(value, &numpos, 0);

    memset(prefix, 0, 16);
    colonloc = E_ExtractPrefix(value, prefix, 16);

    // If has a colon, or is otherwise not just a number...
    if(colonloc || (numpos && *numpos != '\0'))
    {
        const char *strval;
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
    unsigned int numargs;

    // count number of args given in list
    numargs = cfg_size(sec, FIELD_ARGS);

    // init all args to 0
    for(int &arg : mte->args)
        arg = 0;

    // parse the given args values
    for(unsigned int i = 0; i < numargs && i < NUMMTARGS; ++i)
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
    // get the number of mapthing records
    numEDMapThings = cfg_size(cfg, SEC_MAPTHING);

    // if none, we're done
    if(!numEDMapThings)
        return;

    // allocate the mapthing_t structures
    EDThings = estructalloctag(mapthing_t, numEDMapThings, PU_LEVEL);

    // initialize the hash chains
    for(unsigned int &mapthing_chain : mapthing_chains)
        mapthing_chain = numEDMapThings;

    // read fields
    for(unsigned int i = 0; i < numEDMapThings; i++)
    {
        cfg_t      *thingsec;
        const char *tempstr;
        int         tempint;

        thingsec = cfg_getnsec(cfg, SEC_MAPTHING, i);

        // get the record number
        tempint = EDThings[i].recordnum = cfg_getint(thingsec, FIELD_NUM);

        // guard against duplicate record numbers
        if(E_EDThingForRecordNum(tempint) != numEDMapThings)
            I_Error("E_ProcessEDThings: duplicate record number %d\n", tempint);

        // hash this ExtraData mapthing record by its recordnum field
        tempint                  = M_PositiveModulo(EDThings[i].recordnum, NUMMTCHAINS);
        EDThings[i].next         = mapthing_chains[tempint];
        mapthing_chains[tempint] = i;

        // standard fields

        // type
        tempstr          = cfg_getstr(thingsec, FIELD_TYPE);
        EDThings[i].type = (int16_t)(E_ParseTypeField(tempstr));

        // it is not allowed to spawn an ExtraData control object via
        // ExtraData, but the error is tolerated by changing it to an
        // "Unknown" thing
        if(EDThings[i].type == ED_CTRL_DOOMEDNUM)
            EDThings[i].type = mobjinfo[UnknownThingType]->doomednum;

        // options
        tempstr = cfg_getstr(thingsec, FIELD_OPTIONS);
        if(*tempstr == '\0')
            EDThings[i].options = EDThings[i].extOptions = 0;
        else
        {
            EDThings[i].options    = (int16_t)(E_ParseFlags(tempstr, &mt_flagset));
            EDThings[i].extOptions = (uint32_t)(E_ParseFlags(tempstr, &mt_flagset_ex));
        }

        // extended fields

        // get TID field
        EDThings[i].tid = (int16_t)cfg_getint(thingsec, FIELD_TID);
        if(EDThings[i].tid < 0)
            EDThings[i].tid = 0; // you cannot specify a reserved TID

        // get args
        E_ParseThingArgs(&EDThings[i], thingsec);

        // get height
        // ioanch 20151218: fixed point coordinate
        EDThings[i].height = (int16_t)cfg_getint(thingsec, FIELD_HEIGHT) << FRACBITS;

        EDThings[i].special = int16_t(cfg_getint(thingsec, FIELD_SPECIAL));

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
    int          key = M_PositiveModulo(recnum, NUMLDCHAINS);

    num = linedef_chains[key];
    while(num != numEDLines && EDLines[num].recordnum != recnum)
    {
        num = EDLines[num].next;
    }

    return num;
}

//
// E_LineSpecForName
//
// Gets a line special for a name.
//
int E_LineSpecForName(const char *name)
{
    int special = 0;

    // check static inits first
    if(!(special = EV_SpecialForStaticInitName(name)))
    {
        // check normal line types
        ev_binding_t *binding = EV_BindingForName(name);
        if(binding)
            special = binding->actionNumber;
    }

    return special;
}

//
// E_GenTypeForName
//
// Returns a base generalized line type for a name.
//
static int E_GenTypeForName(const char *name)
{
    int                i       = 0;
    static const char *names[] = { "GenFloor", "GenCeiling", "GenDoor",    "GenLockedDoor",
                                   "GenLift",  "GenStairs",  "GenCrusher", nullptr };
    static int         bases[] = { GenFloorBase, GenCeilingBase, GenDoorBase,    GenLockedBase,
                                   GenLiftBase,  GenStairsBase,  GenCrusherBase, 0 };

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
    int  state = 0;

    // if we're already at the end, return nullptr
    if(text[*index] == '\0')
        return nullptr;

    token->clear();

    while((c = text[*index]) != '\0')
    {
        *index += 1;
        switch(state)
        {
        case 0: // default state
            switch(c)
            {
            case ')': // ignore closing bracket
            case ' ':
            case '\t': // ignore whitespace
                continue;
            case '"':
                state = 1; // enter quoted part
                continue;
            case '\'':
                state = 2; // enter quoted part (single quote support)
                continue;
            case '(': // end of current token
            case ',': //
                return token->constPtr();
            default: // everything else == part of value
                *token += c;
                continue;
            }
        case 1:          // in quoted area (double quotes)
            if(c == '"') // end of quoted area
                state = 0;
            else
                *token += c; // everything inside is literal
            continue;
        case 2:           // in quoted area (single quotes)
            if(c == '\'') // end of quoted area
                state = 0;
            else
                *token += c; // everything inside is literal
            continue;
        default: //
            I_Error("E_GenTokenizer: internal error - undefined lexer state\n");
        }
    }

    // return final token, next call will return nullptr
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
static int E_SpeedArg(const char *str)
{
    int                i        = 0;
    static const char *speeds[] = { "slow", "normal", "fast", "turbo", nullptr };

    while(speeds[i] && strcasecmp(str, speeds[i]))
        ++i;

    return (speeds[i] ? i : 0);
}

//
// E_ChangeArg
//
// Parses a floor/ceiling changer type argument.
//
static int E_ChangeArg(const char *str)
{
    int                i         = 0;
    static const char *changes[] = { "none", "zero", "texture", "texturetype", nullptr };

    while(changes[i] && strcasecmp(str, changes[i]))
        ++i;

    return (changes[i] ? i : 0);
}

//
// E_ModelArg
//
// Parses a floor/ceiling changer model argument.
//
static int E_ModelArg(const char *str)
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
static int E_FloorTarget(const char *str)
{
    int                i       = 0;
    static const char *targs[] = { "HnF", "LnF", "NnF", "LnC", "C", "sT", "24", "32", nullptr };

    while(targs[i] && strcasecmp(str, targs[i]))
        ++i;

    return (targs[i] ? i : 0);
}

//
// E_CeilingTarget
//
// Parses the target argument for a generalized ceiling type.
//
static int E_CeilingTarget(const char *str)
{
    int                i       = 0;
    static const char *targs[] = { "HnC", "LnC", "NnC", "HnF", "F", "sT", "24", "32", nullptr };

    while(targs[i] && strcasecmp(str, targs[i]))
        ++i;

    return (targs[i] ? i : 0);
}

//
// E_DirArg
//
// Parses a direction argument.
//
static int E_DirArg(const char *str)
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
static int E_DoorTypeArg(const char *str)
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
static int E_DoorDelay(const char *str)
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
static int E_LockedType(const char *str)
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
static int E_LockedKey(const char *str)
{
    int                i      = 0;
    static const char *keys[] = { "Any",       "RedCard",     "BlueCard", "YellowCard", "RedSkull",
                                  "BlueSkull", "YellowSkull", "All",      nullptr };

    while(keys[i] && strcasecmp(str, keys[i]))
        ++i;

    return (keys[i] ? i : 0);
}

//
// E_LiftTarget
//
// Parses the target argument for a generalized lift.
//
static int E_LiftTarget(const char *str)
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
static int E_LiftDelay(const char *str)
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
static int E_StepSize(const char *str)
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
static int E_GenTrigger(const char *str)
{
    int                i       = 0;
    static const char *trigs[] = { "W1", "WR", "S1", "SR", "G1", "GR", "D1", "DR", nullptr };

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
static int E_ProcessGenSpec(const char *value)
{
    qstring     buffer;
    const char *curtoken = nullptr;
    int         t, forc = 0, tok_index = 0;
    int         trigger;

    // get special name (starts at beginning, ends at '[')
    // and convert to base trigger type
    NEXTTOKEN();
    trigger = E_GenTypeForName(curtoken);

    switch(trigger)
    {
    case GenFloorBase: //
        forc = 1;
        [[fallthrough]];
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
    default: //
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
static int E_LineSpecCB(cfg_t *cfg, cfg_opt_t *opt, const char *value, void *result)
{
    int   num;
    char *endptr;

    num = static_cast<int>(strtol(value, &endptr, 0));

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
                cfg_error(cfg, "integer value for option '%s' is out of range\n", opt->name);
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
    // count number of args given in list
    const unsigned int numargs = cfg_size(sec, FIELD_LINE_ARGS);

    // init all args to 0
    for(int &arg : mlde->args)
        arg = 0;

    // parse the given args values
    for(unsigned int i = 0; i < numargs && i < NUMLINEARGS; ++i)
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
    // get the number of linedef records
    numEDLines = cfg_size(cfg, SEC_LINEDEF);

    // if none, we're done
    if(!numEDLines)
        return;

    // allocate the maplinedefext_t structures
    EDLines = estructalloctag(maplinedefext_t, numEDLines, PU_LEVEL);

    // initialize the hash chains
    for(unsigned int &linedef_chain : linedef_chains)
        linedef_chain = numEDLines;

    // read fields
    for(unsigned int i = 0; i < numEDLines; ++i)
    {
        cfg_t      *linesec;
        const char *tempstr;
        int         tempint;
        bool        tagset = false;

        linesec = cfg_getnsec(cfg, SEC_LINEDEF, i);

        // get the record number
        tempint = EDLines[i].recordnum = cfg_getint(linesec, FIELD_LINE_NUM);

        // guard against duplicate record numbers
        if(E_EDLineForRecordNum(tempint) != numEDLines)
            I_Error("E_ProcessEDLines: duplicate record number %d\n", tempint);

        // hash this ExtraData linedef record by its recordnum field
        tempint                 = M_PositiveModulo(EDLines[i].recordnum, NUMLDCHAINS);
        EDLines[i].next         = linedef_chains[tempint];
        linedef_chains[tempint] = i;

        // standard fields

        // special
        EDLines[i].stdfields.special = (int16_t)cfg_getint(linesec, FIELD_LINE_SPECIAL);

        // tag
        EDLines[i].stdfields.tag = (int16_t)cfg_getint(linesec, FIELD_LINE_TAG);
        if(cfg_size(linesec, FIELD_LINE_TAG) > 0)
            tagset = true;
        // ioanch TODO: set args[0] depending on the specials

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

        EDLines[i].portalid = cfg_getint(linesec, FIELD_LINE_PORTALID);
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
    int          key = M_PositiveModulo(recnum, NUMSECCHAINS);

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
// MaxW: 2016/07/11: This is no longer static as the function is needed for UDMF
//
double E_NormalizeFlatAngle(double input)
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
    // get the number of sector records
    numEDSectors = cfg_size(cfg, SEC_SECTOR);

    // if none, we're done
    if(!numEDSectors)
        return;

    // allocate the mapsectorext_t structures
    EDSectors = estructalloctag(mapsectorext_t, numEDSectors, PU_LEVEL);

    // initialize the hash chains
    for(unsigned int &sector_chain : sector_chains)
        sector_chain = numEDSectors;

    // read fields
    for(unsigned int i = 0; i < numEDSectors; ++i)
    {
        cfg_t          *section;
        int             tempint;
        double          tempdouble;
        const char     *tempstr;
        mapsectorext_t *sec = &EDSectors[i];

        section = cfg_getnsec(cfg, SEC_SECTOR, i);

        // get the record number
        tempint = sec->recordnum = cfg_getint(section, FIELD_SECTOR_NUM);

        // guard against duplicate record numbers
        if(E_EDSectorForRecordNum(tempint) != numEDSectors)
            I_Error("E_ProcessEDSectors: duplicate record number %d\n", tempint);

        // hash this ExtraData sector record by its recordnum field
        tempint                = M_PositiveModulo(sec->recordnum, NUMSECCHAINS);
        sec->next              = sector_chains[tempint];
        sector_chains[tempint] = i;

        // extended fields

        // flags
        tempstr = cfg_getstr(section, FIELD_SECTOR_FLAGS); // flags to set
        if(*tempstr != '\0')
        {
            sec->hasflags = true; // flags were set
            sec->flags    = E_ParseFlags(tempstr, &sector_flagset);
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
        tempstr        = cfg_getstr(section, FIELD_SECTOR_DAMAGEMOD);
        sec->damagemod = E_DamageTypeNumForName(tempstr);

        // damageflags
        tempstr = cfg_getstr(section, FIELD_SECTOR_DAMAGEFLAGS); // flags to set
        if(*tempstr != '\0')
        {
            sec->hasdamageflags = true; // flags were set
            sec->damageflags    = E_ParseFlags(tempstr, &sectordamage_flagset);
        }

        tempstr = cfg_getstr(section, FIELD_SECTOR_DMGFLAGSADD); // flags to add
        if(*tempstr != '\0')
            sec->damageflagsadd = E_ParseFlags(tempstr, &sectordamage_flagset);

        tempstr = cfg_getstr(section, FIELD_SECTOR_DMGFLAGSREM); // flags to remove
        if(*tempstr != '\0')
            sec->damageflagsrem = E_ParseFlags(tempstr, &sectordamage_flagset);

        struct fieldset_t
        {
            const char *offsetx;
            const char *offsety;
            const char *scalex;
            const char *scaley;
            const char *angle;
            const char *terrain;
            const char *pflags;
            const char *alpha;
            const char *portalid;
        };

        // clang-format off

        static const Surfaces<fieldset_t> fieldsets =
        {
            {
                FIELD_SECTOR_FLOOROFFSETX,
                FIELD_SECTOR_FLOOROFFSETY,
                FIELD_SECTOR_FLOORSCALEX,
                FIELD_SECTOR_FLOORSCALEY,
                FIELD_SECTOR_FLOORANGLE,
                FIELD_SECTOR_FLOORTERRAIN,
                FIELD_SECTOR_PORTALFLAGS_F,
                FIELD_SECTOR_OVERLAYALPHA_F,
                FIELD_SECTOR_PORTALID_F
            },
            {
                FIELD_SECTOR_CEILINGOFFSETX,
                FIELD_SECTOR_CEILINGOFFSETY,
                FIELD_SECTOR_CEILINGSCALEX,
                FIELD_SECTOR_CEILINGSCALEY,
                FIELD_SECTOR_CEILINGANGLE,
                FIELD_SECTOR_CEILINGTERRAIN,
                FIELD_SECTOR_PORTALFLAGS_C,
                FIELD_SECTOR_OVERLAYALPHA_C,
                FIELD_SECTOR_PORTALID_C
            },
        };

        // clang-format on

        for(int surf = surf_floor; surf != surf_NUM; surf++)
        {
            // floor and ceiling offsets
            sec->surface[surf].offset = { cfg_getfloat(section, fieldsets[surf].offsetx),
                                          cfg_getfloat(section, fieldsets[surf].offsety) };

            // floor and ceiling scale
            sec->surface[surf].scale = { cfg_getfloat(section, fieldsets[surf].scalex),
                                         cfg_getfloat(section, fieldsets[surf].scaley) };

            // floor and ceiling angles
            tempdouble               = cfg_getfloat(section, fieldsets[surf].angle);
            sec->surface[surf].angle = E_NormalizeFlatAngle(tempdouble);

            // terrain type overrides
            tempstr = cfg_getstr(section, fieldsets[surf].terrain);
            if(strcasecmp(tempstr, "@flat"))
                sec->surface[surf].terrain = E_TerrainForName(tempstr);

            tempstr = cfg_getstr(section, fieldsets[surf].pflags);
            if(*tempstr != '\0')
                sec->surface[surf].pflags = E_ParseFlags(tempstr, &sectorportal_flagset);

            tempint = cfg_getint(section, fieldsets[surf].alpha);
            if(tempint < 0)
                tempint = 0;
            if(tempint > 255)
                tempint = 255;
            sec->surface[surf].alpha = (unsigned int)tempint;

            sec->surface[surf].portalid = cfg_getint(section, fieldsets[surf].portalid);
        }

        // sector colormaps
        sec->topmap = sec->midmap = sec->bottommap = -1; // mark as not specified

        auto checkBadCMap = [sec](int cmap) {
            if(cmap < 0)
            {
                doom_printf(FC_ERROR "Invalid colormap for sector %d in ExtraData '%s'", sec->recordnum,
                            LevelInfo.extraData);
                // Do not correct it. It already started as -1, so keep it -1 in case of error. But warn
                // user.
            }
        };

        tempstr = cfg_getstr(section, FIELD_SECTOR_TOPMAP);
        if(strcasecmp(tempstr, "@default"))
        {
            sec->topmap = R_ColormapNumForName(tempstr);
            checkBadCMap(sec->topmap);
        }

        tempstr = cfg_getstr(section, FIELD_SECTOR_MIDMAP);
        if(strcasecmp(tempstr, "@default"))
        {
            sec->midmap = R_ColormapNumForName(tempstr);
            checkBadCMap(sec->midmap);
        }

        tempstr = cfg_getstr(section, FIELD_SECTOR_BOTTOMMAP);
        if(strcasecmp(tempstr, "@default"))
        {
            sec->bottommap = R_ColormapNumForName(tempstr);
            checkBadCMap(sec->bottommap);
        }
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
    int    lumpnum;

    // reset ExtraData variables (allocations are at PU_LEVEL
    // cache level, so anything from any earlier level has been
    // freed)

    EDThings       = nullptr;
    numEDMapThings = 0;

    EDLines    = nullptr;
    numEDLines = 0;

    // check to see if the ExtraData lump is defined by MapInfo
    if(!LevelInfo.extraData)
        return;

    // initialize the cfg
    cfg = cfg_init(ed_opts, CFGF_NOCASE);
    cfg_set_error_function(cfg, E_ErrorCB);

    if((lumpnum = W_CheckNumForName(LevelInfo.extraData)) < 0)
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
    mapthing_t  *edthing;

    // The record number is stored in the control thing's options field.
    // Check to see if the record exists, and that ExtraData is loaded.
    if(!LevelInfo.extraData || numEDMapThings == 0 ||
       (edThingIdx = E_EDThingForRecordNum((uint16_t)(mt->options))) == numEDMapThings)
    {
        // spawn an Unknown thing
        // ioanch 20151218: fixed point coordinates
        return P_SpawnMobj(mt->x, mt->y, ONFLOORZ, UnknownThingType);
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
void E_LoadLineDefExt(line_t *line, bool applySpecial, UDMFSetupSettings &setupSettings)
{
    unsigned int     edLineIdx;
    maplinedefext_t *edline;

    // ExtraData record number is stored in line tag
    if(!LevelInfo.extraData || numEDLines == 0 || (edLineIdx = E_EDLineForRecordNum(line->args[0])) == numEDLines)
    {
        // if no ExtraData or no such record, zero special and clear tag,
        // and we're finished here.
        line->special = 0;
        line->tag     = -1;
        return;
    }

    // get a pointer to the proper ExtraData line record
    edline = &EDLines[edLineIdx];

    if(applySpecial)
    {
        // apply standard fields to the line
        line->special = edline->stdfields.special;
        // also update side "special"
        if(line->sidenum[0] != -1 && line->special)
            sides[*line->sidenum].special = line->special;
        line->args[0] = edline->stdfields.tag;
    }

    // apply extended fields to the line

    // extended special flags
    line->extflags = edline->extflags;

    // args
    memcpy(line->args, edline->args, 5 * sizeof(int));

    // 03/03/07: id

    // if(edline->id != -1) // haleyjd: only use this field when it is specified
    line->tag = edline->id; // ioanch 20160304: actually apply it always

    // 11/11/10: alpha
    line->alpha = edline->alpha;

    setupSettings.setLinePortal(eindex(line - lines), edline->portalid);
}

//
// E_LoadSectorExt
//
void E_LoadSectorExt(line_t *line, UDMFSetupSettings &setupSettings)
{
    unsigned int    edSectorIdx;
    mapsectorext_t *edsector;
    sector_t       *sector;

    // ExtraData must be loaded
    if(!LevelInfo.extraData || numEDSectors == 0)
    {
        line->args[0] = line->tag = 0;
        return;
    }

    sector = line->frontsector;

    // The ExtraData record number is the line's tag; the line's frontsector is the
    // sector to adjust.
    if((edSectorIdx = E_EDSectorForRecordNum(line->args[0])) == numEDSectors)
    {
        line->args[0] = line->tag = 0;
        return;
    }

    // get a pointer to the proper ExtraData sector record
    edsector = &EDSectors[edSectorIdx];

    // apply extended fields to the sector

    // sector flags
    if(edsector->hasflags) // if flags-to-set were specified, set them.
        sector->flags = edsector->flags;

    sector->flags |= edsector->flagsadd;    // add any flags-to-add
    sector->flags &= ~(edsector->flagsrem); // remove any flags-to-remove

    // damage fields
    sector->damage     = edsector->damage;
    sector->damagemask = edsector->damagemask;
    sector->damagemod  = edsector->damagemod;

    if(edsector->hasdamageflags) // if flags-to-set were specified, set them.
        sector->damageflags = edsector->damageflags;

    sector->damageflags |= edsector->damageflagsadd;    // add any flags-to-add
    sector->damageflags &= ~(edsector->damageflagsrem); // remove any flags-to-remove

    // ioanch: set leakiness from these flags
    if(sector->damageflags & SDMG_LEAKYSUIT)
        sector->leakiness = 5;
    if(sector->damageflags & SDMG_IGNORESUIT)
        sector->leakiness = 256;
    // NOTE: no real need to delete these flags even if we use leakiness. Instead, we really should
    // keep the flags because now the MBF21 instant death special uses them.

    // flat offsets
    sector->srf.floor.offset   = v2fixed_t::doubleToFixed(edsector->surface.floor.offset);
    sector->srf.ceiling.offset = v2fixed_t::doubleToFixed(edsector->surface.ceiling.offset);

    // floor and ceiling scale
    sector->srf.floor.scale   = static_cast<v2float_t>(edsector->surface.floor.scale);
    sector->srf.ceiling.scale = static_cast<v2float_t>(edsector->surface.ceiling.scale);

    // flat angles
    sector->srf.floor.baseangle   = (float)(edsector->surface.floor.angle * PI / 180.0f);
    sector->srf.ceiling.baseangle = (float)(edsector->surface.ceiling.angle * PI / 180.0f);

    // colormaps
    if(edsector->topmap >= 0)
    {
        sector->topmap = edsector->topmap;
        setupSettings.setSectorFlag(eindex(sector - sectors), UDMF_SECTOR_INIT_COLOR_TOP);
    }
    if(edsector->midmap >= 0)
    {
        sector->midmap = edsector->midmap;
        setupSettings.setSectorFlag(eindex(sector - sectors), UDMF_SECTOR_INIT_COLOR_MIDDLE);
    }
    if(edsector->bottommap >= 0)
    {
        sector->bottommap = edsector->bottommap;
        setupSettings.setSectorFlag(eindex(sector - sectors), UDMF_SECTOR_INIT_COLOR_BOTTOM);
    }

    // terrain overrides
    sector->srf.floor.terrain   = edsector->surface.floor.terrain;
    sector->srf.ceiling.terrain = edsector->surface.ceiling.terrain;

    // per-sector portal properties
    sector->srf.floor.pflags = (edsector->surface.floor.pflags | (edsector->surface.floor.alpha << PO_OPACITYSHIFT));
    sector->srf.ceiling.pflags =
        (edsector->surface.ceiling.pflags | (edsector->surface.ceiling.alpha << PO_OPACITYSHIFT));

    if(sector->srf.floor.portal)
        P_CheckSectorPortalState(*sector, surf_floor);
    if(sector->srf.ceiling.portal)
        P_CheckSectorPortalState(*sector, surf_ceil);

    setupSettings.setSectorPortals(eindex(sector - sectors), edsector->surface.ceiling.portalid,
                                   edsector->surface.floor.portalid);
    // TODO: more?

    // clear the line tag
    line->tag = line->args[0] = 0;
}

void E_GetEDMapThings(mapthing_t **things, int *numthings)
{
    *things    = EDThings;
    *numthings = numEDMapThings;
}

void E_GetEDLines(maplinedefext_t **lines, int *numlines)
{
    *lines    = EDLines;
    *numlines = numEDLines;
}

// EOF

