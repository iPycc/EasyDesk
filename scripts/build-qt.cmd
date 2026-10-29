@echo off
setlocal
call "%~dp0qt-env.cmd"
if errorlevel 1 exit /b 1
cd /d "%~dp0..\.toolchain\qt-build"
if not exist Makefile (
 call "..\src\qtbase-everywhere-src-5.15.12\configure.bat" -prefix "%EASYDESK_ROOT%\.toolchain\qt-5.15.12" -release -opensource -confirm-license -nomake examples -nomake tests -no-opengl -no-sql-sqlite -no-sql-odbc -no-icu -no-feature-network -no-feature-sql -mp
 if errorlevel 1 exit /b 1
)
jom -j 12
if errorlevel 1 exit /b 1
jom install
exit /b %errorlevel%
