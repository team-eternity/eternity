===============================================================================
Title                   : The Eternity Engine v4.02.00 "Forseti"
Filename                : ee-4.02.00-win64.zip, ee-4.02.00-macos.dmg,
                          ee-4.02.00-win32.zip, ee-4.02.00-win-legacy.zip
Release date            : 2020-10-13
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
                              Anders Astrand
                              anotak
                              Cephaler
                              Charles Gunyon
                              Shannon Freeman
                              dotfloat
                              Gez
                              Joe Kennedy
                              Joel Murdoch
                              Julian Aubourg
                              Kaitlyn Anne Fox
                              Mike Swanson
                              MP2E
                              Russell Rice
                              ryan
                              Samuel Villarreal
                              SargeBaldy
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
                          Notepad++, GZDoom Builder
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

Please email haleyjd@hotmail.com with any concerns.


===============================================================================
* Features New to Version 4.02.00 *

This release is small enough that all feature changes are going to be listed
uncategorised (outside of bug fixes).

  * Renderer size is no longer tied to window size, meaning you can have
    lower-resolution render targets scaled up to a larger screen.
  * More compatibility hacks have been added to better-support levels that have
    specific comp_ setting requirements.
  * Looping sounds are now preserved whilst the game is paused, resuming after
    the pause ends.
  * SetThingSpecial now actually sets the thing's special.
  * Masked columns (columns which can have transparent parts) now support tall
    patches.
  * Holding shift whilst always running now makes you walk by default.
  * The renderer backbuffer is now transposed, which improves performance in
    some scenarios and will allow for further optimisations down the line.

===============================================================================
* Coming Soon *

These are features planned to debut in future versions of the Eternity Engine:

- Priority -

  ** Slope physics
  ** Aeon scripting system
  ** 100% Heretic support                (Major progress made)
  Hexen Support
  Strife Support                         (In progress)
  PSX Doom support                       (In progress)
  Double flats

  ** == TOP PRIORITY

- Long-Term Goals -

  Improved network code


===============================================================================
* Revision History *

4.02.00 "Forseti" -- 24/01/21

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

Bugs Fixed (between 4.01.00 and 4.02.00):

+ Fixed A_Turn not working properly when using misc1 (dehacked).

+ Fixed a hard-lock that occurred if an in-motion polyobject had the same
  position for two frames in a row.

+ Fixed slopes sometimes rendering incorrectly for high scale values.

+ Fixed blockmap generation if the 0th vertex has the largest x or y.

+ Tentatively fixed an issue where portals could cause bad intercept traversal,
  causing hitscans to get eaten.


Known Issues in v4.02.00:

- Moving polyobject portal movement is not interpolated yet.

- "Inconsistent" linked portals may still show monsters as "blind" until you
  walk through them or get close. We will either fix this or add a compatibility
  flag for it.

- There are some problems with slopes and portals when combined. All slope-
  related bugs will be fixed when physics is introduced.

- Polyobject interactive portals don't support rotation, so only use them with
  translation motion.

- Do not assume stable physical behavior when using polyobject linked portals
  to lead into other polyobject linked portals. Right now the only supported
  behavior is to use them as vehicles/service lifts/platforms.

- Some momentary interpolation glitches occur when an attached horizontal
  portal surface passes through you.
