// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//----------------------------------------------------------------------------
//
// EDF Core Module
//
// EDF is the answer to moving most of the remaining static data
// out of the executable and into user-editable data files. Uses
// the customized version of libConfuse to allow for ease of editing
// and parsing.
//
// Current layout of EDF modules and their contents follows. Note
// that these names are just the defaults; user EDF replacements
// can use whatever names and layouts they so wish.
//
// * root.edf ..... This file includes the others, and is opened by
//                  E_ProcessEDF by default
// * sprites.edf .. Includes the sprnames array and sprite-based
//                  pickup item definitions
// * sounds.edf ... Contains sfxinfo structures and sound sequences
//                  See e_sound.c for implementation.
// * frames.edf ... Contains the states structures
//                  See e_states.c for implementation.
// * things.edf ... Contains the mobjinfo structures
//                  See e_things.c for implementation.
// * cast.edf ..... Contains DOOM II cast call definitions
// * terrain.edf .. Contains terrain-related structures.
//                  See e_ttypes.c for implementation.
// * misc.edf ..... Miscellaneous stuff
// * player.edf ... EDF skins, weapons, and player classes
//                  See e_player.c for implementation.
//
// EDF can also be loaded from WAD lumps, starting from the newest
// lump named "EDFROOT". EDF lumps currently take total precedence
// over any EDF file specified via GFS or command-line.
//
// Other separately processed sources of EDF data include:
//
// * ESTRINGS ..... Lump chain that can define more string objects.
//                  See e_string.c for implementation.
// * ETERRAIN ..... Lump chain that can define terrain-related objects.
//                  See e_ttypes.c for implementation.
// * EMENUS ....... Lump chain that can define menu objects.
//                  See mn_emenu.c for implementation.
// * ESNDINFO ..... Lump chain that can contain any type of sound definition.
// * ESNDSEQ ...... Same as above.
//                  See e_sound.c for implementation.
//
// By James Haley
//
//----------------------------------------------------------------------------

#include <errno.h>

#define NEED_EDF_DEFINITIONS

#include "z_zone.h"
#include "d_io.h"
#include "d_dwfile.h"

#include "Confuse/confuse.h"
#include "Confuse/lexer.h"

#include "w_wad.h"
#include "i_system.h"
#include "d_main.h"
#include "d_dehtbl.h"
#include "d_gi.h"
#include "doomdef.h"
#include "doomstat.h"
#include "info.h"
#include "m_argv.h"
#include "m_utils.h"
#include "mn_engin.h"
#include "p_enemy.h"
#include "p_pspr.h"
#include "f_finale.h"
#include "m_qstr.h"

#include "e_lib.h"
#include "e_edf.h"

#include "e_anim.h"
#include "e_args.h"
#include "e_fonts.h"
#include "e_gameprops.h"
#include "e_inventory.h"
#include "e_mod.h"
#include "e_player.h"
#include "e_puff.h"
#include "e_reverbs.h"
#include "e_sound.h"
#include "e_sprite.h"
#include "e_states.h"
#include "e_string.h"
#include "e_switch.h"
#include "e_things.h"
#include "e_ttypes.h"
#include "e_weapons.h"
#include "mn_emenu.h"

// EDF Keywords used by features implemented in this module

// Sprite variables
#define ITEM_PLAYERSPRITE "playersprite"
#define ITEM_BLANKSPRITE  "blanksprite"

// Cast call
#define SEC_CAST             "castinfo"
#define ITEM_CAST_TYPE       "type"
#define ITEM_CAST_NAME       "name"
#define ITEM_CAST_SA         "stopattack"
#define ITEM_CAST_SOUND      "sound"
#define ITEM_CAST_SOUNDFRAME "frame"
#define ITEM_CAST_SOUNDNAME  "sfx"

// Cast order array
#define SEC_CASTORDER "castorder"

// Boss types
#define SEC_BOSSTYPES "boss_spawner_types"
#define SEC_BOSSPROBS "boss_spawner_probs"  // schepe

// Miscellaneous variables
#define ITEM_D2TITLETICS "doom2_title_tics"
#define ITEM_INTERPAUSE  "intermission_pause"
#define ITEM_INTERFADE   "intermission_fade"
#define ITEM_INTERTL     "intermission_tl"
#define ITEM_MN_EPISODE  "mn_episode"

// sprite variables (global)

int blankSpriteNum;

// function prototypes for libConfuse callbacks (aka EDF functions)

static int bex_include(cfg_t *cfg, cfg_opt_t *opt, int argc, 
                       const char **argv);

static int bex_override(cfg_t *cfg, cfg_opt_t *opt, int argc, 
                        const char **argv);

static int edf_ifenabled(cfg_t *cfg, cfg_opt_t *opt, int argc,
                         const char **argv);

static int edf_ifenabledany(cfg_t *cfg, cfg_opt_t *opt, int argc,
                            const char **argv);

static int edf_ifdisabled(cfg_t *cfg, cfg_opt_t *opt, int argc,
                          const char **argv);

static int edf_ifdisabledany(cfg_t *cfg, cfg_opt_t *opt, int argc,
                             const char **argv);

static int edf_enable(cfg_t *cfg, cfg_opt_t *opt, int argc,
                      const char **argv);

static int edf_disable(cfg_t *cfg, cfg_opt_t *opt, int argc,
                       const char **argv);

static int edf_includeifenabled(cfg_t *cfg, cfg_opt_t *opt, int argc,
                                const char **argv);

static int edf_ifgametype(cfg_t *cfg, cfg_opt_t *opt, int argc,
                          const char **argv);

static int edf_ifngametype(cfg_t *cfg, cfg_opt_t *opt, int argc,
                           const char **argv);

//=============================================================================
//
// EDF libConfuse option structures
//

// cast call
static cfg_opt_t cast_sound_opts[] =
{
   CFG_STR(ITEM_CAST_SOUNDFRAME, "S_NULL", CFGF_NONE),
   CFG_STR(ITEM_CAST_SOUNDNAME,  "none",   CFGF_NONE),
   CFG_END()
};

static cfg_opt_t cast_opts[] =
{
   CFG_STR(ITEM_CAST_TYPE,  NULL,            CFGF_NONE),
   CFG_STR(ITEM_CAST_NAME,  "unknown",       CFGF_NONE),
   CFG_BOOL(ITEM_CAST_SA,   false,           CFGF_NONE),
   CFG_SEC(ITEM_CAST_SOUND, cast_sound_opts, CFGF_MULTI|CFGF_NOCASE),
   CFG_END()
};

//=============================================================================
//
// EDF Root Options
//

#define EDF_TSEC_FLAGS (CFGF_MULTI | CFGF_TITLE | CFGF_NOCASE)
#define EDF_NSEC_FLAGS (CFGF_MULTI | CFGF_NOCASE)

