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
//  Default Config File.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "hal/i_platform.h"
#include "hal/i_gamepads.h"

#include "c_runcmd.h"
#include "d_gi.h"
#include "m_misc.h"
#include "m_syscfg.h"
#include "m_argv.h"
#include "w_wad.h"

// headers needed for externs:
#include "am_map.h"
#include "c_io.h"
#include "c_net.h"
#include "d_dehtbl.h"
#include "d_englsh.h"
#include "d_io.h"
#include "d_main.h"
#include "doomstat.h"
#include "f_wipe.h"
#include "g_game.h"
#include "hu_over.h"
#include "hu_stuff.h"
#include "i_sound.h"
#include "i_video.h"
#include "m_utils.h"
#include "mn_engin.h"
#include "mn_files.h"
#include "mn_menus.h"
#include "p_chase.h"
#include "p_enemy.h"
#include "p_map.h"
#include "p_partcl.h"
#include "p_user.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_sky.h"
#include "r_things.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "v_video.h"

#ifdef HAVE_ADLMIDILIB
#include "adlmidi.h"
#endif


//
// DEFAULTS
//

int config_help;         //jff 3/3/98
int usemouse;

extern double mouseSensitivity_horiz,mouseSensitivity_vert;  // killough
extern bool mouseSensitivity_vanilla; // [CG] 01/20/12
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
#endif

#ifdef HAVE_SPCLIB
extern int spc_preamp;
extern int spc_bass_boost;
#endif

#ifdef HAVE_ADLMIDILIB
extern int midi_device;
extern int adlmidi_numchips;
extern int adlmidi_bank;
extern int adlmidi_emulator;

