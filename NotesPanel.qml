import QtQuick
import QtQuick.Controls
import qs.Commons
import qs.Commons as Commons
import qs.Ui
import "TrayMDBackend" as Backend

NotesPopup {
    id: root
    property real preferredWidth: 0
    property real preferredHeight: 0
    property int editorFontSize: Style.font.heading
    property bool settingsOpen: false
    property bool deletePending: false
    property var openExternalUrl: function(url) { return Qt.openUrlExternally(url) }
    signal settingsEdited(var values)
    readonly property alias textEditor: editor
    readonly property alias markdown: renderer
    readonly property alias editMenu: editorMenu
    readonly property alias noteToolbar: tools
    readonly property alias decorationItems: decorationRepeater
    readonly property color foreground: Commons.Color.popups.text
    readonly property color secondary: Qt.rgba(foreground.r, foreground.g, foreground.b, 0.55)
    // QText backgrounds use an opaque blend for consistent Qt Quick rendering.
    readonly property color codeFill: Backend.Theme.colors.lighter_background || Qt.rgba(
        Commons.Color.popups.background.r * 0.94 + foreground.r * 0.06,
        Commons.Color.popups.background.g * 0.94 + foreground.g * 0.06,
        Commons.Color.popups.background.b * 0.94 + foreground.b * 0.06, 1)
    readonly property real initialHeight: Math.round((screenH > 0 ? screenH : 1080) * 0.5)
    contentHeight: cappedContentHeight(Math.max(300, preferredHeight || initialHeight))
    contentWidth: fittedContentWidth(Math.max(280, preferredWidth || contentHeight))
    focusTarget: editor
    onOpenChanged: if (!open) { deletePending = false; settingsOpen = false }
    onSettingsOpenChanged: if (settingsOpen) Qt.callLater(function() { if (root.settingsOpen) settingsCard.focusFirst() })
    onDeletePendingChanged: if (deletePending) { deleteDialog.selectedIndex = 0; focusEditor() }

    function focusEditor() { editor.forceActiveFocus() }

    function navigate(delta) {
        Backend.Notes.navigate(delta)
        focusEditor()
    }

    function newNote() {
        settingsOpen = false
        Backend.Notes.newNote()
        focusEditor()
    }

    function toggleTask(position) {
        focusEditor()
        renderer.toggleTask(position)
    }

    function shortcutLabel(label, action) {
        const key = Backend.Keybindings.shortcuts[action]
        return label + (key ? " · " + key : "")
    }

    function handleKey(event) {
        event.accepted = false
        if (deletePending) {
            if (deleteDialog.handleKey(event)) event.accepted = true
            return
        }
        if (event.key === Qt.Key_Escape && settingsOpen) {
            settingsOpen = false
            focusEditor()
        } else if (event.key === Qt.Key_Escape || Backend.Keybindings.matches("close", event.key, event.modifiers)) {
            close()
        } else if (Backend.Keybindings.matches("new_note", event.key, event.modifiers)) {
            newNote()
        } else if (Backend.Keybindings.matches("older_note", event.key, event.modifiers)) {
            navigate(-1)
        } else if (Backend.Keybindings.matches("newer_note", event.key, event.modifiers)) {
            navigate(1)
        } else if (Backend.Keybindings.matches("save", event.key, event.modifiers)) {
            Backend.Notes.save()
        } else if (Backend.Keybindings.matches("settings", event.key, event.modifiers)) {
            settingsOpen = !settingsOpen
        } else if (Backend.Keybindings.matches("delete_note", event.key, event.modifiers)) {
            deletePending = true
        } else return
        event.accepted = true
    }

    FocusScope {
        id: content
        anchors.fill: parent
        Keys.priority: Keys.BeforeItem
        Keys.onPressed: function(event) { root.handleKey(event) }

        Item {
            id: header
            anchors.top: parent.top
            width: parent.width
            height: Style.space(40)

            Row {
                id: tools
                anchors.left: parent.left
                spacing: Style.space(3)

                NoteButton {
                    iconText: "‹"
                    tooltipText: root.shortcutLabel("Older note", "older_note")
                    enabled: Backend.Notes.currentIndex > 0
                    onClicked: root.navigate(-1)
                }
                NoteButton {
                    iconText: "›"
                    tooltipText: root.shortcutLabel("Newer note", "newer_note")
                    enabled: Backend.Notes.currentIndex < Backend.Notes.count - 1
                    onClicked: root.navigate(1)
                }
                NoteButton {
                    iconText: "+"
                    tooltipText: root.shortcutLabel("New note", "new_note")
                    onClicked: root.newNote()
                }
            }
            Row {
                anchors.right: parent.right
                spacing: Style.space(3)
                NoteButton {
                    iconText: "⌫"
                    tooltipText: root.shortcutLabel("Delete note", "delete_note")
                    enabled: Backend.Notes.ready
                    onClicked: { root.deletePending = true; root.focusEditor() }
                }
                NoteButton {
                    iconText: "⚙"
                    tooltipText: root.shortcutLabel("Popup settings", "settings")
                    onClicked: root.settingsOpen = !root.settingsOpen
                }
            }
        }

        Rectangle {
            id: divider
            anchors.top: header.bottom
            width: parent.width
            height: 1
            color: Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, 0.12)
        }

        Text {
            id: errorLabel
            anchors.top: divider.bottom
            width: parent.width
            topPadding: visible ? Style.space(10) : 0
            bottomPadding: topPadding
            visible: Backend.Notes.error !== "" || Backend.Keybindings.error !== ""
            text: [Backend.Notes.error, Backend.Keybindings.error].filter(function(message) { return message !== "" }).join("\n")
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: Commons.Color.urgent
            font.family: Style.font.family
            font.pixelSize: Style.font.body
            height: visible ? implicitHeight : 0
        }

        ScrollView {
            id: scroll
            anchors.top: errorLabel.bottom
            anchors.topMargin: Style.space(12)
            anchors.bottom: footer.top
            anchors.bottomMargin: Style.space(12)
            width: parent.width
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            TextArea {
                id: editor
                objectName: "noteEditor"
                width: scroll.availableWidth
                enabled: Backend.Notes.ready
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                persistentSelection: true
                color: root.foreground
                selectionColor: Qt.rgba(Commons.Color.accent.r, Commons.Color.accent.g, Commons.Color.accent.b, 0.30)
                selectedTextColor: root.foreground
                placeholderText: "Jot something down…"
                placeholderTextColor: root.secondary
                font.family: Style.font.family
                font.pixelSize: root.editorFontSize
                leftPadding: Style.space(2)
                rightPadding: Style.space(14)
                topPadding: Style.space(3)
                bottomPadding: Style.space(16)
                background: null
                ContextMenu.menu: EditorMenu {
                    id: editorMenu
                    editor: editor
                }
                Accessible.name: "Note"
                Keys.priority: Keys.BeforeItem
                Keys.onPressed: function(event) {
                    root.handleKey(event)
                    if (event.accepted) return
                    if (root.deletePending || root.settingsOpen) { event.accepted = true; return }
                    if (event.modifiers === Qt.NoModifier) {
                        const pos = renderer.editKey(cursorPosition, selectionStart, selectionEnd, event.key)
                        if (pos >= 0) {
                            cursorPosition = pos
                            event.accepted = true
                        } else if (selectionStart === selectionEnd && (event.key === Qt.Key_Left || event.key === Qt.Key_Right)) {
                            const direction = event.key === Qt.Key_Left ? -1 : 1
                            cursorPosition = renderer.moveCursor(cursorPosition + direction, direction)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Home) {
                            const start = cursorPosition > 0 ? text.lastIndexOf("\n", cursorPosition - 1) + 1 : 0
                            cursorPosition = renderer.moveCursor(start, 0)
                            event.accepted = true
                        }
                    }
                }

                Component.onCompleted: Backend.Notes.attachEditor(textDocument)
                onCursorPositionChanged: {
                    if (!renderer) return
                    if (activeFocus) renderer.cursorPosition = cursorPosition
                    if (selectionStart !== selectionEnd) return
                    const position = renderer.taskContentPosition(cursorPosition)
                    if (position !== cursorPosition) cursorPosition = position
                }

                Connections {
                    target: Backend.Notes
                    function onCurrentChanged() {
                        Backend.Notes.attachEditor(editor.textDocument)
                        editor.cursorPosition = editor.length
                    }
                }

                Backend.MarkdownHighlighter {
                    id: renderer
                    textDocument: editor.textDocument
                    cursorPosition: editor.cursorPosition
                    accent: Commons.Color.accent
                    headingColors: [Backend.Theme.colors.blue || Backend.Theme.colors.color4 || Commons.Color.accent,
                        Backend.Theme.colors.magenta || Backend.Theme.colors.color5 || Commons.Color.accent,
                        Backend.Theme.colors.yellow || Backend.Theme.colors.color3 || Commons.Color.accent]
                    muted: root.secondary
                    codeBackground: root.codeFill
                    fontSize: root.editorFontSize
                }

                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    acceptedModifiers: Qt.ControlModifier
                    // Passive taps coexist with the editor; dragging still
                    // selects text and cancels activation.
                    gesturePolicy: TapHandler.DragThreshold
                    onTapped: function(eventPoint) {
                        let url = renderer.linkAt(editor.positionAt(eventPoint.position.x, eventPoint.position.y))
                        if (url !== "") {
                            if (!/^[a-z][a-z0-9+.-]*:/i.test(url)) url = "https://" + url
                            root.openExternalUrl(url)
                        }
                    }
                }

                Repeater {
                    id: decorationRepeater
                    model: renderer ? renderer.decorations : []
                    delegate: Item {
                        required property var modelData
                        readonly property rect location: {
                            editor.contentHeight; editor.width; editor.text
                            return editor.positionToRectangle(Math.min(editor.length, modelData.position))
                        }
                        readonly property rect endLocation: {
                            editor.contentHeight; editor.width; editor.text
                            return editor.positionToRectangle(Math.min(editor.length, modelData.endPosition === undefined ? modelData.position : modelData.endPosition))
                        }
                        z: modelData.kind === "code" ? -1 : 1
                        x: modelData.kind === "code" ? 0 : location.x
                        y: location.y
                        width: modelData.kind === "rule" || modelData.kind === "code" ? editor.width - x - editor.rightPadding : editor.font.pixelSize
                        height: modelData.kind === "code" ? endLocation.y + endLocation.height - location.y : location.height
                        Rectangle {
                            anchors.fill: parent
                            visible: modelData.kind === "code"
                            color: root.codeFill
                            radius: Style.cornerRadius
                            border.width: 1
                            border.color: Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, 0.12)
                        }
                        Text {
                            visible: modelData.kind === "bullet"
                            text: "•"
                            color: Commons.Color.accent
                            font: editor.font
                        }
                        Rectangle {
                            id: taskBox
                            objectName: "taskCheckbox"
                            visible: modelData.kind === "task"
                            width: Math.max(12, Math.round(editor.font.pixelSize * 0.85))
                            height: width
                            anchors.verticalCenter: parent.verticalCenter
                            radius: 2
                            color: modelData.checked ? Commons.Color.accent : "transparent"
                            border.color: Commons.Color.accent
                            border.width: 1
                            activeFocusOnTab: visible
                            Accessible.role: Accessible.CheckBox
                            Accessible.name: modelData.label || "Task"
                            Accessible.checked: modelData.checked || false
                            Accessible.onPressAction: root.toggleTask(modelData.position)
                            Keys.onSpacePressed: root.toggleTask(modelData.position)
                            Keys.onReturnPressed: root.toggleTask(modelData.position)
                            Text {
                                anchors.centerIn: parent
                                text: "✓"
                                visible: modelData.checked || false
                                color: Commons.Color.popups.background
                                font.family: editor.font.family
                                font.pixelSize: Math.round(taskBox.height * 0.85)
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.toggleTask(modelData.position)
                            }
                        }
                        Rectangle {
                            visible: modelData.kind === "rule"
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            height: 1
                            color: root.secondary
                        }
                    }
                }
            }
        }

        PopupSettings {
            id: settingsCard
            anchors.top: divider.bottom
            anchors.bottom: parent.bottom
            width: parent.width
            visible: root.settingsOpen
            z: 10
            widthValue: root.contentWidth
            heightValue: root.contentHeight
            placementValue: root.placement
            shortcutText: Backend.Shortcut.current
            shortcutError: Backend.Shortcut.error
            onShortcutRequested: function(keys) { Backend.Shortcut.setShortcut(keys) }
            onEdited: function(values) { root.settingsEdited(values) }
            onClosed: { root.settingsOpen = false; root.focusEditor() }
        }

        ConfirmDialog {
            id: deleteDialog
            anchors.fill: parent
            z: 20
            opened: root.deletePending
            message: "Move this note to .trash? It can be recovered from the notes folder."
            confirmText: "Delete"
            selectedIndex: 0
            onCanceled: { root.deletePending = false; root.focusEditor() }
            onConfirmed: {
                root.deletePending = false
                Backend.Notes.deleteCurrent()
                root.focusEditor()
            }
        }

        Item {
            id: footer
            anchors.bottom: parent.bottom
            width: parent.width
            height: Style.space(22)

            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: Backend.Notes.ready
                    ? String(Backend.Notes.currentIndex + 1).padStart(2, "0") + " / " + String(Backend.Notes.count).padStart(2, "0")
                    : "No note open"
                color: root.secondary
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }
            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: Backend.Notes.error !== "" ? "Check save status"
                    : Backend.Notes.dirty ? "Saving…" : "Saved"
                color: Backend.Notes.error !== "" ? Commons.Color.urgent : root.secondary
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }
        }
    }
}