static cfg_opt_t edf_opts[] =
{
   CFG_STR(SEC_SPRITE,          0,                 CFGF_LIST),
   CFG_STR(ITEM_PLAYERSPRITE,   "PLAY",            CFGF_NONE),
   CFG_STR(ITEM_BLANKSPRITE,    "TNT1",            CFGF_NONE),
   CFG_SEC(EDF_SEC_SPRPKUP,     edf_sprpkup_opts,  EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_PICKUPFX,    edf_pkupfx_opts,   EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_SOUND,       edf_sound_opts,    EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_AMBIENCE,    edf_ambience_opts, EDF_NSEC_FLAGS),
   CFG_SEC(EDF_SEC_SNDSEQ,      edf_sndseq_opts,   EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_ENVIROMGR,   edf_seqmgr_opts,   CFGF_NOCASE),
   CFG_SEC(EDF_SEC_REVERB,      edf_reverb_opts,   EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_MOD,         edf_dmgtype_opts,  EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_FRAME,       edf_frame_opts,    EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_FRAMEBLOCK,  edf_fblock_opts,   EDF_NSEC_FLAGS),
   CFG_SEC(EDF_SEC_THING,       edf_thing_opts,    EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_THINGGROUP,  edf_tgroup_opts,   EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_SKIN,        edf_skin_opts,     EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_HEALTHFX,    edf_healthfx_opts, EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_ARMORFX,     edf_armorfx_opts,  EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_AMMOFX,      edf_ammofx_opts,   EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_POWERFX,     edf_powerfx_opts,  EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_WEAPGFX,     edf_weapgfx_opts,  EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_ARTIFACT,    edf_artifact_opts, EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_LOCKDEF,     edf_lockdef_opts,  EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_PCLASS,      edf_pclass_opts,   EDF_TSEC_FLAGS),
   CFG_SEC(SEC_CAST,            cast_opts,         EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_SPLASH,      edf_splash_opts,   EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_TERRAIN,     edf_terrn_opts,    EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_TERDELTA,    edf_terdelta_opts, EDF_NSEC_FLAGS),
   CFG_SEC(EDF_SEC_FLOOR,       edf_floor_opts,    EDF_NSEC_FLAGS),
   CFG_SEC(EDF_SEC_MENU,        edf_menu_opts,     EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_FONT,        edf_font_opts,     EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_STRING,      edf_string_opts,   EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_GAMEPROPS,   edf_game_opts,     EDF_NSEC_FLAGS),
   CFG_SEC(EDF_SEC_SWITCH,      edf_switch_opts,   EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_ANIMATION,   edf_anim_opts,     EDF_NSEC_FLAGS),
   CFG_SEC(EDF_SEC_WEAPONINFO,  edf_wpninfo_opts,  EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_PUFFTYPE,    edf_puff_opts,     EDF_TSEC_FLAGS),
   CFG_SEC(EDF_SEC_PUFFDELTA,   edf_puff_delta_opts, EDF_NSEC_FLAGS),
   CFG_STR(SEC_CASTORDER,       0,                 CFGF_LIST),
   CFG_STR(SEC_BOSSTYPES,       0,                 CFGF_LIST),
   CFG_INT(SEC_BOSSPROBS,       0,                 CFGF_LIST), // schepe
   CFG_SEC(EDF_SEC_FRMDELTA,    edf_fdelta_opts,   EDF_NSEC_FLAGS),
   CFG_SEC(EDF_SEC_TNGDELTA,    edf_tdelta_opts,   EDF_NSEC_FLAGS),
   CFG_SEC(EDF_SEC_SDELTA,      edf_sdelta_opts,   EDF_NSEC_FLAGS),
   CFG_SEC(EDF_SEC_WPNDELTA,    edf_wdelta_opts,   EDF_NSEC_FLAGS),
   CFG_SEC(EDF_SEC_PDELTA,      edf_pdelta_opts,   EDF_NSEC_FLAGS),
   CFG_SEC(EDF_SEC_FNTDELTA,    edf_fntdelta_opts, EDF_NSEC_FLAGS),
   CFG_INT(ITEM_D2TITLETICS,    0,                 CFGF_NONE),
   CFG_INT(ITEM_INTERPAUSE,     0,                 CFGF_NONE),
   CFG_INT(ITEM_INTERFADE,     -1,                 CFGF_NONE),
   CFG_INT_CB(ITEM_INTERTL,     0,                 CFGF_NONE, E_TranslucCB),
   CFG_STR(ITEM_MN_EPISODE,     NULL,              CFGF_NONE),
   CFG_STR(ITEM_FONT_HUD,       "ee_smallfont",    CFGF_NONE),
   CFG_STR(ITEM_FONT_HUDO,      "ee_hudfont",      CFGF_NONE),
   CFG_STR(ITEM_FONT_MENU,      "ee_menufont",     CFGF_NONE),
   CFG_STR(ITEM_FONT_BMENU,     "ee_bigfont",      CFGF_NONE),
   CFG_STR(ITEM_FONT_NMENU,     "ee_smallfont",    CFGF_NONE),
   CFG_STR(ITEM_FONT_FINAL,     "ee_finalefont",   CFGF_NONE),
   CFG_STR(ITEM_FONT_FTITLE,    "",                CFGF_NONE),
   CFG_STR(ITEM_FONT_INTR,      "ee_smallfont",    CFGF_NONE),
   CFG_STR(ITEM_FONT_INTRB,     "ee_bigfont",      CFGF_NONE),
   CFG_STR(ITEM_FONT_INTRBN,    "ee_bignumfont",   CFGF_NONE),
   CFG_STR(ITEM_FONT_CONS,      "ee_consolefont",  CFGF_NONE),
   CFG_FUNC("setdialect",       E_SetDialect),
   CFG_FUNC("include",          E_Include),
   CFG_FUNC("lumpinclude",      E_LumpInclude),
   CFG_FUNC("include_prev",     E_IncludePrev),    // DEPRECATED
   CFG_FUNC("stdinclude",       E_StdInclude),
   CFG_FUNC("userinclude",      E_UserInclude),
   CFG_FUNC("bexinclude",       bex_include),      // DEPRECATED
   CFG_FUNC("bexoverride",      bex_override),
   CFG_FUNC("ifenabled",        edf_ifenabled),
   CFG_FUNC("ifenabledany",     edf_ifenabledany),
   CFG_FUNC("ifdisabled",       edf_ifdisabled),
   CFG_FUNC("ifdisabledany",    edf_ifdisabledany),
   CFG_FUNC("endif",            E_Endif),
   CFG_FUNC("enable",           edf_enable),
   CFG_FUNC("disable",          edf_disable),
   CFG_FUNC("includeifenabled", edf_includeifenabled),
   CFG_FUNC("ifgametype",       edf_ifgametype),
   CFG_FUNC("ifngametype",      edf_ifngametype),
   CFG_END()
};

