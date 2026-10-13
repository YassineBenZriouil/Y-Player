@echo off
rem One-command build: fetch deps, configure, build, run tests.
rem   scripts\build.cmd [debug|release|relwithdebinfo|ci] [run] [exe]
rem "ci" = Release + clang-tidy (warnings as errors). "run" launches the app after a green build.
rem "exe" builds only Y-Player.exe and skips the tests (used by the root build.cmd).
setlocal EnableExtensions
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "PRESET=debug"
set "RUN="
set "EXEONLY="

:args
if "%~1"=="" goto argsdone
if /i "%~1"=="run" (set "RUN=1") else if /i "%~1"=="exe" (set "EXEONLY=1") else (set "PRESET=%~1")
shift
goto args
:argsdone

where cl >nul 2>nul && goto havemsvc
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto nomsvc
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find VC\Auxiliary\Build\vcvars64.bat`) do set "VCVARS=%%i"
if not defined VCVARS goto nomsvc
set "PATH=%PATH%;%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
call "%VCVARS%" >nul || goto fail
:havemsvc

python "%ROOT%\scripts\fetch_deps.py" || goto fail
cd /d "%ROOT%" || goto fail
cmake --preset %PRESET% || goto fail
if defined EXEONLY goto exeonly
cmake --build --preset %PRESET% || goto fail
ctest --preset %PRESET% || goto fail
goto built
:exeonly
cmake --build --preset %PRESET% --target y-player || goto fail
:built

echo.
echo Build OK: %ROOT%\build\%PRESET%\bin\Y-Player.exe
if defined RUN start "" "%ROOT%\build\%PRESET%\bin\Y-Player.exe"
exit /b 0

:nomsvc
echo error: MSVC not found. Install Visual Studio Build Tools with the C++ workload (see README). 1>&2
exit /b 1

:fail
echo error: build failed (preset %PRESET%). 1>&2
exit /b 1
