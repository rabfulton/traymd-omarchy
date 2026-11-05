import QtQuick
import QtQuick.Controls
import qs.Commons
import qs.Commons as Commons

Rectangle {
    id: root
    property int widthValue: 520
    property int heightValue: 540
    property string placementValue: "anchor"
    property var firstField: null
    property string shortcutText: ""
    property string shortcutError: ""
    property bool capturing: false
    signal shortcutRequested(string keys)
    signal edited(var values)
    signal closed()
    color: Commons.Color.popups.background
    function focusFirst() { if (firstField) firstField.forceActiveFocus() }

    ScrollView {
        id: settingsScroll
        anchors.fill: parent
        anchors.margins: Style.space(6)
        contentWidth: availableWidth
        clip: true
        Column {
            width: settingsScroll.availableWidth
            spacing: Style.space(8)
            Text {
                text: "Popup settings"
                color: Commons.Color.popups.text
                font.family: Style.font.family
                font.pixelSize: Style.font.title
            }
            Repeater {
                model: ["Width", "Height"]
                delegate: Row {
                    id: dimension
                    required property string modelData
                    width: parent.width
                    spacing: Style.space(8)
                    Text {
                        width: Style.space(60)
                        anchors.verticalCenter: parent.verticalCenter
                        text: dimension.modelData
                        color: Commons.Color.popups.text
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                    }
                    SpinBox {
                        id: sizeInput
                        width: Math.max(100, dimension.width - Style.space(70))
                        from: dimension.modelData === "Width" ? 280 : 300
                        to: 3000
                        stepSize: 20
                        editable: true
                        value: dimension.modelData === "Width" ? root.widthValue : root.heightValue
                        palette.window: Commons.Color.popups.background
                        palette.base: Commons.Color.background
                        palette.text: Commons.Color.popups.text
                        palette.button: Commons.Color.background
                        palette.buttonText: Commons.Color.popups.text
                        palette.highlight: Commons.Color.accent
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                        onValueModified: root.edited(dimension.modelData === "Width" ? {width: value} : {height: value})
                        Component.onCompleted: if (dimension.modelData === "Width") root.firstField = sizeInput
                    }
                }
            }
            Text {
                text: "Location"
                color: Commons.Color.popups.text
                font.family: Style.font.family
                font.pixelSize: Style.font.body
            }
            ComboBox {
                width: parent.width
                model: ["At bar icon", "Screen centre"]
                currentIndex: root.placementValue === "center" ? 1 : 0
                palette.window: Commons.Color.popups.background
                palette.base: Commons.Color.background
                palette.text: Commons.Color.popups.text
                palette.button: Commons.Color.background
                palette.buttonText: Commons.Color.popups.text
                palette.highlight: Commons.Color.accent
                font.family: Style.font.family
                font.pixelSize: Style.font.body
                onActivated: root.edited({placement: currentIndex === 1 ? "center" : "anchor"})
            }
            Button {
                text: "Reset size"
                palette.button: Commons.Color.background
                palette.buttonText: Commons.Color.popups.text
                font.family: Style.font.family
                font.pixelSize: Style.font.body
                onClicked: root.edited({width: 0, height: 0})
            }
            Text {
                text: "Shortcut"
                color: Commons.Color.popups.text
                font.family: Style.font.family
                font.pixelSize: Style.font.body
            }
            TextField {
                id: shortcutField
                width: parent.width
                readOnly: true
                text: root.capturing ? "Press shortcut…" : root.shortcutText
                placeholderText: "Not set"
                palette.base: Commons.Color.background
                palette.text: Commons.Color.popups.text
                font.family: Style.font.family
                font.pixelSize: Style.font.body
                Keys.priority: Keys.BeforeItem
                Keys.onPressed: function(event) {
                    if (!root.capturing) return
                    event.accepted = true
                    if (event.key === Qt.Key_Escape) { root.capturing = false; return }
                    let key = ""
                    if (event.key >= Qt.Key_A && event.key <= Qt.Key_Z) key = String.fromCharCode(event.key)
                    else if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) key = String.fromCharCode(event.key)
                    else if (event.key >= Qt.Key_F1 && event.key <= Qt.Key_F35) key = "F" + (event.key - Qt.Key_F1 + 1)
                    else if (event.key === Qt.Key_Space) key = "SPACE"
                    if (key === "") return
                    const parts = []
                    if (event.modifiers & Qt.MetaModifier) parts.push("SUPER")
                    if (event.modifiers & Qt.ControlModifier) parts.push("CTRL")
                    if (event.modifiers & Qt.AltModifier) parts.push("ALT")
                    if (event.modifiers & Qt.ShiftModifier) parts.push("SHIFT")
                    parts.push(key)
                    root.capturing = false
                    root.shortcutRequested(parts.join(" + "))
                }
            }
            Row {
                spacing: Style.space(8)
                Button {
                    text: "Set"
                    palette.button: Commons.Color.background
                    palette.buttonText: Commons.Color.popups.text
                    font.family: Style.font.family
                    font.pixelSize: Style.font.body
                    onClicked: { root.capturing = true; shortcutField.forceActiveFocus() }
                }
                Button {
                    text: "Clear"
                    enabled: root.shortcutText !== ""
                    palette.button: Commons.Color.background
                    palette.buttonText: Commons.Color.popups.text
                    font.family: Style.font.family
                    font.pixelSize: Style.font.body
                    onClicked: root.shortcutRequested("")
                }
            }
            Text {
                width: parent.width
                text: root.shortcutError
                visible: text !== ""
                color: Commons.Color.urgent
                font.family: Style.font.family
                font.pixelSize: Style.font.body
                wrapMode: Text.Wrap
            }
            Button {
                text: "Done"
                palette.button: Commons.Color.background
                palette.buttonText: Commons.Color.popups.text
                font.family: Style.font.family
                font.pixelSize: Style.font.body
                onClicked: root.closed()
            }
        }
    }
}
