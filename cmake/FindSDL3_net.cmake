## Copyright (C) 2024 Max Waine
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

find_path(SDL3_NET_INCLUDE_DIR SDL_net.h
          HINTS ENV SDL3DIR
          PATH_SUFFIXES SDL3 include include/SDL3_net)

find_library(SDL3_NET_LIBRARY SDL3_net
             HINTS ENV SDL3DIR
             PATH_SUFFIXES lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL3_net REQUIRED_VARS SDL3_NET_LIBRARY SDL3_NET_INCLUDE_DIR)

## EOF

