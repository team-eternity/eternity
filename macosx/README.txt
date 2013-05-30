NOTES ON COMPILING ETERNITY ON (MAC) OS X
-----------------------------------------

Compiling Eternity using Xcode
------------------------------
This will produce a typical Mac OS X application, complete with user interface.

* This has been tested chiefly on Mac OS X 10.6 and OS X 10.8.

* There are two Xcode projects here: the Eternity engine (same as on all 
  platforms), and the front-end launcher wrapper (GUI-friendly). The latter is 
  less guaranteed to work or easily compile on older Mac OS X platforms.
  
* This project requires SDL, SDL_mixer and SDL_net, each 1.2, to exist on your
  system. It uses their Apple frameworks as libraries.
  
* The project is built in two stages: first open the EternityEngine xcodeproj
  and build it, then build EternityLaunch (from the 'launcher' subfolder). Note
  that EternityLaunch (the GUI launcher) uses the product of EternityEngine
  (called Eternity.app) as a resource. You need to specify the location of that
  file in Xcode
  
* The engine project attempts to run a utility called UPX on the executable.
  That program's job is to pack (compress) executables. On Macs, especially new
  ones, it's desirable not to have large file size, considering they're already
  "fat" binaries designed to accomodate both 32-bit and 64-bit architectures,
  and some new computers come with small SSDs by default. You can install UPX
  via MacPorts or similar. After installing UPX, please refer to the build 
  phases of EternityEngine, at the "Run shell script" entry, and verify that the 
  path to UPX matches the one to the installed UPX. By default it assumes it got
  installed through MacPorts, but it could also be /usr/local/bin/upx, 
  /usr/bin/upx and so on. Type 'which upx' on Terminal to obtain the actual 
  location.
  
Compiling Eternity using CMake
------------------------------
This will produce a traditional UNIX executable of Eternity, capable to be run
from Terminal or other shell (such as Applescript or Automator command of 
running shell script). It uses the same procedure as on other popular UNIX-like
systems.

* You may need the Xcode command-line tools, from Apple's website.

* Open terminal and go to the Eternity code folder (one level above the 'source'
  folder).
  
* You may need CMake. Install it either from their website or from a package ma-
  nager like MacPorts.

* You'll need the SDL dependencies as described in the first section.

* Say these commands (ignore anything after #):
  
$ mkdir build
$ cd build       # these will create a 'build' directory and lead you there
$ cmake ..       # this will run CMake and tell it to refer to the parent folder
                 # (the base code folder, with CMakeLists.txt)
$ make           # this will do the main work of compiling Eternity
$ cd source      # CMake generated some files and folders. You should now go in-
                 # to the new 'source' subfolder of 'build'. This folder 
                 # contains the new 'eternity' executable. Test it by providing
                 # -iwad, -base and -user parameters.
               