//=============================================================================
//
// Error Reporting and Logging
//
// haleyjd 08/09/05: Functionalized EDF verbose logging.
//

// verbose logging, toggled with -edfout cmdline param
static FILE *edf_output = NULL;

//
// E_EDFOpenVerboseLog
//
// Opens the verbose log file, if one isn't already open.
// haleyjd 08/10/05: now works like screenshots, so it won't
// overwrite old log files.
//
static void E_EDFOpenVerboseLog()
{
   if(edf_output)
      return;

   if(!access(".", W_OK))
   {
      static int lognum;
      char fn[16];
      int tries = 100;

      do
         psnprintf(fn, sizeof(fn), "edfout%.2d.txt", lognum++);
      while(!access(fn, F_OK) && --tries);

      if(tries)
         edf_output = fopen(fn, "w");
      else
      {
         if(in_textmode)
            puts("Warning: Couldn't open EDF verbose log!\n");
      }
   }
}

//
// E_EDFCloseVerboseLog
//
// Closes the verbose log, if one is open.
//
static void E_EDFCloseVerboseLog()
{
   if(edf_output)
   {
      E_EDFLogPuts("Closing log file\n");      
      fclose(edf_output);
   }

   edf_output = NULL;
}

//
// E_EDFLogPuts
//
// Calls fputs on the verbose log with the provided message.
//
void E_EDFLogPuts(const char *msg)
{
   if(edf_output)
      fputs(msg, edf_output);
}

//
// E_EDFLogPrintf
//
// Calls vfprintf on the verbose log for formatted messages.
//
void E_EDFLogPrintf(const char *msg, ...)
{
   if(edf_output)
   {
      va_list v;
      
      va_start(v, msg);
      vfprintf(edf_output, msg, v);
      va_end(v);
   }
}

//
// E_EDFLoggedErr
//
// Primary EDF processing error function. Includes the ability to
// output "lv" number of tabs before the error message in the verbose
// log file to maintain proper formatting.
//
void E_EDFLoggedErr(int lv, const char *msg, ...)
{
   qstring msg_no_tabs;
   va_list va;

   if(edf_output)
   {
      va_list va2;

      while(lv--)
         putc('\t', edf_output);
      
      va_start(va2, msg);
      vfprintf(edf_output, msg, va2);
      va_end(va2);
   }

   msg_no_tabs = msg;
   msg_no_tabs.replace("\t", ' ');

   va_start(va, msg);
   I_ErrorVA(msg_no_tabs.constPtr(), va);
   va_end(va);
}

static int  edf_warning_count;
static bool edf_warning_out;

//
// E_EDFLoggedWarning
//
// Similar to above, but this is just a warning message. The number of warnings
// that occur will be printed to the system console after EDF processing is 
// finished, so that users are aware that warnings have occured even if verbose
// logging is not enabled.
//
void E_EDFLoggedWarning(int lv, const char *msg, ...)
{
   ++edf_warning_count;

   if(edf_output)
   {
      va_list va;
      while(lv--)
         putc('\t', edf_output);

      va_start(va, msg);
      vfprintf(edf_output, msg, va);
      va_end(va);
   }
   
   // allow display of warning messages on the system console too
   if(edf_warning_out)
   {
      qstring msg_no_tabs;
      va_list va;

      msg_no_tabs = msg;
      msg_no_tabs.replace("\t", ' ');

      va_start(va, msg);
      vfprintf(stderr, msg_no_tabs.constPtr(), va);
      va_end(va);
   }
}

//
// E_EDFPrintWarningCount
//
// Displays the EDF warning count after EDF processing.
//
static void E_EDFPrintWarningCount()
{
   if(in_textmode && edf_warning_count)
   {
      printf(" %d warnings occured during EDF processing.\n", edf_warning_count);

      if(!edf_warning_out)
         printf(" To see warnings, run Eternity with the -edf-show-warnings parameter.\n");
   }
}

//
// E_EDFResetWarnings
//
// Resets the count of warnings to zero.
//
static void E_EDFResetWarnings()
{
   edf_warning_count = 0;

   edf_warning_out = !!M_CheckParm("-edf-show-warnings");
}

//=============================================================================
//
// EDF-Specific Callback Functions
//

//
// edf_error
//
// This function is given to all cfg_t structures as the error
// callback.
//
static void edf_error(cfg_t *cfg, const char *fmt, va_list ap)
{
   E_EDFLogPuts("Exiting due to parser error\n");

   // 12/16/03: improved error messages
   if(cfg && cfg->filename)
   {
      if(cfg->line)
         fprintf(stderr, "Error at %s:%d:\n", cfg->filename, cfg->line);
      else
         fprintf(stderr, "Error in %s:\n", cfg->filename);
   }

   I_ErrorVA(fmt, ap);
}

//
// bex_include
//
// 12/12/03: New include function that allows EDF to queue
// DeHackEd/BEX files for later processing.  This helps to
// integrate BEX features such as string editing into the
// EDF/BEX superlanguage.
//
// This function interprets paths relative to the current 
// file.
//
static int bex_include(cfg_t *cfg, cfg_opt_t *opt, int argc,
                       const char **argv)
{
   char *currentpath;
   char *filename = NULL;

   // haleyjd 03/18/10: deprecation warning
   E_EDFLoggedWarning(0, "Warning: bexinclude is deprecated. "
                         "Please use a GFS or DEHACKED lump instead.\n");

   if(argc != 1)
   {
      cfg_error(cfg, "wrong number of args to bexinclude()\n");
      return 1;
   }
   if(!cfg->filename)
   {
      cfg_error(cfg, "bexinclude: cfg_t filename is undefined\n");
      return 1;
   }
   if(cfg_lexer_source_type(cfg) >= 0)
   {
      cfg_error(cfg, "bexinclude: cannot call from a wad lump\n");
      return 1;
   }

   currentpath = (char *)(Z_Alloca(strlen(cfg->filename) + 1));
   M_GetFilePath(cfg->filename, currentpath, strlen(cfg->filename) + 1);

   filename = M_SafeFilePath(currentpath, argv[0]);

   // queue the file for later processing
   D_QueueDEH(filename, 0);

   return 0;
}

// 
// bex_override
//
// haleyjd 09/26/10: Setting this flag in an EDF will disable loading of
// DEHACKED lumps from the same wad file.
//
static int bex_override(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
   return 0;
}

//=============================================================================
//
// "Enables" code
//

enum
{
   ENABLE_DOOM,
   ENABLE_HERETIC,
   NUMENABLES,
};

//
// The enables values table -- this can be fiddled with by the
// game engine using E_EDFSetEnableValue below.
//
static E_Enable_t edf_enables[] =
{
   // all game modes are enabled by default
   { "DOOM",     1 },
   { "HERETIC",  1 },
   
   // terminator
   { NULL }
};

