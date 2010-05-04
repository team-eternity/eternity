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
//  Default Config File.
//
// NETCODE_FIXME -- CONSOLE_FIXME -- CONFIG_FIXME:
// All configuration file code is redundant with the console variable
// system. These systems sorely need to be integrated so that configuration
// comes from a console script instead of a separate file format. Need
// to do this without breaking config-in-wad capability, or need to 
// come up with some other easy way for projects to override defaults.
// ALL archived console variables need to be adjusted to have defaults,
// and a way to set their value without changing the default must be
// added. See other CONSOLE_FIXME notes for more info.
//
//-----------------------------------------------------------------------------

#include <sys/stat.h>

#include "z_zone.h"
#include "d_gi.h"
#include "m_misc.h"
#include "m_argv.h"
#include "w_wad.h"

// headers needed for externs:
#include "am_map.h"
#include "c_io.h"
#include "c_net.h"
#include "d_englsh.h"
#include "d_io.h"
#include "d_main.h"
#include "f_wipe.h"
#include "g_game.h"
#include "hu_over.h"
#include "hu_stuff.h"
#include "i_sound.h"
#include "i_video.h"
#include "mn_engin.h"
#include "mn_files.h"
#include "mn_menus.h"
#include "p_map.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_sky.h"
#include "r_things.h"
#include "s_sound.h"
#include "v_video.h"

//
// DEFAULTS
//

int config_help;         //jff 3/3/98
int usemouse;
int usejoystick;

extern int mousebstrafe;
extern int mousebforward;
extern int viewwidth;
extern int viewheight;
extern int mouseSensitivity_horiz,mouseSensitivity_vert;  // killough
extern int leds_always_off;            // killough 3/6/98
extern int showMessages;
extern int screenSize;

extern char *chat_macros[], *wad_files[], *deh_files[];  // killough 10/98
extern char *csc_files[];   // haleyjd: auto-loaded console scripts

extern int hud_msg_timer;   // killough 11/98: timer used for review messages
extern int hud_msg_lines;   // number of message lines in window up to 16
extern int hud_msg_scrollup;// killough 11/98: whether message list scrolls up
extern int message_timer;   // killough 11/98: timer used for normal messages
extern int show_scores;

// haleyjd: SDL-specific configuration values
#ifdef _SDL_VER
extern int  showendoom;
extern int  endoomdelay;
extern char *i_videomode;
extern char *i_default_videomode;
#endif

#ifdef HAVE_SPCLIB
extern int spc_preamp;
extern int spc_bass_boost;
#endif

// haleyjd 10/09/07: wipe waiting
extern int wipewait;

//jff 3/3/98 added min, max, and help string to all entries
//jff 4/10/98 added isstr field to specify whether value is string or int
//
// killough 11/98: entirely restructured to allow options to be modified
// from wads, and to consolidate with menu code

// CONFIG_FIXME: need to restore lost features in future rewrite.

