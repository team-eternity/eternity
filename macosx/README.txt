NOTES ON COMPILING ETERNITY ON (MAC) OS X
-----------------------------------------

EternityEngine.xcodeproj is the file that contains the OSX project that will build for 10.7 (Lion) or later. It's limited that way because of C++11 not being readily implemented on earlier versions.


Dependencies
------------

You need to have SDL, SDL_mixer, SDL_net and other static libraries in the staticlib/ subdirectory of macosx/. The required files are:

libFLAC.alibmikmod.alibogg.alibSDL_mixer.alibSDL_net.alibSDL.alibsmpeg.alibvorbis.alibvorbisfile.a

Easiest way to obtain them is to use a package manager like MacPorts, though it can also be done by downloading the SDL, SDL_mixer and SDL_net source codes and building them. As soon as you install them, change directory to the locations where 'dylib' and 'a'  files were pasted and copy the aforementioned 'a' files to staticlib/. Make sure they are universal i386+x86_64 binaries.

It's also assumed that /opt/local/include/SDL is the default header path. You can substitute another header path with the SDL API, for it.


Mac OS X 10.6
-------------

Note: for now, this process still assumes you're on a Mac with OS X 10.7 or later installed (in practice only 10.8 was tested, not 10.7), but that the resulting application will run on OS X 10.6. Attempts to make this build directly from OS X 10.6 will be succeeded when the selected compiler is proven to work on that system.

There's a special process to build for Mac OS X 10.6. What it essentially does is use custom-built libc++ and libc++abi (Clang C++11 libraries) whose install_name has been set to "@loader_path/<their names>", so they can be used with portable programs. Download them from libcxx.llvm.org and libcxxabi.llvm.org. 

You'll also need to download the non-Apple Clang compiler from clang.llvm.org. The problem with the built-in Clang is that it has OSX deployment limitations: you cannot build for OSX prior to 10.7 any program that uses the C++11 standard library, even if it's separate from the one available from Lion. You can download binaries of Clang from the mentioned website, and place them at the directory of your choice.

The libc++ and libc++abi source packages come with builder shell scripts called "buildit". By default, executing them will result in dynamic (shared) libraries thereof, with install paths of /usr/lib/*, which means that they will have to be deployed to that system location in order to be used. Therefore the "buildit" scripts will need to be modified as described below. 

Eternity's source package has some sample "buildit" scripts shown in the libc++/libc++-builder (and libc++abi-builder) subfolders from this location. You can view them for reference, but please do not replace the "buildit" scripts from the libc++(abi) downloaded source packages.

Download the libcxx and libcxxabi from the llvm website, by following the instructions (at the time of writing, by using "svn"). Do the following in order:

1. To obtain portable libc++abi:
- edit its 'buildit' script (from lib/), look for "install_name", and change the associated value from /usr/lib/libc++.1.dylib to @loader_path/libc++.1.dylib

2. To obtain portable libc++:
- edit its 'buildit' script (from lib/), look for "install_name", and change the associated value from /usr/lib/libc++.1.dylib to @loader_path/libc++.1.dylib
- copy the libc++abi.dylib resulted from the previous paragraph (1.) into the working directory of this 'buildit'.
- search in this 'buildit' the entry "/usr/local/libc++abi.dylib" and replace it with "./libc++abi.dylib"

3. Copy the resulted portable libc++.dylib and libc++abi.dylib into the libc++/ subfolder of this location. Make sure you copy the original files, not the symbolic links.

4. Now you can use Xcode 4 to build the file for OS X 10.6 by building for archiving. It will take a longer time and may consume processor power, because it has to run the supplied Makefile.osx106 batch completely, for each architecture. It will result in an Eternity-106.app wrapper next to the default Eternity.app.


Eternity front-end notes
------------------------

Note that launcher/EternityLaunch.xcodeproj has two resources to copy: Eternity.app and Eternity-106.app. Make sure that only one is set as member for the current target!

Ioan Chera