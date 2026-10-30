#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$project_dir/assets"
TRAYMD_DOCS_PREVIEW=1 TRAYMD_SCREENSHOT_PATH="$project_dir/assets/screenshot.png" \
    bash "$project_dir/scripts/test-ui.sh"
