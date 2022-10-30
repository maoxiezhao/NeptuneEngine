@echo off

setlocal
pushd
echo Generating editor project files...
cd editor
call "..\build" win32 -all