const int BANKS_MAX = (adl_getBanksCount() - 1);
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
   DEFAULT_INT("colour", &default_colour, NULL, 0, 0, TRANSLATIONCOLOURS, default_t::wad_no,
               "the default player colour (green, indigo, brown, red...)"),

   DEFAULT_STR("name", &default_name, NULL, "player", default_t::wad_no,
               "the default player name"),

   // jff 3/24/98 allow default skill setting
   DEFAULT_INT("default_skill", &defaultskill, NULL, 3, 1, 5, default_t::wad_no,
               "selects default skill 1=TYTD 2=NTR 3=HMP 4=UV 5=NM"),

   // haleyjd: fixed for cross-platform support -- see m_misc.h
   // jff 1/18/98 allow Allegro drivers to be set,  -1 = autodetect
   DEFAULT_INT("sound_card", &snd_card, NULL, SND_DEFAULT, SND_MIN, SND_MAX, default_t::wad_no,
               SND_DESCR),

   DEFAULT_INT("music_card", &mus_card, NULL, MUS_DEFAULT, MUS_MIN, MUS_MAX, default_t::wad_no,
               MUS_DESCR),

   // haleyjd 04/15/02: SDL joystick device number
   DEFAULT_INT("joystick_num", &i_joysticknum, NULL, -1, -1, UL, default_t::wad_no,
               "SDL joystick device number, -1 to disable"),

   // joystick sensitivity
   DEFAULT_INT("i_joysticksens", &i_joysticksens, NULL, 7849, 0, 32767, default_t::wad_no,
               "SDL MMSYSTEM joystick sensitivity"),

   DEFAULT_INT("s_precache", &s_precache, NULL, 0, 0, 1, default_t::wad_no,
               "precache sounds at startup"),
  
   // killough 2/21/98
   DEFAULT_INT("pitched_sounds", &pitched_sounds, NULL, 0, 0, 1, default_t::wad_yes,
               "1 to enable variable pitch in sound effects (from id's original code)"),

   // phares
   DEFAULT_INT("translucency", &general_translucency, NULL, 1, 0, 1, default_t::wad_yes,
               "1 to enable translucency for some things"),

   // killough 2/21/98
   DEFAULT_INT("tran_filter_pct", &tran_filter_pct, NULL, 66, 0, 100, default_t::wad_yes,
               "set percentage of foreground/background translucency mix"),

   // killough 2/8/98
   DEFAULT_INT("max_player_corpse", &default_bodyquesize, NULL, 32, UL, UL, default_t::wad_no,
               "number of dead bodies in view supported (negative value = no limit)"),

   // killough 10/98
   DEFAULT_INT("flashing_hom", &flashing_hom, NULL, 1, 0, 1, default_t::wad_no,
               "1 to enable flashing HOM indicator"),

   // killough 3/31/98
   DEFAULT_INT("demo_insurance", &default_demo_insurance, NULL, 2, 0, 2, default_t::wad_no,
               "1=take special steps ensuring demo sync, 2=only during recordings"),
   
   // phares
   DEFAULT_INT("weapon_recoil", &default_weapon_recoil, &weapon_recoil, 0, 0, 1, default_t::wad_yes,
               "1 to enable recoil from weapon fire"),

   // killough 7/19/98
   // sf:changed to bfgtype
   // haleyjd: FIXME - variable is of enum type, non-portable
   DEFAULT_INT("bfgtype", &default_bfgtype, &bfgtype, 0, 0, 4, default_t::wad_yes,
               "0 - normal, 1 - classic, 2 - 11k, 3 - bouncing!, 4 - burst"),

   //sf
   DEFAULT_INT("crosshair", &crosshairnum, NULL, 0, 0, CROSSHAIRS, default_t::wad_yes,
               "0 - none, 1 - cross, 2 - angle"),

   // haleyjd 06/07/05
   DEFAULT_BOOL("crosshair_hilite", &crosshair_hilite, NULL, false, default_t::wad_yes,
                "0 - no highlighting, 1 - aim highlighting enabled"),

   // sf
   DEFAULT_INT("show_scores", &show_scores, NULL, 0, 0, 1, default_t::wad_yes,
               "show scores in deathmatch"),

   DEFAULT_INT("lefthanded", &lefthanded, NULL, 0, 0, 1, default_t::wad_yes,
               "0 - right handed, 1 - left handed"),

   // killough 10/98
   DEFAULT_INT("weapon_hotkey_cycling", &weapon_hotkey_cycling, NULL, 1, 0, 1, default_t::wad_no,
               "1 to allow in-slot weapon cycling (e.g. SSG to SG)"),

   // phares 2/25/98
   DEFAULT_INT("player_bobbing", &default_player_bobbing, &player_bobbing, 1, 0, 1, default_t::wad_yes,
               "1 to enable player bobbing (view moving up/down slightly)"),

   // killough 3/1/98
   DEFAULT_INT("monsters_remember", &default_monsters_remember, &monsters_remember,
               1, 0, 1, default_t::wad_yes,
               "1 to enable monsters remembering enemies after killing others"),

   // killough 7/19/98
   DEFAULT_INT("monster_infighting", &default_monster_infighting, &monster_infighting,
               1, 0, 1, default_t::wad_yes,
               "1 to enable monsters fighting against each other when provoked"),

   // killough 9/8/98
   DEFAULT_INT("monster_backing", &default_monster_backing, &monster_backing, 
               0, 0, 1, default_t::wad_yes, "1 to enable monsters backing away from targets"),

   //killough 9/9/98:
   DEFAULT_INT("monster_avoid_hazards", &default_monster_avoid_hazards, &monster_avoid_hazards,
               1, 0, 1, default_t::wad_yes, "1 to enable monsters to intelligently avoid hazards"),
   
   DEFAULT_INT("monkeys", &default_monkeys, &monkeys, 0, 0, 1, default_t::wad_yes,
               "1 to enable monsters to move up/down steep stairs"),
   
   //killough 9/9/98:
   DEFAULT_INT("monster_friction", &default_monster_friction, &monster_friction,
               1, 0, 1, default_t::wad_yes, "1 to enable monsters to be affected by friction"),
   
   //killough 9/9/98:
   DEFAULT_INT("help_friends", &default_help_friends, &help_friends, 1, 0, 1, default_t::wad_yes,
               "1 to enable monsters to help dying friends"),
   
   // killough 7/19/98
   DEFAULT_INT("player_helpers", &default_dogs, &dogs, 0, 0, 3, default_t::wad_yes,
               "number of single-player helpers"),
   
   // CONFIG_FIXME: 999?
   // killough 8/8/98
   DEFAULT_INT("friend_distance", &default_distfriend, &distfriend, 128, 0, 999, default_t::wad_yes,
               "distance friends stay away"),
   
   // killough 10/98
   DEFAULT_INT("dog_jumping", &default_dog_jumping, &dog_jumping, 1, 0, 1, default_t::wad_yes,
               "1 to enable dogs to jump"),

   DEFAULT_INT("p_lastenemyroar", &p_lastenemyroar, NULL, 1, 0, 1, default_t::wad_yes,
               "1 to enable monster roaring when last enemy is remembered"),

   DEFAULT_INT("p_markunknowns", &p_markunknowns, NULL, 1, 0, 1, default_t::wad_no,
               "1 to mark unknown thingtype locations"),

   DEFAULT_BOOL("p_pitchedflight", &default_pitchedflight, &pitchedflight, true, default_t::wad_yes, 
                "1 to enable flying in the direction you are looking"),
   
   // no color changes on status bar
   DEFAULT_INT("sts_always_red", &sts_always_red, NULL, 1, 0, 1, default_t::wad_yes,
               "1 to disable use of color on status bar"),
   
   DEFAULT_INT("sts_pct_always_gray", &sts_pct_always_gray, NULL, 0, 0, 1, default_t::wad_yes,
               "1 to make percent signs on status bar always gray"),
   
   // killough 2/28/98
   DEFAULT_INT("sts_traditional_keys", &sts_traditional_keys, NULL, 1, 0, 1, default_t::wad_yes,
               "1 to disable doubled card and skull key display on status bar"),

   // killough 3/6/98
   DEFAULT_INT("leds_always_off", &leds_always_off, NULL, 0, 0, 1, default_t::wad_no,
               "1 to keep keyboard LEDs turned off"),

   //jff 4/3/98 allow unlimited sensitivity
   DEFAULT_FLOAT("mouse_sensitivity_horiz", &mouseSensitivity_horiz, NULL, 5.0, 0, UL, default_t::wad_no,
               "adjust horizontal (x) mouse sensitivity"),

   //jff 4/3/98 allow unlimited sensitivity
   DEFAULT_FLOAT("mouse_sensitivity_vert", &mouseSensitivity_vert, NULL, 5.0, 0, UL, default_t::wad_no,
               "adjust vertical (y) mouse sensitivity"),

   // SoM
   DEFAULT_INT("mouse_accel", &mouseAccel_type, NULL,
               ACCELTYPE_NONE, ACCELTYPE_NONE, ACCELTYPE_MAX, default_t::wad_no,
               "0 for no mouse accel, 1 for linear, 2 for choco-doom, 3 for custom"),

   // [CG] 01/20/12
   DEFAULT_BOOL("vanilla_sensitivity", &mouseSensitivity_vanilla, NULL, true, default_t::wad_no,
                "use vanilla mouse sensitivity values"),

   DEFAULT_INT("mouse_accel_threshold", &mouseAccel_threshold, NULL, 10, 0, UL, default_t::wad_no,
               "threshold at which to apply mouse acceleration (custom acceleration mode only)"),

   DEFAULT_FLOAT("mouse_accel_value", &mouseAccel_value, NULL, 2.0, 0, UL, default_t::wad_no,
                 "amount of mouse acceleration to apply (custom acceleration mode only)"),

   // haleyjd 10/24/08
   DEFAULT_INT("mouse_novert", &novert, NULL, 0, 0, 1, default_t::wad_no,
               "0 for normal mouse, 1 for no vertical movement"),

   DEFAULT_INT("smooth_turning", &smooth_turning, NULL, 0, 0, 1, default_t::wad_no,
               "average mouse input when turning player"),

   DEFAULT_INT("sfx_volume", &snd_SfxVolume, NULL, 8, 0, 15, default_t::wad_no,
               "adjust sound effects volume"),

   DEFAULT_INT("music_volume", &snd_MusicVolume, NULL, 8, 0, 15, default_t::wad_no,
               "adjust music volume"),

   DEFAULT_INT("show_messages", &showMessages, NULL, 1, 0, 1, default_t::wad_no,
               "1 to enable message display"),

   DEFAULT_INT("mess_colour", &mess_colour, NULL, CR_RED, 0, CR_BUILTIN, default_t::wad_no,
               "messages colour"),

   // killough 3/6/98: preserve autorun across games
   DEFAULT_INT("autorun", &autorun, NULL, 0, 0, 1, default_t::wad_no, "1 to enable autorun"),

   // haleyjd 08/23/09: allow shift to cancel autorun
   DEFAULT_INT("runiswalk", &runiswalk, NULL, 0, 0, 1, default_t::wad_no, 
               "1 to walk with shift when autorun is enabled"),

   // killough 2/21/98: default to 10
   // sf: removed screenblocks, screensize only now - changed values down 3
   DEFAULT_INT("screensize", &screenSize, NULL, 7, 0, 8, default_t::wad_no, 
               "initial play screen size"),

   //jff 3/6/98 fix erroneous upper limit in range
   DEFAULT_INT("usegamma", &usegamma, NULL, 0, 0, 4, default_t::wad_no,
               "screen brightness (gamma correction)"),

   // killough 10/98: preloaded files
   DEFAULT_STR("wadfile_1", &wad_files[0], NULL, "", default_t::wad_no,
               "WAD file preloaded at program startup"),

   DEFAULT_STR("wadfile_2", &wad_files[1], NULL, "", default_t::wad_no,
               "WAD file preloaded at program startup"),

   DEFAULT_STR("dehfile_1", &deh_files[0], NULL, "", default_t::wad_no,
               "DEH/BEX file preloaded at program startup"),

   DEFAULT_STR("dehfile_2", &deh_files[1], NULL, "", default_t::wad_no,
               "DEH/BEX file preloaded at program startup"),

   // haleyjd: auto-loaded console scripts
   DEFAULT_STR("cscript_1", &csc_files[0], NULL, "", default_t::wad_no,
               "Console script executed at program startup"),
  
   DEFAULT_STR("cscript_2", &csc_files[1], NULL, "", default_t::wad_no,
               "Console script executed at program startup"),
  
   DEFAULT_INT("use_startmap", &use_startmap, NULL, -1, -1, 1, default_t::wad_yes,
               "use start map instead of menu"),

   // killough 10/98: compatibility vector:

   DEFAULT_INT("comp_zombie", &default_comp[comp_zombie], &comp[comp_zombie], 
               1, 0, 1, default_t::wad_yes, "Zombie players can exit levels"),

   DEFAULT_INT("comp_infcheat", &default_comp[comp_infcheat], &comp[comp_infcheat],
               1, 0, 1, default_t::wad_yes, "Powerup cheats are not infinite duration"),

   DEFAULT_INT("comp_stairs", &default_comp[comp_stairs], &comp[comp_stairs],
               1, 0, 1, default_t::wad_yes, "Build stairs exactly the same way that Doom does"),

   DEFAULT_INT("comp_telefrag", &default_comp[comp_telefrag], &comp[comp_telefrag],
               0, 0, 1, default_t::wad_yes, "Monsters can telefrag on MAP30"),

   DEFAULT_INT("comp_dropoff", &default_comp[comp_dropoff], &comp[comp_dropoff],
               1, 0, 1, default_t::wad_yes, "Some objects never move over tall ledges"),

   DEFAULT_INT("comp_falloff", &default_comp[comp_falloff], &comp[comp_falloff],
               0, 0, 1, default_t::wad_yes, "Objects don't fall off ledges under their own weight"),

   DEFAULT_INT("comp_staylift", &default_comp[comp_staylift], &comp[comp_staylift],
               0, 0, 1, default_t::wad_yes, "Monsters randomly walk off of moving lifts"),

   DEFAULT_INT("comp_doorstuck", &default_comp[comp_doorstuck], &comp[comp_doorstuck],
               0, 0, 1, default_t::wad_yes, "Monsters get stuck on doortracks"),

   DEFAULT_INT("comp_pursuit", &default_comp[comp_pursuit], &comp[comp_pursuit],
               0, 0, 1, default_t::wad_yes, "Monsters don't give up pursuit of targets"),

   DEFAULT_INT("comp_vile", &default_comp[comp_vile], &comp[comp_vile],
               1, 0, 1, default_t::wad_yes, "Arch-Vile resurrects invincible ghosts"),

   DEFAULT_INT("comp_pain", &default_comp[comp_pain], &comp[comp_pain],
               0, 0, 1, default_t::wad_yes, "Pain Elemental limited to 20 lost souls"),

   DEFAULT_INT("comp_skull", &default_comp[comp_skull], &comp[comp_skull],
               1, 0, 1, default_t::wad_yes, "Lost souls get stuck behind walls"),

   DEFAULT_INT("comp_blazing", &default_comp[comp_blazing], &comp[comp_blazing],
               0, 0, 1, default_t::wad_yes, "Blazing doors make double closing sounds"),

   DEFAULT_INT("comp_doorlight", &default_comp[comp_doorlight], &comp[comp_doorlight],
               0, 0, 1, default_t::wad_yes, "Tagged doors don't trigger special lighting"),

   DEFAULT_INT("comp_god", &default_comp[comp_god], &comp[comp_god],
               1, 0, 1, default_t::wad_yes, "God mode isn't absolute"),

   DEFAULT_INT("comp_skymap", &default_comp[comp_skymap], &comp[comp_skymap],
               1, 0, 1, default_t::wad_yes, "Sky is unaffected by invulnerability"),

   DEFAULT_INT("comp_floors", &default_comp[comp_floors], &comp[comp_floors],
               0, 0, 1, default_t::wad_yes, "Use exactly Doom's floor motion behavior"),

   DEFAULT_INT("comp_model", &default_comp[comp_model], &comp[comp_model],
               0, 0, 1, default_t::wad_yes, "Use exactly Doom's linedef trigger model"),

   DEFAULT_INT("comp_zerotags", &default_comp[comp_zerotags], &comp[comp_zerotags],
               0, 0, 1, default_t::wad_yes, "Linedef effects work with sector tag = 0"),

   // haleyjd
   DEFAULT_INT("comp_terrain", &default_comp[comp_terrain], &comp[comp_terrain], 
               1, 0, 1, default_t::wad_yes, "Terrain effects not activated on floor contact"),
   
   // haleyjd
   DEFAULT_INT("comp_respawnfix", &default_comp[comp_respawnfix], &comp[comp_respawnfix],
               0, 0, 1, default_t::wad_yes, "Creatures with no spawnpoint respawn at (0,0)"),
   
   // haleyjd
   DEFAULT_INT("comp_fallingdmg", &default_comp[comp_fallingdmg], &comp[comp_fallingdmg],
               1, 0, 1, default_t::wad_yes, "Players do not take falling damage"),
   
   // haleyjd
   DEFAULT_INT("comp_soul", &default_comp[comp_soul], &comp[comp_soul],
               0, 0, 1, default_t::wad_yes, "Lost souls do not bounce on floors"),
   
   // haleyjd 02/15/02: z checks (includes,supercedes comp_scratch)
   DEFAULT_INT("comp_overunder", &default_comp[comp_overunder], &comp[comp_overunder],
               0, 0, 1, default_t::wad_yes, "Things not fully clipped with respect to z coord"),
   
   DEFAULT_INT("comp_theights", &default_comp[comp_theights], &comp[comp_theights],
               0, 0, 1, default_t::wad_yes, "DOOM thingtypes use inaccurate height information"),
   
   DEFAULT_INT("comp_planeshoot", &default_comp[comp_planeshoot], &comp[comp_planeshoot],
               0, 0, 1, default_t::wad_yes, "Tracer shots cannot hit the floor or ceiling"),

   DEFAULT_INT("comp_special", &default_comp[comp_special], &comp[comp_special],
               0, 0, 1, default_t::wad_yes, "One-time line specials are cleared on failure"),

   DEFAULT_INT("comp_ninja", &default_comp[comp_ninja], &comp[comp_ninja],
               0, 0, 1, default_t::wad_yes, "Silent spawns at W/SW/S-facing DM spots"),
   
   DEFAULT_INT("comp_aircontrol", &default_comp[comp_aircontrol], &comp[comp_aircontrol],
               1, 0, 1, default_t::wad_yes, "Disable air control for jumping"),

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

   DEFAULT_INT("key_pause", &key_pause, NULL, KEYD_PAUSE, 0, 255, default_t::wad_no,
               "key to pause the game"),
   
   DEFAULT_INT("key_chat", &key_chat, NULL, 't', 0, 255, default_t::wad_no,
               "key to enter a chat message"),
   
   DEFAULT_INT("key_chatplayer1", &destination_keys[0], NULL, 'g', 0, 255, default_t::wad_no,
               "key to chat with player 1"),
   
   // killough 11/98: fix 'i'/'b' reversal
   DEFAULT_INT("key_chatplayer2", &destination_keys[1], NULL, 'i', 0, 255, default_t::wad_no,
               "key to chat with player 2"),
   
   // killough 11/98: fix 'i'/'b' reversal
   DEFAULT_INT("key_chatplayer3", &destination_keys[2], NULL, 'b', 0, 255, default_t::wad_no,
               "key to chat with player 3"),
   
   DEFAULT_INT("key_chatplayer4", &destination_keys[3], NULL, 'r', 0, 255, default_t::wad_no,
               "key to chat with player 4"),
   
   DEFAULT_INT("automlook", &automlook, NULL, 0, 0, 1, default_t::wad_no, 
               "set to 1 to always mouselook"),
   
   DEFAULT_INT("invert_mouse", &invert_mouse, NULL, 0, 0, 1, default_t::wad_no,
               "set to 1 to invert mouse during mouselooking"),

   DEFAULT_INT("invert_padlook", &invert_padlook, NULL, 0, 0, 1, default_t::wad_no,
               "set to 1 to invert gamepad looking"),
      
   DEFAULT_INT("use_mouse", &usemouse, NULL, 1, 0, 1, default_t::wad_no,
               "1 to enable use of mouse"),
   
   //jff 3/8/98 end of lower range change for -1 allowed in mouse binding
   // haleyjd: rename these buttons on the user-side to prevent confusion
   DEFAULT_INT("mouseb_dblc1", &mouseb_dblc1, NULL, 1, -1, 2, default_t::wad_no,
               "1st mouse button to enable for double-click use action (-1 = disable)"),
   
   DEFAULT_INT("mouseb_dblc2", &mouseb_dblc2, NULL, 2, -1, 2, default_t::wad_no,
               "2nd mouse button to enable for double-click use action (-1 = disable)"),
   
   DEFAULT_STR("chatmacro0", &chat_macros[0], NULL, HUSTR_CHATMACRO0, default_t::wad_yes,
               "chat string associated with 0 key"),
   
   DEFAULT_STR("chatmacro1", &chat_macros[1], NULL, HUSTR_CHATMACRO1, default_t::wad_yes,
               "chat string associated with 1 key"),
   
   DEFAULT_STR("chatmacro2", &chat_macros[2], NULL, HUSTR_CHATMACRO2, default_t::wad_yes,
               "chat string associated with 2 key"),   

   DEFAULT_STR("chatmacro3", &chat_macros[3], NULL, HUSTR_CHATMACRO3, default_t::wad_yes,
               "chat string associated with 3 key"),
   
   DEFAULT_STR("chatmacro4", &chat_macros[4], NULL, HUSTR_CHATMACRO4, default_t::wad_yes,
               "chat string associated with 4 key"),
   
   DEFAULT_STR("chatmacro5", &chat_macros[5], NULL, HUSTR_CHATMACRO5, default_t::wad_yes,
               "chat string associated with 5 key"),

   DEFAULT_STR("chatmacro6", &chat_macros[6], NULL, HUSTR_CHATMACRO6, default_t::wad_yes,
               "chat string associated with 6 key"),
   
   DEFAULT_STR("chatmacro7", &chat_macros[7], NULL, HUSTR_CHATMACRO7, default_t::wad_yes,
               "chat string associated with 7 key"),
   
   DEFAULT_STR("chatmacro8", &chat_macros[8], NULL, HUSTR_CHATMACRO8, default_t::wad_yes,
               "chat string associated with 8 key"),
   
   DEFAULT_STR("chatmacro9", &chat_macros[9], NULL, HUSTR_CHATMACRO9, default_t::wad_yes,
               "chat string associated with 9 key"),
   
   //jff 1/7/98 defaults for automap colors
   //jff 4/3/98 remove -1 in lower range, 0 now disables new map features
   // black //jff 4/6/98 new black
   DEFAULT_INT("mapcolor_back", &mapcolor_back, NULL, 247, 0, 255, default_t::wad_yes,
               "color used as background for automap"),
   
   // dk gray
   DEFAULT_INT("mapcolor_grid", &mapcolor_grid, NULL, 104, 0, 255, default_t::wad_yes,
               "color used for automap grid lines"),
   
   // red-brown
   DEFAULT_INT("mapcolor_wall", &mapcolor_wall, NULL, 181, 0, 255, default_t::wad_yes,
               "color used for one side walls on automap"),
   
   // lt brown
   DEFAULT_INT("mapcolor_fchg", &mapcolor_fchg, NULL, 166, 0, 255, default_t::wad_yes,
               "color used for lines floor height changes across"),
   
   // orange
   DEFAULT_INT("mapcolor_cchg", &mapcolor_cchg, NULL, 231, 0, 255, default_t::wad_yes,
               "color used for lines ceiling height changes across"),
   
   // white
   DEFAULT_INT("mapcolor_clsd", &mapcolor_clsd, NULL, 231, 0, 255, default_t::wad_yes,
               "color used for lines denoting closed doors, objects"),
   
   // red
   DEFAULT_INT("mapcolor_rkey",&mapcolor_rkey, NULL, 175, 0, 255, default_t::wad_yes,
               "color used for red key sprites"),
   
   // blue
   DEFAULT_INT("mapcolor_bkey",&mapcolor_bkey, NULL, 204, 0, 255, default_t::wad_yes,
               "color used for blue key sprites"),
   
   // yellow
   DEFAULT_INT("mapcolor_ykey",&mapcolor_ykey, NULL, 231, 0, 255, default_t::wad_yes,
               "color used for yellow key sprites"),
   
   // red
   DEFAULT_INT("mapcolor_rdor",&mapcolor_rdor, NULL, 175, 0, 255, default_t::wad_yes,
               "color used for closed red doors"),
   
   // blue
   DEFAULT_INT("mapcolor_bdor",&mapcolor_bdor, NULL, 204, 0, 255, default_t::wad_yes,
               "color used for closed blue doors"),
   
   // yellow
   DEFAULT_INT("mapcolor_ydor",&mapcolor_ydor, NULL, 231, 0, 255, default_t::wad_yes,
               "color used for closed yellow doors"),
   
   // dk green
   DEFAULT_INT("mapcolor_tele",&mapcolor_tele, NULL, 119, 0, 255, default_t::wad_yes,
               "color used for teleporter lines"),
   
   // purple
   DEFAULT_INT("mapcolor_secr",&mapcolor_secr, NULL, 176, 0, 255, default_t::wad_yes,
               "color used for lines around secret sectors"),
   
   // none
   DEFAULT_INT("mapcolor_exit",&mapcolor_exit, NULL, 0, 0, 255, default_t::wad_yes,
               "color used for exit lines"),

   // dk gray
   DEFAULT_INT("mapcolor_unsn",&mapcolor_unsn, NULL, 96, 0, 255, default_t::wad_yes,
               "color used for lines not seen without computer map"),
   
   // lt gray
   DEFAULT_INT("mapcolor_flat",&mapcolor_flat, NULL, 88, 0, 255, default_t::wad_yes,
               "color used for lines with no height changes"),
   
   // green
   DEFAULT_INT("mapcolor_sprt",&mapcolor_sprt, NULL, 112, 0, 255, default_t::wad_yes,
               "color used as things"),
   
   // white
   DEFAULT_INT("mapcolor_hair",&mapcolor_hair, NULL, 208, 0, 255, default_t::wad_yes,
               "color used for dot crosshair denoting center of map"),
   
   // white
   DEFAULT_INT("mapcolor_sngl",&mapcolor_sngl, NULL, 208, 0, 255, default_t::wad_yes,
               "color used for the single player arrow"),
   
   // green
   DEFAULT_INT("mapcolor_ply1",&mapcolor_plyr[0], NULL, 112, 0, 255, default_t::wad_yes,
               "color used for the green player arrow"),
   
   // lt gray
   DEFAULT_INT("mapcolor_ply2",&mapcolor_plyr[1], NULL, 88, 0, 255, default_t::wad_yes,
               "color used for the gray player arrow"),
   
   // brown
   DEFAULT_INT("mapcolor_ply3",&mapcolor_plyr[2], NULL, 64, 0, 255, default_t::wad_yes,
               "color used for the brown player arrow"),
   
   // red
   DEFAULT_INT("mapcolor_ply4",&mapcolor_plyr[3], NULL, 176, 0, 255, default_t::wad_yes,
               "color used for the red player arrow"),
   
   // purple                       // killough 8/8/98
   DEFAULT_INT("mapcolor_frnd",&mapcolor_frnd, NULL, 252, 0, 255, default_t::wad_yes,
               "color used for friends"),
   

   DEFAULT_INT("mapcolor_prtl",&mapcolor_prtl, NULL, 109, 0, 255, default_t::wad_yes,
               "color for lines not in the player's portal area"),
   
   DEFAULT_INT("mapportal_overlay",&mapportal_overlay, NULL, 1, 0, 1, default_t::wad_yes,
               "1 to overlay different linked portal areas in the automap"),
            

   DEFAULT_INT("map_point_coord", &map_point_coordinates, NULL, 1, 0, 1, default_t::wad_yes,
               "1 to show automap pointer coordinates in non-follow mode"),
   
   //jff 3/9/98 add option to not show secrets til after found
   // killough change default, to avoid spoilers and preserve Doom mystery
   // show secret after gotten
   DEFAULT_INT("map_secret_after",&map_secret_after, NULL, 1, 0, 1, default_t::wad_yes,
               "1 to not show secret sectors till after entered"),
   
   //jff 1/7/98 end additions for automap

   //jff 2/16/98 defaults for color ranges in hud and status

   // 1 line scrolling window
   DEFAULT_INT("hud_msg_lines",&hud_msg_lines, NULL, 1, 1, 16, default_t::wad_yes,
               "number of lines in review display"),
   
   // killough 11/98
   DEFAULT_INT("hud_msg_scrollup",&hud_msg_scrollup, NULL, 1, 0, 1, default_t::wad_yes,
               "1 enables message review list scrolling upward"),
   
   // killough 11/98
   DEFAULT_INT("message_timer",&message_timer, NULL, 4000, 0, UL, default_t::wad_no,
               "Duration of normal Doom messages (ms)"),

   DEFAULT_INT("hud_overlayid", &hud_overlayid, NULL, -1, -1, VDR_MAXDRIVERS - 1, default_t::wad_no,
               "Select HUD overlay (-1 = default, 0 = modern, 1 = boom"),
   
   //sf : fullscreen hud style
   DEFAULT_INT("hud_overlaylayout", &hud_overlaylayout, NULL, 2, 0, 4, default_t::wad_yes,
               "fullscreen hud layout"),

   DEFAULT_INT("hud_enabled",&hud_enabled, NULL, 1, 0, 1, default_t::wad_yes,
               "fullscreen hud enabled"),
   
   DEFAULT_INT("hud_hidestatus",&hud_hidestatus, NULL, 0, 0, 1, default_t::wad_yes,
               "hide kills/items/secrets info on fullscreen hud"),
   
   DEFAULT_BOOL("hu_showtime", &hu_showtime, NULL, true, default_t::wad_yes,
                "display current level time on automap"),
   
   DEFAULT_BOOL("hu_showcoords", &hu_showcoords, NULL, true, default_t::wad_yes,
                "display player/pointer coordinates on automap"),
   
   DEFAULT_INT("hu_timecolor",&hu_timecolor, NULL, CR_RED, 0, CR_BUILTIN, default_t::wad_yes,
               "color of automap level time widget"),

   DEFAULT_INT("hu_levelnamecolor",&hu_levelnamecolor, NULL, CR_RED, 0, CR_BUILTIN, default_t::wad_yes,
               "color of automap level name widget"),
   
   DEFAULT_INT("hu_coordscolor",&hu_coordscolor, NULL, CR_RED, 0, CR_BUILTIN, default_t::wad_yes,
               "color of automap coordinates widget"),
   
   // below is red
   DEFAULT_INT("health_red",&health_red, NULL, 25, 0, 200, default_t::wad_yes,
               "amount of health for red to yellow transition"),

   // below is yellow
   DEFAULT_INT("health_yellow", &health_yellow, NULL, 50, 0, 200, default_t::wad_yes,
               "amount of health for yellow to green transition"),
   
   // below is green, above blue
   DEFAULT_INT("health_green",&health_green, NULL, 100, 0, 200, default_t::wad_yes,
               "amount of health for green to blue transition"),
   
   // below is red
   DEFAULT_INT("armor_red",&armor_red, NULL, 25, 0, 200, default_t::wad_yes,
               "amount of armor for red to yellow transition"),
   
   // below is yellow
   DEFAULT_INT("armor_yellow",&armor_yellow, NULL, 50, 0, 200, default_t::wad_yes,
               "amount of armor for yellow to green transition"),
   
   // below is green, above blue
   DEFAULT_INT("armor_green",&armor_green, NULL, 100, 0, 200, default_t::wad_yes,
               "amount of armor for green to blue transition"),

   DEFAULT_BOOL("armor_byclass", &armor_byclass, NULL, true, default_t::wad_yes,
                "reflect armor class using blue or green color"),
   
   // below 25% is red
   DEFAULT_INT("ammo_red",&ammo_red, NULL, 25, 0, 100, default_t::wad_yes,
               "percent of ammo for red to yellow transition"),
   
   // below 50% is yellow, above green
   DEFAULT_INT("ammo_yellow",&ammo_yellow, NULL, 50, 0, 100, default_t::wad_yes,
               "percent of ammo for yellow to green transition"),

   DEFAULT_INT("st_fsalpha", &st_fsalpha, NULL, 100, 0, 100, default_t::wad_yes,
               "fullscreen HUD translucency level"),
   
   DEFAULT_INT("c_speed",&c_speed, NULL, 10, 1, 200, default_t::wad_no,
               "console speed, pixels/tic"),
   
   DEFAULT_INT("c_height",&c_height, NULL, 100, 0, 200, default_t::wad_no,
               "console height, pixels"),
   
   DEFAULT_INT("obituaries",&obituaries, NULL, 0, 0, 1, default_t::wad_yes,
               "obituaries on/off"),
   
   DEFAULT_INT("obcolour",&obcolour, NULL, 0, 0, CR_BUILTIN, default_t::wad_no,
               "obituaries colour"),
   
   DEFAULT_INT("draw_particles",&drawparticles, NULL, 0, 0, 1, default_t::wad_yes,
               "toggle particle effects on or off"),
   
   DEFAULT_INT("particle_trans",&particle_trans, NULL, 1, 0, 2, default_t::wad_yes,
               "particle translucency (0 = none, 1 = smooth, 2 = general)"),
   
   DEFAULT_INT("blood_particles",&bloodsplat_particle, NULL, 0, 0, 2, default_t::wad_yes,
               "use sprites, particles, or both for blood (sprites = 0)"),
   
   DEFAULT_INT("bullet_particles",&bulletpuff_particle, NULL, 0, 0, 2, default_t::wad_yes,
               "use sprites, particles, or both for bullet puffs (sprites = 0)"),
   
   DEFAULT_INT("rocket_trails",&drawrockettrails, NULL, 0, 0, 1, default_t::wad_yes,
               "draw particle rocket trails"),

   DEFAULT_INT("grenade_trails",&drawgrenadetrails, NULL, 0, 0, 1, default_t::wad_yes,
               "draw particle grenade trails"),
   
   DEFAULT_INT("bfg_cloud",&drawbfgcloud, NULL, 0, 0, 1, default_t::wad_yes,
               "draw particle bfg cloud"),
   
   DEFAULT_INT("pevent_rexpl",&(particleEvents[P_EVENT_ROCKET_EXPLODE].enabled), NULL,
               0, 0, 1, default_t::wad_yes, "draw particle rocket explosions"),
   
   DEFAULT_INT("pevent_bfgexpl",&(particleEvents[P_EVENT_BFG_EXPLODE].enabled), NULL,
               0, 0, 1, default_t::wad_yes, "draw particle bfg explosions"),

   DEFAULT_INT("stretchsky", &stretchsky, NULL, 0, 0, 1, default_t::wad_yes,
               "stretch short sky textures for mlook"),

