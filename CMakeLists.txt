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
PROJECT ("Eternity Engine" C CXX)
CMAKE_MINIMUM_REQUIRED (VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

IF (COMMAND cmake_policy)
        CMAKE_POLICY (SET CMP0003 NEW)
ENDIF (COMMAND cmake_policy)
IF (${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
        MESSAGE( FATAL_ERROR "In-tree Builds are NOT supported.")
ENDIF (${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR})

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

INCLUDE(CheckIncludeFiles)
INCLUDE(CheckCCompilerFlag)
INCLUDE(CheckCXXCompilerFlag)


################################################################################
######################### Functions  ###########################################

##
## TRY_C_COMPILER_FLAG
##
function(TRY_C_COMPILER_FLAG flag name)
   CHECK_C_COMPILER_FLAG("${flag}" ${name})

   if(${name})
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}" PARENT_SCOPE)
   endif()
endfunction()

##
## TRY_CXX_COMPILER_FLAG
##
function(TRY_CXX_COMPILER_FLAG flag name)
   CHECK_CXX_COMPILER_FLAG(${flag} ${name})

   if(${name})
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}" PARENT_SCOPE)
   endif()
endfunction()


################################################################################
######################### Set Package Details  #################################
SET (CMAKE_MFC_FLAG 2)
SET (CMAKE_INSTALL_MFC_LIBRARIES 1)
INCLUDE(InstallRequiredSystemLibraries)
SET (CPACK_GENERATOR "DEB;RPM;STGZ;ZIP")
SET (CPACK_PACKAGE_VENDOR "Team Eternity")
SET (CPACK_PACKAGE_CONTACT "Team Eternity <haelyjd@hotmail.com>")
SET (CPACK_PACKAGE_VERSION_MAJOR "4")
SET (CPACK_PACKAGE_VERSION_MINOR "1")
SET (CPACK_PACKAGE_VERSION_PATCH "0")
SET (CPACK_PACKAGE_NAME "Eternity Engine")
SET (CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING")
SET (CPACK_PACKAGE_DESCRIPTION_SUMMARY "The Eternity Engine is Team Eternity's flagship product.")
SET (CPACK_DEBIAN_PACKAGE_SECTION "games")
SET (CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.11.0), libgcc1 (>= 1:4.4.3), libsdl2-2.0-0 (>=2.0.7), libsdl2-mixer-2.0-0 (>=2.0.2), libsdl2-net-2.0-0 (>=2.0.1), bash")
SET (CPACK_DEBIAN_PACKAGE_RECOMMENDS "freedoom, game-data-packager")
SET (BUILD_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
IF (${CMAKE_SYSTEM_PROCESSOR} MATCHES "i686")
        SET (CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
        SET (CPACK_RPM_PACKAGE_ARCHITECTURE "i686" )
        SET (BUILD_ARCH "${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
ENDIF (${CMAKE_SYSTEM_PROCESSOR} MATCHES "i686")
IF (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
        SET (CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
        SET (CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64" )
        SET (BUILD_ARCH "${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
ENDIF (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
IF (WIN32)
        SET (BUILD_ARCH "windows-x86")
        IF("$ENV{PROCESSOR_ARCHITECTURE}" STREQUAL "AMD64")
                SET (BUILD_ARCH "windows-$ENV{PROCESSOR_ARCHITECTURE}")
        ENDIF("$ENV{PROCESSOR_ARCHITECTURE}" STREQUAL "AMD64")
ENDIF (WIN32)
SET (CPACK_PACKAGE_FILE_NAME "eternity-engine-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}_${BUILD_ARCH}")
SET (CPACK_DEBIAN_PACKAGE_NAME "${CPACK_PACKAGE_FILE_NAME}")
SET (CPACK_RPM_PACKAGE_NAME "${CPACK_PACKAGE_FILE_NAME}")

SET (BIN_DIR bin)
SET (SHARE_DIR share/eternity)
IF (WIN32)
        SET (LIB_DIR ${BIN_DIR})
        SET (SHARE_DIR ${BIN_DIR})
ELSE (WIN32)
        SET (LIB_DIR lib)
ENDIF (WIN32)
INCLUDE(CPack)


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
######################### Compiler: Features  ##################################
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
   TRY_CXX_COMPILER_FLAG(-std=c++17 FLAG_CXX_stdcxx17)
endif()


################################################################################
######################### Compiler: Warnings  ##################################
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
   TRY_C_COMPILER_FLAG(-Wall   FLAG_C_Wall)
  #TRY_C_COMPILER_FLAG(-Wextra FLAG_C_Wextra)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
   TRY_CXX_COMPILER_FLAG(-Wall   FLAG_CXX_Wall)
  #TRY_CXX_COMPILER_FLAG(-Wextra FLAG_CXX_Wextra)
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "Intel")
   TRY_C_COMPILER_FLAG(-Wall              FLAG_C_Wall)
   TRY_C_COMPILER_FLAG(-Wcheck            FLAG_C_Wcheck)
   TRY_C_COMPILER_FLAG(-Wp64              FLAG_C_Wp64)
   TRY_C_COMPILER_FLAG(-Wshorten-64-to-32 FLAG_C_Wshorten_64_to_32)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
   TRY_CXX_COMPILER_FLAG(-Wall              FLAG_CXX_Wall)
   TRY_CXX_COMPILER_FLAG(-Wcheck            FLAG_CXX_Wcheck)
   TRY_CXX_COMPILER_FLAG(-Wp64              FLAG_CXX_Wp64)
   TRY_CXX_COMPILER_FLAG(-Wshorten-64-to-32 FLAG_CXX_Wshorten_64_to_32)
endif()

if(MSVC)
   add_definitions(-D_CRT_SECURE_NO_WARNINGS)
   add_definitions(-D_CRT_NONSTDC_NO_WARNINGS)
endif()


################################################################################
######################### Compiler: Hardening  #################################
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
   if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
      TRY_C_COMPILER_FLAG(-fstack-protector FLAG_C_fstack_protector)

      if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
         TRY_C_COMPILER_FLAG(-fPIC              FLAG_C_fPIC)
         TRY_C_COMPILER_FLAG(-pie               FLAG_C_pie)
         TRY_C_COMPILER_FLAG(-Wl,-z,relro       FLAG_C_Wl_z_relro)
         TRY_C_COMPILER_FLAG(-Wl,-z,now         FLAG_C_Wl_z_now)
         TRY_C_COMPILER_FLAG(-Wl,--as-needed    FLAG_C_Wl_as_needed)
         TRY_C_COMPILER_FLAG(-Wl,-z,noexecstack FLAG_C_Wl_z_noexecstack)
      endif()
   endif()

   if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      TRY_CXX_COMPILER_FLAG(-fstack-protector FLAG_CXX_fstack_protector)

      if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
         TRY_CXX_COMPILER_FLAG(-fPIC              FLAG_CXX_fPIC)
         TRY_CXX_COMPILER_FLAG(-pie               FLAG_CXX_pie)
         TRY_CXX_COMPILER_FLAG(-Wl,-z,relro       FLAG_CXX_Wl_z_relro)
         TRY_CXX_COMPILER_FLAG(-Wl,-z,now         FLAG_CXX_Wl_z_now)
         TRY_CXX_COMPILER_FLAG(-Wl,--as-needed    FLAG_CXX_Wl_as_needed)
         TRY_CXX_COMPILER_FLAG(-Wl,-z,noexecstack FLAG_CXX_Wl_z_noexecstack)
      endif()
   endif()

   if(CMAKE_C_COMPILER_ID STREQUAL "Intel")
      TRY_C_COMPILER_FLAG(-fstack-protector  FLAG_C_fstackprotector)
      TRY_C_COMPILER_FLAG(-fPIC              FLAG_C_fPIC)
      TRY_C_COMPILER_FLAG(-pie               FLAG_C_pie)
      TRY_C_COMPILER_FLAG(-Wl,-z,relro       FLAG_C_Wl_z_relro)
      TRY_C_COMPILER_FLAG(-Wl,-z,now         FLAG_C_Wl_z_now)
      TRY_C_COMPILER_FLAG(-Wl,--as-needed    FLAG_C_Wl_as_needed)
      TRY_C_COMPILER_FLAG(-Wl,-z,noexecstack FLAG_C_Wl_z_noexecstack)
   endif()

   if(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
      TRY_CXX_COMPILER_FLAG(-fstack-protector  FLAG_CXX_fstackprotector)
      TRY_CXX_COMPILER_FLAG(-fPIC              FLAG_CXX_fPIC)
      TRY_CXX_COMPILER_FLAG(-pie               FLAG_CXX_pie)
      TRY_CXX_COMPILER_FLAG(-Wl,-z,relro       FLAG_CXX_Wl_z_relro)
      TRY_CXX_COMPILER_FLAG(-Wl,-z,now         FLAG_CXX_Wl_z_now)
      TRY_CXX_COMPILER_FLAG(-Wl,--as-needed    FLAG_CXX_Wl_as_needed)
      TRY_CXX_COMPILER_FLAG(-Wl,-z,noexecstack FLAG_CXX_Wl_z_noexecstack)
   endif()

   if(MSVC)
      TRY_C_COMPILER_FLAG(/GS FLAG_C_GS)

      TRY_CXX_COMPILER_FLAG(/GS FLAG_CXX_GS)
   endif()
endif()


################################################################################
######################### Compiler: Optimisation  ##############################
if(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel" OR CMAKE_BUILD_TYPE STREQUAL "Release")
   if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      TRY_C_COMPILER_FLAG(-fomit-frame-pointer FLAG_C_fomit_frame_pointer)
   endif()

   if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      TRY_CXX_COMPILER_FLAG(-fomit-frame-pointer FLAG_CXX_fomit_frame_pointer)
   endif()

   if(CMAKE_C_COMPILER_ID STREQUAL "Intel")
      TRY_C_COMPILER_FLAG(-fomit-frame-pointer FLAG_C_fomit_frame_pointer)
      TRY_C_COMPILER_FLAG(-ipo                 FLAG_C_ipo)
   endif()

   if(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
      TRY_CXX_COMPILER_FLAG(-fomit-frame-pointer FLAG_CXX_fomit_frame_pointer)
      TRY_CXX_COMPILER_FLAG(-ipo                 FLAG_CXX_ipo)
   endif()
endif()


################################################################################
######################### Find Headers #########################################
CHECK_INCLUDE_FILES (stdint.h HAVE_STDINT_H)
IF (NOT HAVE_STDINT_H)
        IF (MSVC)
                ## FIXME: This is bad. stdint.h and inttypes.h should be
                ## moved to a seperate folder. They are not used by MinGW or
                ## in Visual Studio 2010 onwards, and break those builds.
                INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR}/source/win32/)
        ENDIF (MSVC)
ENDIF (NOT HAVE_STDINT_H)

################################################################################
######################### Find Needed Libs #####################################
FIND_PACKAGE (OpenGL)

FIND_PACKAGE (SDL2 REQUIRED)
INCLUDE_DIRECTORIES (${SDL2_INCLUDE_DIR})

FIND_PACKAGE (SDL2_mixer REQUIRED)
INCLUDE_DIRECTORIES (${SDL2_MIXER_INCLUDE_DIR})

FIND_PACKAGE (SDL2_net REQUIRED)
INCLUDE_DIRECTORIES (${SDL2_NET_INCLUDE_DIR})

## FIXME: This is an inelegant hack to find, and grab all needed
## .dll support files on windows. It works by looking for SDL2.dll
## then taking every .dll file found in that directory from your SDK.
## This ensures you get sdl and all of it's needed .dll files, but may
## also grab unneeded .dll files.
IF (WIN32)
       FIND_PATH (SDL2_SUPPORT_LIBS_DIR SDL2.dll
               HINTS
               $ENV{SDL2DIR}
               PATH_SUFFIXES lib64 lib
               PATHS)
       FILE (GLOB SDL2_SUPPORT_LIBS ${SDL2_SUPPORT_LIBS_DIR}/*.dll)
ENDIF (WIN32)
################################################################################
######################### Set Build Targets   ##################################

INCLUDE_DIRECTORIES(acsvm)
INCLUDE_DIRECTORIES(angelscript/add_on/autowrapper)
INCLUDE_DIRECTORIES(angelscript/angelscript/include)
INCLUDE_DIRECTORIES(libpng)
INCLUDE_DIRECTORIES(zlib)

SET(ACSVM_NOFLAGS ON)
SET(ACSVM_SHARED OFF)
ADD_SUBDIRECTORY(acsvm)

ADD_SUBDIRECTORY(angelscript/angelscript/projects/cmake)
ADD_SUBDIRECTORY(zlib)
ADD_SUBDIRECTORY(libpng)
ADD_SUBDIRECTORY(snes_spc)
ADD_SUBDIRECTORY(source)

IF (WIN32 AND MSVC)
        ADD_SUBDIRECTORY(eecrashreport)
ENDIF (WIN32 AND MSVC)

## FIXME: This is a continuation of the Windows SDL inelegant hack. This does
## the actual installation of those support libraries.
IF (WIN32)
        INSTALL (PROGRAMS ${SDL2_SUPPORT_LIBS}
                DESTINATION ${LIB_DIR})
ENDIF (WIN32)

