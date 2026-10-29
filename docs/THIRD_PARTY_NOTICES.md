# Third-party components

The frontend is C++17 and dynamically links Qt 5.15.12 Core, Gui, Widgets and Svg.
This package currently provides the native client shell and local state only. Network, media, input, and file transport components are not included yet.

## Qt 5.15.12

Qt source, including bundled third-party notices, is available at:
https://download.qt.io/archive/qt/5.15/5.15.12/submodules/qtbase-everywhere-opensource-src-5.15.12.tar.xz

Source archive MD5: ad57ee306725e420e374cac41371d80f.
The unmodified source and build logs are retained in this project's .toolchain directory.
Build recipe: scripts/build-qt.cmd. Qt DLLs and plugins are dynamically replaceable
in the portable directory. Applicable LGPLv3/GPLv3 texts accompany this package.
Qt bundled dependency notices are copied from the exact source into licenses/qt-third-party.

## Qt Svg 5.15.12

Built from the matching official source module:
https://download.qt.io/archive/qt/5.15/5.15.12/submodules/qtsvg-everywhere-opensource-src-5.15.12.tar.xz

Build recipe: scripts/build-svg.cmd. The original source and build log are retained
under .toolchain. Qt5Svg.dll and the qsvg image plugin are dynamically replaceable.
The Qt license texts accompany this package. Application icon sources are SVG files
in assets/icons; no third-party icon pack or font is used.

## System fonts

The application uses Microsoft YaHei UI or Microsoft YaHei when installed,
and otherwise uses the system's general UI font. No font files are redistributed.

## Microsoft Visual C++ runtime

The app-local Microsoft.VC143.CRT runtime DLLs accompany this x64 release.
These Microsoft components are subject to their applicable distribution terms.
The package does not install a runtime system-wide.
