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
######################### Set Package Details  #################################
set(CMAKE_MFC_FLAG 2)
set(CMAKE_INSTALL_MFC_LIBRARIES 1)
include(InstallRequiredSystemLibraries)
set(CPACK_GENERATOR "DEB;RPM;STGZ;ZIP")
set(CPACK_PACKAGE_VENDOR "Team Eternity")
set(CPACK_PACKAGE_CONTACT "Team Eternity <haleyjd(at)hotmail.com>")
set(CPACK_PACKAGE_NAME "Eternity Engine")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "The Eternity Engine is Team Eternity's flagship product.")
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.11.0), libgcc1 (>= 1:4.4.3), libsdl2-2.0-0 (>=2.0.7), libsdl2-mixer-2.0-0 (>=2.0.2), libsdl2-net-2.0-0 (>=2.0.1), bash")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "freedoom, game-data-packager")
set(BUILD_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "i686")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
        set(CPACK_RPM_PACKAGE_ARCHITECTURE "i686" )
        set(BUILD_ARCH "${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
endif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "i686")
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
        set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64" )
        set(BUILD_ARCH "${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
endif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
if(WIN32)
        set(BUILD_ARCH "windows-x86")
        if($ENV{PROCESSOR_ARCHITECTURE} STREQUAL AMD64)
                set(BUILD_ARCH "windows-$ENV{PROCESSOR_ARCHITECTURE}")
        endif($ENV{PROCESSOR_ARCHITECTURE} STREQUAL AMD64)
endif(WIN32)
set(CPACK_PACKAGE_FILE_NAME "eternity-engine-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}_${BUILD_ARCH}")
set(CPACK_DEBIAN_PACKAGE_NAME "${CPACK_PACKAGE_FILE_NAME}")
set(CPACK_RPM_PACKAGE_NAME "${CPACK_PACKAGE_FILE_NAME}")

set(BIN_DIR bin)
set(SHARE_DIR share/eternity)
if(WIN32)
        set(LIB_DIR ${BIN_DIR})
        set(SHARE_DIR ${BIN_DIR})
else(WIN32)
        set(LIB_DIR lib)
endif(WIN32)
include(CPack)

