@echo off
setlocal
call "%~dp0webrtc-env.cmd"
if errorlevel 1 exit /b 1

if not exist "%EASYDESK_REPO_ROOT%\.toolchain" mkdir "%EASYDESK_REPO_ROOT%\.toolchain"
if not exist "%EASYDESK_DEPOT_TOOLS%\.git" (
  git clone --filter=blob:none --no-checkout https://chromium.googlesource.com/chromium/tools/depot_tools.git "%EASYDESK_DEPOT_TOOLS%"
  if errorlevel 1 exit /b 1
)

pushd "%EASYDESK_DEPOT_TOOLS%"
git fetch --depth 1 origin %EASYDESK_DEPOT_TOOLS_REVISION%
if errorlevel 1 goto :failed
git checkout --detach %EASYDESK_DEPOT_TOOLS_REVISION%
if errorlevel 1 goto :failed
popd

if not exist "%EASYDESK_WEBRTC_ROOT%" mkdir "%EASYDESK_WEBRTC_ROOT%"
pushd "%EASYDESK_WEBRTC_ROOT%"
if not exist ".gclient" (
  call gclient config --name src https://webrtc.googlesource.com/src.git
  if errorlevel 1 goto :failed
)
call gclient sync --no-history --revision src@%EASYDESK_WEBRTC_REVISION%
if errorlevel 1 goto :failed

pushd "%EASYDESK_WEBRTC_SRC%"
for /f %%R in ('git rev-parse HEAD') do set "EASYDESK_ACTUAL_WEBRTC_REVISION=%%R"
popd
if /i not "%EASYDESK_ACTUAL_WEBRTC_REVISION%"=="%EASYDESK_WEBRTC_REVISION%" (
  echo Expected WebRTC %EASYDESK_WEBRTC_REVISION% but found %EASYDESK_ACTUAL_WEBRTC_REVISION%.
  goto :failed
)
popd
echo WebRTC source synchronized at %EASYDESK_WEBRTC_REVISION%.
exit /b 0

:failed
popd
exit /b 1
