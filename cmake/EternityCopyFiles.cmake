## Copyright (C) 2020 Alex Mayfield
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

include(EternityResources)

function(eternity_copy_libs TARGET)
  set(ETERNITY_DLLS "")

  if(WIN32)
    if(MSVC)
      set(SDL3_DLL_DIR "$<TARGET_FILE_DIR:SDL3::SDL3>")
      set(SDL3_MIXER_DLL_DIR "$<TARGET_FILE_DIR:SDL3::mixer>")
      set(SDL3_NET_DLL_DIR "$<TARGET_FILE_DIR:SDL3::net>")
    else()
      set(SDL3_DLL_DIR "$<TARGET_FILE_DIR:SDL3::SDL3>/../bin")
      set(SDL3_MIXER_DLL_DIR "$<TARGET_FILE_DIR:SDL3::mixer>/../bin")
      set(SDL3_NET_DLL_DIR "$<TARGET_FILE_DIR:SDL3::net>/../bin")
    endif()

    # SDL3
    list(APPEND ETERNITY_DLLS "${SDL3_DLL_DIR}/SDL3.dll")

    # SDL3_mixer
    if(MSVC)
        list(APPEND ETERNITY_DLLS "${SDL3_MIXER_DLL_DIR}/optional/libgme.dll")
        list(APPEND ETERNITY_DLLS "${SDL3_MIXER_DLL_DIR}/optional/libogg-0.dll")
        list(APPEND ETERNITY_DLLS "${SDL3_MIXER_DLL_DIR}/optional/libopus-0.dll")
        list(APPEND ETERNITY_DLLS "${SDL3_MIXER_DLL_DIR}/optional/libopusfile-0.dll")
		list(APPEND ETERNITY_DLLS "${SDL3_MIXER_DLL_DIR}/optional/libwavpack-1.dll")
		list(APPEND ETERNITY_DLLS "${SDL3_MIXER_DLL_DIR}/optional/libxmp.dll")
    endif()
    list(APPEND ETERNITY_DLLS "${SDL3_MIXER_DLL_DIR}/SDL3_mixer.dll")

    # SDL3_net
    list(APPEND ETERNITY_DLLS "${SDL3_NET_DLL_DIR}/SDL3_net.dll")
  endif()

  if(APPLE)
    list(APPEND ETERNITY_FWS SDL3.framework)
    list(APPEND ETERNITY_FWS SDL3_mixer.framework)
    list(APPEND ETERNITY_FWS SDL3_net.framework)

    foreach(ETERNITY_FW ${ETERNITY_FWS})
    add_custom_command(
      TARGET ${TARGET} POST_BUILD
      COMMAND ${CMAKE_SOURCE_DIR}/macosx/copy_frameworks_to_product.sh ${CMAKE_BINARY_DIR}/${ETERNITY_FW} $<TARGET_FILE_DIR:${TARGET}>
      VERBATIM
    )
    endforeach()
  endif()

  # Copy library files to target directory.
  foreach(ETERNITY_DLL ${ETERNITY_DLLS})
    add_custom_command(TARGET ${TARGET} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${ETERNITY_DLL}" "$<TARGET_FILE_DIR:${TARGET}>"
      VERBATIM)
  endforeach()
endfunction()

function(eternity_copy_base_and_user TARGET)
  eternity_build_resources(${TARGET})

  # TODO: Use rm instead of remove_directory in a few years when we can assume people will have CMake 3.17
  #       across the board.
  add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/base" "$<TARGET_FILE_DIR:${TARGET}>/base"
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "$<TARGET_FILE_DIR:${TARGET}>/base/doom/res"
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "$<TARGET_FILE_DIR:${TARGET}>/base/heretic/res"
    VERBATIM)

  add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/user" "$<TARGET_FILE_DIR:${TARGET}>/user"
    VERBATIM)
endfunction()

# EOF

