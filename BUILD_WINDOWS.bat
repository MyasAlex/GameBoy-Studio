@echo off
setlocal EnableExtensions
set "GB_NO_LAUNCH=0"
if /i "%~1"=="--no-launch" set "GB_NO_LAUNCH=1"
if not "%~1"=="" if /i not "%~1"=="--no-launch" (
    echo Usage: BUILD_WINDOWS.bat [--no-launch]
    exit /b 2
)
rem PATH entries do not need quotes; embedded quotes break VS environment scripts.
set "PATH=%PATH:"=%"
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
if errorlevel 1 goto failed
if not exist dist mkdir dist
if errorlevel 1 goto failed
cl /nologo /std:c++20 /O2 /EHsc /utf-8 /MT /W3 /Fo:build\headless.obj /Fe:build\gb_headless.exe src\headless.cpp
if errorlevel 1 goto failed
for %%S in (core cgb_memory cgb_ppu cgb_apu cgb_integration core_extension session studio_tools audio_buffer cartridge_tools) do (
    cl /nologo /std:c++20 /O2 /EHsc /utf-8 /MT /W3 /Fo:build\%%S_tests.obj /Fe:build\gb_%%S_tests.exe tests\%%S_tests.cpp
    if errorlevel 1 goto failed
)
build\gb_core_tests.exe demo.gb
if errorlevel 1 goto failed
build\gb_cgb_memory_tests.exe tests\fixtures\dmg-v0.6.state demo.gb
if errorlevel 1 goto failed
build\gb_cgb_ppu_tests.exe
if errorlevel 1 goto failed
build\gb_cgb_apu_tests.exe
if errorlevel 1 goto failed
build\gb_cgb_integration_tests.exe demo.gbc
if errorlevel 1 goto failed
build\gb_core_extension_tests.exe demo.gb
if errorlevel 1 goto failed
build\gb_session_tests.exe demo.gb
if errorlevel 1 goto failed
build\gb_studio_tools_tests.exe
if errorlevel 1 goto failed
build\gb_audio_buffer_tests.exe
if errorlevel 1 goto failed
build\gb_cartridge_tools_tests.exe
if errorlevel 1 goto failed
cl /nologo /std:c++20 /O2 /EHsc /utf-8 /MT /W3 /DUNICODE /D_UNICODE /Fo:build\windows_main.obj /Fe:dist\GBStudio.exe src\windows_main.cpp /link /SUBSYSTEM:WINDOWS
if errorlevel 1 goto failed
for %%F in (demo.gb demo.gbc README.md README_RU.md COMPATIBILITY.md LICENSE) do (
    copy /y "%%F" "dist\%%F" >nul
    if errorlevel 1 goto failed
)
if not exist dist\tools mkdir dist\tools
copy /y build\gb_headless.exe dist\tools\gb_headless.exe >nul
if errorlevel 1 goto failed
if not exist dist\docs mkdir dist\docs
xcopy /y /e /i docs dist\docs >nul
if errorlevel 1 goto failed
echo SUCCESS: dist\GBStudio.exe
if "%GB_NO_LAUNCH%"=="1" exit /b 0
start "" "dist\GBStudio.exe"
pause
exit /b 0
:missing
echo Install Visual Studio 2022 or 2026 with Desktop development with C++.
if "%GB_NO_LAUNCH%"=="0" pause
exit /b 1
:failed
echo Build or tests failed. Read the error above.
if "%GB_NO_LAUNCH%"=="0" pause
exit /b 1
