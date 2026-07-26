import QtQuick
import QtQuick.Controls

/*
   Recursive bookmarks-folder picker for the "Add to..." menus.

   One level is built when the menu is about to show and torn down when it closes (A1):
   no cache, therefore no invalidation when categories are created, renamed or deleted
   elsewhere. Only branches the user actually hovers are ever built.

   Submenus are created at runtime via Qt.createComponent() rather than by declaring this
   type inside itself, which would be a cyclic type reference.
*/
Menu {
    id: folderMenu

    // Absolute path of the folder this menu represents.
    property string folderPath: ""
    property string folderName: ""
    // Shows the "Add here" entry. Turned off at the top level, where the caller already
    // offers the bookmarks root as "/".
    property bool selfSelectable: true
    // Invoked with the absolute path of the chosen folder. Passed down to every submenu.
    property var pick: function(path) {}

    title: folderName

    property var _submenus: []
    property Component _selfComponent: null

    onAboutToShow: folderMenu._populate()
    // Deferred: the close cascade destroys submenus while their own close handlers may
    // still be on the stack.
    onClosed: Qt.callLater(folderMenu._clear)

    function _populate() {
        if (_submenus.length > 0)
            return
        if (_selfComponent === null)
            _selfComponent = Qt.createComponent('DestinationFolderMenu.qml')
        if (_selfComponent.status !== Component.Ready)
            return
        var subs = fileSystemModel.subFolders(folderPath)
        var created = []
        for (var i = 0; i < subs.length; ++i) {
            var m = _selfComponent.createObject(folderMenu,
                                                {
                                                    folderPath: subs[i].path,
                                                    folderName: subs[i].name,
                                                    pick: folderMenu.pick
                                                })
            if (m === null)
                continue
            folderMenu.addMenu(m)
            created.push(m)
        }
        _submenus = created
    }

    function _clear() {
        if (folderMenu.opened)
            return
        for (var i = 0; i < _submenus.length; ++i)
            folderMenu.removeMenu(_submenus[i]) // removeMenu() also destroys it
        _submenus = []
    }

    MenuItem {
        text: uiTr("Add here")
        icon.source: "/icons/add.svg"
        visible: folderMenu.selfSelectable
        height: visible ? implicitHeight : 0
        onTriggered: folderMenu.pick(folderMenu.folderPath)
    }
    MenuSeparator {
        visible: folderMenu.selfSelectable
        height: visible ? implicitHeight : 0
    }
}
