@echo off
rem Runs the already-built Y-Player.exe. Any arguments (files, playlists) are passed through.
set "EXE=%~dp0build\release\bin\Y-Player.exe"
if exist "%EXE%" goto run
echo Y-Player is not built yet. Run "build" first. 1>&2
exit /b 1
:run
start "" "%EXE%" %*