#ifdef _SDL_VER   
   DEFAULT_INT("showendoom", &showendoom, NULL, 1, 0, 1, default_t::wad_yes,
               "1 to show ENDOOM at exit"),

   DEFAULT_INT("endoomdelay", &endoomdelay, NULL, 350, 35, 3500, default_t::wad_no,
               "Amount of time to display ENDOOM when shown"),
#endif

   DEFAULT_INT("autoaim", &default_autoaim, &autoaim, 1, 0, 1, default_t::wad_yes,
               "1 to enable autoaiming"),
   
   DEFAULT_INT("chasecam_height", &chasecam_height, NULL, 15, -31, 100, default_t::wad_no,
               "preferred height of chasecam above/below player viewheight"),
   
   DEFAULT_INT("chasecam_speed", &chasecam_speed, NULL, 33, 1, 100, default_t::wad_no,
               "percentage of distance to target chasecam moves per gametic"),
   
   DEFAULT_INT("chasecam_dist", &chasecam_dist, NULL, 112, 10, 1024, default_t::wad_no,
               "preferred distance from chasecam to player"),
   
   DEFAULT_INT("allowmlook", &default_allowmlook, &allowmlook, 0, 0, 1, default_t::wad_yes,
               "1 to allow players to look up/down"),
   
   DEFAULT_BOOL("menu_toggleisback", &menu_toggleisback, NULL, false, default_t::wad_no,
                "1 to make menu toggle action back up one level (like zdoom)"),
   
   DEFAULT_INT("mn_classic_menus", &mn_classic_menus, NULL, 0, 0, 1, default_t::wad_yes,
               "1 to enable use of full classic menu emulation"),

   DEFAULT_STR("mn_background", &mn_background, NULL, "default", default_t::wad_yes,
               "menu background"),
   
   DEFAULT_STR("wad_directory", &wad_directory, NULL, ".", default_t::wad_no,
               "user's default wad directory"),
   
   DEFAULT_INT("r_columnengine",&r_column_engine_num, NULL, 
               1, 0, NUMCOLUMNENGINES - 1, default_t::wad_no, 
               "0 = normal, 1 = optimized quad cache"),
   
   DEFAULT_INT("r_spanengine",&r_span_engine_num, NULL,
               0, 0, NUMSPANENGINES - 1, default_t::wad_no, 
               "0 = high precision, 1 = low precision"),

   DEFAULT_INT("r_tlstyle", &r_tlstyle, NULL, 1, 0, R_TLSTYLE_NUM - 1, default_t::wad_yes,
               "Doom object translucency style (0 = none, 1 = Boom, 2 = new)"),
   
   DEFAULT_INT("spechits_emulation", &spechits_emulation, NULL, 0, 0, 2, default_t::wad_no,
               "0 = off, 1 = emulate like Chocolate Doom, 2 = emulate like PrBoom+"),

   DEFAULT_BOOL("donut_emulation", &donut_emulation, NULL, false, default_t::wad_no,
                "emulate undefined EV_DoDonut behavior"),
   
   DEFAULT_INT("wipewait",&wipewait, NULL, 1, 0, 2, default_t::wad_no,
               "0 = never wait on screen wipes, 1 = always wait, 2 = wait when playing demos"),
   
   DEFAULT_INT("wipetype",&wipetype, NULL, 1, 0, 2, default_t::wad_yes,
               "0 = none, 1 = melt, 2 = fade"),
   
