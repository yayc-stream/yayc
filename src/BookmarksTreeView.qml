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

//import QtQuick.Controls 1.4 as QC1
//import QtQuick.Controls.Styles 1.4 as QC1S

import QtQuick.Layouts
import QtQml.Models
import QtWebChannel
import Qt.labs.settings
import Qt.labs.platform as QLP
import Qt5Compat.GraphicalEffects
import yayc 1.0

Rectangle {
    implicitWidth: 200
    color: "black"
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

    property bool showFiltering: false
    property bool searchInTitles: true
    property bool searchInChannelNames: true
    property bool historyView
    property var model: (historyView === undefined) ? undefined
                            : ((historyView) ?
                                historyModel : fileSystemModel)

    property Menu contextMenu: BookmarkContextMenu {
        isHistoryView: viewContainer.historyView
        model: viewContainer.model
        parentView: view
        onClosed: {
            view.contextedKey = ""
        }
    }

    onSearchInTitlesChanged: model.sortFilterProxyModel.searchInTitles = viewContainer.searchInTitles
    onSearchInChannelNamesChanged: model.sortFilterProxyModel.searchInChannelNames = viewContainer.searchInChannelNames

    onSearchInSavedChanged: {
        model.sortFilterProxyModel.workingDirRoot = root.extWorkingDirPath
        model.sortFilterProxyModel.searchInSaved = viewContainer.searchInSaved;
    }
    onSearchInUnsavedChanged: {
        model.sortFilterProxyModel.workingDirRoot = root.extWorkingDirPath
        model.sortFilterProxyModel.searchInUnsaved = viewContainer.searchInUnsaved;
    }
    onSearchInStarredChanged: model.sortFilterProxyModel.searchInStarred = viewContainer.searchInStarred
    onSearchInUnstarredChanged: model.sortFilterProxyModel.searchInUnstarred = viewContainer.searchInUnstarred
    onSearchInOpenedChanged: model.sortFilterProxyModel.searchInOpened = viewContainer.searchInOpened
    onSearchInUnopenedChanged: model.sortFilterProxyModel.searchInUnopened = viewContainer.searchInUnopened
    onSearchInWatchedChanged: model.sortFilterProxyModel.searchInWatched = viewContainer.searchInWatched
    onSearchInUnwatchedChanged: model.sortFilterProxyModel.searchInUnwatched = viewContainer.searchInUnwatched
    onSearchInShortsChanged: model.sortFilterProxyModel.searchInShorts = viewContainer.searchInShorts


    function search() {
        viewContainer.model.sortFilterProxyModel.searchTerm = filterTF.text
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
                                color: (viewContainer.searchInTitles) ? properties.checkedButtonColor : "white"
                                visible: true
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
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
                                color: (viewContainer.searchInChannelNames) ? properties.checkedButtonColor : "white"
                                visible: true
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
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
        onClicked: {
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

        anchors {
            left: parent.left
            right: parent.right
            top: (viewContainer.showFiltering)
                 ? filterContainer.bottom
                 : parent.top
            bottom: parent.bottom
        }

        model: (viewContainer.model !== undefined)
               ? viewContainer.model.sortFilterProxyModel : undefined
        rootIndex: (viewContainer.model !== undefined)
                   ? viewContainer.model.rootPathIndex : undefined
        selectionMode: TableView.SingleSelection

        focus: true

        alternatingRows: false
//        backgroundVisible: false
        property string selectedKey: ""
        property string contextedKey: ""

//        QC1.TableViewColumn {
//            title: "Name"
//            role: "fileName"
//            resizable: true
//        }

        delegate: Rectangle {
            // Assigned to by TreeView:
            id: treeViewDelegate

            required property TreeView treeView
            required property bool isTreeNode
            required property bool expanded
            required property int hasChildren
            required property int depth


            readonly property real indent: 20
            readonly property real padding: 5
            readonly property int defaultLineHeight: 26 // turns out to be 26/28 on standard desktop, in absence of uncommon characters

            height: Math.round(defaultLineHeight * 1.1)
            border.color: (ma.drag.active)
                          ? "red"
                          : (!treeViewDelegate.hasChildren
                                && ((view.selectedKey === key)
                                    || (view.contextedKey === key)))
                            ? "maroon"
                            : (da.hovered
                               ? "green"
                               : "transparent")
            border.width: 2
            color: (treeViewDelegate.hasChildren)
                   ? properties.categoryBgColor
                   : properties.fileBgColor

            property var qmodelindex: undefined
            property bool initialized: qmodelindex !== undefined
            Component.onCompleted: qmodelindex = view.index(row, column)

            property string key: (!treeViewDelegate.hasChildren && initialized)
                                 ? fileSystemModel.keyFromViewItem(qmodelindex)
                                 : ""

            property real duration: (!treeViewDelegate.hasChildren && initialized)
                                    ? viewContainer.model.duration(key)
                                    : 0
            property real progress: (!treeViewDelegate.hasChildren)
                                    ? viewContainer.model.progress(key)
                                    : 0
            property string title: viewContainer.model.title(key) // with key it doesn't update, somehow
            property bool playing: (!treeViewDelegate.hasChildren)
                                   ? (webEngineView.key === key)
                                   : false
            property string videoUrl: (!treeViewDelegate.hasChildren)
                                      ? viewContainer.model.videoUrl(key)
                                      : ""
            property string videoIconUrl: (!treeViewDelegate.hasChildren)
                                          ? viewContainer.model.videoIconUrl(key)
                                          : ""
            property bool starred: (!treeViewDelegate.hasChildren)
                                   ? viewContainer.model.isStarred(key)
                                   : false
            property bool hasWorkingDir: (!treeViewDelegate.hasChildren && root.extWorkingDirExists)
                                   ? viewContainer.model.hasWorkingDir(key, root.extWorkingDirPath)
                                   : false
            property bool shorts: (!treeViewDelegate.hasChildren)
                                  ? utilities.isYoutubeShortsUrl(videoUrl)
                                  : false
            property string creationDate: (treeViewDelegate.hasChildren) ? ""
                                          : viewContainer.model.creationDate(key)

            onQmodelindexChanged: treeViewDelegate.updateProgress()
            onVisibleChanged: treeViewDelegate.updateProgress()

            function updateProgress() {
                if (!viewContainer.historyView
                        && !treeViewDelegate.hasChildren
                        && key) {
                    progress = viewContainer.model.progress(key)
                }
            }

            ToolTip {
                visible: !scrollBarMA.containsMouse
                         && ma.containsMouse
                         && !treeViewDelegate.hasChildren

                text: treeViewDelegate.title + "\n"
                      + "Added " + treeViewDelegate.creationDate
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

            Row {
                anchors.fill: parent
                Item {
                    id: branchIndicator
                    property int size: (treeViewDelegate.hasChildren) ? 16 : 0
                    width: size
                    height: size
                    Image {
                        visible: column === 0 && treeViewDelegate.hasChildren
                        anchors.fill: parent
                        anchors.verticalCenterOffset: 2
                        source: "qrc:/images/arrow.png"
                        transform: Rotation {
                            origin.x: width / 2
                            origin.y: height / 2
                            angle: treeViewDelegate.expanded ? 0 : -90
                        }
                    }
                }
                Row {
                    height: parent.height
                    spacing: 2
                    Image {
                        visible: !treeViewDelegate.hasChildren
                        anchors.verticalCenter: parent.verticalCenter
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
                    } // Video Indicator
                    Image {
                        visible: treeViewDelegate.hasChildren
                        anchors.verticalCenter: parent.verticalCenter
                        source: "qrc:/images/folder-128.png"
                        fillMode: Image.PreserveAspectFit
                    } // Category Indicator


                    Text {
                        id: itemText
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignLeft
                        anchors.verticalCenter: parent.verticalCenter
                        text: treeViewDelegate.hasChildren ? display : title
                        elide: Text.ElideRight
                        color: (!treeViewDelegate.hasChildren
                                // && !treeViewDelegate.shorts
                                && treeViewDelegate.duration === 0.0)
                               ? properties.disabledTextColor
                               : properties.textColor
                        renderType: Text.QtRendering
                        font {
                            pixelSize: properties.fsP1
                            family: mainFont.name
                        }

                        Rectangle {
                            id: progressBar
                            visible: !treeViewDelegate.hasChildren
                            height: (treeViewDelegate.playing) ? 3 : 1
                            property int totalWidth : parent.width // - itemText.x
                            property real progress: (treeViewDelegate.hasChildren)
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
                    } // Delegate text
                } // ContentItem Row
            } // Delegate Row
            DropArea {
                id: da
                anchors.fill: parent
                enabled: !viewContainer.historyView
                         && treeViewDelegate.hasChildren
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
                    if (typeof(drag.source.key) !== "undefined") { // moving category
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
                    if (treeViewDelegate.hasChildren) {
                        viewContainer.contextMenu.setCategoryIndex(treeViewDelegate.qmodelindex)
                        viewContainer.contextMenu.popup()
                    } else {
                        viewContainer.contextMenu.setVideoIndex(treeViewDelegate.qmodelindex)
                        viewContainer.contextMenu.popup()
                    }
                }
                cursorShape: (treeViewDelegate.hasChildren)
                             ? Qt.ArrowCursor
                             : Qt.PointingHandCursor
                onClicked: {
                    if (mouse.button === Qt.RightButton) {
                        contextualAction()
                    } else if (mouse.button === Qt.LeftButton) {
                        if (treeViewDelegate.hasChildren) {
                            return
                        }
                        var url = treeViewDelegate.videoUrl
                        webEngineView.url = url;
                    }
                }
                pressAndHoldInterval: 1000
                onPressAndHold: contextualAction()
                onDoubleClicked: {
                    if (treeViewDelegate.hasChildren) {
                        if (view.isExpanded(treeViewDelegate.qmodelindex))
                            view.collapse(treeViewDelegate.qmodelindex)
                        else
                            view.expand(treeViewDelegate.qmodelindex)
                    }
                }
            } // MouseArea ma
        } // Delegate Root (Rectangle)


//        onActivated : {
//            if (!styleData.hasChildren) {
//                var url = viewContainer.model.videoUrl(index)
//                webEngineView.url = url;
//            }
//        }

        //verticalScrollBarPolicy: Qt.ScrollBarAlwaysOff

        Rectangle {
            id: scrollBar // touch-friendly scrollbar
            visible: scrollHandle.height < view.height - 1
            color: "white"
            opacity: 0.2
            anchors {
                bottom: parent.bottom
                top: parent.top
                right: parent.right
            }
            width: 36
            MouseArea {
                id: scrollBarMA
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                hoverEnabled: true
                onClicked:  {
                    event.accepted = true
                }
                Rectangle {
                    id: scrollHandle
                    color: "white"
                    opacity: 0.01
                    anchors {
                        left: parent.left
                        right: parent.right
                    }
                    property int scrollableHeight: (view.height - scrollHandle.height)
                    property int scrollableContentHeight: (view.contentHeight - view.height)
                    height:  Math.max(
                                view.height * (view.height / view.contentHeight)
                                ,16 )
                    y: Math.max(0, Math.min( (view.contentY / scrollableContentHeight), 1.0))
                       * scrollableHeight

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
            y: scrollHandle.y
            width: scrollHandle.width
            height: scrollHandle.height
            color: "white"
            opacity: .5
        }

    } // QC1.TreeView
}
