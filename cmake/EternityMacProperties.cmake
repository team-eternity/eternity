## Copyright (C) 2021 Ioan Chera
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

function(eternity_set_mac_library_output)
  # This is needed to be able to archive (produce) release builds in Xcode!
  # https://stackoverflow.com/a/30328846/11738219
  set_target_properties(acsvm          PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
  set_target_properties(ADLMIDI_static PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
  set_target_properties(png_static     PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
  set_target_properties(snes_spc       PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
endfunction()

function(eternity_set_xcode_attributes TARGET)
  set_target_properties(
    ${TARGET} PROPERTIES
    XCODE_ATTRIBUTE_CLANG_WARN_BLOCK_CAPTURE_AUTORELEASING   YES
    XCODE_ATTRIBUTE_CLANG_WARN_BOOL_CONVERSION               YES
    XCODE_ATTRIBUTE_CLANG_WARN_CONSTANT_CONVERSION           YES
    XCODE_ATTRIBUTE_CLANG_WARN_EMPTY_BODY                    YES
    XCODE_ATTRIBUTE_CLANG_WARN_ENUM_CONVERSION               YES
    XCODE_ATTRIBUTE_CLANG_WARN_INFINITE_RECURSION            YES
    XCODE_ATTRIBUTE_CLANG_WARN_INT_CONVERSION                YES
    XCODE_ATTRIBUTE_CLANG_WARN_NON_LITERAL_NULL_CONVERSION   YES
    XCODE_ATTRIBUTE_CLANG_WARN_STRICT_PROTOTYPES             YES
    XCODE_ATTRIBUTE_CLANG_WARN_UNGUARDED_AVAILABILITY        YES_AGGRESSIVE
    XCODE_ATTRIBUTE_CLANG_WARN_UNREACHABLE_CODE              YES
    XCODE_ATTRIBUTE_GCC_WARN_64_TO_32_BIT_CONVERSION         YES
    XCODE_ATTRIBUTE_GCC_WARN_ABOUT_RETURN_TYPE               YES_ERROR
    XCODE_ATTRIBUTE_GCC_WARN_UNINITIALIZED_AUTOS             YES_AGGRESSIVE
    XCODE_ATTRIBUTE_GCC_WARN_UNUSED_FUNCTION                 YES
    XCODE_ATTRIBUTE_GCC_WARN_UNUSED_VARIABLE                 YES

    XCODE_ATTRIBUTE_CLANG_WARN_RANGE_LOOP_ANALYSIS           YES
    XCODE_ATTRIBUTE_CLANG_WARN_SUSPICIOUS_MOVE               YES

    XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS                  "@executable_path"

    XCODE_ATTRIBUTE_FRAMEWORK_SEARCH_PATHS                   "\"${CMAKE_BINARY_DIR}\""
    XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME                  YES

    XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${CMAKE_SOURCE_DIR}/macosx/eternity/eternity.entitlements

    XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS           YES
    XCODE_ATTRIBUTE_STRIP_INSTALLED_PRODUCT                  NO
  )
endfunction()

## EOF

