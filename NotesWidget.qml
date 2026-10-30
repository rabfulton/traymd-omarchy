import QtQuick
import qs.Commons
import qs.Commons as Commons
import qs.Ui
import "TrayMDBackend" as Backend

Panel {
    id: root
    moduleName: "traymd-omarchy"
    // The shell's generic summon/toggle route selects the focused monitor.
    manageIpc: false
    readonly property var notes: Backend.Notes
    property var activateFromBar: null
    implicitWidth: button.implicitWidth
    implicitHeight: button.implicitHeight

    function open() {
        Backend.Notes.initialize(setting("notesDirectory", ""))
        controller.show()
    }

    function close() {
        // Keep unsaved text visible when a write fails.
        if (Backend.Notes.save()) controller.hide()
    }

    function closeForPopoutSwitch() {}

    function updateSettings(values) {
        const entry = Object.assign({}, root.settings, values, { id: root.moduleName })
        root.settings = entry
        if (bar && bar.shell) bar.shell.updateEntryInline(root.moduleName, entry)
    }

    WidgetButton {
        id: button
        anchors.fill: parent
        bar: root.bar
        text: "󰎞"
        tooltipText: "Notes"
        active: root.opened
        onPressed: function(mouseButton) {
            if (root.activateFromBar) { root.activateFromBar(mouseButton); return }
            if (mouseButton === Qt.MiddleButton) {
                root.open()
                Backend.Notes.newNote()
            } else root.toggle()
        }
    }

    NotesPanel {
        anchorItem: button
        bar: root.bar
        owner: root
        open: root.opened
        popoutSwitchClosing: root.popoutSwitchClosing
        preferredWidth: Number(root.setting("width", 0))
        preferredHeight: Number(root.setting("height", 0))
        editorFontSize: Number(root.setting("fontSize", Style.font.heading))
        placement: String(root.setting("placement", "anchor"))
        onSettingsEdited: function(values) { root.updateSettings(values) }
    }
}
