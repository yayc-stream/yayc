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
        title: uiTr("Move to...")
        icon.source: "/icons/move.svg"
        enabled: (rootItem.deleteVideoItem || rootItem.deleteCategoryItem) && rootItem.model
        visible: enabled

        function moveTo(destinationPath) {
            if (rootItem.deleteCategoryItem)
                rootItem.model.moveEntry(rootItem.categoryIndex, destinationPath)
            else
                rootItem.model.moveEntry(rootItem.key, destinationPath)
        }

        MenuItem {
            text: "/"
            onClicked: moveToMenu.moveTo(rootItem.model.bookmarksRootPath)
        }
        Repeater {
            model: (moveToMenu.enabled) ? rootItem.model.recentDestinations : undefined
            MenuItem {
                required property var modelData
                text: modelData.name
                onClicked: moveToMenu.moveTo(modelData.path)
            }
        }
    }
    Repeater {
        model: (!rootItem.isHistoryView && rootItem.parentView
                && rootItem.deleteCategoryItem) ? 1 : 0
        MenuItem {
            text: uiTr("Set as destination")
            onClicked: {
                rootItem.model.setLastDestinationCategory(rootItem.categoryIndex)
            }
            icon.source: "/icons/move.svg"
            display: MenuItem.TextBesideIcon
        }
    }
    MenuItem {
        text: uiTr("Add category")
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
        text: uiTr("Add video")
        enabled: !rootItem.isHistoryView && rootItem.parentView
        height: enabled ? implicitHeight : 0
        onClicked: {
            if (rootItem.deleteCategoryItem && rootItem.categoryIndex !== undefined) {
                addVideoDialog.destination = rootItem.model.categoryPath(rootItem.categoryIndex)
            } else {
                addVideoDialog.destination = ""
            }
            addVideoDialog.open()
        }
        icon.source: "/icons/add.svg"
        display: MenuItem.TextBesideIcon
    }
    Repeater {
        model: (!rootItem.isHistoryView && rootItem.parentView
                && rootItem.deleteCategoryItem) ? 1 : 0
        MenuItem {
            text: uiTr("Reload category")
            onClicked: {
                rootItem.model.reloadCategory(rootItem.categoryIndex)
            }
            icon.source: "/icons/refresh.svg"
            display: MenuItem.TextBesideIcon
        }
    }
    Repeater {
        model: (!rootItem.isHistoryView && rootItem.parentView
                && rootItem.deleteCategoryItem) ? 1 : 0
        MenuItem {
            text: uiTr("Delete category")
            onClicked: {
                rootItem.model.deleteEntry(rootItem.categoryIndex)
                if (rootItem.parentContainer)
                    rootItem.parentContainer.refreshLayout()
            }
            icon.source: "/icons/folder_delete.svg"
            display: MenuItem.TextBesideIcon
        }
    }
    Repeater {
        model: (rootItem.deleteVideoItem || !rootItem.parentView) ? 1 : 0
        MenuItem {
            text: uiTr("Delete video") + ((rootItem.isHistoryView) ? " " + uiTr("from History"): "")
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
    }
    Repeater {
        model: (rootItem.parentView && rootItem.deleteVideoItem) ? 1 : 0
        MenuItem {
            TextEdit{
                id: copyLinkClipboardProxy
                visible: false
            }
            text: uiTr("Copy Link")
            onClicked: {
                copyLinkClipboardProxy.text = rootItem.model.videoUrl(rootItem.videoIndex)
                copyLinkClipboardProxy.selectAll();
                copyLinkClipboardProxy.copy()
            }
            icon.source: "/icons/content_copy.svg"
            display: MenuItem.TextBesideIcon
        }
    }
    Repeater {
        model: (!rootItem.isHistoryView
                && (rootItem.deleteVideoItem || !rootItem.parentView)) ? 1 : 0
        MenuItem {
            text: uiTr("Toggle Star")
            onClicked: {
                var starred = rootItem.model.isStarred(rootItem.key)
                rootItem.model.starEntry(rootItem.key, !starred)
                rootItem.model.bumpVersion(rootItem.key)
            }
            icon.source: "/icons/"+(fileSystemModel.isStarred(rootItem.key)
                                    ? "star_fill.svg" : "star.svg")
            display: MenuItem.TextBesideIcon
        }
    }
    Repeater {
        model: (!rootItem.isHistoryView
                && (rootItem.deleteVideoItem || !rootItem.parentView)) ? 1 : 0
        MenuItem {
            text: uiTr("Toggle Viewed")
            onClicked: {
                var viewed = rootItem.model.isViewed(rootItem.key)
                rootItem.model.viewEntry(rootItem.key, !viewed)
                rootItem.model.bumpVersion(rootItem.key)
            }
            icon.source: "/icons/"+(fileSystemModel.isViewed(rootItem.key)
                                    ? "check_circle_fill.svg" : "check_circle.svg")
            display: MenuItem.TextBesideIcon
        }
    }
    Repeater {
        model: (!rootItem.isHistoryView && rootItem.parentView
                && rootItem.deleteVideoItem) ? 1 : 0
        MenuItem {
            text: (rootItem.parentView && rootItem.parentView.selectedKey !== rootItem.key)
                  ? uiTr("Cut")
                  : uiTr("Un-Cut")
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
    }
    Repeater {
        model: (!rootItem.isHistoryView && rootItem.parentView
                && rootItem.deleteCategoryItem) ? 1 : 0
        MenuItem {
            text: uiTr("Paste")
            enabled: parentView && parentView.selectedKey !== ""
            onClicked: {
                var key = parentView.selectedKey
                parentView.selectedKey = ""
                var res = rootItem.model.moveVideo(key, rootItem.categoryIndex)
            }
            icon.source: "/icons/content_paste.svg"
            display: MenuItem.TextBesideIcon
        }
    }
    Repeater {
        model: ((rootItem.deleteVideoItem || !rootItem.parentView)
                && rootItem.extWorkingDirExists
                && rootItem.model.hasWorkingDir(
                    rootItem.key,
                    rootItem.extWorkingDirPath)) ? 1 : 0
        MenuItem {
            text: uiTr("Open containing folder")
            onClicked: {
                rootItem.model.openInBrowser(
                            rootItem.key,
                            rootItem.extWorkingDirPath)
            }
            icon.source: "/icons/open_in_browser.svg"
            display: MenuItem.TextBesideIcon
        }
    }
    Repeater {
        model: ((rootItem.deleteVideoItem || !rootItem.parentView)
                && rootItem.extWorkingDirExists
                && rootItem.model.hasWorkingDir(
                    rootItem.key,
                    rootItem.extWorkingDirPath)) ? 1 : 0
        MenuItem {
            text: uiTr("Delete storage data")
            onClicked: {
                rootItem.model.deleteStorage(
                            rootItem.key,
                            rootItem.extWorkingDirPath)
            }
            icon.source: "/icons/delete_forever.svg"
            display: MenuItem.TextBesideIcon
        }
    }
    // ToDo: add Menu for tagging
    Menu {
        id: extAppMenu
        title: uiTr("Launch in")
        icon.source: "/icons/function.svg"
        enabled: rootItem.extCommandEnabled

        Repeater {
            model: (extAppMenu.enabled) ? rootItem.externalCommands : undefined
            MenuItem {
                text: (rootItem.externalCommands[index])
                        ? rootItem.externalCommands[index].name
                        : ""
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