default_t defaults[] = 
{
   DEFAULT_INT("colour", &default_colour, NULL, 0, 0, TRANSLATIONCOLOURS, wad_no,
               "the default player colour (green, indigo, brown, red...)"),

   DEFAULT_STR("name", &default_name, NULL, "player", wad_no,
               "the default player name"),

   // jff 3/24/98 allow default skill setting
   DEFAULT_INT("default_skill", &defaultskill, NULL, 3, 1, 5, wad_no,
               "selects default skill 1=TYTD 2=NTR 3=HMP 4=UV 5=NM"),

   // haleyjd: fixed for cross-platform support -- see m_misc.h
   // jff 1/18/98 allow Allegro drivers to be set,  -1 = autodetect
   DEFAULT_INT("sound_card", &snd_card, NULL, SND_DEFAULT, SND_MIN, SND_MAX, wad_no,
               SND_DESCR),

   DEFAULT_INT("music_card", &mus_card, NULL, MUS_DEFAULT, MUS_MIN, MUS_MAX, wad_no,
               MUS_DESCR),

#ifdef _SDL_VER
   // haleyjd 04/15/02: SDL joystick device number
   DEFAULT_INT("joystick_num", &i_SDLJoystickNum, NULL, -1, -1, UL, wad_no,
               "SDL joystick device number, -1 to disable"),
    
   // joystick sensitivities
   DEFAULT_INT("joystickSens_x", &joystickSens_x, NULL, 0, -32768, 32767, wad_no,
               "SDL joystick horizontal sensitivity"),

   DEFAULT_INT("joystickSens_y", &joystickSens_y, NULL, 0, -32768, 32767, wad_no,
               "SDL joystick vertical sensitivity"),
#endif

   DEFAULT_INT("s_precache", &s_precache, NULL, 0, 0, 1, wad_no,
               "precache sounds at startup"),

#ifdef DJGPP
   // jff 3/4/98 detect # voices
   DEFAULT_INT("detect_voices", &detect_voices, NULL, 1, 0, 1, wad_no,
               "1 enables voice detection prior to calling install sound"),
#endif

#ifdef _SDL_VER
   DEFAULT_STR("i_videomode", &i_default_videomode, &i_videomode, "640x480w", wad_no,
               "description of video mode parameters (WWWWxHHHH[flags])"),
#endif
  
   // killough 10/98
   DEFAULT_INT("disk_icon", &disk_icon, NULL, 1, 0, 1, wad_no, 
               "1 to enable flashing icon during disk IO"),
  
   // killough 2/21/98
   DEFAULT_INT("pitched_sounds", &pitched_sounds, NULL, 0, 0, 1, wad_yes,
               "1 to enable variable pitch in sound effects (from id's original code)"),

   // phares
   DEFAULT_INT("translucency", &general_translucency, NULL, 1, 0, 1, wad_yes,
               "1 to enable translucency for some things"),

   // killough 2/21/98
   DEFAULT_INT("tran_filter_pct", &tran_filter_pct, NULL, 66, 0, 100, wad_yes,
               "set percentage of foreground/background translucency mix"),

   // killough 2/8/98
   DEFAULT_INT("max_player_corpse", &default_bodyquesize, NULL, 32, UL, UL, wad_no,
               "number of dead bodies in view supported (negative value = no limit)"),

   // killough 10/98
   DEFAULT_INT("flashing_hom", &flashing_hom, NULL, 1, 0, 1, wad_no,
               "1 to enable flashing HOM indicator"),

   // killough 3/31/98
   DEFAULT_INT("demo_insurance", &default_demo_insurance, NULL, 2, 0, 2, wad_no,
               "1=take special steps ensuring demo sync, 2=only during recordings"),
   
   // phares
   DEFAULT_INT("weapon_recoil", &default_weapon_recoil, &weapon_recoil, 0, 0, 1, wad_yes,
               "1 to enable recoil from weapon fire"),

   // killough 7/19/98
   // sf:changed to bfgtype
   // haleyjd: FIXME - variable is of enum type, non-portable
   DEFAULT_INT("bfgtype", &default_bfgtype, &bfgtype, 0, 0, 4, wad_yes,
               "0 - normal, 1 - classic, 2 - 11k, 3 - bouncing!, 4 - burst"),

   //sf
   DEFAULT_INT("crosshair", &crosshairnum, NULL, 0, 0, CROSSHAIRS, wad_yes,
               "0 - none, 1 - cross, 2 - angle"),

   // haleyjd 06/07/05
   DEFAULT_BOOL("crosshair_hilite", &crosshair_hilite, NULL, false, wad_yes,
                "0 - no highlighting, 1 - aim highlighting enabled"),

   // sf
   DEFAULT_INT("show_scores", &show_scores, NULL, 0, 0, 1, wad_yes,
               "show scores in deathmatch"),

   DEFAULT_INT("lefthanded", &lefthanded, NULL, 0, 0, 1, wad_yes,
               "0 - right handed, 1 - left handed"),

   // killough 10/98
   DEFAULT_INT("doom_weapon_toggles", &doom_weapon_toggles, NULL, 1, 0, 1, wad_no,
               "1 to toggle between SG/SSG and Fist/Chainsaw"),

   // phares 2/25/98
   DEFAULT_INT("player_bobbing", &default_player_bobbing, &player_bobbing, 1, 0, 1, wad_yes,
               "1 to enable player bobbing (view moving up/down slightly)"),

   // killough 3/1/98
   DEFAULT_INT("monsters_remember", &default_monsters_remember, &monsters_remember,
               1, 0, 1, wad_yes,
               "1 to enable monsters remembering enemies after killing others"),

   // killough 7/19/98
   DEFAULT_INT("monster_infighting", &default_monster_infighting, &monster_infighting,
               1, 0, 1, wad_yes,
               "1 to enable monsters fighting against each other when provoked"),

   // killough 9/8/98
   DEFAULT_INT("monster_backing", &default_monster_backing, &monster_backing, 
               0, 0, 1, wad_yes, "1 to enable monsters backing away from targets"),

   //killough 9/9/98:
   DEFAULT_INT("monster_avoid_hazards", &default_monster_avoid_hazards, &monster_avoid_hazards,
               1, 0, 1, wad_yes, "1 to enable monsters to intelligently avoid hazards"),
   
   DEFAULT_INT("monkeys", &default_monkeys, &monkeys, 0, 0, 1, wad_yes,
               "1 to enable monsters to move up/down steep stairs"),
   
   //killough 9/9/98:
   DEFAULT_INT("monster_friction", &default_monster_friction, &monster_friction,
               1, 0, 1, wad_yes, "1 to enable monsters to be affected by friction"),
   
   //killough 9/9/98:
   DEFAULT_INT("help_friends", &default_help_friends, &help_friends, 1, 0, 1, wad_yes,
               "1 to enable monsters to help dying friends"),
   
   // killough 7/19/98
   DEFAULT_INT("player_helpers", &default_dogs, &dogs, 0, 0, 3, wad_yes,
               "number of single-player helpers"),
   
   // CONFIG_FIXME: 999?
   // killough 8/8/98
   DEFAULT_INT("friend_distance", &default_distfriend, &distfriend, 128, 0, 999, wad_yes,
               "distance friends stay away"),
   
   // killough 10/98
   DEFAULT_INT("dog_jumping", &default_dog_jumping, &dog_jumping, 1, 0, 1, wad_yes,
               "1 to enable dogs to jump"),

   DEFAULT_INT("p_markunknowns", &markUnknowns, NULL, 1, 0, 1, wad_no,
               "1 to mark unknown thingtype locations"),
   
   // no color changes on status bar
   DEFAULT_INT("sts_always_red", &sts_always_red, NULL, 1, 0, 1, wad_yes,
               "1 to disable use of color on status bar"),
   
   DEFAULT_INT("sts_pct_always_gray", &sts_pct_always_gray, NULL, 0, 0, 1, wad_yes,
               "1 to make percent signs on status bar always gray"),
   
   // killough 2/28/98
   DEFAULT_INT("sts_traditional_keys", &sts_traditional_keys, NULL, 1, 0, 1, wad_yes,
               "1 to disable doubled card and skull key display on status bar"),
   
   // haleyjd 05/16/04: restored (see mn_menus.c); changed def. to 0
   // killough 4/17/98
   DEFAULT_INT("traditional_menu", &traditional_menu, NULL, 0, 0, 1, wad_yes,
               "1 to emulate DOOM's main menu"),

   // killough 3/6/98
   DEFAULT_INT("leds_always_off", &leds_always_off, NULL, 0, 0, 1, wad_no,
               "1 to keep keyboard LEDs turned off"),

   //jff 4/3/98 allow unlimited sensitivity
   DEFAULT_INT("mouse_sensitivity_horiz", &mouseSensitivity_horiz, NULL, 5, 0, UL, wad_no,
               "adjust horizontal (x) mouse sensitivity"),

   //jff 4/3/98 allow unlimited sensitivity
   DEFAULT_INT("mouse_sensitivity_vert", &mouseSensitivity_vert, NULL, 5, 0, UL, wad_no,
               "adjust vertical (y) mouse sensitivity"),

   // SoM
   DEFAULT_INT("mouse_accel", &mouseAccel_type, NULL, 0, 0, 2, wad_no,
               "0 for no mouse accel, 1 for linear, 2 for choco-doom"),

   // haleyjd 10/24/08
   DEFAULT_INT("mouse_novert", &novert, NULL, 0, 0, 1, wad_no,
               "0 for normal mouse, 1 for no vertical movement"),

   DEFAULT_INT("sfx_volume", &snd_SfxVolume, NULL, 8, 0, 15, wad_no,
               "adjust sound effects volume"),

   DEFAULT_INT("music_volume", &snd_MusicVolume, NULL, 8, 0, 15, wad_no,
               "adjust music volume"),

   DEFAULT_INT("show_messages", &showMessages, NULL, 1, 0, 1, wad_no,
               "1 to enable message display"),

   DEFAULT_INT("mess_colour", &mess_colour, NULL, CR_RED, 0, CR_LIMIT-1, wad_no,
               "messages colour"),

   // killough 3/6/98: preserve autorun across games
   DEFAULT_INT("autorun", &autorun, NULL, 0, 0, 1, wad_no, "1 to enable autorun"),

   // haleyjd 08/23/09: allow shift to cancel autorun
   DEFAULT_INT("runiswalk", &runiswalk, NULL, 0, 0, 1, wad_no, 
               "1 to walk with shift when autorun is enabled"),

   // killough 2/21/98: default to 10
   // sf: removed screenblocks, screensize only now - changed values down 3
   DEFAULT_INT("screensize", &screenSize, NULL, 7, 0, 8, wad_no, 
               "initial play screen size"),

   //jff 3/6/98 fix erroneous upper limit in range
   DEFAULT_INT("usegamma", &usegamma, NULL, 0, 0, 4, wad_no,
               "screen brightness (gamma correction)"),

   // killough 10/98: preloaded files
   DEFAULT_STR("wadfile_1", &wad_files[0], NULL, "", wad_no,
               "WAD file preloaded at program startup"),

   DEFAULT_STR("wadfile_2", &wad_files[1], NULL, "", wad_no,
               "WAD file preloaded at program startup"),

   DEFAULT_STR("dehfile_1", &deh_files[0], NULL, "", wad_no,
               "DEH/BEX file preloaded at program startup"),

   DEFAULT_STR("dehfile_2", &deh_files[1], NULL, "", wad_no,
               "DEH/BEX file preloaded at program startup"),

   // haleyjd: auto-loaded console scripts
   DEFAULT_STR("cscript_1", &csc_files[0], NULL, "", wad_no,
               "Console script executed at program startup"),
  
   DEFAULT_STR("cscript_2", &csc_files[1], NULL, "", wad_no,
               "Console script executed at program startup"),
  
   DEFAULT_INT("use_startmap", &use_startmap, NULL, -1, -1, 1, wad_yes,
               "use start map instead of menu"),

   // killough 10/98: compatibility vector:

   DEFAULT_INT("comp_zombie", &default_comp[comp_zombie], &comp[comp_zombie], 
               0, 0, 1, wad_yes, "Zombie players can exit levels"),

   DEFAULT_INT("comp_infcheat", &default_comp[comp_infcheat], &comp[comp_infcheat],
               0, 0, 1, wad_yes, "Powerup cheats are not infinite duration"),

   DEFAULT_INT("comp_stairs", &default_comp[comp_stairs], &comp[comp_stairs],
               1, 0, 1, wad_yes, "Build stairs exactly the same way that Doom does"),

   DEFAULT_INT("comp_telefrag", &default_comp[comp_telefrag], &comp[comp_telefrag],
               0, 0, 1, wad_yes, "Monsters can telefrag on MAP30"),

   DEFAULT_INT("comp_dropoff", &default_comp[comp_dropoff], &comp[comp_dropoff],
               0, 0, 1, wad_yes, "Some objects never move over tall ledges"),

   DEFAULT_INT("comp_falloff", &default_comp[comp_falloff], &comp[comp_falloff],
               0, 0, 1, wad_yes, "Objects don't fall off ledges under their own weight"),

   DEFAULT_INT("comp_staylift", &default_comp[comp_staylift], &comp[comp_staylift],
               0, 0, 1, wad_yes, "Monsters randomly walk off of moving lifts"),

   DEFAULT_INT("comp_doorstuck", &default_comp[comp_doorstuck], &comp[comp_doorstuck],
               0, 0, 1, wad_yes, "Monsters get stuck on doortracks"),

   DEFAULT_INT("comp_pursuit", &default_comp[comp_pursuit], &comp[comp_pursuit],
               0, 0, 1, wad_yes, "Monsters don't give up pursuit of targets"),

   DEFAULT_INT("comp_vile", &default_comp[comp_vile], &comp[comp_vile],
               0, 0, 1, wad_yes, "Arch-Vile resurrects invincible ghosts"),

   DEFAULT_INT("comp_pain", &default_comp[comp_pain], &comp[comp_pain],
               0, 0, 1, wad_yes, "Pain Elemental limited to 20 lost souls"),

   DEFAULT_INT("comp_skull", &default_comp[comp_skull], &comp[comp_skull],
               0, 0, 1, wad_yes, "Lost souls get stuck behind walls"),

   DEFAULT_INT("comp_blazing", &default_comp[comp_blazing], &comp[comp_blazing],
               0, 0, 1, wad_yes, "Blazing doors make double closing sounds"),

   DEFAULT_INT("comp_doorlight", &default_comp[comp_doorlight], &comp[comp_doorlight],
               0, 0, 1, wad_yes, "Tagged doors don't trigger special lighting"),

   DEFAULT_INT("comp_god", &default_comp[comp_god], &comp[comp_god],
               0, 0, 1, wad_yes, "God mode isn't absolute"),

   DEFAULT_INT("comp_skymap", &default_comp[comp_skymap], &comp[comp_skymap],
               0, 0, 1, wad_yes, "Sky is unaffected by invulnerability"),

   DEFAULT_INT("comp_floors", &default_comp[comp_floors], &comp[comp_floors],
               0, 0, 1, wad_yes, "Use exactly Doom's floor motion behavior"),

   DEFAULT_INT("comp_model", &default_comp[comp_model], &comp[comp_model],
               0, 0, 1, wad_yes, "Use exactly Doom's linedef trigger model"),

   DEFAULT_INT("comp_zerotags", &default_comp[comp_zerotags], &comp[comp_zerotags],
               0, 0, 1, wad_yes, "Linedef effects work with sector tag = 0"),

   // haleyjd
   DEFAULT_INT("comp_terrain", &default_comp[comp_terrain], &comp[comp_terrain], 
               1, 0, 1, wad_yes, "Terrain effects not activated on floor contact"),
   
   // haleyjd
   DEFAULT_INT("comp_respawnfix", &default_comp[comp_respawnfix], &comp[comp_respawnfix],
               0, 0, 1, wad_yes, "Creatures with no spawnpoint respawn at (0,0)"),
   
   // haleyjd
   DEFAULT_INT("comp_fallingdmg", &default_comp[comp_fallingdmg], &comp[comp_fallingdmg],
               1, 0, 1, wad_yes, "Players do not take falling damage"),
   
   // haleyjd
   DEFAULT_INT("comp_soul", &default_comp[comp_soul], &comp[comp_soul],
               0, 0, 1, wad_yes, "Lost souls do not bounce on floors"),
   
   // haleyjd 02/15/02: z checks (includes,supercedes comp_scratch)
   DEFAULT_INT("comp_overunder", &default_comp[comp_overunder], &comp[comp_overunder],
               1, 0, 1, wad_yes, "Things not fully clipped with respect to z coord"),
   
   DEFAULT_INT("comp_theights", &default_comp[comp_theights], &comp[comp_theights],
               1, 0, 1, wad_yes, "DOOM thingtypes use inaccurate height information"),
   
   DEFAULT_INT("comp_planeshoot", &default_comp[comp_planeshoot], &comp[comp_planeshoot],
               1, 0, 1, wad_yes, "Tracer shots cannot hit the floor or ceiling"),

   DEFAULT_INT("comp_special", &default_comp[comp_special], &comp[comp_special],
               0, 0, 1, wad_yes, "One-time line specials are cleared on failure"),

   DEFAULT_INT("comp_ninja", &default_comp[comp_ninja], &comp[comp_ninja],
               0, 0, 1, wad_yes, "Silent spawns at W/SW/S-facing DM spots"),
   

   // For key bindings, the values stored in the key_* variables       // phares
   // are the internal Doom Codes. The values stored in the default.cfg
   // file are the keyboard codes. I_ScanCode2DoomCode converts from
   // keyboard codes to Doom Codes. I_DoomCode2ScanCode converts from
   // Doom Codes to keyboard codes, and is only used when writing back
   // to default.cfg. For the printable keys (i.e. alphas, numbers)
   // the Doom Code is the ascii code.

   // haleyjd: This now only sets keys which are not dynamically
   // rebindable for various reasons. To not allow rebinding at all
   // would be bad, but to do it in real time would be hard. This is
   // a compromise.

   // TODO/FIXME: make ALL keys dynamically rebindable

   DEFAULT_INT("key_spy", &key_spy, NULL, KEYD_F12, 0, 255, wad_no,
               "key to view from another player's vantage"),
   
   DEFAULT_INT("key_pause", &key_pause, NULL, KEYD_PAUSE, 0, 255, wad_no,
               "key to pause the game"),
   
   DEFAULT_INT("key_chat", &key_chat, NULL, 't', 0, 255, wad_no,
               "key to enter a chat message"),
   
   DEFAULT_INT("key_chatplayer1", &destination_keys[0], NULL, 'g', 0, 255, wad_no,
               "key to chat with player 1"),
   
   // killough 11/98: fix 'i'/'b' reversal
   DEFAULT_INT("key_chatplayer2", &destination_keys[1], NULL, 'i', 0, 255, wad_no,
               "key to chat with player 2"),
   
   // killough 11/98: fix 'i'/'b' reversal
   DEFAULT_INT("key_chatplayer3", &destination_keys[2], NULL, 'b', 0, 255, wad_no,
               "key to chat with player 3"),
   
   DEFAULT_INT("key_chatplayer4", &destination_keys[3], NULL, 'r', 0, 255, wad_no,
               "key to chat with player 4"),
   
   DEFAULT_INT("automlook", &automlook, NULL, 0, 0, 1, wad_no, 
               "set to 1 to always mouselook"),
   
   DEFAULT_INT("invert_mouse", &invert_mouse, NULL, 1, 0, 1, wad_no,
               "set to 1 to invert mouse during mouselooking"),
      
   DEFAULT_INT("use_mouse", &usemouse, NULL, 1, 0, 1, wad_no,
               "1 to enable use of mouse"),
   
   //jff 3/8/98 end of lower range change for -1 allowed in mouse binding
   // haleyjd: rename these buttons on the user-side to prevent confusion
   DEFAULT_INT("mouseb_dblc1", &mousebstrafe, NULL, 1, -1, 2, wad_no,
               "1st mouse button to enable for double-click use action (-1 = disable)"),
   
   DEFAULT_INT("mouseb_dblc2", &mousebforward, NULL, 2, -1, 2, wad_no,
               "2nd mouse button to enable for double-click use action (-1 = disable)"),
   
   DEFAULT_INT("use_joystick", &usejoystick, NULL, 0, 0, 1, wad_no,
               "1 to enable use of joystick"),
      
   DEFAULT_STR("chatmacro0", &chat_macros[0], NULL, HUSTR_CHATMACRO0, wad_yes,
               "chat string associated with 0 key"),
   
   DEFAULT_STR("chatmacro1", &chat_macros[1], NULL, HUSTR_CHATMACRO1, wad_yes,
               "chat string associated with 1 key"),
   
   DEFAULT_STR("chatmacro2", &chat_macros[2], NULL, HUSTR_CHATMACRO2, wad_yes,
               "chat string associated with 2 key"),   

   DEFAULT_STR("chatmacro3", &chat_macros[3], NULL, HUSTR_CHATMACRO3, wad_yes,
               "chat string associated with 3 key"),
   
   DEFAULT_STR("chatmacro4", &chat_macros[4], NULL, HUSTR_CHATMACRO4, wad_yes,
               "chat string associated with 4 key"),
   
   DEFAULT_STR("chatmacro5", &chat_macros[5], NULL, HUSTR_CHATMACRO5, wad_yes,
               "chat string associated with 5 key"),

   DEFAULT_STR("chatmacro6", &chat_macros[6], NULL, HUSTR_CHATMACRO6, wad_yes,
               "chat string associated with 6 key"),
   
   DEFAULT_STR("chatmacro7", &chat_macros[7], NULL, HUSTR_CHATMACRO7, wad_yes,
               "chat string associated with 7 key"),
   
   DEFAULT_STR("chatmacro8", &chat_macros[8], NULL, HUSTR_CHATMACRO8, wad_yes,
               "chat string associated with 8 key"),
   
   DEFAULT_STR("chatmacro9", &chat_macros[9], NULL, HUSTR_CHATMACRO9, wad_yes,
               "chat string associated with 9 key"),
   
   //jff 1/7/98 defaults for automap colors
   //jff 4/3/98 remove -1 in lower range, 0 now disables new map features
   // black //jff 4/6/98 new black
   DEFAULT_INT("mapcolor_back", &mapcolor_back, NULL, 247, 0, 255, wad_yes,
               "color used as background for automap"),
   
   // dk gray
   DEFAULT_INT("mapcolor_grid", &mapcolor_grid, NULL, 104, 0, 255, wad_yes,
               "color used for automap grid lines"),
   
   // red-brown
   DEFAULT_INT("mapcolor_wall", &mapcolor_wall, NULL, 181, 0, 255, wad_yes,
               "color used for one side walls on automap"),
   
   // lt brown
   DEFAULT_INT("mapcolor_fchg", &mapcolor_fchg, NULL, 166, 0, 255, wad_yes,
               "color used for lines floor height changes across"),
   
   // orange
   DEFAULT_INT("mapcolor_cchg", &mapcolor_cchg, NULL, 231, 0, 255, wad_yes,
               "color used for lines ceiling height changes across"),
   
   // white
   DEFAULT_INT("mapcolor_clsd", &mapcolor_clsd, NULL, 231, 0, 255, wad_yes,
               "color used for lines denoting closed doors, objects"),
   
   // red
   DEFAULT_INT("mapcolor_rkey",&mapcolor_rkey, NULL, 175, 0, 255, wad_yes,
               "color used for red key sprites"),
   
   // blue
   DEFAULT_INT("mapcolor_bkey",&mapcolor_bkey, NULL, 204, 0, 255, wad_yes,
               "color used for blue key sprites"),
   
   // yellow
   DEFAULT_INT("mapcolor_ykey",&mapcolor_ykey, NULL, 231, 0, 255, wad_yes,
               "color used for yellow key sprites"),
   
   // red
   DEFAULT_INT("mapcolor_rdor",&mapcolor_rdor, NULL, 175, 0, 255, wad_yes,
               "color used for closed red doors"),
   
   // blue
   DEFAULT_INT("mapcolor_bdor",&mapcolor_bdor, NULL, 204, 0, 255, wad_yes,
               "color used for closed blue doors"),
   
   // yellow
   DEFAULT_INT("mapcolor_ydor",&mapcolor_ydor, NULL, 231, 0, 255, wad_yes,
               "color used for closed yellow doors"),
   
   // dk green
   DEFAULT_INT("mapcolor_tele",&mapcolor_tele, NULL, 119, 0, 255, wad_yes,
               "color used for teleporter lines"),
   
   // purple
   DEFAULT_INT("mapcolor_secr",&mapcolor_secr, NULL, 176, 0, 255, wad_yes,
               "color used for lines around secret sectors"),
   
   // none
   DEFAULT_INT("mapcolor_exit",&mapcolor_exit, NULL, 0, 0, 255, wad_yes,
               "color used for exit lines"),

   // dk gray
   DEFAULT_INT("mapcolor_unsn",&mapcolor_unsn, NULL, 96, 0, 255, wad_yes,
               "color used for lines not seen without computer map"),
   
   // lt gray
   DEFAULT_INT("mapcolor_flat",&mapcolor_flat, NULL, 88, 0, 255, wad_yes,
               "color used for lines with no height changes"),
   
   // green
   DEFAULT_INT("mapcolor_sprt",&mapcolor_sprt, NULL, 112, 0, 255, wad_yes,
               "color used as things"),
   
   // white
   DEFAULT_INT("mapcolor_hair",&mapcolor_hair, NULL, 208, 0, 255, wad_yes,
               "color used for dot crosshair denoting center of map"),
   
   // white
   DEFAULT_INT("mapcolor_sngl",&mapcolor_sngl, NULL, 208, 0, 255, wad_yes,
               "color used for the single player arrow"),
   
   // green
   DEFAULT_INT("mapcolor_ply1",&mapcolor_plyr[0], NULL, 112, 0, 255, wad_yes,
               "color used for the green player arrow"),
   
   // lt gray
   DEFAULT_INT("mapcolor_ply2",&mapcolor_plyr[1], NULL, 88, 0, 255, wad_yes,
               "color used for the gray player arrow"),
   
   // brown
   DEFAULT_INT("mapcolor_ply3",&mapcolor_plyr[2], NULL, 64, 0, 255, wad_yes,
               "color used for the brown player arrow"),
   
   // red
   DEFAULT_INT("mapcolor_ply4",&mapcolor_plyr[3], NULL, 176, 0, 255, wad_yes,
               "color used for the red player arrow"),
   
   // purple                       // killough 8/8/98
   DEFAULT_INT("mapcolor_frnd",&mapcolor_frnd, NULL, 252, 0, 255, wad_yes,
               "color used for friends"),
   

#ifdef R_LINKEDPORTALS
   DEFAULT_INT("mapcolor_prtl",&mapcolor_prtl, NULL, 109, 0, 255, wad_yes,
               "color for lines not in the player's portal area"),
   
   DEFAULT_INT("mapportal_overlay",&mapportal_overlay, NULL, 1, 0, 1, wad_yes,
               "1 to overlay different linked portal areas in the automap"),
            
#endif

   DEFAULT_INT("map_point_coord", &map_point_coordinates, NULL, 1, 0, 1, wad_yes,
               "1 to show automap pointer coordinates in non-follow mode"),
   
   //jff 3/9/98 add option to not show secrets til after found
   // killough change default, to avoid spoilers and preserve Doom mystery
   // show secret after gotten
   DEFAULT_INT("map_secret_after",&map_secret_after, NULL, 1, 0, 1, wad_yes,
               "1 to not show secret sectors till after entered"),
   
   //jff 1/7/98 end additions for automap

   //jff 2/16/98 defaults for color ranges in hud and status

   // 1 line scrolling window
   DEFAULT_INT("hud_msg_lines",&hud_msg_lines, NULL, 1, 1, 16, wad_yes,
               "number of lines in review display"),
   
   // killough 11/98
   DEFAULT_INT("hud_msg_scrollup",&hud_msg_scrollup, NULL, 1, 0, 1, wad_yes,
               "1 enables message review list scrolling upward"),
   
   // killough 11/98
   DEFAULT_INT("message_timer",&message_timer, NULL, 4000, 0, UL, wad_no,
               "Duration of normal Doom messages (ms)"),
   
   //sf : fullscreen hud style
   DEFAULT_INT("hud_overlaystyle",&hud_overlaystyle, NULL, 1, 0, 4, wad_yes,
               "fullscreen hud style"),

   DEFAULT_INT("hud_enabled",&hud_enabled, NULL, 1, 0, 1, wad_yes,
               "fullscreen hud enabled"),
   
   DEFAULT_INT("hud_hidestatus",&hud_hidestatus, NULL, 0, 0, 1, wad_yes,
               "hide kills/items/secrets info on fullscreen hud"),
   
   DEFAULT_BOOL("hu_showtime", &hu_showtime, NULL, true, wad_yes,
                "display current level time on automap"),
   
   DEFAULT_BOOL("hu_showcoords", &hu_showcoords, NULL, true, wad_yes,
                "display player/pointer coordinates on automap"),
   
   DEFAULT_INT("hu_timecolor",&hu_timecolor, NULL, CR_RED, 0, CR_LIMIT-1, wad_yes,
               "color of automap level time widget"),

   DEFAULT_INT("hu_levelnamecolor",&hu_levelnamecolor, NULL, CR_RED, 0, CR_LIMIT-1, wad_yes,
               "color of automap level name widget"),
   
   DEFAULT_INT("hu_coordscolor",&hu_coordscolor, NULL, CR_RED, 0, CR_LIMIT-1, wad_yes,
               "color of automap coordinates widget"),
   
   // below is red
   DEFAULT_INT("health_red",&health_red, NULL, 25, 0, 200, wad_yes,
               "amount of health for red to yellow transition"),

   // below is yellow
   DEFAULT_INT("health_yellow", &health_yellow, NULL, 50, 0, 200, wad_yes,
               "amount of health for yellow to green transition"),
   
   // below is green, above blue
   DEFAULT_INT("health_green",&health_green, NULL, 100, 0, 200, wad_yes,
               "amount of health for green to blue transition"),
   
   // below is red
   DEFAULT_INT("armor_red",&armor_red, NULL, 25, 0, 200, wad_yes,
               "amount of armor for red to yellow transition"),
   
   // below is yellow
   DEFAULT_INT("armor_yellow",&armor_yellow, NULL, 50, 0, 200, wad_yes,
               "amount of armor for yellow to green transition"),
   
   // below is green, above blue
   DEFAULT_INT("armor_green",&armor_green, NULL, 100, 0, 200, wad_yes,
               "amount of armor for green to blue transition"),
   
   // below 25% is red
   DEFAULT_INT("ammo_red",&ammo_red, NULL, 25, 0, 100, wad_yes,
               "percent of ammo for red to yellow transition"),
   
   // below 50% is yellow, above green
   DEFAULT_INT("ammo_yellow",&ammo_yellow, NULL, 50, 0, 100, wad_yes,
               "percent of ammo for yellow to green transition"),

   DEFAULT_INT("st_fsalpha", &st_fsalpha, NULL, 100, 0, 100, wad_yes,
               "fullscreen HUD translucency level"),
   
   // killough 2/8/98: weapon preferences set by user:
   DEFAULT_INT("weapon_choice_1",&weapon_preferences[0][0], NULL, 6, 1, 9, wad_yes,
               "first choice for weapon (best)"),
   
   DEFAULT_INT("weapon_choice_2",&weapon_preferences[0][1], NULL, 9, 1, 9, wad_yes,
               "second choice for weapon"),
   
   DEFAULT_INT("weapon_choice_3",&weapon_preferences[0][2], NULL, 4, 1, 9, wad_yes,
               "third choice for weapon"),
   
   DEFAULT_INT("weapon_choice_4",&weapon_preferences[0][3], NULL, 3, 1, 9, wad_yes,
               "fourth choice for weapon"),
   
   DEFAULT_INT("weapon_choice_5",&weapon_preferences[0][4], NULL, 2, 1, 9, wad_yes,
               "fifth choice for weapon"),
   
   DEFAULT_INT("weapon_choice_6",&weapon_preferences[0][5], NULL, 8, 1, 9, wad_yes,
               "sixth choice for weapon"),
   
   DEFAULT_INT("weapon_choice_7",&weapon_preferences[0][6], NULL, 5, 1, 9, wad_yes,
               "seventh choice for weapon"),
   
   DEFAULT_INT("weapon_choice_8",&weapon_preferences[0][7], NULL, 7, 1, 9, wad_yes,
               "eighth choice for weapon"),
   
   DEFAULT_INT("weapon_choice_9",&weapon_preferences[0][8], NULL, 1, 1, 9, wad_yes,
               "ninth choice for weapon (worst)"),
   
   DEFAULT_INT("c_speed",&c_speed, NULL, 10, 1, 200, wad_no,
               "console speed, pixels/tic"),
   
   DEFAULT_INT("c_height",&c_height, NULL, 100, 0, 200, wad_no,
               "console height, pixels"),
   
   DEFAULT_INT("obituaries",&obituaries, NULL, 0, 0, 1, wad_yes,
               "obituaries on/off"),
   
   DEFAULT_INT("obcolour",&obcolour, NULL, 0, 0, CR_LIMIT-1, wad_no,
               "obituaries colour"),
   
   DEFAULT_INT("draw_particles",&drawparticles, NULL, 0, 0, 1, wad_yes,
               "toggle particle effects on or off"),
   
   DEFAULT_INT("particle_trans",&particle_trans, NULL, 1, 0, 2, wad_yes,
               "particle translucency (0 = none, 1 = smooth, 2 = general)"),
   
   DEFAULT_INT("blood_particles",&bloodsplat_particle, NULL, 0, 0, 2, wad_yes,
               "use sprites, particles, or both for blood (sprites = 0)"),
   
   DEFAULT_INT("bullet_particles",&bulletpuff_particle, NULL, 0, 0, 2, wad_yes,
               "use sprites, particles, or both for bullet puffs (sprites = 0)"),
   
   DEFAULT_INT("rocket_trails",&drawrockettrails, NULL, 0, 0, 1, wad_yes,
               "draw particle rocket trails"),

   DEFAULT_INT("grenade_trails",&drawgrenadetrails, NULL, 0, 0, 1, wad_yes,
               "draw particle grenade trails"),
   
   DEFAULT_INT("bfg_cloud",&drawbfgcloud, NULL, 0, 0, 1, wad_yes,
               "draw particle bfg cloud"),
   
   DEFAULT_INT("pevent_rexpl",&(particleEvents[P_EVENT_ROCKET_EXPLODE].enabled), NULL,
               0, 0, 1, wad_yes, "draw particle rocket explosions"),
   
   DEFAULT_INT("pevent_bfgexpl",&(particleEvents[P_EVENT_BFG_EXPLODE].enabled), NULL,
               0, 0, 1, wad_yes, "draw particle bfg explosions"),

   DEFAULT_INT("stretchsky", &stretchsky, NULL, 0, 0, 1, wad_yes,
               "stretch short sky textures for mlook"),
   
   DEFAULT_INT("startnewmap", &startOnNewMap, NULL, 0, 0, 1, wad_yes,
               "start game on first new map (DOOM II only)"),
   

#ifdef _SDL_VER   
   DEFAULT_INT("showendoom", &showendoom, NULL, 1, 0, 1, wad_yes,
               "1 to show ENDOOM at exit"),

   DEFAULT_INT("endoomdelay", &endoomdelay, NULL, 350, 35, 3500, wad_no,
               "Amount of time to display ENDOOM when shown"),
#endif

   DEFAULT_INT("autoaim", &default_autoaim, &autoaim, 1, 0, 1, wad_yes,
               "1 to enable autoaiming"),
   
   DEFAULT_INT("chasecam_height", &chasecam_height, NULL, 15, -31, 100, wad_no,
               "preferred height of chasecam above/below player viewheight"),
   
   DEFAULT_INT("chasecam_speed", &chasecam_speed, NULL, 33, 1, 100, wad_no,
               "percentage of distance to target chasecam moves per gametic"),
   
   DEFAULT_INT("chasecam_dist", &chasecam_dist, NULL, 112, 10, 1024, wad_no,
               "preferred distance from chasecam to player"),
   
   DEFAULT_INT("allowmlook", &default_allowmlook, &allowmlook, 0, 0, 1, wad_yes,
               "1 to allow players to look up/down"),
   
   DEFAULT_BOOL("menu_toggleisback", &menu_toggleisback, NULL, false, wad_no,
                "1 to make menu toggle action back up one level (like zdoom)"),
   
   DEFAULT_INT("mn_classic_menus", &mn_classic_menus, NULL, 0, 0, 1, wad_yes,
               "1 to enable use of full classic menu emulation"),

   DEFAULT_STR("mn_background", &mn_background, NULL, "default", wad_yes,
               "menu background"),
   
   DEFAULT_STR("wad_directory", &wad_directory, NULL, ".", wad_no,
               "user's default wad directory"),
   
   DEFAULT_INT("r_columnengine",&r_column_engine_num, NULL, 
               1, 0, NUMCOLUMNENGINES - 1, wad_no, 
               "0 = normal, 1 = optimized quad cache"),
   
   DEFAULT_INT("r_spanengine",&r_span_engine_num, NULL,
               0, 0, NUMSPANENGINES - 1, wad_no, 
               "0 = high precision, 1 = low precision"),
   
   DEFAULT_INT("r_detail", &c_detailshift, NULL, 0, 0, 1, wad_no,
               "0 = high detail, 1 = low detail"),
   
   DEFAULT_INT("r_vissprite_limit", &r_vissprite_limit, NULL, -1, -1, UL, wad_yes,
               "number of vissprites allowed per frame (-1 = no limit)"),

   DEFAULT_INT("r_tlstyle", &r_tlstyle, NULL, 1, 0, R_TLSTYLE_NUM - 1, wad_yes,
               "Doom object translucency style (0 = none, 1 = Boom, 2 = new)"),
   
   DEFAULT_INT("spechits_emulation", &spechits_emulation, NULL, 0, 0, 2, wad_no,
               "0 = off, 1 = emulate like Chocolate Doom, 2 = emulate like PrBoom+"),

   DEFAULT_BOOL("donut_emulation", &donut_emulation, NULL, false, wad_no,
                "emulate undefined EV_DoDonut behavior"),
   
   DEFAULT_INT("wipewait",&wipewait, NULL, 2, 0, 2, wad_no,
               "0 = never wait on screen wipes, 1 = always wait, 2 = wait when playing demos"),
   
   DEFAULT_INT("wipetype",&wipetype, NULL, 1, 0, 2, wad_yes,
               "0 = none, 1 = melt, 2 = fade"),
   
#ifdef HAVE_SPCLIB
   DEFAULT_INT("snd_spcpreamp", &spc_preamp, NULL, 1, 1, 6, wad_yes,
               "preamp volume factor for SPC music"),
   
   DEFAULT_INT("snd_spcbassboost", &spc_bass_boost, NULL, 8, 1, 31, wad_yes,
               "bass boost for SPC music (logarithmic scale, 8 = normal)"),
   
#endif

   { NULL }         // last entry
};

