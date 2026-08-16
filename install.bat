@echo off
setlocal enabledelayedexpansion

rem Usage: install.bat ["<path to ...\Base\Binaries\Win64Steam>"]
rem With no argument, the usual Steam library locations are searched on every drive.

set "GAME=%~1"
set "SUB=steamapps\common\Sid Meier's Civilization VI\Base\Binaries\Win64Steam"

if not "%GAME%"=="" goto :have

for %%d in (C D E F G H I J K L M N O P Q R S T U V W X Y Z) do (
  for %%r in ("%%d:\SteamLibrary" "%%d:\Steam" "%%d:\Games\Steam" ^
              "%%d:\Program Files (x86)\Steam" "%%d:\Program Files\Steam") do (
    if exist "%%~r\%SUB%\CivilizationVI_DX12.exe" (
      if "!GAME!"=="" set "GAME=%%~r\%SUB%"
    )
  )
)

if "%GAME%"=="" (
  echo Could not find CivilizationVI_DX12.exe in any Steam library.
  echo Pass the folder explicitly:
  echo   install.bat "X:\...\Sid Meier's Civilization VI\Base\Binaries\Win64Steam"
  exit /b 1
)
echo Found the game at:
echo   %GAME%
echo.

:have
if not exist "%GAME%\CivilizationVI_DX12.exe" (
  echo CivilizationVI_DX12.exe not found in:
  echo   %GAME%
  echo Pass the correct folder: install.bat "X:\...\Base\Binaries\Win64Steam"
  exit /b 1
)
if not exist "%~dp0build\version.dll" (
  echo build\version.dll not found. Run build.bat first.
  exit /b 1
)

rem Refuse to clobber someone else's proxy. If version_orig.dll is already there this is
rem our own install being refreshed, which is fine.
if exist "%GAME%\version.dll" if not exist "%GAME%\version_orig.dll" (
  echo.
  echo REFUSING TO INSTALL: a version.dll is already present but version_orig.dll is not.
  echo That file belongs to something else -- another proxy mod, ReShade, or similar.
  echo Overwriting it would break that tool. Remove or rename it first.
  exit /b 1
)

if not exist "%GAME%\version_orig.dll" (
  echo Copying the real version.dll -^> version_orig.dll ...
  copy /y "%SystemRoot%\System32\version.dll" "%GAME%\version_orig.dll" >nul
  if errorlevel 1 (
    echo Failed to copy %SystemRoot%\System32\version.dll
    exit /b 1
  )
)

echo Installing the proxy ...
copy /y "%~dp0build\version.dll" "%GAME%\version.dll" >nul
if errorlevel 1 (
  echo Failed to copy version.dll. Is the game running?
  exit /b 1
)

rem The handle widening is off unless CivFixWiden.txt asks for it. Write mode 2 so a
rem fresh install is actually fixed; put 1 in it for the validation run, or 0 for off.
if not exist "%GAME%\CivFixWiden.txt" (
  rem The parens are required: `echo 2>file` would parse the 2 as a stderr redirect.
  (echo 2)>"%GAME%\CivFixWiden.txt"
  echo Wrote CivFixWiden.txt = 2 ^(handle limit 65534^).
)

echo.
echo Installed to:
echo   %GAME%
echo.
echo Start the game and choose DIRECTX 12 at the launcher. The DX11 executable is a
echo different binary, so every patch site fails verification there and nothing applies.
echo.
echo Then check LargeMapFix.log in that folder for:
echo   [widen] mode 2: relocated the resource list ... handle limit = 65535
echo   [capfix] index13: 6 of 6 mask sites widened
echo.
echo Run uninstall.bat to remove.
