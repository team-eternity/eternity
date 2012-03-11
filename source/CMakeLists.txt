## Copyright (c) 2010 Jamie Jones <jamie_jones_au@yahoo.com.au>
##
## This software is free software; you can redistribute it and/or
## modify it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This software is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor,
## Boston, MA  02110-1301  USA
##
################################################################################
######################### CMake Configuration ##################################
CMAKE_MINIMUM_REQUIRED(VERSION 2.4)
IF(COMMAND cmake_policy)
    CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND cmake_policy)

################################################################################
######################### Set Project Source  ##################################
ADD_DEFINITIONS(-DR_LINKEDPORTALS -D_SDL_VER -DZONE_NATIVE -D_CONSOLE)
ADD_DEFINITIONS(-DAMX_NODYNALOAD -DHAVE_SPCLIB)
ADD_DEFINITIONS(-DJSON_IS_AMALGAMATION -DEE_NO_SMALL_SUPPORT)
IF(GL_LIBRARY)
    ADD_DEFINITIONS(-DEE_FEATURE_OPENGL)
ENDIF(GL_LIBRARY)

IF(EE_STATIC_COMPILE)
    ADD_DEFINITIONS(-DCURL_STATICLIB -DLIBARCHIVE_STATIC)
ENDIF(EE_STATIC_COMPILE)

check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(stdint.h    HAVE_STDINT_H)
check_include_file(stddef.h    HAVE_STDDEF_H)

# [CG] For ZIP files; from zlib/CMakeLists.txt (thanks DavidPH)
SET(CMAKE_REQUIRED_DEFINITIONS -D_LARGEFILE64_SOURCE=1)
IF(HAVE_SYS_TYPES_H)
    LIST(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_SYS_TYPES_H)
ENDIF(HAVE_SYS_TYPES_H)
IF(HAVE_STDINT_H)
    LIST(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_STDINT_H)
ENDIF(HAVE_STDINT_H)
IF(HAVE_STDDEF_H)
    LIST(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_STDDEF_H)
ENDIF(HAVE_STDDEF_H)
CHECK_TYPE_SIZE(off64_t OFF64_T)
IF(HAVE_OFF64_T)
    ADD_DEFINITIONS(-D_LARGEFILE64_SOURCE=1)
ENDIF(HAVE_OFF64_T)
SET(CMAKE_REQUIRED_DEFINITIONS)
CHECK_FUNCTION_EXISTS(fseeko HAVE_FSEEKO)
IF(NOT HAVE_FSEEKO)
    ADD_DEFINITIONS(-DNO_FSEEKO)
ENDIF(NOT HAVE_FSEEKO)
CHECK_INCLUDE_FILE(unistd.h Z_HAVE_UNISTD_H)

