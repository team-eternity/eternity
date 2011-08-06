#!/bin/sh

EEDEPS=/home/charlie/Documents/Code/eedeps
MINGW_PATH=/opt/mingw32
MINGW_MACHINE=i586-pc-mingw32

if [ -d build ]
then rm -rf build/*
else mkdir build
fi

cd build

SDLDIR=$EEDEPS/build \
SDLNETDIR=$EEDEPS/build/include/SDL \
SDLMIXERDIR=$EEDEPS/build/include/SDL \
LD_LIBRARY_PATH=$EEDEPS/build/lib \
CPPFLAGS="-I$EEDEPS/include" \
PKG_CONFIG_PATH=$EEDEPS/build/lib/pkgconfig \
cmake -DEE_CROSS_DEPS=$EEDEPS \
      -DEE_MINGW_PATH=$MINGW_PATH \
      -DEE_MINGW_MACHINE=$MINGW_MACHINE \
      -DCMAKE_TOOLCHAIN_FILE=CMakeMinGW.txt \
      -G"Unix Makefiles" ..

cd ..

