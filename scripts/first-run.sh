#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cache_dir="${XDG_CACHE_HOME:-$HOME/.cache}/traymd-omarchy"
mkdir -p "$cache_dir"
exec 9>"$cache_dir/setup.lock"
flock -n 9 || { echo "TrayMD setup is already running."; exit 0; }
if ! bash "$project_dir/install.sh"; then
    echo "TrayMD setup failed. The notes icon can retry setup."
    if [[ -t 0 ]]; then read -r -p "Press Enter to close…"; fi
    exit 1
fi