#ifdef HAVE_SPCLIB
   DEFAULT_INT("snd_spcpreamp", &spc_preamp, NULL, 1, 1, 6, default_t::wad_yes,
               "preamp volume factor for SPC music"),
   
   DEFAULT_INT("snd_spcbassboost", &spc_bass_boost, NULL, 8, 1, 31, default_t::wad_yes,
               "bass boost for SPC music (logarithmic scale, 8 = normal)"),
   
#endif

#ifdef HAVE_ADLMIDILIB
   DEFAULT_INT("snd_mididevice", &midi_device, NULL, -1, -1, 0, default_t::wad_yes,
               "device used for MIDI playback"),

   DEFAULT_INT("snd_oplemulator", &adlmidi_emulator, NULL, ADLMIDI_EMU_DOSBOX, 0, ADLMIDI_EMU_end - 1, default_t::wad_no,
               "TODO: adlmidi_bank description"),

   DEFAULT_INT("snd_numchips", &adlmidi_numchips, NULL, 2, 1, 8, default_t::wad_yes,
               "TODO: adlmidi_numcards description"),

   DEFAULT_INT("snd_bank", &adlmidi_bank, NULL, 72, 0, BANKS_MAX, default_t::wad_yes,
               "TODO: adlmidi_bank description"),
