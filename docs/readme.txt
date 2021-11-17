===============================================================================
Title                   : The Eternity Engine v4.03.00 "Glitnir"
Filename                : ee-4.03.00-win64.zip, ee-4.03.00-macos.dmg,
                          ee-4.03.00-win32.zip, ee-4.03.00-win-legacy.zip
Release date            : 2021-11-06
Author                  : Team Eternity:
                          Ioan "printz" Chera,
                          James "Quasar" Haley,
                          David "DavidPH" Hill,
                          Stephen "SoM" McGranahan,
                          Max "Altazimuth" Waine
Email Address           : (redacted)

Description             : The Eternity Engine is an advanced Doom source port
                          which uniquely combines a high compatibility rate
                          with bleeding-edge editing and gameplay enhancements.
                          Eternity supports all versions of Doom, Doom II,
                          Final Doom, HACX, and the FreeDoom project, and it
                          has in-progress support for Raven Software's Heretic
                          and Hexen and Rogue Entertainment's Strife.

                          See the Eternity wiki for editing documentation at
                          eternity.youfailit.net

Additional Credits to   : Graphics
                          --------
                            Sarah "esselfortium" Mancuso
                            Sven "ptoing" Ruthner
                            Bob Satori
                            Nash Muhandes

                          Works Based On
                          --------------
                          - GPL DOOM Source, Quake 2 Source, Quake 3 Source -
                              id Software
                              Gilles Vollant
                          - GPL Heretic and Hexen Source -
                              Raven Software
                          - DosDOOM -
                              Chi Hoang
                          - BOOM by TeamTNT -
                              Lee Killough, Jim Flynn, Rand Phares,
                              Ty Halderman, Stan Gula
                          - MBF -
                              Lee Killough
                          - SMMU, Chocolate Doom -
                              Simon "fraggle" Howard

                          Significant Code Utilized From
                          ------------------------------
                          - BSP 5.2 -
                              Raphael Quinet, Colin Reed, Lee Killough,
                              Colin Phipps, Simon Howard
                          - Doom64 EX -
                              Samuel "Kaiser" Villarreal
                          - Odamex -
                              "Dr. Sean" Leonard
                          - PrBoom -
                              Colin Phipps, Florian Schulze
                          - PrBoom-Plus -
                              Andrey Budko
                          - ZDoom -
                              Marisa Heit, Graf Zahl, Braden Obrzut

                          Libraries and Utilities
                          -----------------------
                          - Freeverb Public Domain Reverb Engine -
                              Jezar at Dreampoint Design and Engineering
                          - libADLMidi -
                              Joel Yliluoma, Vitaly Novichkov, Jean Pierre Cimalando
                          - libConfuse Parser Library -
                              Martin Hedenfalk
                          - libpng -
                              Glenn Randers-Pehrson, Andreas Dilger,
                              Guy Eric Schalnat, et al.
                          - LLVM libc++ -
                              LLVM Team, University of Illinois at
                              Urbana-Champaign; Howard Hinnant et al.
                          - MinGW libc -
                              Matt Weinstein et al.
                          - SDL2 and Support Libraries -
                              Sam Lantinga et al.
                          - snes_spc 0.9.0 -
                              Shay Green
                          - SpiderMonkey 1.8.0 -
                              Mozilla Foundation; Brendan Eich et al.
                          - Three-Band EQ and Rational-tanh Clipper -
                              Neil C / Etanza Systems, cschueler
                          - zlib -
                              Jean-loup Gailly, Mark Adler, et al.

Special Thanks to       : - Feature and Patch Contributors -
                              4mer
                              Afterglow
                              Anders Astrand
                              anotak
                              Cephaler
                              Charles Gunyon
                              dotfloat
                              Gez
                              Joe Kennedy
                              Joel Murdoch
                              Julian Aubourg
                              Kaitlyn Anne Fox
                              Mike Swanson
                              MP2E
                              Revenant
                              Russell Rice
                              ryan
                              Samuel Villarreal
                              SargeBaldy
                              Shannon Freeman
                              Simon Howard
                              Yagisan
                          - Hosting -
                              Mike "Manc" Lightner
                          - Idea People and Beta Testers -
                              Catoptromancy
                              ellmo
                              Lut
                              Mordeth
                              Xaser
                          - Friends of Team Eternity -
                              Assmaster
                              Chris Rhinehart
                              DooMWiz
                              James Monroe
                              John Romero
                              JKist3
                              Toke


