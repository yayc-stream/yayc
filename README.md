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
- `QtQuickControls2`
- `QtWebEngineQuick`
- `QtWebChannel`
- `Qt5Compat` (core5compat)

## Build instructions

### Provisioning

#### Windows

Building YAYC on Windows requires Qt6 and Visual Studio 2022 (optionally QtCreator).
Visual Studio 2022 (community) can be obtained for free from Microsoft (https://visualstudio.microsoft.com/vs/older-downloads/)
Qt6 can be installed through the Qt online installer (https://www.qt.io/download-qt-installer).

#### Linux

Building YAYC on Linux requires Qt6 and a gcc compiler toolchain.
On Debian-based distributions, the C++ toolchain can be installed with:
```
apt install build-essential
```

Qt6 can be obtained through the Qt online installer or your Linux distribution's package manager.
On Debian-based distributions (Ubuntu, Linux Mint, etc.):
```
apt install libqt6webchannel6 libqt6webenginequick6 qt6-webengine-private-dev qt6-webengine-dev-tools libqt6webengine6-data libqt6network6 qml6-module-qtquick-controls qml6-module-qtquick-nativestyle libqt6quickcontrols2-6 libqt6quickcontrols2impl6 qml6-module-qtquick qml6-module-qtquick-dialogs qml6-module-qtquick-layouts qml6-module-qtquick-localstorage qml6-module-qtquick-window qml6-module-qtquick-templates qml6-module-qtwebchannel qml6-module-qt-labs-settings qml6-module-qt-labs-platform libqt6core5compat6 qml6-module-qt5compat-graphicaleffects qt6-5compat-dev
```

**Note:** YAYC has been tested with Qt 6.8 and Qt 6.10. Earlier Qt6 versions may have compatibility issues. Use the Qt online installer if your distribution provides an older version.


### Building

```
mkdir -p /tmp/yayc_build
cd /tmp/yayc_build
qmake6 <path/to/yayc.pro> CONFIG+=release
make
```

## Releases

We currently provide binary releases for all 3 main desktop platforms. If there are issues with them, please open an issue here on github.

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
