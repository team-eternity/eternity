## Copyright (C) 2020 Alex Mayfield
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

set(DOWNLOAD_LIB, FALSE)
function(check_lib_needs_downloading LIBRARY INCLUDE_DIR LIBRARY_DIR)
   set(${DOWNLOAD_LIB} FALSE PARENT_SCOPE)

   if(NOT DEFINED ${LIBRARY} OR NOT DEFINED ${INCLUDE_DIR})
      set(DOWNLOAD_LIB TRUE PARENT_SCOPE)
      return()
   endif()

   if(DEFINED ${LIBRARY} AND NOT EXISTS ${${LIBRARY}})
      set(DOWNLOAD_LIB TRUE PARENT_SCOPE)
      return()
   endif()

   if(DEFINED ${INCLUDE_DIR} AND NOT EXISTS ${${INCLUDE_DIR}})
      set(DOWNLOAD_LIB TRUE PARENT_SCOPE)
      return()
   endif()
  
   if(WIN32)
      if(MSVC)
         if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${LIBRARY_DIR}")
            set(DOWNLOAD_LIB TRUE PARENT_SCOPE)
            return()
         endif()
      else()
         if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${LIBRARY_DIR}/x86_64-w64-mingw32")
               set(DOWNLOAD_LIB TRUE PARENT_SCOPE)
               return()
            endif()
         else()
            if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${LIBRARY_DIR}/i686-w64-mingw32")
               set(DOWNLOAD_LIB TRUE PARENT_SCOPE)
               return()
            endif()
         endif()
      endif()
   endif()
endfunction()

find_package(OpenGL)

### SDL3 ###

set(SDL3_VERSION "SDL3-3.0.0" CACHE STRING "" FORCE)
check_lib_needs_downloading(SDL3_LIBRARY SDL3_INCLUDE_DIR ${SDL3_VERSION})
if(WIN32 AND DOWNLOAD_LIB)
   if(MSVC)
      file(DOWNLOAD
         "https://192.168.69.140/content/downloads/sdl3/SDL3-devel-3.0.0-VC.zip"
         "${CMAKE_CURRENT_BINARY_DIR}/SDL3-VC.zip"
         #EXPECTED_HASH SHA256=6413358B67F19B5398E902C576518D5350394A863EB53EB27BB9A81E75A36958
		 )
      execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
         "${CMAKE_CURRENT_BINARY_DIR}/SDL3-VC.zip"
         WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

      set(SDL3_DIR "${CMAKE_CURRENT_BINARY_DIR}/${SDL3_VERSION}")
      set(SDL3_INCLUDE_DIR "${SDL3_DIR}/include" CACHE PATH "" FORCE)
      if(CMAKE_SIZEOF_VOID_P EQUAL 8)
         set(SDL3_LIBRARY "${SDL3_DIR}/lib/x64/SDL3.lib" CACHE FILEPATH "" FORCE)
      else()
         set(SDL3_LIBRARY "${SDL3_DIR}/lib/x86/SDL3.lib" CACHE FILEPATH "")
      endif()
   else()
      file(DOWNLOAD
         "https://www.libsdl.org/release/SDL3-devel-3.0.0-mingw.tar.gz"
         "${CMAKE_CURRENT_BINARY_DIR}/SDL3-mingw.tar.gz"
         #EXPECTED_HASH SHA256=D44BD8CD869219F43634A898D7735D0C92D4B2CCA129F709DC8B98402B36D5F3
		 )
      execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
         "${CMAKE_CURRENT_BINARY_DIR}/SDL3-mingw.tar.gz"
         WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

      if(CMAKE_SIZEOF_VOID_P EQUAL 8)
         set(SDL3_DIR "${CMAKE_CURRENT_BINARY_DIR}/${SDL3_VERSION}/x86_64-w64-mingw32")
      else()
         set(SDL3_DIR "${CMAKE_CURRENT_BINARY_DIR}/${SDL3_VERSION}/i686-w64-mingw32")
      endif()
      set(SDL3_INCLUDE_DIR "${SDL3_DIR}/include/SDL3" CACHE PATH "" FORCE)
      set(SDL3_LIBRARY "${SDL3_DIR}/lib/libSDL3.dll.a" CACHE FILEPATH "" FORCE)
   endif()