===============================================================================
* What is included *

New levels              : None
Sounds                  : Yes
Music                   : No
Graphics                : Yes
Dehacked/BEX Patch      : No
Demos                   : No
Other                   : Yes (Required data and wad files)
Other files required    : DOOM, DOOM II, Final Doom, HACX, or Heretic
                          IWAD(s)


* Construction *

Base                    : SMMU v3.21 / v3.30
Build Time              : Nineteen years and counting
Editor(s) used          : Visual Studio 2017/2019, Xcode, UltraEdit-32, SLADE,
                          Visual Studio Code, Notepad++, GZDoom Builder
Known Bugs              : Too many to mention here, see below for more info
May Not Run With...     : Most mods made for other source ports


* Copyright / Permissions *

Authors MAY use the contents of this file as a base for modification or
reuse, subject to the terms of the GNU General Public License v3.0 or later,
which may be found inside the file named "COPYING".  Permissions have been
obtained from original authors for any of their resources modified or
included in this file.

Additional terms and conditions allowed under Section 7 of the GPLv3 apply
to this distribution. See the file named "COPYING-EE" for details.

You MAY distribute this file, subject to the terms of the GNU General Public
License v3.0 or later and any additional compatible terms and conditions.


* Where to get the file that this text file describes *

Web sites: https://github.com/team-eternity/eternity/releases
           https://www.doomworld.com/forum/25-eternity/
           https://eternity.youfailit.net/
           http://eternity.mancubus.net/
FTP sites: None


===============================================================================
* Disclaimer *

This software is covered by NO WARRANTY, implicit or otherwise, including
the implied warranties of merchantability and fitness for a particular
purpose. The authors of this software accept no responsibility for any ill
effects caused by its use and will not be held liable for any damages,
even if they have been made aware of the possibility of such damages.

Bug reports and feature requests will be appreciated.

Please post on the Doomworld forum (https://www.doomworld.com/forum/25-eternity/)
with any concerns.


===============================================================================
* Features New to Version 4.03.00 *

** Level Info **

  * Implemented full UMAPINFO (cross-port level info) support in Eternity.
  * Added the following properties to EMAPINFO: endpic, finaletype endpic,
    enterpic and levelaction-bossdeath (for feature parity with UMAPINFO).
  * Added the 'levelnum' variable to EMAPINFO. When map names don't follow the
    format ExMy or MAPxx, it wasn't possible to use parameterized special
    Teleport_NewMap or linedef type 74 (Exit to map). THANKS TO Afterglow FOR
    THIS FIX.
  * Increased the support for MAPINFO fields, as needed with many megawads
    (still in progress).
  * Fixed various bugs about unusual level transitions. Thanks to the UMAPINFO
    standard for driving these fixes.
  * Improved support for numbering the ExMy and MAPxy going beyond their
    conventional number limitations.
  * Now the level info changes (from any such lump) will also take effect in de-
    mos and vanilla mode if they're only cosmetic.

** Level editing features **

  * Added sector actions for entering and leaving a sector, EESectorActionEnter
    and EESectorActionExit. They have doomednum 9997 and 9998 respectively, the
    same  as ZDoom.
  * Added MBF21 blockplayers linedef flag
  * Added MBF21 blocklandmonsters linedef flag
  * Fixed the Elevator_MoveToFloor and classic counterpart to work even if
    activator doesn't have a linedef.
  * If the SetActorPosition ACS function uses fog, make sure to spawn the fog in
    front of the destination area, not on top of it.

** Modding features **
  * (THANKS TO Afterglow FOR THIS FEATURE)

    Added EDF gameproperties menu.startmap. Restored the ability for PWADs to
    define a Quake-style start map for skill selection instead of using the
    menu. Menu item "New Game" and console command "mn_newgame" will load a
    level optionally defined through EDF.

    This replaces CVAR 'use_startmap' originally from SMMU which has been unused
    since FraggleScript was removed from EE.

    Also removed the commented out menu code from SMMU that prompted players to
    choose the bundled START map going forward.

  * Added the RANGEEIGHTH flag for thingtype, required for Strife compatibility.
    It cuts the considered distance to target by eight, giving a much higher
    missile attack probability.
  * Added the NOTAUTOAIMED flag for thingtype, useful for destructible objects
    which shouldn't capture autoaim.
  * Added a spawn type parameter to the A_FatAttack* codepointers.
  * MBF21: added support for dehacked frames Args[6-8] (1-5 were already
    present).
  * Made thing-based obituaries use deh or EDF strings if they start with $.

** Automap **

  * Now the automap has an overlay mode like in PrBoom+ and Crispy Doom. It can
    be activated with the 'o' key (you may need to bind it manually if you
    upgraded Eternity to this version).
  * Eliminated the automap coordinate shifting by linked portals, because it
    added more confusion for mappers and playtesters alike, and it didn't
    effectively hide any immersion detail (the XY coordinates are already pretty
    arbitrary for the player).

** Sky aspect and freelook **

  * Short skies now fade into a single colour instead of stretching. The
    r_stretchsky console variable and stretchsky OPTIONS entry are deprecated
    now and do nothing. Removed the "stretch short skies" mouse menu option.

    Added more control to the user on how the sky set in EMAPINFO gets drawn.
    The skyrowoffset and sky2rowoffset fields give this control.

    Doom now allows the player to look up and down at maximum ±45 degrees, due
    to popular demand, especially with portal-heavy maps. Heretic still stays
    at ±32 degrees since that's how it was designed (whereas for Doom, looking
    up and down is an optional extension). Mod authors can configure the look
    pitch in two new EDF gameproperties fields: game.lookpitchup and
    game.lookpitchdown.

  * Horizontally scrolling skies (e.g. clouds) now move smoothly.

