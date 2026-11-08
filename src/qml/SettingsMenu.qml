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

    // The update on offer, for the banner and the Advanced row
    QtObject {
        id: offer
        readonly property string latest: smenu.latestRemoteVersionStr
        readonly property bool newRelease: utilities.compareSemver(smenu.appVersionStr, latest) < 0
        // That release has no package for this install
        readonly property bool unavailable: updateManager.unavailableVersion === latest
        // Another YAYC of this install downloads and installs
        readonly property bool elsewhere: updateManager.updatesElsewhere
        readonly property bool downloadable: newRelease && updateManager.updateSupported && !unavailable
                                             && !elsewhere
        readonly property int ustate: updateManager.state
        readonly property bool hasReady: ustate === UpdateManager.ReadyToInstall
                                         || ustate === UpdateManager.NotVerified
                                         || ustate === UpdateManager.ManualInstall
                                         || (ustate === UpdateManager.ExitPending
                                             && updateManager.autoStaged)
        // A ready package older than the release: the newer one can come too
        readonly property bool newerThanReady: downloadable && hasReady
                                               && utilities.compareSemver(updateManager.readyVersion, latest) < 0
                                               && !updateManager.retryDownloads
        // Nothing ready or running
        readonly property bool fromIdle: downloadable
                                         && (ustate === UpdateManager.Idle || ustate === UpdateManager.Failed)
                                         && updateManager.readyVersion !== latest
    }

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

        MouseArea {
            id: ma
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: rowRoot.activated()
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

                // New release / autoupdate banner
                Rectangle {
                    id: updateBanner
                    objectName: "updateBanner"
                    property int ustate: updateManager.state
                    property bool ready: ustate === UpdateManager.ReadyToInstall
                    property bool manual: ustate === UpdateManager.ManualInstall
                    property bool notVerified: ustate === UpdateManager.NotVerified
                    property bool twoLines: offer.hasReady
                    // A check in the background (e.g. while ready) shows nothing
                    property bool checkOnly: updateManager.checking
                                             && (ustate === UpdateManager.Idle
                                                 || ustate === UpdateManager.Failed)
                    property bool busy: checkOnly
                                        || ustate === UpdateManager.Downloading
                                        || ustate === UpdateManager.Verifying
                                        || ustate === UpdateManager.Applying
                    // In Failed, Retry already downloads it again
                    property bool canDownload: offer.newerThanReady
                                               || (offer.fromIdle && !updateManager.canRetry)
                    property bool copied: false

                    function copyToClipboard(text) {
                        utilities.copyText(text)
                        updateBanner.copied = true
                        copiedTimer.restart()
                    }

                    Layout.fillWidth: true
                    Layout.margins: 8
                    height: !(offer.newRelease || busy || ustate !== UpdateManager.Idle) ? 0
                          : (twoLines ? 48 : 36) + (busy ? 8 : 0)
                    visible: height > 0
                    color: YaycProperties.surfaceOverlayColor
                    border.width: twoLines ? 1 : 0
                    border.color: "crimson"
                    radius: 4

                    // Under the action buttons: they take their own clicks
                    MouseArea {
                        id: bannerMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var us = updateManager.state
                            if (updateBanner.manual)
                                updateBanner.copyToClipboard(updateManager.manualInstallCommand)
                            else if (us === UpdateManager.RestartPending || us === UpdateManager.ExitPending) {
                                // false: other YAYC windows would keep it from installing
                                if (updateManager.prepareRestart())
                                    win.restart()
                            }
                            else if (us === UpdateManager.ReadyToInstall)
                                updateManager.applyUpdate()
                            else if (us === UpdateManager.Idle || us === UpdateManager.Failed)
                                Qt.openUrlExternally(smenu.releasesUrl + "/releases")
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 4
                        spacing: 4
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                elide: Label.ElideRight
                                text: {
                                    var us = updateBanner.ustate
                                    if (us === UpdateManager.Downloading)
                                        return uiTr("Downloading update") + " v" + updateManager.downloadVersion + ": "
                                                + Math.round(updateManager.downloadProgress * 100) + "%"
                                    if (us === UpdateManager.Verifying)
                                        return uiTr("Verifying update") + "..."
                                    if (us === UpdateManager.Applying)
                                        return uiTr("Installing update") + "..."
                                    if (us === UpdateManager.ReadyToInstall)
                                        return updateManager.updateMethod === UpdateConfig.ManualDeb
                                                ? uiTr("Update ready. Click to check it and show the install command")
                                                : uiTr("Apply update")
                                    if (us === UpdateManager.ManualInstall)
                                        return uiTr("This command installs the update and closes YAYC:")
                                    if (us === UpdateManager.NotVerified)
                                        return uiTr("Could not verify update")
                                    if (us === UpdateManager.RestartPending)
                                        return uiTr("Update installed. Click to restart")
                                    if (us === UpdateManager.ExitPending)
                                        return uiTr("Update installs when YAYC quits. Click to restart now")
                                    if (updateBanner.checkOnly)
                                        return uiTr("Checking for updates") + "..."
                                    if (us === UpdateManager.Failed)
                                        return uiTr("Update failed")
                                    return uiTr("New release available") + ": v" + smenu.latestRemoteVersionStr
                                            + (offer.unavailable
                                               ? " (" + uiTr("no package for this installation") + ")"
                                               : offer.elsewhere
                                                 ? " (" + uiTr("another YAYC window installs it") + ")"
                                                 : "")
                                }
                                color: "crimson"
                                font.bold: true
                                font.pixelSize: YaycProperties.fsP1
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: updateBanner.twoLines
                                horizontalAlignment: Text.AlignHCenter
                                elide: Label.ElideMiddle
                                text: updateBanner.manual ? updateManager.manualInstallCommand
                                                          : "v" + updateManager.readyVersion
                                color: YaycProperties.textColor
                                font.pixelSize: YaycProperties.fsP2
                            }
                            ProgressBar {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 4
                                visible: updateBanner.busy
                                // Only a download knows how far it is
                                indeterminate: updateBanner.ustate !== UpdateManager.Downloading
                                from: 0
                                to: 1
                                value: updateManager.downloadProgress
                            }
                        }
                        // Also a password prompt still open, or a stage waiting for the quit
                        Button {
                            readonly property bool staged: updateBanner.ustate === UpdateManager.ExitPending
                            visible: updateManager.canCancel || staged
                            flat: true
                            objectName: "bannerCancel"
                            text: uiTr("Cancel")
                            font.pixelSize: YaycProperties.fsP2
                            onClicked: staged ? updateManager.withdrawStage() : updateManager.cancelDownload()
                        }
                        Button {
                            visible: updateBanner.notVerified
                            flat: true
                            objectName: "bannerRetryCheck"
                            text: uiTr("Retry check")
                            font.pixelSize: YaycProperties.fsP2
                            onClicked: updateManager.retry()
                        }
                        Button {
                            visible: updateManager.canInstallUnverified
                            flat: true
                            objectName: "bannerInstallAnyway"
                            text: uiTr("Install anyway")
                            font.pixelSize: YaycProperties.fsP2
                            onClicked: updateManager.applyUnverified()
                        }
                        Button {
                            visible: updateManager.canRetry
                            flat: true
                            objectName: "bannerRetry"
                            text: uiTr("Retry")
                            font.pixelSize: YaycProperties.fsP2
                            onClicked: updateManager.retry()
                        }
                        Button {
                            visible: updateBanner.canDownload
                            flat: true
                            objectName: "bannerDownload"
                            text: uiTr("Download")
                            font.pixelSize: YaycProperties.fsP2
                            onClicked: updateManager.startUpdate(smenu.latestRemoteVersionStr)
                        }
                    }
                    Timer {
                        id: copiedTimer
                        interval: 1500
                        onTriggered: updateBanner.copied = false
                    }
                    ToolTip {
                        id: bannerTip
                        text: updateBanner.copied ? uiTr("Command copied")
                            : updateBanner.manual ? updateManager.manualInstallCommand
                            : updateBanner.notVerified ? updateManager.lastError
                            : (updateBanner.ready || updateBanner.ustate === UpdateManager.ExitPending)
                              && updateManager.lastError !== ""
                              ? uiTr("Last attempt failed") + ": " + updateManager.lastError
                            : updateBanner.ustate === UpdateManager.Failed
                              ? uiTr("Update failed") + ": " + updateManager.lastError
                            : ""
                        visible: bannerMa.containsMouse && text !== ""
                        delay: 400
                        font.pixelSize: YaycProperties.fsP1
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
                    objectName: "advancedRow"
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
                    objectName: "quitRow"
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
                MenuRow {
                    label: uiTr("Keep hover preview playing")
                    iconSource: "/icons/sliders.svg"
                    rowTooltip: uiTr("Keep thumbnail hover previews playing when YAYC loses focus or you switch virtual desktop. Also enables pinning a preview via right-click.")
                    rightItem: Switch {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: smenu.host ? smenu.host.keepForegroundIllusion : false
                        onToggled: if (smenu.host) smenu.host.keepForegroundIllusion = checked
                    }
                    onActivated: if (smenu.host) smenu.host.keepForegroundIllusion = !smenu.host.keepForegroundIllusion
                }
                MenuRow {
                    objectName: "autoUpdateRow"
                    label: uiTr("Automatic updates")
                    iconSource: "/icons/download_for_offline.svg"
                    rowTooltip: !updateManager.updateSupported
                        ? uiTr("Not available for this installation") + ": " + updateManager.unsupportedReason
                        : updateManager.updateMethod === UpdateConfig.ManualDeb
                          ? uiTr("Download new releases automatically; you install them with one terminal command")
                        : !updateManager.canStage
                          ? uiTr("Download new releases automatically; you install them with one click")
                          : uiTr("Download and install new releases automatically, when YAYC quits")
                    rowEnabled: updateManager.updateSupported
                    rightItem: Switch {
                        objectName: "autoUpdateSwitch"
                        anchors.verticalCenter: parent.verticalCenter
                        checked: smenu.host ? smenu.host.autoUpdate : true
                        onToggled: if (smenu.host) smenu.host.autoUpdate = checked
                    }
                    onActivated: if (smenu.host) smenu.host.autoUpdate = !smenu.host.autoUpdate
                }
                // Next step of the update, one button: check → download → apply
                MenuRow {
                    id: updateActionRow
                    objectName: "updateActionRow"
                    property int ustate: updateManager.state
                    property bool busy: updateManager.checking
                                        || ustate === UpdateManager.Downloading
                                        || ustate === UpdateManager.Verifying
                                        || ustate === UpdateManager.Applying
                    property string message: ""
                    // A ready package older than the release: the newer one first
                    // (the banner still offers to apply the older one)
                    property string action: busy ? "busy"
                        : (ustate === UpdateManager.RestartPending || ustate === UpdateManager.ExitPending) ? "restart"
                        : offer.newerThanReady ? "download"
                        : ustate === UpdateManager.ReadyToInstall ? "apply"
                        : ustate === UpdateManager.NotVerified ? "verify"
                        : ustate === UpdateManager.ManualInstall ? "copy"
                        : offer.fromIdle ? "download"
                        : "check"

                    label: {
                        // Short feedback after a check or a copy, then the next step again
                        if (message !== "" && action !== "busy")
                            return message
                        if (action === "busy")
                            return updateManager.checking ? uiTr("Checking for updates") + "..."
                                 : ustate === UpdateManager.Downloading ? uiTr("Downloading update") + "..."
                                 : ustate === UpdateManager.Verifying ? uiTr("Verifying update") + "..."
                                 : uiTr("Installing update") + "..."
                        if (action === "restart")
                            return uiTr("Restart to update")
                        if (action === "apply")
                            return uiTr("Apply update") + " v" + updateManager.readyVersion
                        if (action === "verify")
                            return uiTr("Retry check")
                        if (action === "copy")
                            return uiTr("Copy install command")
                        if (action === "download")
                            return uiTr("Download update") + " v" + smenu.latestRemoteVersionStr
                        return uiTr("Check for updates")
                    }
                    iconSource: "/icons/refresh.svg"
                    // Unsupported installs only reach "check", which works everywhere
                    rowEnabled: action !== "busy"
                    rowTooltip: action === "busy"
                        ? uiTr("An update is being checked, downloaded or installed")
                        : ""
                    onActivated: {
                        message = ""
                        if (action === "restart") {
                            if (updateManager.prepareRestart())
                                win.restart()
                            else
                                message = updateManager.lastError
                        }
                        else if (action === "apply")
                            updateManager.applyUpdate()
                        else if (action === "verify")
                            updateManager.retry()
                        else if (action === "copy") {
                            utilities.copyText(updateManager.manualInstallCommand)
                            message = uiTr("Command copied")
                            messageTimer.restart()
                        } else if (action === "download")
                            updateManager.startUpdate(smenu.latestRemoteVersionStr)
                        else if (action === "check")
                            updateManager.checkForUpdates()
                    }
                    Connections {
                        target: updateManager
                        function onUpToDate() {
                            updateActionRow.message = uiTr("YAYC is up to date")
                            messageTimer.restart()
                        }
                        function onCheckFailed(reason) {
                            updateActionRow.message = uiTr("Update check failed") + ": " + reason
                            messageTimer.restart()
                        }
                    }
                    Timer {
                        id: messageTimer
                        interval: 3000
                        onTriggered: updateActionRow.message = ""
                    }
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