## FIXME: Clarify with EE guys - does -DLINUX mean Linux, or does it really 
##        mean any *nix ?
IF(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    ADD_DEFINITIONS(-DLINUX)
ENDIF(CMAKE_SYSTEM_NAME STREQUAL "Linux")

IF(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR WIN32 OR MINGW OR APPLE)
    ADD_DEFINITIONS(-DHAVE_SCHED_SETAFFINITY)
ENDIF(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR WIN32 OR MINGW OR APPLE)

## [CG] Ensure that _GNU_SOURCE is defined when using GCC.
IF(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    ADD_DEFINITIONS(-D_GNU_SOURCE)
ENDIF(CMAKE_C_COMPILER_ID STREQUAL "GNU")

IF(WIN32 OR MINGW)
    ADD_DEFINITIONS(-DEE_CDROM_SUPPORT)
ENDIF(WIN32 OR MINGW)

IF(WIN32 OR MINGW)
    IF(NOT MINGW)
        INCLUDE_DIRECTORIES(Win32/)
    ENDIF(NOT MINGW)
    FILE(GLOB ARCH_SPECIFIC_SOURCES Win32/*.cpp)
ENDIF(WIN32 OR MINGW)

IF(APPLE)
    INCLUDE_DIRECTORIES(sdl/macosx/)
    FILE(GLOB ARCH_SPECIFIC_SOURCES sdl/macosx/*.m)
ENDIF(APPLE)

INCLUDE_DIRECTORIES(Confuse/)
FILE(GLOB CONFUSE_SOURCES Confuse/*.cpp)

INCLUDE_DIRECTORIES(textscreen/)
FILE(GLOB TEXTSCREEN_SOURCES textscreen/*.c)

INCLUDE_DIRECTORIES(./)
FILE(GLOB ETERNITY_SOURCES *.cpp)

INCLUDE_DIRECTORIES(hal/)
FILE(GLOB HAL_SOURCES hal/*.cpp)

INCLUDE_DIRECTORIES(gl/)
FILE(GLOB GL_SOURCES gl/*.cpp)

FILE(GLOB JSON_SOURCES ${CMAKE_SOURCE_DIR}/json-cpp/*.cpp)

## FIXME: It would be better to pull i_cpu_posix.cpp out into a
## *NIX specfic folder to turn this into a glob to catch new files.
INCLUDE_DIRECTORIES(sdl/)
SET(SDL_SOURCES
    sdl/i_font.cpp
    sdl/i_input.cpp
    sdl/i_main.cpp
    sdl/i_net.cpp
    sdl/i_pcsound.cpp
    sdl/i_picker.cpp
    sdl/i_sdlgl2d.cpp
    sdl/i_sdlmusic.cpp
    sdl/i_sdlsound.cpp
    sdl/i_sound.cpp
    sdl/i_system.cpp
    sdl/i_sdlvideo.cpp
    sdl/i_thread.cpp
    sdl/mmus2mid.cpp
    sdl/ser_main.cpp
)
IF(NOT WIN32)
    SET(SDL_SOURCES ${SDL_SOURCES} sdl/i_cpu_posix.cpp)
ENDIF(NOT WIN32)

SET(ZIP_SOURCES
   ${CMAKE_SOURCE_DIR}/minizip/ioapi.c
   ${CMAKE_SOURCE_DIR}/minizip/mztools.c
   ${CMAKE_SOURCE_DIR}/minizip/unzip.c
   ${CMAKE_SOURCE_DIR}/minizip/zip.c
)

################################################################################
######################### Set Build Targets   ##################################
## Eternity is a Windows CONSOLE application, so it's entry point is main,
## not WinMain. Adding the WIN32 qualifier sets the entry point to WinMain.
ADD_EXECUTABLE(eternity
    ${ARCH_SPECIFIC_SOURCES}
    ${ETERNITY_SOURCES}
    ${CONFUSE_SOURCES}
    ${TEXTSCREEN_SOURCES}
    ${HAL_SOURCES}
    ${GL_SOURCES}
    ${SDL_SOURCES}
    ${JSON_SOURCES}
    ${ZIP_SOURCES}
)
IF(EE_STATIC_COMPILE)
    IF(MINGW)
        FILE(GLOB EE_CROSS_DIRECTX_LIBRARIES
            ${EE_STATIC_DEPS}/source/DirectX/lib/*.a
        )
        TARGET_LINK_LIBRARIES(eternity
            ${EE_CROSS_DIRECTX_LIBRARIES}
            ${EE_STATIC_DEPS}/lib/libseh.a
            ${EE_STATIC_DEPS}/lib/libenet.a
            ${EE_STATIC_DEPS}/lib/libcurl.a
            ${EE_STATIC_DEPS}/lib/libgnutls-extra.a
            ${EE_STATIC_DEPS}/lib/libgnutls-openssl.a
            ${EE_STATIC_DEPS}/lib/libgnutls.a
            ${EE_STATIC_DEPS}/lib/libgcrypt.a
            ${EE_STATIC_DEPS}/lib/libgpg-error.a
            ${EE_STATIC_DEPS}/lib/libidn.a
            ${EE_STATIC_DEPS}/lib/libcares.a
            ${EE_STATIC_DEPS}/lib/libSDL_mixer.a
            ${EE_STATIC_DEPS}/lib/libSDL_ttf.a
            ${EE_STATIC_DEPS}/lib/libSDL_net.a
            ${EE_STATIC_DEPS}/lib/libSDL.a
            ${EE_STATIC_DEPS}/lib/libSDLmain.a
            ${EE_STATIC_DEPS}/lib/libmad.a
            # ${EE_STATIC_DEPS}/lib/libmikmod.a
            ${EE_STATIC_DEPS}/lib/libogg.a
            ${EE_STATIC_DEPS}/lib/libvorbis.a
            ${EE_STATIC_DEPS}/lib/libFLAC.a
            ${EE_STATIC_DEPS}/lib/libfreetype.a
            zlib
            png15_static
            snes_spc-static
            mingw32 m opengl32 glu32 user32 gdi32 dxguid ws2_32 wldap32 winmm
            stdc++
        )
    ELSE(MINGW)
        TARGET_LINK_LIBRARIES(eternity
            ${EE_STATIC_DEPS}/lib/libenet.a
            ${EE_STATIC_DEPS}/lib/libcurl.a
            ${EE_STATIC_DEPS}/lib/libgnutls-extra.a
            ${EE_STATIC_DEPS}/lib/libgnutls-openssl.a
            ${EE_STATIC_DEPS}/lib/libgnutls.a
            ${EE_STATIC_DEPS}/lib/libgcrypt.a
            ${EE_STATIC_DEPS}/lib/libgpg-error.a
            ${EE_STATIC_DEPS}/lib/libidn.a
            ${EE_STATIC_DEPS}/lib/libcares.a
            ${EE_STATIC_DEPS}/lib/libSDL_mixer.a
            ${EE_STATIC_DEPS}/lib/libSDL_ttf.a
            ${EE_STATIC_DEPS}/lib/libSDL_net.a
            ${EE_STATIC_DEPS}/lib/libSDL.a
            ${EE_STATIC_DEPS}/lib/libSDLmain.a
            ${EE_STATIC_DEPS}/lib/libmad.a
            # ${EE_STATIC_DEPS}/lib/libmikmod.a
            ${EE_STATIC_DEPS}/lib/libogg.a
            ${EE_STATIC_DEPS}/lib/libvorbis.a
            ${EE_STATIC_DEPS}/lib/libFLAC.a
            ${EE_STATIC_DEPS}/lib/libfreetype.a
            ${GL_LIBRARY}
            png15_static
            zlib
            snes_spc-static
            pthread m rt
            stdc++
        )
    ENDIF(MINGW)
ELSE(EE_STATIC_COMPILE)
    IF(GL_LIBRARY)
    TARGET_LINK_LIBRARIES(eternity
        ${SDL_LIBRARY}
        ${SDLMIXER_LIBRARY}
        ${SDLNET_LIBRARY}
        ${PC_GNUTLS_LDFLAGS}
        ${PC_ENET_LDFLAGS}
        ${PC_CURL_LDFLAGS}
        ${GL_LIBRARY}
        png15_static
        zlib
        snes_spc
        stdc++
    )
    ELSE(GL_LIBRARY)
    TARGET_LINK_LIBRARIES(eternity
        ${SDL_LIBRARY}
        ${SDLMIXER_LIBRARY}
        ${SDLNET_LIBRARY}
        ${PC_GNUTLS_LDFLAGS}
        ${PC_ENET_LDFLAGS}
        ${PC_CURL_LDFLAGS}
        png15_static
        zlib
        snes_spc
        stdc++
    )
    ENDIF(GL_LIBRARY)
ENDIF(EE_STATIC_COMPILE)

INSTALL(TARGETS eternity
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR}
)

IF(UNIX)
    IF(NOT APPLE OR NOT MINGW)
        SET_TARGET_PROPERTIES(eternity PROPERTIES OUTPUT_NAME eternity.real)
    ENDIF(NOT APPLE OR NOT MINGW)
ENDIF(UNIX)

## FIXME: base really needs to be refactored into read only and writeable
## data to conform to *NIX and Windows file system standards.
## On *NIX we use a wrapper script that set's up a per user
## $HOME/.eternity/base, symlinks in the IWADS, then sets up
## ETERNITYBASE to point to that directory, before executing
## eternity. It's a hack,but it will work for now. A solution for
## Windows would be welcome.
INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/base
    DESTINATION ${SHARE_DIR}
    PATTERN ".svn" EXCLUDE)
IF(UNIX)
    IF(NOT APPLE OR NOT MINGW)
        INSTALL(PROGRAMS ../eternity.sh DESTINATION ${BIN_DIR} RENAME eternity)
    ENDIF(NOT APPLE OR NOT MINGW)
ENDIF(UNIX)

