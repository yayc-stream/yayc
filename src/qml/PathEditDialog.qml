/*
Copyright (C) 2023- YAYC team <info@yayc.stream>
CC BY-NC-SA 4.0 -- see MainYayc.qml header.
*/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as QQD
import Qt5Compat.GraphicalEffects
import yayc 1.0

Dialog {
    id: dlg

    property string dialogTitle: ""
    property string initialPath: ""
    signal pathAccepted(string path)

    title: dialogTitle
    modal: true
    width: 600
    padding: 16
    anchors.centerIn: parent

    onAboutToShow: pathField.text = dlg.initialPath

    RowLayout {
        anchors.fill: parent
        spacing: 8

        TextField {
            id: pathField
            Layout.fillWidth: true
            selectByMouse: true
            font.pixelSize: YaycProperties.fsP1
            cursorVisible: true
            color: utilities.directoryExists(text)
                   ? "#4caf50"
                   : YaycProperties.textColor
            placeholderText: "<" + uiTr("path") + ">"

            Keys.onReturnPressed: if (utilities.directoryExists(text)) dlg.accept()
        }

        Button {
            id: browseBtn
            display: Button.IconOnly
            icon.source: "/icons/folder_open.svg"
            flat: true
            onClicked: folderDlg.open()
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.delay: 300
            ToolTip.text: uiTr("Browse for directory")
        }

        Button {
            display: Button.IconOnly
            icon.source: "/icons/delete_forever.svg"
            flat: true
            enabled: pathField.text !== ""
            onClicked: pathField.text = ""
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.delay: 300
            ToolTip.text: uiTr("Clear path")
        }
    }

    footer: DialogButtonBox {
        Button {
            text: uiTr("OK")
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            enabled: pathField.text === "" || utilities.directoryExists(pathField.text)
        }
        Button {
            text: uiTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }

    onAccepted: dlg.pathAccepted(pathField.text)

    QQD.FolderDialog {
        id: folderDlg
        onAccepted: {
            var path = String(selectedFolder)
            // strip file:// prefix
            path = path.slice(7)
            if (Qt.platform.os === "windows" && path[0] === '/')
                path = path.slice(1)
            pathField.text = path
        }
    }
}
