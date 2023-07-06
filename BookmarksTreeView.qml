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
    property bool showFiltering: false
    property bool searchInTitles: true
    property bool searchInChannelNames: true
    property bool historyView
    property var model: (historyView === undefined) ? undefined
                            : ((historyView) ?
                                historyModel : fileSystemModel)

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
            key = viewContainer.model.keyFromViewItem(idx)
            deleteCategoryItem = false
            deleteVideoItem = true
        }

        MenuItem {
            text: "Add category"
            enabled: !viewContainer.historyView
            height: enabled ? implicitHeight : 0
            onClicked: {
                addCategoryDialog.open()
            }
            icon.source: "/icons/create_new_folder.svg"
            display: MenuItem.TextBesideIcon
        }
        MenuItem {
            text: "Add video"
            enabled: !viewContainer.historyView
            height: enabled ? implicitHeight : 0
            onClicked: {
                addVideoDialog.open()
            }
            icon.source: "/icons/add.svg"
            display: MenuItem.TextBesideIcon
        }
        MenuItem {
            text: "Delete category"
            enabled: !viewContainer.historyView
                     && viewContainer.contextMenu.deleteCategoryItem
            visible: true
            height: enabled ? implicitHeight : 0
            onClicked: {
                if (viewContainer.historyView)
                    return
                viewContainer.model.deleteEntry(viewContainer.contextMenu.categoryIndex)
            }
            icon.source: "/icons/folder_delete.svg"
            display: MenuItem.TextBesideIcon
        }
        MenuItem {
            text: "Delete video" + ((viewContainer.historyView) ? " from History": "")
            enabled: viewContainer.contextMenu.deleteVideoItem
            visible: true
            height: enabled ? implicitHeight : 0
            onClicked: {
                viewContainer.model.deleteEntry(viewContainer.contextMenu.videoIndex)
                root.triggerVideoAdded()
            }
            icon.source: "/icons/remove.svg"
            display: MenuItem.TextBesideIcon
        }
        MenuItem {
            TextEdit{
                id: copyLinkClipboardProxy
                visible: false
            }
            text: "Copy Link"
            enabled: viewContainer.contextMenu.deleteVideoItem
            visible: true
            height: enabled ? implicitHeight : 0
            onClicked: {
                copyLinkClipboardProxy.text = viewContainer.model.videoUrl(viewContainer.contextMenu.videoIndex)
                copyLinkClipboardProxy.selectAll();
                copyLinkClipboardProxy.copy()
            }
            icon.source: "/icons/content_copy.svg"
            display: MenuItem.TextBesideIcon
        }
        MenuItem {
            text: "Toggle Star"
            enabled: !viewContainer.historyView
                     && viewContainer.contextMenu.deleteVideoItem
            visible: true
            height: enabled ? implicitHeight : 0
            onClicked: {
                if (viewContainer.historyView)
                    return
                var starred = viewContainer.model.isStarred(viewContainer.contextMenu.key)
                viewContainer.model.starEntry(viewContainer.contextMenu.key, !starred)
                buttonStarVideo.triggerStarred() // ToDo: check
            }
            icon.source: "/icons/"+(fileSystemModel.isStarred(viewContainer.contextMenu.key)
                                    ? "star_fill.svg" : "star.svg")
            display: MenuItem.TextBesideIcon
        }
        MenuItem {
            text: "Toggle Viewed"
            enabled: !viewContainer.historyView
                     && viewContainer.contextMenu.deleteVideoItem
            visible: true
            height: enabled ? implicitHeight : 0
            onClicked: {
                if (viewContainer.historyView)
                    return
                var viewed = viewContainer.model.isViewed(viewContainer.contextMenu.key)
                viewContainer.model.viewEntry(viewContainer.contextMenu.key, !viewed)
            }
            icon.source: "/icons/"+(fileSystemModel.isViewed(viewContainer.contextMenu.key)
                                    ? "check_circle_fill.svg" : "check_circle.svg")
            display: MenuItem.TextBesideIcon
        }
        MenuItem {
            text: (view.selectedKey !== viewContainer.contextMenu.key)
                  ? "Cut"
                  : "Un-Cut"
            enabled:  !viewContainer.historyView
                      && viewContainer.contextMenu.deleteVideoItem
            visible: true
            height: enabled ? implicitHeight : 0
            onClicked: {
                if (view.selectedKey !== viewContainer.contextMenu.key) {
                    view.selectedKey = viewContainer.contextMenu.key
                } else {
                    view.selectedKey = ""
                }
            }
            icon.source: "/icons/content_cut.svg"
            display: MenuItem.TextBesideIcon
        }
        MenuItem {
            text: "Paste"
            enabled: !viewContainer.historyView
                     && viewContainer.contextMenu.deleteCategoryItem
                     && (view.selectedKey !== "")
            visible: true
            height: enabled ? implicitHeight : 0
            onClicked: {
                if (viewContainer.historyView)
                    return
                var key = view.selectedKey
                view.selectedKey = ""
                var res = viewContainer.model.moveVideo(key, viewContainer.contextMenu.categoryIndex)
            }
            icon.source: "/icons/content_paste.svg"
            display: MenuItem.TextBesideIcon
        }
        MenuItem {
            text: "Open containing folder"
            enabled: viewContainer.contextMenu.deleteVideoItem
                     && root.extWorkingDirExists
                     && viewContainer.model.hasWorkingDir(
                             viewContainer.contextMenu.videoIndex,
                             root.extWorkingDirPath)
            visible: true
            height: enabled ? implicitHeight : 0
            onClicked: {
                viewContainer.model.openInBrowser(
                    viewContainer.contextMenu.videoIndex,
                    root.extWorkingDirPath)
            }
            icon.source: "/icons/open_in_browser.svg"
            display: MenuItem.TextBesideIcon
        }
        // ToDo: add Menu for tagging
        Menu {
            id: extAppMenu
            title: "Launch in external app"
            enabled: viewContainer.contextMenu.deleteVideoItem
                     && root.extCommandEnabled // ToDo: enable only if related dir present in external dir
            visible: enabled
            height: enabled ? implicitHeight : 0

            Repeater {
                model: root.externalCommands
                MenuItem {
                    text: root.externalCommands[index].name
                    enabled: viewContainer.contextMenu.deleteVideoItem
                             && root.extCommandEnabled // ToDo: enable only if related dir present in external dir
                    visible: true
                    height: enabled ? implicitHeight : 0
                    onClicked: {
                        // ToDo: use only one model if processes should be tracked
                        viewContainer.model.openInExternalApp(
                              viewContainer.contextMenu.videoIndex,
                              root.externalCommands[index].command,
                              root.extWorkingDirPath)
                    }
                    icon.source: "/icons/extension.svg"
                    display: MenuItem.TextBesideIcon
                }
            }
        }
    }

    onSearchInTitlesChanged: model.searchInTitles = viewContainer.searchInTitles
    onSearchInChannelNamesChanged: model.searchInChannelNames = viewContainer.searchInChannelNames

    function search() {
        viewContainer.model.searchTerm = filterTF.text
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
        height: 48

        Row {
            spacing: 4
            TextField {
                id: filterTF
                width: filterContainer.width * 0.85
                height: filterContainer.height * 0.95

                selectByMouse: true
                onAccepted: {
                    viewContainer.search()
                }
            }
            Column {
                Image {
                    id: filterButton
                    source: "/icons/search.svg"
                    height: filterContainer.height * 0.6
                    width: height
                    enabled: true
                    visible: true
                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        source: filterButton
                        anchors.fill: filterButton
                        color: "white"
                        visible: true
                    }
                    MouseArea {
                        anchors.fill: parent

                        onClicked: {
                            viewContainer.search()
                        }

                        property bool hovered: false
                        onEntered:  hovered = true
                        onExited: hovered = false
                        hoverEnabled: true
                        ToolTip.visible: hovered
                        ToolTip.text: "Search"
                        ToolTip.delay: 300
                    }
                }
                Row {
                    Image {
                        id: filterButtonVideoTitle
                        source:  "/icons/video_file.svg"
                        height: filterContainer.height * 0.3
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
                        height: filterContainer.height * 0.3
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
    }

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

        model: (viewContainer.model !== undefined)
               ? viewContainer.model.sortFilterProxyModel : undefined
        rootIndex: (viewContainer.model !== undefined)
                   ? viewContainer.model.rootPathIndex : undefined
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
                property bool hasWorkingDir: (!styleData.hasChildren && root.extWorkingDirExists)
                                       ? viewContainer.model.hasWorkingDir(key, root.extWorkingDirPath)
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
                var url = viewContainer.model.videoUrl(index)
                webEngineView.url = url;
            }
        }
    } // QC1.TreeView
}
