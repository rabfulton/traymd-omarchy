import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtTest 1.2
import Quickshell
import qs.Commons as Commons
import qs.Ui
import "Plugin" as Plugin

// Run from build/preview with the installed shell's Commons and Ui modules.
// No visible windows or changes to the running desktop are needed.
ShellRoot {
    Window {
        id: window
        visible: true
        width: 520
        height: 740
        color: Commons.Color.popups.background
        TestEvent { id: keyEvents }
        PluginBarApi {
            id: testBar
            pluginId: "traymd-omarchy"
            moduleName: "traymd-omarchy"
            foreground: Commons.Color.foreground
            barForeground: foreground
            barSize: 32
            fontFamily: "monospace"
        }

        Plugin.BarWidget {
            id: widget
            bar: testBar
            visible: false
            settings: ({ notesDirectory: Quickshell.env("TRAYMD_TEST_NOTES") })
        }

        Plugin.NotesPanel {
            id: panel
            anchorItem: widget
            bar: testBar
            owner: widget
            open: true
            preferredWidth: Number(widget.settings.width || 0)
            preferredHeight: Number(widget.settings.height || 0)
            placement: String(widget.settings.placement || "anchor")
            editorFontSize: 16
            onSettingsEdited: function(values) { widget.updateSettings(values) }
        }
    }

    Timer {
        property int phase: 0
        interval: 150
        repeat: true
        running: true
        onTriggered: {
            if (!widget.notes.initialize(Quickshell.env("TRAYMD_TEST_NOTES"))) {
                console.error("Smoke test failed:", widget.notes.error)
                Qt.exit(1)
                return
            }
            if (phase === 0) {
                if (panel.contentHeight !== 540 || panel.contentWidth !== 432) {
                    console.error("First-run monitor proportions failed", panel.contentWidth, panel.contentHeight)
                    Qt.exit(1); return
                }
                panel.screenH = 1440
                if (panel.contentHeight !== 720 || panel.contentWidth !== 576) {
                    console.error("Monitor resize proportions failed"); Qt.exit(1); return
                }
                panel.screenH = 1080
                const toolbar = panel.noteToolbar
                if (toolbar.x < 0 || toolbar.x + toolbar.width > toolbar.parent.width || toolbar.children.length !== 3) {
                    console.error("Navigation toolbar is outside the header"); Qt.exit(1); return
                }
                const previousCount = widget.notes.count
                const addButton = toolbar.children[2]
                keyEvents.mouseClick(addButton, addButton.width / 2, addButton.height / 2, Qt.LeftButton, Qt.NoModifier, 0)
                if (widget.notes.count !== previousCount + 1) {
                    console.error("Visible add button did not create a note"); Qt.exit(1); return
                }
                const newestName = widget.notes.currentName
                const older = toolbar.children[0]
                keyEvents.mouseClick(older, older.width / 2, older.height / 2, Qt.LeftButton, Qt.NoModifier, 0)
                if (widget.notes.currentName === newestName) { console.error("Older button failed"); Qt.exit(1); return }
                const newer = toolbar.children[1]
                keyEvents.mouseClick(newer, newer.width / 2, newer.height / 2, Qt.LeftButton, Qt.NoModifier, 0)
                if (widget.notes.currentName !== newestName) { console.error("Newer button failed"); Qt.exit(1); return }
                widget.notes.text = ""
                window.requestActivate()
                panel.textEditor.forceActiveFocus()
                for (const ch of "## heading") {
                    if (!keyEvents.keyClickChar(ch, Qt.NoModifier, 0)) {
                        console.error("Could not send typing event")
                        Qt.exit(1)
                        return
                    }
                }
                phase = 1
                return
            }
            if (phase === 1) {
                const editor = panel.textEditor
                if (editor.text !== "## heading" || Math.abs(editor.positionToRectangle(3).x - editor.positionToRectangle(0).x) >= 1) {
                    console.error("Typed heading did not collapse its Markdown prefix", editor.text)
                    Qt.exit(1)
                    return
                }
                keyEvents.keyClick(Qt.Key_Home, Qt.NoModifier, 0)
                if (editor.cursorPosition !== 3) {
                    console.error("Home entered hidden heading markers")
                    Qt.exit(1)
                    return
                }
                keyEvents.keyClick(Qt.Key_Backspace, Qt.NoModifier, 0)
                if (editor.text !== "heading") {
                    console.error("Backspace did not remove heading formatting")
                    Qt.exit(1)
                    return
                }
                keyEvents.keyClick(Qt.Key_Z, Qt.ControlModifier, 0)
                if (editor.text !== "## heading") {
                    console.error("Undo did not restore heading source")
                    Qt.exit(1)
                    return
                }
                widget.notes.text = "- first"
                editor.cursorPosition = editor.length
                keyEvents.keyClick(Qt.Key_Return, Qt.NoModifier, 0)
                if (editor.text !== "- first\n- ") {
                    console.error("Enter did not continue the list", editor.text)
                    Qt.exit(1)
                    return
                }
                keyEvents.keyClick(Qt.Key_Return, Qt.NoModifier, 0)
                if (editor.text !== "- first\n") {
                    console.error("Enter did not exit an empty list")
                    Qt.exit(1)
                    return
                }
                const oldCount = widget.notes.count
                const oldName = widget.notes.currentName
                keyEvents.keyClick(Qt.Key_N, Qt.ControlModifier, 0)
                if (widget.notes.count !== oldCount + 1 || widget.notes.currentIndex !== oldCount) {
                    console.error("Ctrl+N did not create the newest note")
                    Qt.exit(1); return
                }
                editor.insert(0, "delete this test note")
                keyEvents.keyClick(Qt.Key_Left, Qt.AltModifier, 0)
                if (widget.notes.currentName !== oldName) { console.error("Older direction failed"); Qt.exit(1); return }
                keyEvents.keyClick(Qt.Key_Right, Qt.AltModifier, 0)
                keyEvents.keyClick(Qt.Key_Delete, Qt.ControlModifier | Qt.ShiftModifier, 0)
                if (!panel.deletePending) { console.error("Delete prompt missing"); Qt.exit(1); return }
                keyEvents.keyClick(Qt.Key_Escape, Qt.NoModifier, 0)
                if (panel.deletePending || widget.notes.count !== oldCount + 1) { console.error("Delete cancellation failed"); Qt.exit(1); return }
                keyEvents.keyClick(Qt.Key_Delete, Qt.ControlModifier | Qt.ShiftModifier, 0)
                keyEvents.keyClick(Qt.Key_Tab, Qt.NoModifier, 0)
                keyEvents.keyClick(Qt.Key_Return, Qt.NoModifier, 0)
                if (widget.notes.count !== oldCount || panel.deletePending) { console.error("Confirmed deletion failed"); Qt.exit(1); return }
                keyEvents.keyClick(Qt.Key_Comma, Qt.ControlModifier, 0)
                if (!panel.settingsOpen) { console.error("Settings shortcut failed"); Qt.exit(1); return }
                keyEvents.keyClick(Qt.Key_Escape, Qt.NoModifier, 0)
                panel.settingsEdited({width: 680, height: 620, placement: "center"})
                if (panel.contentWidth !== 680 || panel.contentHeight !== 620 || panel.placement !== "center") {
                    console.error("Popup settings did not apply live"); Qt.exit(1); return
                }
                panel.settingsEdited({width: 520, height: 740, placement: "anchor"})
                widget.open()
                keyEvents.keyClick(Qt.Key_W, Qt.ControlModifier, 0)
                if (widget.opened) { console.error("Ctrl+W did not close the popup"); Qt.exit(1); return }
                panel.settingsOpen = true
                phase = 2
                return
            }
            if (phase === 2) {
                keyEvents.keyClick(Qt.Key_Up, Qt.NoModifier, 0)
                if (widget.settings.width !== 540) {
                    console.error("Keyboard adjustment did not persist width", widget.settings.width)
                    Qt.exit(1); return
                }
                keyEvents.keyClick(Qt.Key_Escape, Qt.NoModifier, 0)
                if (panel.settingsOpen) { console.error("Escape did not close settings"); Qt.exit(1); return }
                panel.settingsEdited({width: 520})
                keyEvents.mouseClick(panel.textEditor, 20, 20, Qt.RightButton, Qt.NoModifier, 0)
                phase = 3
                return
            }
            if (phase === 3) {
                if (!panel.editMenu.visible) {
                    console.error("Right-click did not open the custom editor menu"); Qt.exit(1); return
                }
                if (panel.editMenu.popupType !== Popup.Item) {
                    console.error("Editor menu must render inside the app"); Qt.exit(1); return
                }
                panel.editMenu.close()
            }
            running = false
            const docsPreview = Quickshell.env("TRAYMD_DOCS_PREVIEW") === "1"
            if (docsPreview) {
                Commons.Style.fontBaseSize = 14
                panel.settingsEdited({width: 640, height: 560})
                panel.editorFontSize = 17
                window.width = 640
                window.height = 560
            }
            const source = docsPreview
                ? "# A few things to remember\n## This week\n\nMake room for **small ideas** and *good coffee*.\n\n- Book the train for Friday\n- Pick up a notebook on the way home\n\n### Keep this handy\n```python\ndef slow_morning():\n    return \"coffee, then everything else\"\n```\n\n[Explore the Omarchy manual](https://learn.omarchy.org)\n\n*Alt+← / Alt+→ to browse notes*"
                : "# Heading\n## Subheading\n### Smaller heading\n\n**Bold**, *italic* and ***both***.\n\n- One quick thought\n- Another thing to remember\n\n```python\ndef remember():\n    return \"A quick note\"\n```\n\n[Example link](https://example.com)"
            widget.notes.text = source
            if (panel.textEditor.text !== source) {
                console.error("Editor did not load the note")
                Qt.exit(1)
                return
            }
            panel.textEditor.insert(panel.textEditor.length, "\n\nSaved automatically.")
            if (!panel.textEditor.canUndo || widget.notes.text !== panel.textEditor.text) {
                console.error("Editor/store binding or undo failed")
                Qt.exit(1)
                return
            }
            if (!widget.notes.save()) {
                Qt.exit(1)
                return
            }
            Commons.Color.accent = "#e5a463"
            if (!panel.textEditor.canUndo) {
                console.error("Theme change cleared undo history")
                Qt.exit(1)
                return
            }
            if (docsPreview) {
                Commons.Color.accent = "#7aa2f7"
                panel.textEditor.deselect()
                panel.textEditor.focus = false
                panel.textEditor.cursorVisible = false
                panel.textEditor.remove(source.length, panel.textEditor.length)
                widget.notes.save()
            }
            console.log("TRAYMD_SMOKE_OK", widget.notes.currentName)
            panel.grabToImage(function(result) {
                result.saveToFile(Quickshell.env("TRAYMD_TEST_SCREENSHOT"))
                Qt.quit()
            })
        }
    }
}
