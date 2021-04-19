#!/bin/bash

#
# NOTE: this script is meant to be called from XCode, as configured by CMake. Not manually
#

# args: framework_path target_dir

if [ "$#" -ne 2 ]; then
    echo "Illegal number of arguments, need 2"
    exit 1
fi

framework_path="$1"
target_dir="$2"

target_path="$target_dir/$(basename "$framework_path")"

if [ ! -d "$target_path" ]; then
    echo "Copying $framework_path to $target_dir"
    cp -R "$framework_path" "$target_dir"
    echo "Stripping header files from $target_path"
    find "$target_path" -name "*.h" | while read header; do
        echo "Deleting $header"
        rm "$header"
    done
else
    echo "Not copying $framework_path; $target_dir already exists."
fi
