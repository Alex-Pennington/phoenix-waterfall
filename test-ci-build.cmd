@echo off
setlocal

set "MINGW=C:\Users\rayve\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
set "CMAKE=%MINGW%\cmake.exe"

cd /d D:\claude_sandbox\phoenix-waterfall

echo === Phoenix Waterfall - CI Build Test ===
echo.
echo Using cmake: %CMAKE%
echo.

REM Step 1: Clean
echo [1/4] Cleaning build directory...
if exist "build" rmdir /s /q "build"

REM Step 2: Configure
echo [2/4] Configuring...
"%CMAKE%" --preset msys2-ucrt64
if errorlevel 1 goto :fail

REM Step 3: Build
echo [3/4] Building...
"%CMAKE%" --build --preset msys2-ucrt64
if errorlevel 1 goto :fail

REM Step 4: Package
echo [4/4] Packaging...
if not exist "package" mkdir package
copy /Y "build\msys2-ucrt64\waterfall.exe" "package\" >nul
copy /Y "build\msys2-ucrt64\*.dll" "package\" >nul 2>&1
copy /Y "README.md" "package\" >nul
copy /Y "LICENSE" "package\" >nul

echo.
echo === BUILD SUCCESSFUL ===
echo.
dir package\*
goto :end

:fail
echo.
echo === BUILD FAILED ===

:end
