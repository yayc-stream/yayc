import QtQuick
import QtQuick.Controls

/*
   The "recent destinations" slice of an "Add to..." / "Move to..." menu.

   Rebuilt from scratch in the menu's onAboutToShow, not kept in sync by an Instantiator
   or Repeater bound to the model. Items are inserted at an index looked up from the live
   menu (right before `before`), so the result never depends on how many items precede or
   follow the slice, nor on when the model last changed relative to the menu's own
   construction - which is what made runtime-pushed destinations land after the separator
   or after "Browse...".
*/
QtObject {
    id: section

    // Menu the items are inserted into.
    required property Menu menu
    // Item the slice is inserted in front of - the separator ahead of "Browse...".
    // Null appends at the end.
    property Item before: null
    // Destination list: entries with .name and .path.
    property var destinations: []
    // Invoked with the absolute path of the chosen destination.
    property var pick: function(path) {}

    property var _items: []

    function rebuild() {
        for (var i = 0; i < _items.length; ++i)
            section.menu.removeItem(_items[i]) // removeItem() also destroys it
        _items = []

        var dests = section.destinations
        if (!dests)
            return
        var at = _indexOf(section.before)
        var created = []
        for (var j = 0; j < dests.length; ++j) {
            var item = _itemComponent.createObject(section.menu, { destination: dests[j] })
            if (item === null)
                continue
            section.menu.insertItem(at + created.length, item)
            created.push(item)
        }
        _items = created
    }

    function _indexOf(item) {
        if (item === null)
            return section.menu.count
        for (var i = 0; i < section.menu.count; ++i)
            if (section.menu.itemAt(i) === item)
                return i
        return section.menu.count
    }

    property Component _itemComponent: Component {
        MenuItem {
            required property var destination
            text: destination.name
            onTriggered: section.pick(destination.path)
        }
    }
}
