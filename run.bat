@echo off
setlocal
cd /d "%~dp0"
title Stockroom - Store Management

where g++ >nul 2>nul
if errorlevel 1 (
  echo [ERROR] g++ was not found. Install MinGW-w64 ^(for example MSYS2 or WinLibs^) and make sure g++ is on your PATH.
  pause
  exit /b 1
)

if not exist build mkdir build
echo Compiling the C++ backend...
g++ -std=c++17 -O2 -pthread -o build\store_server.exe backend\main.cpp -lws2_32
if errorlevel 1 (
  echo.
  echo [ERROR] Compilation failed. See the messages above.
  pause
  exit /b 1
)

echo Starting server... your browser will open automatically.
echo Close this window ^(or press Ctrl+C^) to stop the server.
echo.
build\store_server.exe --port 8080 --open
echo.
echo Server stopped.
pause
