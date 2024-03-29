The Eternity Engine
===================

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
