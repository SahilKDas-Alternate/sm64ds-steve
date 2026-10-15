@echo off
setlocal
cd /d "%~dp0..\.."
set "STEVE_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%STEVE_VSWHERE%" exit /b 2
rem The v0.9.2 release integration is built here with the VS 2022 x86 compiler.
if not exist build\steve mkdir build\steve
"%STEVE_VSWHERE%" -latest -version "[17.0,18.0)" -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > build\steve\vs2022.txt
set "STEVE_VS="
for /f "usebackq delims=" %%i in ("build\steve\vs2022.txt") do set "STEVE_VS=%%i"
if not defined STEVE_VS exit /b 2
call "%STEVE_VS%\VC\Auxiliary\Build\vcvars32.bat"
if errorlevel 1 exit /b 2
set "STEVE_CMAKE=%STEVE_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake"
if not exist "%STEVE_CMAKE%\CMake\bin\cmake.exe" exit /b 2
python port\steve\check_budget.py
if errorlevel 1 exit /b 3
"%STEVE_CMAKE%\CMake\bin\cmake.exe" -S port -B build\port -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DCMAKE_MAKE_PROGRAM="%STEVE_CMAKE%\Ninja\ninja.exe"
if errorlevel 1 exit /b 1
rem Build one executable, keeping smoke and hires output out of the workspace.
"%STEVE_CMAKE%\Ninja\ninja.exe" -C build\port -j 4 walk_window
if errorlevel 1 exit /b 1
python port\steve\check_budget.py
exit /b %errorlevel%
