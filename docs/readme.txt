===============================================================================
Title                   : The Eternity Engine v4.01.00 "Tyrfing"
Filename                : ee-4.01.00-win32.zip, ee-4.01.00-macos.dmg
Release date            : 2020-07-24
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

Introduced EDF weapons:

  * Users can now define brand new weapons.
  * All the original Doom weapons are now defined through this system.
  * Almost all Heretic weapons are now defined through this system (the others
    aren't defined at all).
  * Almost all of the Heretic weapon codepointers have been added.
  * Additionally, weapondeltas have been added.

Introduced EDF pickupeffects:

  * Users can define brand new pickups.
  * These can define either thing-based or sprite-based pickups. They can also
    be defined externally, or inside a thingtype definition.
  * They encompass health, armor, ammo, power-ups, weapons and artifacts.
  * All the Doom and Heretic pickups now use this system.
  * >>> Pickupitems are deprecated, please do not use them <<<

Added hitscan puff type EDF definitions, which is necessary for Heretic
support:

  * Consideration has also been made for future Hexen and Strife
    support in Eternity. More info on the wiki:
    https://eternity.youfailit.net/wiki/EDF_puff_type_reference

  * effects.edf files have been added in base/doom and base/heretic.

  * Each pufftype defines how a hitscan attack should spawn a thing of a given
    class, with options such as dragonclaw alternate blast, or Strife mauler
    behavior etc.

  * Relevant codepointers have been updated to support pufftype.

New (modern) HUD:

  * Added the new Modern HUD (with graphics courtesy of ptoing), which supports 
    all the layouts the BOOM HUD does as well. Along with this the Modern HUD
    and flat layouts have been made to be the new default for overlay-style
    HUDs.
    
  * Added an inventory bar to Doom. Currently it only works when not using the
    regular Doom HUD, and currently selected items are not rendered  outside of
    when the inventory bar is active.

Added libADLMIDI support, which enables OPL3 MIDI playback. The default
emulator is Nuked OPL3 1.8, with 2 chips, and DMXOPL as the bank.

*** END FEATURED ADDITIONS ****************************************************

- Editing improvements -

  EDF, DeHackEd and Gameplay Modding Stuff

  * The edf "include" directive can now include files with longer names or using
    paths (e.g.: "whereveredfis/monsters/somethingidunno.edf"). This is case-
    insensitive, and all \s are converted to /s when internally hashed.
    Additionally these includes are relative to the file making the include
    (e.g: if "edf/monsters.edf" does "include("monsters/something.edf")" it will
    include "edf/monsters/something.edf"). Includes can be made non-relative by
    putting a '/' before the rest of the path.

    IMPORTANT: . and .. are not supported.

  * Fixed Heretic pods to act properly. For this, new flags have been added:
    MONSTERPASS, LOWAIMPRIO, STICKYCARRY, SETTARGETONDEATH, SLIDEOVERTHINGS. See
    the wiki for details.

  * Added "damagemod" thing property, which allows damage randomization. Thanks
    to Xaser for the addition!

  * Added damagefactor.remove and cleardamagefactors to thingtype and thingdelta
    to be able to remove them from the list.

  * Added the +Skill5Fast prefix flag for non-Decorate frame and framedelta
    definitions.

  * Allowed 0-tic initial states if the states are DECORATE, and don't have
    their duration modified—or aren't made the spawnstate of a thing—by 
    DeHackEd.

  * Added the offset(,) Decorate state specifier which exists in ZDoom and is
    used to set the misc1 and misc2 flags, which are also capable of setting
    weapon coordinates.

    Eternity is also capable of interpolation when using offset(). By default
    it's disabled, but you can enable it by using the "interpolate" specifier as
    a third argument, for example: offset(1, 32, interpolate). When inheriting
    via the Decorate syntax and reapplying offset(), interpolation will be
    disabled again unless you specify it again.

    For the standard "frame" EDF section, you can use the +|-interpolate
    specifier to add or remove the flag.

  * Added the DOOMWEAPONOFFSET gameproperties flag which is already enabled for
    DOOM (solely for Dehacked and past EDF mod compatibility) and enables the
    less intuitive DOOM rule on when offsets have effect. Otherwise the better
    Hexen rule applies.

  * A_Nailbomb codepointer is now fully customizable.

  * Made A_Jump work for weapons with DECORATE states.

  * Fixed the custom spread distribution of A_FireCustomBullets to be
    triangular, just like the vanilla Doom hitscans.

  * Added range parameter to A_CustomPlayerMelee. This displaces the new-ish
    pufftype parameter, punting that to last position.

  * Extended A_PainAttack to have ZDoom's parameters.

  * Extended A_PainDie to have thingtype arg.

  * Added A_SeekerMissile, based on the ZDoom wiki docs for the same 
    codepointer.

  * Added A_CounterDiceRoll, which can do a TTRPG-style damage dice calculation.

  * Added A_SpawnEx, A_SelfDestruct and A_MushroomEx. Thanks to Xaser!

  * Parameterised A_Turn to be able to take a constant value, or using a counter
    as either a BAM or degrees.

  * Add A_TurnProjectile which is A_Turn but updates momentum for projectiles.

  * Added "noammo" to A_FirePlayerMissile. Thanks to Xaser for the addition!

  * New weapon flag PHOENIXRESET, which implements the hacky Heretic way of
    resetting the Phoenix Rod from the powered state. Its drawback is that
    sometimes it can consume more ammo than intended. You can still use
    FORCETOREADY as a more general-purpose counterpart.

  * Now the TINTTAB Heretic lump works as a BOOM-like translucency map for the
    ghost effect.

  * Added absolute.push and absolute.hop to damagetype. Needed by Heretic's
    powered staff.

  * Now playerclass has two new fields: maxhealth (default 100) and superhealth
    (default = maxhealth). They're needed for some built-in effects, mainly the
    HealThing special and the powered A_GauntletAttack.

  * EDF player classes now support an "+AlwaysJump" option, which will enable
    jumping regardless of compatibility setting or EMAPINFO disable-jump. Also,
    setting "speedjump" to 0 will completely disable jumping for that class.

  * Added "viewheight" playerclass property. Thanks to Xaser for the addition!

  * Added "speedjump" playerclass property. Its value is a fixed, and is how
    much upwards speed is added to momz when the player successfully jumps.

  * Added EDF playerdelta.

  * Add "clearrebornitems" flag for playerdeltas. Fix rebornitem allocation
    logic.

  * Updated the EDF healtheffect items to use the new @maxhealth and
    @superhealth keywords instead of absolute values to refer to the current
    playerclass values. They're safely replaceable by Dehacked and healthdelta
    structures.

  * Introduced support for Heretic-style inventory. Items can only be used in
    Heretic at the moment, but support will be extended to other IWADs. All the
    Heretic artifacts now use this system (except for the egg).

  * Split armor's +bonus into +additive and -setabsorption. Added the RAMBO
    cheat from Heretic, and improved givearsenal to give armor in Heretic too.

  * Added a CHASEFAST EDF game property flag. Needed to support Raven games
    where the monster A_Chase frames are sped up on -fast and skill 5.

  * Added game.skillammomultiplier EDF gameprop specifier to customize ammo
    multiplication in the extreme skill levels.

  * Added a "game.itemheight" property to EDF game properties which overrides
    item pickup height, which is set to 8 in Doom/Strife and to 32 in
    Heretic/Hexen.

  * Allow title music to be changed via the EDF gameproperties block. Thanks to
    Xaser for the addition!

  * Finally added support for Dehacked "Monsters Infight" misc setting.

  * Added the vast majority of dehacked extensions made by Doom Retro. Allow 
    non-contiguous sprite frames (e.g. TROO G is missing but F and H are 
    present).

  * Extend dehacked to be able to set drop item.

  * Added EDF fontdelta.

  * Implemented deltas for itemeffects (healtheffect, armoreffect, ammoeffect,
    powereffect, weapongiver, artifact). Replace effect with delta for those 
    ending with effect, and append delta to the end of the others to get the
    name of the appropriate delta structure. weapongiverdelta also has
    clearammogiven, which does what it says on the tin.

    The artifact's args are implicitly cleared if a delta specifies a useaction
    or args in a delta structure targetting said artifact.

  * Re-implemented the MBF pickups.

  * Wherever a string placeholder is used you can use either an EDF string or a
    BEX string. Thanks to marrub for the addition!

  * Sound definitions now allow long lump file names in the definitions, such
    as:
      sound mpistolf {lump "/lsounds/pistol/fire.wav"}
    Thanks to marrub for the addition!

  * Added the UNSTEPPABLE thing flag, needed by the Heretic gameplay.

  * New delta structure, splashdelta. It's similar to terraindelta but applies 
    to splash definitions.


  Level Editing Stuff

  * Added the Generic_Lift special, which ZDoom had added, because there are a
    few Boom generalized specials which can't be represented by other 
    parameterized lift specials.

  * Added the Generic_Stairs special. Needed for proper support of BOOM 
    generalized stairs, which the current parameterized specials aren't capable
    of.

  * Added Ceiling_Waggle action special.

  * New polyobject linedef specials: Polyobj_MoveTo, Polyobj_MoveToSpot,
    Polyobj_OR_MoveTo and Polyobj_OR_MoveToSpot, based on the GZDoom extensions.

  * Added SetWeapon and CheckWeapon ACS functions.

  * Thing_Destroy now works like in GZDoom, unless level is vanilla Hexen (where
    it will still support arg 3). Most notably, arg 2 is now "flags" and 1 means
    "extreme death".

  * Added support for CPXF_ANCESTOR to CheckProximity.

  * Added the push activation from Hexen. The feature was long advertised in
    ExtraData (and later UDMF) but only now it has been implemented.

  * Added UDMF linedef "monstershoot" boolean field. Needed because the ill-
    named "impact" field only allows players to shoot the linedef. This one
    allows monsters too. The ones who wrote the original UDMF specs must have
    been unaware that vanilla Doom has some specials (such as GR: Open Door)
    which are triggerable by both monsters and players, while others (the rising
    platforms) only by players.

  * Added UDMF sector "phasedlight", "lightsequence" and "lightseqalt" boolean
    fields, because ExtraData supports adding them individually, without
    resorting to the "special" field.

  * Added some UDMF-exclusive fields to ExtraData: linedef portalid and sector
    portalid.floor and portalid.ceiling, so that Portal_Define doesn't require
    UDMF any longer.

  * Now ExtraData mapthings also support names in the "special" field, just like
    linedefs. It was badly missing.

  * F_SKY1 now when applied on middle textures has the same effect as floor and
    ceiling skies.

  * EMAPINFO skyDelta and sky2Delta now accept fractional values. It was
    appropriate to add this because an ACS function already supports fixed-point
    for changing these states.

    Currently legacy Hexen MAPINFO doesn't have it however.


  Portals and Polyobjects

  * Inconsistent linked portals between separate sector islands are now 
    supported! This means you can now place "wormholes" in the map without 
    getting errors! The link offsets are updated dynamically in order to provide
    a gameplay as good as possible.

    There is still a restriction left: do not place portals between two areas
    already interconnected by sectors. That would still error out.

  * Changed how floating objects move in relation to portal polyobjects. They no
    longer remain fixed relative to the inner chamber sectors, but are shifted
    in the opposite direction, so they appear as remaining fixed relative to the
    main world. This behaviour is more consistent with how thing movement isn't
    influenced by traditional vertically moving sectors, and more stable as well
    than just adapting velocity when passing the portal. It also avoids
    unrealistic direction changes when the containing polyobject changes
    movement.

  * Polyobjects with portals no longer require their origin box to be connected
    to sectors from the spawn spot's layer.

  * Visual portals on polyobjects now rotate along with the poly.

  * Masked textures (such as mid-grates) can now be applied on portal overlays.
    They can even use the SMMU rippling animation (unlike the 2-sided linedef
    midtextures).

  * Plane portals can now render sloped visplanes, with the slope copied from
    the source sector. Note that horizon portals still lack this feature, due to
    how they were designed.

  * Self-tagging Portal_Define flipped-anchor visual portals are now possible,
    though there'll be no mirror.


- Sound improvements -

  * Added support for stereo WAVs (8 and 16 bit). Additionally relax WAV
    loading, so that WAVs with incorret chunk size will successfully load.

  * Made midiproc shut down if Eternity isn't running.

  * Added console command "s_stopmusic" as well as the alias "stopmus".

  * Made it so if Eternity sets its Windows mixer volume it will not use the
    volume it was set to during its runtime.

  * Fixed replacing music when both tracks have a length of 4 (after the "D_").
    Thanks to JadingTsunami for the code fix and esselfortium for reporting the
    bug with a minimum failing case.


- Control and user experience improvements -

  * Now if an action is bound to more than one key, and two of the keys are
    being pressed, releasing just one of them will not stop the action. You need
    to release all of them.

  * Added turning sensitivity for joysticks.

  * You can now use level lump names with -warp at the command line.

  * Jumping is now a compatibility setting. By default it's disabled. If player
    attempts to jump anyway, a message will be shown that it needs to be
    explicitly enabled in settings.

  * New console command: warp, which, like in ZDoom, teleports player to given
    coordinates.

  * Added support for the Rekkr stand-alone IWAD.

  * Added r_drawplayersprites, which lets players choose not to render weapons
    if they don't want to (for example, if taking photos).

  * Made it so the Windows close button on the console is re-enabled when
    Eternity is closed.

  * Changed the key bindings menus.

  * Made Eternity use WASAPI on Windows Vista and later.

  * Made the "idmypos" cheat toggle location display. Additionally, a new
    console variable "hu_alwaysshowcoords" has been introduced to control the
    same thing.

  * Added an option that allows you to set message alignment to either default,
    left, or centred.

  * Made FoV save in configs.

  * Extended -file to search DOOMWADPATH directories if required.

  * Made "summon" console command check thing num for compat name if no thing is
    found for the regular name.

  * Increased repeat backspace speed. Before it only accepted a repeat input
    every 120ms.

  * Made it so your aim is always the centre of the actual non-HUD viewport.
    This removes the need to move the crosshair up/down depending on player
    pitch.

  * Split "Fullscreen Desktop" into its own option for favoured screen type.

  * Added the -recordfromto command-line parameter from PrBoom+. Thanks to vita
    for adding it!

  * Improved OpenGL performance on some drivers. Thanks to icecream95 for the
    contribution!


- Visual improvements -

  * Significantly improved performance in maps with lots of flats (such as the
    central area of Mothership).
  * Movement through portals is finally interpolated. No more hops or slight
    discontinuities.
  * Hexen style double skies finally work!
  * Weapon sprite motion is now interpolated (smooth).
  * Polyobject movement is now interpolated, though moving portals not so yet.
  * Polyobjects are now properly cut off by 1-sided walls, instead of their
    graphics "bleeding" through them outside the playable area.
  * Interpolate walkcam even while game is paused.
  * Console sliding is now interpolated.
  * Polyobject portals now show in the map as part of the main area, because
    indeed they rarely ever overlap anything vertically.
  * Chasecam and walkcam are now portal-aware. Interpolation is however 
    disabled.
  * Scrolling texture movement (including attached 3dmidtex) is now 
    interpolated.

===============================================================================
* Coming Soon *

These are features planned to debut in future versions of the Eternity Engine:

- Priority -

  ** Slope physics
  ** Aeon scripting system
  ** 100% Heretic support                (Major progress made)
  Hexen Support
  Strife Support
  PSX Doom support                       (In progress)
  Double flats

  ** == TOP PRIORITY

- Long-Term Goals -

  Improved network code


===============================================================================
* Revision History *

4.01.00 "Tyrfing" -- 07/24/20

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

Bugs Fixed (since 4.01.00):

+ Fixed polyobjects not setting up their linedefs' sound origins correctly at
  spawn.

+ Fixed two-way non-interactive portals generated by Line_QuickPortal or
  Portal_Define on one-sided linedefs not showing the upper and lower textures,
  unlike the linked portals.

+ Concealed sector portals were previously missed by the blockmap, resulting in
  buggy behavior when clipping against things inside some through-portal
  elevators.

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

+ Fixed the ceiling movement sound sequence from being cut off on stop. This was
  breaking mods which were replacing DSSTNMOV with something longer, which would
  only fully play when a ceiling stopped moving.

+ Fixed a buffer reading runaway when a demo has no marker at the end. Thanks to
  vita for the bug fix.

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

+ Fixed several cases where portal interpolation would fail displaying
  correctly.

+ Fixed a bug where the walkcam would move a bit backwards when opening console.

+ Fixed objects standing on the outer edges of portal polyobjects not updating
  their floor reference Z.

+ Fixed polyobject doors which rotate more than 180 degrees from malfunctioning
  when blocked by things when closing.

+ Fixed the wrong drawing of Heretic weapons when no status bar is drawn. Added
  a "fullscreenoffset" floating-point field in weaponinfo for that, because
  Heretic weapons have different hardcoded offsets.

+ Fixed a memory leak where things with alphavelocity would never be deallocated
  and gradually slow the game to a crawl.

+ Fixed a crash happening with slopes having MBF-transferred skies.

+ Fixed wrong drawing of animated textures on slopes when the textures are of
  different sizes.

+ Fixed a crash happening with sub-unit linedef length maps (made in UDMF). Now
  the slime-trail reduction function uses floating-point.

+ The scrolling sky scrolling speed, changeable through ACS, wasn't stored in
  save games.

+ Fixed PointPush_SetForce not using argument for magnitude when tagging 
  sectors.

+ Fixed a crash happening when seeing for the first time progressively larger
  swirling textures.

+ Fixed a crash happening when loading a game with alpha-cycling sprites.

+ Fixed a serious crash happening when having 3D clipping on, going first to a
  map with linked portals, then going to a map without portals. The crash would
  occur randomly and not always.

+ Fixed ACS function CheckProximity failing to work when counting players.

+ Fixed EDF thingtype droptype to actually remove inherited object's item.

+ If there are multiple textures with the same name in TEXTURE1+TEXTURE2, the
  first one should be loaded, not the last. This fixes mods such as Army of
  Darkness DOOM, which looked wrong in Eternity.

+ For macOS, fixed the failure to start Eternity from Terminal if its locale was
  set to non-English settings. This may apply to other UNIX-like systems too.

+ Stop HealThing from attempting to heal dead bodies. Crazy stuff would happen,
  such as the view getting garbled and Eternity crashing.

+ Fixed a sky rendering HOM happening in maps like Sargasso level 2 and any
  place where both floor and ceiling of a zero-height sector have F_SKY1 and
  it's expected to see a sky wall. Eternity would show HOM, whereas both
  Chocolate-Doom and PrBoom would render the sky properly.

+ Fixed EDF thingtype translation 14 (white) not having any effect.

+ Fixed buggy rendering of masked middle textures when mixing sector colormaps
  with player invulnerability or light amplification.

+ Fixed the "ignoreskill" flag of "ammoeffect" not working as advertised.

+ Fixed 1-sided linedef wall portals showing HOM if their middle texture isn't
  set.

+ Fixed the ChangeSkill linedef special acting completely wrong when called from
  ACS, and crashing when such a script is triggered from the console.

+ Fixed a crash happening when entering a sector with an invalid colormap
  specified in the UDMF editor or ExtraData.

+ Odd-duration frames now restore correctly when returning to nightmare mode to
  normal.

+ Fixed the CLIPMIDTEX linedef flag not working over portals.

+ Fixed an occasional crash happening when starting maps with things with
  negative doomednums.

+ Fixed the FLIES thingtype particlefx from sounding badly.

+ Fixed ev_text events causing some issues, such as menu navigation occurring
  when inserting specific keys for key bindings on certain menus.

+ Fixed the FOV parameter for A_JumpIfTargetInLOS, which was read in as an int
  FRACUNIT times smaller than it should have been before being converted to an
  angle. It is now read in as a fixed.

+ Made A_AlertMonsters work properly for weapons. Now it actually just alerts
  all the things without being contingent on you pointing your gun at an enemy.
  Thanks to Xaser for this.

+ A_CustomPlayerMelee now correctly turns you through portals.

+ Fixed an issue with window grabbing not working.

+ Fixed next and previous weapon (weapon scrolling) in vanilla recordings.

+ Fixed a crash caused by ANIMATED lumps of size 0. It hard errors now.

+ Fixed crashes due to not being able to handle floating 32-bit audio samples.

+ Fixed cfg saving ignoring symlinks (and just overwriting the link).

+ Fixed crashing if the CONSOLE lump isn't a valid graphic.

+ Fixed HOM happening when trying to render the same portal from two separate 
  windows.

+ Fixed crashes happening when changing the NOBLOCKMAP and NOSECTOR thing
  flags via SetFlags/UnSetFlags.

+ Fixed wrong double check of standby monsters for shadowsphere stealth players.


Known Issues in v4.01.00:

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
