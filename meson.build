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
   default_options : ['buildtype=release', 'warning_level=1',
                      'c_std=c11', 'cpp_std=c++17']
)

# Important global values
cc           = meson.get_compiler('c')
cppc         = meson.get_compiler('cpp')
host_system  = host_machine.system()
debug_config = get_option('buildtype') == 'debug' or get_option('buildtype') == 'debugoptimized'


# This is necessary everywhere
if cppc.get_id() == 'msvc'
   add_global_arguments('-D_CRT_SECURE_NO_WARNINGS',
                        '-D_CRT_NONSTDC_NO_WARNINGS',
      language : ['c', 'cpp']
   )
endif

# TODO: Compiler hardening from the CMake? Is it idiomatic?
if debug_config
   # has_argument is equivalent to TRY_C_COMPILER_FLAG I think
   # It's a method for the compiler
endif

sdl2_ver = ['>=2.0.3']
if host_system == 'linux'
   sdl2_ver += ['!=2.0.9'] # 2.0.9 always crashes when using the GL2D renderer
endif

# In-tree dependencies
dep_acsvm    = dependency('acsvm',    fallback : ['acsvm',       'acsvm_dep'])
dep_snes_spc = dependency('snes_spc', fallback : ['snes_spc', 'snes_spc_dep'])

# Hi-ho, the derry-o... GL stands alone
dep_opengl = dependency('gl', required : false)

# Wrap dependencies
dep_zlib       = dependency('zlib',       static  : true,      fallback : ['zlib',             'zlib_dep'])
dep_libpng     = dependency('libpng',     static  : true,      fallback : ['libpng',            'png_dep'])
dep_sdl2       = dependency('sdl2',       version : sdl2_ver,  fallback : ['sdl2',             'sdl2_dep'])
dep_sdl2_mixer = dependency('SDL2_mixer', version : '>=2.0.2', fallback : ['SDL2_mixer', 'SDL2_mixer_dep'])
dep_sdl2_net   = dependency('SDL2_net',   version : '>=2.0.1', fallback : ['SDL2_net',     'SDL2_net_dep'])


# Conditional dependencies
if host_system == 'windows'
   dep_opengl32 = cppc.find_library('opengl32', required: dep_opengl.found())
   if cppc.get_id() == 'msvc'
      dep_rpcrt4 = cppc.find_library('rpcrt4')
   endif
endif

subdir('source')

## EOF