// 
// E_EDFSetEnableValue
//
// This function lets the rest of the engine be able to set EDF enable values 
// before parsing begins. This is used to turn DOOM and HERETIC modes on and 
// off when loading the default root.edf. This saves time and memory. Note 
// that they are enabled when user EDFs are loaded, but users can use the 
// disable function to turn them off explicitly in that case when the 
// definitions are not needed.
//
void E_EDFSetEnableValue(const char *name, int value)
{
   int idx = E_EnableNumForName(name, edf_enables);

   if(idx != -1)
      edf_enables[idx].enabled = value;
}

static void E_EchoEnables()
{
   E_Enable_t *enable = edf_enables;

   E_EDFLogPuts("\t* Final enable values:\n");

   while(enable->name)
   {
      E_EDFLogPrintf("\t\t%s is %s\n", 
                     enable->name, 
                     enable->enabled ? "enabled" : "disabled");
      ++enable;
   }
}

//
// edf_ifenabled
//
// haleyjd 01/14/04: Causes the parser to skip forward, looking 
// for the next endif function and then calling it, if the 
// parameter isn't defined. I hacked the support for this
// into libConfuse without too much ugliness.
//
// haleyjd 09/06/05: Altered to work on N parameters, to make
// nil the issue of not being able to nest enable tests ^_^
//
static int edf_ifenabled(cfg_t *cfg, cfg_opt_t *opt, int argc,
                         const char **argv)
{
   int i, idx;
   bool enabled = true;

   if(argc < 1)
   {
      cfg_error(cfg, "wrong number of args to ifenabled()\n");
      return 1;
   }

   for(i = 0; i < argc; ++i)
   {
      if((idx = E_EnableNumForName(argv[i], edf_enables)) == -1)
      {
         cfg_error(cfg, "invalid enable value '%s'\n", argv[i]);
         return 1;
      }

      // use AND logic: the block will only be evaluated if ALL
      // options are enabled
      // use short circuit for efficiency
      if(!(enabled = enabled && edf_enables[idx].enabled))
         break;
   }

   // some option was disabled: skip the block
   if(!enabled)
   {
      // force libConfuse to look for an endif function
      cfg->flags |= CFGF_LOOKFORFUNC;
      cfg->lookfor = "endif";
   }

   return 0;
}

//
// edf_ifenabledany
//
// haleyjd 09/06/05: Exactly as above, but uses OR logic.
//
static int edf_ifenabledany(cfg_t *cfg, cfg_opt_t *opt, int argc,
                            const char **argv)
{
   int i, idx;
   bool enabled = false;

   if(argc < 1)
   {
      cfg_error(cfg, "wrong number of args to ifenabledany()\n");
      return 1;
   }

   for(i = 0; i < argc; ++i)
   {
      if((idx = E_EnableNumForName(argv[i], edf_enables)) == -1)
      {
         cfg_error(cfg, "invalid enable value '%s'\n", argv[i]);
         return 1;
      }

      // use OR logic: the block will be evaluated if ANY
      // option is enabled
      // use short circuit for efficiency
      if((enabled = enabled || edf_enables[idx].enabled))
         break;
   }

   // no options were enabled: skip the block
   if(!enabled)
   {
      // force libConfuse to look for an endif function
      cfg->flags |= CFGF_LOOKFORFUNC;
      cfg->lookfor = "endif";
   }

   return 0;
}

//
// edf_ifdisabled
//
// haleyjd 09/06/05: Exactly the same as ifenabled, but parses the
// section if all provided enable values are disabled. Why did I
// not provide this from the beginning? o_O
//
static int edf_ifdisabled(cfg_t *cfg, cfg_opt_t *opt, int argc,
                         const char **argv)
{
   int i, idx;
   bool disabled = true;

   if(argc < 1)
   {
      cfg_error(cfg, "wrong number of args to ifdisabled()\n");
      return 1;
   }

   for(i = 0; i < argc; ++i)
   {
      if((idx = E_EnableNumForName(argv[i], edf_enables)) == -1)
      {
         cfg_error(cfg, "invalid enable value '%s'\n", argv[i]);
         return 1;
      }

      // use AND logic: the block will be evalued if ALL 
      // options are disabled.
      if(!(disabled = disabled && !edf_enables[idx].enabled))
         break;
   }

   if(!disabled)
   {
      // force libConfuse to look for an endif function
      cfg->flags |= CFGF_LOOKFORFUNC;
      cfg->lookfor = "endif";
   }

   return 0;
}

//
// edf_ifdisabledany
//
// haleyjd 09/06/05: Exactly as above, but uses OR logic.
//
static int edf_ifdisabledany(cfg_t *cfg, cfg_opt_t *opt, int argc,
                             const char **argv)
{
   int i, idx;
   bool disabled = false;

   if(argc < 1)
   {
      cfg_error(cfg, "wrong number of args to ifdisabledany()\n");
      return 1;
   }

   for(i = 0; i < argc; ++i)
   {
      if((idx = E_EnableNumForName(argv[i], edf_enables)) == -1)
      {
         cfg_error(cfg, "invalid enable value '%s'\n", argv[i]);
         return 1;
      }

      // use OR logic: the block will be evalued if ANY
      // option is disabled.
      if((disabled = disabled || !edf_enables[idx].enabled))
         break;
   }

   if(!disabled)
   {
      // force libConfuse to look for an endif function
      cfg->flags |= CFGF_LOOKFORFUNC;
      cfg->lookfor = "endif";
   }

   return 0;
}

//
// edf_enable
//
// Enables a builtin option from within EDF.
//
static int edf_enable(cfg_t *cfg, cfg_opt_t *opt, int argc,
                      const char **argv)
{
   int idx;

   if(argc != 1)
   {
      cfg_error(cfg, "wrong number of args to enable()\n");
      return 1;
   }

   if((idx = E_EnableNumForName(argv[0], edf_enables)) == -1)
   {
      cfg_error(cfg, "unknown enable value '%s'\n", argv[0]);
      return 1;
   }

   edf_enables[idx].enabled = 1;
   return 0;
}

//
// edf_disable
//
// Disables a builtin option from within EDF.
//
static int edf_disable(cfg_t *cfg, cfg_opt_t *opt, int argc,
                       const char **argv)
{
   int idx;

   if(argc != 1)
   {
      cfg_error(cfg, "wrong number of args to disable()\n");
      return 1;
   }

   if((idx = E_EnableNumForName(argv[0], edf_enables)) == -1)
   {
      cfg_error(cfg, "unknown enable value '%s'\n", argv[0]);
      return 1;
   }

   // 05/12/08: don't allow disabling the active gamemode
   if(!((GameModeInfo->type == Game_DOOM    && idx == ENABLE_DOOM) ||
        (GameModeInfo->type == Game_Heretic && idx == ENABLE_HERETIC)))
   {
      edf_enables[idx].enabled = 0;
   }

   return 0;
}

