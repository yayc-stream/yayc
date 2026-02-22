# YAYC : Yet Another YouTube Client


[![CC BY-NC-SA License](https://img.shields.io/badge/license-CC%20BY--NC--SA--4.0-green)](https://github.com/yayc-stream/yayc/blob/master/LICENSE)
[![Releases](https://img.shields.io/github/release/yayc-stream/yayc.svg)](https://github.com/yayc-stream/yayc/releases)
[![Github all releases](https://img.shields.io/github/downloads/yayc-stream/yayc/total.svg)](https://GitHub.com/yayc-stream/yayc/releases/)

A YouTube client intially built with Qt5/QML, now Qt6/QML.
It sports bookmarks support, progress tracking, and more.
It is intended to help managing video queues and playlists, sparing the user from having
to use many browser tabs or windows to keep track of multiple content.

## Dependencies

- `QtCore`
- `QtNetwork`
- `QtWidgets`
- `QtQuick`
- `QtQuickControls1`
- `QtQuickControls2`
- `QtWebEngine`
- `QtWebChannel`
- `OpenSSL`

## Build instructions

### Provisioning

#### Windows

Building YAYC on Windows requires Qt6 and Visual Studio 2019 or 2022 (optionally QtCreator).
Visual Studio 2019 (community) can be obtained for free from Microsoft (https://visualstudio.microsoft.com/vs/older-downloads/)
Latest version of Qt6 (6.10) can be installed through the Qt online installer (https://www.qt.io/download-qt-installer).
OpenSSL now appears included by default by the Qt online installer.

#### Linux

Building YAYC on linux requires Qt6 and a gcc compiler toolchain.
On debian-based distributions the c++ toolchain can usually be installed with
```
apt install build-essential
```

Qt6 can be obtained through the Qt online installer (see above) or through your linux distribution package manager.
On debian-based distributions (ubuntu, mint, etc.) for example:
```
apt install libqt6webchannel6 libqt6webenginequick6 libqt6webenginequickdelegatesqml6 qt6-webengine-private-dev qt6-webengine-dev-tools libqt6webengine6-data libqt6network6 qml6-module-qtquick-controls qml6-module-qtquick-nativestyle libqt6quickcontrols2-6 libqt6quickcontrols2impl6 qml6-module-qtquick qml6-module-qtquick-dialogs qml6-module-qtquick-layouts qml6-module-qtquick-localstorage qml6-module-qtquick-window qml6-module-qtquick-templates qml6-module-qtwebchannel qml6-module-qt-labs-settings qml6-module-qt-labs-platform qml6-module-qtquick-dialogs libqt6core5compat6 qml6-module-qt5compat-graphicaleffects qt6-5compat-dev openssl 
```

Note: YAYC has been tested with Qt 6.8 and Qt 6.10. 
Earlier Qt6 versions are untested and with several known issues, limitations and missing component or features, 
and will most likely not be suitable for building YAYC.
If your distribution ships an earlier version of Qt6, it is recommended to use the Qt online installer.


### Building

```
qmake6 CONFIG+=release
make
```

## Build instructions (older versions based on Qt5)

### Provisioning

#### Windows

Building YAYC on Windows requires Qt5 and Visual Studio 2019 (optionally QtCreator).
Visual Studio 2019 (community) can be obtained for free from Microsoft (https://visualstudio.microsoft.com/vs/older-downloads/)
Latest version of Qt5 (5.15) can be installed through the Qt online installer (https://www.qt.io/download-qt-installer).
Remember to select OpenSSL 1.1.1 from the Qt online installer.

**UPDATE:** Starting summer 2024, QtWebEngine shipped with Qt 5.15.2 (last official Qt5 opensource release) is not able to run the youtube embedded player anymore. 
At the same time, it seemingly is the only Qt5 release for windows able to properly deploy with windeployqt. 
Therefore, for the time being, we are unable to provide an updated binary release package for Windows.
A working solution is to use Microsoft *vcpkg* and vcpkg Qt5 package to build and run YAYC. 
Unfortunatly, vcpkg Qt5 package also seemingly has a broken windeployqt setup, and so it cannot be used to produce a binary YAYC windows release.

#### Linux

Building YAYC on linux requires Qt5 and a gcc compiler toolchain.
On debian-based distributions the c++ toolchain can usually be installed with
```
apt install build-essential
```

Qt5 can be obtained through the Qt online installer (see above) or through your linux distribution package manager.
On debian-based distributions (ubuntu, mint, etc.) for example:
```
apt install libqt5webchannel5-dev libqt5webengine5 qtwebengine5-dev qtwebengine5-private-dev qtwebengine5-dev-tools libqt5webengine-data libqt5websockets5-dev libqt5network5 libqt5quickcontrols2-5 qml-module-qtquick-controls qml-module-qtquick-controls2 qml-module-qtquick2 qml-module-qtwebengine qml-module-qtwebchannel qtquickcontrols2-5-dev qml-module-qt-labs-platform qml-module-qt-labs-settings qml-module-qtquick-dialogs openssl
```
Note: OpenSSL 1.1 needs to be available and properly linked to the Qt5 installation.
This should be not a problem with distribution packages, but it may be with installer-based Linux installations.


### Building

```
qmake CONFIG+=release
make
```

## Releases

For the time being we provide a portable Win32 build.
We will not provide pre-built AppImage packages for Linux distributions as this technology proved to be insufficient when it comes to shipping QtWebEngine and OpenSSL based applications.
If you are using a Linux OS, we encourage you to try building YAYC on your system, since modern distributions should provide all the necessary Qt 5.15 libraries, and YAYC is expected to be relatively easy to build (see above).
We are currently working on creating macOS binary distribution packages.

## Usage

```
# yayc
```

Notes: 
- If no bookmarks directory is specified, bookmarks won't be saved.
- If no history directory is specified, history won't be saved.
- If no Google profile directory is specified, YAYC will operate in Inkognito mode, and logging into your Google account won't be remembered at next app restart.

For screenshots and illustrations, visit [https://yayc.stream/#usage](https://yayc.stream/#usage).

## AI Training Prohibition

**The use of this project's code, documentation, or any associated materials for training artificial intelligence models is explicitly prohibited under the terms of this project's [license](LICENSE), for both commercial and non-commercial purposes.**

This prohibition applies to all forms of AI/ML training and indexing, including but not limited to:
- Large language models (LLMs)
- Code generation models
- Retrieval-Augmented Generation (RAG) systems, embedding databases, and vector stores
- Any other machine learning or AI-assisted system

This applies regardless of whether the code is used for direct model training, fine-tuning, embedding generation, retrieval indexing, or any other form of ingestion into an AI system, and regardless of whether it is performed by automated crawlers, scraping tools, or manual collection.

**Any entity — individual, organization, or automated system — that uses this code or its contents for AI training purposes is in direct violation of the license and is therefore legally liable under the applicable copyright law.**

See the full [LICENSE](LICENSE) for details.
