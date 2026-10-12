@echo off
setlocal
cd /d "%~dp0"
where cl >nul 2>&1
if not errorlevel 1 goto build
set "GB_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%GB_VSWHERE%" goto missing
for /f "usebackq tokens=*" %%i in (`"%GB_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "GB_VS_ROOT=%%i"
if not defined GB_VS_ROOT goto missing
call "%GB_VS_ROOT%\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64
if errorlevel 1 goto failed
:build
if not exist build mkdir build
if not exist dist mkdir dist
cl /nologo /std:c++20 /O2 /EHsc /utf-8 /MT /W3 /Fo:build\core_tests.obj /Fe:build\gb_core_tests.exe tests\core_tests.cpp
if errorlevel 1 goto failed
build\gb_core_tests.exe demo.gb
if errorlevel 1 goto failed
cl /nologo /std:c++20 /O2 /EHsc /utf-8 /MT /W3 /DUNICODE /D_UNICODE /Fo:build\windows_main.obj /Fe:dist\GBStudio.exe src\windows_main.cpp /link /SUBSYSTEM:WINDOWS
if errorlevel 1 goto failed
copy /y demo.gb dist\demo.gb >nul
copy /y README_RU.md dist\README_RU.md >nul
copy /y COMPATIBILITY.md dist\COMPATIBILITY.md >nul
echo SUCCESS: dist\GBStudio.exe
start "" "dist\GBStudio.exe"
pause
exit /b 0
:missing
echo Install Visual Studio 2022 or 2026 with Desktop development with C++.
pause
exit /b 1
:failed
echo Build or tests failed. Read the error above.
pause
exit /b 1
