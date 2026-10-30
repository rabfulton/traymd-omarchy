import QtQuick
import Quickshell
import qs.Ui
import "Plugin" as Plugin

// A fresh process must read the saved note without rewriting it on shutdown.
ShellRoot {
    Item {
        PluginBarApi {
            id: testBar
            pluginId: "traymd-omarchy"
            moduleName: "traymd-omarchy"
            fontFamily: "monospace"
            barSize: 32
        }
        Plugin.NotesWidget {
            id: widget
            visible: false
            bar: testBar
            settings: ({notesDirectory: Quickshell.env("TRAYMD_TEST_NOTES")})
        }
    }
    Timer {
        interval: 100
        running: true
        onTriggered: {
            if (!widget.notes.initialize(Quickshell.env("TRAYMD_TEST_NOTES")) || !widget.notes.text.startsWith("# ")) {
                console.error("Saved note did not survive process restart")
                Qt.exit(1)
                return
            }
            console.log("TRAYMD_REOPEN_OK")
            Qt.quit()
        }
    }
}
