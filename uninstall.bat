@echo off
setlocal enabledelayedexpansion

rem Usage: uninstall.bat ["<path to ...\Base\Binaries\Win64Steam>"]
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
  echo Pass the folder explicitly.
  exit /b 1
)

:have
if not exist "%GAME%\CivilizationVI_DX12.exe" (
  echo CivilizationVI_DX12.exe not found in:
  echo   %GAME%
  exit /b 1
)

rem Only remove our own install: version_orig.dll present is the marker.
if not exist "%GAME%\version_orig.dll" (
  echo No version_orig.dll here, so this proxy was not installed by us.
  echo Leaving version.dll alone.
  exit /b 1
)

if exist "%GAME%\version.dll"        del /q "%GAME%\version.dll"
if exist "%GAME%\version_orig.dll"   del /q "%GAME%\version_orig.dll"
if exist "%GAME%\LargeMapFix.log"    del /q "%GAME%\LargeMapFix.log"
if exist "%GAME%\CivFixWiden.txt"    del /q "%GAME%\CivFixWiden.txt"
if exist "%GAME%\CivFixIndex13.txt"  del /q "%GAME%\CivFixIndex13.txt"

echo Removed from:
echo   %GAME%
echo The game is back to stock -- these were memory patches only, so nothing in
echo your saves or settings was ever changed.
