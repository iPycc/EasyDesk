@echo off
set "EASYDESK_REPO_ROOT=%~dp0..\.."
for %%I in ("%EASYDESK_REPO_ROOT%") do set "EASYDESK_REPO_ROOT=%%~fI"
set "EASYDESK_WEBRTC_ROOT=%EASYDESK_REPO_ROOT%\.toolchain\webrtc"
set "EASYDESK_WEBRTC_SRC=%EASYDESK_WEBRTC_ROOT%\src"
set "EASYDESK_DEPOT_TOOLS=%EASYDESK_REPO_ROOT%\.toolchain\depot_tools"
set "EASYDESK_WEBRTC_REVISION=36ea4535a500ac137dbf1f577ce40dc1aaa774ef"
set "EASYDESK_DEPOT_TOOLS_REVISION=7083aadfb53f3de2e20b72058c7f510da8f8218a"
set "DEPOT_TOOLS_UPDATE=0"
set "DEPOT_TOOLS_WIN_TOOLCHAIN=0"
set "vs2022_install=C:\Program Files\Microsoft Visual Studio\2022\Community"
set "PATH=%EASYDESK_DEPOT_TOOLS%;%PATH%"

if not exist "%vs2022_install%\VC\Auxiliary\Build\vcvars64.bat" (
  echo Visual Studio 2022 C++ tools not found at %vs2022_install%.
  exit /b 1
)
exit /b 0