#endif


   // last entry
   DEFAULT_END()
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
   { "pitched_sounds",    1 }, // pitched sounds should be on
   { "allowmlook",        1 }, // mlook defaults to on
   { "wipetype",          2 }, // use crossfade wipe by default
   { "hud_overlaylayout", 4 }, // use graphical HUD style
   
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
static unsigned int default_hash(defaultfile_t *df, const char *name)
{
   return D_HashTableKey(name) % df->numdefaults;
}

//
// M_LookupDefault
//
// Hashes/looks up defaults in the given defaultfile object by name.
// Returns the default_t object, or NULL if not found.
//
static default_t *M_LookupDefault(defaultfile_t *df, const char *name)
{
   default_t *dp;

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
         I_FatalError(I_ERR_KILL,
                      "M_ApplyGameModeDefaults: override for non-integer default %s\n",
                      def->name);
      }
#endif
      // replace the default value
      if(def)
         def->defaultvalue_i = ovr->defaultvalue;

      ++ovr;
   }
}

//=============================================================================
//
// Methods for different types of default items
//

// 
// Strings
//

// Write help for a string option
static bool M_writeDefaultHelpString(default_t *dp, FILE *f)
{
   return (fprintf(f, "[(\"%s\")]", dp->defaultvalue_s) == EOF);
}

