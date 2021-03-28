#
# Copyright(C) 2016-2019 Samuel Villarreal
# Copyright(C) 2021 Max Waine
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#

project(mcpp)

add_library(mcpp STATIC
    src/configed.H
    src/directive.c
    src/eval.c
    src/expand.c
    src/internal.H
    src/main.c
    src/mbchar.c
    src/mcpp_lib.h
    src/mcpp_out.h
    src/noconfig.H
    src/preproc.c
    src/rwops.c
    src/rwops.h
    src/support.c
    src/system.c
    src/system.H
)

set_target_properties(mcpp PROPERTIES PREFIX "")
set_target_properties(mcpp PROPERTIES LINKER_LANGUAGE C)

target_include_directories(mcpp PUBLIC src)
target_compile_definitions(mcpp PRIVATE _LIB;_CRT_SECURE_NO_WARNINGS;UNICODE;_UNICODE;MCPP_LIB=1)
set_target_properties(mcpp PROPERTIES CMAKE_STATIC_LINKER_FLAGS_RELEASE "/LTCG")
set_target_properties(mcpp PROPERTIES CMAKE_STATIC_LINKER_FLAGS_SHIPPING "/LTCG")

if(WIN32)
    target_compile_options(mcpp PRIVATE /W0)
endif()
