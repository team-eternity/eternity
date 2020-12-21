## Copyright (c) 2010 Jamie Jones <jamie_jones_au@yahoo.com.au>
## Copyright (C) 2013 David Hill
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
cmake_minimum_required(VERSION 3.0)
project("Eternity Engine"
        VERSION 4.2.0
        LANGUAGES C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

if(COMMAND cmake_policy)
   cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)
if(${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
   message(FATAL_ERROR "In-tree Builds are NOT supported.")
endif(${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR})

if(NOT EXISTS ${CMAKE_SOURCE_DIR}/adlmidi/CMakeLists.txt)
   message(FATAL_ERROR "adlmidi not found. Please install adlmidi submodule.")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Supported Build Types are:
#    * Debug (CMAKE_C_FLAGS_DEBUG)
#    * Release (CMAKE_C_FLAGS_RELEASE)
#    * RelWithDebInfo (CMAKE_C_FLAGS_RELWITHDEBINFO)
#    * MinSizeRel (CMAKE_C_FLAGS_MINSIZEREL)
# If no build type requested, default to Release
if(NOT DEFINED CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Release)
endif()

# Default to not building shared libraries.
if(NOT DEFINED BUILD_SHARED_LIBS)
   set(BUILD_SHARED_LIBS OFF)
endif()

# Default to not installing any sub-projects.
if(NOT DEFINED SKIP_INSTALL_ALL)
   set(SKIP_INSTALL_ALL ON)
endif()

include(CheckIncludeFiles)


################################################################################
######################### Set Package Details  #################################
include(Packaging)


################################################################################
######################### Documentation  #######################################

# Generate RADME.txt via simple copy.
configure_file(
   ${CMAKE_SOURCE_DIR}/README.adoc
   ${CMAKE_BINARY_DIR}/README.txt
   COPYONLY)

# If present on system, use asciidoc to generate README.html.
find_program(ASCIIDOC_EXE asciidoc)
if(ASCIIDOC_EXE)
   message(STATUS "Found asciidoc: ${ASCIIDOC_EXE}")
   add_custom_target(readme-asciidoc ALL
      COMMAND "${ASCIIDOC_EXE}"
         --backend=html
         --out-file=${CMAKE_BINARY_DIR}/README.html
         ${CMAKE_SOURCE_DIR}/README.adoc
      SOURCES ${CMAKE_SOURCE_DIR}/README.adoc
      )
endif()


################################################################################
############ Compiler: Features, Warnings, Hardening, Optimisation  ############
include(Compiler)


################################################################################
######################### Find Headers #########################################
check_include_files(stdint.h HAVE_STDINT_H)
if(NOT HAVE_STDINT_H)
        if(MSVC)
                ## FIXME: This is bad. stdint.h and inttypes.h should be
                ## moved to a seperate folder. They are not used by MinGW or
                ## in Visual Studio 2010 onwards, and break those builds.
                include_directories(${CMAKE_SOURCE_DIR}/source/win32/)
        endif(MSVC)
endif(NOT HAVE_STDINT_H)

check_include_files(xlocale.h HAVE_XLOCALE_H)
if(HAVE_XLOCALE_H)
        add_definitions(-DHAVE_XLOCALE_H)
endif()

################################################################################
######################### Find Needed Libs #####################################
find_package(OpenGL)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

find_package(SDL2_mixer REQUIRED)
include_directories(${SDL2_MIXER_INCLUDE_DIR})

find_package(SDL2_net REQUIRED)
include_directories(${SDL2_NET_INCLUDE_DIR})

## FIXME: This is an inelegant hack to find, and grab all needed
## .dll support files on windows. It works by looking for SDL2.dll
## then taking every .dll file found in that directory from your SDK.
## This ensures you get sdl and all of it's needed .dll files, but may
## also grab unneeded .dll files.
if(WIN32)
        find_path(SDL2_SUPPORT_LIBS_DIR SDL2.dll
                  HINTS
                  $ENV{SDL2DIR}
                  PATH_SUFFIXES lib64 lib
                  PATHS)
       file(GLOB SDL2_SUPPORT_LIBS ${SDL2_SUPPORT_LIBS_DIR}/*.dll)
endif(WIN32)
################################################################################
######################### Set Build Targets   ##################################

include_directories(acsvm)
include_directories(libpng)
include_directories(zlib)
include_directories(adlmidi/include)

set(ACSVM_NOFLAGS ON)
set(ACSVM_SHARED OFF)
add_subdirectory(acsvm)

set(WITH_CPP_EXTRAS ON)
add_subdirectory(adlmidi)

add_subdirectory(zlib)
add_subdirectory(libpng)
add_subdirectory(snes_spc)
add_subdirectory(source)

if(WIN32 AND MSVC)
        add_subdirectory(eecrashreport)
endif(WIN32 AND MSVC)

## FIXME: This is a continuation of the Windows SDL inelegant hack. This does
## the actual installation of those support libraries.
if(WIN32)
        install(PROGRAMS ${SDL2_SUPPORT_LIBS}
                DESTINATION ${LIB_DIR})
endif(WIN32)

