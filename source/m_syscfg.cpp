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
//    "System" configuration file.
//    Sets some low-level stuff that must be read before the main config
//    file can be loaded.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "doomstat.h"
#include "d_iwad.h"
#include "d_main.h"
#include "d_net.h"
#include "d_gi.h"
#include "gl/gl_vars.h"
#include "hal/i_gamepads.h"
#include "hal/i_picker.h"
#include "hal/i_platform.h"
#include "i_sound.h"
#include "i_video.h"
#include "m_misc.h"
#include "m_shots.h"
#include "mn_menus.h"
#include "r_context.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "w_wad.h"
#include "w_levels.h"

// External variables configured here:

extern int textmode_startup;
extern int realtic_clock_rate; // killough 4/13/98: adjustable timer
extern int iwad_choice;        // haleyjd 03/19/10
extern int fov;

#ifdef _SDL_VER
extern int waitAtExit;
extern int grabmouse;
extern int use_vsync;
extern int audio_buffers;
#endif

#if defined _MSC_VER
extern int disable_sysmenu;
#endif

// option name defines

#define ITEM_USE_DOOM_CONFIG    "use_doom_config"

#define ITEM_IWAD_DOOM_SW       "iwad_doom_shareware"
#define ITEM_IWAD_DOOM          "iwad_doom"
#define ITEM_IWAD_ULTIMATE_DOOM "iwad_ultimate_doom"
#define ITEM_IWAD_DOOM2         "iwad_doom2"
#define ITEM_IWAD_BFGDOOM2      "iwad_bfgdoom2"
#define ITEM_IWAD_TNT           "iwad_tnt"
#define ITEM_IWAD_PLUTONIA      "iwad_plutonia"
#define ITEM_PWAD_ID24RES       "pwad_id24res"
#define ITEM_IWAD_HACX          "iwad_hacx"
#define ITEM_IWAD_HERETIC_SW    "iwad_heretic_shareware"
#define ITEM_IWAD_HERETIC       "iwad_heretic"
#define ITEM_IWAD_HERETIC_SOSR  "iwad_heretic_sosr"
#define ITEM_IWAD_FREEDOOM      "iwad_freedoom"
#define ITEM_IWAD_FREEDOOMU     "iwad_freedoomu"
#define ITEM_IWAD_FREEDM        "iwad_freedm"
#define ITEM_IWAD_REKKR         "iwad_rekkr"
#define ITEM_IWAD_CHOICE        "iwad_choice"

// system defaults array

