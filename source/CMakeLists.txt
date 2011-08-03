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
CMAKE_MINIMUM_REQUIRED (VERSION 2.4)
IF (COMMAND cmake_policy)
    CMAKE_POLICY (SET CMP0003 NEW)
ENDIF (COMMAND cmake_policy)

################################################################################
######################### Set Project Source  ##################################
ADD_DEFINITIONS (-DR_LINKEDPORTALS -D_SDL_VER -DZONE_NATIVE -D_CONSOLE)
ADD_DEFINITIONS (-DAMX_NODYNALOAD -DHAVE_SPCLIB)

## FIXME: Clarify with EE guys - does -DLINUX mean Linux, or does it really 
##        mean any *nix ?
IF (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    ADD_DEFINITIONS (-DLINUX)
ENDIF (CMAKE_SYSTEM_NAME STREQUAL "Linux")

IF (WIN32)
    ADD_DEFINITIONS (-DEE_CDROM_SUPPORT)
ENDIF (WIN32)

## Small does not work on 64bit systems. The most reliable way
## to determine if we are building on a 64bit system is to
## check the size of a pointer - it is 8 bytes under
## 64bit *NIX and 64bit Windows.
IF (CMAKE_SIZEOF_VOID_P MATCHES "8")
    ADD_DEFINITIONS (-DEE_NO_SMALL_SUPPORT)
ENDIF (CMAKE_SIZEOF_VOID_P MATCHES "8")

IF (WIN32)
    INCLUDE_DIRECTORIES (win32/)
    FILE (GLOB ARCH_SPECIFIC_SOURCES win32/*.c)
ENDIF (WIN32)

IF (APPLE)
    INCLUDE_DIRECTORIES (sdl/macosx/)
    FILE (GLOB ARCH_SPECIFIC_SOURCES sdl/macosx/*.m)
ENDIF (APPLE)

INCLUDE_DIRECTORIES (Confuse/)
FILE (GLOB CONFUSE_SOURCES Confuse/*.c)

INCLUDE_DIRECTORIES (textscreen/)
FILE (GLOB TEXTSCREEN_SOURCES textscreen/*.c)

INCLUDE_DIRECTORIES (./)
FILE (GLOB ETERNITY_SOURCES *.c)

## FIXME: It would be better to pull i_cpu_posix.c out into a
## *NIX specfic folder to turn this into a glob to catch new files.
INCLUDE_DIRECTORIES (sdl/)
SET (SDL_SOURCES
    sdl/i_input.c
    sdl/i_main.c
    sdl/i_net.c
    sdl/i_pcsound.c
    sdl/i_picker.c
    sdl/i_sdlmusic.c
    sdl/i_sdlsound.c
    sdl/i_sound.c
    sdl/i_system.c
    sdl/i_video.c
    sdl/mmus2mid.c
    sdl/ser_main.c
)
IF (NOT WIN32)
    SET ($SDL_SOURCES ${SDL_SOURCES} sdl/i_cpu_posix.c)
ENDIF (NOT WIN32)

################################################################################
######################### Set Build Targets   ##################################
## Eternity is a Windows CONSOLE application, so it's entry point is main,
## not WinMain. Adding the WIN32 qualifier sets the entry point to WinMain.
ADD_EXECUTABLE (eternity
    ${ARCH_SPECIFIC_SOURCES}
    ${ETERNITY_SOURCES}
    ${CONFUSE_SOURCES}
    ${TEXTSCREEN_SOURCES}
    ${SDL_SOURCES}
)
TARGET_LINK_LIBRARIES (eternity
    ${SDL_LIBRARY}
    ${SDLMIXER_LIBRARY}
    ${SDLNET_LIBRARY}
    ${LIBARCHIVE_LIBRARIES}
    ${PC_CARES_LDFLAGS}
    ${PC_ENET_LDFLAGS}
    ${PC_JSON_LDFLAGS}
    ${CURL_LIBRARIES}
    snes_spc
)
INSTALL (TARGETS eternity
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR}
)
IF (UNIX)
    IF (NOT APPLE OR NOT MINGW)
        SET_TARGET_PROPERTIES (eternity PROPERTIES OUTPUT_NAME eternity.real)
    ENDIF (NOT APPLE OR NOT MINGW)
ENDIF (UNIX)
## FIXME: base really needs to be refactored into read only and writeable
## data to conform to *NIX and Windows file system standards.
## On *NIX we use a wrapper script that set's up a per user
## $HOME/.eternity/base, symlinks in the IWADS, then sets up
## ETERNITYBASE to point to that directory, before executing
## eternity. It's a hack,but it will work for now. A solution for
## Windows would be welcome.
INSTALL (DIRECTORY ${CMAKE_SOURCE_DIR}/base
    DESTINATION ${SHARE_DIR}
    PATTERN ".svn" EXCLUDE)
IF (UNIX)
    IF (NOT APPLE OR NOT MINGW)
        INSTALL (PROGRAMS ../eternity.sh DESTINATION ${BIN_DIR} RENAME eternity)
    ENDIF (NOT APPLE OR NOT MINGW)
ENDIF (UNIX)