//
// haleyjd 06/29/09: Default Overrides
//
// The following tables are mapped through GameModeInfo and are used to 
// override selected defaults in the primary defaults array above by replacing
// the default values specified there. This is the cleanest way to provide
// gamemode-dependent default values for only some of the options that I can
// think up.
//

default_or_t HereticDefaultORs[] =
{
   // misc
   { "disk_icon",        0 }, // no disk icon (makes consistent)
   { "pitched_sounds",   1 }, // pitched sounds should be on
   { "allowmlook",       1 }, // mlook defaults to on
   { "wipetype",         2 }, // use crossfade wipe by default
   { "hud_overlaystyle", 4 }, // use graphical HUD style
   
   // compatibility
   { "comp_terrain",   0 }, // terrain active
   { "comp_soul",      1 }, // SKULLFLY objects do not bounce
   { "comp_overunder", 0 }, // 3D object clipping is on
   
   // colors
   // TODO: player color
   // TODO: additional automap colors
   { "mapcolor_back",     103     },
   { "mapcolor_grid",     40      },
   { "mapcolor_wall",     96      },
   { "mapcolor_fchg",     110     },
   { "mapcolor_cchg",     75      },
   { "mapcolor_tele",     96      },
   { "mapcolor_secr",     0       },
   { "mapcolor_exit",     0       },
   { "mapcolor_unsn",     40      },
   { "mapcolor_flat",     43      },
   { "mapcolor_prtl",     43      },
   { "hu_timecolor",      CR_GRAY },
   { "hu_levelnamecolor", CR_GRAY },
   { "hu_coordscolor",    CR_GRAY },
   { "mess_colour",       CR_GRAY },

   // this must be last
   { NULL }
};

