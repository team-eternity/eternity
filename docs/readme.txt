===============================================================================
Title                   : The Eternity Engine v4.01.00 "Tyrfing"
Filename                : ee-4.01.00-win32.zip, ee-4.01.00-macos.dmg
Release date            : 2020-07-22
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
                              Randy Heit, Graf Zahl, Braden Obrzut

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
Build Time              : Seventeen years and counting
Editor(s) used          : Visual Studio 2017, Xcode, UltraEdit-32, SLADE,
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
* Features New to Version 4.01.00 *

These include some of the major features added in the latest version:

*** FEATURED ADDITIONS ********************************************************



*** END FEATURED ADDITIONS ****************************************************

- Editing improvements -

  * Fixed Heretic pods to act properly. For this, new flags have been added:
    MONSTERPASS, LOWAIMPRIO, STICKYCARRY, SETTARGETONDEATH, SLIDEOVERTHINGS.
    See the wiki for details.

  * Added hitscan puff type EDF definitions, which is necessary for Heretic
    support. Consideration has also been made for future Hexen and Strife
    support in Eternity. More info on the wiki:
    https://eternity.youfailit.net/wiki/EDF_puff_type_reference

    effects.edf files have been added in base/doom and base/heretic.

    Each pufftype defines how a hitscan attack should spawn a thing of a given
    class, with options such as dragonclaw alternate blast, or Strife mauler
    behavior etc.

    Relevant codepointers have been updated to support pufftype.

  * Added absolute.push and absolute.hop to damagetype. Needed by Heretic's
    powered staff.

  * A_Nailbomb codepointer is now fully customizable.

  * Fixed the custom spread distribution of A_FireCustomBullets to be
    triangular, just like the vanilla Doom hitscans.

  * New polyobject linedef specials: Polyobj_MoveTo, Polyobj_MoveToSpot,
    Polyobj_OR_MoveTo and Polyobj_OR_MoveToSpot, based on the GZDoom extensions.

  * Added the push activation from Hexen. The feature was long advertised in
    ExtraData (and later UDMF) but only now it has been implemented.

  * Added UDMF linedef "monstershoot" boolean field. Needed because the
    ill-named "impact" field only allows players to shoot the linedef.
    This one allows monsters too. The ones who wrote the original UDMF
    specs must have been unaware that vanilla Doom has some specials
    (such as GR: Open Door) which are triggerable by both monsters and
    players, while others (the rising platforms) only by players.

  * Added UDMF sector "phasedlight", "lightsequence" and "lightseqalt"
    boolean fields, because ExtraData supports adding them individually,
    without resorting to the "special" field.

  * Inconsistent linked portals between separate sector islands are now
    supported! This means you can now place "wormholes" in the map without
    getting errors! The link offsets are updated dynamically in order to
    provide a gameplay as good as possible.

    There is still a restriction left: do not place portals between two areas
    already interconnected by sectors. That would still error out.

  * Changed how floating objects move in relation to portal
    polyobjects. They no longer remain fixed relative to the inner
    chamber sectors, but are shifted in the opposite direction, so they
    appear as remaining fixed relative to the main world. This
    behaviour is more consistent with how thing movement isn't
    influenced by traditional vertically moving sectors, and more
    stable as well than just adapting velocity when passing the portal.
    It also avoids unrealistic direction changes when the containing
    polyobject changes movement.

  * Polyobjects with portals no longer require their origin box to be connected
    to sectors from the spawn spot's layer.

  * Visual portals on polyobjects now rotate along with the poly.

  * Added game.skillammomultiplier EDF gameprop specifier to customize ammo
    multiplication in the extreme skill levels.

  * Added a "game.itemheight" property to EDF game properties which
    overrides item pickup height, which is set to 8 in Doom/Strife and to
    32 in Heretic/Hexen.

  * Added a CHASEFAST EDF game property flag. Needed to support Raven games
    where the monster A_Chase frames are sped up on -fast and skill 5.

  * Added the +Skill5Fast prefix flag for non-Decorate frame and framedelta
    definitions.

  * Added some UDMF-exclusive fields to ExtraData: linedef portalid and
    sector portalid.floor and portalid.ceiling, so that Portal_Define
    doesn't require UDMF any longer.

  * F_SKY1 now when applied on middle textures has the same effect as floor
    and ceiling skies.

  * Masked textures (such as mid-grates) can now be applied on portal
    overlays. They can even use the SMMU rippling animation (unlike the
    2-sided linedef midtextures).

  * EMAPINFO skyDelta and sky2Delta now accept fractional values. It
    was appropriate to add this because an ACS function already
    supports fixed-point for changing these states.

    Currently legacy Hexen MAPINFO doesn't have it however.

- Control improvements -

  * Now if an action is bound to more than one key, and two of the keys
    are being pressed, releasing just one of them will not stop the
    action. You need to release all of them.

