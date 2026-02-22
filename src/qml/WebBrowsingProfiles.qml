pragma Singleton
import QtQuick
import QtWebEngine
import yayc 1.0

QtObject {
    id: root
    property string httpUserAgent: "'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36'"
    property string httpAcceptLanguage: "en-US"
    property string profilePath // if empty, the webengineview profile will turn itself "off the record"
    property string customScript: ""
    property bool customScriptEnabled
    property bool initialized: false
    Component.onCompleted: { initialized = true }

    function createWebChannelScripts(customScript) { // TODO: remind the user that changing userScript requires app restart
        let webChannelScript = WebEngine.script()
        webChannelScript.name = "QWebChannel"
        webChannelScript.injectionPoint = WebEngineScript.Deferred
        webChannelScript.worldId = WebEngineScript.MainWorld
        webChannelScript.sourceUrl = Qt.resolvedUrl("qrc:/qtwebchannel/qwebchannel.js")

        let userScript = WebEngine.script()
        userScript.injectionPoint = WebEngineScript.Deferred
        userScript.worldId = WebEngineScript.MainWorld
        userScript.sourceCode = (root.customScriptEnabled) ? root.customScript : ""

        let cssScript = WebEngine.script()
        cssScript.name = "HideYouTubeCategoryBar"
        cssScript.injectionPoint = WebEngineScript.DocumentReady
        cssScript.worldId = WebEngineScript.MainWorld
        cssScript.sourceCode = `
            (function() {
                const style = document.createElement('style');
                style.id = 'yayc-category-bar-style';
                document.head.appendChild(style);
            })();
        `

        return [ webChannelScript, userScript, cssScript ]
    }

    function recreateProfiles() {
        let inkognitoProfile_ = inkognitoPrototype.createObject(root)
        let userProfile_ = userProfilePrototype.createObject(root)
        root.inkognitoProfile = inkognitoProfile_
        root.userProfile = userProfile_

        root.profile = Qt.binding(function() {
            return ((typeof(root.profilePath) !== "undefined" && root.profilePath !== "")
                 ? root.userProfile
                 : root.inkognitoProfile)
        })
    }

    property var profile: null

    property Component inkognitoPrototype: WebEngineProfile {
        httpAcceptLanguage: root.httpAcceptLanguage
        httpUserAgent: root.httpUserAgent
        httpCacheType: WebEngineProfile.MemoryHttpCache
        persistentCookiesPolicy: WebEngineProfile.NoPersistentCookies
        cachePath: ""
        persistentStoragePath: ""
        offTheRecord: true
        userScripts.collection: createWebChannelScripts(root.customScript)
    }

    property WebEngineProfile userProfile: null
    property WebEngineProfile inkognitoProfile:  null

    property Component userProfilePrototype: WebEngineProfile {
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
}
