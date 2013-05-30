NOTES ON COMPILING ETERNITY ON (MAC) OS X
-----------------------------------------

EternityEngine.xcodeproj is the file that contains the OSX project that will build for 10.7 (Lion) or later. It's limited that way because of C++11 not being readily implemented on earlier versions.


Dependencies
------------

You need to have SDL, SDL_mixer, SDL_net and other static libraries in the staticlib/ subdirectory of macosx/. The required files are:

libFLAC.alibmikmod.alibogg.alibSDL_mixer.alibSDL_net.alibSDL.alibsmpeg.alibvorbis.alibvorbisfile.a

Easiest way to obtain them is to use a package manager like MacPorts, though it can also be done by downloading the SDL, SDL_mixer and SDL_net source codes and building them. As soon as you install them, head to the locations where 'dylib' and 'a'  files were pasted and copy the aforementioned 'a' files to staticlib/. Make sure they are universal i386+x86_64 binaries.

It's also assumed that /opt/local/include/SDL is the default header path. You can substitute another header path with SDL API, for it.


Mac OS X 10.6
-------------
There's a special process to build for Mac OS X 10.6 (and possibly earlier but not tested). What it essentially does is use custom-built libc++ and libc++abi whose install_name is the @loader_path/<their names>, so they can be used with portable programs. Get them from libcxx.llvm.org and libcxxabi.llvm.org.

To obtain portable libc++abi:
- edit its 'buildit' script and change install_path from /usr/lib/libc++.1.dylib to @loader_path/libc++.1.dylib

To obtain portable libc++:
- edit its 'buildit' script and change install_path from /usr/lib/libc++.1.dylib to @loader_path/libc++.1.dylib
- copy the libc++abi.dylib resulted from the previous paragraph into the working directory of this 'buildit'.
- edit from this 'buildit' the entry /usr/local/libc++abi.dylib and replace it with ./libc++abi.dylib

To compile for 10.6 or less:
- Apple's built-in Clang can't be used for this. If it sees that it has to look for C++11 libraries, it tells you cannot use OS X 10.6 for it. So download Clang binaries from OSX from clang.llvm.org
- Edit Makefile.osx106 and update the clang and clang++ paths to the downloaded Clang paths.

Ioan Chera