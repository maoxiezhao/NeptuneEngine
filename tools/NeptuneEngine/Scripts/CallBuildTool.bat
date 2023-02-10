@echo off

..\Neptune.Build\bin\Debug\net6.0\Neptune.Build.exe %*
if errorlevel 1 goto Error_BuildFailed
exit /B 0

:Error_BuildFailed
echo CallBuildTool ERROR: Neptune.Build tool failed.
goto Exit
:Exit
exit /B 1
