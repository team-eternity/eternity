## Copyright (C) 2017 Max Waine
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see http://www.gnu.org/licenses/
##
################################################################################

find_path(SDL2_INCLUDE_DIR SDL.h
		  HINTS ENV SDL2DIR
		  PATH_SUFFIXES SDL2 include include/SDL2)

find_library(SDL2_LIBRARY SDL2
		     HINTS ENV SDL2DIR
		     PATH_SUFFIXES lib)

find_library(SDL2_MAIN_LIBRARY SDL2main
		     HINTS ENV SDL2DIR
		     PATH_SUFFIXES lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2 REQUIRED_VARS SDL2_LIBRARY SDL2_INCLUDE_DIR)

set(SDL2_MAIN_FOUND NO)
if(SDL2_MAIN_LIBRARY)
   set(SDL2_MAIN_FOUND YES)
endif()

## EOF

