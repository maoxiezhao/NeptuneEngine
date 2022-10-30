@echo off

echo 1. Clean...
call "scripts\clean.bat"
echo:

echo 2. Generating VS solution...
scripts\premake5.exe --file=scripts\premake.lua %*
echo: