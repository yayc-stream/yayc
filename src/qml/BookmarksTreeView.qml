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

Rectangle {
    implicitWidth: 200
    color: YaycProperties.fileBgColor
    enabled: true
    id: viewContainer

    property alias searchInStarred: selectorStarred.checked
    property alias searchInUnstarred: selectorUnstarred.checked
    property alias searchInOpened: selectorOpened.checked
    property alias searchInUnopened: selectorUnopened.checked
    property alias searchInWatched: selectorWatched.checked
    property alias searchInUnwatched: selectorUnwatched.checked
    property alias searchInSaved: selectorSaved.checked
    property alias searchInUnsaved: selectorUnsaved.checked
    property alias searchInShorts: selectorShorts.checked
    readonly property int _branchIndicatorSize: 16
    readonly property int _defaultLineHeight: 26
    readonly property int _rowHeight: Math.round(_defaultLineHeight * 1.1)

    property bool showFiltering: false
    property bool searchInTitles: true
    property bool searchInChannelNames: true
    property bool historyView
    property var model: (historyView === undefined) ? undefined
                            : ((historyView) ?
                                historyModel : fileSystemModel)
    signal videoSelected(url: string)

    // for ctx menu
    required property string extWorkingDirPath
    required property bool extWorkingDirExists
    required property var externalCommands
    required property bool removeStorageOnDelete
    required property bool extCommandEnabled
    required property string webEngineViewKey

    function clearModel() {
        viewContainer.model = null
    }

    function setModel() {
        viewContainer.model = (historyView === undefined) ? undefined
                                    : ((historyView) ?
                                        historyModel : fileSystemModel)
    }

    property Menu contextMenu: BookmarkContextMenu {
        isHistoryView: viewContainer.historyView
        model: viewContainer.model
        parentView: view
        parentContainer: viewContainer
        onClosed: {
            view.contextedKey = ""
        }
        // required properties
        extWorkingDirExists: viewContainer.extWorkingDirExists
        extWorkingDirPath: viewContainer.extWorkingDirPath
        externalCommands: viewContainer.externalCommands
        removeStorageOnDelete: viewContainer.removeStorageOnDelete
        extCommandEnabled: viewContainer.extCommandEnabled
    }

    onSearchInTitlesChanged: if (model) model.sortFilterProxyModel.searchInTitles = viewContainer.searchInTitles
    onSearchInChannelNamesChanged: if (model) model.sortFilterProxyModel.searchInChannelNames = viewContainer.searchInChannelNames

    onSearchInSavedChanged: {
        if (!model) return
        model.sortFilterProxyModel.workingDirRoot = viewContainer.extWorkingDirPath
        model.sortFilterProxyModel.searchInSaved = viewContainer.searchInSaved;
    }
    onSearchInUnsavedChanged: {
        if (!model) return
        model.sortFilterProxyModel.workingDirRoot = viewContainer.extWorkingDirPath
        model.sortFilterProxyModel.searchInUnsaved = viewContainer.searchInUnsaved;
    }
    onSearchInStarredChanged: if (model) model.sortFilterProxyModel.searchInStarred = viewContainer.searchInStarred
    onSearchInUnstarredChanged: if (model) model.sortFilterProxyModel.searchInUnstarred = viewContainer.searchInUnstarred
    onSearchInOpenedChanged: if (model) model.sortFilterProxyModel.searchInOpened = viewContainer.searchInOpened
    onSearchInUnopenedChanged: if (model) model.sortFilterProxyModel.searchInUnopened = viewContainer.searchInUnopened
    onSearchInWatchedChanged: if (model) model.sortFilterProxyModel.searchInWatched = viewContainer.searchInWatched
    onSearchInUnwatchedChanged: if (model) model.sortFilterProxyModel.searchInUnwatched = viewContainer.searchInUnwatched
    onSearchInShortsChanged: if (model) model.sortFilterProxyModel.searchInShorts = viewContainer.searchInShorts


    function search() {
        viewContainer.model.sortFilterProxyModel.searchTerm = filterTF.text
        forceLayoutTimer.restart()
    }

    function refreshLayout() {
        forceLayoutTimer.restart()
    }

    Timer {
        id: forceLayoutTimer
        interval: 300
        onTriggered: {
            view.forceLayout()
            // Clamp contentY to valid range after filtering reduces content
            let maxContentY = Math.max(0, view.contentHeight - view.height)
            if (view.contentY > maxContentY)
                view.contentY = maxContentY
        }
    }

    // For progress-pumping
    property int heartbeat: 0
    Timer {
        id: progressTimer
        interval: 1000
        repeat: true
        running: viewContainer.webEngineViewKey !== ""
        onTriggered: viewContainer.heartbeat++
    }

    Rectangle {
        id: filterContainer
        color: "transparent"
        enabled: viewContainer.showFiltering
        visible: enabled
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }
        height: 64

        Column {
            anchors.fill: parent
            Row {
                spacing: 0
                TextField {
                    id: filterTF
                    width: filterContainer.width
                           - searchColumn.width
                           - clearSearchButton.width
                           - parent.spacing * 2
                    height: 40

                    font {
                        pixelSize: 16
                    }

                    selectByMouse: true
                    onAccepted: {
                        viewContainer.search()
                    }
                }
                ToolButton {
                    id: clearSearchButton
                    readonly property int buttonSize: 40
                    height: buttonSize
                    width: height
                    enabled: filterTF.text !== ""
                    checkable: false

                    onClicked: {
                        filterTF.text = ""
                        viewContainer.search()
                    }

                    icon {
                        height: buttonSize
                        width: buttonSize
                        source: "/icons/backspace.svg"
                    }
                    display: AbstractButton.IconOnly

                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.text: "Clear search term"
                    ToolTip.delay: 300
                }

                Column {
                    id: searchColumn

                    ToolButton {
                        id: filterButton
                        readonly property int buttonSize: filterTF.height * 0.75
                        height: buttonSize
                        width: height
                        checkable: false

                        onClicked: {
                            viewContainer.search()
                        }

                        icon {
                            height: buttonSize * 2.2
                            width: buttonSize * 2.2
                            source: "/icons/search.svg"
                        }
                        display: AbstractButton.IconOnly

                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: "Search"
                        ToolTip.delay: 300
                    }
                    Row {
                        Image {
                            id: filterButtonVideoTitle
                            source:  "/icons/video_file.svg"
                            height: filterTF.height * 0.3
                            width: height
                            layer.enabled: true
                            layer.mipmap: true
                            layer.effect: ColorOverlay {
                                color: (viewContainer.searchInTitles) ? YaycProperties.checkedButtonColor : YaycProperties.iconColor
                                visible: true
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: (mouse) => {
                                    viewContainer.searchInTitles = !viewContainer.searchInTitles
                                }

                                property bool hovered: false
                                onEntered:  hovered = true
                                onExited: hovered = false
                                hoverEnabled: true
                                ToolTip.visible: hovered
                                ToolTip.text: "Click to " + ((viewContainer.searchInTitles) ? "disable" : "enable") + " search in video titles"
                                ToolTip.delay: 300
                            }
                        }
                        Image {
                            id: filterButtonChannelName
                            source:  "/icons/tv_channel_media_television.svg"
                            height: filterTF.height * 0.3
                            width: height
                            layer.enabled: true
                            layer.mipmap: true
                            layer.effect: ColorOverlay {
                                color: (viewContainer.searchInChannelNames) ? YaycProperties.checkedButtonColor : YaycProperties.iconColor
                                visible: true
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: (mouse) => {
                                    viewContainer.searchInChannelNames = !viewContainer.searchInChannelNames
                                }

                                property bool hovered: false
                                onEntered:  hovered = true
                                onExited: hovered = false
                                hoverEnabled: true
                                ToolTip.visible: hovered
                                ToolTip.text: "Click to " + ((viewContainer.searchInChannelNames) ? "disable" : "enable") + " search in channel names"
                                ToolTip.delay: 300
                            }
                        }
                    }
                }
            }

            ToolBar {

                height: 16
                width: filterContainer.width

                background: Rectangle {
                        height: 16
                        width: filterContainer.width
                        color: "transparent"
                }

                Row {
                    height: buttonSize * .5
                    topPadding: buttonSize * -.25
                    leftPadding: -4
                    spacing: -8
                    readonly property int buttonSize: 30

                    ToolButton {
                        id: selectorUnstarred
                        height: parent.buttonSize
                        width: height
                        enabled: true
                        checkable: true
                        checked: true

                        icon {
                            height: parent.buttonSize
                            width: parent.buttonSize
                            source: "/icons/star.svg"
                        }
                        display: AbstractButton.IconOnly

                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: ((checked) ? "Exclude" : "Include") + " unstarred"
                        ToolTip.delay: 300
                    }
                    ToolButton {
                        id: selectorStarred
                        height: parent.buttonSize
                        width: height
                        enabled: true
                        checkable: true
                        checked: true

                        icon {
                            height: parent.buttonSize
                            width: parent.buttonSize
                            source: "/icons/star_fill.svg"
                        }
                        display: AbstractButton.IconOnly

                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: ((checked) ? "Exclude" : "Include") + " starred"
                        ToolTip.delay: 300
                    }
                    ToolButton {
                        id: selectorUnopened
                        height: parent.buttonSize
                        width: height
                        enabled: true
                        checkable: true
                        checked: true

                        icon {
                            height: parent.buttonSize
                            width: parent.buttonSize
                            source: "/icons/door_closed.svg"
                        }
                        display: AbstractButton.IconOnly

                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: ((checked) ? "Exclude" : "Include") + " unopened"
                        ToolTip.delay: 300
                    }
                    ToolButton {
                        id: selectorOpened
                        height: parent.buttonSize
                        width: height
                        enabled: true
                        checkable: true
                        checked: true

                        icon {
                            height: parent.buttonSize
                            width: parent.buttonSize
                            source: "/icons/door_open.svg"
                        }
                        display: AbstractButton.IconOnly

                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: ((checked) ? "Exclude" : "Include") + " opened"
                        ToolTip.delay: 300
                    }
                    ToolButton {
                        id: selectorUnwatched
                        height: parent.buttonSize
                        width: height
                        enabled: true
                        checkable: true
                        checked: true

                        icon {
                            height: parent.buttonSize
                            width: parent.buttonSize
                            source: "/images/video.png"
                        }
                        display: AbstractButton.IconOnly

                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: ((checked) ? "Exclude" : "Include") + " unwatched"
                        ToolTip.delay: 300
                    }
                    ToolButton {
                        id: selectorWatched
                        height: parent.buttonSize
                        width: height
                        enabled: true
                        checkable: true
                        checked: true

                        icon {
                            height: parent.buttonSize
                            width: parent.buttonSize
                            source: "/images/videoChecked.png"
                        }
                        display: AbstractButton.IconOnly

                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: ((checked) ? "Exclude" : "Include") + " Watched"
                        ToolTip.delay: 300

                        Image {
                            anchors.centerIn: parent
                            source: "/images/Checked.png"
                            width: parent.width * .6
                            height: parent.height * .6
                            z: parent.z + 1
                        }
                    }
                    ToolButton {
                        id: selectorShorts
                        height: parent.buttonSize
                        width: height
                        enabled: true
                        checkable: true
                        checked: true

                        icon {
                            height: parent.buttonSize
                            width: parent.buttonSize
                            source: "/images/short.png"
                        }
                        display: AbstractButton.IconOnly

                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: ((checked) ? "Exclude" : "Include") + " Shorts"
                        ToolTip.delay: 300
                    }
                    ToolButton {
                        id: selectorSaved
                        height: parent.buttonSize
                        width: height
                        enabled: true
                        checkable: true
                        checked: true

                        icon {
                            height: parent.buttonSize
                            width: parent.buttonSize
                            source: "/icons/snippet_folder.svg"
                        }
                        display: AbstractButton.IconOnly

                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: ((checked) ? "Exclude" : "Include") + " videos with storage data"
                        ToolTip.delay: 300
                    }
                    ToolButton {
                        id: selectorUnsaved
                        height: parent.buttonSize
                        width: height
                        enabled: true
                        checkable: true
                        checked: true

                        icon {
                            height: parent.buttonSize
                            width: parent.buttonSize
                            source: "/icons/scan_delete.svg"
                        }
                        display: AbstractButton.IconOnly

                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: ((checked) ? "Exclude" : "Include") + " videos without storage data"
                        ToolTip.delay: 300
                    }
                } // RowLayout
            } // ToolBar
        } // Column
    } // Rectangle

    MouseArea {
        anchors.fill: view
        acceptedButtons: Qt.RightButton
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                parent.contextMenu.deleteCategoryItem = parent.contextMenu.deleteVideoItem = false
                parent.contextMenu.popup()
            }
        }
    }

    // not working apparently