// haleyjd 03/14/09: main defaultfile object
static defaultfile_t maindefaults =
{
   defaults,
   sizeof defaults / sizeof *defaults - 1,
};

// killough 11/98: hash function for name lookup
static unsigned default_hash(defaultfile_t *df, const char *name)
{
  unsigned hash = 0;
  while (*name)
    hash = hash*2 + toupper(*name++);
  return hash % df->numdefaults;
}

//
// M_LookupDefault
//
// Hashes/looks up defaults in the given defaultfile object by name.
// Returns the default_t object, or NULL if not found.
//
static default_t *M_LookupDefault(defaultfile_t *df, const char *name)
{
   register default_t *dp;

   // Initialize hash table if not initialized already
   if(!df->hashInit)
   {
      for(df->hashInit = true, dp = df->defaults; dp->name; dp++)
      {
         unsigned h = default_hash(df, dp->name);
         dp->next = df->defaults[h].first;
         df->defaults[h].first = dp;
      }
   }

   // Look up name in hash table
   for(dp = df->defaults[default_hash(df, name)].first;
       dp && strcasecmp(name, dp->name); dp = dp->next);

   return dp;
}

//
// M_ApplyGameModeDefaults
//
// haleyjd 06/30/09: Overwrites defaults in the defaultfile with values
// specified by name in the current gamemode's default overrides array.
//
static void M_ApplyGameModeDefaults(defaultfile_t *df)
{
   default_or_t *ovr = GameModeInfo->defaultORs;

   // not all gamemodes have default overrides
   if(!ovr)
      return;

   while(ovr->name)
   {
      default_t *def = M_LookupDefault(df, ovr->name);

#ifdef RANGECHECK
      // FIXME: allow defaults for all types
      if(def->type != dt_integer)
      {
         I_Error("M_ApplyGameModeDefaults: override for non-integer default %s\n",
                 def->name);
      }
#endif
      // replace the default value
      if(def)
         def->defaultvalue_i = ovr->defaultvalue;

      ++ovr;
   }
}