static default_t sysdefaults[] =
{
      //jff 3/3/98
   DEFAULT_INT("config_help", &config_help, nullptr, 1, 0, 1, default_t::wad_no,
               "1 to show help strings about each variable in config file"),

   DEFAULT_INT(ITEM_USE_DOOM_CONFIG, &use_doom_config, nullptr, 1, 0, 1, default_t::wad_no,
               "1 to use user/doom/eternity.cfg for all DOOM gamemodes"),

   // IWAD paths

   DEFAULT_BOOL("d_scaniwads", &d_scaniwads, nullptr, true, default_t::wad_no,
                "1 to scan common locations for IWADs"),

   DEFAULT_STR(ITEM_IWAD_DOOM_SW, &gi_path_doomsw, nullptr, "", default_t::wad_no,
               "IWAD path for DOOM Shareware"),

   DEFAULT_STR(ITEM_IWAD_DOOM, &gi_path_doomreg, nullptr, "", default_t::wad_no,
               "IWAD path for DOOM Registered"),

   DEFAULT_STR(ITEM_IWAD_ULTIMATE_DOOM, &gi_path_doomu, nullptr, "", default_t::wad_no,
               "IWAD path for The Ultimate DOOM"),

   DEFAULT_STR(ITEM_IWAD_DOOM2, &gi_path_doom2, nullptr, "", default_t::wad_no,
               "IWAD path for DOOM 2"),

   DEFAULT_STR(ITEM_IWAD_BFGDOOM2, &gi_path_bfgdoom2, nullptr, "", default_t::wad_no,
               "IWAD path for DOOM 2, BFG Edition"),

   DEFAULT_STR(ITEM_IWAD_TNT, &gi_path_tnt, nullptr, "", default_t::wad_no,
               "IWAD path for Final DOOM: TNT - Evilution"),

   DEFAULT_STR(ITEM_IWAD_PLUTONIA, &gi_path_plut, nullptr, "", default_t::wad_no,
               "IWAD path for Final DOOM: The Plutonia Experiment"),

   DEFAULT_STR(ITEM_IWAD_HACX, &gi_path_hacx, nullptr, "", default_t::wad_no,
               "IWAD path for HACX - Twitch 'n Kill (v1.2 or later)"),

   DEFAULT_STR(ITEM_IWAD_HERETIC_SW, &gi_path_hticsw, nullptr, "", default_t::wad_no,
               "IWAD path for Heretic Shareware"),

   DEFAULT_STR(ITEM_IWAD_HERETIC, &gi_path_hticreg, nullptr, "", default_t::wad_no,
               "IWAD path for Heretic Registered"),

   DEFAULT_STR(ITEM_IWAD_HERETIC_SOSR, &gi_path_sosr, nullptr, "", default_t::wad_no,
               "IWAD path for Heretic: Shadow of the Serpent Riders"),

   DEFAULT_STR(ITEM_IWAD_FREEDOOM, &gi_path_fdoom, nullptr, "", default_t::wad_no,
               "IWAD path for Freedoom (Doom 2 gamemission)"),

   DEFAULT_STR(ITEM_IWAD_FREEDOOMU, &gi_path_fdoomu, nullptr, "", default_t::wad_no,
               "IWAD path for Freedoom (Ultimate Doom gamemission)"),

   DEFAULT_STR(ITEM_IWAD_FREEDM, &gi_path_freedm, nullptr, "", default_t::wad_no,
               "IWAD path for FreeDM (Freedoom Deathmatch IWAD)"),

   DEFAULT_STR(ITEM_IWAD_REKKR, &gi_path_rekkr, nullptr, "", default_t::wad_no,
               "IWAD path for Rekkr (Ultimate Doom gamemission)"),

   DEFAULT_INT(ITEM_IWAD_CHOICE, &iwad_choice, nullptr, -1, -1, NUMPICKIWADS, default_t::wad_no,
               "Number of last IWAD chosen from the IWAD picker"),

   DEFAULT_STR(ITEM_PWAD_ID24RES, &gi_path_id24res, nullptr, "", default_t::wad_no,
               "PWAD path for ID24 resources"),

   DEFAULT_STR("master_levels_dir", &w_masterlevelsdirname, nullptr, "", default_t::wad_no,
               "Directory containing Master Levels wad files"),

   DEFAULT_STR("w_norestpath", &w_norestpath, nullptr, "", default_t::wad_no,
               "Path to 'No Rest for the Living'"),

   // 11/04/09: system-level options moved here from the main config

   DEFAULT_INT("textmode_startup", &textmode_startup, nullptr, 0, 0, 1, default_t::wad_no,
               "Start up ETERNITY in text mode"),

   DEFAULT_INT("realtic_clock_rate", &realtic_clock_rate, nullptr, 100, 10, 1000, default_t::wad_no,
               "Percentage of normal speed (35 fps) realtic clock runs at"),

   // killough
   DEFAULT_INT("snd_channels", &default_numChannels, nullptr, 32, 1, 32, default_t::wad_no,
               "number of sound effects handled simultaneously"),

   // haleyjd 12/08/01
   DEFAULT_INT("force_flip_pan", &forceFlipPan, nullptr, 0, 0, 1, default_t::wad_no,
               "Force reversal of stereo audio channels: 0 = normal, 1 = reverse"),

   // haleyjd 04/21/10
   DEFAULT_FLOAT("s_lowfreq", &s_lowfreq, nullptr, 880.0, 0, UL, default_t::wad_no,
                 "High end of low pass band"),
   
   DEFAULT_FLOAT("s_highfreq", &s_highfreq, nullptr, 5000.0, 0, UL, default_t::wad_no,
                 "Low end of high pass band"),

   DEFAULT_FLOAT("s_eqpreamp", &s_eqpreamp, nullptr, 0.93896, 0, 100, default_t::wad_no,
                 "Preamplification factor"),

   DEFAULT_FLOAT("s_lowgain", &s_lowgain, nullptr, 1.2, 0, 300, default_t::wad_no,
                 "Low pass gain"),

   DEFAULT_FLOAT("s_midgain", &s_midgain, nullptr, 1.0, 0, 300, default_t::wad_no,
                 "Midrange gain"),

   DEFAULT_FLOAT("s_highgain", &s_highgain, nullptr, 0.8, 0, 300, default_t::wad_no,
                 "High pass gain"),  

   DEFAULT_INT("s_enviro_volume", &s_enviro_volume, nullptr, 4, 0, 16, default_t::wad_no,
               "Volume of environmental sound sequences"),

   // haleyjd 12/24/11
   DEFAULT_BOOL("s_hidefmusic", &s_hidefmusic, nullptr, true, default_t::wad_no,
                "use hi-def music if available"),

   // jff 3/30/98 add ability to take screenshots in BMP format
   DEFAULT_INT("screenshot_pcx", &screenshot_pcx, nullptr, 3, 0, 3, default_t::wad_no,
               "screenshot format (0=BMP,1=PCX,2=TGA,3=PNG)"),
   
   DEFAULT_INT("screenshot_gamma", &screenshot_gamma, nullptr, 1, 0, 1, default_t::wad_no,
               "1 to use gamma correction in screenshots"),

   DEFAULT_INT("i_videodriverid", &i_videodriverid, nullptr, -1, -1, VDR_MAXDRIVERS-1, 
               default_t::wad_no, i_videohelpstr),

   DEFAULT_STR("i_resolution", &i_default_resolution, &i_resolution, "native", default_t::wad_no,
               "Resolution of the renderer's target (WWWWxHHHH or native)"),

   DEFAULT_STR("i_videomode", &i_default_videomode, &i_videomode, "640x480w", default_t::wad_no,
               "Description of video mode parameters (WWWWxHHHH[flags])"),

   DEFAULT_BOOL("i_letterbox", &i_letterbox, nullptr, false, default_t::wad_no, 
                "Letterbox video modes with aspect ratios narrower than 4:3"),

   DEFAULT_INT("use_vsync", &use_vsync, nullptr, 1, 0, 1, default_t::wad_no,
               "1 to enable wait for vsync to avoid display tearing"),

   DEFAULT_INT("mn_favaspectratio", &mn_favaspectratio, nullptr, 0, 0, AR_NUMASPECTRATIOS-1,
               default_t::wad_no, "Favorite aspect ratio for selection in menus"),

   DEFAULT_INT("mn_favscreentype", &mn_favscreentype, nullptr, 0, 0, MN_NUMSCREENTYPES-1,
               default_t::wad_no, "Favorite screen type for selection in menus"),

   DEFAULT_BOOL("gl_use_extensions", &cfg_gl_use_extensions, nullptr, true, default_t::wad_no,
                "1 to enable use of GL extensions in general"),

   DEFAULT_BOOL("gl_arb_pixelbuffer", &cfg_gl_arb_pixelbuffer, nullptr, false, default_t::wad_no,
                "1 to enable use of GL ARB pixelbuffer object extension"),

   DEFAULT_INT("gl_colordepth", &cfg_gl_colordepth, nullptr, 32, 16, 32, default_t::wad_no,
               "GL backend screen bitdepth (16, 24, or 32)"),

   DEFAULT_INT("gl_filter_type", &cfg_gl_filter_type, nullptr, CFG_GL_NEAREST,
               0, CFG_GL_NUMFILTERS-1, default_t::wad_no,
               "GL2D texture filtering type (0 = GL_LINEAR, 1 = GL_NEAREST)"),

   DEFAULT_BOOL("d_fastrefresh", &d_fastrefresh, nullptr, true, default_t::wad_no,
                "1 to refresh as fast as possible (uses high CPU)"),

   DEFAULT_BOOL("d_interpolate", &d_interpolate, nullptr, true, default_t::wad_no,
                "1 to activate frame interpolation (smooth rendering)"),

   DEFAULT_BOOL("i_forcefeedback", &i_forcefeedback, nullptr, true, default_t::wad_no,
                "1 to enable force feedback through gamepads where supported"),

   DEFAULT_INT("r_numcontexts", &r_numcontexts, nullptr, 1, 1, UL, default_t::wad_no,
               "Amount of renderer threads to run"),

#ifdef _SDL_VER
   DEFAULT_INT("displaynum", &displaynum, nullptr, 0, 0, UL, default_t::wad_no,
               "Display number that the window appears on"),

   DEFAULT_INT("wait_at_exit", &waitAtExit, nullptr, 0, 0, 1, default_t::wad_no,
               "Always wait for input at exit"),
   
   DEFAULT_INT("grabmouse", &grabmouse, nullptr, 1, 0, 1, default_t::wad_no,
               "Toggle mouse input grabbing"),

   DEFAULT_INT("audio_buffers", &audio_buffers, nullptr, 2048, 1024, 8192, default_t::wad_no,
               "SDL_mixer audio buffer size"),

#endif

#ifdef _MSC_VER
   DEFAULT_INT("disable_sysmenu", &disable_sysmenu, nullptr, 1, 0, 1, default_t::wad_no,
               "1 to disable Windows system menu for alt+space compatibility"),
#endif

   // last entry
   DEFAULT_END()
};

//
// System Configuration File Object
//
static defaultfile_t sysdeffile =
{
   sysdefaults,
   sizeof(sysdefaults) / sizeof(*sysdefaults) - 1,
};

//
// M_LoadSysConfig
//
// Parses the system configuration file from the base folder.
//
void M_LoadSysConfig(const char *filename)
{
   startupmsg("M_LoadSysConfig", "Loading system.cfg");

   sysdeffile.fileName = estrdup(filename);

   M_LoadDefaultFile(&sysdeffile);
}

//
// M_SaveSysConfig
//
// Saves values to the system configuration file in the base folder.
//
void M_SaveSysConfig(void)
{
   M_SaveDefaultFile(&sysdeffile);
}

//
// M_GetSysDefaults
//
// haleyjd 07/04/10: Needed in m_misc.c for cvar searches.
//
default_t *M_GetSysDefaults(void)
{
   return sysdefaults;
}

// EOF

