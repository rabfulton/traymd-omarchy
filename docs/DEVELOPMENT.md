# Development

Build dependencies: `base-devel`, `cmake`, `qt6-base`, `qt6-declarative`, `glib2`.
The Quickshell-based Omarchy shell must be installed.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The installable files are staged in `build/plugin`. `./install.sh` installs
missing dependencies through `omarchy pkg add`, builds in the user's cache,
tests, installs the files and enables the plugin through Omarchy. Pass
`--no-deps` to skip package installation. It preserves an installed git checkout
so `omarchy plugin update` can manage the source. Rerun the installer after an
update; the installer restarts the shell to reload the native library.
Use `./install.sh --reset-settings` to clear only saved popup appearance
preferences (with a backup), keeping notes and other shell settings.

Omarchy's plugin-add command clones plugins; it does not compile native
backends. The short installation sequence in the README deliberately adds
the plugin disabled, builds it, then enables it. No prebuilt library is shipped.

## Editor

Markdown interpretation and editing are implemented here. Qt's text document
and syntax-highlighter APIs supply layout, rendering, selection and undo.
Source Markdown remains in the document; there is no HTML/Markdown export
round trip. Fenced code backgrounds are drawn by QML, and code fonts use the
system monospace family. `backend/code_highlight.c` and its header come from
XFCE TrayMD and provide the C/Java/Python lexer.

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

After building, regenerate the README image with:

```bash
./scripts/screenshot.sh
```

This renders the actual editor with sample notes in an isolated Qt session,
using a simple frame instead of a compositor surface. It does not access your
notes or change the running desktop. The image is written to
`assets/screenshot.png`.
