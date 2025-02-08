#!/bin/bash
#
# NOTE: this script is meant to be called from CMake, when it downloads the external libraries.
#

# Check if we have SDL2.framework
set -e

# Check for this version to perform the hacks
SDL_MIXER_HACKED_VERSION=SDL2_mixer-2.0.4.dmg

#
# Check for framework and download if missing or outdated
#
check_sdl() {
    framework="$1"
    url_base="$2"
    dmg_name="$3"
    special="$4"

    version_cache="$framework.version.txt"

    if [ -d "$framework" ] && [ -f "$version_cache" ]; then
        cached_dmg_name="$(cat "$version_cache")"
        if [ "$cached_dmg_name" != "$dmg_name" ]; then
            echo "Outdated $framework found. Triggering re-download"
            rm -rf "$framework"
        fi
    fi

    if [ ! -d "$framework" ]; then
        echo "No $framework added; downloading DMG."
        curl -LO -J "$url_base/$dmg_name"
        hdiutil attach "$dmg_name" -mountpoint sdlmount
        rm "$framework" || echo "No garbage $framework found, no need to clear it."
        cp -R "sdlmount/$framework" .
        hdiutil detach sdlmount

        # Clean up the downloaded DMG
        rm "$dmg_name"

        # Store the DMG name in a hash so we remember to re-download in case of version change
        echo "$dmg_name" > "$version_cache"

        if [ "$special" = mixer_sign ]; then
            if [ "$dmg_name" != "$SDL_MIXER_HACKED_VERSION" ]; then
                echo "Unexpected SDL2 version to perform hacking. Check what needs to be done with the new version first."
                exit 1
            fi
            # Sign all SDL2_mixer subframeworks. Otherwise we get a "not signed at all" error when building EternityLaunch
            # https://stackoverflow.com/a/29263368/11738219
            for name in FLAC Opus OpusFile modplug mpg123; do
                # Add missing required info.plist data
                echo "Doctoring info.plist of $name."
                work_path=SDL2_mixer.framework/Frameworks/$name.framework/Resources
                info_name=$work_path/Info.plist
                temp_name=$work_path/Info2.plist
                line_count=$(wc -l $info_name | awk '{print $1;}')
                header_length=4
                head -4 $info_name > $temp_name
                echo "<key>CFBundleSupportedPlatforms</key><array><string>MacOSX</string></array>" >> $temp_name
                tail -$(($line_count - $header_length)) $info_name >> $temp_name
                mv $temp_name $info_name
            done
            for name in FLAC Ogg Opus OpusFile Vorbis modplug mpg123; do
                echo "Signing SDL2_mixer subframework $name"
                codesign -f -s - SDL2_mixer.framework/Versions/A/Frameworks/$name.framework/$name
            done
        fi
       # if [ "$special" = sdl_sign ]; then
       #     echo "Signing hidapi."
       #     codesign -f -s --deep - SDL2.framework/Versions/A/Frameworks/hidapi.framework/hidapi
       # fi
    fi
}

check_sdl SDL2.framework https://github.com/libsdl-org/SDL/releases/download/release-2.30.8 SDL2-2.30.8.dmg

# IMPORTANT: in case of lack of MIDI, see this issue:
# https://github.com/libsdl-org/SDL_mixer/issues/419
check_sdl SDL2_mixer.framework https://github.com/libsdl-org/SDL_mixer/releases/download/release-2.8.0 SDL2_mixer-2.8.0.dmg

check_sdl SDL2_net.framework https://github.com/libsdl-org/SDL_net/releases/download/release-2.2.0 SDL2_net-2.2.0.dmg