** Save game improvements **

  * Overhauled the save and load menus. You can now have an arbitrary amount of
    savegames, can delete selected savegames with the delete key, and info on
    the save like when it was saved, what map it's on, etc. Saves in the list
    are sorted from most recent to least recent. Quickload now allows you to
    select a quicksave slot if one isn't already selected.

    Credit goes to Devin Acker (Revenant) for the idea and initial code for that
    ended up defining the layout and such that I went with for this.

  * Savegames are now more resistant to version changes. Now if anything gets
    added to the Eternity base resources, the saves will retain the correct
    references (things won't stop working, textures won't shift etc.) This won't
    recover previous saves which failed with this, but new saves will be
    forward compatible.

  * Re-introduce a warning for potentially-incompatible savegames.

** Gameplay improvements **

  * Holding down weapon switch won't rotate any longer between all weapons in
    that slot automatically, unless the "weapon_hotkey_holding" console variable
    is set to ON. There's a new entry in the weapon options menu for this, and
    the option can also be set in a wad OPTIONS lump.

** Interface improvements **

  * Added cvar "hu_crosshair_scale" which can toggle scaling of the crosshair
    off or on. It's saved in the config as "crosshair_scale".
  * Added an option to centre weapons when firing. It's in the weapons menu.
    Console variable: "r_centerfire".
  * Fixed the modern HUD being stretched horizontally. Restricted the modern HUD
    to a 16:9 subscreen (if at 16:9 or wider aspect). Can be toggled on/off with
    the cvar "hu_restrictoverlaywidth".
  * Made the menus use the console font by default.
  * New console variable "am_drawsegs" for displaying the BSP segs in the
    automap. Useful for debugging BSP.
  * Fixed -dog. Made it so -dogs only works if it's not the last command-line
    arg.
  * macOS: reenabled ENDOOM, as it seems stable now.

===============================================================================
* Coming Soon *

These are features planned to debut in future versions of the Eternity Engine:

- Priority -

  ** Slope physics
  ** Aeon scripting system
  ** 100% Heretic support
  Hexen Support
  Strife Support                         (In progress)
  PSX Doom support                       (In progress)
  Double flats

  ** == TOP PRIORITY

- Long-Term Goals -

  Improved network code


===============================================================================
* Revision History *

* Dates are in mm/dd/yy *

4.03.00 "Glitnir" -- 11/06/21

  UMAPINFO support, overlay automap, sky and freelook improvements, save game
  improvements, added the first MBF21 features and sector actions. Many bug
  fixes.

4.02.00 "Forseti" -- 01/24/21

  This update features a transposed renderer backbuffer, which allows various
  optimisations that should improve performance.