//
// edf_includeifenabled
//
// 05/12/08: Includes a file in argv[0] if any of argv[1] - argv[N]
// options are enabled.
//
static int edf_includeifenabled(cfg_t *cfg, cfg_opt_t *opt, int argc,
                                const char **argv)
{
   int i, idx;
   bool enabled = false;

   if(argc < 2)
   {
      cfg_error(cfg, "wrong number of args to includeifenabled()\n");
      return 1;
   }

   for(i = 1; i < argc; ++i)
   {
      if((idx = E_EnableNumForName(argv[i], edf_enables)) == -1)
      {
         cfg_error(cfg, "invalid enable value '%s'\n", argv[i]);
         return 1;
      }

      // use OR logic: the block will be evaluated if ANY
      // option is enabled
      // use short circuit for efficiency
      if((enabled = enabled || edf_enables[idx].enabled))
         break;
   }

   // if any options were enabled, include the file
   if(enabled)
      return E_Include(cfg, opt, 1, argv);
   else
      return 0;
}


//=============================================================================
//
// Game Type Functions
//
// haleyjd 09/06/05:
// These are for things which must vary strictly on game type and not 
// simply whether or not a given game's definitions are enabled.
//

static const char *e_typenames[] =
{
   "DOOM",
   "HERETIC",
};

//
// edf_ifgametype
//
// haleyjd 09/06/05: Just like ifenabled, but considers the game type
// from the game mode info instead of enable values.
//
static int edf_ifgametype(cfg_t *cfg, cfg_opt_t *opt, int argc,
                          const char **argv)
{
   int i, type;
   bool type_match = false;

   if(argc < 1)
   {
      cfg_error(cfg, "wrong number of args to ifgametype()\n");
      return 1;
   }

   for(i = 0; i < argc; ++i)
   {
      type = E_StrToNumLinear(e_typenames, NumGameModeTypes, argv[i]);

      // if the gametype matches ANY specified, we will process the
      // block in question (can short circuit after first match)
      if((type_match = (type_match || type == GameModeInfo->type)))
         break;
   }

   // no type was matched?
   if(!type_match)
   {
      // force libConfuse to look for an endif function
      cfg->flags |= CFGF_LOOKFORFUNC;
      cfg->lookfor = "endif";
   }

   return 0;
}

//
// edf_ifngametype
//
// haleyjd 09/06/05: As above, but negated.
//
static int edf_ifngametype(cfg_t *cfg, cfg_opt_t *opt, int argc,
                           const char **argv)
{
   int i, type;
   bool type_nomatch = true;

   if(argc < 1)
   {
      cfg_error(cfg, "wrong number of args to ifngametype()\n");
      return 1;
   }

   for(i = 0; i < argc; ++i)
   {
      type = E_StrToNumLinear(e_typenames, NumGameModeTypes, argv[i]);

      // if gametype equals NONE of the parameters, we will process
      // the block (can short circuit after first failure)
      if(!(type_nomatch = (type_nomatch && type != GameModeInfo->type)))
         break;
   }

   // did it match some type?
   if(!type_nomatch)
   {
      // force libConfuse to look for an endif function
      cfg->flags |= CFGF_LOOKFORFUNC;
      cfg->lookfor = "endif";
   }

   return 0;
}

//=============================================================================
//
// EDF Parsing Functions
//

//
// E_CreateCfg
//
// haleyjd 03/21/10: Separated from E_ParseEDF[File|Lump]. Creates and 
// initializes a libConfuse cfg_t object for use by EDF. All definitions
// are now accumulated into this singular cfg_t, as opposed to being
// merged from separately allocated ones for secondary EDF sources such
// as wad lumps.
//
static cfg_t *E_CreateCfg(cfg_opt_t *opts)
{
   cfg_t *cfg;

   E_EDFLogPuts("Creating global cfg_t object\n");

   cfg = cfg_init(opts, CFGF_NOCASE);
   cfg_set_error_function(cfg, edf_error);
   cfg_set_lexer_callback(cfg, E_CheckRoot);

   return cfg;
}

//
// E_ParseEDFFile
//
// Parses the specified file.
//
static void E_ParseEDFFile(cfg_t *cfg, const char *filename)
{
   int err;

   if((err = cfg_parse(cfg, filename)))
   {
      E_EDFLoggedErr(1, 
         "E_ParseEDFFile: failed to parse %s (code %d)\n",
         filename, err);
   }
}

//
// E_ParseLumpRecursive
//
// haleyjd 03/21/10: helper routine for E_ParseEDFLump.
// Recursively descends the lump hash chain.
//
static void E_ParseLumpRecursive(cfg_t *cfg, const char *name, int ln)
{
   if(ln >= 0) // terminal case - lumpnum is -1
   {
      lumpinfo_t **lumpinfo = wGlobalDir.getLumpInfo();

      // recurse on next item
      E_ParseLumpRecursive(cfg, name, lumpinfo[ln]->namehash.next);

      // handle this lump
      if(!strncasecmp(lumpinfo[ln]->name, name, 8) &&         // name match
         lumpinfo[ln]->li_namespace == lumpinfo_t::ns_global) // is global
      {
         int err;

         // try to parse it
         if((err = cfg_parselump(cfg, name, ln)))
         {
            E_EDFLoggedErr(1, 
               "E_ParseEDFLump: failed to parse EDF lump %s (#%d, code %d)\n",
               name, ln, err);
         }
      }
   }
}

//
// E_ParseEDFLump
//
// Parses the specified lump. All lumps of the given name will be parsed
// recursively from oldest to newest.
//
static void E_ParseEDFLump(cfg_t *cfg, const char *lumpname)
{
   lumpinfo_t *root;

   // make sure there is at least one lump before starting recursive parsing
   if(W_CheckNumForName(lumpname) < 0)
      E_EDFLoggedErr(1, "E_ParseEDFLump: lump %s not found\n", lumpname);

   // get root of the hash chain for the indicated name
   root = wGlobalDir.getLumpNameChain(lumpname);

   // parse all lumps of this name recursively in last-to-first order
   E_ParseLumpRecursive(cfg, lumpname, root->namehash.index);
}

//
// E_ParseEDFLumpOptional
//
// Calls the function above, but checks to make sure the lump exists
// first so that an error will not occur. Returns immediately if the 
// lump wasn't found.
//
static bool E_ParseEDFLumpOptional(cfg_t *cfg, const char *lumpname)
{
   // check first and return without an error
   if(W_CheckNumForName(lumpname) == -1)
      return false;

   E_ParseEDFLump(cfg, lumpname);

   return true;
}

//
// Independent EDF Lump Names
//
static const char *edf_lumpnames[] =
{
   "ESTRINGS",
   "ETERRAIN",
   "EMENUS",
   "ESNDSEQ",
   "ESNDINFO",
   "EFONTS",
   "EREVERBS",
   "EWEAPONS",
};

