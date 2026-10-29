@echo off
setlocal
call "%~dp0qt-env.cmd"
if errorlevel 1 exit /b 1
if not exist "%EASYDESK_ROOT%\.toolchain\svg-build" mkdir "%EASYDESK_ROOT%\.toolchain\svg-build"
cd /d "%EASYDESK_ROOT%\.toolchain\svg-build"
qmake ..\src\qtsvg-everywhere-src-5.15.12\qtsvg.pro "QT_BUILD_PARTS=libs"
if errorlevel 1 exit /b 1
jom -j 12
if errorlevel 1 exit /b 1
jom install
exit /b %errorlevel%
