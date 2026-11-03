@echo off

echo Updating project...
git pull
if errorlevel 1 exit /b 1

echo Building project...
cmake -S . -B build -G "MinGW Makefiles"
if errorlevel 1 exit /b 1

cmake --build build
if errorlevel 1 exit /b 1

echo Running program...
build\HelloWorld.exe

pause