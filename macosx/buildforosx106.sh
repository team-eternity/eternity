#!/bin/bash

# -----------------------------------------------------------------------------
# 
#  Copyright(C) 2013 Ioan Chera
# 
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
# 
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
# 
# --------------------------------------------------------------------------
# 
#  DESCRIPTION:
#        Shell script for preparing OSX 10.6 deployment of a C++11 application
#        IMPORTANT: Designed to be run from Xcode. You may need to change envi-
#                   ronment variables if you want to run this manually.
# 
# -----------------------------------------------------------------------------

# quit if not exist files
if [ ! -f libc++/libc++.1.dylib -o ! -f libc++/libc++abi.dylib ]; then
    exit 0
fi

# Must be started from the same directory as Makefile.osx106

# Always use this makefile
mak="make -f Makefile.osx106"

# Prepare the executables
arch1=i386
arch2=x86_64
$mak clean
$mak -j arch=$arch1
$mak clean
$mak -j arch=$arch2
$mak clean

# Merge the executables into a universal binary
lipo -create eternity-$arch1 eternity-$arch2 -output eternity
rm eternity-$arch1 eternity-$arch2

# Copy the program
cd "$CONFIGURATION_BUILD_DIR"
OSX106WRAPPER="$PRODUCT_NAME-106$WRAPPER_SUFFIX"
rm -rf "$OSX106WRAPPER"
cp -r "$WRAPPER_NAME" "$OSX106WRAPPER"

# Go back to project directory
cd -

# Copy the resulted eternity executable into the wrapper
EXECPATH="$CONFIGURATION_BUILD_DIR/$OSX106WRAPPER/Contents/MacOS/$EXECUTABLE_NAME"
mv eternity "$EXECPATH"

# Copy the libc++ and libc++abi files there
cp libc++/libc++.1.dylib "$(dirname "$EXECPATH")"
cp libc++/libc++abi.dylib "$(dirname "$EXECPATH")"

# Remove info.plist unwanted data
PLISTPATH="$CONFIGURATION_BUILD_DIR/$OSX106WRAPPER/Contents/Info.plist"
sed 's/macosx10\.8//g' "$PLISTPATH" | sed 's/<string>10\.7<\/string>/<string>10\.5<\/string>/g' > "$PLISTPATH"2
mv "$PLISTPATH"2 "$PLISTPATH"