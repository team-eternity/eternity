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
   license : 'GPLv3+',
   version : '4.01.00',
   default_options : ['warning_level=3', 'c_std=c11', 'cpp_std=c++17']
)

# External dependencies
opengl_dep     = dependency('opengl')
sdl2_dep       = dependency('sdl2',       version : '>=2.0.7')
sdl2_mixer_dep = dependency('sdl2_mixer', version : '>=2.0.2')
sdl2_net_dep   = dependency('sdl2_net',   version : '>=2.0.1')

# In-tree dependencies
acsvm_dep    = dependency('acsvm',    fallback : ['acsvm',    'acsvm_dep'   ])
libpng_dep   = dependency('libpng',   fallback : ['libpng',   'libpng_dep'  ])
snes_spc_dep = dependency('snes_spc', fallback : ['snes_spc', 'snes_spc_dep'])
zlib_dep     = dependency('zlib',     fallback : ['zlib', '    zlib_dep'    ])

eternity_deps = [
   # External dependencies
   opengl_dep,
   sdl2_dep,
   sdl2_mixer_dep,
   sdl2_net_dep,

   # In-tree dependencies
   acsvm_dep,
   libpng_dep,
   snes_spc_dep,  
   zlib_dep,
]

subdir('source')

## EOF