endif()

if(APPLE)
   execute_process(
      COMMAND ${CMAKE_SOURCE_DIR}/macosx/download_sdl.sh
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
   )
   set(SDL3_LIBRARY "-framework SDL3" CACHE FILEPATH "")
   set(SDL3_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/SDL3.Framework/Headers CACHE FILEPATH "")

   set(SDL3_MIXER_LIBRARY "-framework SDL3_mixer" CACHE FILEPATH "")
   set(SDL3_MIXER_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/SDL3_mixer.Framework/Headers CACHE FILEPATH "")

   set(SDL3_NET_LIBRARY "-framework SDL3_net" CACHE FILEPATH "")
   set(SDL3_NET_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/SDL3_net.Framework/Headers CACHE FILEPATH "")

   # Needed for access
   if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/SDL3)
      file(
         CREATE_LINK ${CMAKE_CURRENT_BINARY_DIR}/SDL3.Framework/Headers
               ${CMAKE_CURRENT_BINARY_DIR}/SDL3 SYMBOLIC
      )
   endif()
endif()

find_package(SDL3)

if(SDL3_FOUND)
   # SDL3 target.
   add_library(SDL3::SDL3 UNKNOWN IMPORTED GLOBAL)
   set_target_properties(SDL3::SDL3 PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${SDL3_INCLUDE_DIR}"
      IMPORTED_LOCATION "${SDL3_LIBRARY}")
endif()

### SDL3_mixer ###

set(SDL3_MIXER_VERSION "SDL3_mixer-3.0.0" CACHE STRING "" FORCE)
check_lib_needs_downloading(SDL3_MIXER_LIBRARY SDL3_MIXER_INCLUDE_DIR ${SDL3_MIXER_VERSION})
if(WIN32 AND DOWNLOAD_LIB)
   if(MSVC)
      file(DOWNLOAD
         "https://192.168.69.140/content/downloads/sdl3/SDL3_mixer-devel-3.0.0-VC.zip"
         "${CMAKE_CURRENT_BINARY_DIR}/SDL3_mixer-VC.zip"
         #EXPECTED_HASH SHA256=47A5713937F8D8B903A9C5555FEF3EC73A793EF95ABDD078773FC11FCB00EC8A
		 )
      execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
         "${CMAKE_CURRENT_BINARY_DIR}/SDL3_mixer-VC.zip"
         WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

      set(SDL3_MIXER_DIR "${CMAKE_CURRENT_BINARY_DIR}/${SDL3_MIXER_VERSION}")
      set(SDL3_MIXER_INCLUDE_DIR "${SDL3_MIXER_DIR}/include" CACHE PATH "" FORCE)
      if(CMAKE_SIZEOF_VOID_P EQUAL 8)
         set(SDL3_MIXER_LIBRARY "${SDL3_MIXER_DIR}/lib/x64/SDL3_mixer.lib" CACHE FILEPATH "" FORCE)
      else()
         set(SDL3_MIXER_LIBRARY "${SDL3_MIXER_DIR}/lib/x86/SDL3_mixer.lib" CACHE FILEPATH "" FORCE)
      endif()
   else()
      file(DOWNLOAD
         "https://www.libsdl.org/projects/SDL_mixer/release/SDL3_mixer-devel-3.0.0-mingw.tar.gz"
         "${CMAKE_CURRENT_BINARY_DIR}/SDL3_mixer-mingw.tar.gz"
         #EXPECTED_HASH SHA256=4F426C9F0863707A1315333C7E41E0B56099FA412D26A27756AD8FA4EF0240AE
		 )
      execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
         "${CMAKE_CURRENT_BINARY_DIR}/SDL3_mixer-mingw.tar.gz"
         WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

      if(CMAKE_SIZEOF_VOID_P EQUAL 8)
         set(SDL3_MIXER_DIR "${CMAKE_CURRENT_BINARY_DIR}/${SDL3_MIXER_VERSION}/x86_64-w64-mingw32")
      else()
         set(SDL3_MIXER_DIR "${CMAKE_CURRENT_BINARY_DIR}/${SDL3_MIXER_VERSION}/i686-w64-mingw32")
      endif()
      set(SDL3_MIXER_INCLUDE_DIR "${SDL3_MIXER_DIR}/include/SDL3" CACHE PATH "" FORCE)
      set(SDL3_MIXER_LIBRARY "${SDL3_MIXER_DIR}/lib/libSDL3_mixer.dll.a" CACHE FILEPATH "" FORCE)
   endif()
