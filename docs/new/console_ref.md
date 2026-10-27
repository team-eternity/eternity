------------------------------------------------------------------------

**The Eternity Engine v3.35.90 Console Reference \-- 09/02/08**

------------------------------------------------------------------------

[Return to the Eternity Engine Page](../etcengine.html)

This reference contains information on useful console commands and
variables that work with the Eternity console, as well as pointers on
console command syntax.

- [**Table of Contents**]{##contents}
  - [Notes on Special Command Syntax](#syntax)
  - [Notes on Notation](#notate)
  - [Console Commands](#commands)
    - [Core Commands](#cmdcore)\
      flood, delay, alias, cmdlist, quote, echo, dumplog, openlog,
      closelog, dir
    - [Menus](#cmdmenus)\
      mn_clearmenus, mn_newgame, mn_quit, mn_episode, mn_prevmenu,
      newgame, mn_weaponkeys, mn_keybindings, mn_demos, mn_weapons,
      mn_loadwad, mn_status, mn_hud, mn_mouse, mn_sound, mn_vidmode,
      mn_particle, mn_video, mn_endgame, mn_options, mn_savegame,
      mn_load, mn_player, mn_envkeys, credits, help, mn_enemies,
      mn_compat, mn_automap, quickload, quicksave, mn_joymenu,
      mn_hnewgame, mn_hepis, skinviewer, mn_dynamenu, mn_selectwad,
      mn_search, mn_menus
    - [Keybindings](#cmdkeybindings)\
      listactions, bind, unbindall, unbind, listkeys, bindings
    - [Networking](#cmdnetworking)\
      disconnect, playerinfo, frags, kick, say
    - [Cheats](#cmdcheats)\
      god, noclip, nuke, infammo, mdk, mdkbomb, spacejump, viles,
      vilehit, banish
    - [Video Options](#cmdvideo)\
      v_modelist, animshot, screenshot
    - [Sound Options](#cmdsound)\
      s_playmusic
    - [Player Options](#cmdplayer)\
      listskins, listwads
    - [Demos](#cmddemos)\
      timedemo, playdemo, stopdemo
    - [Gameplay](#cmdgameplay)\
      pause, i_error, spawn, endgame, starttitle, quit, map, kill,
      addfile, exitlevel, defdmflags, summon, give, whistle, p_linespec
    - [EDF- and ExtraData-Related Commands](#cmdedf)\
      e_dumpthings, e_thingtype, e_dumpitems, e_listmapthings,
      e_mapthing, e_listlinedefs, e_linedef
    - [Small Scripting Engine](#cmdsmall)\
      a_running, a_execv, a_execi
  - [Console Variables](#variables)
    - [Console](#varconsole)\
      c_speed, c_height, textmode_startup
    - [Networking variables](#varnetworking)\
      com
    - [Gametype / Deathmatch](#vargametype)\
      skill, nomonsters, respawn, fraglimit, bfgtype, gametype,
      weapspeed, timelimit, fast, turbo, startlevel, dmflags
    - [Demo options](#vardemo)\
      cooldemo, demo_insurance
    - [Player variables](#varplayer)\
      name, skin, colour, walkcam, chasecam, chasecam_height,
      chasecam_dist, chasecam_speed, numhelpers
    - [Input options](#varinput)\
      alwaysmlook, invertmouse, sens_horiz, sens_vert, smooth_turning,
      use_joystick, use_mouse, joysens_x, joysens_y
    - [Environment / Physics variables](#varenvironment)\
      autoaim, allowmlook, bobbing, pushers, nukage, varfriction,
      recoil, bfglook, p_markunknowns
    - [AI variables](#varai)\
      mon_infight, mon_avoid, mon_climb, mon_friction, mon_remember,
      mon_backing, mon_helpfriends, mon_distfriend
    - [Heads-up System variables](#varheadsup)\
      hu_overlay, hu_hidesecrets, hu_obituaries, hu_obitcolor,
      hu_crosshair, hu_crosshair_hilite, hu_showvpo, hu_vpo_threshold,
      hu_messages, hu_messagecolor, hu_messagelines, hu_messagescroll,
      hu_messagetime, hu_showtime, hu_showcoords, hu_timecolor,
      hu_levelnamecolor, hu_coordscolor, show_scores, map_coords
    - [Automap variables](#varautomap)\
      mapcolor_rkey, mapcolor_bkey, mapcolor_ykey, mapcolor_rdor,
      mapcolor_bdor, mapcolor_ydor, mapcolor_frnd, mapcolor_wall,
      mapcolor_clsd, mapcolor_tele, mapcolor_secr, mapcolor_exit,
      mapcolor_unsn, mapcolor_sprt, mapcolor_hair, mapcolor_sngl,
      mapcolor_back, mapcolor_fchg, am_drawnodelines
    - [Status Bar variables](#varstatus)\
      ammo_red, ammo_yellow, health_red, health_yellow, health_green,
      armor_red, armor_yellow, armor_green, st_rednum, st_singlekey,
      st_graypct
    - [Menu variables](#varmenus)\
      use_traditional_menu, wad_directory, mn_toggleisback,
      mn_searchstr, mn_start_mapname
    - [Rendering variables](#varrendering)\
      r_showgun, r_showhom, r_blockmap, r_homflash, r_planeview, r_zoom,
      r_stretchsky, r_precache, r_trans, r_tranpct, r_swirl, gamma,
      draw_particles, bloodsplattype, bulletpufftype, rocket_trails,
      grenade_trails, lefthanded, r_ptcltrans, bfg_cloud
    - [Particle Events](#varparticle)\
      pevt_rexpl, pevt_bfgexpl
    - [Video variables](#varvideo)\
      v_diskicon, v_retrace, v_mode, v_ticker, shot_type, shot_gamma,
      screensize, wipewait
    - [Sound options](#varsound)\
      detect_voices, snd_card, mus_card, sfx_volume, music_volume,
      s_flippan, s_precache, s_pitched, snd_channels, snd_spcpreamp,
      snd_spcbassboost
    - [Chat Macros](#varchat)\
      chatmacro0, chatmacro1, chatmacro2, chatmacro3, chatmacro4,
      chatmacro5, chatmacro6, chatmacro7, chatmacro8, chatmacro9
    - [Weapon Preferences](#varweapon)\
      weappref_0, weappref_1, weappref_2, weappref_3, weappref_4,
      weappref_5, weappref_6, weappref_7, weappref_8
    - [DOOM Compatibility Vector](#vardoomcomp)\
      comp_telefrag, comp_dropoff, comp_vile, comp_pain, comp_skull,
      comp_blazing, comp_doorlight, comp_model, comp_god, comp_falloff,
      comp_floors, comp_skymap, comp_pursuit, comp_doorstuck,
      comp_staylift, comp_zombie, comp_stairs, comp_infcheat,
      comp_zerotags, comp_terrain, comp_respawnfix, comp_fallingdmg,
      comp_soul, comp_overunder, comp_planeshoot
    - [Misc. Configuration](#varmisc)\
      use_startmap, i_gamespeed, i_ledsoff, startonnewmap, i_waitatexit,
      i_showendoom, i_endoomdelay, i_grabmouse
    - [Constants](#varconstants)\
      rngseed, creator, version, ver_date, ver_name, ver_time

[]{#syntax}

------------------------------------------------------------------------

**Notes on Special Command Syntax**

------------------------------------------------------------------------

Commands which take parameters may accept numbers, strings, or members
of a predefined set of values.


      ex:
      $ delay 50
      $ spawn 249 1
      $ name Quasar
      $ hu_overlay distributed

If string arguments contain whitespace, they must be in quotations.
Quotations around strings are otherwise optional.


      ex:
      $ echo "This has to be in quotes"
      $ i_error "R_FindVisplane: no more visplanes!"

Multiple commands can be run from one prompt by separating them with a
semicolon.


      ex:
      $ delay; kill

Variables of type integer, named-value, on / off, and yes / no can be
affected by the following operators:

- \+\
  Affixed to the end of the variable, it will increment it to the next
  valid value, but no higher than the maximum allowed.


          ex:
          $ screensize +
          

- \-\
  Affixed to the end of the variable, it will decrement it to the next
  lower valid value, but no lower than the minimum allowed.


          ex:
          $ screensize -
          

- /\
  Toggles through the variable values from minimum to maximum, wrapping
  around to the minimum value again when it is maxed out. This is most
  useful with two-state yes/no and on/off variables.


          ex:
          $ hu_overlay /
          

[Return to Table of Contents](#contents)

[]{##notate}

------------------------------------------------------------------------

**Notes on Notation**

------------------------------------------------------------------------

In the list below, command arguments enclosed in brackets are optional.
Commands typically have some default behavior when the arguments they
expect are not provided, although this is often to simply print usage
information.

An ellipsis (\...) signifies that the command will accept any number of
arguments and will amalgamate them into one continuous string.\
\
[Return to Table of Contents](#contents)

[]{##commands}

------------------------------------------------------------------------

**Console Commands**

------------------------------------------------------------------------

Console commands carry out a particular action when sent to the console.

[]{##cmdcore}

------------------------------------------------------------------------

**Core Commands**

------------------------------------------------------------------------

These commands are for manipulating the console itself.\
\
Synopsis of Commands:


        * flood     * echo
        * delay     * dumplog
        * alias     * openlog
        * cmdlist   * closelog
        * quote         * dir

- flood\
  \
  Writes 300 garbage characters to the console. Usefulness is of
  debate.\
  \

- delay \<amt\>\
  \
  Delays the running of the next console command by either 1, or if
  provided, by amt game tics.\
  \

- alias \<newcmdname\> \<command\>\
  \
  alias alone will list all currently defined aliases. Providing
  newcmdname alone will remove that alias if it exists. Providing
  newcmdname and a valid console command will create \"newcmdname\" as
  an alias to that command, allowing shortcuts.\
  \
  In order to make an alias take parameters, use the special variable
  \"%opt\", as such:


        alias "mycommand" "hu_overlay %opt"
        

- cmdlist \<filter\>\
  \
  Displays all user-visible console commands, one per line. As of
  Eternity Engine v3.31 Delta, this command accepts an optional filter
  parameter that, if provided, should consist of a single character
  between A and Z. The character will be used to filter the command list
  down to only commands beginning with that letter.\
  \

- quote\
  \
  Just for fun, displays a random quote from IRC.\
  \

- echo msg\
  \
  Echoes a string message to the console. As with all console strings,
  the string should be in quotes if it contains whitespace.\
  \

- dumplog filename\
  \
  Dumps the current state of the console message buffer to the specified
  file as ASCII text. The text will be appended to the file if it
  already exists, allowing multiple use of a single log file.\
  \

- openlog filename\
  \
  Opens the specified file in append mode and begins writing any text
  printed to the console into the file. All console messages will be
  logged until either the closelog command is used, or the program
  closes. Note this log file is totally separate from any used with the
  \"dumplog\" command, which is simply for saving the current buffer. If
  a log file is already open, this command does nothing.\
  \

- closelog\
  \
  Closes a console log opened by the openlog command. If no logging is
  currently occuring, this command does nothing. This command doesn\'t
  apply to files used by dumplog, as they are closed immediately, so it
  doesn\'t need to be used with the dumplog command.\
  \

- dir\
  \
  Lists the contents of the current working directory to the console.

[Return to Table of Contents](#contents)

[]{#cmdmenus}

------------------------------------------------------------------------

**Menus**

------------------------------------------------------------------------

These commands display and affect various aspects of the menu system.\
\
Synopsis of Commands:

- mn_clearmenus
- mn_newgame
- mn_quit
- mn_episode
- mn_prevmenu
- newgame
- mn_weaponkeys
- mn_keybindings
- mn_demos
- mn_weapons
- mn_loadwad
- mn_status
- mn_hud
- mn_mouse
- mn_sound
- mn_vidmode
- mn_particle
- mn_video
- mn_endgame
- mn_options
- mn_savegame
- mn_load
- mn_player
- mn_envkeys
- credits
- help
- mn_enemies
- mn_compat
- mn_automap
- quickload
- quicksave
- mn_joymenu
- mn_hnewgame
- mn_hepis
- skinviewer
- mn_dynamenu
- mn_selectwad
- mn_search
- mn_menus

<!-- -->

- mn_clearmenus\
  \
  Deactivates any active menu and returns to the game\
  \
- mn_newgame\
  \
  Displays the New Game menu or takes the player to the start map,
  depending on their configuration.\
  \
- mn_quit\
  \
  Displays a prompt asking the user if they want to quit.\
  \
- mn_episode\
  \
  Displays the episode selection screen for Ultimate DOOM.\
  Flags: not in network games\
  \
- mn_prevmenu\
  \
  Returns user to the last menu they viewed, or to the game if the
  present menu is the highest-most in that chain.\
  \
- newgame\
  \
  Clears any active menus and starts a new game.\
  Flags: not in network games\
  \
- mn_weaponkeys\
  \
  Displays the weapon keybinding menu.\
  \
- mn_keybindings\
  \
  Displays primary keybindings menu.\
  \
- mn_demos\
  \
  Displays the Demos menu\
  Flags: not in a network game\
  \
- mn_weapons\
  \
  Displays the weapons preferences menu\
  \
- mn_loadwad\
  \
  Displays the wad/file options menu.\
  \
- mn_status\
  \
  Displays the status bar options menu.\
  \
- mn_hud\
  \
  Displays the heads-up display options menu.\
  \
- mn_mouse\
  \
  Displays the mouse options menu.\
  \
- mn_sound\
  \
  Displays the sound options menu.\
  \
- mn_vidmode\
  \
  Displays the video mode options menu.\
  \
- mn_particle\
  \
  Displays the particle effects options menu.\
  \
- mn_video\
  \
  Displays the video options menu.\
  \
- mn_endgame\
  \
  Prompts the user to end the current game and return to the title
  screen.\
  \
- mn_options\
  \
  Displays the main options menu.\
  \
- mn_savegame\
  \
  Displays the save game menu. Will not display if not in a level.\
  \
- mn_load slotnum\
  \
  Loads the save game from save slot slotnum.\
  \
- mn_loadgame\
  \
  Displays the load game menu.\
  \
- mn_player\
  \
  Displays the player setup menu.\
  \
- mn_envkeys\
  \
  Displays the environment keybindings menu.\
  \
- credits\
  \
  Displays game engine / game credits.\
  \
- help\
  \
  Displays built-in and user-provided help screens. See the BOOM editing
  reference on how to provide up to 99 custom help screens.\
  \
- mn_enemies\
  \
  Displays enemy AI options menu.\
  \
- mn_compat\
  \
  Displays the DOOM compatibility options menu.\
  \
- mn_automap\
  \
  Displays the automap options menu.\
  \
- quickload\
  \
  Quickloads a game from a previously chosen quicksave slot. NOTE: does
  nothing in network games or demos\
  \
- quicksave\
  \
  Quicksaves a game. First time command is used, a slot must be chosen
  normally. Subsequent uses will overwrite the save in that slot.\
  NOTE: does nothing in network games or demos\
  \
- mn_joymenu\
  \
  Brings up the SDL joystick configuration menu. NOTE: This command does
  not exist in the DOS version of Eternity.\
  \
- mn_hnewgame\
  \
  Brings up the Heretic new game menu.\
  \
- mn_hepis\
  \
  Brings up the Heretic episode selection menu.\
  \
- skinviewer\
  \
  Brings up the skin viewer menu widget.\
  \
- mn_dmflags\
  \
  Brings up the deathmatch flags configuration menu.\
  \
- mn_dynamenu menuname\
  \
  Brings up the EDF dynamic menu with the given mnemonic. If no such
  menu exists, an error message will appear in the console.\
  \
- mn_selectwad\
  \
  Brings up the wad selection dialog box. The selected wad cannot be
  loaded if the current gamemode is a shareware game. Flags: not in a
  netgame\
  \
- mn_search\
  \
  Begins or continues a menu search operation. This command uses the
  value of the \"mn_searchstr\" console variable to carry out its
  search.\
  \
- mn_menus\
  \
  Brings up the menu options menu.

[Return to Table of Contents](#contents)

[]{#cmdkeybindings}

------------------------------------------------------------------------

**Keybindings**

------------------------------------------------------------------------

These commands allow manipulation of the dynamic keybinding system.\
\
Synopsis of Commands:


        * listactions   * bindings
        * bind
        * unbindall
        * unbind
        * listkeys

- listactions\
  \
  Displays a list of all bindable actions, aside from console commands.\
  \
- bind keyname \<actionname\>\
  \
  Binds the specified action to the specified key. actionname is a
  string and should be enclosed in quotes if it contains whitespace.
  actionname may specify any valid action or console command. If
  \"actionname\" is not provided, the current binding status of the
  specified key will be displayed.\
  \
- unbindall\
  \
  Releases \*all\* dynamic keybindings. Use with caution!\
  \
- unbind keyname \<bindingclass\>\
  \
  Unbinds the specified key from any action, or from only the action in
  the specified class if the bindingclass parameter is specified. As of
  Eternity Engine v3.31 Delta, keys may have one binding from each
  separate binding class. Binding class numbers are displayed next to
  action names when using the \"bind keyname\" command.\
  \
- listkeys\
  \
  Displays a list of all bindable key names. Not all bindable key codes
  correspond to pressable keys, however. Most pressable keys have
  obvious names which correspond to those on the keyboard.\
  \
- bindings\
  \
  Dumps a list of all active keybindings to the console.

[Return to Table of Contents](#contents)

[]{#cmdnetworking}

------------------------------------------------------------------------

**Networking**

------------------------------------------------------------------------

Synopsis of Commands:


        * disconnect
        * playerinfo
        * frags
        * kick
        * say

- disconnect\
  \
  Disconnects from any ongoing network game and sets full-screen console
  mode.\
  Flags: only in network games\
  \
- playerinfo\
  \
  Displays name information for all connected players.\
  \
- frags\
  \
  Displays the current frag count for all players.\
  \
- kick playernum\
  \
  Kicks the player with the specified player number from the game.
  Player numbers can be retrieved with playerinfo.\
  Flags: server-only\
  \
- say \...\
  \
  Sends all arguments as a single message to all players in a network
  game.\
  Flags: net command

[Return to Table of Contents](#contents)

[]{#cmdcheats}

------------------------------------------------------------------------

**Cheats**

------------------------------------------------------------------------

These commands provide an alternative method of entering some cheat
codes.\
\
Synopsis of Commands:


        * god
        * noclip
        * nuke
        * infammo
        * mdk
        * mdkbomb
        * spacejump
        * viles
        * vilehit
        * banish

- god\
  \
  Toggles god mode (IDDQD) on or off.\
  Flags: not in network games, only in levels\
  \
- noclip\
  \
  Toggles no clipping (IDCLIP) on or off.\
  Flags: not in network games, only in levels\
  \
- nuke\
  \
  Kills all enemies on the level, and if there are no enemies, kills all
  friends on the level (equivalent to KILLEM).\
  Flags: server-only, only in level, net command\
  \
- infammo\
  \
  Toggles infinite ammo (INFSHOTS) on or off.\
  Flags: not in network games, only in levels\
  \
- mdk\
  \
  Fires a tracer from the player that instantly kills any monster the
  player is targetting.\
  Flags: only in level, not in netgames, hidden\
  \
- mdkbomb\
  \
  As above, but fires 60 such tracers in a circle around the player.\
  Flags: only in level, not in network games, hidden\
  \
- spacejump\
  \
  Allows the player to jump any number of times in midair; useful for
  testing linked portals.\
  Flags: only in level, not in network games, hidden\
  \
- viles\
  \
  A special command for DOOM II only. Try it and see what happens :)\
  Flags: only in level, not in network games, hidden\
  \
- vilehit\
  \
  If the player is autoaiming at an enemy, the player will perform an
  Arch-vile attack on that enemy.\
  Flags: only in level, not in network games\
  \
- banish\
  \
  If the player is autoaiming at an enemy, the enemy will be removed
  from the play simulation and its memory will be freed once no other
  objects reference it.\
  Flags: only in level, not in network games

[Return to Table of Contents](#contents)

[]{#cmdvideo}

------------------------------------------------------------------------

**Video Options**

------------------------------------------------------------------------

Synopsis of Commands:


        * v_modelist
        * animshot
        * screenshot

- v_modelist\
  \
  Prints a list of available video modes to the console.\
  \
- animshot numframes\
  \
  Takes the given number of consecutive screenshots for the purpose of
  making a crude animation. Would be useful for animated GIFs or AVI
  movies. This command will make the game run very slow, however, and
  will eat up disk space quickly, so use it with care.\
  \
- screenshot\
  \
  Takes a single screenshot. Bind this command to a key in order to take
  a shot without the console in the way.

[Return to Table of Contents](#contents)

[]{#cmdsound}

------------------------------------------------------------------------

**Sound Options**

------------------------------------------------------------------------

Synopsis of Commands:


        * s_playmusic

- s_playmusic name\
  \
  Plays the given lump as music, if that lump can be found in the music
  hash table. Only lumps listed in the internal music table, or new
  lumps with names beginning with \"D\_\" in DOOM or \"MUS\_\" in
  Heretic are available.

[Return to Table of Contents](#contents)

[]{#cmdplayer}

------------------------------------------------------------------------

**Player Options**

------------------------------------------------------------------------

Synopsis of Commands:


        * listskins
        * listwads

- listskins\
  \
  Lists names of all available player skins.\
  \
- listwads\
  \
  Lists all currently loaded WAD files.

[Return to Table of Contents](#contents)

[]{#cmddemos}

------------------------------------------------------------------------

**Demos**

------------------------------------------------------------------------

These commands are related to playing / recording demos. For all of
these commands, the demo specified by demoname must have been added in
either a WAD or on the command-line with the -file option. Demos cannot
be played directly from a file name.\
\
Synopsis of Commands:


        * timedemo
        * playdemo
        * stopdemo

- timedemo demoname showmenu\
  \
  Starts the given demo as a time demo, in which the framerate of the
  game can be measured. If showmenu is set to 1, a meter comparing the
  framerate against that of a \"fast\" machine will be displayed after
  the demo is complete. Otherwise, the framerate will be printed to the
  console as a number.\
  Flags: not in a network game\
  \
- playdemo demoname\
  \
  Plays the given demo normally.\
  Flags: not in a network game\
  \
- stopdemo\
  \
  Stops any currently playing demo and drops to console.\
  Flags: not in a network game

[Return to Table of Contents](#contents)

[]{#cmdgameplay}

------------------------------------------------------------------------

**Gameplay**

------------------------------------------------------------------------

These commands directly affect gameplay in various ways.\
\
Synopsis of Commands:


        * pause       * quit       * defdmflags
        * i_error     * map        * summon
        * spawn       * kill       * give
        * endgame     * addfile    * p_linespec
        * starttitle  * exitlevel

- pause\
  \
  Pauses or unpauses the game.\
  Flags: server-only\
  \

- i_error \...\
  \
  Simulates an internal game engine error. Prints any arguments provided
  as a single string when the game exits.\
  \

- spawn objecttype \<friend\>\
  \
  Spawns a map object of the given internal type (DeHackEd number
  minus 1) in front of the player. If is provided and is a non-zero
  number, the object will be friendly. This command is for developers.\
  Flags: not in a network game, only in levels, hidden\
  \

- endgame\
  \
  Drops to console mode.\
  Flags: not in a network game\
  \

- starttitle\
  \
  Returns the game to the title screen / demo / credits loop.\
  Flags: not in a network game\
  \

- quit\
  \
  Plays a random monster sound and exits the game normally, with no
  prompt.\
  \

- map mapname\
  \
  Transfers game play to the given map. mapname may be the name of a WAD
  file to load, as well as the name of a map header itself.


        Examples:
          map E1M1
          map w00t.wad
        

  Flags: server-only, net command, buffered command\
  \

- kill\
  \
  Causes the player to commit suiceide.\
  Flags: only in level, net command\
  \

- addfile wadfilename\
  \
  Adds the specified wad file at run-time. Some features will not
  function completely as expected when WADs are loaded at run-time, so
  for maximum compatibility always use the command line to load files.\
  Flags: not in network games, buffered command\
  \

- exitlevel\
  \
  Exits the current level. Starting with EE v3.31 beta 1, this command
  will not allow an exit if the player is dead unless the comp_zombie
  variable is set to allow zombie exits.\
  Flags: only in level, server-only, net command, buffered command\
  \

- defdmflags \<mode\>\
  \
  Sets the deathmatch flags variable to its default settings. If no
  argument is provided, dmflags is set to the default for the current
  game type. Otherwise, the \"mode\" parameter indicates what game
  mode\'s defaults to set, with the following values:


        0 = Single-player mode
        1 = Cooperative mode
        2 = Deathmatch mode
        3 = Altdeath mode (deathmatch 2.0, items respawn)
        4 = Trideath mode (DM 3, barrels respawn, players drop backpacks)
        

  Flags: server-only, net command\
  \

- summon thingtypename \<flagslist\> \<mode\>\
  \
  Allows spawning of things via their EDF thing type name. An EDF/BEX
  flag list can optionally be provided, with the flags separated by
  commas only. By default, this will cause the listed flags to be added
  to the thing along with its normal flags. By specifying the third
  parameter as \"set\" or \"remove\", it is possible to set the thing\'s
  flags value to only the listed flags, or to remove the listed flags if
  the thingtype has them set by default.\
  Example to spawn a friendly thing:\
  \$ summon doomimp friend\
  Flags: not in a network game, only in levels, hidden\
  \

- give thingtypename \<num\>\
  \
  Spawns 1, or \"num\" if provided, of the object with this EDF thing
  type name on the player and causes the player to collect it/them. If
  the requested thing type is not collectable, it will not be spawned.
  In addition, if a collectable item is not picked up by the player, it
  will be removed immediately.\
  Flags: not in a network game, only in levels, hidden\
  \

- whistle thingtypename\
  \
  If there is a live, friendly thing of the given EDF type, the first
  such thing found on the map will be teleported in front of the player.
  If the thing doesn\'t fit where the player is trying to teleport it,
  it will not be teleported. It will also not be teleported across
  blocking lines.\
  Flags: not in a network game, only in levels\
  \

- p_linespec name \<arg\> \<arg\> \<arg\> \<arg\> \<arg\>\
  \
  Executes the named Eternity Engine parameterized line special (as
  documented in editref.html) with the given parameters. The line
  special is restricted to behaving as though it has been invoked
  without any linedef to reference, and thus certain effects such as
  sector special transfers will not occur even if specified by the
  special type or arguments.\
  Flags: not in a network game, only in levels.

[Return to Table of Contents](#contents)

[]{#cmdedf}

------------------------------------------------------------------------

**EDF- and ExtraData-Related Commands**

------------------------------------------------------------------------

These commands are related to the Eternity Definition Files (EDF)
system. They expose information which may be useful for editing or for
use with other console commands.\
\
Synopsis of Commands:


        * e_dumpthings      * e_listlinedefs
        * e_thingtype       * e_linedef
        * e_dumpitems
        * e_listmapthings
        * e_mapthing

- e_dumpthings\
  \
  Lists all EDF thing type mnemonics along with the DeHackEd numbers and
  doomednums of the corresponding types.\
  \
  \
- e_thingtype thingtypename\
  \
  Lists verbose information on an EDF thingtype with the given mnemonic.
  If no such thingtype is defined, an error message will be given.\
  \
- e_dumpitems\
  \
  Lists all EDF thing type mnemonics and DeHackEd/DoomEd numbers for
  thing types which are collectable items. Useful in conjunction with
  the give command.\
  \
- e_listmapthings\
  \
  Lists all mapthing definitions from the current map\'s ExtraData. If
  no mapthings are defined or no ExtraData exists for the current map,
  an appropriate error message will be given.\
  \
- e_mapthing recordnum\
  \
  Lists verbose information on an ExtraData mapthing record with the
  given numeric record number. If no such mapthing is defined or no
  ExtraData exists for the current map, an appropriate error message
  will be given.\
  \
- e_listlinedefs\
  \
  Lists all linedef definitions from the current map\'s ExtraData. If no
  linedefs are defined or no ExtraData exists for the current map, an
  appropriate error message will be given.\
  \
- e_linedef recordnum\
  \
  Lists verbose information on an ExtraData linedef record with the
  given numeric record number. If no such linedef is defined or no
  ExtraData exists for the current map, an appropriate error message
  will be given.

[Return to Table of Contents](#contents)

[]{#cmdsmall}

------------------------------------------------------------------------

**Small Scripting Engine**

------------------------------------------------------------------------

These commands are related to the Small scripting engine.\
\
Synopsis of Commands:


        * a_running
        * a_execv
        * a_execi

- a_running\
  \
  Prints debug information about the currently running script callbacks.
  The data listed is as follows: internal script number, internal
  callback type number, and callback data (wait time or sector tag).\
  \
- a_execv vm scriptname\
  \
  Executes a public Small script, passing no parameters (use this for
  scripts declared with an empty parameter list). The \"vm\" parameter
  should be 0 to indicate the Gamescript VM, or 1 to indicate the
  Levelscript VM. If the indicated VM isn\'t valid, an error message
  will be displayed. The indicated script will run under the INVOKE_CCMD
  invocation model.\
  \
- a_execi vm scriptname \... Executes a public Small script, passing one
  or more integer parameters provided to this command. You cannot pass
  string values to Small functions using this command. The number of
  parameters passed should match the prototype of the Small function
  being invoked, or an error may occur. The \"vm\" parameter should be 0
  to indicate the Gamescript VM, or 1 to indicate the Levelscript VM. If
  the indicated VM isn\'t valid, an error message will be displayed. The
  indicated script will run under the INVOKE_CCMD invocation model.

[Return to Table of Contents](#contents)

[]{#variables}

------------------------------------------------------------------------

**Console Variables**

------------------------------------------------------------------------

Variables differ from commands in that they are assigned values rather
than performing actions directly.

[]{#varconsole}

------------------------------------------------------------------------

**Console**

------------------------------------------------------------------------

Synopsis:


        * c_speed
        * c_height
        * textmode_startup

- c_speed\
  type: integer\
  value range: 1 - 200\
  value = speed at which console descends/recedes\
  \
- c_height\
  type: integer\
  value range: 20 - 200\
  value = normal height of the console in pixels\
  \
- textmode_startup\
  type: on / off\
  on = game starts in text mode like the original DOOM\
  off = game starts in console mode with graphics\

[Return to Table of Contents](#contents)

[]{#varnetworking}

------------------------------------------------------------------------

**Networking variables**

------------------------------------------------------------------------

Synopsis:


        * com

- com\
  type: integer\
  value range: 1 - 4\
  value = com port to use for serial connections\

[Return to Table of Contents](#contents)

[]{#vargametype}

------------------------------------------------------------------------

**Game type / Deathmatch options**

------------------------------------------------------------------------

Synopsis:


        * skill       * gametype     * startlevel
        * nomonsters  * weapspeed    * dmflags
        * respawn     * timelimit
        * fraglimit   * fast
        * bfgtype     * turbo

- skill\
  type: named values\
  values: \"im too young to die\", \"hey not too rough\", \"hurt me
  plenty\", \"ultra violence\", \"nightmare\"\
  value = game skill level\
  Flags: netvar, server-only\
  \

- nomonsters\
  type: on / off\
  on = monsters will not be spawned at level start\
  off = monsters will spawn normally\
  Flags: netvar, server-only\
  \

- respawn\
  type: on / off\
  on = normal monsters will respawn shortly after death\
  off = normal monsters do not respawn\
  Flags: netvar, server-only\
  \

- fraglimit\
  type: integer\
  value range: 0 - 100\
  value = max number of frags that can be achieved before the level
  exits\
  Flags: netvar, server-only\
  \

- bfgtype\
  type: named values\
  values: bfg9000, classic, bfg11k, bouncing, \"plasma burst\"\
  value = type of BFG which all players will use\
  Flags: netvar, server-only\
  \

- gametype\
  type: named values\
  values: single, coop, deathmatch\
  value = determines rules used for gameplay; cannot set single if more
  than one player is present\
  Flags: netvar, server-only\
  \

- weapspeed\
  type: integer\
  value range: 1 - 200\
  value = speed at which weapon changes occur\
  Flags: netvar, server-only\
  \

- timelimit\
  type: integer\
  value range: 0 - 100\
  value = max number of minutes a level can be played before moving on
  to next level\
  Flags: netvar, server-only\
  \

- fast\
  type: on / off\
  on = monsters move faster and are more aggressive\
  off = monsters move normally\
  Flags: netvar, server-only\
  \

- turbo\
  type: integer\
  value range: 10 - 400\
  value = turbo scale factor for walking/running\
  \

- startlevel\
  type: string\
  value = name of first level to start new games\
  \

- dmflags\
  type: integer\
  value range: 0 - 2147483647\
  value = deathmatch flags bitmask \-- add the values below together to
  set individual flags; values are as follows:\


          Dec. Value   Hex          Meaning
          ----------------------------------------------------
                   1   0x00000001   Items respawn
                   2   0x00000002   Weapons stay
                   4   0x00000004   Barrels respawn
                   8   0x00000008   Players drop backpacks
                  16   0x00000010   Super powerups respawn  
          

  Flags: netvar, server-only

[Return to Table of Contents](#contents)

[]{#vardemo}

------------------------------------------------------------------------

**Demo options**

------------------------------------------------------------------------

Synopsis:


        * cooldemo
        * demo_insurance

- cooldemo\
  type: on / off\
  on = if the level has intermission cameras, the view will move between
  them randomly during the demo\
  off = demos are normal even in levels with intermission cameras\
  \
- demo_insurance\
  type: named values\
  values: off, on, \"when recording\"\
  value = determines whether or not extra steps should be taken to
  ensure that demos will remain in sync

[Return to Table of Contents](#contents)

[]{#varplayer}

------------------------------------------------------------------------

**Player variables**

------------------------------------------------------------------------

Synopsis:


        * name      * chasecam_height
        * skin          * chasecam_dist
        * colour        * chasecam_speed
        * walkcam       * numhelpers
        * chasecam

- name\
  type: string\
  value = name of console player\
  Flags: netvar\
  \
- skin\
  type: string\
  value = console player\'s current skin\
  Flags: netvar\
  -\> skin names can be retrieved with the listskins command\
  \
- colour\
  type: named values\
  values: green, indigo, brown, red, tomato, dirt, blue, gold, sea,
  black, purple, vomit, pink, cream, white\
  value = consoleplayer\'s clothing color\
  Flags: netvar\
  \
- walkcam\
  type: on / off\
  on = the player controls a walkcam\
  off = the player has a normal viewpoint\
  Flags: not in network games\
  \
- chasecam\
  type: on / off\
  on = the player views himself from a following camera\
  off = the player has a normal viewpoint\
  \
- chasecam_height\
  type: integer\
  value range: -31 - 100 value = preferred height of the chasecam above
  the player\'s viewheight\
  \
- chasecam_dist\
  type: integer\
  value range: 10 - 1024 value = preferred distance from chasecam to
  player\
  \
- chasecam_speed\
  type: integer\
  value range: 1 - 100 value = percentage of distance to target chasecam
  moves per gametic\
  \
- numhelpers\
  type: integer\
  value range: 0 - 3\
  value = number of helper creatures that spawn with the player\
  Flags: not in network game\

[Return to Table of Contents](#contents)

[]{#varinput}

------------------------------------------------------------------------

**Input options**

------------------------------------------------------------------------

Synopsis:


        * alwaysmlook    * use_joystick
        * invertmouse    * use_mouse
        * sens_horiz     * joysens_x
        * sens_vert  * joysens_y
        * smooth_turning * grabmouse

- alwaysmlook\
  type: on / off\
  on = mouse movements are always interpreted as mlook\
  off = mouse movements may be interpreted as player movement\
  \
- invertmouse\
  type: on / off\
  on = mlook mouse movements are reversed \-- \"arcade style\"\
  off = mlook mouse movements correspond to physical mouse movement\
  \
- sens_horiz\
  type: integer\
  value range: 0 - 64\
  value = relative horizontal sensitivity of the mouse\
  \
- sens_vert\
  type: integer\
  value range: 0 - 48\
  value = relative vertical sensitivity of the mouse\
  \
- smooth_turning\
  type: on / off\
  on = mouse turning is smoothed by the game\
  off = raw mouse turning is used\
  \
- use_joystick\
  type: yes / no\
  yes = enables use of joystick/gamepad cross pad and buttons\
  no = joystick/gamepad is disabled\
  \
  \* NOTE: In DOS, the joystick must be calibrated through the Allegro
  configuration program.\
  \
- use_mouse\
  type: yes / no\
  yes = enables use of mouse tracking and buttons\
  no = mouse is disabled\
  \
- joysens_x\
  type: integer\
  value range: -32768, 32767\
  value = horizontal axis sensitivity for SDL joystick\
  NOTE: this variable does not exist in the DOS version of Eternity\
  \
- joysens_y\
  type: integer\
  value range: -32768, 32767\
  value = vertical axis sensitivity for SDL joystick\
  NOTE: this variable does not exist in the DOS version of Eternity\
  \
- grab_mouse\
  type: yes / no\
  yes = the mouse is restricted to Eternity\'s graphics window\
  no = the mouse can move freely\
  NOTE: this variable does not exist in the DOS version of Eternity\

[Return to Table of Contents](#contents)

[]{#varenvironment}

------------------------------------------------------------------------

**Environment / Physics variables**

------------------------------------------------------------------------

Synopsis:


       * autoaim      * varfriction
       * allowmlook   * recoil
       * bobbing      * bfglook
       * pushers      * p_markunknowns
       * nukage

- autoaim\
  type: on / off\
  on = the game engine assists players with aiming automatically\
  off = players must aim precisely\
  Flags: netvar, server-only\
  \
- allowmlook\
  type: on / off\
  on = players may use mlook\
  off = players may not use mlook\
  Flags: netvar, server-only\
  \
- bobbing\
  type: on / off\
  on = players see themselves bob about like chickens\
  off = players see themselves scoot around like robots on wheels\
  Flags: netvar, server-only\
- pushers\
  type: on / off\
  on = objects are affected by wind, current, conveyors\
  off = objects are not affected by wind, current, conveyors\
  Flags: netvar, server-only\
  \
- nukage\
  type: on / off\
  on = damaging sectors hurt players as normal\
  off = damaging sectors are harmless\
  Flags: netvar, server-only\
  \
- varfriction\
  type: on / off\
  on = players experience variable friction\
  off = players always experience normal friction\
  Flags: netvar, server-only\
  \
- recoil\
  type: on / off\
  on = players experience recoil when firing weapons\
  off = players do not experience recoil\
  Flags: netvar, server-only\
  \
- bfglook\
  type: named values\
  values: off, on, fixedgun\
  value = ability to mlook with BFG \-- fixedgun allows mlook to occur,
  but will not allow the unfair effects of BFG mlooking\
  Flags: netvar, server-only\
  \
- p_markunknowns\
  type: yes / no\
  yes = location of unknown thingtypes will be marked with EDF Unknown
  objects\
  no = only messages are displayed when unknown things are used in a
  map\
  NOTE: this affects single-player non-demo games only.

[Return to Table of Contents](#contents)

[]{#varai}

------------------------------------------------------------------------

**AI variables**

------------------------------------------------------------------------

Synopsis:


        * mon_infight   * mon_backing
        * mon_avoid * mon_helpfriends
        * mon_climb * mon_distfriend
        * mon_friction
        * mon_remember

- mon_infight\
  type: on / off\
  on = monsters will fight with each other when provoked\
  off = monsters will fight only with players and friends\
  Flags: netvar, server-only\
  \
- mon_avoid\
  type: on / off\
  on = monsters will avoid damaging sectors if possible\
  off = monsters are naive about damaging sectors\
  Flags: netvar, server-only\
  \
- mon_climb\
  type: on / off\
  on = monsters can climb tall stairs\
  off = monsters cannot navigate tall stairs\
  Flags: netvar, server-only\
  \
- mon_friction\
  type: on / off\
  on = monsters are affected by sector friction attributes\
  off = monsters always experience normal friction\
  Flags: netvar, server-only\
  \
- mon_remember\
  type: on / off\
  on = monsters remember their last enemy after making a kill\
  off = monsters track only one enemy and may fall asleep if it is
  killed\
  Flags: netvar, server-only\
  \
- mon_backing\
  type: on / off\
  on = monsters may intelligently back out of a fight if hurt\
  off = monsters savagely fight to the death\
  Flags: netvar, server-only\
  \
- mon_helpfriends\
  type: on / off\
  on = friends may help other friends with low health\
  off = friends will pursue their own targets without regard to others\
  Flags: netvar, server-only\
  \
- mon_distfriend\
  type: integer\
  value range: 0 - 1024\
  value = average distance friends attempt to remain from players\
  Flags: netvar, server-only\

[Return to Table of Contents](#contents)

[]{#varheadsup}

------------------------------------------------------------------------

**Heads-up system variables**

------------------------------------------------------------------------

Synopsis:

- hu_overlay
- hu_hidesecrets
- hu_obituaries
- hu_obitcolor
- hu_crosshair
- hu_crosshair_hilite
- hu_showvpo
- hu_vpo_threshold
- hu_messages
- hu_messagecolor
- hu_messagelines
- hu_messagescroll
- hu_messagetime
- hu_showtime
- hu_showcoords
- hu_timecolor
- hu_levelnamecolor
- hu_coordscolor
- show_scores
- map_coords

<!-- -->

- hu_overlay\
  type: named values\
  values: off, \"boom style\", flat, distributed, graphical\
  value = style / activation state of HUD\
  \
- hu_hidesecrets\
  type: yes / no\
  yes = kills/items/secrets statistics are not displayed on the HUD\
  no = kills/items/secrets statistics are displayed on the HUD\
  \
- hu_obituaries\
  type: on / off\
  on = obituaries are shown when a player dies\
  off = no obituaries are shown\
  \
- hu_obitcolor\
  type: named values\
  values: brick, tan, gray, green, brown, gold, red, blue, orange,
  yellow\
  value = color of obituary messages\
  \
- hu_crosshair\
  type: named values\
  values: none, cross, angle\
  value = shape / presence of the aiming crosshairs\
  \
- hu_crosshair_hilite\
  type: on / off\
  on = crosshair will highlight when player is aiming at a monster or
  friend\
  off = crosshair remains the default color at all times\
  \
- hu_showvpo\
  type: yes / no\
  yes = a VPO indicator will be displayed when the number of visplanes
  is at or above the level set in the \'hu_vpo_threshold\' variable.\
  no = no VPO indicator will be shown\
  \
  \* NOTE: Due to a more efficient visplane splitting algorithm
  developed in BOOM, Eternity uses fewer visplanes than DOOM did. Due to
  this discrepancy, Eternity uses a customizable number to set the level
  at which a VPO warning is issued. The default value of 85 is
  reasonable, but is not perfect. The appearance of the VPO indicator is
  only to be taken as a warning that the threshold may be near, and not
  that a VPO will occur at an exact location.\
  \
- hu_vpo_threshold\
  type: integer\
  value range: 1 - 128\
  value = number of visplanes required for the VPO indicator to be
  displayed.\
  \
- hu_messages\
  type: on / off\
  on = HUD messages will be displayed in the upper left corner\
  off = HUD messages are not displayed\
  \
- hu_messagecolor\
  type: named values\
  values: brick, tan, gray, green, brown, gold, red, blue, orange,
  yellow\
  value = color of normal HUD messages\
  \
- hu_messagelines\
  type: integer\
  value range: 0 - 14\
  value = number of lines in the HUD message widget\
  \
- hu_messagescroll\
  type: yes / no\
  yes = the HUD message widget can be scrolled to see previous messages\
  no = the HUD message widget cannot be scrolled\
  \
- hu_messagetime\
  type: integer\
  value range: 0 - 100000\
  value = length of time messages appear\
  \
- hu_showtime\
  type: yes / no\
  yes = level timer is displayed on the automap\
  no = level timer is not displayed\
  \
- hu_showcoords\
  type: yes / no\
  yes = player/pointer coordinates are displayed on the automap\
  no = player/pointer coordinates are not displayed\
  \
- hu_timecolor\
  type: named values\
  values: brick, tan, gray, green, brown, gold, red, blue, orange,
  yellow\
  value = color of automap level time HUD widget text\
  \
- hu_levelnamecolor\
  type: named values\
  values: brick, tan, gray, green, brown, gold, red, blue, orange,
  yellow\
  value = color of automap level name HUD widget text\
  \
- hu_coordscolor\
  type: named values\
  values: brick, tan, gray, green, brown, gold, red, blue, orange,
  yellow\
  value = color of automap player/pointer coordinates HUD widget text\
  \
- show_scores\
  type: on / off\
  on = a player frags chart will be shown when you die\
  off = no frags chart is displayed\
  \
- map_coords\
  type: on / off\
  on = automap coordinates follow the pointer in non-follow mode\
  off = automap coordinates always show player coordinates\

[Return to Table of Contents](#contents)

[]{#varautomap}

------------------------------------------------------------------------

**Automap variables**

------------------------------------------------------------------------

Synopsis:


       * mapcolor_rkey   * mapcolor_ydor   * mapcolor_secr   * mapcolor_sngl
       * mapcolor_bkey   * mapcolor_frnd   * mapcolor_exit   * mapcolor_back
       * mapcolor_ykey   * mapcolor_wall   * mapcolor_unsn   * mapcolor_fchg
       * mapcolor_rdor   * mapcolor_clsd   * mapcolor_sprt   * map_coords
       * mapcolor_bdor   * mapcolor_tele   * mapcolor_hair   * am_drawnodelines

- mapcolor_rkey\
  type: integer\
  value range: 0 - 255\
  value = color of red key objects on map\
  \
- mapcolor_bkey\
  type: integer\
  value range: 0 - 255\
  value = color of blue key objects on map\
  \
- mapcolor_ykey\
  type: integer\
  value range: 0 - 255\
  value = color of yellow key objects on map\
  \
- mapcolor_rdor\
  type: integer\
  value range: 0 - 255\
  value = color of red key doors on map\
  \
- mapcolor_bdor\
  type: integer\
  value range: 0 - 255\
  value = color of blue key doors on map\
  \
- mapcolor_ydor\
  type: integer\
  value range: 0 - 255\
  value = color of yellow key doors on map\
  \
- mapcolor_frnd\
  type: integer\
  value range: 0 - 255\
  value = color of friends when in IDDTx2 mode\
  \
- mapcolor_wall\
  type: integer\
  value range: 0 - 255\
  value = color of normal walls\
  \
- mapcolor_clsd\
  type: integer\
  value range: 0 - 255\
  value = color of closed doors / areas\
  \
- mapcolor_tele\
  type: integer\
  value range: 0 - 255\
  value = color of teleportal lines\
  \
- mapcolor_secr\
  type: integer\
  value range: 0 - 255\
  value = color of secret sector boundaries\
  \
- mapcolor_exit\
  type: integer\
  value range: 0 - 255\
  value = color of exit lines\
  \
- mapcolor_unsn\
  type: integer\
  value range: 0 - 255\
  value = color of computer map unseen lines\
  \
- mapcolor_sprt\
  type: integer\
  value range: 0 - 255\
  value = color of normal map objects\
  \
- mapcolor_hair\
  type: integer\
  value range: 0 - 255\
  value = color of map pointer\
  \
- mapcolor_sngl\
  type: integer\
  value range: 0 - 255\
  value = color of player arrow in single-player game\
  \
- mapcolor_back\
  type: integer\
  value range: 0 - 255\
  value = color of automap background\
  \
- mapcolor_fchg\
  type: integer\
  value range: 0 - 255\
  value = color of lines at floor height changes\
  \
- am_drawnodelines\
  type: on / off\
  on = node lines from the BSP are drawn on the automap using the
  friendly thing color\
  off = normal automap behavior\

[Return to Table of Contents](#contents)

[]{#varstatus}

------------------------------------------------------------------------

**Status Bar variables**

------------------------------------------------------------------------

Synopsis:


        * ammo_red  * armor_red * st_graypct
        * ammo_yellow   * armor_yellow
        * health_red    * armor_green
        * health_yellow * st_rednum
        * health_green  * st_singlekey

- ammo_red\
  type: integer\
  value range: 0 - 100\
  value = ammo amount at which status bar numbers turn red\
  \
- ammo_yellow\
  type: integer\
  value range: 0 - 100\
  value = ammo amount at which status bar numbers turn yellow\
  \
- health_red\
  type: integer\
  value range: 0 - 200\
  value = health amount at which status bar numbers turn red\
  \
- health_yellow\
  type: integer\
  value range: 0 - 200\
  value = health amount at which status bar numbers turn yellow\
  \
- health_green\
  type: integer\
  value range: 0 - 200\
  value = health amount at which status bar number turn green\
  \
- armor_red\
  type: integer\
  value range: 0 - 200\
  value = armor amount at which status bar number turn red\
  \
- armor_yellow\
  type: integer\
  value range: 0 - 200\
  value = armor amount at which status bar numbers turn yellow\
  \
- armor_green\
  type: integer\
  value range: 0 - 200\
  value = armor amount at which status bar numbers turn green\
  \
- st_rednum\
  type: yes / no\
  yes = status bar numbers are always red\
  no = status bar numbers change color to reflect condition\
  \
- st_singlekey\
  type: yes / no\
  yes = status bar key icons show only one key type even if both are
  possessed (skull vs key)\
  no = status bar key icons display a double key icon if both keys are
  possessed\
  \
- st_graypct\
  type: yes / no\
  yes = if status bar numbers change colors, the percent sign will be
  gray rather than changing along with the numbers\
  no = if status bar numbers change colors, the percent sign will change
  colors as well \-- otherwise it will be red as normal\

[Return to Table of Contents](#contents)

[]{#varmenus}

------------------------------------------------------------------------

**Menu variables**

------------------------------------------------------------------------

Synopsis:


        * use_traditional_menu
        * wad_directory
        * mn_toggleisback
        * mn_searchstr
        * mn_start_mapname

- use_traditional_menu\
  type: yes / no\
  yes = DOOM / DOOM II original main menu will be emulated\
  no = Eternity\'s main menu behavior is used\
  \
  \* NOTE: This variable has no effect on game modes other than DOOM or
  DOOM II.\
  \
- wad_directory\
  type: string\
  value = user\'s wad directory, which will be listed in the wad loading
  dialog.\
  \
- mn_toggleisback\
  type: yes / no\
  yes = \"menu_toggle\" keybinding action goes back one menu instead of
  exiting\
  no = \"menu_toggle\" keybinding action exits the menu system\
  \
- mn_searchstr\
  type: string\
  value = string to search for in menus via mn_search command\
  \
- mn_start_mapname\
  type: string\
  value = Exact map header name of a map at which a new game started
  from the menus should begin at. If invoked from the menus, this
  command will immediately invoke the gamemode-dependent skill selection
  menu when its value is set.\
  \

[Return to Table of Contents](#contents)

[]{#varrendering}

------------------------------------------------------------------------

**Rendering variables**

------------------------------------------------------------------------

Synopsis:


        * r_showgun     * r_zoom        * r_swirl         * rocket_trails
        * r_showhom     * r_stretchsky  * gamma           * grenade_trails
        * r_blockmap    * r_precache    * draw_particles  * lefthanded
        * r_homflash    * r_trans       * bloodsplattype  * bfg_cloud
        * r_planeview   * r_tranpct     * bulletpufftype

- r_showgun\
  type: yes / no\
  yes = weapon sprites are drawn as normal\
  no = no weapon sprites are drawn\
  \
- r_showhom\
  type: yes / no\
  yes = HOM will appear as a red or flashing red area on the screen\
  no = HOM will appear as a simple lack of screen-redraw\
  \
- r_blockmap\
  type: on / off\
  on = the game engine will rebuild blockmaps at run-time\
  off = the game engine will use prebuilt blockmaps when available\
  \
- r_homflash\
  type: on / off\
  on = when HOM detection is enabled, it will flash between red and
  black\
  off = HOM detection appears as solid red\
  \
  \*\* This option is important if the user may have an epileptic
  condition triggered by flashing solid color patterns. It is
  recommended of course that such persons do not play video games at any
  rate.\
  \
- r_planeview\
  type: on / off\
  on = visplanes are separated by thin black lines\
  off = visplanes are not visible to the user\
  \
  This option is just for technical interest.\
  \
- r_zoom\
  type: integer\
  value range: 0 - 8192\
  value = zoom level of the game camera\
  \
- r_stretchsky\
  type: on / off\
  on = short skies will be stretched to accomidate up/down look\
  off = short skies are tiled (very ugly!)\
  \
- r_precache\
  type: on / off\
  on = various graphics will be precached at the start of the level\
  off = graphics are loaded as needed (shorter start up time, but can
  result in loss of framerate)\
  \
- r_trans\
  type: on / off\
  on = general translucency is enabled\
  off = general translucency is disabled (can improve framerate)\
  \
- r_tranpct\
  type: integer\
  value range: 0 - 100\
  value = general global translucency percentage\
  \
- r_swirl\
  type: on / off\
  on = all animated flats use a swirling effect\
  off = animated flats behave normally\
  \
  \* NOTE: this option is just for testing / fun \-- swirling can be
  achieved for individual flats through the ANIMATED lump.\
  \
- gamma\
  type: integer\
  value range: 0 - 4\
  value = gamma correction level\
  \
- draw_particles\
  type: on / off\
  on = individually enabled particle effects are drawn\
  off = no particle effects are drawn\
  \
- bloodsplattype\
  type: named values\
  values: sprites, particles, both\
  value = effect used for blood splats\
  \
- bulletpufftype\
  type: named values\
  values: sprites, particles, both\
  value = effect used for bullet puffs\
  \
- rocket_trails\
  type: on / off\
  on = rockets have a smoke trail effect\
  off = rockets are normal\
  \
- grenade_trails\
  type: on / off\
  on = grenades have a smoke trail effect\
  off = grenades are normal\
  \
- lefthanded\
  type: named values\
  values: right, left\
  value = handedness of player \-- left causes weapon sprites to be
  flipped\
  \
  \* NOTE: due to discrepancies in the original weapon graphics, left vs
  right handedness is illy-defined and is a popular subject of debate on
  forums, channels, and sites. This feature is just for fun.\
  \
- r_ptcltrans\
  type: named values\
  values: none, smooth, general\
  value = translucency type used by particle effects \-- smooth is
  zdoom-style smooth fading, and general is static, BOOM-style
  translucency as used in Eternity v3.29 Gamma and earlier.\
  \
- bfg_cloud\
  type: on / off\
  on = BFG projectiles have a particle cloud\
  off = BFG projectiles are normal\

[Return to Table of Contents](#contents)

[]{#varparticle}

------------------------------------------------------------------------

**Particle events**

------------------------------------------------------------------------

Synopsis:


        * pevt_rexpl
        * pevt_bfgexpl

- pevt_rexpl\
  type: on / off\
  on = Particle event #1, explosion particle burst, is enabled\
  off = Particle event #1 is disabled\
  \
- pevt_bfgexpl\
  type: on / off\
  on = Particle event #2, bfg particle burst, is enabled\
  off = Particle event #2 is disabled\

[Return to Table of Contents](#contents)

[]{#varvideo}

------------------------------------------------------------------------

**Video variables**

------------------------------------------------------------------------

Synopsis:


        * v_diskicon
        * v_retrace
        * v_mode
        * v_ticker
        * shot_type
        * shot_gamma
        * screensize
        * wipewait

- v_diskicon\
  type: on / off\
  on = flashing disk icon is used to indicate disk access\
  off = disk access is silent\
  \
- v_retrace\
  type: yes / no\
  yes = drawing waits for vertical retrace (vsynch)\
  no = drawing does not wait for vsynch (may cause display tearing)\
  \
- v_mode\
  type: named values\
  values: use v_modelist command to view available modes\
  value = video mode\
  \
- v_ticker\
  type: named values\
  values: off, chart, classic, text\
  value = presence / appearance of an FPS ticker\
  classic is the original FPS ticker enabled through -devparm in DOOM\
  \
- shot_type\
  type: named values\
  values: bmp, pcx\
  value = file type written when screenshots are taken\
  \
- shot_gamma\
  type: yes / no\
  yes = gamma correction will be applied to screenshots.\
  no = no gamma correction will be applied to screenshots.\
  \
- screensize\
  type: integer\
  value range: 0 - 8\
  value = size of game screen\
  \
- wipewait\
  type: named values\
  values: never, always, demos\
  value = whether or not the screen wipe routine blocks the main game
  loop.\
  \"demos\" means to do this during demo playback only. This allows
  seeing the first few frames of a demo when watching demos from the
  command line or a frontend.

[Return to Table of Contents](#contents)

[]{#varsound}

------------------------------------------------------------------------

**Sound options**

------------------------------------------------------------------------

Synopsis:


       * detect_voices   * s_flippan       * snd_spcbassboost
       * snd_card        * s_precache
       * mus_card        * s_pitched
       * sfx_volume      * snd_channels
       * music_volume    * snd_spcpreamp

- detect_voices\
  type: yes / no\
  yes = Allegro will attempt to autodetect the number of hardware voices
  available\
  no = Allegro will use a specified default number of voices\
  \
  \* NOTE: this setting only works in the DOS version of Eternity.\
  \
- snd_card\
  type: named values\
  values:
  - DOS: autodetect, none, SB, \"SB 1.0\", \"SB 1.5\", \"SB 2.0\", \"SB
    Pro\", SB16, GUS
  - Windows: SDL_mixer, none

  value = type of sound driver to be used by Allegro/SDL\
  - autodetect = Allegro will attempt to autodetect the card type (DOS
    only)
  - none = digital sound will be disabled

  \
- mus_card\
  type: named values\
  values:
  - DOS: autodetect, none, adlib, OPL2, 2xOPL2, OPL3, \"SB MIDI\",
    MPU-401, GUS, DIGMID, AWE32\
  - Windows: SDL_mixer, none

  value = type of MIDI driver to be used by Allegro/SDL\
  - autodetect = Allegro will attempt to autodetect the synthesizer (DOS
    only)\
  - none = MIDI output will be disabled

  \
- sfx_volume\
  type: integer\
  value range = 0 - 15\
  value = base volume for digital sound effects\
  \
- music_volume\
  type: integer\
  value range: 0 - 15\
  value = base volume for MIDI synthesis\
  \
- s_flippan\
  type: on / off\
  on = forces reversal of stereo sound channels\
  off = stereo sound will be normal for the card/driver/other settings\
  \
  \* NOTE: on and off for this variable do not equate to flipped or
  non-flipped in the same way for every combination of sound card and/or
  Allego configuration. If your sound is right, turning on s_flippan
  will make it wrong. If your sound is wrong, try this to make it
  right.\
  \
- s_precache\
  type: on / off\
  on = sounds will be precached at the beginning of the game\
  off = sounds will be loaded as they are used\
  \
  \* NOTE: off will make load times shorter, but may degrade
  performance. on may require more system memory.\
  \
- s_pitched\
  type: on / off\
  on = enable variable-pitched sound effects\
  off = sounds always play at real pitch\
  \
- snd_channels\
  type: integer\
  value range: 0 - 128\
  value = number of software channels to maintain\
  \
  \* NOTE: this has nothing to do with the number of available hardware
  channels, which, unless you are very rich, is probably nowhere near
  128 :P\
  \
  This value affects the number of sounds the game will \*attempt\* to
  play at one time. If set too low, you will lose sound unnecessarily.
  Setting it too high will use more memory and take more time to
  process.\
  \
- snd_spcpreamp\
  type: integer\
  value range: 1 - 6\
  value = amplification factor for SPC music.\
  \
- snd_spcbassboost\
  type: integer\
  value range: 0 - 31\
  value = logarithmic bass boost scale for SPC music.\

[Return to Table of Contents](#contents)

[]{#varchat}

------------------------------------------------------------------------

**Chat Macros**

------------------------------------------------------------------------

Chat macros are stored in variables \"chatmacro0\" through
\"chatmacro9\" All are of type string and reflect the chat macros usable
in multiplayer.\
\
[Return to Table of Contents](#contents)

[]{#varweapon}

------------------------------------------------------------------------

**Weapon Preferences**

------------------------------------------------------------------------

Weapon preferences are stored in variables \"weappref_0\" through
\"weappref_8\".\
\
Weapon preferences are named-value variables and use the following
values:


       fist, pistol, shotgun, chaingun, "rocket launcher", "plasma gun",
       bfg, chainsaw, "double shotgun"

[Return to Table of Contents](#contents)

[]{#vardoomcomp}

------------------------------------------------------------------------

**DOOM Compatibility Vector**

------------------------------------------------------------------------

The compatibility vector can be accessed with the following variables.
All variables are of the on / off type and are server-only net
commands.\
\
Synopsis:


       * comp_telefrag     * comp_blazing     * comp_floors      * comp_zombie
       * comp_dropoff      * comp_doorlight   * comp_skymap      * comp_stairs
       * comp_vile         * comp_model       * comp_pursuit     * comp_infcheat
       * comp_pain         * comp_god         * comp_doorstuck   * comp_zerotags
       * comp_skull        * comp_falloff     * comp_staylift    * comp_terrain

       * comp_respawnfix   * comp_theights  
       * comp_fallingdmg
       * comp_soul
       * comp_overunder
       * comp_planeshoot

- comp_telefrag\
  Determines whether enemies telefrag players on MAP30\
  \
- comp_dropoff\
  Determines whether enemies get stuck over tall dropoffs\
  \
- comp_vile\
  Determines whether Archviles can resurrect ghosts\
  \
- comp_pain\
  Toggles Pain Elemental\'s limit of 21 lost souls on the map\
  \
- comp_skull\
  Determines whether Pain Elementals may spawn lost souls past blocking
  lines\
  \
- comp_blazing\
  Toggles double closing sound from blazing doors\
  \
- comp_doorlight\
  Turns on/off tagged sector light fading for DR door line types\
  \
- comp_model\
  Toggles use of DOOM\'s linedef trigger model\
  \
- comp_god\
  Determines whether damage greater than 9999 can kill players in god
  mode\
  \
- comp_falloff\
  Toggles MBF torque simulation physics\
  \
- comp_floors\
  Toggles use of DOOM\'s floor movement model\
  \
- comp_skymap\
  Determines whether invulnerability affects drawing of sky textures\
  \
- comp_pursuit\
  Determines whether enemies remember their last target\
  \
- comp_doorstuck\
  Determines whether enemies may get stuck on door tracks\
  \
- comp_zerotags\
  Determines whether linedef actions with a zero tag will be applied.\
  \
- comp_terrain\
  Toggles Eternity TerrainTypes effects\
  \
- comp_respawnfix\
  Determines whether spawned things respawn at the point (0, 0) or at
  their point of death\
  \
- comp_fallingdmg\
  Toggles Eternity falling damage\
  \
- comp_soul\
  Determines lost soul floor/ceiling bouncing behavior.\
  \
- comp_overunder\
  Toggles Eternity extended 3D object clipping.\
  \
- comp_planeshoot\
  Toggles ability to shoot floors and ceilings with tracer weapons.\
  Note: This variable is required in particular for Eternal DOOM
  compatibility.\
  \
- comp_theights\
  Works along with comp_overunder. If disabled, decorative thingtypes
  from the DOOM and DOOM II gamemodes will have their heights corrected
  using the **correctheight** as defined by the EDF thingtype
  definition. For gameplay reasons, monsters do not define correct
  heights in the default EDF.\
  \

[Return to Table of Contents](#contents)

[]{#varmisc}

------------------------------------------------------------------------

**Misc. Configuration**

------------------------------------------------------------------------

Synopsis:


        * use_startmap
        * i_gamespeed
        * i_ledsoff
        * startonnewmap
        * i_waitatexit
        * i_showendoom
        * i_endoomdelay
        * i_grabmouse

- use_startmap\
  type: named values\
  values: ask, no, yes\
  value = determines whether the game should use the menu or the start
  map to start new games. \"ask\" is used to indicate that the user has
  not chosen a value. The game will prompt the player to decide the next
  time the game is started.\
  \
  \* NOTE: if new wads are added and none of them contain their own
  start map, the start map will not be used.\
  \
- i_gamespeed\
  type: integer\
  value range: 0 - 500\
  value = percentage of real time at which the game clock runs\
  \
- i_ledsoff\
  type: yes / no\
  yes = keyboard LEDs will remain off regardless of lock states\
  no = keyboard LEDs will toggle on/off with presses of lock keys\
  \
  \* NOTE: this variable only works in the DOS version of Eternity.\
  \
- startonnewmap\
  type: yes / no\
  yes = new game starts on first new map, if any\
  no = new game starts at MAP01 or on selected episode\
  \
- i_waitatexit\
  type: yes / no\
  yes = game will wait for input after exiting\
  no = console window will shut immediately on exit\
  \
  \* NOTE: this variable does not exist in the DOS version of Eternity.\
  \
- i_showendoom\
  type: yes / no\
  yes = ENDOOM will be displayed in a textmode emulator window on exit\
  no = ENDOOM is not displayed\
  \
  \* NOTE: this variable does not exist in the DOS version of Eternity.\
  \
- i_endoomdelay\
  type: integer\
  value range: 35 - 3500 value = time in gametics to display the ENDOOM
  in the textmode emulator\
  \
  \* NOTE: this variable does not exist in the DOS version of Eternity.\
  \
- i_grabmouse\
  type: yes / no\
  yes = window grabs mouse input\
  no = window does not grab mouse input\
  \
  \* NOTE: this variable does not exist in the DOS version of Eternity.\

[Return to Table of Contents](#contents)

[]{#varconstants}

------------------------------------------------------------------------

**Constants**

------------------------------------------------------------------------

These variables are read-only and cannot be set to a new value.\
\
Synopsis:


        * rngseed   * ver_time
        * creator
        * version
        * ver_date
        * ver_name

- rngseed\
  type: integer\
  value = current random number generator seed\
  \
- creator\
  type: string\
  value = author of current map as specified via MapInfo\
  \
- version\
  type: integer\
  value = whole-number version of this Eternity Engine build\
  \
- ver_date\
  type: string\
  value = date on which this Eternity Engine executable was compiled\
  \
- ver_name\
  type: string\
  value = special name of this Eternity Engine version\
  \
- ver_time\
  type: string\
  value = time at which this Eternity Engine executable was compiled\

[Return to Table of Contents](#contents)
