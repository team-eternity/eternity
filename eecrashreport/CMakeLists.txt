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
cmake_minimum_required(VERSION 3.0)
if(COMMAND cmake_policy)
   cmake_policy(SET CMP0003 NEW)
endif()

################################################################################
######################### Set Project Source  ##################################
add_definitions(-D_AFXDLL)
file(GLOB EECRASHREPORT_SOURCES *.cpp)
include_directories(.)

################################################################################
######################### Set Build Targets   ##################################
add_executable(eecrashreport WIN32 ${EECRASHREPORT_SOURCES})

set_target_properties(eecrashreport PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/eternity")

target_link_libraries(eecrashreport)
install(TARGETS eecrashreport
        RUNTIME DESTINATION ${BIN_DIR}
        LIBRARY DESTINATION ${LIB_DIR}
        ARCHIVE DESTINATION ${LIB_DIR})

# EOF