- Visual improvements -

  * Movement through portals is finally interpolated. No more hops or slight
    discontinuities.
  * Hexen style double skies finally work!
  * Weapon sprite motion is now interpolated (smooth).
  * Polyobject movement is now interpolated, though moving portals not so yet.
  * Polyobjects are now properly cut off by 1-sided walls, instead of their
    graphics "bleeding" through them outside the playable area.
  * Interpolate walkcam even while game is paused.
  * Console sliding is now interpolated.
  * Polyobject portals now show in the map as part of the main area,
    because indeed they rarely ever overlap anything vertically.
  * Chasecam and walkcam are now portal-aware. Interpolation is however
    disabled.
  * Scrolling texture movement (including attached 3dmidtex) is now
    interpolated.

===============================================================================
* Features New to Version 4.00.00 *

These include some of the major features added in the latest version:

*** FEATURED ADDITIONS ********************************************************

- Migration from SDL to SDL2 -

  * Just after the Windows 10's Fall Creators Update to break the SDL1.2
    version in bizarre ways, a complete migration has been made to the SDL2
    framework. This comes with a bevy of changes.

      * "unicodeinput" has been removed. All text input is UTF-8.
      * -8in32 and the software bitdepth setting are gone.
      * A new console variable "displaynum" exists, and lets you set what
        display the window is created on when you run Eternity.
      * A new console command "maxdisplaynum" has been added, which retrieves
        the maximum permitted value that displaynum can take.
      * -directx and -gdi are gone.
      * Support for many types of music has been updated, due to the new dlls
        used by SDL2.

  * Added EDF animation and switch definitions. Added ANIMDEFS support --
    a subset with what Eternity supports.

  * Monster infighting can now be customized. Several EDF features have
    been added. Make sure to update the "base" folder now.

  * Polyobjects now accept clusters of portals inside them.

*** END FEATURED ADDITIONS ****************************************************



===============================================================================
* Coming Soon *

These are features planned to debut in future versions of the Eternity
Engine:

- Priority -

  ** EDF Inventory/Weapons/Pickup system (Partially finished!)
  ** 100% Heretic support                (Major progress made)
  ** Slope physics
  ** PSX Doom support                    (In progress)
  Aeon scripting system
  Hexen Support
  Strife Support
  Double flats
  More standard TerrainTypes

  ** == TOP PRIORITY

- Long-Term Goals -

  Improved network code


===============================================================================
* Revision History *

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

  Minor maintenance release with numerous interface improvements, and a new
  EDF font specification system.

3.35.90 "Simorgh" -- 01/11/09

  It took two years and almost 450 SVN revisions to create this new
  incarnation of the Eternity Engine.

3.33.50 "Phoenix" -- 10/23/06

  Sad times have given way to happy ones as the energy to do great work
  has been unleashed.

3.33.33 "Paladin" -- 05/17/06

  The most work ever done for a single release possibly excepting only the
  port of v3.29 Gamma to Windows. We apologize for the delay, but it is
  well worth the wait.

3.33.02 "Warrior" -- 10/01/05

  A very respectable amount of progress hopefully offsets the terrible
  delay experienced for this release. 3D object clipping improvement,
  new EDF TerrainTypes engine, and various EDF improvements are the
  most significant features.

3.33.01 "Outcast" -- 06/24/05

  Minor fixes, some user-requested and CQIII-needed features implemented,
  and HUD scripting functions for Small.

3.33.00 "Genesis" -- 05/26/05

  Small scripting finally complete and available to the user. Lots of
  fixes, tweaks, and feature additions on top of that.

3.31.10 "Delta" -- 01/19/05

  More Small support, ExtraData, global MapInfo, and tons of other new
  features and bug fixes make this an extremely critical release.

3.31 Public Beta 7 04/11/04

  High resolution support enabled via generalization of the screen
  drawing code. Some minor fixes to portals, and a new codepointer just
  for good measure.

3.31 Public Beta 6 02/29/04

  A huge amount of progress toward all current goals for the engine.
  Portals, the brain child of SoM, are the major stars of this release.
  Small and ExtraData are on the verge of becoming available to users
  for editing.

3.31 Public Beta 5 12/17/03

  Several minor bug fixes over beta 4, including 3D object clipping and
  file drag-and-drop. Incremental improvements to EDF and to several
  parameterized codepointers.

3.31 Public Beta 4 11/29/03

  Most effort put into improving EDF, largely via user feature requests.
  Major progress has been made toward working ExtraData as well. The
  Heretic status bar is now implemented.

3.31 Public Beta 3 08/08/03

  A long time in coming, and a ton of features to show for the delay.
  EDF steals the show for this release.

