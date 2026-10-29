@echo off
setlocal
call "%~dp0webrtc-env.cmd"
if errorlevel 1 exit /b 1

if not exist "%EASYDESK_WEBRTC_SRC%\.git" (
  echo WebRTC source is missing. Run scripts\dependencies\fetch-webrtc.cmd first.
  exit /b 1
)
if not exist "%EASYDESK_DEPOT_TOOLS%\python3_bin_reldir.txt" (
  echo depot_tools is not initialized. Run scripts\dependencies\fetch-webrtc.cmd first.
  exit /b 1
)
if not exist "%EASYDESK_WINDOWS_SDK%\App Certification Kit\DbgHelp.dll" (
  echo Microsoft-signed DbgHelp.dll was not found in the Windows SDK App Certification Kit.
  exit /b 1
)

if not exist "%EASYDESK_DEBUGGER_SHIM%\Debuggers\x64" mkdir "%EASYDESK_DEBUGGER_SHIM%\Debuggers\x64"
copy /y "%EASYDESK_WINDOWS_SDK%\App Certification Kit\DbgHelp.dll" "%EASYDESK_DEBUGGER_SHIM%\Debuggers\x64\dbghelp.dll" >nul
if errorlevel 1 exit /b 1
for %%D in (Include Lib bin UnionMetadata References) do (
  if not exist "%EASYDESK_DEBUGGER_SHIM%\%%D" (
    mklink /J "%EASYDESK_DEBUGGER_SHIM%\%%D" "%EASYDESK_WINDOWS_SDK%\%%D" >nul
    if errorlevel 1 exit /b 1
  )
)
set "WINDOWSSDKDIR=%EASYDESK_DEBUGGER_SHIM%"

set "EASYDESK_MEDIA_BRIDGE_SOURCE=%EASYDESK_REPO_ROOT%\src\media\bridge"
set "EASYDESK_MEDIA_BRIDGE_TARGET=%EASYDESK_WEBRTC_SRC%\easydesk_media_bridge"
set "EASYDESK_MEDIA_ROOT_PATCH=%EASYDESK_REPO_ROOT%\dependencies\webrtc\easydesk-media-root.patch"
echo Staging EasyDesk media bridge sources...
if not exist "%EASYDESK_MEDIA_BRIDGE_TARGET%" mkdir "%EASYDESK_MEDIA_BRIDGE_TARGET%"
xcopy /d /e /i /y "%EASYDESK_MEDIA_BRIDGE_SOURCE%\*" "%EASYDESK_MEDIA_BRIDGE_TARGET%\" >nul
if errorlevel 2 (
  echo Failed to stage EasyDesk media bridge sources.
  exit /b 1
)

pushd "%EASYDESK_WEBRTC_SRC%"
for /f %%R in ('git rev-parse HEAD') do set "EASYDESK_ACTUAL_WEBRTC_REVISION=%%R"
if /i not "%EASYDESK_ACTUAL_WEBRTC_REVISION%"=="%EASYDESK_WEBRTC_REVISION%" (
  echo Expected WebRTC %EASYDESK_WEBRTC_REVISION% but found %EASYDESK_ACTUAL_WEBRTC_REVISION%.
  popd
  exit /b 1
)

if not exist "out\easydesk" mkdir "out\easydesk"
copy /y "%EASYDESK_REPO_ROOT%\dependencies\webrtc\windows-release.gn" "out\easydesk\args.gn" >nul
if errorlevel 1 goto :failed
echo Applying the EasyDesk media GN overlay...
call git apply --check "%EASYDESK_MEDIA_ROOT_PATCH%"
if errorlevel 1 goto :failed
call git apply "%EASYDESK_MEDIA_ROOT_PATCH%"
if errorlevel 1 goto :failed
echo Generating the WebRTC and EasyDesk media build graph...
call gn gen out\easydesk
set "EASYDESK_MEDIA_BUILD_RESULT=%ERRORLEVEL%"
if not "%EASYDESK_MEDIA_BUILD_RESULT%"=="0" goto :restore_media_overlay
call autoninja -C out\easydesk default
set "EASYDESK_MEDIA_BUILD_RESULT=%ERRORLEVEL%"
:restore_media_overlay
call git apply --reverse "%EASYDESK_MEDIA_ROOT_PATCH%"
if errorlevel 1 goto :failed
if not "%EASYDESK_MEDIA_BUILD_RESULT%"=="0" goto :failed
if not exist "out\easydesk\obj\webrtc.lib" (
  echo WebRTC build completed without the expected webrtc.lib.
  goto :failed
)
if not exist "out\easydesk\easydesk_media.dll" (
  echo WebRTC build completed without the expected easydesk_media.dll.
  goto :failed
)
popd
if exist "%EASYDESK_MEDIA_BRIDGE_TARGET%" rmdir /s /q "%EASYDESK_MEDIA_BRIDGE_TARGET%"
echo WebRTC library and EasyDesk media bridge are ready.
exit /b 0

:failed
call git apply --reverse --check "%EASYDESK_MEDIA_ROOT_PATCH%" >nul 2>&1
if errorlevel 1 goto :failed_cleanup
call git apply --reverse "%EASYDESK_MEDIA_ROOT_PATCH%"
:failed_cleanup
popd
if exist "%EASYDESK_MEDIA_BRIDGE_TARGET%" rmdir /s /q "%EASYDESK_MEDIA_BRIDGE_TARGET%"
exit /b 1
