@ECHO OFF
REM Perform case-insensitive string comparison
IF /i "%1" == "win32" (
SET arch=win32
SET todelete=amd64
) ELSE IF /i "%1" == "x64" (
SET arch=amd64
SET todelete=win32
) ELSE (
ECHO Invalid arch "%1"
EXIT /B
)

REM If we don't need to bother, don't build
IF EXIST "%~dp0%arch%" GOTO nothingtodo
REM If midl isn't present, don't try building
WHERE midl >nul 2>nul
IF %errorlevel% == 1 GOTO :notfound

REM Remove and add tracker files as necessary
IF EXIST "%~dp0%todelete%" DEL "%~dp0%todelete%"
COPY NUL ""%~dp0%arch%""
REM Execute MIDL compilation!
ECHO Building %arch% midiproc
midl -Oicf -W1 -Zp8 /env %arch% /protocol dce /app_config /out ""%~dp0\"" ""%~dp0midiproc.idl""
EXIT /B

:nothingtodo
ECHO midiproc already built for this arch
EXIT /B

:notfound
ECHO MIDL not found
EXIT /B

REM This command should be similar to what was initially used to generate the old midiproc
REM midl -Oicf -W1 -Zp8 /env win32 /protocol dce /app_config midiproc.idl