//
// E_ParseIndividualLumps
//
// haleyjd 03/21/10: consolidated function to handle the parsing of all
// independent EDF lumps.
//
static void E_ParseIndividualLumps(cfg_t *cfg)
{
   for(const char *lumpname : edf_lumpnames)
   {
      E_EDFLogPrintf("\t* Parsing %s lump", lumpname);

      if(!E_ParseEDFLumpOptional(cfg, lumpname))
         E_EDFLogPuts(" - not found, skipping.\n");
      else
         E_EDFLogPuts("\n");
   }
}

//
// E_ProcessSpriteVars
//
// Sets the sprite numbers to be used for the player and blank
// sprites by looking up a provided name in the sprite hash
// table.
//
static void E_ProcessSpriteVars(cfg_t *cfg)
{
   static bool firsttime = true;
   int sprnum;
   const char *str;

   E_EDFLogPuts("\t* Processing sprite variables\n");

   // haleyjd 11/11/09: removed processing of obsolete playersprite variable

   // haleyjd 11/21/11: on subsequent runs, only replace if changed
   if(!firsttime && cfg_size(cfg, ITEM_BLANKSPRITE) == 0)
      return;

   firsttime = false;

   // load blank sprite number

   str = cfg_getstr(cfg, ITEM_BLANKSPRITE);
   sprnum = E_SpriteNumForName(str);
   if(sprnum == -1)
   {
      E_EDFLoggedErr(2, 
         "E_ProcessSpriteVars: invalid blank sprite name: '%s'\n", str);
   }
   E_EDFLogPrintf("\t\tSet sprite %s(#%d) as blank sprite\n", str, sprnum);
   blankSpriteNum = sprnum;
}

// haleyjd 04/13/08: this replaces S_sfx[0].
sfxinfo_t NullSound =
{
   { 'n', 'o', 'n', 'e', '\0' }, // name
   { '\0' },                     // pcslump
   sfxinfo_t::sg_none,           // singularity
   255, 0, 0,                    // priority, pitch, volume
   sfxinfo_t::pitch_none,        // pitch_type
   0,                            // skin sound
   CHAN_AUTO,                    // subchannel
   0, 0, 0,                      // flags, clipping_dist, close_dist
   NULL, NULL, NULL, 0,          // link, alias, random sounds
   NULL, 0, 0, 0,                // data, length, alen, usefulness
   { 'n', 'o', 'n', 'e', '\0' }, // mnemomnic
   { NULL, NULL, NULL, 0 },      // numlinks
   NULL,                         // next
   0                             // dehackednum
};

//
// E_CollectNames
//
// Pre-creates and hashes by name states, things, and inventory
// definitions, for purpose of mutual and forward references.
//
static void E_CollectNames(cfg_t *cfg)
{
   E_CollectStates(cfg);    // see e_states.cpp
   E_CollectThings(cfg);    // see e_things.cpp
}

//
// E_ProcessStatesAndThings
//
// E_ProcessEDF now calls this function to accomplish all state
// and thing processing. 
//
static void E_ProcessStatesAndThings(cfg_t *cfg)
{
   E_EDFLogPuts("\t* Beginning state and thing processing\n");

   // allocate structures, build mnemonic and dehnum hash tables
   E_CollectNames(cfg);

   // process states: see e_states.c
   E_ProcessStates(cfg);

   // process frameblocks, also in e_states.c
   E_ProcessFrameBlocks(cfg);

   // process things: see e_things.c
   E_ProcessThings(cfg);

   // process thing groups. Needs to be after things.
   E_ProcessThingGroups(cfg);
}

//
// E_ProcessPlayerData
//
// Processes all player-related sections.
//
static void E_ProcessPlayerData(cfg_t *cfg)
{
   E_ProcessSkins(cfg);
   E_ProcessPlayerClasses(cfg);
}

//
// E_ProcessCast
//
// Creates the DOOM II cast call information
//
static void E_ProcessCast(cfg_t *cfg)
{
   static bool firsttime = true;
   int numcastorder = 0, numcastsections = 0;
   cfg_t **ci_order;

   E_EDFLogPuts("\t* Processing cast call\n");
   
   // get number of cast sections
   numcastsections = cfg_size(cfg, SEC_CAST);

   if(firsttime && !numcastsections) // on main parse, at least one is required.
      E_EDFLoggedErr(2, "E_ProcessCast: no cast members defined.\n");

   firsttime = false;

   E_EDFLogPrintf("\t\t%d cast member(s) defined\n", numcastsections);

   // haleyjd 11/21/11: allow multiple runs
   if(castorder)
   {
      // free names
      for(int i = 0; i < max_castorder; i++)
      {
         if(castorder[i].name)
            efree(castorder[i].name);
      }
      // free castorder
      efree(castorder);
      castorder = NULL;
      max_castorder = 0;
   }

   // check if the "castorder" array is defined for imposing an
   // order on the castinfo sections
   numcastorder = cfg_size(cfg, SEC_CASTORDER);

   E_EDFLogPrintf("\t\t%d cast member(s) in castorder\n", numcastorder);

   // determine size of castorder
   max_castorder = (numcastorder > 0) ? numcastorder : numcastsections;

   // allocate with size+1 for an end marker
   castorder = estructalloc(castinfo_t, max_castorder + 1);
   ci_order  = ecalloc(cfg_t **, sizeof(cfg_t *), max_castorder);

   if(numcastorder > 0)
   {
      for(int i = 0; i < numcastorder; i++)
      {
         const char *title = cfg_getnstr(cfg, SEC_CASTORDER, i);         
         cfg_t *section    = cfg_gettsec(cfg, SEC_CAST, title);

         if(!section)
         {
            E_EDFLoggedErr(2, 
               "E_ProcessCast: unknown cast member '%s' in castorder\n", 
               title);
         }

         ci_order[i] = section;
      }
   }
   else
   {
      // no castorder array is defined, so use the cast members
      // in the order they are encountered (for backward compatibility)
      for(int i = 0; i < numcastsections; i++)
         ci_order[i] = cfg_getnsec(cfg, SEC_CAST, i);
   }


   for(int i = 0; i < max_castorder; i++)
   {
      int j;
      const char *tempstr;
      int tempint = 0;
      cfg_t *castsec = ci_order[i];
      mobjinfo_t *mi = nullptr;

      // resolve thing type
      tempstr = cfg_getstr(castsec, ITEM_CAST_TYPE);
      if(!tempstr || 
         (tempint = E_ThingNumForName(tempstr)) == -1)
      {
         E_EDFLoggedWarning(2, "Warning: cast %d: unknown thing type %s\n",
                            i, tempstr);

         tempint = UnknownThingType;
      }
      castorder[i].type = tempint;
      mi = mobjinfo[castorder[i].type];

      // get cast name, if any -- the first seventeen entries can
      // default to using the internal string editable via BEX strings
      tempstr = cfg_getstr(castsec, ITEM_CAST_NAME);
      if(cfg_size(castsec, ITEM_CAST_NAME) == 0 && i < 17)
         castorder[i].name = NULL; // set from DeHackEd
      else
         castorder[i].name = estrdup(tempstr); // store provided value

      // get stopattack flag (used by player)
      castorder[i].stopattack = cfg_getbool(castsec, ITEM_CAST_SA);

      // process sound blocks (up to four will be processed)
      tempint = cfg_size(castsec, ITEM_CAST_SOUND);
      for(j = 0; j < 4; j++)
      {
         castorder[i].sounds[j].frame = 0;
         castorder[i].sounds[j].sound = 0;
      }
      for(j = 0; j < tempint && j < 4; j++)
      {
         sfxinfo_t *sfx;
         const char *name;
         cfg_t *soundsec = cfg_getnsec(castsec, ITEM_CAST_SOUND, j);

         // if these are invalid, just fill them with zero rather
         // than causing an error, as they're very unimportant

         // name of sound to play
         name = cfg_getstr(soundsec, ITEM_CAST_SOUNDNAME);
         
         // haleyjd 03/22/06: modified to support dehnum auto-allocation
         if((sfx = E_EDFSoundForName(name)) == NULL)
         {
            E_EDFLoggedWarning(2, "Warning: cast member references invalid sound %s\n",
                               name);
            castorder[i].sounds[j].sound = 0;
         }
         else
         {
            if(sfx->dehackednum != -1 || E_AutoAllocSoundDEHNum(sfx))
               castorder[i].sounds[j].sound = sfx->dehackednum;
            else
            {
               E_EDFLoggedWarning(2, "Warning: could not auto-allocate a DeHackEd number "
                                     "for sound %s\n", name);
               castorder[i].sounds[j].sound = 0;
            }
         }

         // name of frame that triggers sound event
         name = cfg_getstr(soundsec, ITEM_CAST_SOUNDFRAME);
         castorder[i].sounds[j].frame = E_SafeStateNameOrLabel(mi, name);
      }
   }

   // initialize the end marker to all zeroes
   memset(&castorder[max_castorder], 0, sizeof(castinfo_t));

   // free the ci_order table
   efree(ci_order);
}

