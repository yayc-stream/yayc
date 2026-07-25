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
import Qt.labs.settings
import Qt.labs.platform as QLP
import yayc 1.0

Item {
    id: root
    property alias url: webEngineView.url
    property alias profile: webEngineView.profile
    property alias key: webEngineView.key
    property alias volume: sliderVolume.value
    property alias userSpecifiedVolume: sliderVolume.userValue
    property bool muted: false
    required property real wevZoomFactor
    required property real wevZoomFactorVideo
    property int homeGridColumns: 4
    property bool autoSkipAd: false
    // Global playback rate, applied to every player kind (watch/Shorts/preview).
    // Pushed down from MainYayc, pulled back up when changed YouTube-side.
    property real playbackRate: 1.0
    onPlaybackRateChanged: applyPlayerSettings()
    property bool showCategoryBar: true
    required property string customScript
    required property string profilePath // if empty, the webengineview profile will turn itself "off the record"
    required property string youtubePath
    required property string historyPath
    property bool windowHidden: win.hidden
    property alias addVideoEnabled: buttonAddVideo.enabled
    property alias guideToggled: buttonToggleGuide.checked

    property string lastHoveredLink
    property string lastHoveredTooltip

    // --- Hover-preview pinning ---------------------------------------------
    // While pinned, the page's pointer position is frozen (see
    // YaycUtilities::freezeWebViewHover) so the preview keeps playing while the
    // cursor moves to the toolbar or another window. ctxMenuOpen freezes too, and
    // that is load-bearing: moving the cursor off the thumbnail into the context
    // menu is itself a leave, which would kill the preview before the user could
    // ever click "Pin".
    property bool previewPinned: false
    property bool ctxMenuOpen: false
    readonly property bool freezeHover: previewPinned || ctxMenuOpen
    property url pinnedLink: ""
    // Where the last context menu was requested, in view coordinates. Used to
    // nudge hover when swapping pins (see pinPreview).
    property point lastCtxPos: Qt.point(-1, -1)
    // Set while waiting for a swapped-in preview to start playing.
    property url pendingPinLink: ""

    onPreviewPinnedChanged: {
        runScript("window.__yayc_pinned = " + previewPinned)
        if (previewPinned) {
            // Pinning implies "I want to watch this one", so give it sound. Safe to
            // unmute here specifically: the pin followed a right-click, satisfying
            // Chromium's user-activation requirement for audible playback.
            runScript("if (window.__yayc_unmutePreviews) window.__yayc_unmutePreviews();")
        }
    }

    // YouTube needs a moment of hover before it starts a preview; this is how
    // long we wait after releasing the freeze before pinning the new one.
    Timer {
        id: repinTimer
        interval: 1200
        onTriggered: {
            if (root.pendingPinLink == "")
                return
            root.pinnedLink = root.pendingPinLink
            root.pendingPinLink = ""
            root.previewPinned = true
        }
    }

    // Chromium's hover state is stale after a freeze, so the cursor sitting on the
    // new thumbnail isn't enough to start its preview. A synthetic move at the same
    // position restarts YouTube's hover timer. Needs the trusted-injection patch;
    // without it the user's next physical mouse movement does the same job.
    function nudgeHover() {
        if (!timePuller.hasTrustedMouseInjection)
            return
        if (root.lastCtxPos.x < 0)
            return
        webEngineView.sendTrustedMouseMove(root.lastCtxPos)
    }

    function pinPreview(link) {
        if (root.previewPinned) {
            // Swapping pins. The new preview cannot be playing yet - hover was
            // frozen - so release the freeze, nudge hover so YouTube starts it,
            // and pin once it has had time to come up.
            root.previewPinned = false
            root.pinnedLink = ""
            root.pendingPinLink = link
            nudgeHover()
            repinTimer.restart()
            return
        }
        root.pinnedLink = link
        root.previewPinned = true
    }

    function unpinPreview() {
        repinTimer.stop()
        root.pendingPinLink = ""
        if (!root.previewPinned)
            return
        root.previewPinned = false
        root.pinnedLink = ""
    }

    Binding {
        target: utilities
        property: "freezeWebViewHover"
        value: root.freezeHover
    }

    Shortcut {
        sequence: "Esc"
        enabled: root.previewPinned
        onActivated: root.unpinPreview()
    }

    Connections {
        target: utilities
        function onWebViewPressedWhileFrozen() { root.unpinPreview() }
    }

    // for ctx menu
    required property string extWorkingDirPath
    required property string easyListPath
    required property bool extWorkingDirExists
    required property var externalCommands
    required property bool removeStorageOnDelete
    required property bool extCommandEnabled

    function reload() {
        webEngineView.reload()
    }

    Component.onCompleted: {
        if (sliderVolume.value !== sliderVolume.userValue)
            sliderVolume.value = sliderVolume.userValue
        triggerWorkingDir()
        triggerVideoAdded()
    }

    Connections {
        target: fileSystemModel
        function onVersionBumped(key) {
            if (key === webEngineView.key)
                triggerWorkingDir()
        }
    }

    function action(code) {
        return webEngineView.action(code)
    }

    property int addedVideoTrigger: 0
    function triggerVideoAdded() { addedVideoTrigger += 1 }
    property int workingDirTrigger: 0
    function triggerWorkingDir() { workingDirTrigger += 1 }

    property alias isShorts: webEngineView.isShorts
    property alias isYoutubeChannel: webEngineView.isYoutubeChannel
    property alias isYoutubeHome: webEngineView.isYoutubeHome
    property alias isYoutubeVideo: webEngineView.isYoutubeVideo
    property alias keyHasWorkingDir: webEngineView.keyHasWorkingDir
    property alias loading: webEngineView.loading
    property alias zoomFactor: webEngineView.zoomFactor

    function isCurrentVideoAdded(key, trigger) {
        if (!utilities.isYoutubeVideoUrl(root.url))
            return false;
        return fileSystemModel.isVideoBookmarked(key)
    }

    function isWorkingDirPresent(key, path, trigger) {
        if (root.extWorkingDirExists)
            return fileSystemModel.hasWorkingDir(key, path)
        return 0
    }

    property QtObject timePuller: QtObject {
        // ID, under which this object will be known at WebEngineView side
        WebChannel.id: "backend"

        property real videoPosition: 0
        property real videoDuration: 0
        property string videoID
        property string shortsSignal: ""
        onShortsSignalChanged: {
            if (shortsSignal !== "" && !shortsSignal.startsWith("ERR:"))
                update()
        }
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
        property string videoQuality: ""
        property var availableQualityLevels: []
        property int previewPinLost: 0
        onPreviewPinLostChanged: root.unpinPreview()

        property real skipAdX: -1
        property real skipAdY: -1
        property int skipAdSeq: 0

        // True only if the loaded QtWebEngine was patched to expose
        // QQuickWebEngineView::sendTrustedMouse{Press,Release,Move}() (see
        // patches/qtwebengine/0001-trusted-mouse-event-injection-qml-api.patch).
        // Resolved dynamically against whatever WebEngineQuick plugin is
        // actually loaded at runtime, so this works unmodified whether YAYC
        // is run against a stock or patched QtWebEngine.
        readonly property bool hasTrustedMouseInjection:
            typeof webEngineView.sendTrustedMousePress === "function"

        onSkipAdSeqChanged: {
            var x = skipAdX * webEngineView.zoomFactor
            var y = skipAdY * webEngineView.zoomFactor
            if (x > 0 && y > 0
                    && x < webEngineView.width && y < webEngineView.height) {
                console.log('[yayc-adskip] simulating click at', x, y)
                if (hasTrustedMouseInjection) {
                    webEngineView.sendTrustedMousePress(Qt.point(x, y))
                    webEngineView.sendTrustedMouseRelease(Qt.point(x, y))
                } else {
                    utilities.simulateClick(webEngineView, x, y)
                }
                adSkipResumeTimer.restart()
            }
        }

        function clickGuideButton() {
            if (webEngineView.isYoutubeHome || webEngineView.isYoutubeChannel) {
                root.runScript(WebEngineInternals.script_clickGuide)
            }
        }

        function pullHomeData() {
            if (webEngineView.isYoutubeHome || webEngineView.isYoutubeChannel){
                root.runScript(WebEngineInternals.script_homePageStatusFetcher)
            }
        }

        onGuideButtonCheckedChanged: {
            if (guideButtonChecked === buttonToggleGuide.checked) {
                return;
            }
            clickGuideButton();
        }

        onVolumeChanged: if (root.volume !== volume) root.volume = volume
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

            if (webEngineView.key !== "") // webEngineView.key and k could be ""
                root.addVideoEnabled = true


            if (webEngineView.key !== ""
                  && videoTitle === "") {
                // missing title, silently ignore
                return;
            }

            // url didn't change, position changed
            update()
        }

        // Pull: the player reported its rate (e.g. changed via YouTube's own UI).
        // The !== guard keeps this from ping-ponging with the push in
        // root.onPlaybackRateChanged.
        onPlaybackRateChanged: {
            if (playbackRate > 0 && root.playbackRate !== playbackRate)
                root.playbackRate = playbackRate
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

    Timer {
        id: adSkipResumeTimer
        interval: 500
        onTriggered: root.runScript(WebEngineInternals.getPlayVideoScript(webEngineView.isShorts))
    }

    WebChannel {
        id : web_channel
        registeredObjects: [root.timePuller]
    }

    function seekBy(deltaSec) {
        if (webEngineView.isYoutubeVideo)
            runScript(WebEngineInternals.getSeekByScript(deltaSec, webEngineView.isShorts))
    }

    function runScript(s) {
        webEngineView.runJavaScript(s)
    }

    function applyCategoryBarVisibility() {
        let hide = !root.showCategoryBar
        root.runScript(`
            (function() {
                function apply() {
                    let fg = document.querySelector('#frosted-glass');
                    if (fg) {
                        if (${hide}) {
                            fg.classList.remove('with-chipbar');
                        } else {
                            fg.classList.add('with-chipbar');
                        }
                    }
                    let el = document.getElementById('yayc-category-bar-style');
                    if (el) el.textContent = ${hide} ? 'ytd-feed-filter-chip-bar-renderer { display: none !important; }' : '';
                }
                apply();
                setTimeout(apply, 500);
                setTimeout(apply, 2500);
            })();
        `)
    }

    // Pushes the global rate/volume into the page, where script_applyPlayerSettings
    // spreads them across whichever players currently exist. Safe to call with no
    // player (or no page) loaded: the values are stored and picked up on the next
    // player that appears.
    function applyPlayerSettings() {
        let r = (root.playbackRate > 0) ? root.playbackRate : 1
        let v = (sliderVolume.userValue >= 0) ? sliderVolume.userValue : sliderVolume.value
        runScript("window.__yayc_playerRate = " + r + ";"
                  + " window.__yayc_playerVolume = " + v + ";"
                  + " if (window.__yayc_applyPlayerSettings) window.__yayc_applyPlayerSettings();")
    }

    function applyHomeGridColumns() {
        let cols = root.homeGridColumns
        root.runScript(`
            (function() {
                function apply() {
                    let el = document.getElementById('yayc-grid-columns-style');
                    if (!el) {
                        el = document.createElement('style');
                        el.id = 'yayc-grid-columns-style';
                        document.head.appendChild(el);
                    }
                    el.textContent = 'ytd-rich-grid-renderer { --ytd-rich-grid-items-per-row: ${cols} !important; }';
                }
                apply();
                setTimeout(apply, 500);
                setTimeout(apply, 2500);
            })();
        `)
    }

    Timer {
        id: guideToggleSingleShot
        repeat: false
        running: false
        interval: 4500 // webEngineView is not emitting loadingChanged
                       // when clicking on the youtube logo to go back to the homepage
                       // However, onUrlChanged is also too soon, as the page is not laoded
        onTriggered: {
            root.timePuller.pullHomeData()
        }
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
        anchors.fill: parent
        zoomFactor: (isYoutubeVideo ? root.wevZoomFactorVideo : root.wevZoomFactor) || 1.0

        objectName: "webEngineView"

        webChannel: web_channel

        // profile: {
        //     if (!WebBrowsingProfiles.initialized)
        //         return null

        //     if (typeof(WebBrowsingProfiles.profilePath) !== "undefined"
        //           && WebBrowsingProfiles.profilePath !== ""
        //           && WebBrowsingProfiles.profilePath.userProfile !== undefined)
        //         return WebBrowsingProfiles.profilePath.userProfile

        //     if (WebBrowsingProfiles.profilePath.inkognitoProfile !== undefined)
        //         return WebBrowsingProfiles.profilePath.inkognitoProfile

        //     return null
        // }

        onJavaScriptConsoleMessage: function(level, message, lineNumber, sourceID) {
            // console.log(message)
            return;
            // // Suppress preload warnings from YouTube
            // if (message.includes("preloaded using link preload"))
            //     return;
            // console.log(sourceID + ":" + lineNumber, message);
        }

        settings {
            autoLoadImages: true
            dnsPrefetchEnabled: true

            fullScreenSupportEnabled: true
            javascriptCanAccessClipboard: true
            javascriptCanPaste: true
            screenCaptureEnabled: true
            playbackRequiresUserGesture: false
        }

        onIsYoutubeHomeChanged: if (webEngineView.isYoutubeHome) guideToggleSingleShot.start()
        onIsYoutubeChannelChanged: if (webEngineView.isYoutubeChannel) guideToggleSingleShot.start()

        onUrlChanged: {
            if (utilities.isYoutubeVideoUrl(url)) {
              root.addVideoEnabled = true
              key = utilities.getVideoID(url)
              isShorts = utilities.isYoutubeShortsUrl(url)
              dataPuller.startPulling()
              return;
            }

            root.addVideoEnabled = false
            isShorts = false
            dataPuller.stop()
            key = ""
        }

        // This doesn't work as expected.
        // It should be combined at least with detecting yt specific events
        // such as yt-navigate-finish or yt-page-data-updated
        onLoadingChanged: (loadingInfo) => {
            // TODO: add check that this is youtube home
            if (loadingInfo.status === WebEngineView.LoadSucceededStatus) {
                root.applyCategoryBarVisibility()
                root.applyHomeGridColumns()
                if (root.autoSkipAd)
                    runScript("window.__yayc_autoSkipAdEnabled = true")
                root.unpinPreview()
                root.applyPlayerSettings()
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

                if (root.windowHidden)
                    return; // save CPU

                if (!webEngineView.isYoutubeVideo
                        || webEngineView.key === "")
                    return;

                if (webEngineView.isShorts) {
                    root.runScript(WebEngineInternals.script_videoTimeShorts)
                } else {
                    root.runScript(WebEngineInternals.script_videoTime)
                }
            }

            onTriggered: {
                pullTime()
            }
        }

        component CtxClipboardActions: Repeater {
            model: [
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
                             case WebEngineView.Copy:"/icons/content_copy.svg";break;
                             case WebEngineView.Paste:"/icons/content_paste.svg";break;
                             case WebEngineView.Cut:"/icons/content_cut.svg";break;
                             case WebEngineView.CopyLinkToClipboard:"/icons/add_link.svg";break;
                             }
                display: MenuItem.TextBesideIcon
            }
        }

        component CtxExtAppsMenu: Menu {
            id: extAppsMenu
            required property string ctxKey
            required property url ctxLink
            required property bool ctxIsVideo
            title: "Launch in"
            icon.source: "/icons/function.svg"
            enabled: root.extCommandEnabled && ctxIsVideo
            height: enabled ? implicitHeight : 0
            Repeater {
                model: extAppsMenu.enabled ? root.externalCommands : undefined
                MenuItem {
                    text: (root.externalCommands[index])
                            ? root.externalCommands[index].name : ""
                    enabled: true; visible: true
                    height: enabled ? implicitHeight : 0
                    onClicked: fileSystemModel.enqueueExternalApp(
                                extAppsMenu.ctxKey,
                                utilities.normalizeVideoUrl(extAppsMenu.ctxLink),
                                root.externalCommands[index].command,
                                root.extWorkingDirPath)
                    icon.source: "/icons/extension.svg"
                    display: MenuItem.TextBesideIcon
                }
            }
        }

        property Menu _ctxMenuAdded: Menu {
            property url requestedLink: ""
            property string requestedLinkText: ""
            property string requestedKey: utilities.getVideoID(requestedLink)
            property bool linkIsVideo: requestedKey !== ""

            // Unconditional: when a pin was taken, previewPinned keeps freezeHover
            // true on its own. Gating this on previewPinned would latch
            // ctxMenuOpen forever and hover would never thaw after unpinning.
            onClosed: root.ctxMenuOpen = false

            MenuItem {
                // Right-clicking the already-pinned video offers Unpin; any other
                // video offers Pin, which swaps the pin over to it.
                readonly property bool ctxIsPinned: root.previewPinned
                        && root.pinnedLink == webEngineView._ctxMenuAdded.requestedLink
                readonly property bool ctxCanPin: webEngineView._ctxMenuAdded.linkIsVideo && !ctxIsPinned
                text: ctxCanPin ? uiTr("Pin preview") : uiTr("Unpin preview")
                icon.source: "/icons/picture_in_picture.svg"
                visible: ctxCanPin || root.previewPinned
                height: visible ? implicitHeight : 0
                onTriggered: {
                    if (ctxCanPin)
                        root.pinPreview(webEngineView._ctxMenuAdded.requestedLink)
                    else
                        root.unpinPreview()
                }
            }
            MenuItem {
                text: uiTr("Added") + " (" + fileSystemModel.categoryName(webEngineView._ctxMenuAdded.requestedKey) + ")"
                icon.source: "/icons/add.svg"
                enabled: false
            }
            CtxClipboardActions {}
            CtxExtAppsMenu {
                ctxKey: webEngineView._ctxMenuAdded.requestedKey
                ctxLink: webEngineView._ctxMenuAdded.requestedLink
                ctxIsVideo: webEngineView._ctxMenuAdded.linkIsVideo
            }
        }

        property Menu _ctxMenuNotAdded: Menu {
            property url requestedLink: ""
            property string requestedLinkText: ""
            property string requestedKey: utilities.getVideoID(requestedLink)
            property bool linkIsVideo: requestedKey !== ""

            // Unconditional: when a pin was taken, previewPinned keeps freezeHover
            // true on its own. Gating this on previewPinned would latch
            // ctxMenuOpen forever and hover would never thaw after unpinning.
            onClosed: root.ctxMenuOpen = false

            MenuItem {
                // Right-clicking the already-pinned video offers Unpin; any other
                // video offers Pin, which swaps the pin over to it.
                readonly property bool ctxIsPinned: root.previewPinned
                        && root.pinnedLink == webEngineView._ctxMenuNotAdded.requestedLink
                readonly property bool ctxCanPin: webEngineView._ctxMenuNotAdded.linkIsVideo && !ctxIsPinned
                text: ctxCanPin ? uiTr("Pin preview") : uiTr("Unpin preview")
                icon.source: "/icons/picture_in_picture.svg"
                visible: ctxCanPin || root.previewPinned
                height: visible ? implicitHeight : 0
                onTriggered: {
                    if (ctxCanPin)
                        root.pinPreview(webEngineView._ctxMenuNotAdded.requestedLink)
                    else
                        root.unpinPreview()
                }
            }
            Menu {
                title: uiTr("Add to...")
                icon.source: "/icons/add.svg"
                visible: webEngineView._ctxMenuNotAdded.linkIsVideo
                height: visible ? implicitHeight : 0

                MenuItem {
                    text: "/"
                    onClicked: {
                        var key = webEngineView._ctxMenuNotAdded.requestedKey
                        if (key !== "") {
                            fileSystemModel.addEntry(key, webEngineView._ctxMenuNotAdded.requestedLinkText,
                                                     "", "", "")
                            root.triggerVideoAdded()
                        }
                    }
                }
                Repeater {
                    model: fileSystemModel.recentDestinations
                    MenuItem {
                        required property var modelData
                        text: modelData.name
                        onClicked: {
                            var key = webEngineView._ctxMenuNotAdded.requestedKey
                            if (key !== "") {
                                fileSystemModel.addEntry(key, webEngineView._ctxMenuNotAdded.requestedLinkText,
                                                         "", "", "", 0, 0, modelData.path)
                                root.triggerVideoAdded()
                            }
                        }
                    }
                }
            }
            CtxClipboardActions {}
            CtxExtAppsMenu {
                ctxKey: webEngineView._ctxMenuNotAdded.requestedKey
                ctxLink: webEngineView._ctxMenuNotAdded.requestedLink
                ctxIsVideo: webEngineView._ctxMenuNotAdded.linkIsVideo
            }
        }

        function isValidHttpUrl(url) {
            let regEx = /^https?:\/\/(?:www\.)?[-a-zA-Z0-9@:%._\+~#=]{1,256}\.[a-zA-Z0-9()]{1,6}\b([-a-zA-Z0-9()@:%_\+.~#?&//=]*)$/gm;
            return regEx.test(url);
        }
        onContextMenuRequested: (request) => {
            var link = request.linkUrl
            var linkText = request.linkText
            var key = utilities.getVideoID(link)
            var isAdded = key !== "" && fileSystemModel.isVideoBookmarked(key)
            var menu = isAdded ? webEngineView._ctxMenuAdded : webEngineView._ctxMenuNotAdded
            menu.requestedLink = link
            menu.requestedLinkText = linkText
            request.accepted = true
            // Freeze before the menu appears, so crossing into it doesn't end the
            // preview. Released in the menus' onClosed unless a pin was taken.
            root.ctxMenuOpen = true
            root.lastCtxPos = request.position
            menu.popup()
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
    } // WebEngineView

    // While a preview is pinned, Chromium stops receiving mouse movement, so YouTube's
    // player auto-hide (ytp-autohide, cursor: none) fires and never gets cancelled -
    // Chromium then pushes that blank cursor onto the render item and the pointer
    // disappears over the whole view. This overlay sits above the web view purely to
    // supply a cursor that Qt resolves in preference to the item's own.
    //
    // acceptedButtons: NoButton so clicks fall through untouched (the C++ filter still
    // sees the press and unpins), and no wheel handling, so scrolling keeps working.
    // Right-click also reaches the page, which the pin-swap flow depends on.
    MouseArea {
        anchors.fill: webEngineView
        // Only while actually pinned - not for the brief ctxMenuOpen freeze, where
        // the cursor is over the menu anyway and an overlay could only interfere.
        visible: root.previewPinned
        enabled: visible
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        cursorShape: Qt.ArrowCursor
        z: webEngineView.z + 1
    }

    Rectangle {
        id: zoomOverlay
        anchors.centerIn: parent
        width: zoomLabel.width + 48
        height: zoomLabel.height + 32
        radius: 12
        color: "#80000000"
        opacity: 0
        visible: opacity > 0

        property real lastZoom: 1.0
        property bool lastIsVideo: webEngineView.isYoutubeVideo
        property bool initialized: false

        Label {
            id: zoomLabel
            anchors.centerIn: parent
            text: Math.round(webEngineView.zoomFactor * 100) + "%"
            font.pixelSize: 48
            color: "white"
        }

        Timer {
            id: zoomOverlayTimer
            interval: 1500
            onTriggered: zoomFadeOut.start()
        }

        NumberAnimation {
            id: zoomFadeOut
            target: zoomOverlay
            property: "opacity"
            to: 0
            duration: 300
        }

        Connections {
            target: webEngineView
            function onZoomFactorChanged() {
                var newZoom = webEngineView.zoomFactor
                if (Math.abs(newZoom - zoomOverlay.lastZoom) < 0.01)
                    return
                zoomOverlay.lastZoom = newZoom
                if (!zoomOverlay.initialized) {
                    zoomOverlay.initialized = true
                    return
                }
                if (webEngineView.isYoutubeVideo !== zoomOverlay.lastIsVideo) {
                    zoomOverlay.lastIsVideo = webEngineView.isYoutubeVideo
                    return
                }
                zoomFadeOut.stop()
                zoomOverlay.opacity = 1
                zoomOverlayTimer.restart()
            }
        }
    }

    property RowLayout webViewTools: RowLayout {
        anchors.fill: parent
        property Menu contextMenu: BookmarkContextMenu {
            isHistoryView: false
            model: fileSystemModel
            parentView: null
            x: buttonAddVideo.x
            y: buttonAddVideo.y + buttonAddVideo.height

            // required properties
            extWorkingDirExists: root.extWorkingDirExists
            extWorkingDirPath: root.extWorkingDirPath
            externalCommands: root.externalCommands
            removeStorageOnDelete: root.removeStorageOnDelete
            extCommandEnabled: root.extCommandEnabled
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
            text: uiTr("Add")
            enabled: false
            visible: true
            onClicked: {
                if (!currentVideoAdded)
                    addToToolbarMenu.popup()
            }
            onPressAndHold: {
                if (currentVideoAdded) {
                    root.webViewTools.contextMenu.setKey(webEngineView.key)
                    root.webViewTools.contextMenu.open()
                }
            }

            property bool currentVideoAdded: webEngineView.isYoutubeVideo
                    && isCurrentVideoAdded(webEngineView.key,
                                           root.addedVideoTrigger)
            property int workingDirPresent : (webEngineView.isYoutubeVideo)
                    ? webEngineView.keyHasWorkingDir : 0

            function addToDestination(destinationPath) {
                if (!utilities.isYoutubeVideoUrl(root.url))
                    return
                var k = utilities.getVideoID(root.timePuller.videoID,
                                             root.timePuller.vendor,
                                             root.timePuller.shorts)
                if (webEngineView.key !== k)
                    return
                fileSystemModel.addEntry(webEngineView.key,
                                         root.timePuller.videoTitle,
                                         root.timePuller.channelURL,
                                         root.timePuller.channelAvatar,
                                         root.timePuller.channelName,
                                         root.timePuller.videoDuration,
                                         root.timePuller.videoPosition,
                                         destinationPath)
                if (k !== "" && root.timePuller.shorts)
                    fileSystemModel.viewEntry(webEngineView.key, true)
                root.triggerVideoAdded()
            }

            icon {
                source: "/icons/add.svg"
                color: (currentVideoAdded)
                       ? (enabled)
                         ? YaycProperties.addedTextColor
                         : YaycProperties.addedDisabledTextColor
                       : (enabled)
                         ? "white"
                         : YaycProperties.disabledTextColor
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
                          ? uiTr("Bookmarked in") + ": " + fileSystemModel.categoryName(webEngineView.key)
                          : uiTr("Add Video to Bookmarks")
            ToolTip.delay: 300

            Menu {
                id: addToToolbarMenu
                MenuItem {
                    text: "/"
                    onClicked: root.timePuller.addCurrentVideo()
                }
                Repeater {
                    model: fileSystemModel.recentDestinations
                    MenuItem {
                        required property var modelData
                        text: modelData.name
                        onClicked: buttonAddVideo.addToDestination(modelData.path)
                    }
                }
            }
        }
        ToolButton {
            id: buttonStarVideo
            text: uiTr("Star")
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
                          ? uiTr("Unstar current video")
                          : uiTr("Star current video")
            ToolTip.delay: 300
        }
        ToolButton {
            id: buttonCopyLink
            text: uiTr("Copy")
            enabled: true
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
            ToolTip.text: uiTr("Copy URL to Clipboard")
            ToolTip.delay: 300
        }
        ToolButton {
            id: buttonToggleGuide
            text: uiTr("Toggle Guide panel") // left panel on the youtube home
            enabled: webEngineView.isYoutubeHome
            visible: true
            checkable: true
            checked: false

            onCheckedChanged: {
                root.timePuller.clickGuideButton()
            }

            icon.source: "/icons/menu.svg"
            display: AbstractButton.IconOnly

            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.text: uiTr("Toggle Guide panel")
            ToolTip.delay: 300
        }
        ToolButton {
            id: buttonToggleCategoryBar
            checkable: true
            checked: root.showCategoryBar
            onCheckedChanged: {
                root.showCategoryBar = checked
                root.applyCategoryBarVisibility()
            }
            icon.source: "/icons/categories.svg"
            display: AbstractButton.IconOnly
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.text: uiTr("Toggle YouTube category bar")
            ToolTip.delay: 300
        }
        ToolButton {
            id: buttonGridColumns
            checkable: true

            onCheckedChanged: {
                if (checked) {
                    ToolTip.toolTip.close()
                    gridColumnsMenu.open()
                } else {
                    gridColumnsMenu.close()
                }
            }

            icon.source: "/icons/grid.svg"

            display: (root.homeGridColumns !== 4)
                     ? AbstractButton.TextUnderIcon
                     : AbstractButton.IconOnly
            text: root.homeGridColumns
            spacing: -6

            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.text: uiTr("Set home page grid columns")
            ToolTip.delay: 300
        }
        ToolButton {
            id: buttonPip
            enabled: webEngineView.isYoutubeVideo
                     || webEngineView.isYoutubeHome
                     || webEngineView.isYoutubeChannel // TODO: just check if it's "youtube".
            visible: true
            checkable: false

            onClicked: {
                root.runScript(`
                    (function() {
                        var player = document.getElementById('movie_player');
                        if (player) {
                            player.dispatchEvent(new KeyboardEvent('keydown', {key: 'i', code: 'KeyI', keyCode: 73, which: 73, bubbles: true}));
                        }
                    })();
                `)
            }

            icon.source: "/icons/picture_in_picture.svg"
            display: AbstractButton.IconOnly

            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.text: uiTr("Picture-in-Picture")
            ToolTip.delay: 300
        }
        ToolButton {
            id: buttonSpeed
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
            text: (root.playbackRate > 0) ? root.playbackRate.toFixed(2) : "1.00"
            spacing: -6

            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.text: uiTr("Set playback rate")
            ToolTip.delay: 300
        }
        ToolButton {
            id: buttonUnpinPreview
            // Only exists while something is pinned: it is the escape hatch when
            // the pinned thumbnail has scrolled away or is otherwise unreachable.
            visible: root.previewPinned
            icon.source: "/icons/picture_in_picture.svg"
            display: AbstractButton.IconOnly

            onClicked: root.unpinPreview()

            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.text: uiTr("Unpin preview")
            ToolTip.delay: 300
        }
        ToolButton {
            id: buttonQuality
            enabled: webEngineView.isYoutubeVideo && root.timePuller.availableQualityLevels.length > 0
            visible: true
            checkable: true

            onCheckedChanged: {
                if (checked) {
                    ToolTip.toolTip.close()
                    qualityMenu.open()
                } else {
                    qualityMenu.close()
                }
            }

            icon.source: "/icons/sliders.svg"

            display: AbstractButton.TextUnderIcon
            text: WebEngineInternals.formatQualityLabel(root.timePuller.videoQuality) // TODO: move here?
            spacing: -6

            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.text: uiTr("Set video quality")
            ToolTip.delay: 300
        }
        ToolButton {
            id: buttonAutoSkipAd
            visible: true
            checkable: true
            checked: root.autoSkipAd

            onCheckedChanged: {
                root.autoSkipAd = checked
                runScript("window.__yayc_autoSkipAdEnabled = " + checked)
            }

            icon.source: "/icons/step_over.svg"
            display: AbstractButton.IconOnly

            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.text: uiTr("Auto skip ads")
            ToolTip.delay: 300
        }
        ToolButton {
            id: buttonPlayPause
            enabled: webEngineView.isYoutubeVideo && (root.timePuller.playerState !== -1)
            visible: true
            checkable: false

            onClicked: {
                var scriptToRun
                if (root.timePuller.playerState === 1 && webEngineView.key !== "")
                    scriptToRun = WebEngineInternals.getPauseVideoScript(webEngineView.isShorts)
    // Br0ken, try https://stackoverflow.com/a/58581660/962856, because .click() also doesn't work
    //                        else if (root.timePuller.playerState === -1)
    //                            scriptToRun = WebEngineInternals.getPlayNextVideoScript(utilities.isYoutubeShortsUrl(root.url))
                else
                    scriptToRun = WebEngineInternals.getPlayVideoScript(webEngineView.isShorts)

    //                        console.log(root.timePuller.playerState, scriptToRun)
                root.runScript(scriptToRun)
            }

            icon.source: (root.timePuller.playerState === 1)
                            ? "/icons/pause.svg"
                            : "/icons/play_arrow.svg"

            display: AbstractButton.IconOnly

            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.text: (root.timePuller.playerState === 1)
                            ? uiTr("Pause video")
                            : uiTr("Play video")
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
                implicitWidth: 130
                anchors.centerIn: parent

                value: 0
                from: 0
                stepSize: 1
                to: 100
                snapMode: Slider.SnapAlways
                property real userValue: -1

                function setVolume() {
                    // Applies to every player kind, not just the watch player.
                    root.applyPlayerSettings()
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
    }
    // Toolbar Menus
    Menu {
        id: gridColumnsMenu
        y: 0
        width: 48
        visible: false
        onAboutToShow: x = buttonGridColumns.mapToItem(root, buttonGridColumns.width - width, 0).x
        ColumnLayout {
            width: parent.width
            spacing: 4
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: gridColumnsSlider.value
                font.bold: true
            }
            Slider {
                id: gridColumnsSlider
                orientation: Qt.Vertical
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredHeight: 150
                from: 12
                to: 1
                stepSize: 1
                value: root.homeGridColumns
                snapMode: Slider.SnapAlways
                onValueChanged: {
                    if (value !== root.homeGridColumns) {
                        root.homeGridColumns = value
                        root.applyHomeGridColumns()
                    }
                }
            }
        }
        onClosed: buttonGridColumns.checked = false
    }
    Menu {
        id: playbackRateMenu
        y: 0
        width: 48
        visible: false
        onAboutToShow: x = buttonSpeed.mapToItem(root, buttonSpeed.width - width, 0).x
        ColumnLayout {
            width: parent.width
            Repeater {
                model: WebEngineInternals.videoSpeeds
                ToolButton {
                    height: playbackRateMenu.width
                    width: height
                    enabled: true
                    checkable: false
                    checked: root.playbackRate.toFixed(2) === text

                    z: playbackRateMenu.z + 5

                    text: WebEngineInternals.videoSpeeds[index]

                    display: AbstractButton.TextOnly

                    onClicked: {
                        buttonSpeed.checked = false
                        // Sets the global; the change handler pushes it to every
                        // player kind (watch/Shorts/preview).
                        root.playbackRate = parseFloat(text)
                    }

                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.text: uiTr("Set playback rate to") + " " + text
                    ToolTip.delay: 300
                }
            }
        }
    }
    Menu {
        id: qualityMenu
        y: 0
        width: 64
        visible: false
        onAboutToShow: x = buttonQuality.mapToItem(root, buttonQuality.width * .5 - width * .5, 0).x
        ColumnLayout {
            width: parent.width
            spacing: 0
            // Quality options
            Repeater {
                model: root.timePuller.availableQualityLevels.filter(q => q !== "auto")
                ToolButton {
                    Layout.alignment: Qt.AlignHCenter
                    width: qualityMenu.width
                    height: 32
                    enabled: true
                    checkable: false
                    checked: root.timePuller.videoQuality === modelData

                    z: qualityMenu.z + 1

                    text: WebEngineInternals.formatQualityLabel(modelData, true)

                    display: AbstractButton.TextOnly

                    onClicked: {
                        buttonQuality.checked = false
                        let scriptToRun = WebEngineInternals.getQualitySetterScript(
                                    modelData, utilities.isYoutubeShortsUrl(root.url)
                                 )
                        root.runScript(scriptToRun)
                    }

                    hoverEnabled: true
                }
            }
            // Separator
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                Layout.topMargin: 4
                Layout.bottomMargin: 4
                color: palette.mid
            }
            // Auto option at bottom
            ToolButton {
                Layout.alignment: Qt.AlignHCenter
                width: qualityMenu.width
                height: 32
                enabled: true
                checkable: false

                z: qualityMenu.z + 1

                text: "\u00A0Auto\u00A0"

                display: AbstractButton.TextOnly

                onClicked: {
                    buttonQuality.checked = false
                    let scriptToRun = WebEngineInternals.getQualitySetterScript(
                                "auto", utilities.isYoutubeShortsUrl(root.url)
                             )
                    root.runScript(scriptToRun)
                }

                hoverEnabled: true
                ToolTip.visible: hovered
                ToolTip.text: uiTr("Let YouTube choose the best quality based on connection speed")
                ToolTip.delay: 300
            }
        }
        onClosed: buttonQuality.checked = false
    }
} // root item
