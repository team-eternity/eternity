#!/bin/bash

# Called from CMake
if [ "$#" -ne 2 ]; then
    echo "Illegal number of arguments, need 2"
    exit 1
fi

game_path="$1"
storage_path="$2"

dirs="SDL2.framework SDL2_mixer.framework SDL2_net.framework"

for dir in $dirs; do
    if [ -d "$storage_path/$dir" ]; then
        rm -rf "$storage_path/$dir"
    fi
    cp -R "$game_path/$dir" "$storage_path"
done

if [ -f "$storage_path/eternity" ]; then
    rm "$storage_path/eternity"
fi
cp "$game_path/eternity" "$storage_path"
