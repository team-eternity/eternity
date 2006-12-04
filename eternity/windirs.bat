@echo off
echo Creating directory trees
if not exist Debug goto errend
if not exist Release goto errend
md Debug\base
md Debug\base\doom
md Debug\base\heretic
md Release\base
md Release\base\doom
md Release\base\heretic
echo Copying EDF files
copy edf\*.edf Debug\base
copy edf\*.edf Release\base
echo Copying WAD files
copy wads\eternity.wad Debug\base\doom
copy wads\eternity.wad Release\base\doom
copy wads\eterhtic.wad Debug\base\heretic
del Debug\base\heretic\eternity.wad
ren Debug\base\heretic\eterhtic.wad eternity.wad
copy wads\eterhtic.wad Release\base\heretic
del Release\base\heretic\eternity.wad
ren Release\base\heretic\eterhtic.wad eternity.wad
goto :EOF
:errend
echo Error: build Visual C++ targets first!
