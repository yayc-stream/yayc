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

/*
  == Known issues ==

  - webengineview onLinkHovered does not provide text.
    The workaround used here is tricky as it requires hovering the link with the tooltip,
    and as soon as the tooltip is shown, grab the text from the tooltip.
    THe side effect is that if a video is added without making the respective tooltip pop first,
    old tooltip text will be picked up. This will however be updated upon video opening.

    Alternative solution: fetch the title in the background through a regular get request +
    page scraping. This would likely help also add video dialogs.

  - QQ1.TreeView is terribly buggy.
    consider using https://code.qt.io/cgit/qt-extensions/qttreeview.git/about/ or
    another alternative. qttreeview needs to be checked against partial model updates
*/

ApplicationWindow {
    id: root
    objectName: "root"
    height: Screen.height;
    width: Screen.width
    visible: true
    title: qsTr("YAYC")


    property alias addVideoEnabled: buttonAddVideo.enabled

    property int addedVideoTrigger: 0
    function triggerVideoAdded() { addedVideoTrigger += 1 }
    property url url: "https://youtube.com"
    property bool filesystemModelReady: false

    function quit() {
        syncAll()
        Qt.quit()
    }

    function minimizeToTray() {
        syncAll()
        root.hide()
    }

    Shortcut {
        sequence: StandardKey.Quit
        onActivated: {
            root.quit()
        }
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
        sequence: "Ctrl+H"
        onActivated: {
            root.minimizeToTray()
        }
    }

    onClosing: {
        close.accepted = false
        root.minimizeToTray()
    }

    QLP.SystemTrayIcon {
        visible: true
        icon.source: "qrc:/images/yayc-alt.png"
        menu: QLP.Menu {
            QLP.MenuItem {
                text: (root.visibility == Window.Hidden)
                        ? qsTr("Show")
                        : qsTr("Minimize to Tray")
                onTriggered: {
                    if (root.visibility == Window.Hidden) {
                        root.show()
                        root.raise()
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
                if (root.visible)
                    root.hide()
                else
                    root.show()
            }
        }
    }

    property string profilePath // if empty, the webengineview profile will turn itself "off the record"
    property string youtubePath
    property string historyPath
    property string easyListPath
    property string extWorkingDirPath
    property bool extWorkingDirExists: root.extWorkingDirPath !== ""
    property string extCommand
    property bool extCommandEnabled: (root.extWorkingDirExists
                                      && root.extCommand !== "")
    property string extCommandName

    property bool firstRun: true
    property bool limitationOfLiabilityAccepted: false

    property string lastestRemoteVersion: appVersion
    property var lastVersionCheckDate
    property string donateUrl
    property string donateUrlETag
    property string customScript
    property bool darkMode: true
    property bool debugMode: false
    property real wevZoomFactor: 1.0

    Settings {
        id: settings
        property alias lolAccepted: root.limitationOfLiabilityAccepted
        property alias firstRun: root.firstRun
        property alias profilePath: root.profilePath
        property alias youtubePath: root.youtubePath
        property alias historyPath: root.historyPath
        property alias easyListPath: root.easyListPath
        property alias extWorkingDirPath: root.extWorkingDirPath
        property alias extCommand: root.extCommand
        property alias extCommandName: root.extCommandName
        property alias lastUrl: webEngineView.url
        property alias lastestRemoteVersion: root.lastestRemoteVersion
        property alias lastVersionCheckDate: root.lastVersionCheckDate
        property alias donateUrl: root.donateUrl
        property alias donateUrlETag: root.donateUrlETag
        property alias customScript: root.customScript
        property alias darkMode: root.darkMode
        property alias debugMode: root.debugMode
        property alias wevZoomFactor: root.wevZoomFactor
        property var splitView

        Component.onCompleted: {
            disclaimerContainer.visible = Qt.binding(function() { return !settings.lolAccepted })
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
    }

    Component.onCompleted:  {
        utilities.networkFound.connect(onNetworkFound)
        utilities.latestVersion.connect(onLatestVersionFound)
        utilities.donateETag.connect(onDonateETag)
        utilities.donateUrl.connect(onDonateUrl)
        fileSystemModel.directoryLoaded.connect(onFSmodelDirectoryLoaded)
        fileSystemModel.filesAdded.connect(onFSModelFilesAdded)
        if (youtubePath !== "")
            fileSystemModel.setRoot(youtubePath)
        if (historyPath !== "")
            historyModel.setRoot(historyPath)
        if (easyListPath !== "")
            requestInterceptor.setEasyListPath(easyListPath)
        splitView.restoreState(settings.splitView)
    }
    Component.onDestruction: {
        settings.splitView = splitView.saveState()
    }

    onYoutubePathChanged: { // this might be triggering double setRoot. move it into fileDialog?
        settings.sync()
        if (youtubePath !== "") {
            fileSystemModel.setRoot(youtubePath)
        }
    }

    onHistoryPathChanged: {
        settings.sync()
        if (historyPath !== "")
            historyModel.setRoot(historyPath)
    }

    onProfilePathChanged: {
        webEngineView.reload()
        settings.sync()
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
            var intervalSeconds = 3600 * 24 // don't check more often than once per day
            if (diffSeconds < intervalSeconds) {
                return;
            }
        }
        // Kick version checker
        utilities.getLatestVersion()
        utilities.getDonateEtag()
    }

    function compareAppVersion(version1,version2) { // true if 1 < 2
        var result=false;

        if(typeof version1!=='object'){ version1=version1.toString().split('.'); }
        if(typeof version2!=='object'){ version2=version2.toString().split('.'); }

        for(var i=0;i<(Math.max(version1.length,version2.length));i++){

            if(version1[i]==undefined){ version1[i]=0; }
            if(version2[i]==undefined){ version2[i]=0; }

            if(Number(version1[i])<Number(version2[i])){
                result=true;
                break;
            }
            if(version1[i]!=version2[i]){
                break;
            }
        }
        return(result);
    }

    function onLatestVersionFound(latestVersion) {
        var now = new Date()
        root.lastVersionCheckDate = now
        var previousRemoteVersion = root.lastestRemoteVersion
        root.lastestRemoteVersion = latestVersion

        if (compareAppVersion(previousRemoteVersion, latestVersion)) { // if latest is greater
            // highlight settings
            root.firstRun = true
        }
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
        root.wevZoomFactor = webEngineView.zoomFactor
        fileSystemModel.sync()
        historyModel.sync();
        settings.sync()
    }

    function deUrlizePath(path) {
        return path.slice(7) // strip file://
    }

    function isCurrentVideoAdded(key, trigger) {
        if (!utilities.isYoutubeVideoUrl(webEngineView.url))
            return false;
        return fileSystemModel.isVideoBookmarked(key)
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

    property string httpUserAgent: "Mozilla/5.0 (X11; Linux x86_64; rv:90.0) Gecko/20100101 Firefox/90.0"
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

        property string script_videoTime: "
            var backend;
            new QWebChannel(qt.webChannelTransport, function (channel) {
                backend = channel.objects.backend;
            });
            setTimeout(function() {  //function puller()

                backend.channelURL = document.getElementById('text').firstChild.href;
                backend.channelName = document.getElementById('text').firstChild.text;
                backend.channelAvatar = document.getElementById('owner').firstElementChild.firstElementChild.firstElementChild.firstElementChild.src;

                ytplayer = document.getElementById('movie_player');

                backend.videoTitle = ytplayer.getVideoData().title;
                backend.videoDuration = ytplayer.getDuration();
                backend.videoPosition = ytplayer.getCurrentTime();
            }, 100);
            //puller();
        "

        property string script_backend: "
            var backend;
            new QWebChannel(qt.webChannelTransport, function (channel) {
                backend = channel.objects.backend;
            });
        "
        property string script_videoTitleShorts: "
            var backend;
            new QWebChannel(qt.webChannelTransport, function (channel) {
                backend = channel.objects.backend;
            });
            setTimeout(function() {
                var activeShort = document.querySelectorAll('ytd-reel-video-renderer[is-active]')[0].querySelector('div[id=\"channel-info\"]')
                backend.channelURL = activeShort.children[0].href
                backend.channelName = activeShort.children[1].getElementsByTagName('yt-formatted-string')[0].textContent
                backend.channelAvatar = activeShort.firstElementChild.firstElementChild.firstElementChild.src

                backend.videoTitle = document.title;
                //console.log(document.title);
            }, 100);
        "
    }

    QtObject {
        id: timePuller

        // ID, under which this object will be known at WebEngineView side
        WebChannel.id: "backend"

        property real videoPosition: 0
        property real videoDuration: 0
        property string videoTitle
        property string channelURL
        property string channelName
        property string channelAvatar
        property string keyBefore

        onVideoPositionChanged: {
            if (webEngineView.key != keyBefore) {
                root.addVideoEnabled = false
                // console.log("timePuller data changed while URL changed")
                return
            }
            //console.log("CHAN: ",channelURL, channelName, channelAvatar)
            root.addVideoEnabled = true
            // it's not a short, url didn't change, position changed
            update()
        }

        onVideoTitleChanged: {
            if (webEngineView.key != keyBefore) {
                root.addVideoEnabled = false
                // console.log("timePuller data changed while URL changed")
                return
            }
            root.addVideoEnabled = true
            if (!utilities.isYoutubeShortsUrl(webEngineView.url)) {
                // silently ignore
                return;
            }
            if (videoTitle === "")
                return;

            // it's a short, url didn't change, and title is not null
            updateShort()
        }

        function update() {
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
        function updateShort() {
            fileSystemModel.updateEntry(webEngineView.key,
                                        videoTitle,
                                        channelURL,
                                        channelAvatar,
                                        channelName)
            if (!historyModel.updateEntry(webEngineView.key,
                                          videoTitle,
                                          channelURL,
                                          channelAvatar,
                                          channelName)) {
                historyModel.addEntry(webEngineView.key,
                                      videoTitle,
                                      channelURL,
                                      channelAvatar,
                                      channelName)
            }
        }
        function addCurrentVideo() {
            if (!utilities.isYoutubeVideoUrl(webEngineView.url)) {
                // Q_UNREACHABLE
                return;
            }
            if (webEngineView.key != keyBefore) {
                root.addVideoEnabled = false
                return
            }

            if (utilities.isYoutubeShortsUrl(webEngineView.url)) {
                fileSystemModel.addEntry(webEngineView.key,
                                         videoTitle,
                                         channelURL,
                                         channelAvatar,
                                         channelName)
            } else {
                fileSystemModel.addEntry(webEngineView.key,
                                         videoTitle,
                                         channelURL,
                                         channelAvatar,
                                         channelName,
                                         videoDuration,
                                         videoPosition)
            }
            root.triggerVideoAdded()
        }
    } // timePuller

    WebChannel {
        id : web_channel
        registeredObjects: [timePuller]
    }

    Item {
        id: mainContainer
        anchors.fill: parent
        SplitView {
            id: splitView
            anchors.fill: parent
            Rectangle {
                anchors {
                    top: parent.top
                    bottom: parent.bottom
                }
                implicitWidth: 200
                color: "black"
                enabled: true
                id: treeViewContainer

                property Menu contextMenu: Menu {
                    cascade: true
                    property bool deleteCategoryItem: false
                    property bool deleteVideoItem: false
                    property var categoryIndex
                    property var videoIndex
                    property string key: ""
                    onOpened: {
                        // workaround for the submenu occasionally showing opened
                        extAppMenu.close()
                    }

                    function setCategoryIndex(idx) {
                        categoryIndex = idx
                        deleteCategoryItem = true
                        deleteVideoItem = false
                    }

                    function setVideoIndex(idx) {
                        videoIndex = idx
                        key = fileSystemModel.keyFromViewItem(idx)
                        deleteCategoryItem = false
                        deleteVideoItem = true
                    }

                    MenuItem {
                        text: "Add category"
                        enabled: true
                        height: enabled ? implicitHeight : 0
                        onClicked: {
                            addCategoryDialog.open()
                        }
                        icon.source: "/icons/create_new_folder.svg"
                        display: MenuItem.TextBesideIcon
                    }
                    MenuItem {
                        text: "Add video"
                        enabled: true
                        height: enabled ? implicitHeight : 0
                        onClicked: {
                            addVideoDialog.open()
                        }
                        icon.source: "/icons/add.svg"
                        display: MenuItem.TextBesideIcon
                    }
                    MenuItem {
                        text: "Delete category"
                        enabled: treeViewContainer.contextMenu.deleteCategoryItem
                        visible: true
                        height: enabled ? implicitHeight : 0
                        onClicked: {
                            fileSystemModel.deleteEntry(treeViewContainer.contextMenu.categoryIndex)
                        }
                        icon.source: "/icons/folder_delete.svg"
                        display: MenuItem.TextBesideIcon
                    }
                    MenuItem {
                        text: "Delete video"
                        enabled: treeViewContainer.contextMenu.deleteVideoItem
                        visible: true
                        height: enabled ? implicitHeight : 0
                        onClicked: {
                            fileSystemModel.deleteEntry(treeViewContainer.contextMenu.videoIndex)
                            root.triggerVideoAdded()
                        }
                        icon.source: "/icons/remove.svg"
                        display: MenuItem.TextBesideIcon
                    }
//                    MenuItem {
//                        text: "Download video"
//                        enabled: treeViewContainer.contextMenu.deleteVideoItem
//                        visible: true
//                        height: enabled ? implicitHeight : 0
//                        onClicked: {
//                            fileSystemModel.downloadEntry(treeViewContainer.contextMenu.videoIndex)
//                        }
//                        icon.source: "/icons/download_for_offline.svg"
//                        display: MenuItem.TextBesideIcon
//                    }

                    MenuItem {
                        TextEdit{
                            id: copyLinkClipboardProxy
                            visible: false
                        }
                        text: "Copy Link"
                        enabled: treeViewContainer.contextMenu.deleteVideoItem
                        visible: true
                        height: enabled ? implicitHeight : 0
                        onClicked: {
                            copyLinkClipboardProxy.text = fileSystemModel.videoUrl(treeViewContainer.contextMenu.videoIndex)
                            copyLinkClipboardProxy.selectAll();
                            copyLinkClipboardProxy.copy()
                        }
                        icon.source: "/icons/content_copy.svg"
                        display: MenuItem.TextBesideIcon
                    }
                    MenuItem {
                        text: "Toggle Star"
                        enabled: treeViewContainer.contextMenu.deleteVideoItem
                        visible: true
                        height: enabled ? implicitHeight : 0
                        onClicked: {
                            var starred = fileSystemModel.isStarred(treeViewContainer.contextMenu.videoIndex)
                            fileSystemModel.starEntry(treeViewContainer.contextMenu.videoIndex, !starred)
                            buttonStarVideo.triggerStarred()
                        }
                        icon.source: "/icons/"+(fileSystemModel.isStarred(treeViewContainer.contextMenu.videoIndex)
                                                ? "star_fill.svg" : "star.svg")
                        display: MenuItem.TextBesideIcon
                    }
                    MenuItem {
                        text: "Toggle Viewed"
                        enabled: treeViewContainer.contextMenu.deleteVideoItem
                        visible: true
                        height: enabled ? implicitHeight : 0
                        onClicked: {
                            var viewed = fileSystemModel.isViewed(treeViewContainer.contextMenu.videoIndex)
                            fileSystemModel.viewEntry(treeViewContainer.contextMenu.videoIndex, !viewed)
                        }
                        icon.source: "/icons/"+(fileSystemModel.isViewed(treeViewContainer.contextMenu.videoIndex)
                                                ? "check_circle_fill.svg" : "check_circle.svg")
                        display: MenuItem.TextBesideIcon
                    }
                    MenuItem {
                        text: (view.selectedKey !== treeViewContainer.contextMenu.key)
                              ? "Cut"
                              : "Un-Cut"
                        enabled: treeViewContainer.contextMenu.deleteVideoItem
                        visible: true
                        height: enabled ? implicitHeight : 0
                        onClicked: {
                            if (view.selectedKey !== treeViewContainer.contextMenu.key) {
                                view.selectedKey = treeViewContainer.contextMenu.key
                            } else {
                                view.selectedKey = ""
                            }
                        }
                        icon.source: "/icons/content_cut.svg"
                        display: MenuItem.TextBesideIcon
                    }
                    MenuItem {
                        text: "Paste"
                        enabled: treeViewContainer.contextMenu.deleteCategoryItem
                                 && (view.selectedKey !== "")
                        visible: true
                        height: enabled ? implicitHeight : 0
                        onClicked: {
                            var key = view.selectedKey
                            view.selectedKey = ""
                            var res = fileSystemModel.moveVideo(key, treeViewContainer.contextMenu.categoryIndex)
                        }
                        icon.source: "/icons/content_paste.svg"
                        display: MenuItem.TextBesideIcon
                    }
                    MenuItem {
                        text: "Open containing folder"
                        enabled: treeViewContainer.contextMenu.deleteVideoItem
                                 && root.extWorkingDirExists
                                 && fileSystemModel.hasWorkingDir(treeViewContainer.contextMenu.videoIndex,
                                                                  root.extWorkingDirPath)
                        visible: true
                        height: enabled ? implicitHeight : 0
                        onClicked: {
                            fileSystemModel.openInBrowser(treeViewContainer.contextMenu.videoIndex,
                                                          root.extWorkingDirPath)
                        }
                        icon.source: "/icons/open_in_browser.svg"
                        display: MenuItem.TextBesideIcon
                    }
                    Menu {
                        id: extAppMenu
                        title: "Launch in external app"
                        enabled: treeViewContainer.contextMenu.deleteVideoItem
                                 && root.extCommandEnabled // ToDo: enable only if related dir present in external dir
                        visible: enabled
                        height: enabled ? implicitHeight : 0

                        Repeater {
                            model: 1
                            MenuItem {
                                text: root.extCommandName
                                enabled: treeViewContainer.contextMenu.deleteVideoItem
                                         && root.extCommandEnabled // ToDo: enable only if related dir present in external dir
                                visible: true
                                height: enabled ? implicitHeight : 0
                                onClicked: {
                                    fileSystemModel.openInExternalApp(treeViewContainer.contextMenu.videoIndex,
                                                                      root.extCommand,
                                                                      root.extWorkingDirPath)
                                }
                                icon.source: "/icons/extension.svg"
                                display: MenuItem.TextBesideIcon
                            }
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: {
                        if (mouse.button === Qt.RightButton) {
                            parent.contextMenu.deleteCategoryItem = parent.contextMenu.deleteVideoItem = false
                            parent.contextMenu.popup()
                        }
                    }
                }

                QC1.TreeView {
                    id: view

                    anchors.fill: parent
                    model: (fileSystemModel) ? fileSystemModel.sortFilterProxyModel : undefined
                    rootIndex: (fileSystemModel) ? fileSystemModel.rootPathIndex : undefined
                    selectionMode: 0

                    focus: true
                    headerVisible: false
                    alternatingRowColors: false
                    backgroundVisible: false
                    property string selectedKey: ""

                    QC1.TableViewColumn {
                        title: "Name"
                        role: "fileName"
                        resizable: true
                    }

                    style: QC1S.TreeViewStyle {
                        textColor: properties.textColor
                        highlightedTextColor: properties.textColor
                        backgroundColor: properties.paneBackgroundColor
                        alternateBackgroundColor: properties.paneBackgroundColor
                        branchDelegate: Item {
                            width: 16
                            height: 16
                            Image {
                                visible: styleData.column === 0 && styleData.hasChildren
                                anchors.fill: parent
                                anchors.verticalCenterOffset: 2
                                source: "qrc:/images/arrow.png"
                                transform: Rotation {
                                    origin.x: width / 2
                                    origin.y: height / 2
                                    angle: styleData.isExpanded ? 0 : -90
                                }
                            }
                        }

                        itemDelegate: Rectangle {
                            id: treeViewDelegate
                            readonly property int defaultLineHeight: 26 // turns out to be 26/28 on standard desktop, in absence of uncommon characters
                            height: Math.round(defaultLineHeight * 1.1)
                            border.color: (ma.drag.active)
                                          ? "red"
                                          : (!styleData.hasChildren && view.selectedKey === key)
                                            ? "maroon"
                                            : (da.hovered
                                               ? "green"
                                               : "transparent")
                            border.width: 2
                            color: (styleData.hasChildren)
                                   ? properties.categoryBgColor
                                   : properties.fileBgColor
                            property var qmodelindex: styleData.index
                            property string key: (!styleData.hasChildren)
                                                 ? styleData.value //fileSystemModel.keyFromViewItem(qmodelindex)
                                                 : ""

                            property real duration: (!styleData.hasChildren)
                                                    ? fileSystemModel.duration(qmodelindex)
                                                    : 0
                            property real progress: (!styleData.hasChildren)
                                                    ? fileSystemModel.progress(key)
                                                    : 0
                            property string title: fileSystemModel.title(qmodelindex) // with key it doesn't update, somehow
                            property bool playing: (!styleData.hasChildren)
                                                   ? (webEngineView.key === key)
                                                   : false
                            property string videoUrl: (!styleData.hasChildren)
                                                      ? fileSystemModel.videoUrl(qmodelindex)
                                                      : ""
                            property string videoIconUrl: (!styleData.hasChildren)
                                                          ? fileSystemModel.videoIconUrl(qmodelindex)
                                                          : ""
                            property bool starred: (!styleData.hasChildren)
                                                   ? fileSystemModel.isStarred(qmodelindex)
                                                   : false
                            property bool hasWorkingDir: (!styleData.hasChildren && root.extWorkingDirExists)
                                                   ? fileSystemModel.hasWorkingDir(qmodelindex, root.extWorkingDirPath)
                                                   : false
                            property bool shorts: (!styleData.hasChildren)
                                                  ? utilities.isYoutubeShortsUrl(videoUrl)
                                                  : false

                            onStarredChanged: {
                            }

                            onQmodelindexChanged: {
                                treeViewDelegate.updateProgress()
                            }

                            function updateProgress() {
                                if (!styleData.hasChildren && key) {
                                    progress = fileSystemModel.progress(key)
                                }
                            }

                            onVisibleChanged: {
                                treeViewDelegate.updateProgress()
                            }

                            ToolTip {
                                visible: ma.containsMouse && !styleData.hasChildren
                                text: treeViewDelegate.title
                                delay: 300
                                font {
                                    family: mainFont.name
                                }

                                Image {
                                    id: tooltipThumbnail
                                    visible: parent.visible
                                    source : (visible && treeViewDelegate.key !== "") ? "image://videothumbnail/" + treeViewDelegate.key : ""
                                    anchors.left: parent.right
                                    anchors.leftMargin: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    fillMode: Image.PreserveAspectFit
                                    height: 128
                                }
                            }

                            Drag.active: ma.drag.active
                            Drag.dragType: Drag.Automatic
                            Drag.imageSource : "qrc:/images/save.png"
                            Image {
                                visible: !styleData.hasChildren
                                anchors {
                                    left: parent.left
                                    leftMargin: 2
                                    top: parent.top
                                    topMargin: 2
                                    bottom: parent.bottom
                                    bottomMargin: 2
                                }
                                source: treeViewDelegate.videoIconUrl

                                fillMode: Image.PreserveAspectFit

                                Image {
                                    visible: treeViewDelegate.starred
                                    anchors.fill: parent
                                    source: "qrc:/images/starred.png"
                                }
                                Image {
                                    visible: treeViewDelegate.hasWorkingDir
                                    anchors.fill: parent
                                    source: "qrc:/images/workingdirpresent.png"
                                }
                            }
                            Image {
                                visible: styleData.hasChildren
                                anchors {
                                    left: parent.left
                                    leftMargin: 2
                                    top: parent.top
                                    topMargin: 4
                                    bottom: parent.bottom
                                    bottomMargin: 4
                                }
                                source: "qrc:/images/folder-128.png"
                                fillMode: Image.PreserveAspectFit
                            }

                            Rectangle {
                                id: progressBar
                                visible: !styleData.hasChildren
                                height: (treeViewDelegate.playing) ? 3 : 1
                                property int totalWidth : parent.width - itemText.x
                                property real progress: (styleData.hasChildren)
                                                        ? 0
                                                        : treeViewDelegate.progress

                                anchors {
                                    left: itemText.left
                                    bottom: parent.bottom
                                    bottomMargin: 1
                                    right: parent.right
                                    rightMargin: (1. - progress) * totalWidth
                                }
                                color: (treeViewDelegate.playing)
                                       ? "green"
                                       : "red"
                            }

                            Text {
                                id: itemText
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: styleData.textAlignment

                                anchors {
                                    fill: parent
                                    leftMargin: horizontalAlignment === Text.AlignLeft ? 24 : 1
                                    rightMargin: horizontalAlignment === Text.AlignRight ? 8 : 1
                                }
                                text: styleData.hasChildren ? styleData.value : title
                                elide: Text.ElideRight
                                color: (!styleData.hasChildren && !treeViewDelegate.shorts &&
                                        (treeViewDelegate.duration === 0.0))
                                       ? properties.disabledTextColor
                                       : textColor
                                renderType: Text.QtRendering
                                font {
                                    pixelSize: properties.fsP1
                                    family: mainFont.name
                                }
                            }
                            DropArea {
                                id: da
                                anchors.fill: parent
                                enabled: styleData.hasChildren && !ma.drag.active
                                visible: enabled
                                property bool hovered: false
                                onEntered: {
                                    hovered = true
                                }
                                onExited: {
                                    hovered = false
                                }
                                onDropped:
                                {
                                    hovered = false
                                    if (typeof(drag.source.key) !== "undefined") { // moving category
                                        fileSystemModel.moveEntry(drag.source.qmodelindex, treeViewDelegate.qmodelindex)
                                    } else {
                                        fileSystemModel.moveVideo(drag.source.key, treeViewDelegate.qmodelindex)
                                    }
                                }
                            }
                            MouseArea {
                                id: ma
                                anchors.fill: parent
                                enabled: true // !styleData.hasChildren allow categories to be moved
                                visible: enabled
                                drag.target: dummy
                                drag.smoothed: false // Disable smoothed so that the Item pixel from where we started the drag remains under the mouse cursor
                                acceptedButtons: Qt.RightButton | Qt.LeftButton
                                hoverEnabled: true

                                function contextualAction() {
                                    if (styleData.hasChildren) {
                                        treeViewContainer.contextMenu.setCategoryIndex(treeViewDelegate.qmodelindex)
                                        treeViewContainer.contextMenu.popup()
                                    } else {
                                        treeViewContainer.contextMenu.setVideoIndex(treeViewDelegate.qmodelindex)
                                        treeViewContainer.contextMenu.popup()
                                    }
                                }
                                cursorShape: (styleData.hasChildren)
                                             ? Qt.ArrowCursor
                                             : Qt.PointingHandCursor
                                onClicked: {
                                    if (mouse.button === Qt.RightButton) {
                                        contextualAction()
                                    } else if (mouse.button === Qt.LeftButton) {
                                        if (styleData.hasChildren) {
                                            return
                                        }
                                        var url = treeViewDelegate.videoUrl
                                        webEngineView.url = url;
                                    }
                                }
                                pressAndHoldInterval: 1000
                                onPressAndHold: {
                                    contextualAction()
                                }

                                onPressed: {

                                }

                                onReleased: {

                                }
                                onDoubleClicked: {
                                    if (styleData.hasChildren) {
                                        if (view.isExpanded(treeViewDelegate.qmodelindex))
                                            view.collapse(treeViewDelegate.qmodelindex)
                                        else
                                            view.expand(treeViewDelegate.qmodelindex)
                                    }
                                }
                            }
                        }

                        headerDelegate: Rectangle {
                            height: Math.round(textItem.implicitHeight * 1.2)
                            color: "black"
                            border.color: properties.viewBorderColor
                            Text {
                                id: textItem
                                anchors.fill: parent
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: styleData.textAlignment
                                anchors.leftMargin: horizontalAlignment === Text.AlignLeft ? 12 : 1
                                anchors.rightMargin: horizontalAlignment === Text.AlignRight ? 8 : 1
                                text: styleData.hasChildren ? styleData.value : title
                                elide: Text.ElideRight
                                color: textColor
                                renderType: Text.QtRendering
                            }
                        }
                        handle: Rectangle {
                            implicitWidth: 20
                            implicitHeight: 30
                            color: "transparent"
                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                implicitWidth: 14
                                anchors.bottom: parent.bottom
                                anchors.top: parent.top
                                color: properties.selectionColor
                            }
                        }
                        scrollBarBackground: Rectangle {
                            implicitWidth: 20
                            implicitHeight: 30
                            color: properties.paneBackgroundColor
                        }
                        decrementControl: Image {
                            width: 20
                            source: "qrc:/images/arrow.png"
                            transform: Rotation {
                                origin.x: width / 2
                                origin.y: height / 2
                                angle: 180
                            }
                        }
                        incrementControl: Image {
                            width: 20
                            source: "qrc:/images/arrow.png"
                        }
                    }


                    onActivated : {
                        if (!styleData.hasChildren) {
                            var url = fileSystemModel.videoUrl(index)
                            webEngineView.url = url;
                        }
                    }
                }
            }
            WebEngineView {
                id: webEngineView
                url: root.url
                property string key

                zoomFactor: root.wevZoomFactor

                enabled: true
                visible: enabled
                SplitView.minimumWidth: 200
                SplitView.fillWidth: true
                anchors {
                    top: parent.top
                    bottom: parent.bottom
                }

                objectName: "webEngineView"

                webChannel: web_channel

                profile: (typeof(root.profilePath) !== "undefined" && root.profilePath !== "")
                         ? userProfile
                         : inkognitoProfile

                settings {
                    autoLoadImages: true
                    dnsPrefetchEnabled: true
                }


                onUrlChanged: {
                    root.addVideoEnabled = false
                    if (utilities.isYoutubeVideoUrl(url)) {
                        key = utilities.getVideoID(url)
                        if (utilities.isYoutubeShortsUrl(url)) {
                            fileSystemModel.viewEntry(key, true);
                            dataPuller.interval = 5000;
                        } else {
                            dataPuller.interval = 5000;
                        }
                        dataPuller.start()
                    } else {
                        dataPuller.stop()
                        key = ""
                    }
                }

                userScripts: [
                    WebEngineScript {
                        injectionPoint: WebEngineScript.Deferred
                        name: "QWebChannel"
                        worldId: WebEngineScript.MainWorld
                        sourceUrl: "qrc:/qtwebchannel/qwebchannel.js"
                    },
                    WebEngineScript {
                        injectionPoint: WebEngineScript.Deferred
                        worldId: WebEngineScript.MainWorld
                        sourceCode: root.customScript
                    }
                ]

                Timer {
                    id: dataPuller
                    interval: 10000;
                    running: false;
                    repeat: true
                    function pullTime() {
                        interval = 10000
                        // console.log(timePuller.keyBefore, webEngineView.key, timePuller.videoTitle, timePuller.videoPosition, timePuller.videoDuration)
                        timePuller.keyBefore = webEngineView.key

                        if (utilities.isYoutubeShortsUrl(webEngineView.url)) {
                            webEngineView.runJavaScript(internals.script_videoTitleShorts)
                        } else {
                            webEngineView.runJavaScript(internals.script_videoTime)
                        }
                    }

                    onTriggered: {
                        pullTime()
                    }
                }

                property Menu contextMenu: Menu {
                    MenuItem {
                        text: "Add"
                        enabled: typeof(root.lastHoveredLink) !== "undefined" && root.lastHoveredLink !== ""
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
                        icon.source: "/icons/add.svg"
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
                onContextMenuRequested: function(request) {
                    {
                        request.accepted = true;
                        contextMenu.popup();
                    }
                }

                onLinkHovered:  {
                    if (hoveredUrl.toString().length > 0
                            && utilities.isYoutubeVideoUrl(hoveredUrl)) {
                        root.lastHoveredLink = hoveredUrl
                    }
                }

                onTooltipRequested: {
                    if (request.type === TooltipRequest.Show) {
                        root.lastHoveredTooltip = request.text
                    }
                }
            }
        }
    } // mainContainer

    header: ToolBar {
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

                    text: webEngineView.url
                    selectByMouse: true
                    onEditingFinished: {}
                    onAccepted: {
                        if (text == webEngineView.url)
                            return
                        webEngineView.url = text
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
                    property bool currentVideoAdded: isCurrentVideoAdded(webEngineView.key,
                                                                         root.addedVideoTrigger)
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
                    enabled: root.addVideoEnabled
                    visible: true
                    onClicked: {
                        copyLinkClipboardProxy.text = webEngineView.url
                        copyLinkClipboardProxy.selectAll();
                        copyLinkClipboardProxy.copy()
                        copyLinkClipboardProxy.text = ""
                    }
                    icon.source: "/icons/content_copy.svg"
                    display: AbstractButton.IconOnly

                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.text: "Copy Video URL to Clipboard"
                    ToolTip.delay: 300
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
                        layer.effect: ShaderEffect {
                            fragmentShader: "
                                uniform lowp sampler2D source; // this item
                                uniform lowp float qt_Opacity; // inherited opacity of this item
                                varying highp vec2 qt_TexCoord0;
                                void main() {
                                    lowp vec4 p = texture2D(source, qt_TexCoord0);
                                    if (p.a < .1)
                                        gl_FragColor = vec4(0, 0, 0, 0);
                                    else
                                        gl_FragColor = vec4(1, 0.9, 0, p.a);
                                }"
                        }
                    }
                }
            }
        }
    } // header

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
                onClicked: settingsMenu.close()

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


                // https://stackoverflow.com/questions/23791343/qml-repeater-for-multiple-items-without-a-wrapping-item
                // external app
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
                    focus: true
                    selectByMouse: true
                    font.pixelSize: properties.fsP1
                    cursorVisible: true
                    color: properties.textColor
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignLeft
                    text: root.extCommandName
                    onTextChanged: root.extCommandName = text

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
                    Layout.columnSpan: 3
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignLeft

                    text: root.extCommand
                    onTextChanged: {
                        if (utilities.executableExists(text)) {
                            root.extCommand = text;
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
                    Layout.leftMargin: -16
                    Layout.rightMargin: 0
                    onClicked: {
                        root.extCommandName = ""
                        root.extCommand = ""
                    }
                    hoverEnabled: true

                    ToolTip.visible: hovered
                    ToolTip.delay: 300
                    ToolTip.text: "Clear external command"
                }
                // external app end

                Button {
                    id: buttonOpenJSDialog
                    flat: true
                    display: Button.TextOnly
                    text: "Custom\nScript"
                    Layout.alignment: Qt.AlignVCenter
                    Layout.leftMargin: -12
                    onClicked: customScriptDialog.open()
                    hoverEnabled: true

                    ToolTip.visible: hovered
                    ToolTip.delay: 300
                    ToolTip.text: "Edit the custom script that is run after loading a video page"
                }
                Row {
                    Layout.columnSpan: 2
                    Layout.alignment: Qt.AlignVCenter
                    CheckBox {
                        id: darkModeCheck
                        checked: root.darkMode
                        text: qsTr("Dark mode (requires restart)")
                        onCheckedChanged: {
                            root.darkMode = checked
                        }
                    }
                }
                Row {
                    Layout.columnSpan: 1
                    Layout.alignment: Qt.AlignVCenter

                    CheckBox {
                        id: debugModeCHeck
                        checked: root.debugMode
                        text: qsTr("Developer mode")
                        onCheckedChanged: {
                            root.debugMode = checked
                        }
                    }
                }
                Button {
                    id: buttonResetSettings
                    flat: true
                    visible: root.debugMode
                    enabled: visible
                    display: Button.IconOnly
                    icon.source: "/icons/restart.svg"
                    text: "Custom\nScript"
                    Layout.columnSpan: 1
                    Layout.alignment: Qt.AlignVCenter
                    Layout.leftMargin: -12
                    onClicked: utilities.clearSettings()
                    hoverEnabled: true

                    ToolTip.visible: hovered
                    ToolTip.delay: 300
                    ToolTip.text: "Clear all settings (restarts YAYC)"
                }
                Item {

                }
            } // GridLayout

            RowLayout {
                Item {
                    Layout.fillWidth: true
                    height: buttonOpenGProfile.height * 6
                }

                Rectangle {
                    id: newReleaseContainer
                    visible: root.lastestRemoteVersion !== appVersion
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
                    height: buttonOpenGProfile.height * 6
                }
            }

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
                }
            } // RowLayout
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
            if (requestInterceputilitiestor.isYoutubeShortsUrl(u)) {
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

    QQD.FileDialog {
        id: fileDialogVideos
        nameFilters: []
        title: "Please choose a directory to store videos"
        selectExisting: true
        selectFolder: true

        onAccepted: {
            fileDialogVideos.close()
            var path = String(fileDialogVideos.fileUrl)
            root.youtubePath = root.deUrlizePath(path)
        }
        onRejected: {
        }
    }

    QQD.FileDialog {
        id: fileDialogHistory
        nameFilters: []
        title: "Please choose a directory to store history"
        selectExisting: true
        selectFolder: true

        onAccepted: {
            fileDialogHistory.close()
            var path = String(fileDialogHistory.fileUrl)
            root.historyPath = root.deUrlizePath(path)
        }
        onRejected: {
        }
    }

    QQD.FileDialog {
        id: fileDialogEasylist
        nameFilters: []
        title: "Please choose easylist.txt"
        selectExisting: true
        selectFolder: false

        onAccepted: {
            fileDialogEasylist.close()
            var path = String(fileDialogEasylist.fileUrl)
            root.easyListPath = root.deUrlizePath(path)
            requestInterceptor.setEasyListPath(root.easyListPath)
        }
        onRejected: {
        }
    }

    QQD.FileDialog {
        id: fileDialogExtWorkingDir
        nameFilters: []
        title: "Please choose a working directory to run the external application"
        selectExisting: true
        selectFolder: true
        onAccepted: {
            fileDialogExtWorkingDir.close()
            var path = String(fileDialogExtWorkingDir.fileUrl)
            root.extWorkingDirPath = root.deUrlizePath(path)
        }
        onRejected: {
        }
    }

    QQD.FileDialog {
        id: fileDialogProfile
        nameFilters: []
        title: "Please choose a directory for your Google profile"
        selectExisting: true
        selectFolder: true
        onAccepted: {
            fileDialogProfile.close()
            var path = String(fileDialogProfile.fileUrl)
            root.profilePath = root.deUrlizePath(path)
        }
        onRejected: {
        }
    }
}
