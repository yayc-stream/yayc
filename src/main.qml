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

import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Dialogs 1.3 as QQD
import QtWebEngine 1.11
import QtQuick.Controls 1.4 as QC1
import QtQuick.Controls.Styles 1.4 as QC1S
import QtQuick.Layouts 1.15
import QtQml.Models 2.2
import QtWebChannel 1.15
import Qt.labs.settings 1.1
import Qt.labs.platform 1.1 as QLP
import QtGraphicalEffects 1.0
import yayc 1.0

ApplicationWindow {
    id: win
    height: Screen.height;
    width: Screen.width
    visible: true
    title: qsTr("YAYC")
    property bool hidden: false
    signal interfaceLoaded()
    property bool isInterfaceLoaded: false

    onInterfaceLoaded: win.isInterfaceLoaded = true

    function minimizeToTray() {
        if (mainYaycLoader.loaded())
            mainYaycLoader.item.minimizeToTray()
        else
            win.hide()
    }

    onClosing: {
        close.accepted = false
        win.minimizeToTray()
    }

    Shortcut {
        sequence: StandardKey.Quit
        onActivated: {
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

    onVisibilityChanged: {
        win.hidden = (visibility == Window.Hidden || visibility == Window.Minimized)
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
            timerInterfaceLoaded.start()
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

    Timer {
        id: timerInterfaceLoaded
        interval: 100
        running: false
        repeat: false
        onTriggered: win.interfaceLoaded()
    }
}
