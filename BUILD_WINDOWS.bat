@echo off
setlocal
cd /d "%~dp0"
echo ==== GB Studio Beta: compiling for Windows x64 ====
where cmake >nul 2>&1
if errorlevel 1 (
  echo ERROR: CMake not found. Install Visual Studio with Desktop development with C++ + CMake tools.
  pause
  exit /b 1
)
cmake -S . -B build
if errorlevel 1 (
  echo ERROR: CMake configuration failed. Use Visual Studio Developer Command Prompt if needed.
  pause
  exit /b 1
)
cmake --build build --config Release
if errorlevel 1 (
  echo ERROR: Compilation failed.
  pause
  exit /b 1
)
cmake --install build --config Release --prefix dist
if errorlevel 1 ( echo ERROR: Install step failed. & pause & exit /b 1 )
echo SUCCESS! Your emulator is at: dist\GBStudio.exe
if exist "dist\GBStudio.exe" start "" "dist\GBStudio.exe"
pause
