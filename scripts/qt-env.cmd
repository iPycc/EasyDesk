@echo off
set "EASYDESK_ROOT=%~dp0.."
set "PATH=%EASYDESK_ROOT%\.toolchain\qt-5.15.12\bin;%EASYDESK_ROOT%\.toolchain\jom;C:\Program Files\Git\usr\bin;%PATH%"
if defined VSCMD_VER exit /b 0
set "EASYDESK_VS="
"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -version "[17.0,18.0)" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%EASYDESK_ROOT%\.toolchain\vs-path.txt"
for /f "usebackq delims=" %%i in ("%EASYDESK_ROOT%\.toolchain\vs-path.txt") do set "EASYDESK_VS=%%i"
if not defined EASYDESK_VS (
 echo Visual Studio 2022 C++ tools not found.
 exit /b 1
)
call "%EASYDESK_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
exit /b %errorlevel%
