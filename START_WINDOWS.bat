@echo off
cd /d "%~dp0"
if exist "dist\GBStudio.exe" (
  start "" "dist\GBStudio.exe" %*
) else (
  call BUILD_WINDOWS.bat
)
