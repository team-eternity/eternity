@ECHO OFF
if exist ""%~dp0amd64"" GOTO nothingtodo
if exist ""%~dp0win32"" del ""%~dp0win32""
copy NUL ""%~dp0amd64""
ECHO Building amd64 midiproc
midl -Oicf -W1 -Zp8 /env amd64 /protocol dce /app_config /out ""%~dp0\"" ""%~dp0midiproc.idl""
EXIT /B
:nothingtodo
ECHO midiproc already built for this arch

REM This command should be similar to what was initially used to generate the old midiproc
REM midl -Oicf -W1 -Zp8 /env win32 /protocol dce /app_config midiproc.idl
