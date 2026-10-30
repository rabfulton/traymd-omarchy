# Development

Build dependencies: `base-devel`, `cmake`, `qt6-base`, `qt6-declarative`, `glib2`.
The Quickshell-based Omarchy shell must be installed.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The installable files are staged in `build/plugin`. `BarWidget.qml` is a QML-only
bootstrap, so a freshly cloned plugin can appear in the bar without a compiled
backend. It checks `scripts/backend-status.py` before loading `NotesWidget.qml`.
The marker fingerprints native sources and the installed Qt version. If a
build is needed, it launches `scripts/first-run.sh` in a terminal automatically.

`./install.sh` installs missing dependencies through `omarchy pkg add`, builds
in the user's cache, tests, installs and restarts the shell. It preserves the
Git checkout and the selected bar section. Pass `--no-deps` to skip package
installation, or `--reset-settings` to reset popup appearance with a backup.
Generated native files and `.native-build.json` are ignored by Git.

Release installs use `omarchy plugin add … --enable`; updates use
`omarchy plugin update traymd-omarchy`. Loading the updated widget checks whether
a rebuild is required. There are no hooks into Omarchy's own updater and no prebuilt
library downloads. `scripts/switch-to-release.py` migrates a copied development
install, backing up the shell configuration and preserving note/settings data.

## Editor

Markdown interpretation and editing are implemented here. Qt's text document
and syntax-highlighter APIs supply layout, rendering, selection and undo.
Source Markdown remains in the document; there is no HTML/Markdown export
round trip. Fenced code backgrounds are drawn by QML, and code fonts use the
system monospace family. `backend/code_highlight.c` and its header come from
XFCE TrayMD and provide the C/Java/Python lexer.

Each `NoteDocument` owns a QTextDocument and an immutable file path. TextAreas
borrow that document through `QQuickTextDocument::setTextDocument`; there is no
writable note-text property or QML buffer copy-back. The document also owns its
single Markdown highlighter, undo history and 1.5-second autosave timer. Only
content edits make it dirty; formatting and creating/destroying views do not.
The note store watches the directory and files for additions, replacements and
removals. Clean documents reload from disk. A simultaneous external write while
there are local edits reports a conflict and leaves both contents intact.

`NotesPopup.qml` adapts Omarchy's KeyboardPanel for optional screen-centred
placement. The card-sized overlay stays open on focus loss and does not
intercept pointer input outside the card. Its original source and license are from
[Omarchy](https://github.com/basecamp/omarchy). Packaged shell files are never
modified.

Global shortcuts use a managed block in `~/.config/hypr/bindings.lua`.
The settings dialog checks for conflicts, backs up the file, reloads Hyprland
and checks config errors. If validation fails, the previous file is restored.

## Validation and screenshot

Backend tests cover storage, rendering, navigation, deletion and theme changes.
The headless UI test sends real Qt keyboard events but substitutes the
compositor-facing surface. A separate qmllint check validates imports and
types on the production popup. Focus, placement and outside-click dismissal
still need a live desktop check.
The UI suite also replaces whole widgets and editor instances, reloads the
Quickshell engine, checks deliberate deletion and menu edits, and reopens notes
in a fresh process. Saved bytes and modification times must remain unchanged
when merely reopening or closing the app.
Link click tests replace the external URL opener with a callback, checking
targets and text selection without launching a browser.

After building, regenerate the README image with:

```bash
./scripts/screenshot.sh
```

This renders the actual editor with sample notes in an isolated Qt session,
using a simple frame instead of a compositor surface. It does not access your
notes or change the running desktop. The image is written to
`assets/screenshot.png`.