// haleyjd 11/19/03:
// a default probability array for the boss_spawn_probs list,
// for backward compatibility

static int BossDefaults[11] = 
{
   50, 90, 120, 130, 160, 162, 172, 192, 222, 246, 256
};

//
// E_ProcessBossTypes
//
// Gets the thing type entries in the boss_spawner_types list,
// for use by the SpawnFly codepointer.
//
// modified by schepe to remove 11-type limit
//
static void E_ProcessBossTypes(cfg_t *cfg)
{
   int i, a = 0;
   int numTypes = cfg_size(cfg, SEC_BOSSTYPES);
   int numProbs = cfg_size(cfg, SEC_BOSSPROBS);
   bool useProbs = true;

   E_EDFLogPuts("\t* Processing boss spawn types\n");

   if(!numTypes)
   {
      // haleyjd 05/31/06: allow zero boss types
      E_EDFLogPuts("\t\tNo boss types defined\n");
      return;
   }

   // haleyjd 11/19/03: allow defaults for boss spawn probs
   if(!numProbs)
      useProbs = false;

   if(useProbs ? numTypes != numProbs : numTypes != 11)
   {
      E_EDFLoggedErr(2, 
         "E_ProcessBossTypes: %d boss types, %d boss probs\n",
         numTypes, useProbs ? numProbs : 11);
   }

   // haleyjd 11/21/11: allow multiple runs
   if(BossSpawnTypes)
   {
      efree(BossSpawnTypes);
      BossSpawnTypes = NULL;
   }
   if(BossSpawnProbs)
   {
      efree(BossSpawnProbs);
      BossSpawnProbs = NULL;
   }

   NumBossTypes = numTypes;
   BossSpawnTypes = ecalloc(int *, numTypes, sizeof(int));
   BossSpawnProbs = ecalloc(int *, numTypes, sizeof(int));

   // load boss spawn probabilities
   for(i = 0; i < numTypes; ++i)
   {
      if(useProbs)
      {
         a += cfg_getnint(cfg, SEC_BOSSPROBS, i);
         BossSpawnProbs[i] = a;
      }
      else
         BossSpawnProbs[i] = BossDefaults[i];
   }

   // check that the probabilities total 256
   if(useProbs && a != 256)
   {
      E_EDFLoggedErr(2, 
         "E_ProcessBossTypes: boss spawn probs do not total 256\n");
   }

   for(i = 0; i < numTypes; ++i)
   {
      const char *typeName = cfg_getnstr(cfg, SEC_BOSSTYPES, i);
      int typeNum = E_ThingNumForName(typeName);

      if(typeNum == -1)
      {
         E_EDFLoggedWarning(2, "Warning: invalid boss type '%s'\n", typeName);

         typeNum = UnknownThingType;
      }

      BossSpawnTypes[i] = typeNum;

      E_EDFLogPrintf("\t\tAssigned type %s(#%d) to boss type %d\n",
                     mobjinfo[typeNum]->name, typeNum, i);
   }
}

extern int wi_pause_time;
extern int wi_fade_color;
extern fixed_t wi_tl_level;

//
// E_ProcessMiscVars
//
// Miscellaneous optional EDF settings.
//
static void E_ProcessMiscVars(cfg_t *cfg)
{
   // allow setting of title length for DOOM II
   if(cfg_size(cfg, ITEM_D2TITLETICS) > 0)
      GameModeInfoObjects[commercial]->titleTics = cfg_getint(cfg, ITEM_D2TITLETICS);

   // allow setting a pause time for the intermission
   if(cfg_size(cfg, ITEM_INTERPAUSE) > 0)
      wi_pause_time = cfg_getint(cfg, ITEM_INTERPAUSE);

   if(cfg_size(cfg, ITEM_INTERFADE) > 0)
   {
      wi_fade_color = cfg_getint(cfg, ITEM_INTERFADE);
      if(wi_fade_color < 0)
         wi_fade_color = 0;
      else if(wi_fade_color > 255)
         wi_fade_color = 255;
   }

   if(cfg_size(cfg, ITEM_INTERTL) > 0)
   {
      wi_tl_level = cfg_getint(cfg, ITEM_INTERTL);
      if(wi_tl_level < 0)
         wi_tl_level = 0;
      else if(wi_tl_level > 65536)
         wi_tl_level = 65536;
   }
}

//=============================================================================
//
// Main EDF Routines
//

