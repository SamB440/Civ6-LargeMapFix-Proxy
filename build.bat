@echo off
setlocal enabledelayedexpansion

rem Locate the C++ toolchain. vswhere is the supported way and handles any edition or
rem install path; the hardcoded fallbacks below only run if it is missing or finds nothing.
rem
rem vswhere's path contains "(x86)", and a literal ')' closes any parenthesised cmd
rem construct it is expanded inside -- an `if (...)` body or a `for /f in (...)` clause
rem alike. Rather than fight the quoting, capture the output through a temp file: plain
rem redirection has no such trap, and the failure mode is a readable empty result.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSOUT=%TEMP%\civfix_vswhere.txt"
set "VCVARS="
set "VSROOT="
if not exist "%VSWHERE%" goto :novswhere
"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath >"%VSOUT%" 2>nul
if exist "%VSOUT%" set /p VSROOT=<"%VSOUT%"
if exist "%VSOUT%" del /q "%VSOUT%"
if defined VSROOT set "VCVARS=!VSROOT!\VC\Auxiliary\Build\vcvars64.bat"
:novswhere
if defined VCVARS if not exist "!VCVARS!" set "VCVARS="
if not defined VCVARS (
  for %%e in (Community Professional Enterprise BuildTools) do (
    if not defined VCVARS if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%e\VC\Auxiliary\Build\vcvars64.bat" (
      set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\%%e\VC\Auxiliary\Build\vcvars64.bat"
    )
  )
)
if not defined VCVARS (
  echo Could not find vcvars64.bat.
  echo Install Visual Studio 2022 with the "Desktop development with C++" workload,
  echo or run this from a "x64 Native Tools Command Prompt".
  exit /b 1
)

echo Using !VCVARS!
call "!VCVARS!" >nul
if errorlevel 1 exit /b 1

cd /d "%~dp0"
if not exist build mkdir build

echo Building version.dll ...
cl /nologo /O2 /MT /EHsc /std:c++17 /W3 /LD ^
   src\proxy.cpp src\LargeMapFix.cpp src\Widen.cpp ^
   /Fe:build\version.dll /Fo:build\ /Fd:build\ ^
   /link /INCREMENTAL:NO /OPT:REF
if errorlevel 1 (
  echo BUILD FAILED
  exit /b 1
)

del /q build\*.obj build\*.exp build\*.lib build\*.pdb 2>nul

rem Verify the export surface. The game resolves version.dll's exports by name AND by
rem ordinal, and every one must be a true forwarder to version_orig -- a dropped or
rem non-forwarded export is a silent failure that only shows up as the game refusing to
rem start. A mistyped /export pragma produces exactly that, so it is checked here rather
rem than assumed.
rem Counted with a for/f loop rather than `find /c`: `find` is shadowed by Git Bash's
rem Unix find on many developer PATHs, where `find /c /v ""` becomes a recursive scan
rem of C:\ that never returns. findstr has no such collision.
dumpbin /nologo /exports build\version.dll >build\exports.txt 2>nul
if errorlevel 1 goto :nodumpbin
set /a FWD=0
set /a TOTAL=0
for /f "delims=" %%l in ('findstr /c:"forwarded to version_orig." build\exports.txt') do set /a FWD+=1
for /f "delims=" %%l in ('findstr /r /c:"forwarded to " build\exports.txt') do set /a TOTAL+=1
del /q build\exports.txt 2>nul
if not "!FWD!"=="17" goto :badexports
if not "!TOTAL!"=="17" goto :badexports
echo Exports: 17/17 present, all forwarded to version_orig.
goto :ok

:badexports
echo.
echo EXPORT CHECK FAILED: !TOTAL! exports, !FWD! forwarded to version_orig -- expected 17
echo and 17. The linker /export pragmas in src\proxy.cpp did not all take effect; this
echo DLL would break the game rather than proxy it. Not shipping it.
del /q build\version.dll 2>nul
exit /b 1

:nodumpbin
echo Warning: dumpbin unavailable, skipping the export check.

:ok
echo.
echo Done: build\version.dll
echo Run install.bat to copy it into the game folder.
