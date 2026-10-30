import QtQuick
import qs.Commons as Commons

// Only the compositor-facing surface is replaced for headless UI tests.
// Production uses the installed shell's real KeyboardPanel unchanged.
Item {
    id: root
    required property Item anchorItem
    required property QtObject bar
    property var owner: null
    property bool open: false
    property string placement: "anchor"
    property bool popoutSwitchClosing: false
    property real contentWidth: 520
    property real contentHeight: 540
    property Item focusTarget: null
    property real screenH: 1080
    default property alias contentItem: holder.children
    width: contentWidth
    height: contentHeight
    function fittedContentWidth(value) { return value }
    function cappedContentHeight(value) { return value }
    function close() { if (owner) owner.close() }
    Rectangle {
        anchors.fill: parent
        color: Commons.Color.popups.background
        border.color: Commons.Color.popups.border
        border.width: 1
    }
    Item {
        id: holder
        anchors.fill: parent
        anchors.margins: 20
    }
}