4.01.00 "Tyrfing" -- 10/13/20

  Major update which introduces EDF weapons, EDF items, EDF bullet puff effects,
  a new HUD by ptoing and libADLMIDI music support. In addition a lot of other
  features have been added and bugs fixed.

4.00.00 "Völuspá" -- 03/16/18

  The first release after the move to SDL2. It also features EDF animations and
  switches, EDF-configurable monster infighting. In addition, sector portals
  inside polyobject portals now physically move with the portal, as well as
  visually.

3.42.02 "Heimdal" -- 05/07/17

  Huge release featuring UDMF, fully working linked portals, completed
  parameterized line support and many more mapping features.

3.40.46 "Bifrost" -- 01/19/14

  Huge release featuring frame interpolation, reverb engine, force feedback
  support for XInput, a large amount of progress toward inventory/weapons/full
  Heretic support, and more.

3.40.37 "Gungnir" -- 05/27/13

  Once again meant to be a minor patch release. Instead, includes across the
  board improvements and additions including DynaBSPs, linked portal line LOS,
  new gamepad HAL, MIDI RPC server, new linedef special system, and more.

3.40.30 "Alfheim" -- 11/04/12

  Started as a minor patch release and bloomed into something much greater.

3.40.25 "Midgard" -- 08/26/12

  Interim release to hold over the fans until Aeon and inventory branches are
  ready. New improved ACS interpreter by David Hill.

3.40.20 "Mjolnir" -- 12/25/11

  Santa Quas brings a new Christmas release with some cool new features.

3.40.15 "Wodanaz" -- 06/21/11

  First release with ability to draw on a hardware-accelerated device context.

3.40.11 "Aasgard" -- 05/02/11

  Minor maintenance release to fix saving of levels that contain polyobjects.

3.40.10 "Aasgard" -- 05/01/11

  "Blue Box" release to showcase Vaporware project demo.

3.40.00 "Rebirth" -- 01/08/11

  10th anniversary release, and the first release from the C++ codebase.

3.39.20 "Resheph" -- 10/10/10

  Point release for additional HACX 1.2 support.

3.37.00 "Sekhmet" -- 01/01/10

  Big release after nearly a year of development time with some major
  advancements, though many are behind-the-scenes improvements.

3.35.92 "Nekhbet" -- 03/22/09

  Minor maintenance release with numerous interface improvements, and a new EDF
  font specification system.

3.35.90 "Simorgh" -- 01/11/09

  It took two years and almost 450 SVN revisions to create this new incarnation
  of the Eternity Engine.

3.33.50 "Phoenix" -- 10/23/06

  Sad times have given way to happy ones as the energy to do great work has been
  unleashed.

3.33.33 "Paladin" -- 05/17/06

  The most work ever done for a single release possibly excepting only the port
  of v3.29 Gamma to Windows. We apologize for the delay, but it is well worth
  the wait.

3.33.02 "Warrior" -- 10/01/05

  A very respectable amount of progress hopefully offsets the terrible delay
  experienced for this release. 3D object clipping improvement, new EDF
  TerrainTypes engine, and various EDF improvements are the most significant
  features.

3.33.01 "Outcast" -- 06/24/05

  Minor fixes, some user-requested and CQIII-needed features implemented, and
  HUD scripting functions for Small.

3.33.00 "Genesis" -- 05/26/05

  Small scripting finally complete and available to the user. Lots of fixes,
  tweaks, and feature additions on top of that.

3.31.10 "Delta" -- 01/19/05

  More Small support, ExtraData, global MapInfo, and tons of other new features
  and bug fixes make this an extremely critical release.

3.31 Public Beta 7 04/11/04

  High resolution support enabled via generalization of the screen drawing code.
  Some minor fixes to portals, and a new codepointer just for good measure.

3.31 Public Beta 6 02/29/04

  A huge amount of progress toward all current goals for the engine. Portals,
  the brain child of SoM, are the major stars of this release. Small and
  ExtraData are on the verge of becoming available to users for editing.

3.31 Public Beta 5 12/17/03

  Several minor bug fixes over beta 4, including 3D object clipping and file
  drag-and-drop. Incremental improvements to EDF and to several parameterized
  codepointers.

3.31 Public Beta 4 11/29/03

  Most effort put into improving EDF, largely via user feature requests. Major
  progress has been made toward working ExtraData as well. The Heretic status
  bar is now implemented.