//
// E_InitEDF
//
// Shared initialization code for E_ProcessEDF and E_ProcessNewEDF
//
static cfg_t *E_InitEDF()
{
   // set warning count to zero
   E_EDFResetWarnings();

   // Check for -edfout to enable verbose logging
   if(M_CheckParm("-edfout"))
      E_EDFOpenVerboseLog();

   // Create the one and only cfg_t
   return E_CreateCfg(edf_opts);
}

//
// E_ParseEDF
//
// Shared parsing phase code for E_ProcessEDF and E_ProcessNewEDF
//
static void E_ParseEDF(cfg_t *cfg, const char *filename)
{
   E_EDFLogPuts("\n===================== Parsing Phase =====================\n");

   // Parse the "root" EDF, which is either a chain of wad lumps named EDFROOT;
   // or a single file, which is determined by the -edf parameter, a GFS script,
   // the /base/game directory, or the default master EDF, which is root.edf in
   // /base.

   if(W_CheckNumForName("EDFROOT") != -1)
   {
      puts("E_ProcessEDF: Loading root lump.\n");
      E_EDFLogPuts("\t* Parsing lump EDFROOT\n");

      E_ParseEDFLump(cfg, "EDFROOT");
   }
   else if(filename)
   {
      printf("E_ProcessEDF: Loading root file %s\n", filename);
      E_EDFLogPrintf("\t* Parsing EDF file %s\n", filename);

      E_ParseEDFFile(cfg, filename);
   }

   // Parse independent EDF lumps, which are not supposed to have been included
   // through any of the above (though if they have been, this is now accounted
   // for via the SHA-1 hashing mechanism which avoids reparses).
   E_ParseIndividualLumps(cfg);
}

//
// E_DoEDFProcessing
//
// haleyjd 11/21/11: Shared processing phase code, now that all EDF sections
// are fully dynamic with repeatable processing.
//
static void E_DoEDFProcessing(cfg_t *cfg, bool firsttime)
{
   E_EDFLogPuts("\n=================== Processing Phase ====================\n");

   // Echo final enable values to the log file for reference.
   if(firsttime)
      E_EchoEnables();

   // NOTE: The order of most of the following calls is extremely 
   // important and must be preserved, unless the static routines 
   // above and in other files are rewritten accordingly.
   
   // process strings
   E_ProcessStrings(cfg);

   // process sprites
   E_ProcessSprites(cfg);

   // process sounds
   E_ProcessSounds(cfg);

   // process ambience information
   E_ProcessAmbience(cfg);

   // process sound sequences
   E_ProcessSndSeqs(cfg);

   // process reverb definitions (12/22/13)
   E_ProcessReverbs(cfg);

   // process damage types
   E_ProcessDamageTypes(cfg);

   // process frame and thing definitions (made dynamic 11/06/11)
   E_ProcessStatesAndThings(cfg);
 
   // process sprite-related variables (made dynamic 11/21/11)
   E_ProcessSpriteVars(cfg);

   // process hitscan puff effects
   E_ProcessPuffs(cfg);

   // collect the weapons now, so that weapon tracker can be autogenerated
   E_CollectWeapons(cfg);

   // process inventory
   E_ProcessInventory(cfg);

   // process weapons
   E_ProcessWeaponInfo(cfg);

   // process player sections
   E_ProcessPlayerData(cfg);

   // process cast call (made dynamic 11/21/11)
   E_ProcessCast(cfg);

   // process boss spawn types (made dynamic 11/21/11)
   E_ProcessBossTypes(cfg);

   // process TerrainTypes
   E_ProcessTerrainTypes(cfg);

   // process switches and animations
   E_ProcessSwitches(cfg);
   E_ProcessAnimations(cfg);

   // process dynamic menus
   MN_ProcessMenus(cfg);

   // process fonts
   E_ProcessFonts(cfg);

   // process misc vars (made dynamic 11/21/11)
   E_ProcessMiscVars(cfg);

   // post main-processing
   E_ProcessPickups(cfg);
   E_ProcessThingPickups(cfg);

   // 08/30/03: apply deltas
   E_ProcessSoundDeltas(cfg, true); // see e_sound.cpp
   E_ProcessStateDeltas(cfg);       // see e_states.cpp
   E_ProcessThingDeltas(cfg);       // see e_things.cpp
   E_ProcessWeaponDeltas(cfg);      // see e_weapons.cpp
   E_ProcessPlayerDeltas(cfg);      // see e_player.cpp
   E_ProcessFontDeltas(cfg);        // see e_fonts.cpp

   // 07/19/12: game properties
   E_ProcessGameProperties(cfg);    // see e_gameprops.cpp

   // post-processing routines
   E_SetThingDefaultSprites();
   E_ProcessFinalWeaponSlots();
}

//
// E_CleanUpEDF
//
// Shared shutdown phase code for E_ProcessEDF and E_ProcessNewEDF
//
static void E_CleanUpEDF(cfg_t *cfg)
{
   E_EDFLogPuts("\n==================== Shutdown Phase =====================\n");

   // Free the cfg_t object.
   E_EDFLogPuts("\t* Freeing main cfg object\n");
   cfg_free(cfg);

   // check heap integrity for safety
   E_EDFLogPuts("\t* Checking zone heap integrity\n");
   Z_CheckHeap();

   // close the verbose log file
   E_EDFCloseVerboseLog();

   // Output warnings display
   E_EDFPrintWarningCount();
}

//
// E_ProcessEDF
//
// Public function to parse and process the root EDF file. Called by 
// D_DoomInit.  Assumes that certain BEX data structures, especially the
// codepointer hash table, have already been built.
//
void E_ProcessEDF(const char *filename)
{
   cfg_t *cfg;
   
   //
   // Initialization - open log and create a cfg_t
   //
   cfg = E_InitEDF();

   //
   // Parsing
   //
   // haleyjd 03/21/10: All parsing is now streamlined into a single process,
   // using the unified cfg_t object created above.
   //
   E_ParseEDF(cfg, filename);

   //
   // Processing
   //
   // Now we chew on all of the data accumulated from the various sources.
   //
   E_DoEDFProcessing(cfg, true);

   //
   // Shutdown and Cleanup
   //
   E_CleanUpEDF(cfg);
}

//
// E_ProcessNewEDF
//
// haleyjd 03/24/10: This routine is called to do parsing and processing of new
// EDF lumps when loading wad files at runtime.
//
void E_ProcessNewEDF()
{
   cfg_t *cfg;
   
   //
   // Initialization - open log and create a cfg_t
   //
   cfg = E_InitEDF();

   //
   // Parsing - parse only EDFROOT lumps, not root.edf
   //
   E_ParseEDF(cfg, NULL);

   //
   // Processing
   //
   E_DoEDFProcessing(cfg, false);

   //
   // Shutdown and Cleanup
   //
   E_CleanUpEDF(cfg);

   // clear cached state argument evaluations
   E_ResetAllArgEvals();

   // clear cached sounds
   E_UpdateSoundCache();
}

// EOF