// Write a string option key/value pair
static bool M_writeDefaultString(default_t *dp, FILE *f)
{
   const char *value = 
      dp->modified ? dp->orig_default_s : *(const char **)dp->location;

   return (fprintf(f, "%-25s \"%s\"\n", dp->name, value) == EOF);
}

// Set the value of a string option
static void M_setDefaultValueString(default_t *dp, void *value, bool wad)
{
   const char *strparm = (const char *)value;

   if(wad && !dp->modified)                        // Modified by wad
   {                                               // First time modified
      dp->modified = 1;                            // Mark it as modified
      dp->orig_default_s = *(char **)dp->location; // Save original default
   }
   else
      efree(*(char **)dp->location);               // Free old value

   *(char **)dp->location = estrdup(strparm);      // Change default value

   if(dp->current)                                // Current value
   {
      efree(*(char **)dp->current);                // Free old value
      *(char **)dp->current = estrdup(strparm);    // Change current value
   }
}

// Read a string option and set it
static bool M_readDefaultString(default_t *dp, char *src, bool wad)
{
   int len = static_cast<int>(strlen(src) - 1);

   while(ectype::isSpace(src[len]))
      len--;

   if(src[len] == '"')
      len--;

   src[len+1] = 0;

   dp->methods->setValue(dp, src+1, wad);

   return false;
}

// Set to default value
static void M_setDefaultString(default_t *dp)
{
   // phares 4/13/98:
   // provide default strings with their own malloced memory so that when
   // we leave this routine, that's what we're dealing with whether there
   // was a config file or not, and whether there were chat definitions
   // in it or not. This provides consistency later on when/if we need to
   // edit these strings (i.e. chat macros in the Chat Strings Setup screen).

   *(char **)dp->location = estrdup(dp->defaultvalue_s);
}

// Test if a string default matches the given cvar
static bool M_checkCVarString(default_t *dp, variable_t *var)
{
   // config strings only match string and chararray cvar types
   if(var->type != vt_string && var->type != vt_chararray)
      return false;

   // FIXME: this test may not work for vt_chararray (* vs **)

   // test the pointer
   return (dp->location == var->variable || dp->location == var->v_default);
}

static void M_getDefaultString(default_t *dp, void *dest)
{
   *(const char **)dest = dp->defaultvalue_s;
}

//
// Integers
//

// Write help for an integer option
static bool M_writeDefaultHelpInt(default_t *dp, FILE *f)
{
   bool written = false;

   if(dp->limit.min == UL)
   {
      if(dp->limit.max == UL)
         written = (fprintf(f, "[?-?(%d)]", dp->defaultvalue_i) == EOF);
      else 
      {
         written = (fprintf(f, "[?-%d(%d)]", dp->limit.max, 
                            dp->defaultvalue_i) == EOF);
      }
   }
   else if(dp->limit.max == UL)
   {
      written = (fprintf(f, "[%d-?(%d)]", dp->limit.min, 
                         dp->defaultvalue_i) == EOF);
   }
   else
   {
      written = (fprintf(f, "[%d-%d(%d)]", dp->limit.min, dp->limit.max, 
                         dp->defaultvalue_i) == EOF);
   }

   return written;
}

// Write an integer key/value pair
static bool M_writeDefaultInt(default_t *dp, FILE *f)
{
   int value = dp->modified ? dp->orig_default_i : *(int *)dp->location;

   return (fprintf(f, "%-25s %5i\n", dp->name,
                   strncmp(dp->name, "key_", 4) ? value : 
                   I_DoomCode2ScanCode(value)) == EOF);
}

