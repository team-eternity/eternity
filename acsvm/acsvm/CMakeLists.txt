##-----------------------------------------------------------------------------
##
## Copyright (C) 2015 David Hill
##
## See COPYING for license information.
##
##-----------------------------------------------------------------------------
##
## CMake file for acsvm.
##
##-----------------------------------------------------------------------------


##----------------------------------------------------------------------------|
## Environment Configuration                                                  |
##

include_directories(.)


##----------------------------------------------------------------------------|
## Targets                                                                    |
##

##
## acsvm
##
add_library(acsvm ${ACSVM_SHARED_DECL}
   Action.cpp
   Array.cpp
   CallFunc.cpp
   CodeData.cpp
   Environ.cpp
   Function.cpp
   Init.cpp
   Jump.cpp
   Module.cpp
   ModuleACS0.cpp
   ModuleACSE.cpp
   PrintBuf.cpp
   Scope.cpp
   Script.cpp
   String.cpp
   Thread.cpp
   ThreadExec.cpp
   Tracer.cpp
)

ACSVM_INSTALL_LIB(acsvm)

## EOF

