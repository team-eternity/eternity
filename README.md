![img](./Eepicon.png)

The Eternity Engine Plus
========================

Eternity Engine Plus - is my ([DRON12261](https://github.com/dron12261games)) fork of [Eternity Engine](https://github.com/team-eternity/eternity) by Team Eternity, designed to expand its capabilities, particularly in terms of modding and mapping. I'm developing it based on my own programming skills and the needs of my [ETERNUM](https://github.com/dron12261games/RES-Eternum-Resource-Pack) project (a large resource pack for mappers).

I do not intend for this port to be entirely independent. I plan to submit all my improvements and changes as PRs to the main Eternity Engine repository. However, since development in the Eternity Engine is quite slow and it sometimes takes a long time for PRs to be accepted (or even rejected), I think it would be a good idea to have a separate fork that provides early access to my innovations without having to wait for them to appear in the main branch. I also want to keep this fork up to date with the latest developments in Eternity Engine itself, at least until it causes major issues or until I decide to abandon this project altogether.

**Eternity Engine Plus current version:** `0.1.0` (WIP)

**Base Eternity Engine version:** `4.06.00` (WIP)

### Project wiki with all info about new features:
`placeholder (WIP)`

## New features
New ACS functions:
* void Give Inventory(str item_name, int amount, bool silent) - Can be used to give all types of items, as well as health, armor, weapons, backpacks, and all types of power effects to the player. Negative value of amount will mean to give maximum.
* void ClearInventory() - Takes all armor, backpack, weapons and ammunition, keys, all powers and all items except those marked with the UNDROPPABLE flag.
* int UseInventory(str item_name) - Uses and consume the item if the player has it in their inventory. Returns true if the use is successful, otherwise false.
* int GetMaxInventory(int tid, str item_name) - Returns the current maximum number of items a player can carry. If a health/armor item is passed, returns the maximum amount of health/armor that this type of item can heal/repair. If PowerArtifact is passed, it returns the duration of this artifact (if it is infinite, it returns -1). If type of PowerEffect is passed directly, it returns -1 if effect is infinite by default, otherwise 1.
* int GetArmorInfo(int infotype) - Allows to directly get information about the current state of the player's armor. If infotype = 0, the current amount of armor will be returned; if 1, savefactor will be returned; if 2, savedivisor will be returned. Otherwise, 0.
* str GetSectorColormap(int tag, int type) - Returns the name of the colormap of the first sector with the specified tag.
* int SetSectorColormap(int tag, int type, str colormap, bool isBoom) - Changes the colormap in the specified sector.
* int IsSectorColormapBoomkind(int tag, int type) - Returns 1 if the colormap in the sector uses the old Boom logic. Returns 0 if it uses the new Eternity (or SMMU) logic. Returns -1 on error (for example, if the sector cannot be found, or an invalid type is specified).

Rewritten/improved old ACS functions:
* void TakeInventory (str item_name, int amount) - Now, it can be used to take all types of items, as well as health, armor, weapons, backpacks, and all types of power effects from the player. Also, if the function is passed a amount of item that is greater than the player actually has, the item will be taken away completely (previously, it was necessary to specify a number of items equal to what was in the player's inventory, which was a bug). Negative value of amount will mean to take all.
* int CheckInventory (str item_name) - Now, it can be used to check all types of items, as well as health, armor, weapons, backpacks, and all types of power effects with the player.

New Dehacked/EDF codepointers:
* A_LightEx(int lightDelta) - Parameterized version of A_Light0, A_Light1, and A_Light2. lightDelta - a number indicating how much the render brightness around the player should change. Positive values will make everything brighter, while negative values will make everything darker.

New thing type flags:
* NOSEEKINVISIBLE - prevents the seeker from targeting players with invisibility powerup active
* IGNORESEEKER - prevents them from being targeted by seekers

New QoL features:
* New cheat codes for automap: IDDKT/RAVKMAP, IDDIT/RAVIMAP, IDDST/RAVSMAP.
* PowerFlight, PowerAllMap, and PowerSilencer can now have limited duration and be modded in EDF using the permanent, additivetime, and overrideself flags, just like other powers. This will not affect vanilla Doom and Heretic, nor will it affect cheats, and the fix is aimed at modding capabilities in EDF. However, PowerStrength remains infinite regardless of duration settings.

## Bug fixes:
* Fixed a freeze with an infinite loop when the player had only "Unknown" (dummy weapon) left and tried to change a weapon.
* Fixed a game crash that occurred when a player had only one weapon left and, after firing it, ran out of ammo and attempted to switch to a more suitable weapon (which did not exist).
* Fixed a bug where a player's all weapons were taken via TakeInventory, but they still had the last weapon they used in their hands (even though that weapon was no longer in the player's inventory).
* Fixed a bug with the "alwayspickup" flag for Health Effect and ArmorEffect. Previously, if a player picked up a
health/armor item with this flag, whose maxamount/maxsaveamount was less than the player's current amount,
the player's health/armor would be reset to the maxamount/maxsaveamount of the picked up item. Now, in such
a situation, the player's current health/armor will not be reset and will remain unchanged.
* Fixed a bug in the ACS SetActorProperty function where new health was not applied to the player via APROP_Health.

## How to get started with mapping/modding:
`placeholder (WIP)`







The Eternity Engine (original README.md)
========================================

Eternity is an advanced [DOOM](http://doomwiki.org/wiki/Doom) source
port maintained by James "Quasar" Haley, descended from Simon
"fraggle" Howard's SMMU. It has a whole host of advanced features
for editors and players alike, including:

* [ACS](http://doomwiki.org/wiki/ACS), including many of ZDoom's
  enhancements

* [EDF](http://eternity.youfailit.net/index.php?title=EDF), Eternity
  Definition File, a language to define and modify monsters,
  decorations, sounds, text strings, menus, terrain types, and other
  kinds of data.

* [ENDOOM](http://doomwiki.org/wiki/ENDOOM) and animated startup screen
  support.

* High-resolution support (practically unlimited).

* Support for _Master Levels_ and _No Rest for the Living_, allowing
  to play them without command line options.

* Portals which can be used to create skyboxes and fake 3D
  architecture. Linked portal allow objects to pass through them, as
  well.

* [PNG](http://www.libpng.org/pub/png/) support

* Aided with [SDL 2](http://libsdl.org/), Eternity is very portable and
  runs on a large range of operating systems: Windows (confirmed as
  low as XP, and all the way through Windows 10), Linux, Mac
  OS X, FreeBSD, OpenBSD, and more.

* Heretic, Hexen, and Strife support in-progress.

For more on its features, check out the
[Eternity Engine Wiki](http://eternity.youfailit.net/index.php?title=Main_Page).

Eternity Engine is maintained using the Git version control system,
and the canonical source of the repository is available at
[GitHub](https://github.com/team-eternity/eternity).

Compiling
---------
There are four ways available for building Eternity: CMake, Visual
Studio, Xcode files, and Homebrew, for Unix, Windows, and both
Mac OS X respectively.

Git submodule
-------------
The project contains the ADLMIDI submodule. You need to load it after you've just cloned Eternity, by using this command:

```
git submodule update --init
```

Building with CMake (Unix OSes and Visual Studio 2017+)
-------------------------------------------------------
CMake should be capable of generating build files for all platforms,
but it is most commonly used only for Unix OSes and Windows, and not
thoroughly tested outside of it.

1. If you haven't already, extract the source *.zip file or clone the
Git repository, in the top-level directory you should see a
+CMakeLists.txt+ file. You should be in this directory.

2. Create a new empty directory and change to it, eg: +mkdir build+
followed by +cd build+. You cannot do an in-tree build.

3. If using Visual Studio make sure that you have installed the MFC
and ATL components for whatever version you are using.

4. Run CMake. Usually you will want to run +cmake ..+, but you might
want to change the generator with a special command, for example:

```
cmake .. -G "Unix Makefiles"
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:\sdk\x64 -G "NMake Makefiles"
cmake .. -DCMAKE_BUILD_TYPE=MinSizRel -G "Visual Studio 15 2017 Win64"
cmake .. -G "Visual Studio 16 2019" -A "x64"
```

5. Run your build tool. On Unix, you probably want to just run +make+.

As an optional final step, you can create a binary installation
package with CPack. For Windows, it will collect all the needed
runtime libraries and bundle it with the Eternity engine. Some
examples:

```
cpack -G ZIP
cpack -G DEB
cpack -G RPM
cpack -G STGZ
```

Building with CMake (macOS Xcode)
---------------------------------
You need Mac OS X 10.15 or more to run Eternity.

For macOS, currently only the Xcode generator is supported.

```
cmake .. -G Xcode
```

Or preferably use the CMake GUI app.

After that, open "Eternity Engine.xcodeproj". Inside it you'll have two targets of
interest:

1. eternity: the direct game executable. Suitable for debugging the main game.
2. EternityLauncher: the full app bundle. Suitable for release builds.

NOTE 1: CPack isn't currently supported on macOS for Eternity. Use the Xcode IDE to
produce a distributable app bundle.

NOTE 2: As of now, all SDL dependencies are downloaded during the CMake configuration
process. Eternity will _not_ link to whatever you may have installed to a central
location from Homebrew, other package managers or source-built.