// Set the value of an integer option
static void M_setDefaultValueInt(default_t *dp, void *value, bool wad)
{
   int parm = *(int *)value;

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

// Read the value of an integer option and set it to the default
static bool M_readDefaultInt(default_t *dp, char *src, bool wad)
{
   int parm = 0;

   if(sscanf(src, "%i", &parm) != 1)
      return true;                         // Not A Number

   if(!strncmp(dp->name, "key_", 4))    // killough
      parm = I_ScanCode2DoomCode(parm);

   // call setValue method through method table rather than directly
   // (could allow redirection in the future)
   dp->methods->setValue(dp, &parm, wad);

   return false;
}

// Set to default value
static void M_setDefaultInt(default_t *dp)
{
   *(int *)dp->location = dp->defaultvalue_i;
}

// Test if an integer default matches the given cvar
static bool M_checkCVarInt(default_t *dp, variable_t *var)
{
   if(var->type != vt_int)
      return false;

   return (dp->location == var->variable || dp->location == var->v_default);
}

static void M_getDefaultInt(default_t *dp, void *dest)
{
   *(int *)dest = dp->defaultvalue_i;
}

//
// Floats
//

// Write help for a float option
static bool M_writeDefaultHelpFloat(default_t *dp, FILE *f)
{
   bool written = false;

   if(dp->limit.min == UL)
   {
      if(dp->limit.max == UL)
         written = (fprintf(f, "[?-?](%g)]", dp->defaultvalue_f) == EOF);
      else
      {
         written = (fprintf(f, "[?-%g(%g)]", (double)dp->limit.max / 100.0,
                            dp->defaultvalue_f) == EOF);
      }
   }
   else if(dp->limit.max == UL)
   {
      written = (fprintf(f, "[%g-?(%g)]", (double)dp->limit.min / 100.0,
                         dp->defaultvalue_f) == EOF);
   }
   else
   {
      written = (fprintf(f, "[%g-%g(%g)]",
                         (double)dp->limit.min / 100.0,
                         (double)dp->limit.max / 100.0,
                         dp->defaultvalue_f) == EOF);
   }

   return written;
}

// Write a key/value pair for a float option
static bool M_writeDefaultFloat(default_t *dp, FILE *f)
{
   double value = dp->modified ? dp->orig_default_f : *(double *)dp->location;

   return (fprintf(f, "%-25s %#g\n", dp->name, value) == EOF);
}

// Set the value of a float option
static void M_setDefaultValueFloat(default_t *dp, void *value, bool wad)
{
   double tmp = *(double *)value;

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

// Read the value of a float option from a string and set it
static bool M_readDefaultFloat(default_t *dp, char *src, bool wad)
{
   double tmp;

   if(sscanf(src, "%lg", &tmp) != 1)
      return true;                       // Not A Number

   dp->methods->setValue(dp, &tmp, wad);

   return false;
}

// Set to default value
static void M_setDefaultFloat(default_t *dp)
{
   *(double *)dp->location = dp->defaultvalue_f;
}

// Test if a float default matches the given cvar
static bool M_checkCVarFloat(default_t *dp, variable_t *var)
{
   if(var->type != vt_float)
      return false;

   return (dp->location == var->variable || dp->location == var->v_default);
}

static void M_getDefaultFloat(default_t *dp, void *dest)
{
   *(double *)dest = dp->defaultvalue_f;
}

//
// Booleans
//

// Write help for a bool option
static bool M_writeDefaultHelpBool(default_t *dp, FILE *f)
{
   return (fprintf(f, "[0-1(%d)]", !!dp->defaultvalue_b) == EOF);
}

// Write a key/value pair for a bool option
static bool M_writeDefaultBool(default_t *dp, FILE *f)
{
   bool value = dp->modified ? dp->orig_default_b : *(bool *)dp->location;

   return (fprintf(f, "%-25s %5i\n", dp->name, !!value) == EOF);
}

// Sets the value of a bool option
static void M_setDefaultValueBool(default_t *dp, void *value, bool wad)
{
   bool parm = *(bool *)value;
   if(wad)
   {
      if(!dp->modified) // First time it's modified by wad
      {
         dp->modified = 1;                               // Mark it as modified
         dp->orig_default_b = *(bool *)dp->location;  // Save original default
      }
      if(dp->current)            // Change current value
         *(bool *)dp->current = !!parm;
   }
   *(bool *)dp->location = !!parm;  // Change default
}

// Reads the value of a bool option from a string and sets it
static bool M_readDefaultBool(default_t *dp, char *src, bool wad)
{
   int parm;

   if(sscanf(src, "%i", &parm) != 1)
      return true;                       // Not A Number

   dp->methods->setValue(dp, &parm, wad);

   return false;
}

// Set to default value
static void M_setDefaultBool(default_t *dp)
{
   *(bool *)dp->location = dp->defaultvalue_b;
}

// Test if a bool default matches the given cvar
static bool M_checkCVarBool(default_t *dp, variable_t *var)
{
   if(var->type != vt_toggle)
      return false;

   return (dp->location == var->variable || dp->location == var->v_default);
}

static void M_getDefaultBool(default_t *dp, void *dest)
{
   *(bool *)dest = dp->defaultvalue_b;
}

//
// Interface objects for defaults
//
static default_i defaultInterfaces[] =
{
   // dt_integer
   { 
      M_writeDefaultHelpInt,
      M_writeDefaultInt,
      M_setDefaultValueInt,
      M_readDefaultInt,
      M_setDefaultInt,
      M_checkCVarInt,
      M_getDefaultInt
   },
   // dt_string
   { 
      M_writeDefaultHelpString,
      M_writeDefaultString,
      M_setDefaultValueString,
      M_readDefaultString,
      M_setDefaultString,
      M_checkCVarString,
      M_getDefaultString
   },
   // dt_float
   { 
      M_writeDefaultHelpFloat,
      M_writeDefaultFloat,
      M_setDefaultValueFloat,
      M_readDefaultFloat,
      M_setDefaultFloat,
      M_checkCVarFloat,
      M_getDefaultFloat
   },
   // dt_boolean
   { 
      M_writeDefaultHelpBool,
      M_writeDefaultBool,
      M_setDefaultValueBool,
      M_readDefaultBool,
      M_setDefaultBool,
      M_checkCVarBool,
      M_getDefaultBool
   },
};

//
// M_populateDefaultMethods
//
// Method population for default objects
//
static void M_populateDefaultMethods(defaultfile_t *df)
{
   default_t *dp;

   for(dp = df->defaults; dp->name; dp++)
      dp->methods = &defaultInterfaces[dp->type];
}

//=============================================================================
//
// Default File Writing
//

//
// M_defaultFileWriteError
//
// Call this when a fatal error occurs writing the configuration file.
//
static void M_defaultFileWriteError(defaultfile_t *df, const char *tmpfile)
{
   // haleyjd 01/29/11: Why was this fatal? just print the message.
   printf("Warning: could not write defaults to %s: %s\n%s left unchanged\n",
          tmpfile, errno ? strerror(errno) : "(Unknown Error)", df->fileName);
}

//
// M_SaveDefaultFile
//
void M_SaveDefaultFile(defaultfile_t *df)
{
   qstring tmpfile; //char *tmpfile = NULL;
   default_t *dp;
   unsigned int blanks;
   FILE *f;

   // killough 10/98: for when exiting early
   if(!df->loaded || !df->fileName)
      return;

   //len = M_StringAlloca(&tmpfile, 2, 1, userpath, "/tmpetern.cfg");

   tmpfile = userpath;
   tmpfile.pathConcatenate("tmpetern.cfg");
   //psnprintf(tmpfile, len, "%s/tmpetern.cfg", userpath);
   //M_NormalizeSlashes(tmpfile);

   errno = 0;
   if(!(f = fopen(tmpfile.constPtr(), "w")))  // killough 9/21/98
   {
      M_defaultFileWriteError(df, tmpfile.constPtr());
      return;
   }

   // 3/3/98 explain format of file
   // killough 10/98: use executable's name

   if(config_help &&
      fprintf(f,
              ";eternity.cfg format:\n"
              ";[min-max(default)] description of variable\n"
              ";* at end indicates variable is settable in wads\n"
              ";variable   value\n\n") == EOF)
   {
      M_defaultFileWriteError(df, tmpfile.constPtr());
      fclose(f);
      return;
   }

   // killough 10/98: output comment lines which were read in during input

   for(blanks = 1, dp = df->defaults; ; dp++, blanks = 0)
   {
      int brackets = 0;

      // If we still haven't seen any blanks,
      // Output a blank line for separation

      if(!blanks && putc('\n',f) == EOF)
      {
         M_defaultFileWriteError(df, tmpfile.constPtr());
         fclose(f);
         return;
      }

      if(!dp->name)      // If we're at end of defaults table, exit loop
         break;

      //jff 3/3/98 output help string
      //
      // killough 10/98:
      // Don't output config help if any [ lines appeared before this one.
      // Make default values, and numeric range output, automatic.

      if(config_help && !brackets)
      {
         if(dp->methods->writeHelp(dp, f) ||
            fprintf(f, " %s %s\n", dp->help, dp->wad_allowed ? "*" : "") == EOF)
         {
            M_defaultFileWriteError(df, tmpfile.constPtr());
            fclose(f);
            return;         
         }
      }

      // killough 11/98:
      // Write out original default if .wad file has modified the default

      //jff 4/10/98 kill super-hack on pointer value
      // killough 3/6/98:
      // use spaces instead of tabs for uniform justification

      if(dp->methods->writeOpt(dp, f))
      {
         M_defaultFileWriteError(df, tmpfile.constPtr());
         fclose(f);
         return;
      }
   }

   if(fclose(f) == EOF)
   {
      M_defaultFileWriteError(df, tmpfile.constPtr());
      return;
   }

   remove(df->fileName);

   if(rename(tmpfile.constPtr(), df->fileName))
   {
      // haleyjd 01/29/11: No error here, just print the message
      printf("Warning: could not write defaults to %s: %s\n", df->fileName,
              errno ? strerror(errno) : "(Unknown Error)");
   }
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
bool M_ParseOption(defaultfile_t *df, const char *p, bool wad)
{
   char name[80], strparm[100];
   default_t *dp;
   
   while(ectype::isSpace(*p))  // killough 10/98: skip leading whitespace
      p++;

   //jff 3/3/98 skip lines not starting with an alphanum
   // killough 10/98: move to be made part of main test, add comment-handling

   if(sscanf(p, "%79s %99[^\n]", name, strparm) != 2 || !ectype::isAlnum(*name) ||
      !(dp = M_LookupDefault(df, name)) || 
      (*strparm == '"') == (dp->type != dt_string) ||
      (wad && !dp->wad_allowed))
   {
      return true;
   }

   return dp->methods->readOpt(dp, strparm, wad); // Success (false) or failure (true)
}

//
// M_LoadOptions()
//
// killough 11/98:
// This function is used to load the OPTIONS lump.
// It allows wads to change game options.
//
void M_LoadOptions()
{
   int lump;
   
   if((lump = W_CheckNumForName("OPTIONS")) != -1)
   {
      int size = W_LumpLength(lump), buflen = 0;
      char *buf = NULL, *p, *options = p = (char *)(wGlobalDir.cacheLumpNum(lump, PU_STATIC));
      while (size > 0)
      {
         int len = 0;
         while(len < size && p[len++] && p[len-1] != '\n');
         if(len >= buflen)
         {
            buflen = len + 1;
            buf = erealloc(char *, buf, buflen);
         }
         strncpy(buf, p, len)[len] = 0;
         p += len;
         size -= len;
         M_ParseOption(&maindefaults, buf, true);
      }
      efree(buf);
      Z_ChangeTag(options, PU_CACHE);
   }

   //  MN_ResetMenu();       // reset menu in case of change

   // haleyjd 05/05/11: Believe it or not this was handled through the menu
   // system in MBF, as the above commented-out line left over from SMMU
   // hints. M_ResetMenu would execute action callbacks on any menu item
   // that needed updating, and the action for general_translucency in MBF
   // was altered so that it would refresh the tranmap.
   //
   // Without this here, the game will crash if an OPTIONS lump changes the
   // value of general_translucency.
   // FIXME: This is not extensible and should be considered a temporary hack!
   // Instead there should be a generalized post-update action for options.
   R_ResetTrans();
}

//
// M_LoadDefaultFile
//
void M_LoadDefaultFile(defaultfile_t *df)
{
   default_t *dp;
   FILE *f;

   // haleyjd 07/03/10: set default object methods for easy calls
   M_populateDefaultMethods(df);

   // set everything to base values
   for(dp = df->defaults; dp->name; dp++)
      dp->methods->setDefault(dp);

   M_NormalizeSlashes(df->fileName);
   
   // read the file in, overriding any set defaults
   //
   // killough 9/21/98: Print warning if file missing, and use fgets for reading

   if(!(f = fopen(df->fileName, "r")))
      printf("Warning: Cannot read %s -- using built-in defaults\n", df->fileName);
   else
   {
      int skipblanks = 1, line = 0;
      char s[256];

      while(fgets(s, sizeof s, f))
      {
         if(!M_ParseOption(df, s, false))
            line++;       // Line numbers
         else
         {             // Remember comment lines
            const char *p = s;
            
            while(ectype::isSpace(*p))  // killough 10/98: skip leading whitespace
               p++;

            if(*p)                // If this is not a blank line,
            {
               skipblanks = 0;    // stop skipping blanks.
            }
            else
            {
               if(skipblanks)      // If we are skipping blanks, skip line
                  continue;
               else            // Skip multiple blanks, but remember this one
               {
                  skipblanks = 1;
                  p = "\n";
               }
            }
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
void M_LoadDefaults()
{
   defaultfile_t *df = &maindefaults;

   // check for a custom default file   
   if(!df->fileName)
   {
      int p;
      if((p = M_CheckParm("-config")) && p < myargc - 1)
         printf(" default file: %s\n", df->fileName = estrdup(myargv[p + 1]));
      else
         df->fileName = estrdup(basedefault);
   }

   // haleyjd 06/30/09: apply gamemode defaults first
   M_ApplyGameModeDefaults(df);

   M_LoadDefaultFile(df);
}

//
// M_findCVarInDefaults
//
// haleyjd 07/04/10: static subroutine for M_FindDefaultForCVar that looks
// for a match in a single set of defaults.
//
static default_t *M_findCVarInDefaults(default_t *defaultset, variable_t *var)
{
   default_t *dp, *ret = NULL;

   for(dp = defaultset; dp->name; dp++)
   {
      if(dp->methods->checkCVar(dp, var))
      {
         ret = dp;
         break;
      }
   }

   return ret;
}

//
// M_FindDefaultForCVar
//
// haleyjd 07/04/10: Given a cvar, this function will try to find a matching
// default object amongst the various default sets. This is a stopgap 
// implementation to create some coherence between the console and config
// systems.
//
default_t *M_FindDefaultForCVar(variable_t *var)
{
   default_t *ret = NULL;

   // check normal defaults array first, then system defaults
   if(!(ret = M_findCVarInDefaults(defaults, var)))
      ret = M_findCVarInDefaults(M_GetSysDefaults(), var);

   return ret;
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

