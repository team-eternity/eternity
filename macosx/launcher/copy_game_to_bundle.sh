#!/bin/bash

# NOTE: this is to be run from script

if [ "$#" -ne 2 ]; then
    echo "Illegal number of arguments, need 2"
    exit 1
fi

game_path="$1"      # product path of game (contains eternity and stripped frameworks)
target_path="$2"    # product path of app bundle

resource_path="$target_path/../Resources"

dirs="SDL2.framework SDL2_mixer.framework SDL2_net.framework"

for dir in $dirs; do
    if [ -d "$resource_path/$dir" ]; then
        rm -rf "$resource_path/$dir"
    fi
    cp -R "$game_path/$dir" "$resource_path"
done

if [ -f "$resource_path/eternity" ]; then
    rm "$resource_path/eternity"
fi
cp -R "$game_path/eternity" "$resource_path"
