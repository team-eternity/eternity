##
## Copyright (C) 2019 Max Waine
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
## along with this program.  If not, see <https://www.gnu.org/licenses/>.
##

project('Eternity Engine', 'c', 'cpp',
   license         : ['GPL-2.0+', 'LGPL-2.1+', 'Libpng', 'Zlib'],
   version         : '4.01.00',
   subproject_dir  : '.', # Maybe one day this can be removed, but not right now
   default_options : ['buildtype=release', 'warning_level=3',
                      'c_std=c11', 'cpp_std=c++17']
)

# This is necessary everywhere
if meson.get_compiler('cpp').get_id() == 'msvc'
  add_global_arguments('-D_CRT_SECURE_NO_WARNINGS',
                       '-D_CRT_NONSTDC_NO_WARNINGS')
endif

# TODO: Compiler hardening from the CMake? Is it idiomatic?
if get_option('debug') || get_option('debugoptimized')
   # has_argument is equivalent to TRY_C_COMPILER_FLAG I think
   # It's a method for the compiler
endif

# In-tree dependencies
dep_acsvm    = dependency('acsvm',    fallback : ['acsvm',    'dep_acsvm'   ])
dep_snes_spc = dependency('snes_spc', fallback : ['snes_spc', 'dep_snes_spc'])

# External dependencies
dep_opengl     = dependency('opengl',     required : false)
dep_sdl2       = dependency('sdl2',       version  : '>=2.0.7')
dep_sdl2_mixer = dependency('sdl2_mixer', version  : '>=2.0.2')
dep_sdl2_net   = dependency('sdl2_net',   version  : '>=2.0.1')

# Wrap dependencies (TODO: Merge with external)
dep_zlib       = subproject('zlib').get_variable('zlib_dep')
dep_libpng     = subproject('libpng').get_variable('png_dep')

subdir('source')

## EOF

