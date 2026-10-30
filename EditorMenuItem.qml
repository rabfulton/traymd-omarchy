import QtQuick
import QtQuick.Controls
import qs.Commons
import qs.Commons as Commons

MenuItem {
    id: root
    property string hint: ""
    implicitHeight: Math.max(30, Style.space(26))
    implicitWidth: 240
    font.family: Style.font.family
    font.pixelSize: Style.font.body
    background: Rectangle {
        color: root.highlighted ? Style.hoverFillFor(Commons.Color.popups.text, Commons.Color.accent) : "transparent"
        radius: Style.cornerRadius
    }
    contentItem: Item {
        Text {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            font: root.font
            color: root.highlighted ? Commons.Color.accent : Commons.Color.popups.text
            opacity: root.enabled ? 1 : 0.35
        }
        Text {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: root.hint
            font: root.font
            color: Commons.Color.popups.text
            opacity: root.enabled ? 0.5 : 0.25
        }
    }
}