//    function expandAll() {
//        for(var i=0; i < view.model.rowCount(); i++) {
//            var index = view.model.index(i,0)
//            if(!view.isExpanded(index)) {
//                view.expand(index)
//            }
//        }
//    }

//    function collapse()  {
//        for(var i=0; i < view.model.rowCount(); i++) {
//            var index = view.model.index(i,0)
//            if(view.isExpanded(index)) {
//                view.collapse(index)
//            }
//        }
//    }

    TreeView {
        id: view
        clip: true
        reuseItems: false
        boundsBehavior: Flickable.StopAtBounds

        anchors {
            left: parent.left
            right: parent.right
            top: (viewContainer.showFiltering)
                 ? filterContainer.bottom
                 : parent.top
            bottom: parent.bottom
        }

        model: (viewContainer.model !== null)
               ? viewContainer.model.sortFilterProxyModel : null
        rootIndex: (viewContainer.model !== null)
                   ? viewContainer.model.rootPathIndex : fileSystemModel.nullIndex
        selectionMode: TableView.SingleSelection

        focus: true

        alternatingRows: false

        columnWidthProvider: function(column) {
            return column === 0 ? width : 0
        }

        property string selectedKey: ""
        property string contextedKey: ""

        delegate: Rectangle {
            // Assigned to by TreeView:
            id: treeViewDelegate

            // The following properties are assigned to by TreeView:
            required property TreeView treeView
            required property bool isTreeNode
            required property bool expanded
            required property bool hasChildren
            required property int depth
            required property int row
            required property int column
            required property bool current
            required property string display
            required property bool isDirectory // from IsDirRole

            readonly property real indent: 24
            readonly property real padding: (isDirectory)
                                            ? 0
                                            : viewContainer._branchIndicatorSize
            implicitHeight: column === 0 ? viewContainer._rowHeight : 0
            border.color: (ma.drag.active)
                          ? "red"
                          : (!treeViewDelegate.isDirectory
                                && ((view.selectedKey === key)
                                    || (view.contextedKey === key)))
                            ? "maroon"
                            : (da.hovered
                               ? "green"
                               : "transparent")
            border.width: 2
            color: (treeViewDelegate.isDirectory)
                   ? YaycProperties.categoryBgColor
                   : YaycProperties.fileBgColor

            property var qmodelindex: treeView.index(row, column)

            property bool initialized: qmodelindex !== undefined

            required property string key // from KeyRole
            property int revision: 0

            property real duration: {
                if (!viewContainer.historyView
                    && !treeViewDelegate.isDirectory
                    && initialized)
                {
                    let d = viewContainer.model.duration(key)
                    if (d === 0 && playing) {
                        viewContainer.heartbeat // Dependency, triggered by progressTimer
                    }
                    return d
                }
                return 0
            }
            property real progress: {
                if (!viewContainer.historyView
                    && !treeViewDelegate.isDirectory
                    && key.length > 0
                    && row >= 0)
                {
                    if (playing) viewContainer.heartbeat // to trigger updates on playing
                    return viewContainer.model.progress(key)
                }
                return 0
            }
            property string title: (viewContainer.model && !treeViewDelegate.isDirectory) ? viewContainer.model.title(key) : "" // with key it doesn't update, somehow
            property bool playing: (!treeViewDelegate.isDirectory)
                                   ? (viewContainer.webEngineViewKey === key)
                                   : false
            property string videoUrl: (!treeViewDelegate.isDirectory)
                                      ? viewContainer.model.videoUrl(key)
                                      : ""
            property string videoIconUrl: {
                if (!treeViewDelegate.isDirectory) {
                    if (playing) {
                        viewContainer.heartbeat
                    }
                    revision // force re-evaluation on metadata change
                    return viewContainer.model.videoIconUrl(key)
                }
                return ""
            }
            property bool starred: {
                if (!treeViewDelegate.isDirectory) {
                    revision // force re-evaluation on metadata change
                    return viewContainer.model.isStarred(key)
                }
                return false
            }
            property int hasWorkingDir: {
                if (!treeViewDelegate.isDirectory && viewContainer.extWorkingDirExists) {
                    revision // force re-evaluation on metadata change
                    return viewContainer.model.hasWorkingDir(key, viewContainer.extWorkingDirPath)
                }
                return 0
            }
            property bool shorts: (!treeViewDelegate.isDirectory)
                                  ? utilities.isYoutubeShortsUrl(videoUrl)
                                  : false
            property string creationDate: (treeViewDelegate.isDirectory) ? ""
                                          : viewContainer.model.creationDate(key)

            // might not be needed
            // onQmodelindexChanged: Qt.binding(treeViewDelegate.updateProgress())
            // onVisibleChanged: Qt.binding(treeViewDelegate.updateProgress())
            // onKeyChanged:  Qt.binding(treeViewDelegate.updateProgress())

            ToolTip {
                visible: !scrollBarMA.containsMouse
                         && ma.containsMouse
                         && !treeViewDelegate.isDirectory

                text: treeViewDelegate.title + "\n"
                      + "Added " + treeViewDelegate.creationDate
                      + "  -- Duration "
                      + treeViewDelegate.duration
                      + " -- " + treeViewDelegate.key
                delay: 300
                font {
                    family: mainFont.name
                }

                Image {
                    id: tooltipThumbnail
                    visible: parent.visible
                    source : (visible && treeViewDelegate.key !== "")
                             ? "image://videothumbnail/" + treeViewDelegate.key
                             : ""
                    asynchronous: true
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

            readonly property real nestPadding: treeViewDelegate.padding
                                                + (treeViewDelegate.depth * treeViewDelegate.indent)
            Row {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                x: treeViewDelegate.nestPadding
                Item {
                    id: branchIndicator
                    property int indicatorSize: (treeViewDelegate.isDirectory)
                                                ? viewContainer._branchIndicatorSize
                                                : 0
                    width: indicatorSize
                    height: indicatorSize
                    Image {
                        visible: treeViewDelegate.hasChildren // arrow only when expandable
                        width: branchIndicator.indicatorSize
                        height: branchIndicator.indicatorSize
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: 6
                        source: "qrc:/images/arrow.png"
                        rotation: treeViewDelegate.expanded ? 0 : -90
                    }
                }
                Row {
                    height: parent.height
                    spacing: 2
                    visible: true
                    Image {
                        id: iconVideo
                        visible: !treeViewDelegate.isDirectory
                        anchors.verticalCenter: parent.verticalCenter
                        source: treeViewDelegate.videoIconUrl
                        fillMode: Image.PreserveAspectFit
                        height: parent.height

                        Image {
                            id: iconStarred
                            visible: treeViewDelegate.starred
                            anchors.fill: parent
                            source: "qrc:/images/starred.png"
                        }
                        Image {
                            id: iconDataPresent
                            visible: treeViewDelegate.hasWorkingDir == 2
                            anchors.fill: parent
                            source: "qrc:/images/workingdirpresent.png"
                        }
                        Image {
                            id: iconSummaryPresent
                            visible: treeViewDelegate.hasWorkingDir == 1
                            anchors.fill: parent
                            source: "qrc:/images/workingdirpresentempty.png"
                        }
                    } // Video Indicator
                    Image {
                        visible: treeViewDelegate.isDirectory
                        anchors.verticalCenter: parent.verticalCenter
                        source: YaycProperties.isDarkMode ? "qrc:/images/folder-128.png"
                                                          : "qrc:/images/folder-128-light.png"
                        fillMode: Image.PreserveAspectFit
                        height: parent.height
                    } // Category Indicator

                    Text {
                        id: itemText
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignLeft
                        anchors.verticalCenter: parent.verticalCenter
                        text: treeViewDelegate.isDirectory ? display : title
                        elide: Text.ElideRight
                        color: (!treeViewDelegate.isDirectory
                                // && !treeViewDelegate.shorts
                                && treeViewDelegate.duration === 0.0)
                               ? YaycProperties.disabledTextColor
                               : YaycProperties.textColor
                        renderType: Text.QtRendering
                        font {
                            pixelSize: YaycProperties.fsP1
                            family: mainFont.name
                        }
                    } // Delegate text
                } // ContentItem Row
            } // Delegate Row

            DropArea {
                id: da
                anchors.fill: parent
                enabled: !viewContainer.historyView
                         && treeViewDelegate.isDirectory
                         && !ma.drag.active
                visible: enabled
                property bool hovered: false
                onEntered: {
                    if (viewContainer.historyView)
                        return
                    hovered = true
                }
                onExited: {
                    if (viewContainer.historyView)
                        return
                    hovered = false
                }
                onDropped:
                {
                    if (viewContainer.historyView)
                        return

                    hovered = false
                    if (typeof(drag.source.key) === "undefined") { // moving category
                        viewContainer.model.moveEntry(drag.source.qmodelindex, treeViewDelegate.qmodelindex)
                    } else {
                        viewContainer.model.moveVideo(drag.source.key, treeViewDelegate.qmodelindex)
                    }
                }
            } // DropArea da
            MouseArea {
                id: ma
                anchors.fill: parent // treeViewDelegate
                enabled: true // !styleData.hasChildren allow categories to be moved
                visible: enabled
                drag.target: (!viewContainer.historyView)
                               ? dummy
                               : undefined
                drag.smoothed: false // Disable smoothed so that the Item pixel from where we started the drag remains under the mouse cursor
                acceptedButtons: Qt.RightButton | Qt.LeftButton
                hoverEnabled: true

                function contextualAction() {
                    if (treeViewDelegate.isDirectory) {
                        viewContainer.contextMenu.targetDelegate = null
                        viewContainer.contextMenu.setCategoryIndex(treeViewDelegate.qmodelindex)
                        viewContainer.contextMenu.popup()
                    } else {
                        viewContainer.contextMenu.targetDelegate = treeViewDelegate
                        viewContainer.contextMenu.setVideoIndex(treeViewDelegate.qmodelindex)
                        viewContainer.contextMenu.popup()
                    }
                }
                cursorShape: (treeViewDelegate.isDirectory)
                             ? Qt.ArrowCursor
                             : Qt.PointingHandCursor
                onClicked: (mouse) => {
                    if (mouse.button === Qt.RightButton) {
                        contextualAction()
                    } else if (mouse.button === Qt.LeftButton) {
                        if (treeViewDelegate.isDirectory) {
                            return
                        }
                        var url = treeViewDelegate.videoUrl
                        // webEngineView.url = url;
                        viewContainer.videoSelected(url)
                    }
                }
                pressAndHoldInterval: 1500
                onPressAndHold: contextualAction()
                onDoubleClicked: {
                    if (treeViewDelegate.hasChildren || treeViewDelegate.expanded) {
                        if (treeViewDelegate.expanded)
                            treeView.collapse(treeViewDelegate.row)
                        else
                            treeView.expand(treeViewDelegate.row)
                    }
                }
            } // MouseArea ma
            Rectangle {
                id: progressBar
                visible: !treeViewDelegate.isDirectory
                height: (treeViewDelegate.playing) ? 3 : 1
                property int totalWidth : parent.width // - itemText.x
                property real progress: (treeViewDelegate.isDirectory)
                                        ? 0
                                        : treeViewDelegate.progress

                anchors {
                    left: parent.left
                    leftMargin: treeViewDelegate.nestPadding + iconVideo.width
                    bottom: parent.bottom
                    bottomMargin: 1
                    right: parent.right
                    rightMargin: (1. - progress) * totalWidth
                }
                color: (treeViewDelegate.playing)
                       ? "green"
                       : "red"
            } // ProgressBar
        } // Delegate Root (Rectangle)
    } // QC1.TreeView

    Rectangle {
        id: scrollBar // touch-friendly scrollbar
        visible: scrollHandle.height < view.height - 1
        color: YaycProperties.iconColor
        opacity: 0.2
        anchors {
            bottom: view.bottom
            top: view.top
            right: view.right
        }
        width: 36
        MouseArea {
            id: scrollBarMA
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: true
            onClicked: (mouse) => {
                mouse.accepted = true
            }
            Rectangle {
                id: scrollHandle
                color: YaycProperties.iconColor
                opacity: 0.01
                anchors {
                    left: parent.left
                    right: parent.right
                }
                // Use row count * row height for reliable content height after filtering
                property real estimatedContentHeight: Math.max(view.rows * viewContainer._rowHeight, view.height)
                property int scrollableHeight: (view.height - scrollHandle.height)
                property int scrollableContentHeight: (estimatedContentHeight - view.height)
                height:  Math.max(
                            view.height * (view.height / estimatedContentHeight)
                            ,16 )
                y: scrollableContentHeight > 0
                   ? Math.max(0, Math.min( (view.contentY / scrollableContentHeight), 1.0)) * scrollableHeight
                   : 0

                onYChanged: {
                    if (!scrollHandleMA.drag.active)
                        return
                    view.contentY = (y / scrollableHeight) * scrollableContentHeight
                }

                MouseArea {
                    id: scrollHandleMA
                    anchors.fill: parent
                    drag {
                        target: scrollHandle
                        axis: Drag.YAxis
                        minimumY: 0
                        maximumY: view.height - scrollHandle.height
                    }
                }
            }
        }
    }
    Rectangle {
        id: visualHandle
        visible: scrollBar.visible
        anchors.left: scrollBar.left
        y: scrollHandle.y + view.y
        width: scrollHandle.width
        height: scrollHandle.height
        color: YaycProperties.iconColor
        opacity: .5
    }
}
