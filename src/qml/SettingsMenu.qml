/*
Copyright (C) 2023- YAYC team <info@yayc.stream>
CC BY-NC-SA 4.0 -- see MainYayc.qml header.
*/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import yayc 1.0

Popup {
    id: smenu

    // External refs / data
    property Item host                     // MainYayc root
    property var settingsObj               // QQmlSettings instance (for language)
    property string appVersionStr: ""
    property string latestRemoteVersionStr: ""
    property string releasesUrl: ""
    property string donateUrl: ""

    // Path strings (bound from MainYayc)
    property string youtubePath: ""
    property string historyPath: ""
    property string profilePath: ""
    property string extWorkingDirPath: ""

    // Action signals (MainYayc connects to existing dialogs)
    signal openBookmarksEdit()
    signal openHistoryEdit()
    signal openProfileEdit()
    signal openExtWorkDirEdit()
    signal openExtCommandsPopup()
    signal openAboutDialog()
    signal openHelpDialog()
    signal openProxyDialog()
    signal openCustomScriptDialog()
    signal clearSettingsRequested()
    signal quitRequested()

    width: 340
    padding: 0
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: YaycProperties.paneBackgroundColor
        border.color: YaycProperties.viewBorderColor
        border.width: 1
        radius: 6
    }

    onAboutToShow: stack.replace(null, topPage)

    // -------- Reusable inline components --------

    component MenuRow: Rectangle {
        id: rowRoot
        property string label: ""
        property string iconSource: ""
        property bool chevron: false
        property bool destructive: false
        property string shortcutText: ""
        property string rowTooltip: ""
        property alias rightItem: rightContainer.children
        property bool rowEnabled: true
        signal activated()

        Layout.fillWidth: true
        implicitHeight: 44
        color: ma.containsMouse && rowEnabled ? YaycProperties.hoverOverlayColor : "transparent"
        enabled: rowEnabled
        opacity: rowEnabled ? 1.0 : 0.5

        ToolTip {
            visible: ma.containsMouse && rowRoot.rowTooltip !== ""
            text: rowRoot.rowTooltip
            delay: 400
            font.pixelSize: YaycProperties.fsP1
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 10

            Item {
                Layout.preferredWidth: 22
                Layout.preferredHeight: 22
                visible: rowRoot.iconSource !== ""
                Image {
                    id: rowIcon
                    anchors.fill: parent
                    source: rowRoot.iconSource
                    visible: false
                    sourceSize.width: 22
                    sourceSize.height: 22
                }
                ColorOverlay {
                    anchors.fill: rowIcon
                    source: rowIcon
                    color: rowRoot.destructive ? YaycProperties.checkedButtonColor
                                               : YaycProperties.iconColor
                }
            }
            Label {
                text: rowRoot.label
                color: rowRoot.destructive ? YaycProperties.checkedButtonColor
                                           : YaycProperties.textColor
                font.pixelSize: YaycProperties.fsH4
                Layout.fillWidth: true
                elide: Label.ElideRight
            }
            Item {
                id: rightContainer
                Layout.preferredHeight: 28
                Layout.preferredWidth: childrenRect.width
                visible: children.length > 0
            }
            Label {
                text: rowRoot.shortcutText
                color: YaycProperties.disabledTextColor
                font.pixelSize: YaycProperties.fsP1
                visible: rowRoot.shortcutText !== ""
            }
            Label {
                text: "›"
                color: YaycProperties.disabledTextColor
                font.pixelSize: YaycProperties.fsH4
                visible: rowRoot.chevron
            }
        }

        MouseArea {
            id: ma
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: rowRoot.activated()
        }
    }

    component MenuDivider: Rectangle {
        Layout.fillWidth: true
        Layout.topMargin: 4
        Layout.bottomMargin: 4
        implicitHeight: 1
        color: YaycProperties.viewBorderColor
        opacity: 0.4
    }

    component HeaderRow: Rectangle {
        id: hdrRoot
        property string title: ""
        signal back()
        Layout.fillWidth: true
        implicitHeight: 48
        color: "transparent"
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 12
            spacing: 8
            ToolButton {
                icon.source: "/icons/arrow_back.svg"
                display: AbstractButton.IconOnly
                onClicked: hdrRoot.back()
                ToolTip.text: uiTr("Back")
                ToolTip.visible: hovered
                ToolTip.delay: 300
            }
            Label {
                text: hdrRoot.title
                color: YaycProperties.textColor
                font.pixelSize: YaycProperties.fsH4
                font.bold: true
                Layout.fillWidth: true
                elide: Label.ElideRight
            }
        }
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: YaycProperties.viewBorderColor
            opacity: 0.4
        }
    }

    // -------- StackView --------

    contentItem: StackView {
        id: stack
        implicitWidth: 340
        implicitHeight: currentItem ? currentItem.implicitHeight : 100
        initialItem: topPage
        clip: true
        pushEnter: null
        pushExit: null
        popEnter: null
        popExit: null
        replaceEnter: null
        replaceExit: null
    }

    // -------- Top page --------

    Component {
        id: topPage
        Item {
            implicitHeight: topCol.implicitHeight + 8
            ColumnLayout {
                id: topCol
                width: parent.width
                spacing: 0

                // New release banner
                Rectangle {
                    Layout.fillWidth: true
                    Layout.margins: 8
                    height: utilities.compareSemver(smenu.appVersionStr,
                                                    smenu.latestRemoteVersionStr) < 0 ? 36 : 0
                    visible: height > 0
                    color: YaycProperties.surfaceOverlayColor
                    radius: 4
                    Label {
                        anchors.centerIn: parent
                        text: uiTr("New release available") + ": v" + smenu.latestRemoteVersionStr
                        color: "crimson"
                        font.bold: true
                        font.pixelSize: YaycProperties.fsP1
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Qt.openUrlExternally(smenu.releasesUrl + "/releases")
                    }
                }

                MenuRow {
                    label: uiTr("Bookmarks")
                    iconSource: "qrc:/images/youtube-128.png"
                    chevron: true
                    rowTooltip: uiTr("Directory where bookmarks data is stored")
                        + (smenu.youtubePath !== "" ? "\n" + smenu.youtubePath : "")
                    onActivated: smenu.openBookmarksEdit()
                }
                MenuRow {
                    label: uiTr("History")
                    iconSource: "/icons/history.svg"
                    chevron: true
                    rowTooltip: uiTr("Directory where watch history is stored")
                        + (smenu.historyPath !== "" ? "\n" + smenu.historyPath : "")
                    onActivated: smenu.openHistoryEdit()
                }
                MenuRow {
                    label: uiTr("Profile")
                    iconSource: "qrc:/images/google-chrome-is.svg"
                    chevron: true
                    rowTooltip: uiTr("Chromium profile directory for login cookies")
                        + (smenu.profilePath !== "" ? "\n" + smenu.profilePath : "")
                    onActivated: smenu.openProfileEdit()
                }
                MenuRow {
                    label: uiTr("Ext working dir")
                    iconSource: "/icons/exit_to_app.svg"
                    chevron: true
                    visible: smenu.host && smenu.host.debugMode
                    rowTooltip: uiTr("Working directory for external executable")
                        + (smenu.extWorkingDirPath !== "" ? "\n" + smenu.extWorkingDirPath : "")
                    onActivated: smenu.openExtWorkDirEdit()
                }
                MenuRow {
                    label: uiTr("External commands")
                    iconSource: "/icons/extension.svg"
                    chevron: true
                    visible: smenu.host && smenu.host.debugMode
                    rowTooltip: uiTr("Configure commands run via the context menu")
                    rowEnabled: smenu.host && smenu.host.extWorkingDirExists
                    onActivated: smenu.openExtCommandsPopup()
                }

                MenuDivider {}

                MenuRow {
                    label: uiTr("Language")
                    iconSource: "/icons/description.svg"
                    rowTooltip: uiTr("Interface language")
                    rightItem: ComboBox {
                        id: langCombo
                        anchors.verticalCenter: parent.verticalCenter
                        model: utilities.availableLanguages
                        currentIndex: {
                            if (!smenu.settingsObj) return 0
                            var idx = model.indexOf(smenu.settingsObj.language)
                            return idx >= 0 ? idx : 0
                        }
                        displayText: utilities.languageDisplayName(currentText)
                        onActivated: if (smenu.settingsObj) smenu.settingsObj.language = currentText
                        delegate: ItemDelegate {
                            text: utilities.languageDisplayName(modelData)
                            width: langCombo.width
                        }
                        implicitWidth: 130
                        font.pixelSize: YaycProperties.fsP2
                    }
                }

                MenuRow {
                    label: uiTr("Dark mode")
                    iconSource: "/icons/sliders.svg"
                    rowTooltip: uiTr("Toggle dark/light theme")
                    rightItem: Switch {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: smenu.host ? smenu.host.darkMode : true
                        onToggled: if (smenu.host) smenu.host.darkMode = checked
                    }
                    onActivated: if (smenu.host) smenu.host.darkMode = !smenu.host.darkMode
                }

                MenuDivider {}

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    Layout.topMargin: 6
                    Layout.bottomMargin: 6
                    spacing: 2
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: uiTr("Last bookmarks destinations")
                            color: YaycProperties.textColor
                            font.pixelSize: YaycProperties.fsH4
                            Layout.fillWidth: true
                        }
                        Label {
                            text: maxDestSlider.value
                            color: YaycProperties.disabledTextColor
                            font.pixelSize: YaycProperties.fsP1
                        }
                    }
                    Slider {
                        id: maxDestSlider
                        Layout.fillWidth: true
                        from: 1; to: 10; stepSize: 1
                        snapMode: Slider.SnapAlways
                        value: smenu.host ? smenu.host.maxRecentDestinations : 5
                        onMoved: if (smenu.host) smenu.host.maxRecentDestinations = value
                        ToolTip {
                            parent: maxDestSlider.handle
                            visible: maxDestSlider.hovered || maxDestSlider.pressed
                            text: maxDestSlider.value.toString()
                        }
                    }
                }

                MenuDivider {}

                MenuRow {
                    label: uiTr("Advanced")
                    iconSource: "/icons/settings.svg"
                    chevron: true
                    rowTooltip: uiTr("Advanced and developer options")
                    onActivated: stack.push(advancedPage)
                }

                MenuDivider {}

                MenuRow {
                    label: uiTr("About")
                    iconSource: "/icons/info.svg"
                    chevron: true
                    rowTooltip: uiTr("About YAYC")
                    onActivated: { smenu.openAboutDialog(); smenu.close() }
                }
                MenuRow {
                    label: uiTr("Help")
                    iconSource: "/icons/help.svg"
                    chevron: true
                    rowTooltip: uiTr("Open help center")
                    onActivated: { smenu.openHelpDialog(); smenu.close() }
                }
                MenuRow {
                    label: uiTr("Support us")
                    iconSource: "/icons/star_fill.svg"
                    chevron: true
                    rowEnabled: smenu.donateUrl !== ""
                    rowTooltip: uiTr("Support YAYC development")
                    onActivated: { Qt.openUrlExternally(smenu.donateUrl); smenu.close() }
                }

                MenuDivider {}

                MenuRow {
                    label: uiTr("Quit")
                    iconSource: "/icons/exit_to_app.svg"
                    shortcutText: "Ctrl+Q"
                    rowTooltip: uiTr("Exit YAYC")
                    onActivated: smenu.quitRequested()
                }
            }
        }
    }

    // -------- Advanced page --------

    Component {
        id: advancedPage
        Item {
            implicitHeight: advCol.implicitHeight + 8
            ColumnLayout {
                id: advCol
                width: parent.width
                spacing: 0
                HeaderRow {
                    title: uiTr("Advanced")
                    onBack: stack.pop()
                }
                MenuRow {
                    label: uiTr("Developer mode")
                    iconSource: "/icons/extension.svg"
                    rowTooltip: uiTr("Show developer options: external working dir and commands")
                    rightItem: Switch {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: smenu.host ? smenu.host.debugMode : false
                        onToggled: if (smenu.host) smenu.host.debugMode = checked
                    }
                    onActivated: if (smenu.host) smenu.host.debugMode = !smenu.host.debugMode
                }
                MenuDivider {}
                MenuRow {
                    label: uiTr("Delete storage on remove")
                    iconSource: "/icons/delete_forever.svg"
                    rowTooltip: uiTr("Erase related video data in the external working directory when a video is deleted")
                    rightItem: Switch {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: smenu.host ? smenu.host.removeStorageOnDelete : false
                        onToggled: if (smenu.host) smenu.host.removeStorageOnDelete = checked
                    }
                    onActivated: if (smenu.host) smenu.host.removeStorageOnDelete = !smenu.host.removeStorageOnDelete
                }
                MenuRow {
                    label: uiTr("Blank when invisible")
                    iconSource: "/icons/exit_to_app.svg"
                    rowTooltip: uiTr("Unload the web view when YAYC is hidden and no video is playing")
                    rightItem: Switch {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: smenu.host ? smenu.host.blankWhenHidden : false
                        onToggled: if (smenu.host) smenu.host.blankWhenHidden = checked
                    }
                    onActivated: if (smenu.host) smenu.host.blankWhenHidden = !smenu.host.blankWhenHidden
                }
                MenuDivider {}
                MenuRow {
                    label: uiTr("Custom script")
                    iconSource: "/icons/js.svg"
                    chevron: true
                    rowTooltip: uiTr("Edit the custom script run after loading a video page")
                    onActivated: { smenu.openCustomScriptDialog(); smenu.close() }
                }
                MenuRow {
                    label: uiTr("Proxy settings")
                    iconSource: "/icons/settings.svg"
                    chevron: true
                    rowTooltip: uiTr("Edit proxy settings for network access")
                    onActivated: { smenu.openProxyDialog(); smenu.close() }
                }
                MenuDivider {}
                MenuRow {
                    label: uiTr("Clear settings")
                    iconSource: "/icons/delete_forever.svg"
                    destructive: true
                    rowTooltip: uiTr("Erase all settings and restart YAYC")
                    onActivated: smenu.clearSettingsRequested()
                }
            }
        }
    }
}
