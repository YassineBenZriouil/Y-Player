@echo off
rem Builds Y-Player.exe (Release) without running the tests.
rem Windows locks a running exe, so the build cannot replace it while the player is open.
tasklist /FI "IMAGENAME eq Y-Player.exe" /NH | "%SystemRoot%\System32\find.exe" /i "Y-Player.exe" >nul
if errorlevel 1 goto build
echo Y-Player is running. Close it first, then run "build" again. 1>&2
exit /b 1
:build
call "%~dp0scripts\build.cmd" release exe
