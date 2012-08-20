## Copyright (c) 2012 David Hill
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
################################################################################
######################### CMake Configuration ##################################
cmake_minimum_required(VERSION 2.4)

################################################################################
######################### Set Project Source  ##################################
if(${CMAKE_BUILD_TYPE} STREQUAL Debug)
   add_definitions(-DDEBUG)
else(${CMAKE_BUILD_TYPE} STREQUAL Debug)
   add_definitions (-DNDEBUG)
endif(${CMAKE_BUILD_TYPE} STREQUAL Debug)

# Defines from the original automake system.
add_definitions(-DXP_UNIX -DSVR4 -DSYSV -D_BSD_SOURCE -DPOSIX_SOURCE -DHAVE_LOCALTIME_R)
add_definitions(-DHAVE_VA_COPY -DVA_COPY=va_copy)

# Static linking, definitely.
add_definitions(-DSTATIC_JS_API)

include_directories(. "${CMAKE_BINARY_DIR}")

# Generate jsautocfg.h
set(JSAUTOCFG_SRC "${CMAKE_CURRENT_SOURCE_DIR}/jscpucfg.c")
set(JSAUTOCFG_HDR "${CMAKE_BINARY_DIR}/jsautocfg.h")

if(NOT EXISTS "${JSAUTOCFG_HDR}")
   message(STATUS "Generate jsautocfg.h")

   try_run(JSAUTOCFG_RUN_RESULT JSAUTOCFG_COMPILE_RESULT
           ${CMAKE_BINARY_DIR} ${JSAUTOCFG_SRC}
           COMPILE_OUTPUT_VARIABLE JSAUTOCFG_COMPILE_OUTPUT
           RUN_OUTPUT_VARIABLE JSAUTOCFG_RUN_OUTPUT)

   if(NOT JSAUTOCFG_COMPILE_RESULT)
      message(STATUS "Generate jsautocfg.h - failed")

      file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
           "jscpucfg.c failed to compile with output:\n${JSAUTOCFG_COMPILE_OUTPUT}\n\n")

   elseif(JSAUTOCFG_RUN_RESULT)
      message(STATUS "Generate jsautocfg.h - failed")

      file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
           "jscpucfg.c failed to run with output:\n${JSAUTOCFG_RUN_OUTPUT}\n\n")

   else()
      file(WRITE "${JSAUTOCFG_HDR}" "${JSAUTOCFG_RUN_OUTPUT}")

      message(STATUS "Generate jsautocfg.h - done")
   endif()
endif()

# Test for array-type va_list.
if(NOT DEFINED HAVE_VA_LIST_AS_ARRAY)
   message(STATUS "Check for array-type va_list")

   set(HAVE_VA_LIST_AS_ARRAY_SRC "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/Check_HAVE_VA_LIST_AS_ARRAY.cpp")

   file(WRITE ${HAVE_VA_LIST_AS_ARRAY_SRC}
        "#include <stdarg.h>\n"
        "void f1(va_list *) {}\n"
        "void f2(va_list ap) {f1(&ap);}\n"
        "int main() {}\n")

   try_run(HAVE_VA_LIST_AS_ARRAY_RUN_RESULT HAVE_VA_LIST_AS_ARRAY_COMPILE_RESULT
           ${CMAKE_BINARY_DIR} ${HAVE_VA_LIST_AS_ARRAY_SRC}
           COMPILE_OUTPUT_VARIABLE HAVE_VA_LIST_AS_ARRAY_COMPILE_OUTPUT
           RUN_OUTPUT_VARIABLE HAVE_VA_LIST_AS_ARRAY_RUN_OUTPUT)

   if(NOT HAVE_VA_LIST_AS_ARRAY_COMPILE_RESULT)
      set(HAVE_VA_LIST_AS_ARRAY 1 CACHE INTERNAL "HAVE_VA_LIST_AS_ARRAY")

   else()
      set(HAVE_VA_LIST_AS_ARRAY 0 CACHE INTERNAL "HAVE_VA_LIST_AS_ARRAY")

   endif()

   message(STATUS "Check for array-type va_list - done")
endif()

if(HAVE_VA_LIST_AS_ARRAY)
   add_definitions(-DHAVE_VA_LIST_AS_ARRAY)
endif()

################################################################################
######################### Set Build Targets   ##################################
add_library(js STATIC
   jsapi.c
   jsarena.c
   jsarray.c
   jsatom.c
   jsbool.c
   jscntxt.c
   jsdate.c
   jsdbgapi.c
   jsdhash.c
   jsdtoa.c
   jsemit.c
   jsexn.c
   jsfile.c
   jsfun.c
   jsgc.c
   jshash.c
   jsinterp.c
   jsinvoke.c
   jsiter.c
   jskwgen.c
   jslock.c
   jslog2.c
   jslong.c
   jsmath.c
   jsnum.c
   jsobj.c
   jsopcode.c
   jsparse.c
   jsprf.c
   jsregexp.c
   jsscan.c
   jsscope.c
   jsscript.c
   jsstr.c
   jsutil.c
   jsxdrapi.c
   jsxml.c
   prmjtime.c
)

# EOF

