@echo off
setlocal
rem Run from any working directory; write only to this repo's ignored build/.
cd /d "%~dp0..\.."
set "STEVE_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%STEVE_VSWHERE%" exit /b 2
for /f "usebackq delims=" %%i in (`"%STEVE_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "STEVE_VS=%%i"
if not defined STEVE_VS exit /b 2
call "%STEVE_VS%\VC\Auxiliary\Build\vcvars32.bat"
if errorlevel 1 exit /b 2
if not exist build\steve mkdir build\steve
python port\steve\check_budget.py
if errorlevel 1 exit /b 3
cl /nologo /EHsc /std:c++17 /W4 /O2 port\steve\core.cpp port\steve\core_test.cpp /Fobuild\steve\ /Febuild\steve\steve_core_test.exe
if errorlevel 1 exit /b 1
build\steve\steve_core_test.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /W4 /O2 port\steve\assets.cpp port\steve\assets_test.cpp /Fobuild\steve\ /Febuild\steve\steve_assets_test.exe
if errorlevel 1 exit /b 1
build\steve\steve_assets_test.exe
if errorlevel 1 exit /b 1
python port\steve\check_budget.py
exit /b %errorlevel%
