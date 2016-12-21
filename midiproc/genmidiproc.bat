@ECHO OFF
ECHO THIS MUST BE RUN FROM THE DEVELOPER COMMAND PROMPT FOR VS2015
ECHO Choose your environment:
ECHO 1. win32
ECHO 2. ia64/win64
ECHO 3. amd64
SET SELECTION=
SET /P SELECTION=Enter selection: %=%
IF %SELECTION%==1  GOTO win32
IF %SELECTION%==2  GOTO ia64
IF %SELECTION%==3  GOTO amd64

:win32
ECHO Building win32
midl -Oicf -W1 -Zp8 /env win32 /protocol dce /app_config midiproc.idl
EXIT /B
:ia64
ECHO Building ia64/win64
midl -Oicf -W1 -Zp8 /env ia64 /protocol dce /app_config midiproc.idl
EXIT /B
:amd64
ECHO Building amd64
midl -Oicf -W1 -Zp8 /env amd64 /protocol dce /app_config midiproc.idl
EXIT /B

REM This command should be similar to what was initially used to generate the old midiproc
REM midl -Oicf -W1 -Zp8 /env win32 /protocol dce /app_config midiproc.idl
