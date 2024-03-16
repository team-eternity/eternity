## Copyright (C) 2021 Max Waine
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

function(eternity_build_resources TARGET)
  foreach(GAME IN ITEMS "doom" "heretic")
    set(RESOURCE_FILE "${CMAKE_SOURCE_DIR}/base/${GAME}/eternity.pke")
    file(GLOB_RECURSE ETERNITY_GAME_RESOURCES LIST_DIRECTORIES false "${CMAKE_SOURCE_DIR}/base/${GAME}/res/*")
    if(UNIX AND NOT APPLE)
      set(INPUT ${ETERNITY_GAME_RESOURCES})
    else()
      set(INPUT "./")
    endif()

    add_custom_command(OUTPUT ${RESOURCE_FILE}
      COMMAND "${CMAKE_COMMAND}" -E tar "cf" ${RESOURCE_FILE} --format=zip ${INPUT}
      DEPENDS ${ETERNITY_GAME_RESOURCES}
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/base/${GAME}/res"
    )

    source_group("Resource Files\\base\\${GAME}" FILES ${RESOURCE_FILE})
    target_sources(${TARGET} PRIVATE ${RESOURCE_FILE})
  endforeach()
endfunction()

# EOF

