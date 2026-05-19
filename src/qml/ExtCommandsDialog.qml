/*
Copyright (C) 2023- YAYC team <info@yayc.stream>
CC BY-NC-SA 4.0 -- see MainYayc.qml header.
*/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import yayc 1.0

Dialog {
    id: extDlg

    property Item host       // MainYayc root

    modal: true
    width: 700
    x: (parent ? (parent.width - width) * 0.5 : 0)
    y: (parent ? (parent.height - height) * 0.5 : 0)

    header: Item {
        width: extDlg.width
        height: YaycProperties.fsH3 * 1.5
        Label {
            anchors.centerIn: parent
            text: "<b>" + uiTr("External commands") + "</b>"
            font.pixelSize: YaycProperties.fsH3
        }
    }
    footer: DialogButtonBox {
        standardButtons: DialogButtonBox.Close
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        // Read-only ext working dir info
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item {
                Layout.preferredWidth: 22
                Layout.preferredHeight: 22
                Image {
                    id: wdirImg
                    source: "/icons/exit_to_app.svg"
                    visible: false
                    anchors.fill: parent
                }
                ColorOverlay {
                    source: wdirImg
                    anchors.fill: wdirImg
                    color: YaycProperties.iconColor
                }
            }
            Label {
                text: uiTr("Working dir") + ":"
                color: YaycProperties.textColor
                font.pixelSize: YaycProperties.fsP2
            }
            Label {
                Layout.fillWidth: true
                text: extDlg.host && extDlg.host.extWorkingDirExists
                      ? extDlg.host.extWorkingDirPath
                      : "<" + uiTr("undefined — set from hamburger menu") + ">"
                color: extDlg.host && extDlg.host.extWorkingDirExists
                       ? "#4caf50"
                       : YaycProperties.disabledTextColor
                font.pixelSize: YaycProperties.fsP2
                elide: Label.ElideMiddle
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: YaycProperties.viewBorderColor
            opacity: 0.4
        }

        // Commands list
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 280
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: parent.width
                spacing: 8

                Repeater {
                    id: extCommandsRepeater
                    model: extDlg.host ? extDlg.host.externalCommands : []
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Item {
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 28
                            Image {
                                id: cmdImg
                                source: "/icons/extension.svg"
                                visible: false
                                anchors.fill: parent
                            }
                            ColorOverlay {
                                source: cmdImg
                                anchors.fill: cmdImg
                                color: YaycProperties.iconColor
                            }
                        }
                        TextField {
                            id: extCmdName
                            Layout.preferredWidth: 100
                            selectByMouse: true
                            font.pixelSize: YaycProperties.fsP1
                            color: YaycProperties.textColor
                            text: modelData.name
                            placeholderText: uiTr("Name")
                            onEditingFinished: {
                                if (!extDlg.host) return
                                var cmds = extDlg.host.externalCommands.slice()
                                cmds[index] = Object.assign({}, cmds[index], {name: text})
                                extDlg.host.externalCommands = cmds
                            }
                        }
                        TextField {
                            id: extCmdCmd
                            Layout.fillWidth: true
                            selectByMouse: true
                            font.pixelSize: YaycProperties.fsP1
                            color: utilities.executableExists(text)
                                   ? "#4caf50"
                                   : YaycProperties.textColor
                            text: modelData.command
                            placeholderText: uiTr("Command path")
                            onEditingFinished: {
                                if (!extDlg.host) return
                                var cmds = extDlg.host.externalCommands.slice()
                                cmds[index] = Object.assign({}, cmds[index], {command: text})
                                extDlg.host.externalCommands = cmds
                            }
                        }
                        Button {
                            flat: true
                            display: Button.IconOnly
                            icon.source: "/icons/delete_forever.svg"
                            onClicked: if (extDlg.host) extDlg.host.removeCommand(index)
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.delay: 300
                            ToolTip.text: uiTr("Remove this command")
                        }
                        Button {
                            flat: true
                            display: Button.IconOnly
                            icon.source: "/icons/add.svg"
                            visible: index === (extCommandsRepeater.count - 1)
                            enabled: extDlg.host && extDlg.host.extWorkingDirExists
                            onClicked: if (extDlg.host) extDlg.host.pushEmptyCommand()
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.delay: 300
                            ToolTip.text: uiTr("Add another command")
                        }
                    }
                }

                Button {
                    Layout.alignment: Qt.AlignLeft
                    visible: extCommandsRepeater.count === 0
                    enabled: extDlg.host && extDlg.host.extWorkingDirExists
                    text: uiTr("Add command")
                    icon.source: "/icons/add.svg"
                    onClicked: if (extDlg.host) extDlg.host.pushEmptyCommand()
                }
            }
        }
    }
}
