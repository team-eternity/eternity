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

acsvm_inc = include_directories('.')

acsvm_src = files(
   'ACSVM/Action.cpp',
   'ACSVM/Array.cpp',
   'ACSVM/BinaryIO.cpp',
   'ACSVM/CallFunc.cpp',
   'ACSVM/CodeData.cpp',
   'ACSVM/Environment.cpp',
   'ACSVM/Error.cpp',
   'ACSVM/Function.cpp',
   'ACSVM/Init.cpp',
   'ACSVM/Jump.cpp',
   'ACSVM/Module.cpp',
   'ACSVM/ModuleACS0.cpp',
   'ACSVM/ModuleACSE.cpp',
   'ACSVM/PrintfBuf.cpp',
   'ACSVM/Scope.cpp',
   'ACSVM/Script.cpp',
   'ACSVM/Serial.cpp',
   'ACSVM/String.cpp',
   'ACSVM/Thread.cpp',
   'ACSVM/ThreadExec.cpp',
   'ACSVM/Tracer.cpp',
)

acsvm_lib = static_library('acsvm', acsvm_src)

dep_acsvm = declare_dependency(link_with : acsvm_lib,
                               include_directories : acsvm_inc)

## EOF

