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
######################### Set Project Source  ##################################

# General flags.

add_definitions(-DR_LINKEDPORTALS -D_SDL_VER -D_CONSOLE -DHAVE_SPCLIB -DHAVE_ADLMIDILIB -DKATE_MEMORIAL)

# Library search flags.

if(OPENGL_LIBRARY)
   add_definitions(-DEE_FEATURE_OPENGL)
endif()

# Build specific flags.

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
   add_definitions(-D_DEBUG -DRANGECHECK -DINSTRUMENTED -DZONEIDCHECK)
endif()

# System specific flags.

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
   add_definitions(-DLINUX)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
   add_definitions(-DEE_FEATURE_REGISTRYSCAN)

   if(MSVC)
      add_definitions(-DEE_FEATURE_MIDIRPC -DEE_FEATURE_XINPUT)
   endif()
endif()

# Source files.

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
   file(GLOB ARCH_SPECIFIC_SOURCES Win32/*.cpp)
   if(MSVC)
      if(CMAKE_SIZEOF_VOID_P EQUAL 8)
         add_custom_command(OUTPUT midiproc_c.c midiproc_s.c midiproc.h
         COMMAND midl -Oicf -W1 -Zp8 /env amd64 /protocol dce /app_config /out ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../midiproc/midiproc.idl
         MAIN_DEPENDENCY ../midiproc/midiproc.idl)
	  elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
	     add_custom_command(OUTPUT midiproc_c.c midiproc_s.c midiproc.h
         COMMAND midl -Oicf -W1 -Zp8 /env win32 /protocol dce /app_config /out ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../midiproc/midiproc.idl
         MAIN_DEPENDENCY ../midiproc/midiproc.idl)
	  endif()
      include_directories(${CMAKE_CURRENT_BINARY_DIR})

      set(ARCH_SPECIFIC_SOURCES ${ARCH_SPECIFIC_SOURCES} midiproc_c.c)
	  
      include_directories(Win32/)
   endif()
endif()

IF (APPLE)
        INCLUDE_DIRECTORIES (sdl/macosx/)
        FILE (GLOB ARCH_SPECIFIC_SOURCES sdl/macosx/*.m)
ENDIF (APPLE)

IF (LINUX)
        INCLUDE_DIRECTORIES (linux/)
        FILE (GLOB ARCH_SPECIFIC_SOURCES linux/*.cpp)
ENDIF (LINUX)

INCLUDE_DIRECTORIES (Confuse/)
FILE (GLOB CONFUSE_SOURCES Confuse/*.cpp)

INCLUDE_DIRECTORIES (textscreen/)
FILE (GLOB TEXTSCREEN_SOURCES textscreen/*.c)

INCLUDE_DIRECTORIES (./)
FILE (GLOB ETERNITY_SOURCES *.cpp)

INCLUDE_DIRECTORIES (hal/)
FILE (GLOB HAL_SOURCES hal/*.cpp)

INCLUDE_DIRECTORIES (gl/)
FILE (GLOB GL_SOURCES gl/*.cpp)

INCLUDE_DIRECTORIES (sdl/)
FILE (GLOB SDL_SOURCES sdl/*.cpp)

################################################################################
######################### Set Build Targets   ##################################
## Eternity is a Windows CONSOLE application, so it's entry point is main,
## not WinMain. Adding the WIN32 qualifier sets the entry point to WinMain.
ADD_EXECUTABLE (eternity ${ARCH_SPECIFIC_SOURCES} ${ETERNITY_SOURCES} ${CONFUSE_SOURCES}
                ${TEXTSCREEN_SOURCES} ${HAL_SOURCES} ${GL_SOURCES} ${SDL_SOURCES})
                
set_target_properties(eternity
   PROPERTIES
   CXX_STANDARD          17
   CXX_STANDARD_REQUIRED ON
)

target_link_libraries(eternity ${SDL2_LIBRARY} ${SDL2_MIXER_LIBRARY} ${SDL2_NET_LIBRARY} acsvm png_static snes_spc ADLMIDI_static)

if(OPENGL_LIBRARY)
   target_link_libraries(eternity ${OPENGL_LIBRARY})
endif()

if(MSVC)
   target_link_libraries(eternity rpcrt4)
   
   target_compile_options(eternity PUBLIC "/utf-8")

   if(NOT MSVC_VERSION LESS  "1914")
      target_compile_options(eternity PUBLIC "/std:c++17")
   endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_link_libraries(eternity version)
endif()

INSTALL (TARGETS eternity
        RUNTIME DESTINATION ${BIN_DIR}
        LIBRARY DESTINATION ${LIB_DIR}
        ARCHIVE DESTINATION ${LIB_DIR})

INSTALL (DIRECTORY ${CMAKE_SOURCE_DIR}/base
        DESTINATION ${SHARE_DIR}
        PATTERN "delete.me" EXCLUDE
        PATTERN ".svn" EXCLUDE)

## EOF

