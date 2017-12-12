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

find_path(SDL2_MIXER_INCLUDE_DIR SDL_mixer.h
          HINTS ENV SDL2DIR
          PATH_SUFFIXES SDL2 include include/SDL2_mixer)

find_library(SDL2_MIXER_LIBRARY SDL2_mixer
             HINTS ENV SDL2DIR
             PATH_SUFFIXES lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2_MIXER REQUIRED_VARS SDL2_MIXER_LIBRARY SDL2_MIXER_INCLUDE_DIR)

## EOF

