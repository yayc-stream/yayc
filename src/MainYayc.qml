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

Item {
    id: root
    objectName: "root"

    property alias addVideoEnabled: buttonAddVideo.enabled

    property int addedVideoTrigger: 0
    function triggerVideoAdded() { addedVideoTrigger += 1 }
    property int workingDirTrigger: 0
    function triggerWorkingDir() { workingDirTrigger += 1 }
    property alias url: webEngineView.url
    property url previousUrl
    property bool filesystemModelReady: false
    property bool windowHidden: win.hidden

    function quit() {
        // the setting is an alias for reloading purposes
        if (root.windowHidden && root.blankWhenHidden && previousUrl !== "") {
            settings.lastUrl = previousUrl // ToDo: try to save/restore position too
        } else {
            settings.lastUrl = timePuller.getCurrentVideoURLWithPosition()
        }
        syncAll()
        win.quitting = true
        Qt.quit()
    }

    function minimizeToTray() {
        syncAll()
        win.hide()
    }

    function runScript(s) {
        webEngineView.runJavaScript(s)
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

    onWindowHiddenChanged: {
        if (!root.blankWhenHidden || webEngineView.isYoutubeVideo)
            return
        if (windowHidden) {
            // store && change
            previousUrl = url
            url = "about:blank"
        } else {
            // restore
            url = previousUrl
            previousUrl = ""
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
                    root.quit()
                }
            }
        }
        onActivated: {
            if (reason == QLP.SystemTrayIcon.Trigger) {
                if (win.visible)
                    win.hide()
                else
                    win.show()
            }
        }
    }

    property string profilePath // if empty, the webengineview profile will turn itself "off the record"
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
    property real wevZoomFactor
    property real wevZoomFactorVideo
    property alias volume: sliderVolume.value
    property bool muted: false
    property bool guideToggled: buttonToggleGuide.checked

    property var externalCommands: []
    function pushEmptyCommand() {
        var empty = {name : "", command : ""}
        if (root.externalCommands.length !== 0
                && root.externalCommands[root.externalCommands.length - 1].name == ""
                && root.externalCommands[root.externalCommands.length - 1].command == "")
            return;
        var newCommands = root.externalCommands
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
    Settings {
        id: settings
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
        property alias volume: root.volume
        property alias userSpecifiedVolume: sliderVolume.userValue
        property alias guidePaneToggled: root.guideToggled
        property alias proxyType: proxyMenu.proxyType
        property alias proxyPort: proxyMenu.proxyPort
        property alias proxyHost: proxyMenu.proxyHost
        property var splitView

        Component.onCompleted: {
            disclaimerContainer.visible = Qt.binding(function() { return !settings.lolAccepted })
            webEngineView.zoomFactor =  Qt.binding(function() {
                var res = (webEngineView.isYoutubeVideo)
                                ? root.wevZoomFactorVideo
                                : root.wevZoomFactor
                return (res) ? res : 1.0
            })
            timerSettings.start()
        }
    }

    WebEngineProfile {
        id: userProfile
        httpAcceptLanguage: root.httpAcceptLanguage
        httpUserAgent: root.httpUserAgent
        httpCacheType: WebEngineProfile.MemoryHttpCache
        persistentCookiesPolicy: WebEngineProfile.ForcePersistentCookies

        cachePath: (typeof(root.profilePath) !== "undefined" && root.profilePath !== "")
                   ? root.profilePath + "/cache" : ""

        persistentStoragePath: (typeof(root.profilePath) !== "undefined" && root.profilePath !== "")
                               ? root.profilePath + "/data" : ""

        storageName: "yayc"
        offTheRecord: false
        userScripts.collection: createWebChannelScripts(root.customScript)
    }

    WebEngineProfile {
        id: inkognitoProfile
        httpAcceptLanguage: root.httpAcceptLanguage
        httpUserAgent: root.httpUserAgent
        httpCacheType: WebEngineProfile.MemoryHttpCache
        persistentCookiesPolicy: WebEngineProfile.NoPersistentCookies
        cachePath: ""
        persistentStoragePath: ""
        offTheRecord: true
        userScripts.collection: createWebChannelScripts(root.customScript)
    }

    function createWebChannelScripts(customScript) { // TODO: remind the user that changing userScript requires app restart
        let webChannelScript = WebEngine.script()
        webChannelScript.name = "QWebChannel"
        webChannelScript.injectionPoint = WebEngineScript.Deferred
        webChannelScript.worldId = WebEngineScript.MainWorld
        webChannelScript.sourceUrl = Qt.resolvedUrl("qrc:/qtwebchannel/qwebchannel.js")

        let userScript = WebEngine.script()
        userScript.injectionPoint = WebEngineScript.Deferred
        userScript.worldId = WebEngineScript.MainWorld
        userScript.sourceCode = (settings.customScriptEnabled) ? customScript : ""

        return [ webChannelScript, userScript ]
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
        triggerWorkingDir()
        triggerVideoAdded()
        if (sliderVolume.value !== sliderVolume.userValue)
            sliderVolume.value = sliderVolume.userValue
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

    onProfilePathChanged: {
        webEngineView.reload()
        settings.sync()
    }

    Timer {
        id: zoomFactorSyncer
        repeat: true
        running: true
        interval: 1000 * 5
        onTriggered: {
            syncZoomFactor()
        }
    }

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
        if (res === 1) { // if latest is greater
            // highlight settings
            root.firstRun = true
        }
    }


    function resetFilesystemModels() {
        console.log("resetFilesystemModels")
        clearFilesystemModels()
        Qt.callLater(setFilesystemModels)
    }

    function clearFilesystemModels() {
        console.log("clearFilesystemModels")
        bookmarksContainer.clearModel()
        historyContainer.clearModel()
        fileSystemModel.setRoot("")
        historyModel.setRoot("")
    }

    function setFilesystemModels() {
        console.log("setFilesystemModels ", youtubePath,  historyPath)
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
        if (!root.settingsLoaded)
            return
        if (webEngineView.isYoutubeVideo)
            root.wevZoomFactorVideo = webEngineView.zoomFactor
        else
            root.wevZoomFactor = webEngineView.zoomFactor
    }

    function deUrlizePath(path) {
        path = path.slice(7) // strip file://
        if (Qt.platform.os === "windows" &&
                path[0] === '/') {
            path = path.slice(1)
        }
        return path
    }

    function isCurrentVideoAdded(key, trigger) {
        if (!utilities.isYoutubeVideoUrl(root.url))
            return false;
        return fileSystemModel.isVideoBookmarked(key)
    }

    function isWorkingDirPresent(key, trigger) {
        if (root.extWorkingDirExists)
            return fileSystemModel.hasWorkingDir(key,
                                                root.extWorkingDirPath)
        return 0
    }

    property string lastHoveredLink
    property string lastHoveredTooltip

    Item { id: dummy } // Workaround for QTBUG-59940
    QtObject {
        id: properties
        property color textColor: "#ffffff"
        property color disabledTextColor: "#a0a1a2"
        property color addedTextColor: "#32cd32" //"limegreen"
        property color addedDisabledTextColor: "#196619" // tinted down
        property color selectionColor: "#43adee"
        property color listHighlightColor: "#585a5c"
        property color paneBackgroundColor: "#2e2f30"
        property color paneColor: "#373839"
        property color viewBorderColor: "#000000"
        property color itemBackgroundColor: "#46484a"
        property color itemColor: "#cccccc"
        property color iconHighlightColor: "#26282a"
        property string labelFontFamily: "Open Sans"
        property color fileBgColor: "black"
        property color categoryBgColor: "black"
        property color checkedButtonColor: "#EF9A9A" // Red material accent
        readonly property real fsH0: 40
        readonly property real fsH1: 34
        readonly property real fsH2: 28
        readonly property real fsH3: 24
        readonly property real fsH4: 20
        readonly property real fsH5: 16
        readonly property real fsH6: 12
        readonly property real fsP1: 16
        readonly property real fsP2: 12
    }

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

    QtObject{
        id: internals

        function getPlayer(isShorts) {
            var res = ""
            if (isShorts) {
//                res += "var ytplayer = document.getElementById('player').getPlayer();
                res += "
    var activeShort = document.querySelectorAll('ytd-reel-video-renderer[is-active]')[0];
    var ytplayer = activeShort.querySelector('ytd-player[id=\"player\"]').getPlayer();
"
            } else {
                res += "var ytplayer = document.getElementById('movie_player');
"
            }
            return res
        }

        property string script_videoTime: "
            var backend;
            new QWebChannel(qt.webChannelTransport, function (channel) {
                backend = channel.objects.backend;
            });
            setTimeout(function() {  //function puller()
                    backend.channelURL = document.getElementById('text').firstChild.href;
                    backend.channelName = document.getElementById('text').firstChild.text;
                    backend.channelAvatar = document.getElementById('owner').firstElementChild.firstElementChild.firstElementChild.firstElementChild.src;

                    var ytplayer = document.getElementById('movie_player');

                    backend.videoTitle = ytplayer.getVideoData().title;
                    backend.videoDuration = ytplayer.getDuration();
                    backend.videoPosition = ytplayer.getCurrentTime();
                    backend.playbackRate = ytplayer.getPlaybackRate();
                    backend.playerState = ytplayer.getPlayerState();
                    backend.volume = ytplayer.getVolume();
                    backend.muted = ytplayer.isMuted();

                    var url = document.getElementsByTagName('ytd-watch-flexy')[0].getAttribute('video-id')
                    backend.videoID = url;
                    backend.shorts = false;
                    backend.vendor = 'YTB';
            }, 100);
            //puller();
        "

        property string script_backend: "
            var backend;
            new QWebChannel(qt.webChannelTransport, function (channel) {
                backend = channel.objects.backend;
            });
        "

        property string script_homePageStatusFetcher: "
            var backend;
            new QWebChannel(qt.webChannelTransport, function (channel) {
                console.log('ASDASDASDA');
                backend = channel.objects.backend;
            });
            setTimeout(function() {
                var btn = document.querySelectorAll(
                    'button[id=\"button\"][class=\"style-scope yt-icon-button\"][aria-label=\"Guide\"]')[0]

                backend.guideButtonChecked = btn.getAttribute(\"aria-pressed\")
            }, 100);
        "

        property string script_clickGuide: "
            var backend;
            new QWebChannel(qt.webChannelTransport, function (channel) {
                backend = channel.objects.backend;
            });
            setTimeout(function() {
                var btn = document.querySelectorAll(
                    'button[id=\"button\"][class=\"style-scope yt-icon-button\"][aria-label=\"Guide\"]')[0]
                btn.click()
                backend.guideButtonChecked = btn.getAttribute(\"aria-pressed\")
            }, 100);
"

        property string script_videoTimeShorts: "
            var backend;
            new QWebChannel(qt.webChannelTransport, function (channel) {
                backend = channel.objects.backend;
            });
            setTimeout(function() {
//                try {
                    var activeShort = document.querySelectorAll('ytd-reel-video-renderer[is-active]')[0]
                    //var chanInfo = activeShort.querySelector('div[id=\"channel-info\"]')
                    backend.channelURL = activeShort.getElementsByClassName('yt-core-attributed-string__link yt-core-attributed-string__link--call-to-action-color yt-core-attributed-string--link-inherit-color')[0].href.replace('/shorts', '');
                    backend.channelName = activeShort.getElementsByClassName('yt-core-attributed-string__link yt-core-attributed-string__link--call-to-action-color yt-core-attributed-string--link-inherit-color')[0].textContent
                    backend.channelAvatar = document.getElementsByClassName('yt-spec-avatar-shape__image ytCoreImageHost')[0].src

                    //var url = activeShort.getElementsByClassName('player-container style-scope ytd-reel-video-renderer')[0].getAttribute('style')
                    var url = activeShort.getElementsByClassName('ytp-title-link yt-uix-sessionlink')[0].href.split('/');
                    url = url[url.length - 1]
                    backend.videoID = url;
                    backend.shorts = true;
                    backend.vendor = 'YTB';

                    var ytplayer = activeShort.querySelector('ytd-player[id=\"player\"]').getPlayer();

                    backend.videoTitle = document.title;
                    backend.videoDuration = ytplayer.getDuration();
                    backend.videoPosition = ytplayer.getCurrentTime();
                    backend.playbackRate = ytplayer.getPlaybackRate();
                    backend.playerState = ytplayer.getPlayerState();
                    backend.volume = ytplayer.getVolume();
                    backend.muted = ytplayer.isMuted();
                    //console.log(document.title);
//                } catch (e) {
//                    console.log(e);
//                }
            }, 100);
        "

        function getPlaybackRateSetterScript(rate, isShorts) {
            var res = "
            setTimeout(function() {
    " + getPlayer(isShorts) +
"                 ytplayer.setPlaybackRate(" + rate + ");
        }, 100);
"
            return res;
        }

        function getVolumeSetterScript(volume, isShorts) {
            var res = "
            setTimeout(function() {
    " + getPlayer(isShorts) +
"                 ytplayer.setVolume(" + volume + ");
        }, 100);
"
            return res;
        }

        function getMutedSetterScript(muted, isShorts) {
            var res = "
            setTimeout(function() {
    " + getPlayer(isShorts)

            if (muted) {
                res += "                 ytplayer.mute();
"
            } else {
                res += "                 ytplayer.unMute();
"
            }

            res +=
"       }, 100);
"
            return res;
        }

        readonly property var videoSpeeds: [
            "0.25",
            "0.50",
            "0.75",
            "1.00",
            "1.25",
            "1.50",
            "1.75",
            "2.00",
        ]

        function getPlayVideoScript(isShorts) {
            var res = "
            setTimeout(function() {
    " + getPlayer(isShorts) +
"                 ytplayer.playVideo();
        }, 100);
"
            return res;
        }

        function getPlayNextVideoScript(isShorts) {
            var res = "
            setTimeout(function() {
    " + getPlayer(isShorts) +
"                 ytplayer.playNextVideo();
        }, 100);
"
            return res;
        }

        function getPauseVideoScript(isShorts) {
            var res = "
            setTimeout(function() {
" + getPlayer(isShorts) +
"                 ytplayer.pauseVideo();
        }, 100);
"
            return res;
        }
    }

    Timer {
        id: guideToggleSingleShot
        repeat: false
        running: false
        interval: 2750 // webEngineView is not emitting loadingChanged
                       // when clicking on the youtube logo to go back to the homepage
                       // However, onUrlChanged is also too soon, as the page is not laoded
        onTriggered: {
            timePuller.pullHomeData()
        }
    }


    property bool guideButtonCheckedClientSet: false
    QtObject {
        id: timePuller

        // ID, under which this object will be known at WebEngineView side
        WebChannel.id: "backend"

        property real videoPosition: 0
        property real videoDuration: 0
        property string videoID
        property string videoTitle
        property string channelURL
        property string channelName
        property string channelAvatar
        property string vendor

        property real playbackRate
        property int playerState
        property int volume
        property bool muted
        property bool shorts
        property bool guideButtonChecked

        function clickGuideButton() {
            if (webEngineView.isYoutubeHome || webEngineView.isYoutubeChannel) {
                root.runScript(internals.script_clickGuide)
            }
        }

        function pullHomeData() {
            if (webEngineView.isYoutubeHome || webEngineView.isYoutubeChannel){
                root.runScript(internals.script_homePageStatusFetcher)
            }
        }

        onGuideButtonCheckedChanged: {
            if (root.guideButtonCheckedClientSet) {
                guideButtonCheckedClientSet = false
                return
            }

            if (guideButtonChecked === buttonToggleGuide.checked) {
                return;
            }
            clickGuideButton();
        }

        onVolumeChanged: root.volume = volume
        onMutedChanged: root.muted = muted

        function getCurrentVideoURLWithPosition() {
            if (webEngineView.isYoutubeVideo && videoID !== ""
                    && webEngineView.key == utilities.getVideoID(videoID, vendor, shorts))
                return utilities.urlWithPosition(root.url, timePuller.videoPosition)
            return root.url
        }

        onVideoPositionChanged: {
            var k = utilities.getVideoID(videoID, vendor, shorts)
            if (webEngineView.key !== k) {
                root.addVideoEnabled = false
                return
            }

            root.addVideoEnabled = true

            if (webEngineView.key !== ""
                  && videoTitle === "") {
                // missing title, silently ignore
                return;
            }

            // url didn't change, position changed
            update()
        }

        onPlaybackRateChanged: {

        }

        function update() {
            var k = utilities.getVideoID(videoID, vendor, shorts)
            if (k !== webEngineView.key)
                return

            fileSystemModel.updateEntry(webEngineView.key,
                                        videoTitle,
                                        channelURL,
                                        channelAvatar,
                                        channelName,
                                        videoDuration,
                                        videoPosition)
            if (!historyModel.updateEntry(webEngineView.key,
                                          videoTitle,
                                          channelURL,
                                          channelAvatar,
                                          channelName,
                                          videoDuration,
                                          videoPosition)) {
                historyModel.addEntry(webEngineView.key,
                                      videoTitle,
                                      channelURL,
                                      channelAvatar,
                                      channelName,
                                      videoDuration,
                                      videoPosition)
            }
        }
        function addCurrentVideo() {
            if (!utilities.isYoutubeVideoUrl(root.url)) {
                // Q_UNREACHABLE
                return;
            }

            var k = utilities.getVideoID(videoID, vendor, shorts)

            if (webEngineView.key !== k) {
                root.addVideoEnabled = false
                return
            }

            fileSystemModel.addEntry(webEngineView.key,
                                     videoTitle,
                                     channelURL,
                                     channelAvatar,
                                     channelName,
                                     videoDuration,
                                     videoPosition)

            if (k !== "" && shorts)
                fileSystemModel.viewEntry(webEngineView.key, true);
            root.triggerVideoAdded()
        }
    } // timePuller

    WebChannel {
        id : web_channel
        registeredObjects: [timePuller]
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
                anchors {
                    top: parent.top
                    bottom: parent.bottom
                }
                showFiltering: bookmarksToolButton.searching
            }
            BookmarksTreeView {
                id: historyContainer
                visible: false
                historyView: true
                width: 200
                implicitWidth: 200
                anchors {
                    top: parent.top
                    bottom: parent.bottom
                }
                showFiltering: historyToolButton.searching
            }

            WebEngineView {
                id: webEngineView
                url: "https://youtube.com"
                property string key
                property string easyListPath: root.easyListPath
                property bool isShorts: false
                property bool isYoutubeChannel: utilities.isYoutubeChannelPage(url)
                property bool isYoutubeHome: utilities.isYoutubeHomepage(url)
                property bool isYoutubeVideo: utilities.isYoutubeVideoUrl(url)
                property int keyHasWorkingDir: isWorkingDirPresent(webEngineView.key,
                                                                    root.extWorkingDirPath,
                                                                    root.workingDirTrigger)
                enabled: true
                visible: enabled
                SplitView.minimumWidth: 200
                SplitView.fillWidth: true
                anchors {
                    top: parent.top
                    bottom: parent.bottom
                }

                objectName: "webEngineView"
//                Component.onCompleted: {
//                    utilities.addRequestInterceptor(this)
//                    requestInterceptor.setEasyListPath(easyListPath)

//                    let webChannelScript = WebEngine.script()
//                    webChannelScript.name = "QWebChannel"
//                    webChannelScript.injectionPoint = WebEngineScript.Deferred
//                    webChannelScript.worldId = WebEngineScript.MainWorld
//                    webChannelScript.sourceUrl = Qt.resolvedUrl("qrc:/qtwebchannel/qwebchannel.js")

//                    let userScript = WebEngine.script()
//                    userScript.injectionPoint = WebEngineScript.Deferred
//                    userScript.worldId = WebEngineScript.MainWorld
//                    userScript.sourceCode = root.customScript

//                    var list = [ webChannelScript, userScript ]
//                    webEngineView.userScripts.insert(list)
//                }

                webChannel: web_channel

                profile: (typeof(root.profilePath) !== "undefined" && root.profilePath !== "")
                         ? userProfile
                         : inkognitoProfile

                settings {
                    autoLoadImages: true
                    dnsPrefetchEnabled: true

                    fullScreenSupportEnabled: true
                    javascriptCanAccessClipboard: true
                    javascriptCanPaste: true
                    screenCaptureEnabled: true
                    playbackRequiresUserGesture: false
                }

                onLoadingChanged: (loadingInfo) => {
                    if (loadingInfo.status === WebEngineView.LoadSucceededStatus
                        || loadingInfo.status ===  WebEngineView.LoadStoppedStatus) {

                        if (webEngineView.isYoutubeHome
                                || webEngineView.isYoutubeChannel) {
                            guideToggleSingleShot.start()
                        }
                        if (loadingInfo.status === WebEngineView.LoadSucceededStatus) {
                            if (utilities.isYoutubeVideoUrl(url)) {
                              root.addVideoEnabled = true
                              zoomFactor = root.wevZoomFactorVideo
                              key = utilities.getVideoID(url)
                              isShorts = utilities.isYoutubeShortsUrl(url)
                              dataPuller.startPulling()
                              return;
                            }

                            root.addVideoEnabled = false
                            isShorts = false
                            zoomFactor = root.wevZoomFactor
                            dataPuller.stop()
                            key = ""
                         }
                    } else {
                        // loading or errored
                    }
                }

                Timer {
                    id: dataPuller
                    interval: 5000;
                    running: false;
                    repeat: true

                    function startPulling() {
                        start()
                    }

                    function pullTime() {
                        interval = 5000
                        // console.log(timePuller.keyBefore, webEngineView.key, timePuller.videoTitle, timePuller.videoPosition, timePuller.videoDuration)

                        if (!webEngineView.isYoutubeVideo
                                || webEngineView.key === "")
                            return;

                        if (webEngineView.isShorts) {
                            root.runScript(internals.script_videoTimeShorts)
                        } else {
                            root.runScript(internals.script_videoTime)
                        }
                    }

                    onTriggered: {
                        pullTime()
                    }
                }

                property Menu contextMenu: Menu {
                    MenuItem {
                        text: currentVideoAdded ? "Added" : "Add"
                        enabled: linkHovered && !currentVideoAdded

                        property bool linkHovered: typeof(root.lastHoveredLink) !== "undefined" && root.lastHoveredLink !== ""
                        property bool storagePresent: linkHovered && fileSystemModel.isVideoBookmarked(utilities.getVideoID(root.lastHoveredLink))
                        property bool currentVideoAdded: linkHovered && fileSystemModel.isVideoBookmarked(utilities.getVideoID(root.lastHoveredLink))
                        property bool workingDirPresent: currentVideoAdded
                                                         && root.extWorkingDirExists
                                                         && fileSystemModel.hasWorkingDir(utilities.getVideoID(root.lastHoveredLink),
                                                                                          root.extWorkingDirPath)
                        icon {
                            source: "/icons/add.svg"
                            color: (currentVideoAdded) // ToDo: deduplicate
                                   ? (enabled)
                                     ? properties.addedTextColor
                                     : properties.addedDisabledTextColor
                                   : (enabled)
                                     ? "white"
                                     : properties.disabledTextColor
                        }

                        Image {
                            visible: parent.workingDirPresent > 0
                            anchors {
                                left: parent.left
                                top: parent.top
                                bottom: parent.bottom
                                leftMargin: 12
                                topMargin: 12
                                bottomMargin: 4
                            }

                            source: (parent.workingDirPresent == 2)
                                        ? "qrc:/images/workingdirpresent.png"
                                        : "qrc:/images/workingdirpresentempty.png"
                            opacity: .7
                        }

                        onClicked: {
                            var key = utilities.getVideoID(root.lastHoveredLink)
                            if (key !== "")
                                fileSystemModel.addEntry(
                                            key,
                                            root.lastHoveredTooltip,
                                            "",
                                            "",
                                            "")
                        }
                        display: MenuItem.TextBesideIcon
                    }
                    Repeater {
                        model: [
                            WebEngineView.Back,
                            WebEngineView.Forward,
                            WebEngineView.Reload,
                            WebEngineView.Copy,
                            WebEngineView.Paste,
                            WebEngineView.Cut,
                            WebEngineView.CopyLinkToClipboard,
                        ]
                        MenuItem {
                            text: webEngineView.action(modelData).text
                            enabled: webEngineView.action(modelData).enabled
                            onClicked: webEngineView.action(modelData).trigger()
                            icon.source: switch(modelData) {
                                         case WebEngineView.Back:"/icons/arrow_back.svg";break;
                                         case WebEngineView.Forward:"/icons/arrow_forward.svg";break;
                                         case WebEngineView.Reload:"/icons/refresh.svg";break;
                                         case WebEngineView.Copy:"/icons/content_copy.svg";break;
                                         case WebEngineView.Paste:"/icons/content_paste.svg";break;
                                         case WebEngineView.Cut:"/icons/content_cut.svg";break;
                                         case WebEngineView.CopyLinkToClipboard:"/icons/add_link.svg";break;
                                         }
                            display: MenuItem.TextBesideIcon
                        }
                    }
                }

                function isValidHttpUrl(url) {
                    let regEx = /^https?:\/\/(?:www\.)?[-a-zA-Z0-9@:%._\+~#=]{1,256}\.[a-zA-Z0-9()]{1,6}\b([-a-zA-Z0-9()@:%_\+.~#?&//=]*)$/gm;
                    return regEx.test(url);
                }
                onContextMenuRequested: (request) => {
                    {
                        request.accepted = true;
                        contextMenu.popup();
                    }
                }

                onLinkHovered: (hoveredUrl) => {
                    if (hoveredUrl.toString().length > 0
                            && utilities.isYoutubeVideoUrl(hoveredUrl)) {
                        root.lastHoveredLink = hoveredUrl
                    }
                }

                onTooltipRequested: (request) => {
                    if (request.type === TooltipRequest.Show) {
                        root.lastHoveredTooltip = request.text
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

        property Menu contextMenu: BookmarkContextMenu {
            isHistoryView: false
            model: fileSystemModel
            parentView: null
            x: buttonAddVideo.x
            y: buttonAddVideo.y + buttonAddVideo.height
        }

        ColumnLayout {
            anchors.fill: parent
            RowLayout {
                Layout.alignment: Qt.AlignVCenter | Qt.AlignJustify
                Layout.fillWidth: true
                Layout.fillHeight: true
                id: navigationBar

                ToolButton {
                    property int itemAction: WebEngineView.Back
                    text: webEngineView.action(itemAction).text
                    enabled: webEngineView.action(itemAction).enabled
                    onClicked: webEngineView.action(itemAction).trigger()
                    icon.source: "/icons/arrow_back.svg"
                    display: AbstractButton.IconOnly //TextUnderIcon

                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.text: "Go Back"
                    ToolTip.delay: 300
                }

                ToolButton {
                    property int itemAction: WebEngineView.Forward
                    text: webEngineView.action(itemAction).text
                    enabled: webEngineView.action(itemAction).enabled
                    onClicked: webEngineView.action(itemAction).trigger()
                    icon.source: "/icons/arrow_forward.svg"
                    display: AbstractButton.IconOnly

                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.text: "Go Forward"
                    ToolTip.delay: 300
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
                               ? properties.checkedButtonColor
                               : "white"
                    }
                    display: AbstractButton.IconOnly

                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.text: (checked_)
                                  ? "Hide bookmarks pane"
                                  : "Show bookmarks pane"
                    ToolTip.delay: 300

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
                            color: "white"
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
                               ? properties.checkedButtonColor
                               : "white"
                    }

                    display: AbstractButton.IconOnly

                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.text: (checked)
                                  ? "Hide history pane"
                                  : "Show history pane"
                    ToolTip.delay: 300

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
                            color: "white"
                            visible: true
                        }
                    }
                }

                ToolButton {
                    id: reloadButton
                    property int itemAction: webEngineView.loading ? WebEngineView.Stop : WebEngineView.Reload
                    text: webEngineView.action(itemAction).text
                    enabled: webEngineView.action(itemAction).enabled
                    onClicked: webEngineView.action(itemAction).trigger()
                    icon.source: "/icons/" + (webEngineView.loading ? "cancel.svg" : "refresh.svg")
                    display: AbstractButton.IconOnly

                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.text: (webEngineView.loading) ? "Stop" : "Reload"
                    ToolTip.delay: 300
                }

                TextField {
                    Layout.fillWidth: true

                    text: root.url
                    selectByMouse: true
                    onEditingFinished: {}
                    onAccepted: {
                        if (text == root.url)
                            return
                        root.url = text
                    }
                }
                ToolButton {
                    id: buttonAddVideo
                    text: "Add"
                    enabled: false
                    visible: true
                    onClicked: {
                        timePuller.addCurrentVideo()
                    }
                    onPressAndHold: {
                        if (currentVideoAdded)
                            toolBar.contextMenu.setKey(webEngineView.key)
                            toolBar.contextMenu.open()
                    }

                    property bool currentVideoAdded: isCurrentVideoAdded(webEngineView.key,
                                                                         root.addedVideoTrigger)
                    property int workingDirPresent: webEngineView.keyHasWorkingDir

                    icon {
                        source: "/icons/add.svg"
                        color: (currentVideoAdded)
                               ? (enabled)
                                 ? properties.addedTextColor
                                 : properties.addedDisabledTextColor
                               : (enabled)
                                 ? "white"
                                 : properties.disabledTextColor
                    }

                    Image {
                        visible: parent.workingDirPresent == 2
                        anchors {
                            fill: parent
                            rightMargin: 12
                            topMargin: 12
                            bottomMargin: 4
                            leftMargin: 4
                        }

                        source: "qrc:/images/workingdirpresent.png"
                        opacity: .5
                    }
                    Image {
                        visible: parent.workingDirPresent == 1
                        anchors {
                            fill: parent
                            rightMargin: 12
                            topMargin: 12
                            bottomMargin: 4
                            leftMargin: 4
                        }

                        source: "qrc:/images/workingdirpresentempty.png"
                        opacity: .5
                    }
                    display: AbstractButton.IconOnly

                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.text: (currentVideoAdded)
                                  ? "Video already bookmarked"
                                  : "Add Video to Bookmarks"
                    ToolTip.delay: 300

                }
                ToolButton {
                    id: buttonStarVideo
                    text: "Star"
                    enabled: buttonAddVideo.currentVideoAdded
                    visible: true
                    onClicked: {
                        fileSystemModel.starEntry(webEngineView.key, !starred)
                        triggerStarred()
                    }

                    function isStarred(key, counter) {
                        return enabled &&
                                fileSystemModel.isStarred(key)
                    }

                    property int _starredTrigger: 0;
                    function triggerStarred() { _starredTrigger += 1 }
                    property bool starred: isStarred(webEngineView.key, _starredTrigger)

                    icon {
                        source: "/icons/"+(buttonStarVideo.starred
                                                ? "star_fill.svg" : "star.svg")
                    }
                    display: AbstractButton.IconOnly

                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.text: (starred)
                                  ? "Unstar current video"
                                  : "Star current video"
                    ToolTip.delay: 300
                }
                ToolButton {
                    id: buttonCopyLink
                    text: "Copy"
                    enabled: true //root.addVideoEnabled
                    visible: true
                    TextEdit{
                        id: copyLinkClipboardProxy
                        visible: false
                    }
                    onClicked: {
                        copyLinkClipboardProxy.text = root.url
                        copyLinkClipboardProxy.selectAll();
                        copyLinkClipboardProxy.copy()
                        copyLinkClipboardProxy.text = ""
                    }
                    icon.source: "/icons/content_copy.svg"
                    display: AbstractButton.IconOnly

                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.text: "Copy URL to Clipboard"
                    ToolTip.delay: 300
                }

                ToolButton {
                    id: buttonToggleGuide
                    text: "Toggle Guide panel"
                    enabled: webEngineView.isYoutubeHome
                    visible: true
                    checkable: true
                    checked: false

                    onCheckedChanged: {
                        timePuller.clickGuideButton()
                    }

                    icon.source: "/icons/menu.svg"
                    display: AbstractButton.IconOnly

                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.text: "Toggle Guide panel"
                    ToolTip.delay: 300
                }

                ToolButton {
                    id: buttonToggleJS
                    text: "Activate/Deactivate custom script"
                    enabled: settings.customScript !== ""
                    visible: enabled
                    checkable: true
                    checked: true

                    onCheckedChanged: {
                        timePuller.clickGuideButton()
                    }

                    icon.source: "/icons/js.svg"
                    display: AbstractButton.IconOnly

                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.text: "Toggle custom script"
                    ToolTip.delay: 300
                }


                ToolButton {
                    id: buttonSpeed
                    enabled: webEngineView.isYoutubeVideo
                    visible: true
                    checkable: true

                    onCheckedChanged: {
                        if (checked) {
                            ToolTip.toolTip.close()
                            playbackRateMenu.open()
                        } else {
                            playbackRateMenu.close()
                        }
                    }

                    icon.source: "/icons/speed.svg"

                    display: (text !== "1.00")
                             ? AbstractButton.TextUnderIcon
                             : AbstractButton.IconOnly
                    text: (timePuller.playbackRate) ? timePuller.playbackRate.toFixed(2) : "1.00"
                    spacing: -6

                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.text: "Set playback rate"
                    ToolTip.delay: 300
                }

                ToolButton {
                    id: buttonPlayPause
                    enabled: webEngineView.isYoutubeVideo && (timePuller.playerState !== -1)
                    visible: true
                    checkable: false

                    onClicked: {
                        var scriptToRun
                        if (timePuller.playerState === 1 && webEngineView.key !== "")
                            scriptToRun = internals.getPauseVideoScript(webEngineView.isShorts)
// Br0ken, try https://stackoverflow.com/a/58581660/962856, because .click() also doesn't work
//                        else if (timePuller.playerState === -1)
//                            scriptToRun = internals.getPlayNextVideoScript(utilities.isYoutubeShortsUrl(root.url))
                        else
                            scriptToRun = internals.getPlayVideoScript(webEngineView.isShorts)

//                        console.log(timePuller.playerState, scriptToRun)
                        root.runScript(scriptToRun)
                    }

                    icon.source: (timePuller.playerState === 1)
                                    ? "/icons/pause.svg"
                                    : "/icons/play_arrow.svg"

                    display: AbstractButton.IconOnly

                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.text: (timePuller.playerState === 1)
                                    ? "Pause video"
                                    : "Play video"
                    ToolTip.delay: 300
                }

                MouseArea {
                    height: sliderVolume.height
                    width: sliderVolume.implicitWidth
                    hoverEnabled: true
                    property bool hovered: false
                    onEntered: hovered = true
                    onExited: hovered = false
                    Slider {
                        id: sliderVolume
                        enabled: webEngineView.isYoutubeVideo
                        implicitWidth: 130
                        anchors.centerIn: parent

                        value: 0
                        from: 0
                        stepSize: 1
                        to: 100
                        snapMode: Slider.SnapAlways
                        property real userValue: -1

                        function setVolume() {
                            var newVolume = (userValue >= 0) ? userValue : value

                            var scriptToRun = internals.getVolumeSetterScript(newVolume, utilities.isYoutubeShortsUrl(root.url))
                            root.runScript(scriptToRun)
                        }

                        onUserValueChanged: {
                            setVolume()
                        }

                        onValueChanged: {
                            setVolume()
                        }

                        onMoved: {
                            userValue = value
                        }

                        ToolTip {
                            parent: sliderVolume.handle
                            visible: sliderVolume.pressed || sliderVolume.parent.hovered
                            text: sliderVolume.value.toFixed(0)
                        }
                    }
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
            }
        }
    } // header

    Menu {
        id: playbackRateMenu
        y: 0 + toolBar.height
        x: buttonSpeed.x + buttonSpeed.width - width
        width: 48
        visible: false
        ColumnLayout {
            width: parent.width
            Repeater {
                model: internals.videoSpeeds
                ToolButton {
                    height: playbackRateMenu.width
                    width: height
                    enabled: true
                    checkable: false
                    checked: timePuller.playbackRate.toFixed(2) === text

                    z: playbackRateMenu.z + 5

                    text: internals.videoSpeeds[index]

                    display: AbstractButton.TextOnly

                    onClicked: {
                        buttonSpeed.checked = false
                        var scriptToRun = internals.getPlaybackRateSetterScript(
                                    text, utilities.isYoutubeShortsUrl(root.url)
                                 )
    //                    console.log(scriptToRun)
                        webEngineView.runJavaScript(scriptToRun)
                    }

                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.text: "Set playback rate to " + text
                    ToolTip.delay: 300
                }
            }
        }
    }

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
                    height: properties.fsH3 * 1.5
                    Label {
                        anchors {
                            topMargin: 4
                            centerIn: parent
                        }
                        text: "<b>Proxy Settings</b>"
                        font.pixelSize: properties.fsH3
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
                    height: properties.fsH3 * 1.5
                    Label {
                        anchors {
                            topMargin: 4
                            centerIn: parent
                        }
                        text: "<b>Settings</b>"
                        font.pixelSize: properties.fsH3
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

                height: 210
                color: "transparent"
                border.color: "transparent"


                ScrollView {
                    anchors.fill: parent
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width: settingsScrollViewContainer.width
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
                                    color: "white"
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
                                        text: "Chromium cookies path:\n" + root.profilePath
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
                                text: (root.profilePath === "") ? "<undefined>" : root.profilePath
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
                                ToolTip.text: "Chromium cookies path:\n" + root.profilePath
                            }
                            Button {
                                flat: true
                                display: Button.IconOnly
                                icon.source: "/icons/delete_forever.svg"
                                Layout.alignment: Qt.AlignVCenter
                                Layout.leftMargin: -16
                                Layout.rightMargin: 0
                                onClicked: root.profilePath = ""
                                hoverEnabled: true

                                ToolTip.visible: hovered
                                ToolTip.delay: 300
                                ToolTip.text: "Clear Chromium cookies path"
                            }
                        } // GridLayout
                        GridLayout {
                            width: parent.width
                            columns: 8
                            rowSpacing: 16
                            columnSpacing: 16
                            visible: root.debugMode

                            // easylist
                            Image {
                                width: 32
                                height: 32
                                Layout.preferredWidth: width
                                Layout.preferredHeight: height
                                Layout.alignment: Qt.AlignVCenter
                                source: "qrc:/images/ad-blocker-fi-128.png"

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    property bool hovered: false
                                    onEntered:  hovered = true
                                    onExited: hovered = false

                                    ToolTip {
                                        visible: parent.hovered
                                        y: parent.height * 0.12
                                        text: "easylist.txt path (find it at https://easylist.to/easylist/easylist.txt):\n" + root.profilePath
                                        delay: 300
                                    }
                                }
                            }
                            Label {
                                text: "Easylist:"
                                Layout.alignment: Qt.AlignVCenter
                            }
                            Label {
                                Layout.columnSpan: 4
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignLeft
                                text: (root.easyListPath === "") ? "<undefined>" : root.easyListPath
                            }
                            Button {
                                id: buttonOpenEasylist
                                flat: true
                                display: Button.IconOnly
                                icon.source: "/icons/folder_open.svg"
                                Layout.alignment: Qt.AlignVCenter
                                onClicked: fileDialogEasylist.open()
                                hoverEnabled: true

                                ToolTip.visible: hovered
                                ToolTip.delay: 300
                                ToolTip.text: "easylist.txt path (find it at https://easylist.to/easylist/easylist.txt):\n" + root.profilePath
                            }
                            Button {
                                flat: true
                                display: Button.IconOnly
                                icon.source: "/icons/delete_forever.svg"
                                Layout.alignment: Qt.AlignVCenter
                                Layout.leftMargin: -16
                                Layout.rightMargin: 0
                                onClicked: root.easyListPath = ""
                                hoverEnabled: true

                                ToolTip.visible: hovered
                                ToolTip.delay: 300
                                ToolTip.text: "Clear easylist path"
                            }
                            // easylist end

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
                                    color: "white"
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
                                            color: "white"
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
                                        font.pixelSize: properties.fsP1
                                        cursorVisible: true
                                        color: properties.textColor

                                        text: modelData.name
                                        onTextChanged: {
                                            root.externalCommands[index].name = text
                                        }

                                        ToolTip.visible: hovered
                                        ToolTip.delay: 300
                                        ToolTip.text: "Name of the external command on the menu:\n" + extCmdName.text

                                    }
                                    TextField {
                                        id: extCmdCmd
                                        focus: true
                                        selectByMouse: true
                                        font.pixelSize: properties.fsP1
                                        cursorVisible: true
                                        color: properties.textColor
                                        Layout.fillWidth: true

                                        width: parent.width - parent.spacing - extCmdName.width

                                        text: modelData.command
                                        onTextChanged: {
                                            if (utilities.executableExists(text)) {
                                                root.externalCommands[index].command = text
                                                color = properties.textColor
                                            } else {
                                                color = "firebrick"
                                            }
                                        }

                                        ToolTip.visible: hovered
                                        ToolTip.delay: 300
                                        ToolTip.text: "External command to trigger through context menu:\n" + extCmdCmd.text

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

                RowLayout {
                    id: settingsButtonsLayout
                    anchors.centerIn: parent
                    width: parent.width
                    spacing: 0

                    Row {
                        Layout.alignment: Qt.AlignLeft
                        CheckBox {
                            Layout.alignment: Qt.AlignLeft
                            id: darkModeCheck
                            checked: root.darkMode
                            text: qsTr("Dark mode\n(requires restart)")
                            onCheckedChanged: {
                                root.darkMode = checked
                            }
                        }
                        CheckBox {

                            id: debugModeCHeck
                            checked: root.debugMode
                            text: qsTr("Developer\nmode")
                            onCheckedChanged: {
                                root.debugMode = checked
                            }
                        }

                        CheckBox {

                            id: deleteStorageCheck
                            visible: root.debugMode
                            checked: root.removeStorageOnDelete
                            text: qsTr("Delete\nstorage")
                            onCheckedChanged: {
                                root.removeStorageOnDelete = checked
                            }

                            hoverEnabled: true

                            ToolTip.visible: hovered
                            ToolTip.delay: 300
                            ToolTip.text: "Controls whether to erase related video data within the working directory for external executable (if specified) upon deletion"
                        }
                        CheckBox {

                            id: blankWhenHiddenCheck
                            visible: root.debugMode
                            checked: root.blankWhenHidden
                            text: qsTr("Blank when\ninvisible")
                            onCheckedChanged: {
                                root.blankWhenHidden = checked
                            }

                            hoverEnabled: true

                            ToolTip.visible: hovered
                            ToolTip.delay: 300
                            ToolTip.text: "Controls whether to change the URL to about:blank when YAYC is minimized to save CPU"
                        }
                    }
                    Row {
                        Layout.alignment: Qt.AlignLRight
                        Layout.rightMargin: -32
                        Button {
                            id: buttonOpenProxyDialog
                            flat: true
                            display: Button.TextOnly
                            text: "Proxy\nSettings"
                            onClicked: proxyMenu.open()
                            hoverEnabled: true

                            ToolTip.visible: hovered
                            ToolTip.delay: 300
                            ToolTip.text: "Edit the proxy settings used to access the network"
                        }
                        Button {
                            id: buttonOpenJSDialog
                            flat: true
                            display: Button.TextOnly
                            visible: root.debugMode
                            text: "Custom\nScript"
                            onClicked: customScriptDialog.open()
                            hoverEnabled: true

                            ToolTip.visible: hovered
                            ToolTip.delay: 300
                            ToolTip.text: "Edit the custom script that is run after loading a video page"
                        }
                        Button {
                            id: buttonResetSettings
                            flat: true
                            visible: root.debugMode
                            enabled: true
                            display: Button.IconOnly
                            icon.source: "/icons/restart.svg"
                            text: "Custom\nScript"
                            onClicked: utilities.clearSettings()
                            hoverEnabled: true

                            ToolTip.visible: hovered
                            ToolTip.delay: 300
                            ToolTip.text: "Clear all settings (restarts YAYC)"
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
                    visible: utilities.compareSemver(appVersion, root.lastestRemoteVersion) > 0
                    color: (maNewVersion.hovered)
                            ? Qt.rgba(1,1,1,0.1)
                            : "transparent"
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
                            pixelSize: properties.fsH2
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
                        onClicked: {
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
                        Image {
                            id: infoimg
                            width: 32
                            height: 32
                            anchors.verticalCenter: parent.verticalCenter
                            source: "/icons/info.svg"
                            ColorOverlay {
                                source: infoimg
                                anchors.fill: infoimg
                                color: "white"
                            }
                        }
                        Rectangle {
                            width: aboutRow.width + 16
                            height: buttonOpenGProfile.height
                            Layout.alignment: Qt.AlignLeft
                            color: (maAbout.hovered) ? Qt.rgba(1,1,1,0.1) : "transparent"
                            Row {
                                id: aboutRow
                                anchors.centerIn: parent
                                Label {
                                    id: aboutLabel
                                    text: "About "
                                    font.pixelSize: properties.fsP1
                                }
                                Image {
                                    anchors.verticalCenter: parent.verticalCenter
                                    source: "/images/yayc-inlined.png"
                                    fillMode: Image.PreserveAspectFit
                                    height: properties.fsP1
                                    mipmap: true
                                    smooth: true
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
                        Image {
                            id: helpImg
                            width: 32
                            height: 32

                            anchors.verticalCenter: parent.verticalCenter
                            source: "/icons/help.svg"
                            ColorOverlay {
                                source: helpImg
                                anchors.fill: helpImg
                                color: "white"
                            }
                        }
                        Rectangle {
                            width: helpLabel.width + 16
                            height: buttonOpenGProfile.height
                            Layout.alignment: Qt.AlignLeft
                            color: (maHelp.hovered) ? Qt.rgba(1,1,1,0.1) : "transparent"
                            Label {
                                anchors.centerIn: parent
                                id: helpLabel
                                text: "Help"
                                font.pixelSize: properties.fsP1
                            }
                            MouseArea {
                                id: maHelp
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                property bool hovered: false
                                onEntered: hovered = true
                                onExited: hovered = false
                                onClicked: {
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
                            && donateButton.enabled) ? Qt.rgba(0.94,0.6,0.6,0.6)
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
                            onClicked: {
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
            height: properties.fsH3 * 1.5
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
                        height: properties.fsH2
                        mipmap: true
                        smooth: true
                    }
                }

                Label {
                    id: aboutTitleVersion
                    text: "  v"+appVersion
                    font.pixelSize: properties.fsH2
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
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
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
                            font.pixelSize: properties.fsP2 * 1.05
                        }

                        Image {
                            height: properties.fsP2
                            fillMode: Image.PreserveAspectFit
                            source: "/images/by-nc-sa_15.svg"
                            smooth: true
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 1

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
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

                            font.pixelSize: properties.fsP1
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
                            pixelSize: properties.fsH4
                        }
                    }
                    Rectangle {
                        color: "transparent"
                        height: 200
                        width: parent.width

                        ScrollView {
                            anchors.fill: parent

                            TextArea {
                                font.pixelSize: properties.fsP1
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
                                    pixelSize: properties.fsH4
                                }
                            }
                            Label {
                                id: labelIssues
                                text: '<a href="' + repositoryURL + '/issues">Get involved</a>'
                                font {
                                    bold: true
                                    pixelSize: properties.fsH4
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
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
                                    pixelSize: properties.fsH4
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
                                    pixelSize: properties.fsH4
                                }
                                onLinkActivated: Qt.openUrlExternally(link)
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
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
            height: properties.fsH3 * 1.5
            Row {
                anchors.centerIn: parent
                topPadding: 8
                Label {
                    text: "Help Center"
                    font.pixelSize: properties.fsH2
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
                            color: "white"
                            font.family: mainFont.name
                            font.pixelSize: properties.fsP1
                            text: helpContainer.tooltips[index]
                        }
                        background: Rectangle {
                            color: Qt.rgba(.1,.1,.1,0.65)
                            border.color: Qt.rgba(1,1,1,0.15)
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
            height: properties.fsH3 * 1.5
            Row {
                anchors.centerIn: parent
                topPadding: 8
                Label {
                    text: "Custom JS script"
                    font.pixelSize: properties.fsH2
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
            height: properties.fsH3 * 1.5
            Row {
                anchors.centerIn: parent
                topPadding: 8

                Label {
                    text: "Disclaimer"
                    font.pixelSize: properties.fsH2
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
                                font.pixelSize: properties.fsP1
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
                font.pixelSize: properties.fsP1
                cursorVisible: true
                color: properties.textColor
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
                font.pixelSize: properties.fsP1
                cursorVisible: true
                color: properties.textColor
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

    QQD.FileDialog {
        id: fileDialogEasylist
        nameFilters: []
        title: "Please choose easylist.txt"
        options: QQD.FileDialog.ReadOnly

        onAccepted: {
            fileDialogEasylist.close()
            var path = String(fileDialogEasylist.fileUrl)
            root.easyListPath = root.deUrlizePath(path)
            requestInterceptor.setEasyListPath(root.easyListPath)
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
            root.profilePath = root.deUrlizePath(path)
        }
        onRejected: {
        }
    }
}