3.31 Public Beta 2 03/05/03

  New BFGs, object z clipping, movable 3DMidTex lines, tons more Heretic
  support, and some really nasty bugs fixed. Also first version to use
  SDL_mixer for better sound.

3.31 Public Beta 1 09/11/02

  Prelim. Heretic support, 3DMIDTEX flag, zdoom-style translucency, and
  some other stuff now in. And of course, bug fixes galore.

3.29 "Gamma" 07/04/02

  Final release for v3.29. Massive changes all over. First version to
  include Windows support. SoM is now co-programmer.

3.29 Development Beta 5 10/02/01

  Minor release to demonstrate dialog, primarily

3.29 Public Beta 4 06/30/01

  Fixed a good number of bugs once again. Improved portability with
  Julian's help, major feature is working demo recording. See info below
  and changelog for more info, as usual.

3.29 Public Beta 3 05/10/01

  Fixed a large number of bugs, added several interesting features. See
  bug list below, and the changelog, for more information.

3.29 Public Beta 2 01/09/01

  Fixed mouse buttons - problem was leftover code in I_GetEvent() that
  caused the mouse button variables to never be asserted - code was
  unreachable because of an assignment of the variable 'buttons' to
  'lastbuttons'.

3.29 Public Beta 1 01/08/01

  Initial Release


===============================================================================
* Bugs Fixed, Known Issues *

Bugs Fixed (since 4.01.00):

+ Fixed polyobjects not setting up their linedefs' sound origins correctly at
  spawn.

+ Fixed two-way non-interactive portals generated by Line_QuickPortal or
  Portal_Define on one-sided linedefs not showing the upper and lower textures,
  unlike the linked portals.

+ Concealed sector portals were previously missed by the blockmap, resulting in
  buggy behavior when clipping against things inside some through-portal elevators.

+ Fixed weird texture glitches happening with multiple polyobjects in a sector,
  some still and some moving.

+ 2-sided polyobjects now show correctly in Eternity. The back side was hidden,
  which was a regression from vanilla Hexen's polyobjects.

+ Fixed an off-by-one overflow (otherwise harmless) which caused transparent
  midtextures to show extra black (or index 0) pixels below most columns.

+ Sounds with "alias" or "random" entries didn't work correctly in sound
  sequences with "playrepeat" and other similar commands.

+ Fixed wrong Heretic sound sequences, by adding more game mode specific
  implementations in EDF.

+ Fixed the ceiling movement sound sequence from being cut off on stop.
  This was breaking mods which were replacing DSSTNMOV with something longer,
  which would only fully play when a ceiling stopped moving.

+ Fixed a buffer reading runaway when a demo has no marker at the end. Thanks
  to vita for the bug fix.

+ The Heretic skills 1 and 5 now multiply the ammo per item correctly.

+ Fixed ChangeSkill (linedef special) not updating the fast parameter.

+ Fixed a lockup caused by having an EDF animation block of different texture
  type than something already defined internally.

+ Fixed a regression with portal autoaiming.

+ Fixed a problem with the macOS launcher putting IWADs in the PWAD list.

+ Fixed a rare glitch with the macOS launcher app, causing a startup crash.

+ Fixed a situation where linedefs would still block despite being in different
  portal layers.

+ Fixed a problem where particles appeared in the wrong place, because of nearby
  portals.

+ Fixed edge-case situations where line portal teleportation would fail, such as
  when sliding really slowly along a wall, or almost parallel to one.

+ Fixed several cases where portal interpolation would fail displaying correctly.

+ Fixed a bug where the walkcam would move a bit backwards when opening console.

+ Fixed objects standing on the outer edges of portal polyobjects not updating
  their floor reference Z.

+ Fixed polyobject doors which rotate more than 180 degrees from
  malfunctioning when blocked by things when closing.

+ Fixed the wrong drawing of Heretic weapons when no status bar is drawn.
  Added a "fullscreenoffset" floating-point field in weaponinfo for that,
  because Heretic weapons have different hardcoded offsets.

+ Fixed a memory leak where things with alphavelocity would never be
  deallocated and gradually slow the game to a crawl.

+ Fixed a crash happening with slopes having MBF-transferred skies.

+ Fixed wrong drawing of animated textures on slopes when the textures are
  of different sizes.

+ Fixed a crash happening with sub-unit linedef length maps (made in UDMF).
  Now the slime-trail reduction function uses floating-point.

+ The scrolling sky scrolling speed, changeable through ACS, wasn't stored
  in save games.


Known Issues in v4.01.00:

- Moving polyobject portal movement is not interpolated yet.

- "Inconsistent" linked portals may still show monsters as "blind" until you
  walk through them or get close. We will either fix this or add a compatibility
  flag for it.

- There are some problems with slopes and portals when combined. All
  slope-related bugs will be fixed when physics is introduced.

- Polyobject portals don't support rotation, so only use them with translation
  motion.