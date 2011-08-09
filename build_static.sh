#!/bin/bash

###
# [CG] This script builds either a (mostly) static native Linux binary, or a
#      (mostly) static cross-compiled Windows binary using MinGW.  Both modes
#      require that EE_DEPS is set to the top-level EE dependencies
#      distribution folder (this folder contains the 'source' folder).  Static
#      compilation requires no further configuration.  Cross-compiling using
#      MinGW requires that EE_CROSS_COMPILE is set to "mingw" (it defaults to
#      "").  Finally, you may also need to set the locations of the various
#      MinGW compilation tools and the machine designation in the clearly
#      marked section below.
###

EE_DEPS="/home/charlie/eedeps"
EE_CROSS_COMPILE=""

################################################################################
# [CG] Edit these to suit your MinGW installation if necessary.                #
MINGW_C_COMPILER="/usr/bin/i586-mingw32msvc-gcc"
MINGW_CXX_COMPILER="/usr/bin/i586-mingw32msvc-g++"
MINGW_AR="/usr/bin/i586-mingw32msvc-ar"
MINGW_RANLIB="/usr/bin/i586-mingw32msvc-ranlib"
MINGW_LINKER="/usr/bin/i586-mingw32msvc-ld"
MINGW_RC_COMPILER="/usr/bin/i586-mingw32msvc-windres"
MINGW_BIN_PREFIX="/usr"
MINGW_ROOT_PREFIX="/usr/i586-mingw32msvc"
MINGW_SYSTEM="Windows"
MINGW_DEPS="$EE_DEPS/mingw"
#                                                                              #
################################################################################

MINGW_HAVE_SCHED_SET_AFFINITY="1"
MINGW_HAVE_STDINT_H="1"

LINUX_SYSTEM="Linux"
LINUX_BIN_PREFIX="/usr"
LINUX_ROOT_PREFIX="/usr"
LINUX_HAVE_SCHED_SET_AFFINITY="1"
LINUX_HAVE_STDINT_H="1"
LINUX_DEPS="$EE_DEPS/linux32"

STATIC_C_COMPILER=""
STATIC_CXX_COMPILER=""
STATIC_AR=""
STATIC_RANLIB=""
STATIC_LINKER=""
STATIC_RC_COMPILER=""

if [ "$EE_CROSS_COMPILE" == "mingw" ]
then
    STATIC_SYSTEM=$MINGW_SYSTEM
    STATIC_BIN_PREFIX=$MINGW_BIN_PREFIX
    STATIC_ROOT_PREFIX=$MINGW_ROOT_PREFIX
    STATIC_HAVE_STDINT_H=$MINGW_HAVE_STDINT_H
    STATIC_HAVE_SSA=$MINGW_HAVE_SCHED_SET_AFFINITY
    STATIC_DEPS=$MINGW_DEPS
    STATIC_C_COMPILER=$MINGW_C_COMPILER
    STATIC_CXX_COMPILER=$MINGW_CXX_COMPILER
    STATIC_AR=$MINGW_AR
    STATIC_RANLIB=$MINGW_RANLIB
    STATIC_LINKER=$MINGW_LINKER
    STATIC_RC_COMPILER=$MINGW_RC_COMPILER
else
    STATIC_SYSTEM=$LINUX_SYSTEM
    STATIC_BIN_PREFIX=$LINUX_BIN_PREFIX
    STATIC_ROOT_PREFIX=$LINUX_ROOT_PREFIX
    STATIC_HAVE_STDINT_H=$LINUX_HAVE_STDINT_H
    STATIC_HAVE_SSA=$LINUX_HAVE_SCHED_SET_AFFINITY
    STATIC_DEPS=$LINUX_DEPS
fi

if [ ! "$1" ]
then echo "Usage: $0 [ generator ] ... see 'cmake -h' for details"; exit 1
fi

if [ -d sbuild ]
then rm -rf sbuild/*
else mkdir sbuild
fi

( cd sbuild && SDLDIR=$STATIC_DEPS \
               SDLNETDIR=$STATIC_DEPS/include/SDL \
               SDLMIXERDIR=$STATIC_DEPS/include/SDL \
               LD_LIBRARY_PATH=$STATIC_DEPS/lib \
               PKG_CONFIG_PATH=$STATIC_DEPS/lib/pkgconfig \
               cmake -DEE_STATIC_COMPILE="1" \
                     -DEE_CROSS_COMPILE=$EE_CROSS_COMPILE \
                     -DEE_STATIC_SYSTEM=$STATIC_SYSTEM \
                     -DEE_STATIC_BIN_PREFIX=$STATIC_BIN_PREFIX \
                     -DEE_STATIC_ROOT_PREFIX=$STATIC_ROOT_PREFIX \
                     -DEE_STATIC_DEPS=$STATIC_DEPS \
                     -DEE_STATIC_C_COMPILER=$STATIC_C_COMPILER \
                     -DEE_STATIC_CXX_COMPILER=$STATIC_CXX_COMPILER \
                     -DEE_STATIC_AR=$STATIC_AR \
                     -DEE_STATIC_RANLIB=$STATIC_RANLIB \
                     -DEE_STATIC_LINKER=$STATIC_LINKER \
                     -DEE_STATIC_RC_COMPILER=$STATIC_RC_COMPILER \
                     -DHAVE_STDINT_H=$STATIC_HAVE_STDINT_H \
                     -DHAVE_SCHED_SET_AFFINITY=$STATIC_HAVE_SSA \
                     -DCMAKE_TOOLCHAIN_FILE=CMakeStatic.txt \
                     -G"$1" .. )

