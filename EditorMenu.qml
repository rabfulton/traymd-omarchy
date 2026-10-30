import QtQuick
import QtQuick.Controls
import qs.Commons
import qs.Commons as Commons
import qs.Ui

Menu {
    id: root
    required property var editor
    popupType: Popup.Item
    padding: Style.space(5)
    width: Math.min(root.editor.width, Math.max(220, Style.font.body * 17))
    background: BorderSurface {
        color: Commons.Color.popups.background
        borderSpec: Border.flat(Commons.Color.popups.border, Style.normalBorderWidth)
        radius: Style.cornerRadius
    }
    EditorMenuItem { text: "Undo"; hint: "Ctrl+Z"; enabled: root.editor.canUndo; onTriggered: root.editor.undo() }
    EditorMenuItem { text: "Redo"; hint: "Ctrl+Shift+Z"; enabled: root.editor.canRedo; onTriggered: root.editor.redo() }
    MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Commons.Color.popups.border; opacity: 0.3 } }
    EditorMenuItem { text: "Cut"; hint: "Ctrl+X"; enabled: root.editor.selectedText !== ""; onTriggered: root.editor.cut() }
    EditorMenuItem { text: "Copy"; hint: "Ctrl+C"; enabled: root.editor.selectedText !== ""; onTriggered: root.editor.copy() }
    EditorMenuItem { text: "Paste"; hint: "Ctrl+V"; enabled: root.editor.canPaste; onTriggered: root.editor.paste() }
    EditorMenuItem { text: "Select all"; hint: "Ctrl+A"; enabled: root.editor.length > 0; onTriggered: root.editor.selectAll() }
}
