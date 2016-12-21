@ECHO OFF
ECHO THIS MUST BE RUN FROM THE DEVELOPER COMMAND PROMPT FOR VS2015
ECHO 1. 32-bit
ECHO 2. 64-bit
SET SELECTION=
SET /P SELECTION=Enter selection: %=%
IF %SELECTION%=="1"  GOTO win32
IF %SELECTION%=="2"  GOTO win64

:win32
ECHO Building Win32
midl -Oicf -W1 -Zp8 /env win32 /protocol dce /app_config midiproc.idl
EXIT /B
:win64
ECHO Building Win64
midl -Oicf -W1 -Zp8 /env win64 /protocol dce /app_config midiproc.idl
EXIT /B

REM This command should be similar to what was initially used to generate the old midiproc
REM midl -Oicf -W1 -Zp8 /env win32 /protocol dce /app_config midiproc.idl
