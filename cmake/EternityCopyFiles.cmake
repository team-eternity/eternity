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

function(eternity_copy_libs TARGET)
  set(ETERNITY_DLLS "")

  if(WIN32)
    if(MSVC)
      set(SDL2_DLL_DIR "$<TARGET_FILE_DIR:SDL2::SDL2>")
      set(SDL2_MIXER_DLL_DIR "$<TARGET_FILE_DIR:SDL2::mixer>")
      set(SDL2_NET_DLL_DIR "$<TARGET_FILE_DIR:SDL2::net>")
    else()
      set(SDL2_DLL_DIR "$<TARGET_FILE_DIR:SDL2::SDL2>/../bin")
      set(SDL2_MIXER_DLL_DIR "$<TARGET_FILE_DIR:SDL2::mixer>/../bin")
      set(SDL2_NET_DLL_DIR "$<TARGET_FILE_DIR:SDL2::net>/../bin")
    endif()

    # SDL2
    list(APPEND ETERNITY_DLLS "${SDL2_DLL_DIR}/SDL2.dll")

    # SDL2_mixer
    list(APPEND ETERNITY_DLLS "${SDL2_MIXER_DLL_DIR}/libFLAC-8.dll")
    list(APPEND ETERNITY_DLLS "${SDL2_MIXER_DLL_DIR}/libmodplug-1.dll")
    list(APPEND ETERNITY_DLLS "${SDL2_MIXER_DLL_DIR}/libmpg123-0.dll")
    list(APPEND ETERNITY_DLLS "${SDL2_MIXER_DLL_DIR}/libogg-0.dll")
    list(APPEND ETERNITY_DLLS "${SDL2_MIXER_DLL_DIR}/libopus-0.dll")
    list(APPEND ETERNITY_DLLS "${SDL2_MIXER_DLL_DIR}/libopusfile-0.dll")
    list(APPEND ETERNITY_DLLS "${SDL2_MIXER_DLL_DIR}/libvorbis-0.dll")
    list(APPEND ETERNITY_DLLS "${SDL2_MIXER_DLL_DIR}/libvorbisfile-3.dll")
    list(APPEND ETERNITY_DLLS "${SDL2_MIXER_DLL_DIR}/SDL2_mixer.dll")

    # SDL2_net
    list(APPEND ETERNITY_DLLS "${SDL2_NET_DLL_DIR}/SDL2_net.dll")
  endif()

  # Copy library files to target directory.
  foreach(ETERNITY_DLL ${ETERNITY_DLLS})
    add_custom_command(TARGET ${TARGET} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${ETERNITY_DLL}" $<TARGET_FILE_DIR:${TARGET}> VERBATIM)
  endforeach()
endfunction()

function(eternity_copy_base_and_user TARGET)
  add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${CMAKE_SOURCE_DIR}/base" "$<TARGET_FILE_DIR:${TARGET}>/base" VERBATIM)

  add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${CMAKE_SOURCE_DIR}/user" "$<TARGET_FILE_DIR:${TARGET}>/user" VERBATIM)
endfunction()

# EOF

