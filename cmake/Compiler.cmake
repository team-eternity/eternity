## Copyright (c) 2010 Jamie Jones <jamie_jones_au@yahoo.com.au>
## Copyright (C) 2013 David Hill
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

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)


################################################################################
######################### Functions  ###########################################

##
## try_c_compiler_flag
##
function(try_c_compiler_flag flag name)
   check_c_compiler_flag("${flag}" ${name})

   if(${name})
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}" PARENT_SCOPE)
   endif()
endfunction()

##
## try_cxx_compiler_flag
##
function(try_cxx_compiler_flag flag name)
   check_cxx_compiler_flag(${flag} ${name})

   if(${name})
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}" PARENT_SCOPE)
   endif()
endfunction()


################################################################################
######################### Compiler: Features  ##################################
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
   try_c_compiler_flag(-std=c++17 FLAG_CXX_stdcxx17)
endif()

# Crappy hack for GCC because it's a bad compiler
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
   try_c_compiler_flag(-lstdc++fs FLAG_CXX_lstdcxxfs)
endif()


################################################################################
######################### Compiler: Warnings  ##################################
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
   try_c_compiler_flag(-Wall   FLAG_C_Wall)
  #try_c_compiler_flag(-Wextra FLAG_C_Wextra)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
   try_c_compiler_flag(-Wall   FLAG_CXX_Wall)
  #try_c_compiler_flag(-Wextra FLAG_CXX_Wextra)
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "Intel")
   try_c_compiler_flag(-Wall              FLAG_C_Wall)
   try_c_compiler_flag(-Wcheck            FLAG_C_Wcheck)
   try_c_compiler_flag(-Wp64              FLAG_C_Wp64)
   try_c_compiler_flag(-Wshorten-64-to-32 FLAG_C_Wshorten_64_to_32)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
   try_c_compiler_flag(-Wall              FLAG_CXX_Wall)
   try_c_compiler_flag(-Wcheck            FLAG_CXX_Wcheck)
   try_c_compiler_flag(-Wp64              FLAG_CXX_Wp64)
   try_c_compiler_flag(-Wshorten-64-to-32 FLAG_CXX_Wshorten_64_to_32)
endif()

if(MSVC)
   add_definitions(-D_CRT_SECURE_NO_WARNINGS)
   add_definitions(-D_CRT_NONSTDC_NO_WARNINGS)
endif()


################################################################################
######################### Compiler: Hardening  #################################
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
   if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
      try_c_compiler_flag(-fstack-protector FLAG_C_fstack_protector)

      if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
         try_c_compiler_flag(-fPIC              FLAG_C_fPIC)
         try_c_compiler_flag(-pie               FLAG_C_pie)
         try_c_compiler_flag(-Wl,-z,relro       FLAG_C_Wl_z_relro)
         try_c_compiler_flag(-Wl,-z,now         FLAG_C_Wl_z_now)
         try_c_compiler_flag(-Wl,--as-needed    FLAG_C_Wl_as_needed)
         try_c_compiler_flag(-Wl,-z,noexecstack FLAG_C_Wl_z_noexecstack)
      endif()
   endif()

   if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      try_c_compiler_flag(-fstack-protector FLAG_CXX_fstack_protector)

      if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
         try_c_compiler_flag(-fPIC              FLAG_CXX_fPIC)
         try_c_compiler_flag(-pie               FLAG_CXX_pie)
         try_c_compiler_flag(-Wl,-z,relro       FLAG_CXX_Wl_z_relro)
         try_c_compiler_flag(-Wl,-z,now         FLAG_CXX_Wl_z_now)
         try_c_compiler_flag(-Wl,--as-needed    FLAG_CXX_Wl_as_needed)
         try_c_compiler_flag(-Wl,-z,noexecstack FLAG_CXX_Wl_z_noexecstack)
      endif()
   endif()

   if(CMAKE_C_COMPILER_ID STREQUAL "Intel")
      try_c_compiler_flag(-fstack-protector  FLAG_C_fstackprotector)
      try_c_compiler_flag(-fPIC              FLAG_C_fPIC)
      try_c_compiler_flag(-pie               FLAG_C_pie)
      try_c_compiler_flag(-Wl,-z,relro       FLAG_C_Wl_z_relro)
      try_c_compiler_flag(-Wl,-z,now         FLAG_C_Wl_z_now)
      try_c_compiler_flag(-Wl,--as-needed    FLAG_C_Wl_as_needed)
      try_c_compiler_flag(-Wl,-z,noexecstack FLAG_C_Wl_z_noexecstack)
   endif()

   if(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
      try_c_compiler_flag(-fstack-protector  FLAG_CXX_fstackprotector)
      try_c_compiler_flag(-fPIC              FLAG_CXX_fPIC)
      try_c_compiler_flag(-pie               FLAG_CXX_pie)
      try_c_compiler_flag(-Wl,-z,relro       FLAG_CXX_Wl_z_relro)
      try_c_compiler_flag(-Wl,-z,now         FLAG_CXX_Wl_z_now)
      try_c_compiler_flag(-Wl,--as-needed    FLAG_CXX_Wl_as_needed)
      try_c_compiler_flag(-Wl,-z,noexecstack FLAG_CXX_Wl_z_noexecstack)
   endif()

   if(MSVC)
      try_c_compiler_flag(/GS FLAG_C_GS)

      try_c_compiler_flag(/GS FLAG_CXX_GS)
   endif()
endif()


################################################################################
######################### Compiler: Optimisation  ##############################
if(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel" OR CMAKE_BUILD_TYPE STREQUAL "Release")
   if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      try_c_compiler_flag(-fomit-frame-pointer FLAG_C_fomit_frame_pointer)
   endif()

   if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      try_c_compiler_flag(-fomit-frame-pointer FLAG_CXX_fomit_frame_pointer)
   endif()

   if(CMAKE_C_COMPILER_ID STREQUAL "Intel")
      try_c_compiler_flag(-fomit-frame-pointer FLAG_C_fomit_frame_pointer)
      try_c_compiler_flag(-ipo                 FLAG_C_ipo)
   endif()

   if(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
      try_c_compiler_flag(-fomit-frame-pointer FLAG_CXX_fomit_frame_pointer)
      try_c_compiler_flag(-ipo                 FLAG_CXX_ipo)
   endif()
endif()

