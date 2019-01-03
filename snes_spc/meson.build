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

project('snes_spc', 'cpp'
   license: 'LGPL-2.1+'
)

snes_spc_inc = include_directories('.')

snes_spc_src = files(
   'dsp.cpp',
   'SNES_SPC.cpp',
   'SNES_SPC_misc.cpp',
   'SNES_SPC_state.cpp',
   'spc.cpp',
   'SPC_DSP.cpp',
   'SPC_Filter.cpp',
)

if meson.get_compiler('cpp').get_id() == 'msvc'
   snes_spc_src += files('snes_spc.def')
endif

snes_spc_lib = shared_library('snes_spc', snes_spc_src,
   cpp_args : '-DSNES_SPC_EXPORTS'
)

dep_snes_spc = declare_dependency(
   link_with           : snes_spc_lib,
   include_directories : snes_spc_inc
)

## EOF

