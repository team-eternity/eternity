##-----------------------------------------------------------------------------
##
## Copyright (C) 2015 David Hill
##
## See COPYING for license information.
##
##-----------------------------------------------------------------------------
##
## Root CMake file.
##
##-----------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.6)

project(acsvm)

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)


##----------------------------------------------------------------------------|
## Functions                                                                  |
##

##
## ACSVM_INSTALL_EXE
##
function(ACSVM_INSTALL_EXE name)
   if(ACSVM_INSTALL_EXE)
      install(TARGETS ${name}
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
      )
   endif()
endfunction()

##
## ACSVM_INSTALL_LIB
##
function(ACSVM_INSTALL_LIB name)
   if(ACSVM_INSTALL_LIB)
      if(ACSVM_INSTALL_API)
         install(TARGETS ${name}
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
         )
      elseif(ACSVM_SHARED)
         install(TARGETS ${name}
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
         )
      endif()
   endif()
endfunction()

##
## ACSVM_TRY_C_FLAG
##
if(NOT ACSVM_NOFLAGS)
   function(ACSVM_TRY_C_FLAG flag name)
      CHECK_C_COMPILER_FLAG(${flag} ${name})

      if(${name})
         set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}" PARENT_SCOPE)
      endif()
   endfunction()
endif()

##
## ACSVM_TRY_CXX_FLAG
##
if(NOT ACSVM_NOFLAGS)
   function(ACSVM_TRY_CXX_FLAG flag name)
      CHECK_CXX_COMPILER_FLAG(${flag} ${name})

      if(${name})
         set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}" PARENT_SCOPE)
      endif()
   endfunction()
endif()


##----------------------------------------------------------------------------|
## Environment Detection                                                      |
##

if(NOT ACSVM_NOFLAGS)
   if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
      ACSVM_TRY_C_FLAG(-Wall      ACSVM_C_FLAG_Wall   )
      ACSVM_TRY_C_FLAG(-Wextra    ACSVM_C_FLAG_Wextra )
      ACSVM_TRY_C_FLAG(-Wpedantic ACSVM_C_FLAG_Wpedantic)
      ACSVM_TRY_C_FLAG(-Wshadow   ACSVM_C_FLAG_Wshadow)

      ACSVM_TRY_C_FLAG(-std=c11 ACSVM_C_FLAG_C11)
   endif()

   if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      ACSVM_TRY_CXX_FLAG(-Wall      ACSVM_CXX_FLAG_Wall   )
      ACSVM_TRY_CXX_FLAG(-Wextra    ACSVM_CXX_FLAG_Wextra )
      ACSVM_TRY_CXX_FLAG(-Wpedantic ACSVM_CXX_FLAG_Wpedantic)
      ACSVM_TRY_CXX_FLAG(-Wshadow   ACSVM_CXX_FLAG_Wshadow)

      ACSVM_TRY_CXX_FLAG(-Wno-gnu-label-as-value ACSVM_CXX_FLAG_Wno_gnu_label)

      ACSVM_TRY_CXX_FLAG(-std=c++11 ACSVM_CXX_FLAG_CXX11)
   endif()
endif()


##----------------------------------------------------------------------------|
## Variables                                                                  |
##

##
## ACSVM_INSTALL_API
##
if(NOT DEFINED ACSVM_INSTALL_API)
   set(ACSVM_INSTALL_API OFF CACHE BOOL "Install ACSVM headers.")
endif()

##
## ACSVM_INSTALL_DOC
##
if(NOT DEFINED ACSVM_INSTALL_DOC)
   set(ACSVM_INSTALL_DOC ON CACHE BOOL "Install ACSVM documentation.")
endif()

##
## ACSVM_INSTALL_EXE
##
if(NOT DEFINED ACSVM_INSTALL_EXE)
   set(ACSVM_INSTALL_EXE ON CACHE BOOL "Install ACSVM executables.")
endif()

##
## ACSVM_INSTALL_LIB
##
if(NOT DEFINED ACSVM_INSTALL_LIB)
   set(ACSVM_INSTALL_LIB ON CACHE BOOL "Install ACSVM libraries.")
endif()

##
## ACSVM_SHARED
##
## If true (or equiavalent), libraries will be built as SHARED. Otherwise,
## they are built as STATIC.
##
if(NOT DEFINED ACSVM_SHARED)
   set(ACSVM_SHARED ON CACHE BOOL "Build libraries as shared objects.")
endif()

##
## ACSVM_SHARED_DECL
##
## Used internally for convenience in add_library commands.
##
if(ACSVM_SHARED)
   set(ACSVM_SHARED_DECL SHARED)
else()
   set(ACSVM_SHARED_DECL STATIC)
endif()


##----------------------------------------------------------------------------|
## Environment Configuration                                                  |
##

include_directories(.)


##----------------------------------------------------------------------------|
## Targets                                                                    |
##

add_subdirectory(acsvm)

if(EXISTS "${CMAKE_SOURCE_DIR}/capi")
   add_subdirectory(capi)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/exec")
   add_subdirectory(exec)
endif()

## EOF

