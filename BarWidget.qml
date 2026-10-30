import QtQuick
import Quickshell
import Quickshell.Io
import qs.Ui as Ui

// This entry point intentionally has no native imports. A fresh git install
// can show its icon and finish setup before loading the compiled editor.
Ui.BarWidget {
    id: root
    moduleName: "traymd-omarchy"
    property string pluginDirectory: Quickshell.env("HOME") + "/.config/omarchy/plugins/traymd-omarchy"
    property bool pendingOpen: false
    property bool pendingNewNote: false
    property bool setupLaunching: false
    property var launchSetup: function() {
        Quickshell.execDetached(["omarchy-launch-terminal", "bash", pluginDirectory + "/scripts/first-run.sh"])
    }
    readonly property bool ready: editorLoader.status === Loader.Ready
    readonly property bool opened: ready && editorLoader.item.opened
    implicitWidth: ready ? editorLoader.item.implicitWidth : setupButton.implicitWidth
    implicitHeight: ready ? editorLoader.item.implicitHeight : setupButton.implicitHeight

    function open() {
        pendingOpen = true
        if (!check.running) check.running = true
    }
    function close() {
        pendingOpen = false
        pendingNewNote = false
        if (ready) editorLoader.item.close()
    }
    function toggle() { if (opened) close(); else open() }
    function closeForPopoutSwitch() {}
    function showEditor() {
        pendingOpen = false
        editorLoader.item.open()
        if (pendingNewNote) {
            pendingNewNote = false
            editorLoader.item.notes.newNote()
        }
    }
    function finishSetup() {
        if (setupLaunching) return
        setupLaunching = true
        launchCooldown.restart()
        launchSetup()
    }
    function injectEditor() {
        if (!editorLoader.item) return
        editorLoader.item.bar = root.bar
        editorLoader.item.settings = root.settings
        editorLoader.item.activateFromBar = function(button) {
            if (button === Qt.MiddleButton) { root.pendingNewNote = true; root.open() }
            else root.toggle()
        }
    }
    onBarChanged: injectEditor()
    onSettingsChanged: injectEditor()
    Component.onCompleted: check.running = true

    Process {
        id: check
        command: ["python3", root.pluginDirectory + "/scripts/backend-status.py"]
        onExited: function(code) {
            if (code === 0) {
                if (editorLoader.status === Loader.Error) {
                    if (root.pendingOpen) root.finishSetup()
                    root.pendingOpen = false
                } else if (!editorLoader.active) editorLoader.active = true
                else if (root.pendingOpen && root.ready) {
                    root.showEditor()
                }
            } else {
                editorLoader.active = false
                root.pendingOpen = false
                root.finishSetup()
            }
        }
    }
    Loader {
        id: editorLoader
        anchors.fill: parent
        active: false
        source: Qt.resolvedUrl("NotesWidget.qml")
        onLoaded: {
            root.injectEditor()
            if (root.pendingOpen) {
                root.showEditor()
            }
        }
        onStatusChanged: if (status === Loader.Error) root.pendingOpen = false
    }
    Ui.WidgetButton {
        id: setupButton
        anchors.fill: parent
        visible: !root.ready
        bar: root.bar
        text: "󰎞"
        tooltipText: root.setupLaunching ? "Setting up notes…" : "Notes · click to finish setup"
        onPressed: root.open()
    }
    Timer { id: launchCooldown; interval: 3000; onTriggered: root.setupLaunching = false }
}