3.31 Public Beta 3 08/08/03

  A long time in coming, and a ton of features to show for the delay. EDF steals
  the show for this release.

3.31 Public Beta 2 03/05/03

  New BFGs, object z clipping, movable 3DMidTex lines, tons more Heretic
  support, and some really nasty bugs fixed. Also first version to use SDL_mixer
  for better sound.

3.31 Public Beta 1 09/11/02

  Prelim. Heretic support, 3DMIDTEX flag, zdoom-style translucency, and some
  other stuff now in. And of course, bug fixes galore.

3.29 "Gamma" 07/04/02

  Final release for v3.29. Massive changes all over. First version to include
  Windows support. SoM is now co-programmer.

3.29 Development Beta 5 10/02/01

  Minor release to demonstrate dialog, primarily

3.29 Public Beta 4 06/30/01

  Fixed a good number of bugs once again. Improved portability with Julian's
  help, major feature is working demo recording. See info below and changelog
  for more info, as usual.

3.29 Public Beta 3 05/10/01

  Fixed a large number of bugs, added several interesting features. See bug list
  below, and the changelog, for more information.

3.29 Public Beta 2 01/09/01

  Fixed mouse buttons - problem was leftover code in I_GetEvent() that caused
  the mouse button variables to never be asserted - code was unreachable because
  of an assignment of the variable 'buttons' to 'lastbuttons'.

3.29 Public Beta 1 01/08/01

  Initial Release


===============================================================================
* Bugs Fixed, Known Issues *

Bugs Fixed (between 4.02.00 and 4.03.00):

+ Fixed a crash happening when looking at non-power-of-2 midtextures straight
  on their edges. Eliminated the risk of illegal memory access when it would not
  crash.

+ Fixed a glaring portal autoaim failure (it was affecting wads like Heartland).

+ Fixed problems with noise alert propagation through linked portals.

+ Fixed Chainsaw and Fist not being switched away from if dehacked made them use
  ammo.

+ Dehacked-based chainsaw replacement no longer make the rattling sound if not
  intended.

+ Fixed an issue where crushers would desync and permanently close when saving
  and loading.

+ Fixed screen going black when streaming on Discord.

+ Fixed a rare crash potentially happening when changing the level.

+ Fixed the wrong coordinate display in automap when follow mode is off.

+ Fixed a bug where classic format floor and ceiling linked portal specials were
  wrongly finding the like-tagged edge portal flagged linedef as anchor.

+ Fixed a crash happening when using the "idmypos" cheat when the automap wasn't
  started yet and follow mode is off. Easy to reproduce after loading a game.

+ Saving a game failed storing the polyobject activators of ACS scripts.

+ Several linedef and sector properties were NOT saved, even though they should.

+ thinggroup definitions didn't work if they reused thing types! Fixed.

+ Fixed the LineEffect codepointer combined with complex effects (e.g. ACS) and
  saving/loading crashing Eternity. Also fixed a crash happening when using it
  to open locked doors. Under MBF it would work purely due to chance (undefined
  behaviour otherwise).

+ Fixed so LineEffect always uses the DOOM classic format specials, even inside
  UDMF. This also aligns the rules with ZDoom's.

+ Some activation specials (notably those beyond UDMF index 255) didn't work
  with EMAPINFO levelaction.

+ The "chgun" hack apparently hasn't worked for awhile, but nobody was here to
  check it until now.

+ Fix negative ExtraData recordnums causing unpredictable behaviour.

+ Skip __macosx/... files from PKE resources from listing as embedded WADs and
  exiting with error.

+ Invalid entries in EMAPINFO are now detected early.


Known Issues in v4.03.00:

- Moving polyobject portal movement is not interpolated yet.

- "Inconsistent" linked portals may still show monsters as "blind" until you
  walk through them or get close. We will either fix this or add a compatibility
  flag for it.

- There are some problems with slopes and portals when combined. All slope-
  related bugs will be fixed when physics is fully introduced.

- Polyobject interactive portals don't support rotation, so only use them with
  translation motion.

- Do not assume stable physical behavior when using polyobject linked portals
  to lead into other polyobject linked portals. Right now the only supported
  behavior is to use them as vehicles/service lifts/platforms.

- Some momentary interpolation glitches occur when an attached horizontal
  portal surface passes through you.
