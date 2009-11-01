#! /bin/bash

mkdir ~/.eternity

cp -R base/ ~/.eternity/base

if [ $ETERNITYBASE ]; then
   echo "ETERNITYBASE = $ETERNITYBASE";
else
   echo Adding "export ETERNITYBASE='~/.eternity/base'" to ~/.bashrc;
   export ETERNITYBASE='~/.eternity/base';
   (echo "export ETERNITYBASE='~/.eternity/base'") >> ~/.bashrc;
fi
