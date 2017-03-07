@ECHO OFF
CALL "%VS140COMNTOOLS%\VsDevCmd.bat"
ECHO Building win32
midl -Oicf -W1 -Zp8 /env win32 /protocol dce /app_config /out ""%~dp0\"" ""%~dp0midiproc.idl""
EXIT /B

REM This command should be similar to what was initially used to generate the old midiproc
REM midl -Oicf -W1 -Zp8 /env win32 /protocol dce /app_config midiproc.idl