//
// M_SaveDefaultFile
//
void M_SaveDefaultFile(defaultfile_t *df)
{
   char *tmpfile = NULL;
   size_t len;
   register default_t *dp;
   unsigned int line, blanks;
   FILE *f;

   // killough 10/98: for when exiting early
   if(!df->loaded || !df->fileName)
      return;

   len = M_StringAlloca(&tmpfile, 2, 14, D_DoomExeDir(), D_DoomExeName());

   psnprintf(tmpfile, len, "%s/tmp%.5s.cfg", D_DoomExeDir(), D_DoomExeName());
   M_NormalizeSlashes(tmpfile);

   errno = 0;
   if(!(f = fopen(tmpfile, "w")))  // killough 9/21/98
      goto error;

   // 3/3/98 explain format of file
   // killough 10/98: use executable's name

   if(config_help && !df->helpHeader &&
      fprintf(f,";%s.cfg format:\n"
              ";[min-max(default)] description of variable\n"
              ";* at end indicates variable is settable in wads\n"
              ";variable   value\n\n", D_DoomExeName()) == EOF)
      goto error;

   // killough 10/98: output comment lines which were read in during input

   for(blanks = 1, line = 0, dp = df->defaults; ; dp++, blanks = 0)
   {
      int brackets = 0;

      for(; line < df->numcomments && df->comments[line].line <= dp - df->defaults; ++line)
      {
         if(*(df->comments[line].text) != '[' || (brackets = 1, config_help))
         {
            // If we haven't seen any blank lines
            // yet, and this one isn't blank,
            // output a blank line for separation

            if((!blanks && (blanks = 1, 
                            *(df->comments[line].text) != '\n' &&
                            putc('\n',f) == EOF)) ||
               fputs(df->comments[line].text, f) == EOF)
               goto error;
         }
      }

      // If we still haven't seen any blanks,
      // Output a blank line for separation

      if(!blanks && putc('\n',f) == EOF)
         goto error;

      if(!dp->name)      // If we're at end of defaults table, exit loop
         break;

      //jff 3/3/98 output help string
      //
      // killough 10/98:
      // Don't output config help if any [ lines appeared before this one.
      // Make default values, and numeric range output, automatic.

      if(config_help && !brackets)
      {
         boolean printError = false;

         switch(dp->type)
         {
         case dt_string:
            printError = (fprintf(f, "[(\"%s\")]", dp->defaultvalue_s) == EOF);
            break;
         case dt_integer:
            if(dp->limit.min == UL)
            {
               if(dp->limit.max == UL)
                  printError = (fprintf(f, "[?-?(%d)]", dp->defaultvalue_i) == EOF);
               else 
               {
                  printError = 
                     (fprintf(f, "[?-%d(%d)]", dp->limit.max, dp->defaultvalue_i) == EOF);
               }
            }
            else if(dp->limit.max == UL)
            {
               printError = 
                  (fprintf(f, "[%d-?(%d)]", dp->limit.min, dp->defaultvalue_i) == EOF);
            }
            else
            {
               printError = (fprintf(f, "[%d-%d(%d)]", dp->limit.min, dp->limit.max,
                             dp->defaultvalue_i) == EOF);
            }
            break;
         case dt_float:
            if(dp->limit.min == UL)
            {
               if(dp->limit.max == UL)
                  printError = (fprintf(f, "[?-?](%g)]", dp->defaultvalue_f) == EOF);
               else
               {
                  printError =
                     (fprintf(f, "[?-%g(%g)]", (double)dp->limit.max / 100.0, 
                              dp->defaultvalue_f) == EOF);
               }
            }
            else if(dp->limit.max == UL)
            {
               printError = 
                  (fprintf(f, "[%g-?(%g)]", (double)dp->limit.min / 100.0,
                           dp->defaultvalue_f) == EOF);
            }
            else
            {
               printError = (fprintf(f, "[%g-%g(%g)]",
                             (double)dp->limit.min / 100.0,
                             (double)dp->limit.max / 100.0,
                             dp->defaultvalue_f) == EOF);
            }
            break;
         case dt_boolean:
            printError = (fprintf(f, "[0-1(%d)]", !!dp->defaultvalue_b) == EOF);
            break;
         default:
            break;
         }

         if(printError ||
            fprintf(f, " %s %s\n", dp->help, dp->wad_allowed ? "*" : "") == EOF)
            goto error;
      }

      // killough 11/98:
      // Write out original default if .wad file has modified the default

      //jff 4/10/98 kill super-hack on pointer value
      // killough 3/6/98:
      // use spaces instead of tabs for uniform justification

      switch(dp->type)
      {
      case dt_integer:
         {
            int value = 
               dp->modified ? dp->orig_default_i : *(int *)dp->location;

            if(fprintf(f, "%-25s %5i\n", dp->name,
                       strncmp(dp->name, "key_", 4) ? value : 
                       I_DoomCode2ScanCode(value)) == EOF)
               goto error;
         }
         break;
      case dt_string:
         {
            const char *value = 
               dp->modified ? dp->orig_default_s : *(const char **)dp->location;

            if(fprintf(f, "%-25s \"%s\"\n", dp->name, value) == EOF)
               goto error;
         }
         break;
      case dt_float:
         {
            double value = 
               dp->modified ? dp->orig_default_f : *(double *)dp->location;

            if(fprintf(f, "%-25s %#g\n", dp->name, value) == EOF)
               goto error;
         }
         break;
      case dt_boolean:
         {
            boolean value = 
               dp->modified ? dp->orig_default_b : *(boolean *)dp->location;

            if(fprintf(f, "%-25s %5i\n", dp->name, !!value) == EOF)
               goto error;
         }
         break;
      default:
         break;
      }
   }

   if(fclose(f) == EOF)
   {
   error:
      I_Error("Could not write defaults to %s: %s\n%s left unchanged\n",
              tmpfile, errno ? strerror(errno) : "(Unknown Error)", df->fileName);
      return;
   }

   remove(df->fileName);

   if(rename(tmpfile, df->fileName))
      I_Error("Could not write defaults to %s: %s\n", df->fileName,
              errno ? strerror(errno) : "(Unknown Error)");
}

