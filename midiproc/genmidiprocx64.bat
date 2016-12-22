@ECHO OFF
CALL "%VS140COMNTOOLS%\VsDevCmd.bat"
ECHO Building amd64
midl -Oicf -W1 -Zp8 /env amd64 /protocol dce /app_config midiproc.idl
EXIT /B

REM This command should be similar to what was initially used to generate the old midiproc
REM midl -Oicf -W1 -Zp8 /env win32 /protocol dce /app_config midiproc.idl
