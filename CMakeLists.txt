## Copyright (c) 2010 Jamie Jones <jamie_jones_au@yahoo.com.au>
## Copyright (c) 2011 Charles Gunyon
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
PROJECT("Eternity Engine" C CXX)
CMAKE_MINIMUM_REQUIRED(VERSION 2.4)
IF(COMMAND cmake_policy)
    CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND cmake_policy)
IF(${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
    MESSAGE(FATAL_ERROR "In-tree Builds are NOT supported.")
ENDIF(${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR})

# Supported Build Types are:
#    * Debug(CMAKE_C_FLAGS_DEBUG)
#    * Release(CMAKE_C_FLAGS_RELEASE)
#    * RelWithDebInfo(CMAKE_C_FLAGS_RELWITHDEBINFO)
#    * MinSizeRel(CMAKE_C_FLAGS_MINSIZEREL)
# If no build type requested, default to Debug
IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Debug)
ENDIF(NOT CMAKE_BUILD_TYPE)

INCLUDE(CheckIncludeFiles)
INCLUDE(CheckCCompilerFlag)
INCLUDE(CheckCXXCompilerFlag)
## [CG] pkg-config is now necessary.
INCLUDE(FindPkgConfig)
IF(NOT PKG_CONFIG_FOUND)
    MESSAGE(FATAL_ERROR "pkg-config was not found.")
ENDIF(NOT PKG_CONFIG_FOUND)
################################################################################
######################### Set Package Details  #################################
SET(CMAKE_MFC_FLAG 2)
SET(CMAKE_INSTALL_MFC_LIBRARIES 1)
INCLUDE(InstallRequiredSystemLibraries)
## SET(CPACK_GENERATOR "DEB;RPM;STGZ;ZIP;NSIS")
SET(CPACK_PACKAGE_VENDOR "Team Eternity")
SET(CPACK_PACKAGE_CONTACT "Team Eternity <haleyjd@hotmail.com>")
SET(CPACK_PACKAGE_VERSION_MAJOR "3")
SET(CPACK_PACKAGE_VERSION_MINOR "39")
SET(CPACK_PACKAGE_VERSION_PATCH "50")
SET(CPACK_PACKAGE_NAME "Eternity Engine")
SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY
     "The Eternity Engine is Team Eternity's flagship product.")
