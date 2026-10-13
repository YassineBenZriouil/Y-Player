@echo off
rem Builds Y-Player.exe, then runs it. Any arguments (files, playlists) are passed to the player.
call "%~dp0build.cmd" || exit /b 1
call "%~dp0run.cmd" %*
