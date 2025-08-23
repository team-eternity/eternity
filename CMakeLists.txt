## Copyright (c) 2010 Jamie Jones <jamie_jones_au@yahoo.com.au>
## Copyright (C) 2013 David Hill
## Copyright (C) 2020 Max Waine
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
cmake_minimum_required(VERSION 3.16)

# Must be before project()
# Can't set anything below 10.15 now (10.13 would have worked, but we want std::filesystem)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment version")

project("Eternity Engine"
        VERSION 4.5.3
        LANGUAGES C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "eternity")

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

if(NOT CMAKE_EXPORT_COMPILE_COMMANDS)
  # Export compile commands unless we're generating a modern project.
   if(CMAKE_GENERATOR MATCHES "Make" OR CMAKE_GENERATOR MATCHES "Ninja")
      set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
   else()
      set(CMAKE_EXPORT_COMPILE_COMMANDS OFF)
   endif()
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS "${CMAKE_EXPORT_COMPILE_COMMANDS}" CACHE BOOL
    "Export compile commands for use in supported editors."
    FORCE)

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

# Generate README.txt via simple copy.
configure_file(
   ${CMAKE_SOURCE_DIR}/README.md
   ${CMAKE_BINARY_DIR}/README.txt
   COPYONLY)

################################################################################
############ Compiler: Features, Warnings, Hardening, Optimisation  ############
include(Compiler)

################################################################################
######################### Find Needed Libs #####################################
include(ExternalLibraries)

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
add_subdirectory(snes_spc)

# WARNING: this section must be before add_subdirectory(libpng)
if(WIN32)
   set(PNG_BUILD_ZLIB ON CACHE BOOL "Custom zlib location" FORCE)

   set(ZLIB_LIBRARY zlibstatic CACHE STRING "zlib library name" FORCE)
   set(ZLIB_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/zlib CACHE STRING "zlib include directory" FORCE)
   find_package(ZLIB REQUIRED)

   set(PNG_SHARED OFF CACHE BOOL "Build shared pnglib" FORCE)
   set(PNG_EXECUTABLES OFF CACHE BOOL "Build libpng executables" FORCE)
   set(PNG_TESTS OFF CACHE BOOL "Build libpng tests" FORCE)
endif()
add_subdirectory(libpng)

# WARNING: add_subdirectory(source) must be below libpng, otherwise the png_static isn't found on Mac.
add_subdirectory(source)


if(WIN32 AND MSVC)
   find_package(MFC)
   if(MFC_FOUND)
      add_subdirectory(eecrashreport)

      add_dependencies(eternity eecrashreport)
   else()
      message("Warning: MFC/ATL not found. eecrashreport will not be built.")
   endif()
endif()
if(APPLE)
   add_subdirectory(macosx/launcher)
endif()
# EOF

