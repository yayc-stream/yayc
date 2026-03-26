/*
Copyright (C) 2023- YAYC team <yaycteam@gmail.com>

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
import Qt5Compat.GraphicalEffects
import yayc 1.0

Menu {
    id: rootItem
    cascade: true
    property bool deleteCategoryItem: false
    property bool deleteVideoItem: false
    property var categoryIndex: undefined
    property var videoIndex: undefined
    property var model
    property string key: ""
    property bool isHistoryView: true
    property var parentView: null  // It's null when spawned from the + button in the toolbar
    property var parentContainer: null
    required property bool extWorkingDirExists
    required property string extWorkingDirPath
    required property var externalCommands
    required property bool removeStorageOnDelete
    required property bool extCommandEnabled

    onOpened: {
        // workaround for the submenu occasionally showing opened
        extAppMenu.close()
        moveToMenu.close()
    }

    property string categoryName: ""

    function setCategoryIndex(idx, name) {
        if (parentView)
            parentView.contextedKey = ""
        categoryIndex = idx
        categoryName = name || ""
        videoIndex = undefined
        deleteCategoryItem = true
        deleteVideoItem = false
    }

    function setVideoIndex(idx) {
        videoIndex = idx
        categoryIndex = undefined
        key = rootItem.model.keyFromViewItem(idx)
        if (parentView)
            parentView.contextedKey = key
        deleteCategoryItem = false
        deleteVideoItem = true
    }

    function setKey(k) {
        videoIndex = null
        categoryIndex = null
        key = k
        deleteCategoryItem = false
        deleteVideoItem = true
    }

    Menu {
        id: moveToMenu
        title: "Move to..."
        icon.source: "/icons/move.svg"
        enabled: rootItem.deleteVideoItem
                 && rootItem.model
        visible: enabled
        height: enabled ? implicitHeight : 0

        MenuItem {
            text: "/"
            onClicked: rootItem.model.moveEntry(rootItem.key, rootItem.model.bookmarksRootPath)
        }
        Repeater {
            model: (moveToMenu.enabled) ? rootItem.model.recentDestinations : undefined
            MenuItem {
                required property var modelData
                text: modelData.name
                onClicked: rootItem.model.moveEntry(rootItem.key, modelData.path)
            }
        }
    }
    MenuItem {
        text: "Set as destination"
        enabled: !rootItem.isHistoryView && rootItem.parentView
                 && rootItem.deleteCategoryItem
        visible: enabled
        height: enabled ? implicitHeight : 0
        onClicked: {
            rootItem.model.setLastDestinationCategory(rootItem.categoryIndex)
        }
        icon.source: "/icons/move.svg"
        display: MenuItem.TextBesideIcon
    }
    MenuItem {
        text: "Add category"
        enabled: !rootItem.isHistoryView && rootItem.parentView
        height: enabled ? implicitHeight : 0
        onClicked: {
            addCategoryDialog.parentCategoryIndex = rootItem.categoryIndex
            addCategoryDialog.parentCategoryName = rootItem.categoryName
            addCategoryDialog.open()
        }
        icon.source: "/icons/create_new_folder.svg"
        display: MenuItem.TextBesideIcon
    }
    MenuItem {
        text: "Add video"
        enabled: !rootItem.isHistoryView && rootItem.parentView
        height: enabled ? implicitHeight : 0
        onClicked: {
            addVideoDialog.open()
        }
        icon.source: "/icons/add.svg"
        display: MenuItem.TextBesideIcon
    }
    MenuItem {
        text: "Delete category"
        enabled: !rootItem.isHistoryView && rootItem.parentView
                 && rootItem.deleteCategoryItem
        visible: true
        height: enabled ? implicitHeight : 0
        onClicked: {
            if (rootItem.isHistoryView)
                return
            rootItem.model.deleteEntry(rootItem.categoryIndex)
            if (rootItem.parentContainer)
                rootItem.parentContainer.refreshLayout()
        }
        icon.source: "/icons/folder_delete.svg"
        display: MenuItem.TextBesideIcon
    }
    MenuItem {
        text: "Delete video" + ((rootItem.isHistoryView) ? " from History": "")
        enabled: (rootItem.deleteVideoItem || !rootItem.parentView)
        visible: true
        height: enabled ? implicitHeight : 0
        onClicked: {
            rootItem.model.deleteEntry(rootItem.key,
                                            (rootItem.removeStorageOnDelete)
                                            ? rootItem.extWorkingDirPath
                                            : "",
                                            rootItem.removeStorageOnDelete)
            // root.triggerVideoAdded() FIXME
            if (rootItem.parentContainer)
                rootItem.parentContainer.refreshLayout()
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
        enabled: rootItem.parentView && rootItem.deleteVideoItem
        visible: true
        height: enabled ? implicitHeight : 0
        onClicked: {
            copyLinkClipboardProxy.text = rootItem.model.videoUrl(rootItem.videoIndex)
            copyLinkClipboardProxy.selectAll();
            copyLinkClipboardProxy.copy()
        }
        icon.source: "/icons/content_copy.svg"
        display: MenuItem.TextBesideIcon
    }
    MenuItem {
        text: "Toggle Star"
        enabled: !rootItem.isHistoryView
                 && (rootItem.deleteVideoItem || !rootItem.parentView)
        visible: true
        height: enabled ? implicitHeight : 0
        onClicked: {
            if (rootItem.isHistoryView)
                return
            var starred = rootItem.model.isStarred(rootItem.key)
            rootItem.model.starEntry(rootItem.key, !starred)
            rootItem.model.bumpVersion(rootItem.key)
        }
        icon.source: "/icons/"+(fileSystemModel.isStarred(rootItem.key)
                                ? "star_fill.svg" : "star.svg")
        display: MenuItem.TextBesideIcon
    }
    MenuItem {
        text: "Toggle Viewed"
        enabled: !rootItem.isHistoryView
                 && (rootItem.deleteVideoItem || !rootItem.parentView)
        visible: true
        height: enabled ? implicitHeight : 0
        onClicked: {
            if (rootItem.isHistoryView)
                return
            var viewed = rootItem.model.isViewed(rootItem.key)
            rootItem.model.viewEntry(rootItem.key, !viewed)
            rootItem.model.bumpVersion(rootItem.key)
        }
        icon.source: "/icons/"+(fileSystemModel.isViewed(rootItem.key)
                                ? "check_circle_fill.svg" : "check_circle.svg")
        display: MenuItem.TextBesideIcon
    }
    MenuItem {
        text: (rootItem.parentView && rootItem.parentView.selectedKey !== rootItem.key)
              ? "Cut"
              : "Un-Cut"
        enabled:  !rootItem.isHistoryView && rootItem.parentView
                  && rootItem.deleteVideoItem
        visible: true
        height: enabled ? implicitHeight : 0
        onClicked: {
            if (parentView.selectedKey !== rootItem.key) {
                parentView.selectedKey = rootItem.key
            } else {
                parentView.selectedKey = ""
            }
        }
        icon.source: "/icons/content_cut.svg"
        display: MenuItem.TextBesideIcon
    }
    MenuItem {
        text: "Paste"
        enabled: (!rootItem.isHistoryView && rootItem.parentView)
                 && rootItem.deleteCategoryItem
                 && (parentView && parentView.selectedKey !== "")
        visible: true
        height: enabled ? implicitHeight : 0
        onClicked: {
            if (rootItem.isHistoryView)
                return
            var key = parentView.selectedKey
            parentView.selectedKey = ""
            var res = rootItem.model.moveVideo(key, rootItem.categoryIndex)
        }
        icon.source: "/icons/content_paste.svg"
        display: MenuItem.TextBesideIcon
    }
    MenuItem {
        text: "Open containing folder"
        enabled: (rootItem.deleteVideoItem || !rootItem.parentView)
                 && rootItem.extWorkingDirExists
                 && rootItem.model.hasWorkingDir(
                     rootItem.key,
                     rootItem.extWorkingDirPath)
        visible: true
        height: enabled ? implicitHeight : 0
        onClicked: {
            rootItem.model.openInBrowser(
                        rootItem.key,
                        rootItem.extWorkingDirPath)
        }
        icon.source: "/icons/open_in_browser.svg"
        display: MenuItem.TextBesideIcon
    }
    MenuItem {
        text: "Delete storage data"
        enabled: (rootItem.deleteVideoItem || !rootItem.parentView)
                 && rootItem.extWorkingDirExists
                 && rootItem.model.hasWorkingDir(
                     rootItem.key,
                     rootItem.extWorkingDirPath)
        visible: true
        height: enabled ? implicitHeight : 0
        onClicked: {
            rootItem.model.deleteStorage(
                        rootItem.key,
                        rootItem.extWorkingDirPath)
        }
        icon.source: "/icons/delete_forever.svg"
        display: MenuItem.TextBesideIcon
    }
    // ToDo: add Menu for tagging
    Menu {
        id: extAppMenu
        title: "Launch in external app"
        enabled: rootItem.extCommandEnabled
        height: enabled ? implicitHeight : 0

        Repeater {
            model: (extAppMenu.enabled)
                    ? rootItem.externalCommands
                    : undefined
            MenuItem {
                text: (rootItem.externalCommands[index])
                        ? rootItem.externalCommands[index].name
                        : ""
                enabled: true
                visible: true
                height: enabled ? implicitHeight : 0
                onClicked: {
                    if (rootItem.categoryIndex !== undefined
                            && rootItem.categoryIndex !== null) {
                        rootItem.model.enqueueCategoryExternalApp(
                                    rootItem.categoryIndex,
                                    rootItem.externalCommands[index].command,
                                    rootItem.extWorkingDirPath)
                    } else {
                        rootItem.model.enqueueExternalApp(
                                    rootItem.key,
                                    rootItem.externalCommands[index].command,
                                    rootItem.extWorkingDirPath)
                    }
                }
                icon.source: "/icons/extension.svg"
                display: MenuItem.TextBesideIcon
            }
        }
    }
}
