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
call gn gen out\easydesk
if errorlevel 1 goto :failed
call autoninja -C out\easydesk webrtc
if errorlevel 1 goto :failed
if not exist "out\easydesk\obj\webrtc.lib" (
  echo WebRTC build completed without the expected webrtc.lib.
  goto :failed
)
popd
echo WebRTC library ready at .toolchain\webrtc\src\out\easydesk\obj\webrtc.lib.
exit /b 0

:failed
popd
exit /b 1
