#!/usr/bin/env bash

## Copyright (c) 2010 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
iwads=( doom2.wad doom2f.wad plutonia.wad tnt.wad doom.wad doomu.wad doom1.wad
heretic.wad heretic1.wad freedoom.wad freedoomu.wad freedoom1.wad hacx.wad )
export ETERNITYBASE="$HOME/.eternity/base"
export DOOMWADDIR="$HOME/.eternity/"
################################################################################
if [ ! -e "$HOME/.eternity/base/startup.wad" ]
then
        echo "$HOME/.eternity/base/startup.wad not found."
        echo "Assuming first run or corrupted base"
        mkdir $HOME/.eternity/
        cp -fRv /usr/share/eternity-engine/base $HOME/.eternity/base
fi

## Search for known IWADs. Check the Debian canonical install locations.
for wadcheck in "${iwads[@]}"
do
        echo "Searching for: $wadcheck"
        if [ ! -e $HOME/.eternity/$wadcheck ]
        then
                if [ -e /usr/share/games/doom/$wadcheck ]
                then
                        echo "Installing: $wadcheck"
                        ln -s /usr/share/games/doom/$wadcheck $HOME/.eternity/$wadcheck
                fi
        else
                echo "Found: $wadcheck"
        fi
done
eternity.real $@
exit

