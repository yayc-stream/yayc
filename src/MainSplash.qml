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

Rectangle {
    id: splash
    color: "black"
    opacity: 0.0
    visible: false

    Timer {
        id: opacityTimer
        running: false
        repeat: false
        interval: 1000
        onTriggered: splash.opacity = 1
    }

    Component.onCompleted: opacityTimer.start()

    Image {
        id: yaycLogo
        source: "/images/yayc-inlined.png"
        anchors.centerIn: parent
        fillMode: Image.PreserveAspectFit
        height: 128
        mipmap: true
        smooth: true
    }
    ProgressBar {
        id: progressBar
        anchors {
            left: yaycLogo.left
            right: yaycLogo.right
            top: yaycLogo.bottom
            topMargin: 8
        }

        indeterminate: true
    }
}
