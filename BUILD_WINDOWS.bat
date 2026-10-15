@echo off
setlocal EnableExtensions
set "GB_NO_LAUNCH=0"
if /i "%~1"=="--no-launch" set "GB_NO_LAUNCH=1"
if not "%~1"=="" if /i not "%~1"=="--no-launch" exit /b 2
cd /d "%~dp0"
where cmake >nul 2>&1
if not errorlevel 1 goto build
set "GB_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%GB_VSWHERE%" goto missing
for /f "usebackq tokens=*" %%i in (`"%GB_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "GB_VS_ROOT=%%i"
if not defined GB_VS_ROOT goto missing
call "%GB_VS_ROOT%\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64
if errorlevel 1 goto failed
set "PATH=%GB_VS_ROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
where cmake >nul 2>&1
if errorlevel 1 goto missing
:build
cmake -S . -B build\cmake -A x64 -DBUILD_TESTING=ON
if errorlevel 1 goto failed
cmake --build build\cmake --config Release --parallel 4
if errorlevel 1 goto failed
ctest --test-dir build\cmake -C Release --output-on-failure
if errorlevel 1 goto failed
cmake --install build\cmake --config Release --prefix dist
if errorlevel 1 goto failed
echo SUCCESS: dist\GBStudio.exe
if "%GB_NO_LAUNCH%"=="1" exit /b 0
start "" "dist\GBStudio.exe"
exit /b 0
:missing
echo Install Visual Studio with Desktop development with C++ and CMake tools.
if "%GB_NO_LAUNCH%"=="0" pause
exit /b 1
:failed
echo Build or tests failed. Read the error above.
if "%GB_NO_LAUNCH%"=="0" pause
exit /b 1
