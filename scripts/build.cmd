@echo off
setlocal
call "%~dp0qt-env.cmd"
if errorlevel 1 exit /b 1
cd /d "%~dp0.."
if not exist ".toolchain\webrtc\src\out\easydesk\easydesk_media.dll" (
 echo WebRTC media runtime is missing. Run scripts\dependencies\build-webrtc.cmd first.
 exit /b 1
)
if not exist build mkdir build
cd build
qmake ..\EasyDesk.pro
if errorlevel 1 exit /b 1
jom -j 12
if errorlevel 1 exit /b 1
copy /y "..\.toolchain\webrtc\src\out\easydesk\easydesk_media.dll" "bin\easydesk_media.dll" >nul
exit /b %errorlevel%
