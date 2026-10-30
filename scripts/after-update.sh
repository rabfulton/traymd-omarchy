#!/usr/bin/env bash
# Complete preparation inside the updater, rather than on the next icon click.
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mode=${1:-plugin}
[[ $mode == plugin || $mode == system ]] || { echo "Unknown update mode: $mode" >&2; exit 2; }
cache_dir="${XDG_CACHE_HOME:-$HOME/.cache}/traymd-omarchy"
mkdir -p "$cache_dir"
exec 9>"$cache_dir/setup.lock"
flock 9

prepare() {
    if python3 "$project_dir/scripts/backend-status.py"; then
        # Git-only QML changes also need a fresh component cache.
        if [[ $mode == plugin && -z ${OMARCHY_UPDATE_LOGGED:-} ]]; then omarchy restart shell; fi
        return
    fi
    echo "Preparing TrayMD editor…"
    local options=(--no-deps)
    if [[ $mode == system || -n ${OMARCHY_UPDATE_LOGGED:-} ]]; then options+=(--no-restart); fi
    bash "$project_dir/install.sh" "${options[@]}"
}

output=/dev/stdout
# Omarchy redirects git merge's output. Keep builds in the existing terminal
# when there is one, and always retain the complete log for unattended runs.
if [[ $mode == plugin && -t 0 ]] && ( : >/dev/tty ) 2>/dev/null; then output=/dev/tty; fi
if prepare 2>&1 | tee "$cache_dir/update.log" >"$output"; then exit 0; fi
echo "TrayMD preparation failed. See $cache_dir/update.log" >&2
if command -v notify-send >/dev/null; then
    notify-send --urgency=critical "TrayMD update failed" "See $cache_dir/update.log" || true
fi
exit 1
