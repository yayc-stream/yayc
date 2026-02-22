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
import Qt.labs.platform as QLP
import Qt5Compat.GraphicalEffects
import yayc 1.0

Item {
    id: root
    objectName: "MainYayc"

    property var webEngineView: webEngineViewLoader.item
    property url url: "https://youtube.com"
    property url previousUrl: ""
    property bool filesystemModelReady: false
    property bool windowHidden: win.hidden

    function prepareQuit() {
        // the setting is an alias for reloading purposes
        if (root.windowHidden && root.blankWhenHidden && root.previousUrl !== "") {
            settings.lastUrl = root.previousUrl // ToDo: try to save/restore position too
        } else if (webEngineView) {
            settings.lastUrl = webEngineView.timePuller.getCurrentVideoURLWithPosition()
        }
        syncAll()
    }

    function quit() {
        root.prepareQuit()
        win.quitting = true
        Qt.quit()
    }

    function minimizeToTray() {
        syncAll()
        win.hide()
    }

    Shortcut {
        sequence: "Ctrl+P"
        onActivated: {
            if (root.debugMode) {
                utilities.printSettingsPath()
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+F"
        onActivated: {
            utilities.fetchMissingThumbnails()
        }
    }

    Shortcut {
        sequence: "F5"
        onActivated: {
            resetFilesystemModels()
        }
    }

    QLP.SystemTrayIcon {
        visible: true
        icon.source: "qrc:/images/yayc-alt.png"
        menu: QLP.Menu {
            QLP.MenuItem {
                text: (win.visibility == Window.Hidden)
                        ? qsTr("Show")
                        : qsTr("Minimize to Tray")
                onTriggered: {
                    if (win.visibility == Window.Hidden) {
                        win.show()
                        win.raise()
                    } else {
                        root.minimizeToTray()
                    }
                }
            }
            QLP.MenuItem {
                text: qsTr("Quit")
                onTriggered: {
                    win.quitApp()
                }
            }
        }
        onActivated: (reason) =>{
            if (reason == QLP.SystemTrayIcon.Trigger) {
                if (win.visible)
                    win.hide()
                else
                    win.show()
            }
        }
    }

    property string profilePath: WebBrowsingProfiles.profilePath //cant alias a property of a different component
    property string youtubePath
    property string historyPath
    property string easyListPath
    property string extWorkingDirPath
    property bool extWorkingDirExists: root.extWorkingDirPath !== ""
    property bool extCommandEnabled: (root.extWorkingDirExists
                                      && root.externalCommands.length > 0
                                      && root.externalCommands[0].command !== "")

    property bool firstRun: true
    property bool limitationOfLiabilityAccepted: false

    property string lastestRemoteVersion: appVersion
    property var lastVersionCheckDate
    property string donateUrl
    property string donateUrlETag
    property string customScript
    property bool darkMode: true
    property bool debugMode: false
    property bool removeStorageOnDelete: false
    property bool blankWhenHidden: false
    property bool showCategoryBar: true
    property int homeGridColumns: 4
    property real wevZoomFactor
    property real wevZoomFactorVideo
    property real volume: 0
    property real userSpecifiedVolume: -1
    property bool muted: false
    property bool guideToggled: false

    property var externalCommands: []
    function pushEmptyCommand() {
        var empty = {name : "", command : ""}
        if (root.externalCommands.length !== 0
                && root.externalCommands[root.externalCommands.length - 1].name == ""
                && root.externalCommands[root.externalCommands.length - 1].command == "")
            return;
        var newCommands = root.externalCommands.slice()
        newCommands.push(empty)
        root.externalCommands = newCommands
    }
    function removeCommand(idx) {
        var newCommands = []
        for (var i = 0; i < root.externalCommands.length; i++) {
            if (i !== idx)
                newCommands.push(root.externalCommands[i])
        }
        root.externalCommands = newCommands
        if (newCommands.length == 0)
            pushEmptyCommand()
    }

    Binding { target: YaycProperties; property: "isDarkMode"; value: root.darkMode }
    onDarkModeChanged: {
        utilities.setColorScheme(root.darkMode)
        if (root.settingsLoaded && webEngineView)
            deferReloadAfterColorSchemeChange.restart()
    }
    Timer {
        id: deferReloadAfterColorSchemeChange
        interval: 4000
        onTriggered: if (webEngineView) webEngineView.reload()
    }

    property bool settingsLoaded: false
    Timer {
        id: timerSettings
        interval: 2000
        running: false
        repeat: false
        onTriggered: {
            root.settingsLoaded = true
        }
    }
    property bool settingsInitialized: false
    YaycSettings {
        id: settings
        location: configFileUrl
        property alias lolAccepted: root.limitationOfLiabilityAccepted
        property alias firstRun: root.firstRun
        property alias profilePath: root.profilePath
        property alias youtubePath: root.youtubePath
        property alias historyPath: root.historyPath
        property alias easyListPath: root.easyListPath
        property alias extWorkingDirPath: root.extWorkingDirPath
        property alias externalCommands: root.externalCommands
        property alias lastUrl: root.url
        property alias lastestRemoteVersion: root.lastestRemoteVersion
        property alias lastVersionCheckDate: root.lastVersionCheckDate
        property alias donateUrl: root.donateUrl
        property alias donateUrlETag: root.donateUrlETag
        property alias customScript: root.customScript
        property alias customScriptEnabled: buttonToggleJS.checked
        property alias darkMode: root.darkMode
        property alias debugMode: root.debugMode
        property alias wevZoomFactor: root.wevZoomFactor
        property alias wevZoomFactorVideo: root.wevZoomFactorVideo
        property alias removeStorageOnDelete: root.removeStorageOnDelete
        property alias blankWhenHidden: root.blankWhenHidden
        property alias showCategoryBar: root.showCategoryBar
        property alias homeGridColumns: root.homeGridColumns
        property alias volume: root.volume
        property alias userSpecifiedVolume: root.userSpecifiedVolume
        property alias guidePaneToggled: root.guideToggled
        property alias proxyType: proxyMenu.proxyType
        property alias proxyPort: proxyMenu.proxyPort
        property alias proxyHost: proxyMenu.proxyHost
        property var splitView

        onLoadedChanged: {
            if (!loaded || root.settingsInitialized)
                return
            root.settingsInitialized = true

            disclaimerContainer.visible = Qt.binding(function() { return !settings.lolAccepted })

            WebBrowsingProfiles.profilePath = Qt.binding(function() { return settings.profilePath })
            WebBrowsingProfiles.customScript = Qt.binding(function() { return settings.customScript })
            WebBrowsingProfiles.customScriptEnabled = Qt.binding(function() { return settings.customScriptEnabled })
            deferRecreateWebEngineProfiles.restart()

            // TODO: rework this
            timerSettings.start()
        }

        function updateWebEngineProfiles() {
            if (!root.settingsInitialized)
                return
            deferRecreateWebEngineProfiles.restart()
        }

        onCustomScriptChanged: settings.updateWebEngineProfiles()
        onCustomScriptEnabledChanged: settings.updateWebEngineProfiles()
        onProfilePathChanged: settings.updateWebEngineProfiles()
    }

    //FIXME figure the issue
    Timer {
        id: deferRecreateWebEngineProfiles
        interval: 500
        onTriggered: {
            WebBrowsingProfiles.recreateProfiles()
            // profile binding is handled by sourceComponent: profile: WebBrowsingProfiles.profile
        }
    }

    Component.onCompleted:  {
        utilities.networkFound.connect(onNetworkFound)
        utilities.latestVersion.connect(onLatestVersionFound)
        utilities.donateETag.connect(onDonateETag)
        utilities.donateUrl.connect(onDonateUrl)

        // Re-enable (maybe) after fixing the connections after deletion/re-instantiation of these models
        // fileSystemModel.directoryLoaded.connect(onFSmodelDirectoryLoaded)
        // fileSystemModel.filesAdded.connect(onFSModelFilesAdded)
        win.interfaceLoaded.connect(resetFilesystemModels)

        splitView.restoreState(settings.splitView)
        if (root.externalCommands.length == 0) {
            root.pushEmptyCommand()
        }
    }
    Component.onDestruction: {
        settings.splitView = splitView.saveState()
    }

    onYoutubePathChanged: { // this might be triggering double setRoot. move it into fileDialog?
        settings.sync()
        if (youtubePath !== "" && win.isInterfaceLoaded) {
            fileSystemModel.setRoot(youtubePath)
        }
    }

    onHistoryPathChanged: {
        settings.sync()
        if (historyPath !== "" && win.isInterfaceLoaded) {
            historyModel.setRoot(historyPath)
        }
    }

    Connections {
        target: WebBrowsingProfiles
        function onProfileChanged() {
            if (webEngineView)
                webEngineView.reload()
            settings.sync()
        }
    }

    Connections {
        target: bookmarksContainer
        function onVideoSelected(url_) {
            root.url = url_
        }
    }

    Connections {
        target: historyContainer
        function onVideoSelected(url_) {
            root.url = url_
        }
    }

    // zoomFactorSyncer timer removed: syncZoomFactor is now called
    // reactively via webViewSync Connections onZoomFactorChanged

    Timer {
        id: fileSystemSyncer // will sync only dirty entries
        repeat: true
        running: true
        interval: 1000 * 60
        onTriggered: {
            syncAll()
        }
    }

    Timer {
        id: networkChecker
        repeat: true
        running: true
        interval: 1000 * 60 * 10 // 10 min
        onTriggered: {
            utilities.checkConnectivity()
        }
    }

    function onNetworkFound() {
        var now = new Date() // Current date now.
        if (typeof(root.lastVersionCheckDate) !== "undefined") {
            var diff = (now - root.lastVersionCheckDate); // Difference in milliseconds.
            var diffSeconds = parseInt(diff/1000);
            var intervalSeconds = 3600 * 2 // don't check more often than once per 2h
            if (diffSeconds < intervalSeconds) {
                return;
            }
        }
        // Kick version checker
        utilities.getLatestVersion()
        utilities.getDonateEtag()
    }

    function onLatestVersionFound(latestVersion) {
        var now = new Date()
        root.lastVersionCheckDate = now
        var previousRemoteVersion = root.lastestRemoteVersion
        root.lastestRemoteVersion = latestVersion

        var res = utilities.compareSemver(previousRemoteVersion, latestVersion)
        if (res === -1) { // if latest is greater
            // highlight settings
            root.firstRun = true
        }
    }


    function resetFilesystemModels() {
        clearFilesystemModels()
        Qt.callLater(setFilesystemModels)
    }

    function clearFilesystemModels() {
        bookmarksContainer.clearModel()
        historyContainer.clearModel()
        fileSystemModel.setRoot("")
        historyModel.setRoot("")
    }

    function setFilesystemModels() {
        if (youtubePath !== "")
            fileSystemModel.setRoot(youtubePath)
        if (historyPath !== "") {
            historyModel.setRoot(historyPath)
        }
        bookmarksContainer.setModel()
        historyContainer.setModel()
    }

    function onDonateETag(latestETag) {
        if (latestETag === root.donateUrlETag)
            return;
        root.donateUrlETag = latestETag
        utilities.getDonateURL()
    }

    function onDonateUrl(latestDonateUrl) {
        if (latestDonateUrl === root.donateUrl)
            return;
        root.donateUrl = latestDonateUrl
        root.firstRun = true
    }

    function syncAll() {
        syncZoomFactor()
        fileSystemModel.sync()
        historyModel.sync();
        settings.sync()
    }

    function syncZoomFactor() {
        if (!root.settingsLoaded || !webEngineView)
            return
        var newZoom = webEngineView.zoomFactor
        if (webEngineView.isYoutubeVideo) {
            if (root.wevZoomFactorVideo !== newZoom)
                root.wevZoomFactorVideo = newZoom
        } else {
            if (root.wevZoomFactor !== newZoom)
                root.wevZoomFactor = newZoom
        }
    }

    function deUrlizePath(path) {
        path = path.slice(7) // strip file://
        if (Qt.platform.os === "windows" &&
                path[0] === '/') {
            path = path.slice(1)
        }
        return path
    }

    Item { id: dummy } // Workaround for QTBUG-59940

    function onFSmodelDirectoryLoaded(path) {
        // console.log( "directoryLoaded ", path );
    }
    function onFSModelFilesAdded(paths) {
        if (!root.filesystemModelReady) return;
        // console.log( "onFSModelRowInserted ", paths );
    }
    function onYoutubeUrlRequested(u) {
        // console.log("onYoutubeUrlRequested ",u)
    }

    property string httpUserAgent: "'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36'"
    property string httpAcceptLanguage: "en-US"

    FontLoader {
        id: mainFont
        source: "qrc:/fonts/NotoSansDisplay-VariableFont_wdth,wght.ttf"
    }
    FontLoader {
        id: emojiFont
        source: "qrc:/fonts/NotoEmoji-VariableFont_wght.ttf"

    }

    Item {
        id: mainContainer
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            top: toolBar.bottom
        }

        SplitView {
            id: splitView
            anchors.fill: parent

            BookmarksTreeView {
                id: bookmarksContainer
                historyView: false
                width: 200
                implicitWidth: 200
                showFiltering: bookmarksToolButton.searching

                extWorkingDirExists: root.extWorkingDirExists
                extWorkingDirPath: root.extWorkingDirPath
                externalCommands: root.externalCommands
                removeStorageOnDelete: root.removeStorageOnDelete
                extCommandEnabled: root.extCommandEnabled
                webEngineViewKey: webEngineView ? webEngineView.key : ""
            }
            BookmarksTreeView {
                id: historyContainer
                visible: false
                historyView: true
                width: 200
                implicitWidth: 200
                showFiltering: historyToolButton.searching

                extWorkingDirExists: root.extWorkingDirExists
                extWorkingDirPath: root.extWorkingDirPath
                externalCommands: root.externalCommands
                removeStorageOnDelete: root.removeStorageOnDelete
                extCommandEnabled: root.extCommandEnabled
                webEngineViewKey: webEngineView ? webEngineView.key : ""
            }

            Item {
                id: webViewWrapper
                SplitView.minimumWidth: 200
                SplitView.fillWidth: true

                anchors {
                    top: parent.top
                    bottom: parent.bottom
                }

                property bool videoPlaying: false

                Loader {
                    id: webEngineViewLoader
                    anchors.fill: parent
                    asynchronous: true

                    // Desired state: profile exists AND (window visible OR audio playing)
                    property bool shouldBeActive: WebBrowsingProfiles.profile !== null
                                                   && (!root.windowHidden || webViewWrapper.videoPlaying)
                    onShouldBeActiveChanged: {
                        if (shouldBeActive) {
                            webEngineViewLoaderDeactivateTimer.stop()
                            webEngineViewLoaderActivateTimer.restart()
                        } else {
                            webEngineViewLoaderActivateTimer.stop()
                            webEngineViewLoaderDeactivateTimer.restart()
                        }
                    }
                    Timer {
                        id: webEngineViewLoaderActivateTimer
                        interval: 500
                        onTriggered: webEngineViewLoader.active = true
                    }
                    Timer {
                        id: webEngineViewLoaderDeactivateTimer
                        interval: 10 * 1000
                        onTriggered: webEngineViewLoader.active = false
                    }
                    active: false // initial; managed imperatively above

                    onLoaded: {
                        item.webViewTools.parent = webViewToolsContainer
                    }
                    onItemChanged: if (!item) webViewWrapper.videoPlaying = false

                    sourceComponent: WebView {
                        // Declarative bindings: re-applied on every Loader activation.
                        // User navigation / slider changes break these bindings,
                        // but webViewSync pull Connections keeps root in sync.
                        url: root.url
                        previousUrl: root.previousUrl
                        profile: WebBrowsingProfiles.profile
                        volume: root.volume
                        userSpecifiedVolume: root.userSpecifiedVolume
                        guideToggled: root.guideToggled
                        showCategoryBar: root.showCategoryBar
                        homeGridColumns: root.homeGridColumns

                        enabled: true
                        visible: enabled

                        // required properties (one-way, parent -> child)
                        customScript: root.customScript
                        wevZoomFactor: root.wevZoomFactor
                        historyPath: root.historyPath
                        youtubePath: root.youtubePath
                        extWorkingDirPath: root.extWorkingDirPath
                        wevZoomFactorVideo: root.wevZoomFactorVideo
                        easyListPath: root.easyListPath
                        profilePath: root.profilePath
                        extWorkingDirExists: root.extWorkingDirExists
                        externalCommands: root.externalCommands
                        removeStorageOnDelete: root.removeStorageOnDelete
                        extCommandEnabled: root.extCommandEnabled
                    }

                    // Keep videoPlaying in sync for Loader active condition
                    Connections {
                        target: webEngineViewLoader.item
                        ignoreUnknownSignals: true
                        function onIsYoutubeVideoChanged() {
                            webViewWrapper.videoPlaying = webEngineViewLoader.item.isYoutubeVideo
                        }
                    }
                } // Loader

                Item {
                    anchors.fill: parent
                    visible: !webEngineViewLoader.item
                    Text {
                        anchors.centerIn: parent
                        text: "Loading..."
                        color: YaycProperties.textColor
                        font.pixelSize: YaycProperties.fsH2
                    }
                }
            } // webViewWrapper

            // Bidirectional property sync between root and WebView (Loader item).
            // 1. Initial: sourceComponent bindings set WebView props from root on each load.
            // 2. Child->parent: pull Connections copy WebView changes back to root.
            // 3. Parent->child: push aliases detect root changes, imperatively assign to WebView.
            //    (needed after WebView breaks the declarative binding via internal assignment)
            // !== guards in both directions prevent infinite loops.
            // On unload root retains values; on reload sourceComponent re-applies them.
            QtObject {
                id: webViewSync

                // Push (parent -> child)
                property alias url: root.url
                onUrlChanged: if (webEngineView && webEngineView.url !== url)
                                  webEngineView.url = url

                property alias volume: root.volume
                onVolumeChanged: if (webEngineView && webEngineView.volume !== volume)
                                     webEngineView.volume = volume

                property alias userSpecifiedVolume: root.userSpecifiedVolume
                onUserSpecifiedVolumeChanged: if (webEngineView && webEngineView.userSpecifiedVolume !== userSpecifiedVolume)
                                                  webEngineView.userSpecifiedVolume = userSpecifiedVolume

                // Pull (child -> parent)
                property Connections connections : Connections {
                    target: webEngineView
                    function onUrlChanged() {
                        if (root.url !== webEngineView.url)
                            root.url = webEngineView.url
                    }
                    function onPreviousUrlChanged() {
                        if (root.previousUrl !== webEngineView.previousUrl)
                            root.previousUrl = webEngineView.previousUrl
                    }
                    function onVolumeChanged() {
                        if (root.volume !== webEngineView.volume)
                            root.volume = webEngineView.volume
                    }
                    function onUserSpecifiedVolumeChanged() {
                        if (root.userSpecifiedVolume !== webEngineView.userSpecifiedVolume)
                            root.userSpecifiedVolume = webEngineView.userSpecifiedVolume
                    }
                    function onGuideToggledChanged() {
                        if (root.guideToggled !== webEngineView.guideToggled)
                            root.guideToggled = webEngineView.guideToggled
                    }
                    function onShowCategoryBarChanged() {
                        if (root.showCategoryBar !== webEngineView.showCategoryBar)
                            root.showCategoryBar = webEngineView.showCategoryBar
                    }
                    function onHomeGridColumnsChanged() {
                        if (root.homeGridColumns !== webEngineView.homeGridColumns)
                            root.homeGridColumns = webEngineView.homeGridColumns
                    }
                    function onZoomFactorChanged() {
                        syncZoomFactor()
                    }
                }
            }
        }
    } // mainContainer

    ToolBar {
        id: toolBar
        anchors {
            left: parent.left
            top: parent.top
            right: parent.right
        }
        ColumnLayout {
            anchors.fill: parent
            RowLayout {
                Layout.alignment: Qt.AlignVCenter | Qt.AlignJustify
                Layout.fillWidth: true
                Layout.fillHeight: true
                id: navigationBar

                RowLayout {
                    id: staticControlsLeft

                    ToolButton {
                        property int itemAction: WebEngineView.Back
                        text: webEngineView ? webEngineView.action(itemAction).text : ""
                        enabled: webEngineView ? webEngineView.action(itemAction).enabled : false
                        onClicked: if (webEngineView) webEngineView.action(itemAction).trigger()
                        icon.source: "/icons/arrow_back.svg"
                        display: AbstractButton.IconOnly //TextUnderIcon

                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: "Go Back (long press for history)"
                        ToolTip.delay: 300
                        ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2

                        TapHandler {
                            onLongPressed: {
                                historyToolButton.checked_ = !historyToolButton.checked_
                                historyContainer.visible = historyToolButton.checked_
                            }
                        }
                    }

                    ToolButton {
                        property int itemAction: WebEngineView.Forward
                        text: webEngineView ? webEngineView.action(itemAction).text : ""
                        enabled: webEngineView ? webEngineView.action(itemAction).enabled : false
                        onClicked: if (webEngineView) webEngineView.action(itemAction).trigger()
                        icon.source: "/icons/arrow_forward.svg"
                        display: AbstractButton.IconOnly

                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: "Go Forward (long press for history)"
                        ToolTip.delay: 300
                        ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2

                        TapHandler {
                            onLongPressed: {
                                historyToolButton.checked_ = !historyToolButton.checked_
                                historyContainer.visible = historyToolButton.checked_
                            }
                        }
                    }

                    ToolButton {
                        text: "Bookmarks"
                        id: bookmarksToolButton
                        enabled: true
                        checkable: false
                        property bool checked_: true // bypassing built in checkable to make it tristate
                        property bool searching: false
                        onClicked: {
                            if (checked_)
                                if (!searching)
                                    searching = true
                                else
                                    checked_ = searching = false
                            else
                                checked_ = true

                            bookmarksContainer.visible = checked_
                        }

                        icon {
                            source: "/icons/bookmarks.svg"
                            color: (checked_)
                                   ? YaycProperties.checkedButtonColor
                                   : YaycProperties.iconColor
                        }
                        display: AbstractButton.IconOnly

                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: (checked_)
                                      ? "Hide bookmarks pane"
                                      : "Show bookmarks pane"
                                      ? "Hide bookmarks pane"
                                      : "Show bookmarks pane"
                        ToolTip.delay: 300
                        ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2

                        Image {
                            id: bookmarksToolButtonOverlay
                            anchors {
                                left: parent.horizontalCenter
                                right: parent.right
                                top: parent.verticalCenter
                                bottom: parent.bottom
                                leftMargin: 4
                                topMargin: 4
                            }

                            source: "/icons/search.svg"
                            enabled: true
                            visible: parent.searching
                            z: parent.z + 1

                            layer.enabled: true
                            layer.effect: ColorOverlay {
                                source: bookmarksToolButtonOverlay
                                anchors.fill: bookmarksToolButtonOverlay
                                color: YaycProperties.iconColor
                                visible: true
                            }
                        }
                    }

                    ToolButton {
                        text: "History"
                        id: historyToolButton
                        enabled: true
                        checkable: false
                        property bool checked_: false // bypassing built in checkable to make it tristate
                        property bool searching: false
                        onClicked: {
                            if (checked_)
                                if (!searching)
                                    searching = true
                                else
                                    checked_ = searching = false
                            else
                                checked_ = true

                            historyContainer.visible = checked_
                        }

                        icon {
                            source: "/icons/event_repeat.svg"
                            color: (checked_)
                                   ? YaycProperties.checkedButtonColor
                                   : YaycProperties.iconColor
                        }

                        display: AbstractButton.IconOnly

                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: (checked_)
                                      ? "Hide history pane"
                                      : "Show history pane"
                                      ? "Hide history pane"
                                      : "Show history pane"
                        ToolTip.delay: 300
                        ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2

                        Image {
                            id: historyToolButtonOverlay
                            anchors {
                                left: parent.horizontalCenter
                                right: parent.right
                                top: parent.verticalCenter
                                bottom: parent.bottom
                                leftMargin: 4
                                topMargin: 4
                            }

                            source: "/icons/search.svg"
                            enabled: true
                            visible: parent.searching
                            z: parent.z + 1

                            layer.enabled: true
                            layer.effect: ColorOverlay {
                                source: historyToolButtonOverlay
                                anchors.fill: historyToolButtonOverlay
                                color: YaycProperties.iconColor
                                visible: true
                            }
                        }
                    }

                    ToolButton {
                        id: reloadButton
                        property bool wevLoading: webEngineView ? webEngineView.loading : false
                        property int itemAction: wevLoading ? WebEngineView.Stop : WebEngineView.Reload
                        text: webEngineView ? webEngineView.action(itemAction).text : ""
                        enabled: webEngineView ? webEngineView.action(itemAction).enabled : false
                        onClicked: if (webEngineView) webEngineView.action(itemAction).trigger()
                        icon.source: "/icons/" + (wevLoading ? "cancel.svg" : "refresh.svg")
                        display: AbstractButton.IconOnly

                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: wevLoading ? "Stop" : "Reload"
                        ToolTip.delay: 300
                        ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                    }

                    ToolButton {
                        id: buttonHome
                        onClicked: root.url = "https://www.youtube.com"
                        icon.source: "/icons/home.svg"
                        display: AbstractButton.IconOnly
                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: "Go to YouTube Home"
                        ToolTip.delay: 300
                        ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                    }
                }
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    id: webViewToolsContainer
                }
                RowLayout {
                    id: rightStaticTools

                    ToolButton {
                        id: buttonToggleJS
                        text: "Activate/Deactivate custom script"
                        enabled: settings.customScript !== ""
                        visible: enabled
                        checkable: true
                        checked: true

                        icon.source: "/icons/js.svg"
                        display: AbstractButton.IconOnly

                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: "Toggle custom script"
                        ToolTip.delay: 300
                        ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                    }

                    ToolButton {
                        id: settingsButton
                        text: "Settings"
                        icon.source: "/icons/settings.svg"
                        display: AbstractButton.IconOnly

                        onClicked: {
                            root.firstRun = false
                            settingsGlitter.enabled = false
                            settingsMenu.open()
                        }

                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: "Open Settings Panel"
                        ToolTip.delay: 300
                        ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                        AnimatedImage {
                            id: settingsGlitter
                            anchors {
                                left: parent.left
                                right: parent.right
                                top: parent.top
                                bottom: parent.bottom
                                leftMargin: 2
                                rightMargin: 2
                                topMargin: 2
                                bottomMargin: 2
                            }

                            source: "/images/glitter-2.webp"
                            enabled: root.firstRun
                            playing: enabled
                            visible: enabled
                            speed: 0.5

                            layer.enabled: true
                            layer.mipmap: true
                            // TODO: fix this -- QTBUG-87402
    //                        layer.effect: ShaderEffect {
    //                            fragmentShader: "
    //                                uniform lowp sampler2D source; // this item
    //                                uniform lowp float qt_Opacity; // inherited opacity of this item
    //                                varying highp vec2 qt_TexCoord0;
    //                                void main() {
    //                                    lowp vec4 p = texture2D(source, qt_TexCoord0);
    //                                    if (p.a < .1)
    //                                        gl_FragColor = vec4(0, 0, 0, 0);
    //                                    else
    //                                        gl_FragColor = vec4(1, 0.9, 0, p.a);
    //                                }"
    //                        }
                        }
                    }
                } // RowLayout right static tools
            }
        }
    } // header


    Dialog {
        id: proxyMenu
        x: (parent.width - width) * 0.5
        y: (parent.height - height) * 0.5
        width: 800
        height: 300
        visible: false
        modal: true

        header: Item {
            width: aboutContainer.width
            height: YaycProperties.fsH3 * 1.5
            Label {
                anchors {
                    topMargin: 4
                    centerIn: parent
                }
                text: "<b>Proxy Settings</b>"
                font.pixelSize: YaycProperties.fsH3
            }
        }
        footer: DialogButtonBox {
            standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
        }

        property string proxyType: "none"
        property string proxyHost: ""
        property int proxyPort: 0

        GridLayout {
            anchors.fill: parent
            columns: 2

            Label {
                text: "Proxy Type:"
            }

            ComboBox {
                id: proxyTypeComboBox
                model: ["None", "HTTP", "SOCKS5"]
                Layout.fillWidth: true
                onCurrentTextChanged: {
                    proxyMenu.proxyType = currentText.toLowerCase()
                }
            }

            GroupBox {
                id: proxySettingsGroup
                Layout.columnSpan: 2
                Layout.fillWidth: true
                enabled: proxyMenu.proxyType !== "none"

                GridLayout {
                    anchors.fill: parent
                    columns: 2

                    Label {
                        text: "Host:"
                    }

                    TextField {
                        id: hostTextField
                        Layout.fillWidth: true
                        placeholderText: "Enter proxy host"
                        onTextChanged: proxyMenu.proxyHost = text
                    }

                    Label {
                        text: "Port:"
                    }

                    SpinBox {
                        id: portSpinBox
                        editable: true
                        Layout.fillWidth: true
                        from: 0
                        to: 65535
                        value: 0
                        onValueChanged: proxyMenu.proxyPort = value
                    }
                }
            }
        }

        onAccepted: {
            utilities.setNetworkProxy(proxyType, proxyHost, proxyPort)
        }

        onRejected: {
            close()
        }
    }

    Dialog {
        id: settingsMenu
        x: (parent.width - width) * 0.5
        y: (parent.height - height) * 0.5
        width: 800
        visible: false
        modal: true
        header: Item {
                    width: aboutContainer.width
                    height: YaycProperties.fsH3 * 1.5
                    Label {
                        anchors {
                            topMargin: 4
                            centerIn: parent
                        }
                        text: "<b>Settings</b>"
                        font.pixelSize: YaycProperties.fsH3
                    }
                }
        footer: RowLayout {
            Button {
                Layout.alignment: Qt.AlignLeft
                Layout.leftMargin: 8
                text: qsTr("Quit")
                onClicked: root.quit()

                hoverEnabled: true
                ToolTip.visible: hovered
                ToolTip.delay: 100
                ToolTip.text: "Exit YAYC\nCtrl+Q does it too"
                ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
            }
            Button {
                Layout.alignment: Qt.AlignRight
                Layout.rightMargin: 8
                text: qsTr("Close")
                onClicked: {
                    root.externalCommands = root.externalCommands // hack to push notifications
                    settingsMenu.close()
                }

                hoverEnabled: true
                ToolTip.visible: hovered
                ToolTip.delay: 100
                ToolTip.text: "Close settings"
                ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
            }
        }
        ColumnLayout {
            id: settingsMain
            anchors.fill: parent
            Item {
                height: 1
                width: settingsMenu.width - 32
            }
            Rectangle {
                id: settingsScrollViewContainer
                Layout.fillWidth: true

                height: 260
                color: YaycProperties.surfaceOverlayColor
                border.color: "transparent"
                radius: 6

                ScrollView {
                    id: scrollViewSettingsDirectories
                    anchors.fill: parent
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    Component.onCompleted: {
                        scrollViewSettingsDirectories.contentItem.boundsBehavior = Flickable.StopAtBounds
                    }

                    ColumnLayout {
                        width: settingsScrollViewContainer.width - scrollViewSettingsDirectories.ScrollBar.vertical.width
                        spacing: 16
                        GridLayout {
                            width: parent.width
                            columns: 8
                            rowSpacing: 16
                            columnSpacing: 16

                            // bookmarks
                            Image {
                                width: 32
                                height: 32
                                Layout.preferredWidth: width
                                Layout.preferredHeight: height
                                source: "qrc:/images/youtube-128.png"
                                Layout.alignment: Qt.AlignVCenter

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                        property bool hovered: false
                                        onEntered:  hovered = true
                                        onExited: hovered = false

                                    ToolTip {
                                        visible: parent.hovered
                                        y: parent.height * 0.12
                                        font.pixelSize: YaycProperties.fsP2
                                        text: "Bookmarks data path:\n" + root.youtubePath
                                        delay: 300
                                    }
                                }
                            }
                            Label {
                                text: "Bookmarks:"
                                Layout.alignment: Qt.AlignVCenter
                            }
                            Label {
                                Layout.columnSpan: 4
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignLeft
                                text: (root.youtubePath === "") ? "<undefined>" : root.youtubePath
                            }
                            Button {
                                flat: true
                                display: Button.IconOnly
                                icon.source: "/icons/folder_open.svg"
                                Layout.alignment: Qt.AlignVCenter
                                onClicked: fileDialogVideos.open()
                                hoverEnabled: true

                                ToolTip.visible: hovered
                                ToolTip.delay: 300
                                ToolTip.text: "Bookmarks data path:\n" + root.youtubePath
                                ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                            }
                            Button {
                                flat: true
                                display: Button.IconOnly
                                icon.source: "/icons/delete_forever.svg"
                                Layout.alignment: Qt.AlignVCenter
                                Layout.leftMargin: -16
                                Layout.rightMargin: 0
                                onClicked: root.youtubePath = ""
                                hoverEnabled: true

                                ToolTip.visible: hovered
                                ToolTip.delay: 300
                                ToolTip.text: "Clear bookmarks data path"
                                ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                            }

                            // history
                            Item {
                                width: 32
                                height: 32
                                Layout.preferredWidth: width
                                Layout.preferredHeight: height
                                Layout.alignment: Qt.AlignVCenter
                                Image {
                                    id: histimg
                                    source: "/icons/history.svg"
                                    visible: false
                                    anchors.fill: parent
                                }
                                ColorOverlay {
                                    source: histimg
                                    anchors.fill: histimg
                                    color: YaycProperties.iconColor
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    property bool hovered: false
                                    onEntered:  hovered = true
                                    onExited: hovered = false

                                    ToolTip {
                                        visible: parent.hovered
                                        y: parent.height * 0.12
                                        font.pixelSize: YaycProperties.fsP2
                                        text: "YouTube history path:\n" + root.historyPath
                                        delay: 300
                                    }
                                }
                            }
                            Label {
                                text: "History:"
                                Layout.alignment: Qt.AlignVCenter
                            }
                            Label {
                                Layout.columnSpan: 4
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignLeft
                                text:  (root.historyPath === "") ? "<undefined>" : root.historyPath
                            }
                            Button {
                                flat: true
                                display: Button.IconOnly
                                icon.source: "/icons/folder_open.svg"
                                Layout.alignment: Qt.AlignVCenter
                                onClicked: fileDialogHistory.open()
                                hoverEnabled: true

                                ToolTip.visible: hovered
                                ToolTip.delay: 300
                                ToolTip.text: "YouTube history path:\n" + root.historyPath
                                ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                            }
                            Button {
                                flat: true
                                display: Button.IconOnly
                                icon.source: "/icons/delete_forever.svg"
                                Layout.alignment: Qt.AlignVCenter
                                Layout.leftMargin: -16
                                Layout.rightMargin: 0
                                onClicked: root.historyPath = ""
                                hoverEnabled: true

                                ToolTip.visible: hovered
                                ToolTip.delay: 300
                                ToolTip.text: "Clear YouTube history path"
                                ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                            }

                            // chromium profile
                            Image {
                                width: 32
                                height: 32
                                Layout.preferredWidth: width
                                Layout.preferredHeight: height
                                Layout.alignment: Qt.AlignVCenter
                                source: "qrc:/images/google-chrome-is.svg"

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    property bool hovered: false
                                    onEntered:  hovered = true
                                    onExited: hovered = false

                                    ToolTip {
                                        visible: parent.hovered
                                        y: parent.height * 0.12
                                        font.pixelSize: YaycProperties.fsP2
                                        text: "Chromium cookies path:\n" + WebBrowsingProfiles.profilePath
                                        delay: 300
                                    }
                                }
                            }
                            Label {
                                text: "Profile:"
                                Layout.alignment: Qt.AlignVCenter
                            }
                            Label {
                                Layout.columnSpan: 4
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignLeft
                                text: (WebBrowsingProfiles.profilePath === "")
                                      ? "<undefined>" : WebBrowsingProfiles.profilePath
                            }
                            Button {
                                id: buttonOpenGProfile
                                flat: true
                                display: Button.IconOnly
                                icon.source: "/icons/folder_open.svg"
                                Layout.alignment: Qt.AlignVCenter
                                onClicked: fileDialogProfile.open()
                                hoverEnabled: true

                                ToolTip.visible: hovered
                                ToolTip.delay: 300
                                ToolTip.text: "Chromium cookies path:\n" + WebBrowsingProfiles.profilePath
                                ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                            }
                            Button {
                                flat: true
                                display: Button.IconOnly
                                icon.source: "/icons/delete_forever.svg"
                                Layout.alignment: Qt.AlignVCenter
                                Layout.leftMargin: -16
                                Layout.rightMargin: 0
                                onClicked: WebBrowsingProfiles.profilePath = ""
                                hoverEnabled: true

                                ToolTip.visible: hovered
                                ToolTip.delay: 300
                                ToolTip.text: "Clear Chromium cookies path"
                                ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                            }
                        } // GridLayout
                        GridLayout {
                            width: parent.width
                            columns: 8
                            rowSpacing: 16
                            columnSpacing: 16
                            visible: root.debugMode

                            // external workdir
                            Item {
                                width: 32
                                height: 32
                                Layout.preferredWidth: width
                                Layout.preferredHeight: height
                                Layout.alignment: Qt.AlignVCenter
                                Image {
                                    id: extworkdirimg
                                    source: "/icons/exit_to_app.svg"
                                    visible: false
                                    anchors.fill: parent
                                }
                                ColorOverlay {
                                    source: extworkdirimg
                                    anchors.fill: extworkdirimg
                                    color: YaycProperties.iconColor
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    property bool hovered: false
                                    onEntered:  hovered = true
                                    onExited: hovered = false

                                    ToolTip {
                                        visible: parent.hovered
                                        y: parent.height * 0.12
                                        font.pixelSize: YaycProperties.fsP2
                                        text: "Working directory for external executable:\n" + root.extWorkingDirPath
                                        delay: 300
                                    }
                                }
                            }
                            Label {
                                text: "Ext Working Dir:"
                                Layout.alignment: Qt.AlignVCenter
                            }
                            Label {
                                Layout.columnSpan: 4
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignLeft
                                text: (!root.extWorkingDirExists) ? "<undefined>" : root.extWorkingDirPath
                            }
                            Button {
                                id: buttonOpenExternalWorkingDir
                                flat: true
                                display: Button.IconOnly
                                icon.source: "/icons/folder_open.svg"
                                Layout.alignment: Qt.AlignVCenter
                                onClicked: fileDialogExtWorkingDir.open()
                                hoverEnabled: true

                                ToolTip.visible: hovered
                                ToolTip.delay: 300
                                ToolTip.text: "Working directory for external executable:\n" + root.extWorkingDirPath
                                ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                            }
                            Button {
                                flat: true
                                display: Button.IconOnly
                                icon.source: "/icons/delete_forever.svg"
                                Layout.alignment: Qt.AlignVCenter
                                Layout.leftMargin: -16
                                Layout.rightMargin: 0
                                onClicked: root.extWorkingDirPath = ""
                                hoverEnabled: true

                                ToolTip.visible: hovered
                                ToolTip.delay: 300
                                ToolTip.text: "Clear external executable working directory path"
                                ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                            }
                            // external workdir end

                            // external app
                            Repeater {
                                id: extCommandsRepeater
                                model: root.externalCommands
                                delegate: RowLayout {
                                    Layout.columnSpan: 8
                                    Layout.alignment: Qt.AlignLeft
                                    Layout.fillWidth: true
                                    spacing: 10

                                    Item {
                                        visible: root.extWorkingDirExists
                                        width: 32
                                        height: 32
                                        Layout.preferredWidth: width
                                        Layout.preferredHeight: height
                                        Layout.alignment: Qt.AlignVCenter
                                        Image {
                                            id: extcmdimg
                                            source: "/icons/extension.svg"
                                            visible: false
                                            anchors.fill: parent
                                        }
                                        ColorOverlay {
                                            source: extcmdimg
                                            anchors.fill: extcmdimg
                                            color: YaycProperties.iconColor
                                        }
                                    }
                                    Label {
                                        visible: root.extWorkingDirExists
                                        text: "External cmd:"
                                        Layout.alignment: Qt.AlignVCenter
                                    }

                                    TextField {
                                        id: extCmdName
                                        width: 80
                                        focus: true
                                        selectByMouse: true
                                        font.pixelSize: YaycProperties.fsP1
                                        cursorVisible: true
                                        color: YaycProperties.textColor

                                        text: modelData.name
                                        onTextChanged: {
                                            root.externalCommands[index].name = text
                                        }

                                        ToolTip.visible: hovered
                                        ToolTip.delay: 300
                                        ToolTip.text: "Name of the external command on the menu:\n" + extCmdName.text
                                        ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                                    }
                                    TextField {
                                        id: extCmdCmd
                                        focus: true
                                        selectByMouse: true
                                        font.pixelSize: YaycProperties.fsP1
                                        cursorVisible: true
                                        color: YaycProperties.textColor
                                        Layout.fillWidth: true

                                        text: modelData.command
                                        onTextChanged: {
                                            if (utilities.executableExists(text)) {
                                                root.externalCommands[index].command = text
                                                color = YaycProperties.textColor
                                            } else {
                                                color = "firebrick"
                                            }
                                        }

                                        ToolTip.visible: hovered
                                        ToolTip.delay: 300
                                        ToolTip.text: "External command to trigger through context menu:\n" + extCmdCmd.text
                                        ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                                    }
                                    Button {
                                        flat: true
                                        visible: root.extWorkingDirExists
                                        display: Button.IconOnly
                                        icon.source: "/icons/delete_forever.svg"
                                        Layout.alignment: Qt.AlignVCenter
                                        Layout.leftMargin: -8
                                        Layout.rightMargin: 0
                                        onClicked: {
                                            root.removeCommand(index)
                                        }
                                        hoverEnabled: true

                                        ToolTip.visible: hovered
                                        ToolTip.delay: 300
                                        ToolTip.text: "Clear external command"
                                        ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                                    }
                                    Button {
                                        flat: true
                                        visible: index === (extCommandsRepeater.count - 1)
                                        display: Button.IconOnly
                                        icon.source: "/icons/add.svg"
                                        Layout.alignment: Qt.AlignVCenter
                                        Layout.leftMargin: -8
                                        Layout.rightMargin: 0
                                        onClicked: {
                                            root.pushEmptyCommand()
                                        }
                                        hoverEnabled: true

                                        ToolTip.visible: hovered
                                        ToolTip.delay: 300
                                        ToolTip.text: "Add another command"
                                        ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                                    }
                                }
                            }
                            // external apps end
                        } // GridLayout Experimental settings
                    }
                }
            }

            Rectangle {
                color: "transparent"
                width: settingsMain.width
                height: settingsButtonsLayout.height

                ColumnLayout {
                    id: settingsButtonsLayout
                    width: parent.width
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        CheckBox {
                            id: darkModeCheck
                            checked: root.darkMode
                            text: qsTr("Dark mode")
                            onCheckedChanged: root.darkMode = checked
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.delay: 300
                            ToolTip.text: "Toggle dark/light theme"
                            ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                        }
                        CheckBox {
                            id: debugModeCHeck
                            checked: root.debugMode
                            text: qsTr("Advanced")
                            onCheckedChanged: root.debugMode = checked
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.delay: 300
                            ToolTip.text: "Show advanced settings and developer options"
                            ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            id: buttonOpenProxyDialog
                            flat: true
                            text: "Proxy Settings"
                            onClicked: proxyMenu.open()
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.delay: 300
                            ToolTip.text: "Edit the proxy settings used to access the network"
                            ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        visible: root.debugMode

                        CheckBox {
                            id: deleteStorageCheck
                            checked: root.removeStorageOnDelete
                            text: qsTr("Delete storage")
                            onCheckedChanged: root.removeStorageOnDelete = checked
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.delay: 300
                            ToolTip.text: "Controls whether to erase related video data within the working directory for external executable (if specified) upon deletion"
                            ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                        }
                        CheckBox {
                            id: blankWhenHiddenCheck
                            checked: root.blankWhenHidden
                            text: qsTr("Blank when invisible")
                            onCheckedChanged: root.blankWhenHidden = checked
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.delay: 300
                            ToolTip.text: "Controls whether to change the URL to about:blank when YAYC is minimized to save CPU"
                            ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                        }
                        Button {
                            id: buttonOpenJSDialog
                            flat: true
                            text: "Custom Script"
                            onClicked: customScriptDialog.open()
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.delay: 300
                            ToolTip.text: "Edit the custom script that is run after loading a video page"
                            ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            id: buttonResetSettings
                            flat: true
                            text: "Clear Settings"
                            onClicked: utilities.clearSettings(configFileUrl)
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.delay: 300
                            ToolTip.text: "Erase all settings and restart YAYC"
                            ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                        }
                    }
                }

            }

            RowLayout {
                Item {
                    Layout.fillWidth: true
                    height: newReleaseContainer.height * 1.5
                }

                Rectangle {
                    id: newReleaseContainer
                    visible: utilities.compareSemver(appVersion, root.lastestRemoteVersion) < 0

                    color: (maNewVersion.hovered) ? YaycProperties.hoverOverlayColor : "transparent"
                    Layout.alignment: Qt.AlignCenter

                    width: settingsMenu.width * 0.55
                    height: 64
                    Label {
                        anchors.centerIn: parent
                        id: latestReleaseLabel
                        text: "New release available: v" + root.lastestRemoteVersion
                        color: "crimson"
                        font {
                            bold: true
                            pixelSize: YaycProperties.fsH2
                        }
                    }
                    MouseArea {
                        id: maNewVersion
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        property bool hovered: false
                        onEntered: hovered = true
                        onExited: hovered = false
                        onClicked: (mouse) => {
                            Qt.openUrlExternally(repositoryURL + "/releases");
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    height: newReleaseContainer.height * 1.5
                }
            } // New Release RowLayout

            RowLayout {
                ColumnLayout {
                    Row {
                        Layout.alignment: Qt.AlignLeft
                        Item {
                            width: 32
                            height: 32
                            anchors.verticalCenter: parent.verticalCenter
                            Image {
                                id: infoimg
                                source: "/icons/info.svg"
                                visible: false
                                anchors.fill: parent
                            }
                            ColorOverlay {
                                source: infoimg
                                anchors.fill: infoimg
                                color: YaycProperties.iconColor
                            }
                        }
                        Rectangle {
                            width: aboutRow.width + 16
                            height: buttonOpenGProfile.height
                            Layout.alignment: Qt.AlignLeft
                            color: (maAbout.hovered) ? YaycProperties.hoverOverlayColor : "transparent"
                            Row {
                                id: aboutRow
                                anchors.centerIn: parent
                                Label {
                                    id: aboutLabel
                                    text: "About "
                                    font.pixelSize: YaycProperties.fsP1
                                }
                                Image {
                                    anchors.verticalCenter: parent.verticalCenter
                                    source: "/images/yayc-inlined.png"
                                    fillMode: Image.PreserveAspectFit
                                    height: YaycProperties.fsP1
                                    mipmap: true
                                    smooth: true
                                    layer.enabled: true
                                    layer.effect: ColorOverlay { color: YaycProperties.iconColor }
                                }
                            }
                            MouseArea {
                                id: maAbout
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                property bool hovered: false
                                onEntered: hovered = true
                                onExited: hovered = false
                                onClicked: {
                                    aboutContainer.visible = true
                                }
                            }
                        }
                    }
                    Row {
                        Layout.alignment: Qt.AlignLeft
                        Item {
                            width: 32
                            height: 32
                            anchors.verticalCenter: parent.verticalCenter
                            Image {
                                id: helpImg
                                source: "/icons/help.svg"
                                visible: false
                                anchors.fill: parent
                            }
                            ColorOverlay {
                                source: helpImg
                                anchors.fill: helpImg
                                color: YaycProperties.iconColor
                            }
                        }
                        Rectangle {
                            width: helpLabel.width + 16
                            height: buttonOpenGProfile.height
                            Layout.alignment: Qt.AlignLeft
                            color: (maHelp.hovered) ? YaycProperties.hoverOverlayColor : "transparent"
                            Label {
                                anchors.centerIn: parent
                                id: helpLabel
                                text: "Help"
                                font.pixelSize: YaycProperties.fsP1
                            }
                            MouseArea {
                                id: maHelp
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                property bool hovered: false
                                onEntered: hovered = true
                                onExited: hovered = false
                                onClicked: (mouse) => {
                                    helpContainer.visible = true
                                }
                            }
                        }
                    } // Row
                    Item { height: 12; width: 12 } // without things are clipped too much
                } // ColumnLayout

                Item { Layout.fillWidth: true }

                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.rightMargin: 8
                    radius: 6
                    color: (maDonate.hovered
                            && donateButton.enabled) ? YaycProperties.hoverOverlayColor
                                                     : "transparent"
                    width: donateButton.width + 6
                    height: donateButton.height + 6

                    Image {
                        id: donateButton
                        source: "/images/support-us-button.webp"
                        anchors.centerIn: parent
                        width: implicitWidth
                        height: implicitHeight
                        visible: enabled
                        enabled: root.donateUrl !== ""
                        MouseArea {
                            id: maDonate
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            property bool hovered: false
                            onEntered: hovered = true
                            onExited: hovered = false
                            onClicked: (mouse) => {
                                Qt.openUrlExternally(root.donateUrl)
                            }
                        }
                    }
                } // Support Button
            } // About+Help+Support RowLayout
        } // ColumnLayout
    } // settingsMenu Dialog

    Dialog {
        id: aboutContainer
        x: (parent.width - width) * 0.5
        y: (parent.height - height) * 0.5
        width: 800
        visible: false
        modal: true
        header: Item {
            width: aboutContainer.width
            height: YaycProperties.fsH3 * 1.5
            Row {
                anchors.centerIn: parent
                topPadding: 8
                Item {
                    width: children[0].width * 1.05
                    height: children[0].height * 1.05
                    Image {
                        source: "/images/yayc-inlined.png"
                        anchors.centerIn: parent
                        fillMode: Image.PreserveAspectFit
                        height: YaycProperties.fsH2
                        mipmap: true
                        smooth: true
                        layer.enabled: true
                        layer.effect: ColorOverlay { color: YaycProperties.iconColor }
                    }
                }

                Label {
                    id: aboutTitleVersion
                    text: "  v"+appVersion
                    font.pixelSize: YaycProperties.fsH2
                    font.bold: true
                }
            }
        }

        footer: DialogButtonBox {
            standardButtons: DialogButtonBox.Close
        }
        ColumnLayout {
            width: parent.width
            RowLayout {
                Layout.fillWidth: true
                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    Layout.leftMargin: 8
                    Rectangle {
                        width: 224
                        height: 224
                        color: "transparent"

                        Image {
                            anchors {
                                top: parent.top
                                bottom: parent.bottom
                                left: parent.left
                                right: parent.right
                                topMargin: 16
                                bottomMargin: 16
                                leftMargin: 16
                                rightMargin: 16
                            }
                            source: "/images/yayc-square.png"
                            sourceSize: Qt.size(parent.width - 32, parent.height - 32)
                            smooth: true
                            layer.enabled: true
                            layer.effect: ColorOverlay { color: YaycProperties.iconColor }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: (mouse) => {
                                    Qt.openUrlExternally(repositoryURL)
                                }
                            }
                        }
                    }
                    Item {
                        width: 10
                        height: 10
                    }
                    Row {
                        Layout.alignment: Qt.AlignCenter
                        Label {
                            text: "Licensed under  "
                            font.pixelSize: YaycProperties.fsP2 * 1.05
                        }

                        Image {
                            height: YaycProperties.fsP2
                            fillMode: Image.PreserveAspectFit
                            source: "/images/by-nc-sa_15.svg"
                            smooth: true
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 1

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: (mouse) => {
                                    Qt.openUrlExternally(repositoryURL + '/blob/master/LICENSE')
                                }
                            }
                        }
                    }
                }
                Column { // ColumnLayout is screwing the width inside a RowLayout
                    Layout.fillWidth: true

                    Rectangle {
                        color: "transparent"
                        height: 50
                        width: parent.width

                        Label {
                            anchors {
                                left: parent.left
                                right: parent.right
                            }

                            font.pixelSize: YaycProperties.fsP1
                            wrapMode: Text.WordWrap
                            text:
    "YAYC is your modern YouTube client, to help with the "
    + "organization of scheduled and viewed content, progress tracking, and more!"

                        }
                    }

                    Label {
                        text: "Changelog"
                        font {
                            bold: true
                            pixelSize: YaycProperties.fsH4
                        }
                    }
                    Rectangle {
                        color: "transparent"
                        height: 200
                        width: parent.width

                        ScrollView {
                            anchors.fill: parent

                            TextArea {
                                font.pixelSize: YaycProperties.fsP1
                                wrapMode: Text.WordWrap
                                textFormat: Text.MarkdownText
                                readOnly: true
                                background: null // Material style bug
                                text: utilities.getChangelog()
                            }
                        }
                    }

                    Rectangle {
                        color: "transparent"
                        width: parent.width
                        height: children[0].height
                        RowLayout {
                            Label {
                                text: 'Want to help? '
                                font {
                                    bold: true
                                    pixelSize: YaycProperties.fsH4
                                }
                            }
                            Label {
                                id: labelIssues
                                text: '<a href="' + repositoryURL + '/issues">Get involved</a>'
                                font {
                                    bold: true
                                    pixelSize: YaycProperties.fsH4
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: (mouse) => {
                                        Qt.openUrlExternally(
                                        labelIssues.linkAt(labelIssues.width * 0.5,
                                                           labelIssues.height * 0.5))
                                    }
                                }
                            }
                            Label {
                                text: ' or '
                                font {
                                    bold: true
                                    pixelSize: YaycProperties.fsH4
                                }
                                enabled: donateButton.enabled
                                visible: enabled
                            }
                            Label {
                                id: labelDonation
                                enabled: donateButton.enabled
                                visible: enabled
                                text: '<a href="'+root.donateUrl+'">make a donation</a>!'
                                font {
                                    bold: true
                                    pixelSize: YaycProperties.fsH4
                                }
                                onLinkActivated: Qt.openUrlExternally(link)
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: (mouse) => {
                                        Qt.openUrlExternally(
                                        labelDonation.linkAt(labelDonation.width * 0.5,
                                                             labelDonation.height * 0.5))
                                    }
                                }
                            }
                        } // RowLayout
                    } // Rectangle
                } // Column
            } // RowLayout
            Item {
                height: 16
                Layout.fillWidth: true
            }
        } // ColumnLayout
    } // aboutContainer

    Dialog {
        id: helpContainer
        x: (parent.width - width) * 0.5
        y: (parent.height - height) * 0.5
        width: 800
        visible: false
        modal: true
        header: Item {
            width: helpContainer.width
            height: YaycProperties.fsH3 * 1.5
            Row {
                anchors.centerIn: parent
                topPadding: 8
                Label {
                    text: "Help Center"
                    font.pixelSize: YaycProperties.fsH2
                    font.bold: true
                }
            }
        }

        footer: DialogButtonBox {
            standardButtons: DialogButtonBox.Close
        }
        property var images: [
            "/doc/0_startup.png",
            "/doc/1_settings.png",
            "/doc/2_bookmarks_context_menu.png",
            "/doc/3_bookmarks_drag_drop.png"
        ]
        property var tooltips: [
            "YAYC main view consist of a left pane with the bookmarks,\n"
           +"a right pane with a youtube view, and a toolbar.\n"
           +"The toolbar consists of the common web browser controls,\n"
           +"plus a button to add the current video to bookmarks, a button\n"
           +"to copy it to the clipboard, and a button to open Settings.",

            "The Settings panel lets you select directories for where to store bookmarks,\n"
           +"the viewing history, and the Google profile data, to automatically log you\n"
           +"in at every access. If the bookmarks directory isn't specified, bookmarks won't be\n"
           +"stored. If the history directory isn't specified, history won't be saved.\n"
           +"If the Google profile directory isn't specified, YAYC will work in Inkognito mode.",

            "After set up, interaction can be performed through context menus (right click).\n"
           +"There is a context menu in the bookmarks pane and a context menu in the YouTube pane.\n",

            "Bookmarks management can be performed through drag and drop, cut and paste,\n"
           +"and other operations offered by the context menu."
        ]
        ColumnLayout {
            width: parent.width
            ListView {
                id: listViewHelp
                width: parent.width
                height: 600

                model: helpContainer.images
                clip: true
                spacing: 5
                delegate: Image {
                    width: listViewHelp.width
                    fillMode: Image.PreserveAspectFit
                    source: modelData
                    smooth: true

                    ToolTip {
                        visible: maHelpImage.hovered
                        y: parent.height * 0.12
                        contentItem: Text{
                            color: YaycProperties.textColor
                            font.family: mainFont.name
                            font.pixelSize: YaycProperties.fsP2
                            text: helpContainer.tooltips[index]
                        }
                        background: Rectangle {
                            color: YaycProperties.tooltipBgColor
                            border.color: YaycProperties.tooltipBorderColor
                            radius: height * .15
                        }
                    }

                    MouseArea {
                        id: maHelpImage
                        anchors.fill: parent
                        hoverEnabled: true
                        property bool hovered: false
                        onEntered: hovered = true
                        onExited: hovered = false
                    }
                 }
            }

            Item {
                height: 16
                Layout.fillWidth: true
            }
        } // ColumnLayout
    } // helpContainer Dialog

    Dialog {
        id: customScriptDialog
        x: (parent.width - width) * 0.5
        y: (parent.height - height) * 0.5
        width: 800
        visible: false
        modal: true

        header: Item {
            width: customScriptDialog.width
            height: YaycProperties.fsH3 * 1.5
            Row {
                anchors.centerIn: parent
                topPadding: 8
                Label {
                    text: "Custom JS script"
                    font.pixelSize: YaycProperties.fsH2
                    font.bold: true
                }
            }
        }

        ColumnLayout {
            width: parent.width
            Item {
                width: parent.width
                height: 350
                ScrollView {
                    id: jseditScroll
                    anchors.fill: parent
                    clip: true

                    TextArea {
                        id: jsedit
                        wrapMode: TextEdit.NoWrap
                        selectByMouse: true
                        text: root.customScript
                    }
                }
            }
            Item {
                height: 16
                Layout.fillWidth: true
            }
        } // ColumnLayout

        footer: RowLayout {
            Button {
                Layout.alignment: Qt.AlignLeft
                Layout.leftMargin: 8
                text: qsTr("Set")
                onClicked: {
                    root.customScript = jsedit.text
                    customScriptDialog.accept()
                }
                hoverEnabled: true
                ToolTip.visible: hovered
                ToolTip.delay: 100
                ToolTip.text: "Set a custom JavaScript to be run on every video page"
                ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
            }
            Button {
                Layout.alignment: Qt.AlignRight
                Layout.rightMargin: 8
                text: qsTr("Cancel")
                onClicked: {
                    jsedit.text = root.customScript
                    customScriptDialog.close()
                    customScriptDialog.reject()
                }
                hoverEnabled: true
                ToolTip.visible: hovered
                ToolTip.delay: 100
                ToolTip.text: "Abort"
                ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
            }
        } // RowLayout
    } // customScriptDialog

    Dialog {
        id: disclaimerContainer
        x: (parent.width - width) * 0.5
        y: (parent.height - height) * 0.5
        width: 800
        visible: false

        modal: true
        header: Item {
            width: disclaimerContainer.width
            height: YaycProperties.fsH3 * 1.5
            Row {
                anchors.centerIn: parent
                topPadding: 8

                Label {
                    text: "Disclaimer"
                    font.pixelSize: YaycProperties.fsH2
                    font.bold: true
                }
            }
        }

        footer: RowLayout {
            Button {
                Layout.alignment: Qt.AlignLeft
                Layout.leftMargin: 8
                text: qsTr("Accept")
                onClicked: {
                    if (!lolAcknowledged.checked) {
                        lolAcknowledgedContainer.border.color = "firebrick"
                    } else {
                        root.limitationOfLiabilityAccepted = true
                    }
                }

                hoverEnabled: true
                ToolTip.visible: hovered
                ToolTip.delay: 100
                ToolTip.text: "Accept the conditions and limitation of liability"
                ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
            }
            Button {
                Layout.alignment: Qt.AlignRight
                Layout.rightMargin: 8
                text: qsTr("Cancel")
                onClicked: root.quit()

                hoverEnabled: true
                ToolTip.visible: hovered
                ToolTip.delay: 100
                ToolTip.text: "Exit"
                ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
            }
        }

        ColumnLayout {
            width: parent.width
            RowLayout {
                Layout.fillWidth: true
                Column {
                    Layout.fillWidth: true
                    Rectangle {
                        color: "transparent"
                        height: 300
                        width: parent.width

                        ScrollView {
                            anchors.fill: parent

                            TextArea {
                                font.pixelSize: YaycProperties.fsP1
                                wrapMode: Text.WordWrap
                                textFormat: Text.MarkdownText
                                readOnly: true
                                background: null // Material style bug
                                text: utilities.getDisclaimer()
                            }
                        }
                    }
                } // Column
            } // RowLayout
            Rectangle {
                id: lolAcknowledgedContainer
                color: "transparent"
                width: lolAcknowledged.width + 4
                height: lolAcknowledged.height + 4
                CheckBox {
                    anchors.centerIn: parent
                    id: lolAcknowledged
                    checked: false
                    text: qsTr("I Understand and Agree")
                }
            }
            Item {
                height: 16
                Layout.fillWidth: true
            }
        } // ColumnLayout
    } // disclaimerContainer

    Dialog {
        id: addCategoryDialog
        modal: true
        title: "Create new category"
        width: 450
        height: 180
        padding: 16
        anchors.centerIn: parent

        onVisibleChanged: {
            if (visible) {
                forceActiveFocus(Qt.PopupFocusReason)

            }
        }

        onAccepted: {
            var res = fileSystemModel.addCategory(newCategoryInput.text)
            if (res) {

            } else {
                console.log("Failed creating new category ", newCategoryInput.text)
            }
            newCategoryInput.text = ""
        }
        onRejected: {
            newCategoryInput.text = ""
        }

        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                verticalCenter: parent.verticalCenter
            }
            height: newCategoryInput.height * 1.3
            color: "transparent"
            TextField {
                id: newCategoryInput
                focus: true
                anchors {
                    left: parent.left
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                }
                selectByMouse: true
                font.pixelSize: YaycProperties.fsP1
                cursorVisible: true
                color: YaycProperties.textColor
            }
        }

        footer: DialogButtonBox {
            standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
        }
    } // addCategoryDialog

    Dialog {
        id: addVideoDialog
        modal: true
        title: "Add new video"
        width: 650
        height: 180
        padding: 16
        anchors.centerIn: parent

        function addVideo(u) {
            if (!utilities.isYoutubeVideoUrl(u)) {
                // Q_UNREACHABLE
                console.log("Wrong URL fed!")
                return;
            }
            if (utilities.isYoutubeShortsUrl(u)) {
                fileSystemModel.addEntry(utilities.getVideoID(u),
                                         "", // title
                                         "", // channel URL
                                         "", // channel Avatar url
                                         ""  // channel name
                                         )
            } else {
                fileSystemModel.addEntry(utilities.getVideoID(u),
                                         "", // title
                                         "", // channel URL
                                         "", // channel Avatar url
                                         "", // channel name
                                         1,
                                         0) // ToDo: make it update
            }
        }

        onAccepted: {
            var videoUrl = newVideoInput.text;
            newVideoInput.clear()
            addVideoDialog.addVideo(videoUrl)
            close()
        }
        onRejected: {
            newVideoInput.clear()
            close()
        }

        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                verticalCenter: parent.verticalCenter
            }
            height: newVideoInput.height * 1.3
            color: "transparent"
            TextField {
                id: newVideoInput
                focus: true
                anchors {
                    left: parent.left
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                }
                selectByMouse: true
                font.pixelSize: YaycProperties.fsP1
                cursorVisible: true
                color: YaycProperties.textColor
            }
        }

        footer: DialogButtonBox {
            standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
        }
    } // addVideoDialog

    QQD.FolderDialog {
        id: fileDialogVideos
        title: "Please choose a directory to store videos"

        onAccepted: {
            fileDialogVideos.close()
            var path = String(fileDialogVideos.selectedFolder)
            root.youtubePath = root.deUrlizePath(path)
        }
        onRejected: {
        }
    }

    QQD.FolderDialog {
        id: fileDialogHistory
        title: "Please choose a directory to store history"

        onAccepted: {
            fileDialogHistory.close()
            var path = String(fileDialogHistory.selectedFolder)
            root.historyPath = root.deUrlizePath(path)
        }
        onRejected: {
        }
    }

    QQD.FolderDialog {
        id: fileDialogExtWorkingDir
        title: "Please choose a working directory to run the external application"

        onAccepted: {
            fileDialogExtWorkingDir.close()
            var path = String(fileDialogExtWorkingDir.selectedFolder)
            root.extWorkingDirPath = root.deUrlizePath(path)
        }
        onRejected: {
        }
    }

    QQD.FolderDialog {
        id: fileDialogProfile
        title: "Please choose a directory for your Google profile"

        onAccepted: {
            fileDialogProfile.close()
            var path = String(fileDialogProfile.selectedFolder)
            WebBrowsingProfiles.profilePath = root.deUrlizePath(path)
        }
        onRejected: {
        }
    }
}
