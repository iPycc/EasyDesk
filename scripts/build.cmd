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
for %%f in (Qt5Core.dll Qt5Gui.dll Qt5Widgets.dll Qt5Svg.dll) do (
 copy /y "..\.toolchain\qt-5.15.12\bin\%%f" "bin\%%f" >nul
 if errorlevel 1 exit /b 1
)
for %%d in (platforms styles imageformats) do if not exist "bin\%%d" mkdir "bin\%%d"
copy /y "..\.toolchain\qt-5.15.12\plugins\platforms\qwindows.dll" "bin\platforms\qwindows.dll" >nul
if errorlevel 1 exit /b 1
copy /y "..\.toolchain\qt-5.15.12\plugins\styles\qwindowsvistastyle.dll" "bin\styles\qwindowsvistastyle.dll" >nul
if errorlevel 1 exit /b 1
copy /y "..\.toolchain\qt-5.15.12\plugins\imageformats\qsvg.dll" "bin\imageformats\qsvg.dll" >nul
if errorlevel 1 exit /b 1
copy /y "..\packaging\windows\qt.conf" "bin\qt.conf" >nul
exit /b %errorlevel%
