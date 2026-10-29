@echo off
setlocal
call "%~dp0qt-env.cmd"
if errorlevel 1 exit /b 1
cd /d "%~dp0.."
if not exist build mkdir build
cd build
qmake ..\EasyDesk.pro
if errorlevel 1 exit /b 1
jom -j 12
exit /b %errorlevel%
