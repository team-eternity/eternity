This project uses CMake to generate build scripts for Windows, *NIX and OSX.

Building the Eternity Engine with CMake

1) Extract the Eternity Engine source package to a working directory. In the root
of the working directory you should see a CMakeLists.txt file.

2) Create a new empty directory and change to it. You can NOT do an in-tree build.

3) Run CMake. The general command is the same across all supported platforms,
but the generator may be different. Some examples:

cmake ../eternity -DCMAKE_BUILD_TYPE=Release -G"Unix Makefiles"
cmake ..\eternity -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:\sdk\x64 -G"NMake Makefiles"
cmake ..\eternity -DCMAKE_BUILD_TYPE=RelWithDebInfo -G"MinGW Makefiles"
cmake ..\eternity -DCMAKE_BUILD_TYPE=Release -G"Visual Studio 6"
cmake ..\eternity -DCMAKE_BUILD_TYPE=MinSizRel -G"Visual Studio 9 2008 Win64"

If you are missing build dependencies CMake will halt the generation of build scripts until
this is corrected. There is no make clean, just delete the build directory and start again
from step 2.

4) Run your build tool.

Optional:

5) Create binary installation packages. After building with CMake and still in the working
directory run CPack to create the needed packages. For Windows packages this WILL collect
all needed runtime libraries and bundle it with the Eternity Engine. Some examples:

cpack -G ZIP
cpack -G DEB
cpack -G RPM
cpack -G STGZ
