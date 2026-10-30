#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
plugin_dir="${XDG_CONFIG_HOME:-$HOME/.config}/omarchy/plugins/traymd-omarchy"
source_id=$(printf '%s' "$project_dir" | sha256sum | cut -c1-12)
build_dir="${XDG_CACHE_HOME:-$HOME/.cache}/traymd-omarchy/build-$source_id"

command -v omarchy >/dev/null || { echo "Omarchy is required." >&2; exit 1; }
install_deps=true
reset_settings=false
for argument in "$@"; do
    case "$argument" in
        --no-deps) install_deps=false ;;
        --reset-settings) reset_settings=true ;;
        --help|-h) echo "Usage: ./install.sh [--no-deps] [--reset-settings]"; exit 0 ;;
        *) echo "Unknown option: $argument" >&2; exit 1 ;;
    esac
done
if $install_deps; then
    omarchy pkg add base-devel cmake qt6-base qt6-declarative glib2
fi
cmake -S "$project_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure
omarchy plugin validate "$build_dir/plugin"

# Fully stage the build before touching the installed plugin. Keep an existing
# version outside the watched plugins directory for rollback.
if [[ -e $plugin_dir ]]; then
    backup_dir="${XDG_STATE_HOME:-$HOME/.local/state}/traymd-omarchy/backups/$(date +%Y%m%d_%H%M%S_%N)"
    mkdir -p "$backup_dir"
    cp -a "$plugin_dir" "$backup_dir/plugin"
    echo "Previous plugin backed up to $backup_dir/plugin"
fi
mkdir -p "$plugin_dir"
mkdir -p "$plugin_dir/TrayMDBackend"
# Replace the library inode rather than truncating one the shell may have
# mapped into memory. Truncation could crash a running shell on upgrade.
library_tmp=$(mktemp "$plugin_dir/TrayMDBackend/.backend.XXXXXX")
cp "$build_dir/plugin/TrayMDBackend/libtraymdbackend.so" "$library_tmp"
chmod 755 "$library_tmp"
mv -f "$library_tmp" "$plugin_dir/TrayMDBackend/libtraymdbackend.so"
printf 'module TrayMDBackend\nplugin traymdbackend %s\n' "$plugin_dir/TrayMDBackend" > "$plugin_dir/TrayMDBackend/qmldir"
for file in manifest.json BarWidget.qml NotesPanel.qml NotesPopup.qml PopupSettings.qml NoteButton.qml EditorMenu.qml EditorMenuItem.qml LICENSE; do
    file_tmp=$(mktemp "$plugin_dir/.install.XXXXXX")
    cp "$build_dir/plugin/$file" "$file_tmp"
    chmod 644 "$file_tmp"
    mv -f "$file_tmp" "$plugin_dir/$file"
done
omarchy-shell shell rescanPlugins
omarchy plugin enable traymd-omarchy --section right
if $reset_settings; then python3 "$project_dir/scripts/reset-settings.py"; fi
echo "Restarting the Omarchy shell to load the installed plugin…"
omarchy restart shell
echo "TrayMD Omarchy is enabled. Click the notes icon in the bar."
echo "Toggle with: omarchy-shell shell toggle traymd-omarchy"
