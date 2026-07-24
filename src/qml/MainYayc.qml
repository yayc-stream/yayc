/*
Copyright (C) 2023- YAYC team <info@yayc.stream>

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
    property bool filesystemModelReady: false
    property bool windowHidden: win.hidden

    function prepareQuit() {
        if (webEngineView) {
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

    Binding {
        target: keyInterceptor
        property: "playerActive"
        value: webEngineView
               && webEngineView.isYoutubeVideo
               && webEngineView.timePuller.playerState === 1
    }

    Connections {
        target: keyInterceptor
        function onSeekRequested(deltaSec) {
            if (webEngineView)
                webEngineView.seekBy(deltaSec)
        }
    }

    QLP.SystemTrayIcon {
        visible: true
        icon.source: {
            if (!root.webEngineView)
                return "qrc:/images/yayc-alt-grey.png"
            if (root.webEngineView.isYoutubeVideo
                    && root.webEngineView.timePuller.playerState === 1)
                return "qrc:/images/yayc-alt-red.png"
            return "qrc:/images/yayc-alt.png"
        }
        tooltip: {
            if (!root.webEngineView)
                return "YAYC"
            if (root.webEngineView.isYoutubeVideo
                    && root.webEngineView.timePuller.playerState === 1)
                return "YAYC - Playing"
            return "YAYC - Idle"
        }
        menu: QLP.Menu {
            QLP.MenuItem {
                text: (win.visibility == Window.Hidden)
                        ? uiTr("Show")
                        : uiTr("Minimize to Tray")
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
                text: uiTr("Quit")
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
    property bool autoSkipAd: false
    property bool keepForegroundIllusion: false
    property bool showCategoryBar: true
    property int homeGridColumns: 4
    property int maxRecentDestinations: 5
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
    Binding { target: fileSystemModel; property: "maxRecentDestinations"; value: root.maxRecentDestinations }
    Binding { target: utilities; property: "keepForegroundIllusion"; value: root.keepForegroundIllusion }
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
        property alias autoSkipAd: root.autoSkipAd
        property alias keepForegroundIllusion: root.keepForegroundIllusion
        property alias showCategoryBar: root.showCategoryBar
        property alias homeGridColumns: root.homeGridColumns
        property alias maxRecentDestinations: root.maxRecentDestinations
        property alias volume: root.volume
        property alias userSpecifiedVolume: root.userSpecifiedVolume
        property alias guidePaneToggled: root.guideToggled
        property alias proxyType: proxyMenu.proxyType
        property alias proxyPort: proxyMenu.proxyPort
        property alias proxyHost: proxyMenu.proxyHost
        property var splitView
        property string language: "en"
        onLanguageChanged: {
            utilities.currentLanguage = language
        }

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
        utilities.videoUrlResolved.connect(addVideoDialog.addVideo)

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

                    // Desired state: profile exists AND (window visible OR playing OR unload-on-hide disabled)
                    property bool shouldBeActive: WebBrowsingProfiles.profile !== null
                                                   && (webViewWrapper.videoPlaying
                                                       || !root.windowHidden
                                                       || !root.blankWhenHidden)
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
                        autoSkipAd: root.autoSkipAd
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
                        text: uiTr("Loading...")
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
                    function onAutoSkipAdChanged() {
                        if (root.autoSkipAd !== webEngineView.autoSkipAd)
                            root.autoSkipAd = webEngineView.autoSkipAd
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
                        ToolTip.text: uiTr("Go Back (long press for history)")
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
                        ToolTip.text: uiTr("Go Forward (long press for history)")
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
                        text: uiTr("Bookmarks")
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
                        text: uiTr("History")
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
                        ToolTip.text: uiTr("Go to YouTube Home")
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
                        text: uiTr("Activate/Deactivate custom script")
                        enabled: settings.customScript !== ""
                        visible: enabled
                        checkable: true
                        checked: true

                        icon.source: "/icons/js.svg"
                        display: AbstractButton.IconOnly

                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: uiTr("Toggle custom script")
                        ToolTip.delay: 300
                        ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
                    }

                    ToolButton {
                        id: settingsButton
                        text: uiTr("Settings")
                        icon.source: "/icons/settings.svg"
                        display: AbstractButton.IconOnly

                        onClicked: {
                            root.firstRun = false
                            settingsGlitter.enabled = false
                            if (settingsPopup.visible) {
                                settingsPopup.close()
                            } else {
                                var p = settingsButton.mapToItem(root, 0, settingsButton.height)
                                settingsPopup.x = Math.max(8, p.x - settingsPopup.width + settingsButton.width)
                                settingsPopup.y = p.y + 4
                                settingsPopup.open()
                            }
                        }

                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: uiTr("Open Settings Panel")
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
                text: "<b>" + uiTr("Proxy Settings") + "</b>"
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
                text: uiTr("Proxy Type") + ":"
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
                        text: uiTr("Host") + ":"
                    }

                    TextField {
                        id: hostTextField
                        Layout.fillWidth: true
                        placeholderText: uiTr("Enter proxy host")
                        onTextChanged: proxyMenu.proxyHost = text
                    }

                    Label {
                        text: uiTr("Port") + ":"
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

    SettingsMenu {
        id: settingsPopup
        parent: root
        host: root
        settingsObj: settings
        appVersionStr: appVersion
        latestRemoteVersionStr: root.lastestRemoteVersion
        releasesUrl: repositoryURL
        donateUrl: root.donateUrl
        youtubePath: root.youtubePath
        historyPath: root.historyPath
        profilePath: WebBrowsingProfiles.profilePath
        extWorkingDirPath: root.extWorkingDirPath

        onOpenBookmarksEdit: bookmarksEditDialog.open()
        onOpenHistoryEdit: historyEditDialog.open()
        onOpenProfileEdit: profileEditDialog.open()
        onOpenExtWorkDirEdit: extWorkDirEditDialog.open()
        onOpenExtCommandsPopup: extCommandsDialog.open()
        onOpenAboutDialog: aboutContainer.visible = true
        onOpenHelpDialog: helpContainer.visible = true
        onOpenProxyDialog: proxyMenu.open()
        onOpenCustomScriptDialog: customScriptDialog.open()
        onClearSettingsRequested: utilities.clearSettings(configFileUrl)
        onQuitRequested: root.quit()
    }

    PathEditDialog {
        id: bookmarksEditDialog
        dialogTitle: uiTr("Bookmarks directory")
        initialPath: root.youtubePath
        onPathAccepted: function(p) { root.youtubePath = p }
    }
    PathEditDialog {
        id: historyEditDialog
        dialogTitle: uiTr("History directory")
        initialPath: root.historyPath
        onPathAccepted: function(p) { root.historyPath = p }
    }
    PathEditDialog {
        id: profileEditDialog
        dialogTitle: uiTr("Profile directory")
        initialPath: WebBrowsingProfiles.profilePath
        onPathAccepted: function(p) { WebBrowsingProfiles.profilePath = p }
    }
    PathEditDialog {
        id: extWorkDirEditDialog
        dialogTitle: uiTr("External working directory")
        initialPath: root.extWorkingDirPath
        onPathAccepted: function(p) { root.extWorkingDirPath = p }
    }

    ExtCommandsDialog {
        id: extCommandsDialog
        parent: root
        host: root
    }

    Dialog {
        id: aboutContainer
        x: (parent.width - width) * 0.5
        y: (parent.height - height) * 0.5
        width: 960
        height: Math.min(parent.height * 0.9, 640)
        visible: false
        modal: true
        padding: 20
        bottomPadding: 0

        header: Item {
            width: aboutContainer.width
            height: YaycProperties.fsH2 * 2
            Row {
                anchors.centerIn: parent
                spacing: 10
                Image {
                    source: "/images/yayc-inlined.png"
                    anchors.verticalCenter: parent.verticalCenter
                    fillMode: Image.PreserveAspectFit
                    height: YaycProperties.fsH2
                    mipmap: true
                    smooth: true
                    layer.enabled: true
                    layer.effect: ColorOverlay { color: YaycProperties.iconColor }
                }
                Label {
                    id: aboutTitleVersion
                    anchors.verticalCenter: parent.verticalCenter
                    text: "v" + appVersion
                    font.pixelSize: YaycProperties.fsH2
                    font.bold: true
                }
            }
        }

        footer: DialogButtonBox {
            standardButtons: DialogButtonBox.Close
        }

        RowLayout {
            anchors.fill: parent
            spacing: 24

            ColumnLayout {
                Layout.alignment: Qt.AlignTop
                Layout.preferredWidth: 224
                spacing: 16

                Item {
                    Layout.preferredWidth: 224
                    Layout.preferredHeight: 224
                    Image {
                        anchors.fill: parent
                        anchors.margins: 8
                        source: "/images/yayc-square.png"
                        sourceSize: Qt.size(208, 208)
                        smooth: true
                        mipmap: true
                        fillMode: Image.PreserveAspectFit
                        layer.enabled: true
                        layer.effect: ColorOverlay { color: YaycProperties.iconColor }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Qt.openUrlExternally(repositoryURL)
                        }
                    }
                }

                Row {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 6
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: uiTr("Licensed under")
                        font.pixelSize: YaycProperties.fsP2 * 1.05
                    }
                    Image {
                        anchors.verticalCenter: parent.verticalCenter
                        height: YaycProperties.fsP1
                        fillMode: Image.PreserveAspectFit
                        source: "/images/by-nc-sa_15.svg"
                        smooth: true
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Qt.openUrlExternally(repositoryURL + '/blob/master/LICENSE')
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 14

                Label {
                    Layout.fillWidth: true
                    font.pixelSize: YaycProperties.fsP1
                    wrapMode: Text.WordWrap
                    text: uiTr("YAYC is your modern YouTube client, to help with the organization of scheduled and viewed content, progress tracking, and more!")
                }

                Label {
                    text: uiTr("Changelog")
                    font.bold: true
                    font.pixelSize: YaycProperties.fsH4
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 160
                    color: YaycProperties.paneColor
                    radius: 6
                    border.color: YaycProperties.tooltipBorderColor
                    border.width: 1

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 8
                        clip: true

                        TextArea {
                            font.pixelSize: YaycProperties.fsP1
                            wrapMode: Text.WordWrap
                            textFormat: Text.MarkdownText
                            readOnly: true
                            background: null
                            text: utilities.getChangelog()
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Label {
                        text: uiTr("Want to help?")
                        font.bold: true
                        font.pixelSize: YaycProperties.fsH4
                    }
                    Label {
                        id: labelIssues
                        text: '<a href="' + repositoryURL + '/issues">' + uiTr("Get involved") + '</a>'
                        font.bold: true
                        font.pixelSize: YaycProperties.fsH4
                        linkColor: YaycProperties.selectionColor
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Qt.openUrlExternally(
                                labelIssues.linkAt(labelIssues.width * 0.5,
                                                   labelIssues.height * 0.5))
                        }
                    }
                    Label {
                        text: uiTr("or")
                        font.bold: true
                        font.pixelSize: YaycProperties.fsH4
                        enabled: root.donateUrl !== ""
                        visible: enabled
                    }
                    Label {
                        id: labelDonation
                        enabled: root.donateUrl !== ""
                        visible: enabled
                        text: '<a href="'+root.donateUrl+'">' + uiTr("make a donation") + '</a>!'
                        font.bold: true
                        font.pixelSize: YaycProperties.fsH4
                        linkColor: YaycProperties.selectionColor
                        onLinkActivated: Qt.openUrlExternally(link)
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Qt.openUrlExternally(
                                labelDonation.linkAt(labelDonation.width * 0.5,
                                                     labelDonation.height * 0.5))
                        }
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }
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
                    text: uiTr("Help Center")
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
            uiTr("YAYC main view consist of a left pane with the bookmarks,\n"
           +"a right pane with a youtube view, and a toolbar.\n"
           +"The toolbar consists of the common web browser controls,\n"
           +"plus a button to add the current video to bookmarks, a button\n"
           +"to copy it to the clipboard, and a button to open Settings."),

            uiTr("The Settings panel lets you select directories for where to store bookmarks,\n"
           +"the viewing history, and the Google profile data, to automatically log you\n"
           +"in at every access. If the bookmarks directory isn't specified, bookmarks won't be\n"
           +"stored. If the history directory isn't specified, history won't be saved.\n"
           +"If the Google profile directory isn't specified, YAYC will work in Inkognito mode."),

            uiTr("After set up, interaction can be performed through context menus (right click).\n"
           +"There is a context menu in the bookmarks pane and a context menu in the YouTube pane.\n"),

            uiTr("Bookmarks management can be performed through drag and drop, cut and paste,\n"
           +"and other operations offered by the context menu.")
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
                    text: uiTr("Custom JS script")
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
                text: uiTr("Set")
                onClicked: {
                    root.customScript = jsedit.text
                    customScriptDialog.accept()
                }
                hoverEnabled: true
                ToolTip.visible: hovered
                ToolTip.delay: 100
                ToolTip.text: uiTr("Set a custom JavaScript to be run on every video page")
                ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
            }
            Button {
                Layout.alignment: Qt.AlignRight
                Layout.rightMargin: 8
                text: uiTr("Cancel")
                onClicked: {
                    jsedit.text = root.customScript
                    customScriptDialog.close()
                    customScriptDialog.reject()
                }
                hoverEnabled: true
                ToolTip.visible: hovered
                ToolTip.delay: 100
                ToolTip.text: uiTr("Abort")
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
                    text: uiTr("Disclaimer")
                    font.pixelSize: YaycProperties.fsH2
                    font.bold: true
                }
            }
        }

        footer: RowLayout {
            Button {
                Layout.alignment: Qt.AlignLeft
                Layout.leftMargin: 8
                text: uiTr("Accept")
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
                ToolTip.text: uiTr("Accept the conditions and limitation of liability")
                ToolTip.toolTip.font.pixelSize: YaycProperties.fsP2
            }
            Button {
                Layout.alignment: Qt.AlignRight
                Layout.rightMargin: 8
                text: uiTr("Cancel")
                onClicked: root.quit()

                hoverEnabled: true
                ToolTip.visible: hovered
                ToolTip.delay: 100
                ToolTip.text: uiTr("Exit")
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
                    text: uiTr("I Understand and Agree")
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
        property var parentCategoryIndex: undefined
        property string parentCategoryName: ""
        title: parentCategoryName !== ""
               ? "Create new category in \"" + parentCategoryName + "\""
               : "Create new category"
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
            var res = (parentCategoryIndex !== undefined && parentCategoryIndex !== null)
                      ? fileSystemModel.addCategory(newCategoryInput.text, parentCategoryIndex)
                      : fileSystemModel.addCategory(newCategoryInput.text)
            if (res) {

            } else {
                console.log("Failed creating new category ", newCategoryInput.text)
            }
            newCategoryInput.text = ""
            parentCategoryIndex = undefined
            parentCategoryName = ""
        }
        onRejected: {
            newCategoryInput.text = ""
            parentCategoryIndex = undefined
            parentCategoryName = ""
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
                onAccepted: addCategoryDialog.accept()
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

        property string destination: ""

        function addVideo(normalizedUrl) {
            if (!utilities.isYoutubeVideoUrl(normalizedUrl)) {
                console.log("Wrong URL fed!")
                return;
            }
            if (utilities.isYoutubeShortsUrl(normalizedUrl)) {
                fileSystemModel.addEntry(utilities.getVideoID(normalizedUrl),
                                         "", // title
                                         "", // channel URL
                                         "", // channel Avatar url
                                         "", // channel name
                                         0, 0,
                                         addVideoDialog.destination)
            } else {
                fileSystemModel.addEntry(utilities.getVideoID(normalizedUrl),
                                         "", // title
                                         "", // channel URL
                                         "", // channel Avatar url
                                         "", // channel name
                                         1, 0,
                                         addVideoDialog.destination)
            }
            addVideoDialog.destination = ""
        }

        onAccepted: {
            var videoUrl = newVideoInput.text;
            newVideoInput.clear()
            utilities.resolveAndNormalizeUrl(videoUrl)
            close()
        }
        onRejected: {
            addVideoDialog.destination = ""
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
}
