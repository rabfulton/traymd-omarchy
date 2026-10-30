import QtQuick
import Quickshell
import QtTest 1.2
import qs.Ui
import "Plugin" as Plugin

// Exercise both complete widget replacement and Quickshell engine reload.
ShellRoot {
    property var notes: null
    TestEvent { id: keyEvents }
    Connections {
        target: Quickshell
        function onReloadCompleted() { Quickshell.inhibitReloadPopup() }
    }
    PersistentProperties {
        id: state
        reloadableId: "note-lifecycle"
        property int reloads: 0
        property string original: ""
    }
    Item {
        PluginBarApi {
            id: testBar
            pluginId: "traymd-omarchy"
            moduleName: "traymd-omarchy"
            fontFamily: "monospace"
            barSize: 32
        }
        Loader {
            id: view
            sourceComponent: Component {
                Plugin.NotesWidget {
                    visible: false
                    bar: testBar
                    settings: ({notesDirectory: Quickshell.env("TRAYMD_TEST_NOTES")})
                }
            }
        }
    }
    Timer {
        property int phase: 0
        interval: 150
        repeat: true
        running: true
        onTriggered: {
            if (!notes && view.item) notes = view.item.notes
            if (!notes || !notes.initialize(Quickshell.env("TRAYMD_TEST_NOTES"))) {
                console.error("Lifecycle load failed"); Qt.exit(1); return
            }
            if (state.original === "") state.original = notes.text
            if (state.original === "" || notes.text !== state.original || (state.reloads > 0 && notes.dirty)) {
                console.error("Replacing widgets or reloading the shell changed the note"); Qt.exit(1); return
            }
            if (phase === 0) {
                if (state.reloads === 0) {
                    // A real list edit, still inside the autosave idle period.
                    const panel = view.item.children.find(function(child) { return child.textEditor !== undefined })
                    if (!panel) { console.error("Editor panel missing"); Qt.exit(1); return }
                    panel.textEditor.insert(panel.textEditor.length, "\nPending edit before reload.")
                    if (!notes.dirty) {
                        console.error("Could not create a pending edit for the reload test"); Qt.exit(1); return
                    }
                    state.original = notes.text
                }
                view.active = false; phase = 1; return
            }
            if (phase === 1) { view.active = true; phase = 2; return }
            running = false
            if (state.reloads === 0) {
                state.reloads += 1
                Quickshell.reload(false)
            } else {
                console.log("TRAYMD_LIFECYCLE_OK")
                Qt.quit()
            }
        }
    }
}
