#! /bin/bash

mkdir ~/.eternity
mkdir ~/.eternity/base
mkdir ~/.eternity/base/doom
mkdir ~/.eternity/base/heretic
mkdir ~/.eternity/base/shots

cp edf/*.edf ~/.eternity/base
cp wads/eternity.wad ~/.eternity/base/doom
cp wads/eterhtic.wad ~/.eternity/base/heretic/eternity.wad

if [ $ETERNITYBASE ]; then
   echo "ETERNITYBASE = $ETERNITYBASE";
else
   echo Adding "export ETERNITYBASE='~/.eternity/base'" to ~/.bashrc;
   export ETERNITYBASE='~/.eternity/base';
   (echo "export ETERNITYBASE='~/.eternity/base'") >> ~/.bashrc;
fi