//
// M_SaveDefaults
//
// haleyjd 3/14/09: This is now only to write the primary config file.
//
void M_SaveDefaults(void)
{
   M_SaveDefaultFile(&maindefaults);
}

//
// M_ParseOption()
//
// killough 11/98:
//
// This function parses .cfg file lines, or lines in OPTIONS lumps
//
boolean M_ParseOption(defaultfile_t *df, const char *p, boolean wad)
{
   char name[80], strparm[100];
   default_t *dp;
   int parm;
   double tmp;
   
   while(isspace(*p))  // killough 10/98: skip leading whitespace
      p++;

   //jff 3/3/98 skip lines not starting with an alphanum
   // killough 10/98: move to be made part of main test, add comment-handling

   if(sscanf(p, "%79s %99[^\n]", name, strparm) != 2 || !isalnum(*name) ||
      !(dp = M_LookupDefault(df, name)) || 
      (*strparm == '"') == (dp->type != dt_string) ||
      (wad && !dp->wad_allowed))
   {
      return 1;
   }

   switch(dp->type)
   {
   case dt_integer:
      {
         if(sscanf(strparm, "%i", &parm) != 1)
            return 1;                       // Not A Number
         
         if(!strncmp(name, "key_", 4))    // killough
            parm = I_ScanCode2DoomCode(parm);
         
         //jff 3/4/98 range check numeric parameters
         if((dp->limit.min == UL || dp->limit.min <= parm) &&
            (dp->limit.max == UL || dp->limit.max >= parm))
         {
            if(wad)
            {
               if(!dp->modified) // First time it's modified by wad
               {
                  dp->modified = 1;                           // Mark it as modified
                  dp->orig_default_i = *(int *)dp->location;  // Save original default
               }
               if(dp->current)            // Change current value
                  *(int *)dp->current = parm;
            }
            *(int *)dp->location = parm;  // Change default
         }
      }
      break;
   case dt_string:
      {
         int len = strlen(strparm) - 1;

         while(isspace(strparm[len]))
            len--;

         if(strparm[len] == '"')
            len--;

         strparm[len+1] = 0;

         if(wad && !dp->modified)                        // Modified by wad
         {                                               // First time modified
            dp->modified = 1;                            // Mark it as modified
            dp->orig_default_s = *(char **)dp->location; // Save original default
         }
         else
            free(*(char **)dp->location);               // Free old value

         *(char **)dp->location = strdup(strparm+1);    // Change default value

         if(dp->current)                                // Current value
         {
            free(*(char **)dp->current);                // Free old value
            *(char **)dp->current = strdup(strparm+1);  // Change current value
         }
      }
      break;
   case dt_float:
      {
         if(sscanf(strparm, "%lg", &tmp) != 1)
            return 1;                       // Not A Number
                  
         //jff 3/4/98 range check numeric parameters
         if((dp->limit.min == UL || (double)dp->limit.min / 100.0 <= tmp) &&
            (dp->limit.max == UL || (double)dp->limit.max / 100.0 >= tmp))
         {
            if(wad)
            {
               if(!dp->modified) // First time it's modified by wad
               {
                  dp->modified = 1;                             // Mark it as modified
                  dp->orig_default_f = *(double *)dp->location; // Save original default
               }
               if(dp->current)              // Change current value
                  *(double *)dp->current = tmp;
            }
            *(double *)dp->location = tmp;  // Change default
         }
      }
      break;
   case dt_boolean:
      {
         if(sscanf(strparm, "%i", &parm) != 1)
            return 1;                       // Not A Number
                  
         //jff 3/4/98 range check numeric parameters
         if((dp->limit.min == UL || dp->limit.min <= parm) &&
            (dp->limit.max == UL || dp->limit.max >= parm))
         {
            if(wad)
            {
               if(!dp->modified) // First time it's modified by wad
               {
                  dp->modified = 1;                               // Mark it as modified
                  dp->orig_default_b = *(boolean *)dp->location;  // Save original default
               }
               if(dp->current)            // Change current value
                  *(boolean *)dp->current = !!parm;
            }
            *(boolean *)dp->location = !!parm;  // Change default
         }
      }
      break;
   default:
      break;
   }

   return 0;                          // Success
}

