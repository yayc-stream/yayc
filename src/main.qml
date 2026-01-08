/*
Copyright (C) 2023- YAYC team <yaycteam@gmail.com>

This work is licensed under the terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit https://creativecommons.org/licenses/by-nc-sa/4.0/ or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

In addition to the above,
- The use of this work for training artificial intelligence is prohibited for both commercial and non-commercial use.
- Any and all donation options in derivative work must be the same as in the original work.
- All use of this work outside of the above terms must be explicitly agreed upon in advance with the exclusive copyright owner(s).
- Any derivative work must retain the above copyright and acknowledge that any and all use of the derivative work outside the above terms
  must be explicitly agreed upon in advance with the exclusive copyright owner(s) of the original work.

*/

import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Dialogs as QQD
import QtWebEngine
import QtQuick.Layouts
import QtQml.Models
import QtWebChannel
import Qt.labs.settings
import Qt.labs.platform as QLP
import Qt5Compat.GraphicalEffects
import yayc 1.0

ApplicationWindow {
    id: win
    height: Screen.height;
    width: Screen.width
    visible: true
    title: qsTr("YAYC")
    property bool hidden: (win.visibility == Window.Hidden || win.visibility == Window.Minimized)
    property bool quitting: false

    function minimizeToTray() {
        if (mainYaycLoader.loaded())
            mainYaycLoader.item.minimizeToTray()
        else
            win.hide()
    }

    onClosing: (close) => {
        if (win.quitting) {
            close.accepted = true
        } else {
            close.accepted = false
            win.minimizeToTray()
        }
    }

    Shortcut {
        sequence: StandardKey.Quit
        onActivated: {
            win.quitting = true
            if (mainYaycLoader.loaded())
                mainYaycLoader.item.quit()
            else
                Qt.quit()
        }
    }

    Shortcut {
        sequence: "Ctrl+H"
        onActivated: win.minimizeToTray()
    }

    Loader {
        id: mainYaycLoader
        anchors.fill: parent
        active: false
        source: "/MainYayc.qml"
        asynchronous: true
        onLoaded: {
            item.visible = true;
            mainSplashLoader.source = "";
        }
    }

    Loader {
        id: mainSplashLoader
        anchors.fill: parent
        source: "/MainSplash.qml"

        onLoaded: {
            item.visible = true
            mainYaycLoader.active = true;
        }
    }
}
