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

    QC1.TreeView {
        id: view

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
        selectionMode: 0

        focus: true
        headerVisible: false
        alternatingRowColors: false
        backgroundVisible: false
        property string selectedKey: ""
        property string contextedKey: ""

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
                              : (!styleData.hasChildren && ((view.selectedKey === key) || (view.contextedKey === key)))
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
                                        ? viewContainer.model.duration(qmodelindex)
                                        : 0
                property real progress: (!styleData.hasChildren)
                                        ? viewContainer.model.progress(key)
                                        : 0
                property string title: viewContainer.model.title(qmodelindex) // with key it doesn't update, somehow
                property bool playing: (!styleData.hasChildren)
                                       ? (webEngineView.key === key)
                                       : false
                property string videoUrl: (!styleData.hasChildren)
                                          ? viewContainer.model.videoUrl(qmodelindex)
                                          : ""
                property string videoIconUrl: (!styleData.hasChildren)
                                              ? viewContainer.model.videoIconUrl(qmodelindex)
                                              : ""
                property bool starred: (!styleData.hasChildren)
                                       ? viewContainer.model.isStarred(qmodelindex)
                                       : false
                property int hasWorkingDir: (!styleData.hasChildren && root.extWorkingDirExists)
                                       ? viewContainer.model.hasWorkingDir(key, root.extWorkingDirPath)
                                       : 0
                property bool shorts: (!styleData.hasChildren)
                                      ? utilities.isYoutubeShortsUrl(videoUrl)
                                      : false
                property string creationDate: (styleData.hasChildren) ? ""
                                              : viewContainer.model.creationDate(key)

                onStarredChanged: {
                }

                onQmodelindexChanged: {
                    treeViewDelegate.updateProgress()
                }

                function updateProgress() {
                    if (!viewContainer.historyView
                            && !styleData.hasChildren
                            && key) {
                        progress = viewContainer.model.progress(key)
                    }
                }

                onVisibleChanged: {
                    treeViewDelegate.updateProgress()
                }

                ToolTip {
                    visible: !scrollBarMA.containsMouse
                             && ma.containsMouse
                             && !styleData.hasChildren

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
                        source : (visible && treeViewDelegate.key !== "") ? "image://videothumbnail/" + treeViewDelegate.key : ""
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

                Image {
                    id: iconVideo
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
                }
                Image {
                    id: iconFolder
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
                    color: (!styleData.hasChildren
                            // && !treeViewDelegate.shorts
                            && treeViewDelegate.duration === 0.0)
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
                    enabled: !viewContainer.historyView
                             && styleData.hasChildren
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
                }
                MouseArea {
                    id: ma
                    anchors.fill: parent
                    enabled: true // !styleData.hasChildren allow categories to be moved
                    visible: enabled
                    drag.target: (!viewContainer.historyView)
                                   ? dummy
                                   : undefined
                    drag.smoothed: false // Disable smoothed so that the Item pixel from where we started the drag remains under the mouse cursor
                    acceptedButtons: Qt.RightButton | Qt.LeftButton
                    hoverEnabled: true

                    function contextualAction() {
                        if (styleData.hasChildren) {
                            viewContainer.contextMenu.setCategoryIndex(treeViewDelegate.qmodelindex)
                            viewContainer.contextMenu.popup()
                        } else {
                            viewContainer.contextMenu.setVideoIndex(treeViewDelegate.qmodelindex)
                            viewContainer.contextMenu.popup()
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
//            scrollBarBackground: Rectangle {
//                implicitWidth: 20
//                implicitHeight: 30
//                color: properties.paneBackgroundColor
//            }
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
                var url = viewContainer.model.videoUrl(index)
                webEngineView.url = url;
            }
        }

        verticalScrollBarPolicy: Qt.ScrollBarAlwaysOff

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
                    height:  Math.max(
                                view.height * (view.height / view.__listView.contentHeight)
                                     ,16 )
                    y: Math.max(0, Math.min( (view.__listView.contentY
                        / (view.__listView.contentHeight - view.height))
                                , 1.0))
                       * (view.height - scrollHandle.height)

                    onYChanged: {
                        if (!scrollHandleMA.drag.active)
                            return
                        view.__listView.contentY = y / (view.height - scrollHandle.height)
                                                   * (view.__listView.contentHeight - view.height)
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
