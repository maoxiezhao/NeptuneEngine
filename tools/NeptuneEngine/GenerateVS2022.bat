@echo off
setlocal
pushd
echo Generating Neptune Engine project files...

call "Scripts\CallBuildTool.bat" -genproject -vs2022
if errorlevel 1 goto BuildToolFailed

popd
echo Done!
pause
exit /B 0

:BuildToolFailed
echo Neptune.Build tool failed.
pause
goto Exit

:Exit
popd
exit /B 1
