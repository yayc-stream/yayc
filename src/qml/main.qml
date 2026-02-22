/*
Copyright (C) 2023- YAYC team <yaycteam@gmail.com>

This work is licensed under the terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit https://creativecommons.org/licenses/by-nc-sa/4.0/ or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

In addition to the above,
- The use of this work for training, fine-tuning, or otherwise feeding artificial intelligence systems is prohibited for both commercial and non-commercial use.
  This includes, but is not limited to, the ingestion of this work into large language models (LLMs), code generation models,
  Retrieval-Augmented Generation (RAG) systems, embedding databases, vector stores, or any other AI-assisted system.
- Any and all donation options in derivative work must be the same as in the original work.
- All use of this work outside of the above terms must be explicitly agreed upon in advance with the exclusive copyright owner(s).
- Any derivative work must retain the above copyright and acknowledge that any and all use of the derivative work outside the above terms
  must be explicitly agreed upon in advance with the exclusive copyright owner(s) of the original work.

*/

import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Controls.Material
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
    property bool hidden: (win.visibility == Window.Hidden)
    property bool quitting: false
    signal interfaceLoaded()
    property bool isInterfaceLoaded: false

    Material.theme: YaycProperties.isDarkMode ? Material.Dark : Material.Light
    Material.primary: YaycProperties.isDarkMode ? "#3d3d3d" : "#f5f5f5"

    Component.onCompleted: {
        YaycProperties.isDarkMode = initialDarkMode
        utilities.setColorScheme(initialDarkMode)
    }
    onInterfaceLoaded: win.isInterfaceLoaded = true

    function quit() {
        if (mainYaycLoader.isLoaded() && mainYaycLoader.item)
            mainYaycLoader.item.prepareQuit()

        win.quitting = true
        Qt.quit()
    }

    function quitApp() {
        win.quit()
    }

    Dialog {
        id: quitConfirmDialog
        title: qsTr("Quit YAYC")
        modal: true
        anchors.centerIn: parent
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        Label {
            text: qsTr("Are you sure you want to quit?")
        }
        footer: RowLayout {
            Layout.margins: 16
            Button {
                Layout.leftMargin: 16
                Layout.bottomMargin: 8
                flat: true
                Material.foreground: Material.accent
                text: qsTr("Hide")
                onClicked: {
                    quitConfirmDialog.close()
                    win.minimizeToTray()
                }
            }
            Item { Layout.fillWidth: true }
            Button {
                Layout.rightMargin: 16
                Layout.bottomMargin: 8
                flat: true
                Material.foreground: Material.accent
                text: qsTr("Quit")
                onClicked: quitConfirmDialog.accepted()
            }
        }
        onAccepted: {
            win.quit()
        }
    }

    function minimizeToTray() {
        if (mainYaycLoader.isLoaded())
            mainYaycLoader.item.minimizeToTray()
        else
            win.hide()
    }

    onClosing: (close) => {
        if (win.quitting) {
            close.accepted = true
        } else {
            close.accepted = false
            if (Qt.platform.os !== "osx") {
                win.minimizeToTray()
            } else {
                quitConfirmDialog.open()
            }
        }
    }

    Shortcut {
        sequence: Qt.platform.os === "osx" ? "Meta+Q" : "Ctrl+Q"
        onActivated: win.quit()
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
        function isLoaded() { return status === Loader.Ready }
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
