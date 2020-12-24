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
cmake_minimum_required(VERSION 3.1)
if(COMMAND cmake_policy)
   cmake_policy(SET CMP0003 NEW)
endif()

################################################################################
######################### Everything Else ######################################
add_executable(midiproc WIN32)

set_target_properties(midiproc PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/eternity")

target_include_directories(midiproc PRIVATE . ${SDL2_INCLUDE_DIR} ${SDL2_MIXER_INCLUDE_DIR})
target_link_libraries(midiproc rpcrt4 ${SDL2_LIBRARY} ${SDL2_MIXER_LIBRARY})
target_sources(midiproc PRIVATE
               "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
               "${CMAKE_CURRENT_BINARY_DIR}/midiproc_s.c"
               "${CMAKE_CURRENT_BINARY_DIR}/midiproc.h")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
   add_custom_command(OUTPUT midiproc_c.c midiproc_s.c midiproc.h
   COMMAND midl -Oicf -W1 -Zp8 /env amd64 /protocol dce /app_config /out ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../midiproc/midiproc.idl
   MAIN_DEPENDENCY ../midiproc/midiproc.idl)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
   add_custom_command(OUTPUT midiproc_c.c midiproc_s.c midiproc.h
      COMMAND midl -Oicf -W1 -Zp8 /env win32 /protocol dce /app_config /out ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../midiproc/midiproc.idl
      MAIN_DEPENDENCY ../midiproc/midiproc.idl)
endif()

install(TARGETS midiproc
        RUNTIME DESTINATION ${BIN_DIR}
        LIBRARY DESTINATION ${LIB_DIR}
        ARCHIVE DESTINATION ${LIB_DIR})

# EOF

