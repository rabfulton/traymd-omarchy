#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
lint_dir="$2/surface-lint"
shell_dir="${OMARCHY_PATH:-/usr/share/omarchy}/shell"
popup_file="${3:-$project_dir/NotesPopup.qml}"
# qmllint may resolve transitive types that aren't in scope at runtime. Keep
# an explicit regression guard for the UI import required by BorderSurface.
grep -Eq '^import qs[.]Ui([[:space:]]|$)' "$popup_file" || {
    echo "Popup must explicitly import qs.Ui for BorderSurface." >&2
    exit 1
}
mkdir -p "$lint_dir/qs"
ln -sfn "$shell_dir/Ui" "$lint_dir/qs/Ui"
ln -sfn "$shell_dir/Commons" "$lint_dir/qs/Commons"
# Bar and theme objects deliberately expose dynamic properties. Keep type and
# import checking enabled on the real surface, which the headless test stubs.
"$1" --missing-property disable --unqualified disable -I "$lint_dir" \
    "$popup_file"
