param([ValidatePattern('^EasyDesk(?:-[a-zA-Z0-9-]+)?$')][string]$PackageName = 'EasyDesk')
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$qtRoot = Join-Path $projectRoot '.toolchain\qt-5.15.12'
$targetRoot = Join-Path $projectRoot ('dist\' + $PackageName)
New-Item -ItemType Directory -Force -Path $targetRoot, "$targetRoot\platforms", "$targetRoot\styles", "$targetRoot\licenses" | Out-Null
Copy-Item -LiteralPath "$projectRoot\build\bin\EasyDesk.exe" -Destination $targetRoot
foreach ($fileName in @('Qt5Core.dll','Qt5Gui.dll','Qt5Widgets.dll','Qt5Svg.dll')) {
    Copy-Item -LiteralPath "$qtRoot\bin\$fileName" -Destination $targetRoot
}
Copy-Item -LiteralPath "$qtRoot\plugins\platforms\qwindows.dll" -Destination "$targetRoot\platforms"
Copy-Item -LiteralPath "$qtRoot\plugins\styles\qwindowsvistastyle.dll" -Destination "$targetRoot\styles"
New-Item -ItemType Directory -Force -Path "$targetRoot\imageformats" | Out-Null
Copy-Item -LiteralPath "$qtRoot\plugins\imageformats\qsvg.dll" -Destination "$targetRoot\imageformats"
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsRoot = & $vswhere -version '[17.0,18.0)' -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$redist = Get-ChildItem -LiteralPath "$vsRoot\VC\Redist\MSVC" -Directory | Where-Object Name -Match '^14\.' | Sort-Object Name -Descending | Select-Object -First 1
if (-not $redist) { throw 'MSVC app-local runtime not found' }
Get-ChildItem -LiteralPath "$($redist.FullName)\x64\Microsoft.VC143.CRT" -Filter '*.dll' | Copy-Item -Destination $targetRoot
$oldFontLicense = Join-Path $targetRoot 'licenses\NotoSansSC-OFL.txt'
if (Test-Path -LiteralPath $oldFontLicense) { Remove-Item -LiteralPath $oldFontLicense }
foreach ($licenseName in @('LICENSE.LGPL3','LICENSE.GPL3','LICENSE.FDL')) {
    Copy-Item -LiteralPath "$projectRoot\.toolchain\src\qtbase-everywhere-src-5.15.12\$licenseName" -Destination "$targetRoot\licenses"
}
Copy-Item -LiteralPath "$projectRoot\docs\THIRD_PARTY_NOTICES.md" -Destination "$targetRoot\licenses"
$thirdPartyRoot = Join-Path $projectRoot '.toolchain\src\qtbase-everywhere-src-5.15.12\src\3rdparty'
Get-ChildItem -LiteralPath $thirdPartyRoot -Recurse -File | Where-Object { $_.Name -match '^(LICENSE|LICENCE|COPYING|COPYRIGHT)|^qt_attribution\.json$' } | ForEach-Object {
    $relativeName = $_.FullName.Substring($thirdPartyRoot.Length + 1)
    $noticeTarget = Join-Path "$targetRoot\licenses\qt-third-party" $relativeName
    New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($noticeTarget)) | Out-Null
    Copy-Item -LiteralPath $_.FullName -Destination $noticeTarget
}
Copy-Item -LiteralPath "$projectRoot\docs\PORTABLE_README.txt" -Destination "$targetRoot\README.txt"
@'
[Paths]
Plugins=.
'@ | Set-Content -Encoding ascii -LiteralPath "$targetRoot\qt.conf"
$zipPath = Join-Path $projectRoot 'dist\EasyDesk-0.2.0-dev-win64.zip'
Compress-Archive -LiteralPath $targetRoot -DestinationPath $zipPath -Force
Write-Output "Portable application: $targetRoot\EasyDesk.exe"
Write-Output "ZIP: $zipPath"
