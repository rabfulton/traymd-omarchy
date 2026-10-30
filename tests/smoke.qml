import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtTest 1.2
import Quickshell
import qs.Commons as Commons
import qs.Ui
import "Plugin" as Plugin
import "UnpreparedPlugin" as Unprepared

// Run from build/preview with the installed shell's Commons and Ui modules.
// No visible windows or changes to the running desktop are needed.
ShellRoot {
    property var openedLinks: []
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

        Plugin.NotesWidget {
            id: widget
            bar: testBar
            visible: false
            settings: ({ notesDirectory: Quickshell.env("TRAYMD_TEST_NOTES") })
        }

        Loader {
            id: bootstrap
            visible: false
            sourceComponent: Component {
                Plugin.BarWidget {
                    bar: testBar
                    pluginDirectory: Quickshell.env("TRAYMD_TEST_PLUGIN_DIR")
                    settings: ({ notesDirectory: Quickshell.env("TRAYMD_TEST_NOTES") })
                }
            }
        }
        Unprepared.BarWidget {
            id: unprepared
            visible: false
            bar: testBar
            pluginDirectory: Quickshell.env("TRAYMD_UNPREPARED_PLUGIN_DIR")
        }

        Loader {
            id: reloadProbe
            active: false
            visible: false
            sourceComponent: Component {
                Plugin.NotesPanel {
                    anchorItem: widget
                    bar: testBar
                    owner: widget
                    open: false
                }
            }
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
            openExternalUrl: function(url) { openedLinks.push(url); return true }
            onSettingsEdited: function(values) { widget.updateSettings(values) }
        }
    }

    Timer {
        property int phase: 0
        property bool busy: false
        interval: 150
        repeat: true
        running: true
        onTriggered: {
            if (busy) return
            busy = true
            try {
            if (!widget.notes.initialize(Quickshell.env("TRAYMD_TEST_NOTES"))) {
                console.error("Smoke test failed:", widget.notes.error)
                Qt.exit(1)
                return
            }
            if (phase === 0) {
                if (!bootstrap.item || !bootstrap.item.ready || bootstrap.item.implicitWidth <= 0) {
                    console.error("Bootstrap did not load the prepared native widget"); Qt.exit(1); return
                }
                if (unprepared.ready || unprepared.implicitWidth <= 0 || unprepared.opened) {
                    console.error("Fresh install did not show a setup icon safely"); Qt.exit(1); return
                }
                bootstrap.active = false
                if (panel.contentHeight !== 540 || panel.contentWidth !== 540) {
                    console.error("First-run monitor proportions failed", panel.contentWidth, panel.contentHeight)
                    Qt.exit(1); return
                }
                panel.screenH = 1440
                if (panel.contentHeight !== 720 || panel.contentWidth !== 720) {
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
                for (const ch of "delete this test note") keyEvents.keyClickChar(ch, Qt.NoModifier, 0)
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
                widget.notes.text = "- [ ] Pack a notebook"
                panel.textEditor.cursorPosition = panel.textEditor.length
                phase = 4
                return
            }
            if (phase === 4) {
                const item = panel.decorationItems.itemAt(0)
                let checkbox = null
                if (item) for (const child of item.children) if (child.objectName === "taskCheckbox") checkbox = child
                if (!checkbox || !checkbox.visible) { console.error("Task checkbox missing"); Qt.exit(1); return }
                const cursor = panel.textEditor.cursorPosition
                keyEvents.mouseClick(checkbox, checkbox.width / 2, checkbox.height / 2, Qt.LeftButton, Qt.NoModifier, 0)
                if (widget.notes.text !== "- [x] Pack a notebook" || panel.textEditor.cursorPosition !== cursor) {
                    console.error("Checkbox click did not update source while preserving cursor", widget.notes.text)
                    Qt.exit(1); return
                }
                keyEvents.keyClick(Qt.Key_Z, Qt.ControlModifier, 0)
                if (widget.notes.text !== "- [ ] Pack a notebook") { console.error("Task toggle undo failed"); Qt.exit(1); return }
                keyEvents.keyClick(Qt.Key_Z, Qt.ControlModifier | Qt.ShiftModifier, 0)
                if (widget.notes.text !== "- [x] Pack a notebook") { console.error("Task toggle redo failed"); Qt.exit(1); return }
                panel.textEditor.cursorPosition = panel.textEditor.length
                keyEvents.keyClick(Qt.Key_Return, Qt.NoModifier, 0)
                if (widget.notes.text !== "- [x] Pack a notebook\n- [ ] ") { console.error("Task continuation failed"); Qt.exit(1); return }
                keyEvents.keyClick(Qt.Key_Return, Qt.NoModifier, 0)
                if (widget.notes.text !== "- [x] Pack a notebook\n") { console.error("Empty task exit failed"); Qt.exit(1); return }
                if (!widget.notes.save()) { console.error("Saving tasks failed"); Qt.exit(1); return }
                widget.notes.text = "Reload must preserve this note."
                widget.notes.save()
                reloadProbe.active = true
                phase = 5
                return
            }
            if (phase === 5) {
                if (!reloadProbe.item || widget.notes.text !== "Reload must preserve this note.") {
                    console.error("Creating an editor overwrote saved note contents"); Qt.exit(1); return
                }
                reloadProbe.active = false
                phase = 6
                return
            }
            if (phase === 6) {
                if (widget.notes.text !== "Reload must preserve this note.") {
                    console.error("Unloading an editor overwrote saved note contents"); Qt.exit(1); return
                }
                reloadProbe.active = true
                phase = 7
                return
            }
            if (phase === 7) {
                reloadProbe.item.textEditor.clear()
                if (widget.notes.text !== "Reload must preserve this note.") {
                    console.error("Programmatic editor reset overwrote note contents"); Qt.exit(1); return
                }
                reloadProbe.active = false
                const original = widget.notes.text
                panel.textEditor.forceActiveFocus()
                keyEvents.keyClick(Qt.Key_A, Qt.ControlModifier, 0)
                keyEvents.keyClick(Qt.Key_Delete, Qt.NoModifier, 0)
                if (widget.notes.text !== "") { console.error("User deletion was not saved to the model"); Qt.exit(1); return }
                keyEvents.keyClick(Qt.Key_Z, Qt.ControlModifier, 0)
                if (widget.notes.text !== original) { console.error("Undo of user deletion failed"); Qt.exit(1); return }
                panel.textEditor.select(0, 6)
                panel.editMenu.itemAt(3).triggered() // Cut
                if (widget.notes.text !== original.slice(6)) { console.error("Menu cut did not update the model"); Qt.exit(1); return }
                panel.editMenu.itemAt(5).triggered() // Paste
                if (widget.notes.text !== original) { console.error("Menu paste did not update the model"); Qt.exit(1); return }
                widget.notes.text = "Before [Example link](https://example.com/path) after\nhttps://example.org/plain\nwww.example.net\n`https://example.com/code`\n```\nhttps://example.com/fenced\n```"
                panel.textEditor.cursorPosition = 0
                phase = 8
                return
            }
            if (phase === 8) {
                const editor = panel.textEditor
                const source = widget.notes.text
                function clickAt(text, modifiers) {
                    const rect = editor.positionToRectangle(source.indexOf(text) + 2)
                    keyEvents.mouseClick(editor, rect.x + 2, rect.y + rect.height / 2, Qt.LeftButton, modifiers, 0)
                }
                clickAt("Example link", Qt.ControlModifier)
                if (openedLinks.length !== 1 || openedLinks[0] !== "https://example.com/path") {
                    console.error("Ctrl+click did not open the Markdown link", JSON.stringify(openedLinks)); Qt.exit(1); return
                }
                clickAt("Example link", Qt.NoModifier)
                if (openedLinks.length !== 1 || editor.cursorPosition < source.indexOf("Example link") || editor.cursorPosition >= source.indexOf("Example link") + "Example link".length) {
                    console.error("Ordinary clicks must place the cursor for editing without opening links"); Qt.exit(1); return
                }
                clickAt("https://example.org/plain", Qt.NoModifier)
                clickAt("www.example.net", Qt.NoModifier)
                if (openedLinks.length !== 1) { console.error("Ordinary URL clicks opened a browser"); Qt.exit(1); return }
                clickAt("https://example.org/plain", Qt.ControlModifier)
                clickAt("www.example.net", Qt.ControlModifier)
                if (openedLinks.length !== 3 || openedLinks[1] !== "https://example.org/plain" || openedLinks[2] !== "https://www.example.net") {
                    console.error("Ctrl+click on plain URLs did not open their targets", JSON.stringify(openedLinks)); Qt.exit(1); return
                }
                clickAt("Before", Qt.ControlModifier)
                clickAt("https://example.com/code", Qt.ControlModifier)
                clickAt("https://example.com/fenced", Qt.ControlModifier)
                if (openedLinks.length !== 3) { console.error("Non-link text opened a URL"); Qt.exit(1); return }
                const start = editor.positionToRectangle(source.indexOf("Example link") + 1)
                const end = editor.positionToRectangle(source.indexOf("Example link") + 10)
                keyEvents.mousePress(editor, start.x, start.y + start.height / 2, Qt.LeftButton, Qt.NoModifier, 0)
                keyEvents.mouseMove(editor, end.x, end.y + end.height / 2, 0, Qt.LeftButton, Qt.NoModifier)
                keyEvents.mouseRelease(editor, end.x, end.y + end.height / 2, Qt.LeftButton, Qt.NoModifier, 0)
                if (openedLinks.length !== 3 || editor.selectedText === "" || widget.notes.text !== source) {
                    console.error("Dragging a link must select text without opening or editing it"); Qt.exit(1); return
                }
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
                ? "# A few things to remember\n## This week\n\nMake room for **small ideas** and *good coffee*.\n\n- [x] Book the train for Friday\n- [ ] Pick up a notebook on the way home\n\n### Keep this handy\n```python\ndef slow_morning():\n    return \"coffee, then everything else\"\n```\n\n[Explore the Omarchy manual](https://learn.omarchy.org)\n\n*Alt+← / Alt+→ to browse notes*"
                : "# Heading\n## Subheading\n### Smaller heading\n\n**Bold**, *italic* and ***both***.\n\n- One quick thought\n- Another thing to remember\n\n```python\ndef remember():\n    return \"A quick note\"\n```\n\n[Example link](https://example.com)"
            widget.notes.text = source
            if (panel.textEditor.text !== source) {
                console.error("Editor did not load the note")
                Qt.exit(1)
                return
            }
            panel.textEditor.cursorPosition = panel.textEditor.length
            panel.textEditor.forceActiveFocus()
            keyEvents.keyClick(Qt.Key_Return, Qt.NoModifier, 0)
            keyEvents.keyClick(Qt.Key_Return, Qt.NoModifier, 0)
            for (const ch of "Saved automatically.") keyEvents.keyClickChar(ch, Qt.NoModifier, 0)
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
                widget.notes.text = source
                widget.notes.save()
            }
            console.log("TRAYMD_SMOKE_OK", widget.notes.currentName)
            panel.grabToImage(function(result) {
                result.saveToFile(Quickshell.env("TRAYMD_TEST_SCREENSHOT"))
                Qt.quit()
            })
            } finally { busy = false }
        }
    }
}