SET(CPACK_PACKAGE_EXECUTABLES "eternity;Eternity Engine")
SET(CPACK_PACKAGE_INSTALL_DIRECTORY "Eternity Engine")
SET(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "Eternity Engine")
SET(CPACK_STRIP_FILES TRUE)
SET(CPACK_DEBIAN_PACKAGE_SECTION "games")
SET(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libc6 (>= 2.11.0),
     libgcc1 (>= 1:4.4.3),
     libsdl1.2debian (>=1.2.14),
     libsdl-mixer1.2 (>=1.2.8),
     libsdl-net1.2 (>=1.2.7),
     libbz2-1.0 (>=1.0.4),
     zlib1g (>=1:1.2.3.4)
     libarchive1 (>=2.8.4),
     libc-ares2 (>=1.1.1-5.1),
     libcurl3-gnutls (>=7.21.0),
     libjson0 (>=0.9-1),
     libenet1a (>=1.3.0),
     bash"
)
SET(CPACK_DEBIAN_PACKAGE_RECOMMENDS "freedoom, game-data-packager")
SET(BUILD_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
IF(${CMAKE_SYSTEM_PROCESSOR} MATCHES "i686")
    SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
    SET(CPACK_RPM_PACKAGE_ARCHITECTURE "i686" )
    SET(BUILD_ARCH "${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
ENDIF(${CMAKE_SYSTEM_PROCESSOR} MATCHES "i686")
IF(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
    SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    SET(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64" )
    SET(BUILD_ARCH "${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
ENDIF(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
IF(WIN32)
    SET(BUILD_ARCH "windows-x86")
    IF("$ENV{PROCESSOR_ARCHITECTURE}" STREQUAL "AMD64")
        SET(BUILD_ARCH "windows-$ENV{PROCESSOR_ARCHITECTURE}")
    ENDIF("$ENV{PROCESSOR_ARCHITECTURE}" STREQUAL "AMD64")
ENDIF(WIN32)
SET(CPACK_PACKAGE_FILE_NAME "eternity-engine-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}_${BUILD_ARCH}")
SET(CPACK_DEBIAN_PACKAGE_NAME "${CPACK_PACKAGE_FILE_NAME}")
SET(CPACK_RPM_PACKAGE_NAME "${CPACK_PACKAGE_FILE_NAME}")

SET(BIN_DIR bin)
SET(SHARE_DIR share/eternity-engine)
IF(WIN32)
    SET(LIB_DIR ${BIN_DIR})
    SET(SHARE_DIR ${BIN_DIR})
ELSE(WIN32)
    SET(LIB_DIR lib)
ENDIF(WIN32)

SET(CPACK_SOURCE_DIR "${CMAKE_SOURCE_DIR}")
SET(CPACK_BINARY_DIR "${CMAKE_BINARY_DIR}")
SET(CPACK_PROJECT_CONFIG_FILE "${CMAKE_SOURCE_DIR}/EECPackConfig.txt")

INCLUDE(CPack)
################################################################################
######################### Compiler: Warnings  ##################################
IF(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    CHECK_C_COMPILER_FLAG(-Wall GCC_C_WALL)
    IF(GCC_C_WALL)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
    ENDIF(GCC_C_WALL)
    CHECK_C_COMPILER_FLAG(-Wextra GCC_C_WEXTRA)
    IF(GCC_C_WEXTRA)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
    ENDIF(GCC_C_WEXTRA)
ENDIF(CMAKE_C_COMPILER_ID STREQUAL "GNU")
IF(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    CHECK_CXX_COMPILER_FLAG(-Wall GCC_CXX_WALL)
    IF(GCC_CXX_WALL)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    ENDIF(GCC_CXX_WALL)
    CHECK_CXX_COMPILER_FLAG(-Wextra GCC_CXX_WEXTRA)
    IF(GCC_CXX_WEXTRA)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
    ENDIF(GCC_CXX_WEXTRA)
ENDIF(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")

IF(CMAKE_C_COMPILER_ID STREQUAL "Intel")
    CHECK_C_COMPILER_FLAG(-Wall INTEL_C_WALL)
    IF(INTEL_C_WALL)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
    ENDIF(INTEL_C_WALL)
    IF(INTEL_C_WCHECK)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wcheck")
    ENDIF(INTEL_C_WCHECK)
    CHECK_C_COMPILER_FLAG(-Wp64 INTEL_C_WP64)
    IF(INTEL_C_WP64)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wp64")
    ENDIF(INTEL_C_WP64)
    CHECK_C_COMPILER_FLAG(-Wshorten-64-to-32 INTEL_C_WSHORT64TO32)
    IF(INTEL_C_WSHORT64TO32)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wshorten-64-to-32")
    ENDIF(INTEL_C_WSHORT64TO32)
ENDIF(CMAKE_C_COMPILER_ID STREQUAL "Intel")
IF(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    CHECK_CXX_COMPILER_FLAG(-Wall INTEL_CXX_WALL)
    IF(INTEL_CXX_WALL)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    ENDIF(INTEL_CXX_WALL)
    IF(INTEL_CXX_WCHECK)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wcheck")
    ENDIF(INTEL_CXX_WCHECK)
    CHECK_CXX_COMPILER_FLAG(-Wp64 INTEL_CXX_WP64)
    IF(INTEL_CXX_WP64)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wp64")
    ENDIF(INTEL_CXX_WP64)
    CHECK_CXX_COMPILER_FLAG(-Wshorten-64-to-32 INTEL_CXX_WSHORT64TO32)
    IF(INTEL_CXX_WSHORT64TO32)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshorten-64-to-32")
    ENDIF(INTEL_CXX_WSHORT64TO32)
ENDIF(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")

IF(MSVC)
    ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)
    ADD_DEFINITIONS(-D_CRT_NONSTDC_NO_WARNINGS)
ENDIF(MSVC)
IF(MSVC)
    CHECK_C_COMPILER_FLAG(/W4 MSVC_C_W4)
    IF(MSVC_C_W4)
        ## FIXME: This is an inelegant hack to club MSVC into
        ## submission. By default CMake defines /W3 but if /W4
        ## is available, I want to use it. Unfortantly for MSVC
        ## later options while superseeding earlier options, also
        ## cause MSVC to emit a warning message each time, so
        ## if /W4 is supported, we will do a REGEX replacement of
        ## it as I want to see real warnings...
        ## Better solutions are welcome.
        IF(${CMAKE_C_FLAGS} MATCHES "/W3")
            STRING(REGEX REPLACE "/W3" "/W4"
                ${CMAKE_C_FLAGS}
                "${${CMAKE_C_FLAGS}}"
            )
        ENDIF(${CMAKE_C_FLAGS} MATCHES "/W3")
    ENDIF(MSVC_C_W4)
ENDIF(MSVC)
IF(MSVC)
    CHECK_CXX_COMPILER_FLAG(/W4 MSVC_CXX_W4)
    IF(MSVC_CXX_W4)
        ## FIXME: This is an inelegant hack to club MSVC into
        ## submission. By default CMake defines /W3 but if /W4
        ## is available, I want to use it. Unfortantly for MSVC
        ## later options while superseeding earlier options, also
        ## cause MSVC to emit a warning message each time, so
        ## if /W4 is supported, we will do a REGEX replacement of
        ## it as I want to see real warnings...
        ## Better solutions are welcome.
        IF(${CMAKE_CXX_FLAGS} MATCHES "/W3")
            STRING(REGEX REPLACE "/W3" "/W4"
                ${CMAKE_CXX_FLAGS}
                "${${CMAKE_CXX_FLAGS}}"
            )
        ENDIF(${CMAKE_CXX_FLAGS} MATCHES "/W3")
    ENDIF(MSVC_CXX_W4)
ENDIF(MSVC)
################################################################################
######################### Compiler: Hardening  #################################
IF(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "GNU")
    ADD_DEFINITIONS(-D_FORTIFY_SOURCE=2)
ENDIF(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "GNU")
IF(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    CHECK_C_COMPILER_FLAG(-fstack-protector GCC_C_FSTACKPROTECTOR)
    IF(GCC_C_FSTACKPROTECTOR)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector")
    ENDIF(GCC_C_FSTACKPROTECTOR)
    IF(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        CHECK_C_COMPILER_FLAG(-fPIC GCC_C_FPIC)
        IF(GCC_C_FPIC)
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
        ENDIF(GCC_C_FPIC)
        ## [CG] This will actually break linking with SDL_mixer on Debian
        ##      6.0, so disable it here.
        ## CHECK_C_COMPILER_FLAG(-pie GCC_C_PIE)
        ## IF(GCC_C_PIE)
        ##         SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pie")
        ## ENDIF(GCC_C_PIE)
        CHECK_C_COMPILER_FLAG(-Wl,-z,relro GCC_C_WLZRELRO)
        IF(GCC_C_WLZRELRO)
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,relro")
        ENDIF(GCC_C_WLZRELRO)
        CHECK_C_COMPILER_FLAG(-Wl,-z,now GCC_C_WLZNOW)
        IF(GCC_C_WLZNOW)
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,now")
        ENDIF(GCC_C_WLZNOW)
        CHECK_C_COMPILER_FLAG(-Wl,--as-needed GCC_C_WLASNEEDED)
        IF(GCC_C_WLASNEEDED)
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--as-needed")
        ENDIF(GCC_C_WLASNEEDED)
        CHECK_C_COMPILER_FLAG(-Wl,-z,noexecstack GCC_C_WLZNOEXECSTACK)
        IF(GCC_C_WLZNOEXECSTACK)
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,noexecstack")
        ENDIF(GCC_C_WLZNOEXECSTACK)
    ENDIF(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
ENDIF(CMAKE_C_COMPILER_ID STREQUAL "GNU")
IF(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    CHECK_CXX_COMPILER_FLAG(-fstack-protector GCC_CXX_FSTACKPROTECTOR)
    IF(GCC_CXX_FSTACKPROTECTOR)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector")
    ENDIF(GCC_CXX_FSTACKPROTECTOR)
    IF(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        CHECK_CXX_COMPILER_FLAG(-fPIC GCC_CXX_FPIC)
        IF(GCC_CXX_FPIC)
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
        ENDIF(GCC_CXX_FPIC)
        CHECK_CXX_COMPILER_FLAG(-pie GCC_CXX_PIE)
        IF(GCC_CXX_PIE)
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pie")
        ENDIF(GCC_CXX_PIE)
        CHECK_CXX_COMPILER_FLAG(-Wl,-z,relro GCC_CXX_WLZRELRO)
        IF(GCC_CXX_WLZRELRO)
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-z,relro")
        ENDIF(GCC_CXX_WLZRELRO)
        CHECK_CXX_COMPILER_FLAG(-Wl,-z,now GCC_CXX_WLZNOW)
        IF(GCC_CXX_WLZNOW)
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-z,now")
        ENDIF(GCC_CXX_WLZNOW)
        CHECK_CXX_COMPILER_FLAG(-Wl,--as-needed GCC_CXX_WLASNEEDED)
        IF(GCC_CXX_WLASNEEDED)
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--as-needed")
        ENDIF(GCC_CXX_WLASNEEDED)
        CHECK_CXX_COMPILER_FLAG(-Wl,-z,noexecstack GCC_CXX_WLZNOEXECSTACK)
        IF(GCC_CXX_WLZNOEXECSTACK)
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-z,noexecstack")
        ENDIF(GCC_CXX_WLZNOEXECSTACK)
    ENDIF(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
ENDIF(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")

IF(CMAKE_C_COMPILER_ID STREQUAL "Intel")
    CHECK_C_COMPILER_FLAG(-fstack-protector INTEL_C_FSTACKPROTECTOR)
    IF(INTEL_C_FSTACKPROTECTOR)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector")
    ENDIF(INTEL_C_FSTACKPROTECTOR)
    CHECK_C_COMPILER_FLAG(-fPIC INTEL_C_FPIC)
    IF(INTEL_C_FPIC)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    ENDIF(INTEL_C_FPIC)
    CHECK_C_COMPILER_FLAG(-pie INTEL_C_PIE)
    IF(INTEL_C_PIE)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pie")
    ENDIF(INTEL_C_PIE)
    CHECK_C_COMPILER_FLAG(-Wl,-z,relro INTEL_C_WLZRELRO)
    IF(INTEL_C_WLZRELRO)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,relro")
    ENDIF(INTEL_C_WLZRELRO)
    CHECK_C_COMPILER_FLAG(-Wl,-z,now INTEL_C_WLZNOW)
    IF(INTEL_C_WLZNOW)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,now")
    ENDIF(INTEL_C_WLZNOW)
    CHECK_C_COMPILER_FLAG(-Wl,--as-needed INTEL_C_WLASNEEDED)
    IF(INTEL_C_WLASNEEDED)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--as-needed")
    ENDIF(INTEL_C_WLASNEEDED)
    CHECK_C_COMPILER_FLAG(-Wl,-z,noexecstack INTEL_C_WLZNOEXECSTACK)
    IF(INTEL_C_WLZNOEXECSTACK)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,noexecstack")
    ENDIF(INTEL_C_WLZNOEXECSTACK)
ENDIF(CMAKE_C_COMPILER_ID STREQUAL "Intel")
IF(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    CHECK_CXX_COMPILER_FLAG(-fstack-protector INTEL_CXX_FSTACKPROTECTOR)
    IF(INTEL_CXX_FSTACKPROTECTOR)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector")
    ENDIF(INTEL_CXX_FSTACKPROTECTOR)
    CHECK_CXX_COMPILER_FLAG(-fPIC INTEL_CXX_FPIC)
    IF(INTEL_CXX_FPIC)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
    ENDIF(INTEL_CXX_FPIC)
    CHECK_CXX_COMPILER_FLAG(-pie INTEL_CXX_PIE)
    IF(INTEL_CXX_PIE)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pie")
    ENDIF(INTEL_CXX_PIE)
    CHECK_CXX_COMPILER_FLAG(-Wl,-z,relro INTEL_CXX_WLZRELRO)
    IF(INTEL_CXX_WLZRELRO)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-z,relro")
    ENDIF(INTEL_CXX_WLZRELRO)
    CHECK_CXX_COMPILER_FLAG(-Wl,-z,now INTEL_CXX_WLZNOW)
    IF(INTEL_CXX_WLZNOW)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-z,now")
    ENDIF(INTEL_CXX_WLZNOW)
    CHECK_CXX_COMPILER_FLAG(-Wl,--as-needed INTEL_CXX_WLASNEEDED)
    IF(INTEL_CXX_WLASNEEDED)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--as-needed")
    ENDIF(INTEL_CXX_WLASNEEDED)
    CHECK_CXX_COMPILER_FLAG(-Wl,-z,noexecstack INTEL_CXX_WLZNOEXECSTACK)
    IF(INTEL_CXX_WLZNOEXECSTACK)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-z,noexecstack")
    ENDIF(INTEL_CXX_WLZNOEXECSTACK)
ENDIF(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")

IF(MSVC)
    CHECK_C_COMPILER_FLAG(/GS MSVC_C_GS)
    IF(MSVC_C_GS)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /GS")
    ENDIF(MSVC_C_GS)
ENDIF(MSVC)
IF(MSVC)
    CHECK_CXX_COMPILER_FLAG(/GS MSVC_CXX_GS)
    IF(MSVC_CXX_GS)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GS")
    ENDIF(MSVC_CXX_GS)
ENDIF(MSVC)
################################################################################
######################### Compiler: Optimisation  ##############################
## NOTE: This is actually a counter-optimistion - it makes it slower, but as
## the Eternity Engine does break strict aliasing rules, correct code is
## preferable to fast code.
IF(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    CHECK_C_COMPILER_FLAG(-fno-strict-aliasing GCC_C_FNOSTRICTALIASING)
    IF(GCC_C_FNOSTRICTALIASING)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing")
    ENDIF(GCC_C_FNOSTRICTALIASING)
ENDIF(CMAKE_C_COMPILER_ID STREQUAL "GNU")
IF(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    CHECK_CXX_COMPILER_FLAG(-fno-strict-aliasing GCC_CXX_FNOSTRICTALIASING)
    IF(GCC_CXX_FNOSTRICTALIASING)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing")
    ENDIF(GCC_CXX_FNOSTRICTALIASING)
ENDIF(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")

IF(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel" OR CMAKE_BUILD_TYPE STREQUAL "Release")
    IF(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        CHECK_C_COMPILER_FLAG(-fomit-frame-pointer GCC_C_FOMITFRAMEPOINTER)
        IF(GCC_C_FOMITFRAMEPOINTER)
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fomit-frame-pointer")
        ENDIF(GCC_C_FOMITFRAMEPOINTER)
        CHECK_C_COMPILER_FLAG(-flto GCC_C_FLTO)
        IF(GCC_C_FLTO)
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto")
        ENDIF(GCC_C_FLTO)
    ENDIF(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    IF(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        CHECK_CXX_COMPILER_FLAG(-fomit-frame-pointer GCC_CXX_FOMITFRAMEPOINTER)
        IF(GCC_CXX_FOMITFRAMEPOINTER)
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fomit-frame-pointer")
        ENDIF(GCC_CXX_FOMITFRAMEPOINTER)
        CHECK_CXX_COMPILER_FLAG(-flto GCC_CXX_FLTO)
        IF(GCC_CXX_FLTO)
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
        ENDIF(GCC_CXX_FLTO)
    ENDIF(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")

    IF(CMAKE_C_COMPILER_ID STREQUAL "Intel")
        CHECK_C_COMPILER_FLAG(-fomit-frame-pointer INTEL_C_FOMITFRAMEPOINTER)
        IF(INTEL_C_FOMITFRAMEPOINTER)
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fomit-frame-pointer")
        ENDIF(INTEL_C_FOMITFRAMEPOINTER)
        CHECK_C_COMPILER_FLAG(-ipo INTEL_C_IPO)
        IF(INTEL_C_IPO)
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ipo")
        ENDIF(INTEL_C_IPO)
    ENDIF(CMAKE_C_COMPILER_ID STREQUAL "Intel")
    IF(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
        CHECK_CXX_COMPILER_FLAG(-fomit-frame-pointer INTEL_CXX_FOMITFRAMEPOINTER)
        IF(INTEL_CXX_FOMITFRAMEPOINTER)
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fomit-frame-pointer")
        ENDIF(INTEL_CXX_FOMITFRAMEPOINTER)
        CHECK_CXX_COMPILER_FLAG(-ipo INTEL_CXX_IPO)
        IF(INTEL_CXX_IPO)
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ipo")
        ENDIF(INTEL_CXX_IPO)
    ENDIF(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
ENDIF(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel" OR CMAKE_BUILD_TYPE STREQUAL "Release")

################################################################################
######################### Find Headers #########################################
CHECK_INCLUDE_FILES(stdint.h HAVE_STDINT_H)
IF(NOT HAVE_STDINT_H)
    IF(MSVC)
        ## FIXME: This is bad. stdint.h and inttypes.h should be
        ## moved to a seperate folder. They are not used by MinGW or
        ## in Visual Studio 2010 onwards, and break those builds.
        INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/source/win32/)
    ENDIF(MSVC)
ENDIF(NOT HAVE_STDINT_H)
################################################################################
######################### Find Needed Libs #####################################
IF(EE_STATIC_COMPILE)
    INCLUDE_DIRECTORIES(${EE_STATIC_ROOT_PREFIX}/include)
    INCLUDE_DIRECTORIES(${EE_STATIC_DEPS}/include)
ENDIF(EE_STATIC_COMPILE)

MESSAGE(STATUS "Looking for OpenGL")
FIND_LIBRARY(GL_LIBRARY GL)
IF(GL_LIBRARY)
    MESSAGE(STATUS "Looking for OpenGL - found")
ELSE(GL_LIBRARY)
    MESSAGE(STATUS "Looking for OpenGL - not found")
ENDIF(GL_LIBRARY)

FIND_PACKAGE(SDL 1.2.14 REQUIRED)
INCLUDE_DIRECTORIES(${SDL_INCLUDE_DIR})

FIND_PACKAGE(SDL_mixer 1.2.8 REQUIRED)
INCLUDE_DIRECTORIES(${SDLMIXER_INCLUDE_DIR})

FIND_PACKAGE(SDL_net 1.2.7 REQUIRED)
INCLUDE_DIRECTORIES(${SDLNET_INCLUDE_DIR})

## [CG] Further checks are unnecessary if building statically; it's assumed
##      that the user has set up the dependencies properly.
IF(NOT EE_STATIC_COMPILE)
PKG_SEARCH_MODULE(PC_LIBARCHIVE REQUIRED libarchive>=2.8.0)
INCLUDE_DIRECTORIES(${PC_LIBARCHIVE_INCLUDE_DIR})
CHECK_LIBRARY_EXISTS(
    ${PC_LIBARCHIVE_LIBRARIES}
    archive_write_set_compression_bzip2
    ""
    HAVE_LIBARCHIVE_BZIP2_SUPPORT
)

## IF(NOT HAVE_LIBARCHIVE_BZIP2_SUPPORT)
##     MESSAGE(FATAL_ERROR "No BZip2 support found in libarchive.")
## ENDIF(NOT HAVE_LIBARCHIVE_BZIP2_SUPPORT)

PKG_SEARCH_MODULE(PC_CURL REQUIRED libcurl>=7.21.0)
INCLUDE_DIRECTORIES(${PC_CURL_INCLUDE_DIR})

## [CG] Check that libcurl has non-blocking DNS support.
FIND_PROGRAM(
    CURL_CONFIG_EXECUTABLE NAMES curl-config DOC "curl-config executable"
)
IF(NOT CURL_CONFIG_EXECUTABLE)
    MESSAGE(FATAL_ERROR "curl-config not found.")
ENDIF(NOT CURL_CONFIG_EXECUTABLE)
EXECUTE_PROCESS(COMMAND ${CURL_CONFIG_EXECUTABLE} "--features"
    RESULT_VARIABLE CURL_CONFIG_FAILED
    OUTPUT_VARIABLE CURL_CONFIG_OUTPUT
    ERROR_VARIABLE CURL_CONFIG_ERROR
)
IF(CURL_CONFIG_FAILED)
    MESSAGE(FATAL_ERROR CURL_CONFIG_ERROR)
ENDIF(CURL_CONFIG_FAILED)
STRING(REGEX MATCH AsynchDNS CURL_ASYNC ${CURL_CONFIG_OUTPUT})
IF(NOT CURL_ASYNC)
    MESSAGE("WARNING: libcurl was not built with asynchronous DNS support.")
ENDIF(NOT CURL_ASYNC)

PKG_SEARCH_MODULE(PC_GNUTLS REQUIRED gnutls>=2.8.0)
INCLUDE_DIRECTORIES(${PC_GNUTLS_INCLUDE_DIR})

PKG_SEARCH_MODULE(PC_ENET REQUIRED libenet>=1.3.0)
INCLUDE_DIRECTORIES(${PC_ENET_INCLUDE_DIR})

PKG_SEARCH_MODULE(PC_JSON REQUIRED json>=0.9)
INCLUDE_DIRECTORIES(${PC_JSON_INCLUDE_DIR})

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/libpng)

## FIXME: This is an inelegant hack to find, and grab all needed
## .dll support files on windows. It works by looking for SDL.dll
## then taking every .dll file found in that directory from your SDK.
## This ensures you get sdl and all of it's needed .dll files, but may
## also grab unneeded .dll files.
IF(WIN32)
   FIND_PATH(SDL_SUPPORT_LIBS_DIR SDL.dll
       HINTS
       $ENV{SDLDIR}
       PATH_SUFFIXES lib64 lib
       PATHS)
   FILE(GLOB SDL_SUPPORT_LIBS ${SDL_SUPPORT_LIBS_DIR}/*.dll)
ENDIF(WIN32)
ENDIF(NOT EE_STATIC_COMPILE)
################################################################################
######################### Set Build Targets   ##################################

ADD_SUBDIRECTORY(snes_spc)
ADD_SUBDIRECTORY(source)
ADD_SUBDIRECTORY(libpng)

IF(NOT EE_STATIC_COMPILE)
IF(WIN32)
    ADD_SUBDIRECTORY(eecrashreport)
ENDIF(WIN32)

## FIXME: This is a continuation of the Windows SDL inelegant hack. This does
## the actual installation of those support libraries.
IF(WIN32)
    INSTALL(PROGRAMS ${SDL_SUPPORT_LIBS} DESTINATION ${LIB_DIR})
ENDIF(WIN32)
ENDIF(NOT EE_STATIC_COMPILE)

