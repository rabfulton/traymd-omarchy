#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
shell_dir="${OMARCHY_PATH:-/usr/share/omarchy}/shell"
build_dir="${TRAYMD_BUILD_DIR:-$project_dir/build}"
preview_dir="$build_dir/preview"
test_dir=$(mktemp -d /tmp/traymd-omarchy-ui.XXXXXX)
trap 'rm -rf -- "$test_dir"' EXIT
mkdir -p "$preview_dir" "$test_dir/runtime"
chmod 700 "$test_dir/runtime"
ln -sfn "$shell_dir/Commons" "$preview_dir/Commons"
# Use real UI components with only the platform surface stubbed out. This
# checks QML creation, the native module, editing, theme bindings and undo
# without access to a compositor or the user's notes/configuration.
if [[ -L $preview_dir/Ui ]]; then rm -- "$preview_dir/Ui"; fi
mkdir -p "$preview_dir/Ui"
cp -a "$shell_dir/Ui/." "$preview_dir/Ui/"
cp "$project_dir/tests/KeyboardPanelStub.qml" "$preview_dir/Ui/KeyboardPanel.qml"
if [[ -L $preview_dir/Plugin ]]; then unlink "$preview_dir/Plugin"; fi
mkdir -p "$preview_dir/Plugin"
cp -a "$build_dir/plugin/." "$preview_dir/Plugin/"
cp "$project_dir/tests/KeyboardPanelStub.qml" "$preview_dir/Plugin/NotesPopup.qml"
mkdir -p "$preview_dir/Plugin/scripts"
printf '%s\n' 'raise SystemExit(0)' > "$preview_dir/Plugin/scripts/backend-status.py"
mkdir -p "$preview_dir/UnpreparedPlugin/scripts"
cp "$project_dir/BarWidget.qml" "$preview_dir/UnpreparedPlugin/BarWidget.qml"
printf '%s\n' 'raise SystemExit(1)' > "$preview_dir/UnpreparedPlugin/scripts/backend-status.py"
cp "$project_dir/tests/smoke.qml" "$preview_dir/shell.qml"
status=0
env -u WAYLAND_DISPLAY QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME= QT_SCALE_FACTOR=1 \
    QT_QUICK_CONTROLS_STYLE=Basic \
    XDG_RUNTIME_DIR="$test_dir/runtime" XDG_CONFIG_HOME="$test_dir/config" \
    TRAYMD_TEST_NOTES="$test_dir/notes" \
    TRAYMD_TEST_PLUGIN_DIR="$preview_dir/Plugin" \
    TRAYMD_UNPREPARED_PLUGIN_DIR="$preview_dir/UnpreparedPlugin" \
    TRAYMD_DOCS_PREVIEW="${TRAYMD_DOCS_PREVIEW:-0}" \
    TRAYMD_TEST_SCREENSHOT="${TRAYMD_SCREENSHOT_PATH:-$build_dir/preview.png}" \
    timeout 20s quickshell --path "$preview_dir/shell.qml" --no-color \
    > "$build_dir/ui-test.log" 2>&1 || status=$?
cat "$build_dir/ui-test.log"
(( status == 0 )) || exit "$status"
grep -q TRAYMD_SMOKE_OK "$build_dir/ui-test.log"
if grep -Eq 'ReferenceError|TypeError|Binding loop|Unable to assign|Failed to load configuration|ERROR:.*caused by' "$build_dir/ui-test.log"; then
    exit 1
fi
test -s "${TRAYMD_SCREENSHOT_PATH:-$build_dir/preview.png}"
