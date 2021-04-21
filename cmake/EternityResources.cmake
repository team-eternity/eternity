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
    file(GLOB_RECURSE ETERNITY_GAME_RESOURCES "${CMAKE_SOURCE_DIR}/base/${GAME}/res/*")

    add_custom_command(OUTPUT "${CMAKE_SOURCE_DIR}/base/${GAME}/eternity.pke"
      COMMAND "${CMAKE_COMMAND}" -E tar "cvf" "${CMAKE_SOURCE_DIR}/base/${GAME}/eternity.pke" --format=zip "./"
      DEPENDS ${ETERNITY_GAME_RESOURCES}
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/base/${GAME}/res"
    )

    source_group("Resource Files\\base\\${GAME}" FILES "${CMAKE_SOURCE_DIR}/base/${GAME}/eternity.pke")

    target_sources(${TARGET} PRIVATE
      "${CMAKE_SOURCE_DIR}/base/${GAME}/eternity.pke"
    )
  endforeach()
endfunction()

# EOF

