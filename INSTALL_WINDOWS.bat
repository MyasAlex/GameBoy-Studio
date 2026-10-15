@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0INSTALL_WINDOWS.ps1" -DesktopShortcut -RegisterOpenWith
if errorlevel 1 (echo Installation failed. Close the emulator and check permissions.)
pause