//
// M_LoadOptions()
//
// killough 11/98:
// This function is used to load the OPTIONS lump.
// It allows wads to change game options.
//
void M_LoadOptions(void)
{
   int lump;
   
   if((lump = W_CheckNumForName("OPTIONS")) != -1)
   {
      int size = W_LumpLength(lump), buflen = 0;
      char *buf = NULL, *p, *options = p = W_CacheLumpNum(lump, PU_STATIC);
      while (size > 0)
      {
         int len = 0;
         while(len < size && p[len++] && p[len-1] != '\n');
         if(len >= buflen)
            buf = realloc(buf, buflen = len+1);
         strncpy(buf, p, len)[len] = 0;
         p += len;
         size -= len;
         M_ParseOption(&maindefaults, buf, true);
      }
      free(buf);
      Z_ChangeTag(options, PU_CACHE);
   }

   //  MN_ResetMenu();       // reset menu in case of change
}

//
// M_LoadDefaultFile
//
void M_LoadDefaultFile(defaultfile_t *df)
{
   register default_t *dp;
   FILE *f;

   // set everything to base values
   //
   // phares 4/13/98:
   // provide default strings with their own malloced memory so that when
   // we leave this routine, that's what we're dealing with whether there
   // was a config file or not, and whether there were chat definitions
   // in it or not. This provides consistency later on when/if we need to
   // edit these strings (i.e. chat macros in the Chat Strings Setup screen).

   for(dp = df->defaults; dp->name; dp++)
   {
      switch(dp->type)
      {
      case dt_integer:
         *(int *)dp->location = dp->defaultvalue_i;
         break;
      case dt_string:
         *(char **)dp->location = strdup(dp->defaultvalue_s);
         break;
      case dt_float:
         *(double *)dp->location = dp->defaultvalue_f;
         break;
      case dt_boolean:
         *(boolean *)dp->location = dp->defaultvalue_b;
         break;
      default:
         break;
      }
   }

   M_NormalizeSlashes(df->fileName);
   
   // read the file in, overriding any set defaults
   //
   // killough 9/21/98: Print warning if file missing, and use fgets for reading

   if(!(f = fopen(df->fileName, "r")))
      printf("Warning: Cannot read %s -- using built-in defaults\n", df->fileName);
   else
   {
      int skipblanks = 1, line = df->numcomments = df->helpHeader = 0;
      char s[256];

      while(fgets(s, sizeof s, f))
      {
         if(!M_ParseOption(df, s, false))
            line++;       // Line numbers
         else
         {             // Remember comment lines
            const char *p = s;
            
            while(isspace(*p))  // killough 10/98: skip leading whitespace
               p++;

            if(*p)                // If this is not a blank line,
            {
               skipblanks = 0;    // stop skipping blanks.
               if(strstr(p, ".cfg format:"))
                  df->helpHeader = true;
            }
            else
            {
               if(skipblanks)      // If we are skipping blanks, skip line
                  continue;
               else            // Skip multiple blanks, but remember this one
                  skipblanks = 1, p = "\n";
            }

            if(df->numcomments >= df->numcommentsalloc)
               df->comments = realloc(df->comments, sizeof *(df->comments) *
                                      (df->numcommentsalloc = df->numcommentsalloc ?
                                       df->numcommentsalloc * 2 : df->numdefaults));
            df->comments[df->numcomments].line = line;
            df->comments[df->numcomments++].text = strdup(p);
         }
      }
      fclose(f);
   }

   df->loaded = true;            // killough 10/98
   
   //jff 3/4/98 redundant range checks for hud deleted here
}

//
// M_LoadDefaults
//
// haleyjd 03/14/09: This is now the function to handle the default config.
//
void M_LoadDefaults(void)
{
   int p;
   
   defaultfile_t *df = &maindefaults;

   // check for a custom default file   
   if(!df->fileName)
   {
      if((p = M_CheckParm("-config")) && p < myargc - 1)
         printf(" default file: %s\n", df->fileName = strdup(myargv[p + 1]));
      else
         df->fileName = strdup(basedefault);
   }

   // haleyjd 06/30/09: apply gamemode defaults first
   M_ApplyGameModeDefaults(df);

   M_LoadDefaultFile(df);
}

//
// M_ResetDefaultFileComments
//
// haleyjd 01/04/10: Removes any saved comments from the given defaults file
//
void M_ResetDefaultFileComments(defaultfile_t *df)
{
   if(df->comments)
   {
      free(df->comments);
      df->comments = NULL;
      df->numcomments = df->numcommentsalloc = 0;
   }
}

//
// M_ResetDefaultComments
//
// haleyjd 01/04/10: Removes saved comments from the game config file.
//
void M_ResetDefaultComments(void)
{
   M_ResetDefaultFileComments(&maindefaults);
}

//=============================================================================
//
// File IO Routines
//

//
// M_WriteFile
//
// killough 9/98: rewritten to use stdio and to flash disk icon
//
boolean M_WriteFile(char const *name, void *source, unsigned int length)
{
   FILE *fp;
   boolean result;
   
   errno = 0;
   
   if(!(fp = fopen(name, "wb")))         // Try opening file
      return 0;                          // Could not open file for writing
   
   I_BeginRead();                       // Disk icon on
   result = (fwrite(source, 1, length, fp) == length);   // Write data
   fclose(fp);
   I_EndRead();                         // Disk icon off
   
   if(!result)                          // Remove partially written file
      remove(name);
   
   return result;
}

//
// M_ReadFile
//
// killough 9/98: rewritten to use stdio and to flash disk icon

int M_ReadFile(char const *name, byte **buffer)
{
   FILE *fp;
   
   errno = 0;
   
   if((fp = fopen(name, "rb")))
   {
      size_t length;

      I_BeginRead();
      fseek(fp, 0, SEEK_END);
      length = ftell(fp);
      fseek(fp, 0, SEEK_SET);
      *buffer = Z_Malloc(length, PU_STATIC, 0);
      
      if(fread(*buffer, 1, length, fp) == length)
      {
         fclose(fp);
         I_EndRead();
         return length;
      }
      fclose(fp);
   }

   // sf: do not quit on file not found
   //  I_Error("Couldn't read file %s: %s", name, 
   //	  errno ? strerror(errno) : "(Unknown Error)");
   
   return -1;
}

// 
// M_FileLength
//
// Gets the length of a file given its handle.
// haleyjd 03/09/06: made global
// haleyjd 01/04/10: use fseek/ftell
//
int M_FileLength(FILE *f)
{
   long curpos, len;

   curpos = ftell(f);
   fseek(f, 0, SEEK_END);
   len = ftell(f);
   fseek(f, curpos, SEEK_SET);

   return (int)len;
}

//=============================================================================
//
// Portable non-standard libc functions
//

// haleyjd: portable strupr function
char *M_Strupr(char *string)
{
   char *s = string;

   while(*s)
   {
      char c = *s;
      *s++ = toupper(c);
   }

   return string;
}

// haleyjd: portable strlwr function
char *M_Strlwr(char *string)
{
   char *s = string;

   while(*s)
   {
      char c = *s;
      *s++ = tolower(c);
   }

   return string;
}

// haleyjd: portable itoa, from DJGPP libc

/* Copyright (C) 2001 DJ Delorie, see COPYING.DJ for details */

char *M_Itoa(int value, char *string, int radix)
{
#ifdef EE_HAVE_ITOA
   return ITOA_NAME(value, string, radix);
#else
   char tmp[33];
   char *tp = tmp;
   int i;
   unsigned int v;
   int sign;
   char *sp;

   if(radix > 36 || radix <= 1)
   {
      errno = EDOM;
      return 0;
   }

   sign = (radix == 10 && value < 0);

   if(sign)
      v = -value;
   else
      v = (unsigned int)value;

   while(v || tp == tmp)
   {
      i = v % radix;
      v = v / radix;

      if(i < 10)
         *tp++ = i + '0';
      else
         *tp++ = i + 'a' - 10;
   }

   if(string == 0)
      string = (char *)(malloc)((tp-tmp)+sign+1);
   sp = string;

   if(sign)
      *sp++ = '-';

   while(tp > tmp)
      *sp++ = *--tp;
   *sp = 0;

   return string;
#endif
}

//=============================================================================
//
// Filename and Path Routines
//