endif()

find_package(SDL3_mixer)

if(SDL3_MIXER_FOUND)
   # SDL3_mixer target.
   add_library(SDL3::mixer UNKNOWN IMPORTED GLOBAL)
   set_target_properties(SDL3::mixer PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${SDL3_MIXER_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES SDL3::SDL3
      IMPORTED_LOCATION "${SDL3_MIXER_LIBRARY}")
endif()

### SDL3_net ###

set(SDL3_NET_VERSION "SDL3_net-3.0.0" CACHE STRING "" FORCE)
check_lib_needs_downloading(SDL3_NET_LIBRARY SDL3_NET_INCLUDE_DIR ${SDL3_NET_VERSION})
if(WIN32 AND DOWNLOAD_LIB)
   if(MSVC)
      file(DOWNLOAD
         "https://192.168.69.140/content/downloads/sdl3/SDL3_net-devel-3.0.0-VC.zip"
         "${CMAKE_CURRENT_BINARY_DIR}/SDL3_net-VC.zip"
         #EXPECTED_HASH SHA256=F364E55BABB44E47B41D039A43C640AA1F76615B726855591B555321C7D870DD
		 )
      execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
         "${CMAKE_CURRENT_BINARY_DIR}/SDL3_net-VC.zip"
         WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

      set(SDL3_NET_DIR "${CMAKE_CURRENT_BINARY_DIR}/${SDL3_NET_VERSION}")
      set(SDL3_NET_INCLUDE_DIR "${SDL3_NET_DIR}/include" CACHE PATH "" FORCE)
      if(CMAKE_SIZEOF_VOID_P EQUAL 8)
         set(SDL3_NET_LIBRARY "${SDL3_NET_DIR}/lib/x64/SDL3_net.lib" CACHE FILEPATH "" FORCE)
      else()
         set(SDL3_NET_LIBRARY "${SDL3_NET_DIR}/lib/x86/SDL3_net.lib" CACHE FILEPATH "" FORCE)
      endif()
   else()
      file(DOWNLOAD
         "https://www.libsdl.org/projects/SDL_net/release/SDL3_net-devel-3.0.0-mingw.tar.gz"
         "${CMAKE_CURRENT_BINARY_DIR}/SDL3_net-mingw.tar.gz"
         #EXPECTED_HASH SHA256=369829E06C509D5E001FABDCBE006FF3EFA934F3825DC0AE1B076F5CE9C183C4
		 )
      execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf
         "${CMAKE_CURRENT_BINARY_DIR}/SDL3_net-mingw.tar.gz"
         WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

      if(CMAKE_SIZEOF_VOID_P EQUAL 8)
         set(SDL3_NET_DIR "${CMAKE_CURRENT_BINARY_DIR}/${SDL3_NET_VERSION}/x86_64-w64-mingw32")
      else()
         set(SDL3_NET_DIR "${CMAKE_CURRENT_BINARY_DIR}/${SDL3_NET_VERSION}/i686-w64-mingw32")
      endif()
      set(SDL3_NET_INCLUDE_DIR "${SDL3_NET_DIR}/include/SDL3" CACHE PATH "" FORCE)
      set(SDL3_NET_LIBRARY "${SDL3_NET_DIR}/lib/libSDL3_net.dll.a" CACHE FILEPATH "" FORCE)
   endif()
endif()

find_package(SDL3_net)

if(SDL3_NET_FOUND)
   # SDL3_net target.
   add_library(SDL3::net UNKNOWN IMPORTED GLOBAL)
   set_target_properties(SDL3::net PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${SDL3_NET_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES SDL3::SDL3
      IMPORTED_LOCATION "${SDL3_NET_LIBRARY}")
endif()

## EOF