//
// M_GetFilePath
//
// haleyjd: general file path name extraction
//
void M_GetFilePath(const char *fn, char *base, size_t len)
{
   boolean found_slash = false;
   char *p;

   memset(base, 0, len);

   p = base + len - 1;

   strncpy(base, fn, len);
   
   while(p >= base)
   {
      if(*p == '/' || *p == '\\')
      {
         found_slash = true; // mark that the path ended with a slash
         *p = '\0';
         break;
      }
      *p = '\0';
      p--;
   }

   // haleyjd: in the case that no slash was ever found, yet the
   // path string is empty, we are dealing with a file local to the
   // working directory. The proper path to return for such a string is
   // not "", but ".", since the format strings add a slash now. When
   // the string is empty but a slash WAS found, we really do want to
   // return the empty string, since the path is relative to the root.
   if(!found_slash && *base == '\0')
      *base = '.';
}

//
// M_ExtractFileBase
//
void M_ExtractFileBase(const char *path, char *dest)
{
   const char *src = path + strlen(path) - 1;
   int length;
   
   // back up until a \ or the start
   while(src != path && src[-1] != ':' // killough 3/22/98: allow c:filename
         && *(src-1) != '\\'
         && *(src-1) != '/')
      src--;

   // copy up to eight characters
   memset(dest, 0, 8);
   length = 0;

   while(*src && *src != '.')
      if(++length == 9)
         I_Error("Filename base of %s > 8 chars", path);
      else
         *dest++ = toupper(*src++);
}

//
// M_AddDefaultExtension
//
// 1/18/98 killough: adds a default extension to a path
// Note: Backslashes are treated specially, for MS-DOS.
//
char *M_AddDefaultExtension(char *path, const char *ext)
{
   char *p = path;
   while(*p++);
   while(p-- > path && *p != '/' && *p != '\\')
      if(*p == '.')
         return path;
   if(*ext != '.')
      strcat(path, ".");
   return strcat(path, ext);
}

//
// M_NormalizeSlashes
//
// Remove trailing slashes, translate backslashes to slashes
// The string to normalize is passed and returned in str
//
// killough 11/98: rewritten
//
void M_NormalizeSlashes(char *str)
{
   char *p;
   
   // Convert all backslashes to slashes
   for(p = str; *p; p++)
   {
      if(*p == '\\')
         *p = '/';
   }

   // Remove trailing slashes
   while(p > str && *--p == '/')
      *p = 0;

   // Collapse multiple slashes
   for(p = str; (*str++ = *p); )
      if(*p++ == '/')
         while(*p == '/')
            p++;
}

//
// M_StringAlloca
//
// haleyjd: This routine takes any number of strings and a number of extra
// characters, calculates their combined length, and calls Z_Alloca to create
// a temporary buffer of that size. This is extremely useful for allocation of
// file paths, and is used extensively in d_main.c.  The pointer returned is
// to a temporary Z_Alloca buffer, which lives until the next main loop
// iteration, so don't cache it. Note that this idiom is not possible with the
// normal non-standard alloca function, which allocates stack space.
//
int M_StringAlloca(char **str, int numstrs, size_t extra, const char *str1, ...)
{
   va_list args;
   size_t len = extra;

   if(numstrs < 1)
      I_Error("M_StringAlloca: invalid input\n");

   len += strlen(str1);

   --numstrs;

   if(numstrs != 0)
   {   
      va_start(args, str1);
      
      while(numstrs != 0)
      {
         const char *argstr = va_arg(args, const char *);
         
         len += strlen(argstr);
         
         --numstrs;
      }
      
      va_end(args);
   }

   ++len;

   *str = Z_Alloca(len);

   return len;
}

//----------------------------------------------------------------------------
//
// $Log: m_misc.c,v $
// Revision 1.60  1998/06/03  20:32:12  jim
// Fixed mispelling of key_chat string
//
// Revision 1.59  1998/05/21  12:12:28  jim
// Removed conditional from net code
//
// Revision 1.58  1998/05/16  09:41:15  jim
// formatted net files, installed temp switch for testing Stan/Lee's version
//
// Revision 1.57  1998/05/12  12:47:04  phares
// Removed OVER_UNDER code
//
// Revision 1.56  1998/05/05  19:56:01  phares
// Formatting and Doc changes
//
// Revision 1.55  1998/05/05  16:29:12  phares
// Removed RECOIL and OPT_BOBBING defines
//
// Revision 1.54  1998/05/03  23:05:19  killough
// Fix #includes, remove external decls duplicated elsewhere, fix LONG() conflict
//
// Revision 1.53  1998/04/23  13:07:27  jim
// Add exit line to automap
//
// Revision 1.51  1998/04/22  13:46:12  phares
// Added Setup screen Reset to Defaults
//
// Revision 1.50  1998/04/19  01:13:50  killough
// Fix freeing memory before use in savegame code
//
// Revision 1.49  1998/04/17  10:35:50  killough
// Add traditional_menu option for main menu
//
// Revision 1.48  1998/04/14  08:18:11  killough
// replace obsolete adaptive_gametic with realtic_clock_rate
//
// Revision 1.47  1998/04/13  21:36:33  phares
// Cemented ESC and F1 in place
//
// Revision 1.46  1998/04/13  12:30:02  phares
// Resolved Z_Free error msg when no boom.cfg file
//
// Revision 1.45  1998/04/12  22:55:33  phares
// Remaining 3 Setup screens
//
// Revision 1.44  1998/04/10  23:21:41  jim
// fixed string/int differentiation by value
//
// Revision 1.43  1998/04/10  06:37:54  killough
// Add adaptive gametic timer option
//
// Revision 1.42  1998/04/06  11:05:00  jim
// Remove LEESFIXES, AMAP bdg->247
//
// Revision 1.41  1998/04/06  04:50:00  killough
// Support demo_insurance=2
//
// Revision 1.40  1998/04/05  00:51:13  phares
// Joystick support, Main Menu re-ordering
//
// Revision 1.39  1998/04/03  14:45:49  jim
// Fixed automap disables at 0, mouse sens unbounded
//
// Revision 1.38  1998/03/31  10:44:31  killough
// Add demo insurance option
//
// Revision 1.37  1998/03/31  00:39:44  jim
// Screenshots in BMP format added
//
// Revision 1.36  1998/03/25  16:31:23  jim
// Fixed bad default value for defaultskill
//
// Revision 1.34  1998/03/23  15:24:17  phares
// Changed pushers to linedef control
//
// Revision 1.33  1998/03/20  00:29:47  phares
// Changed friction to linedef control
//
// Revision 1.32  1998/03/11  17:48:16  phares
// New cheats, clean help code, friction fix
//
// Revision 1.31  1998/03/10  07:06:30  jim
// Added secrets on automap after found only option
//
// Revision 1.30  1998/03/09  18:29:12  phares
// Created separately bound automap and menu keys
//
// Revision 1.29  1998/03/09  11:00:20  jim
// allowed -1 in mouse bindings and map functions
//
// Revision 1.28  1998/03/09  07:35:18  killough
// Rearrange order of cfg options, add capslock options
//
// Revision 1.27  1998/03/06  21:41:04  jim
// fixed erroneous range for gamma in config
//
// Revision 1.26  1998/03/05  00:57:47  jim
// Scattered HUD
//
// Revision 1.25  1998/03/04  11:55:42  jim
// Add range checking, help strings to BOOM.CFG
//
// Revision 1.24  1998/03/02  15:34:15  jim
// Added Rand's HELP screen as lump and loaded and displayed it
//
// Revision 1.23  1998/03/02  11:36:44  killough
// clone defaults, add sts_traditional_keys
//
// Revision 1.22  1998/02/27  19:22:05  jim
// Range checked hud/sound card variables
//
// Revision 1.21  1998/02/27  08:10:02  phares
// Added optional player bobbing
//
// Revision 1.20  1998/02/26  22:58:39  jim
// Added message review display to HUD
//
// Revision 1.19  1998/02/24  22:00:57  killough
// turn translucency back on by default
//
// Revision 1.18  1998/02/24  08:46:05  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.17  1998/02/23  14:21:14  jim
// Merged HUD stuff, fixed p_plats.c to support elevators again
//
// Revision 1.16  1998/02/23  04:40:48  killough
// Lots of new options
//
// Revision 1.14  1998/02/20  21:57:00  phares
// Preliminarey sprite translucency
//
// Revision 1.13  1998/02/20  18:46:58  jim
// cleanup of HUD control
//
// Revision 1.12  1998/02/19  16:54:33  jim
// Optimized HUD and made more configurable
//
// Revision 1.11  1998/02/18  11:56:11  jim
// Fixed issues with HUD and reduced screen size
//
// Revision 1.9  1998/02/15  03:21:20  phares
// Jim's comment: Fixed bug in automap from mistaking framebuffer index for mark color
//
// Revision 1.8  1998/02/15  03:17:56  phares
// User-defined keys
//
// Revision 1.6  1998/02/09  03:04:12  killough
// Add weapon preferences, player corpse, vsync options
//
// Revision 1.5  1998/02/02  13:37:26  killough
// Clone compatibility flag, for TNTCOMP to work
//
// Revision 1.4  1998/01/26  19:23:49  phares
// First rev with no ^Ms
//
// Revision 1.3  1998/01/26  04:59:07  killough
// Fix DOOM 1 screenshot acknowledgement
//
// Revision 1.2  1998/01/21  16:56:16  jim
// Music fixed, defaults for cards added
//
// Revision 1.1.1.1  1998/01/19  14:02:57  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

