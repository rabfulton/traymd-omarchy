#!/usr/bin/env python3
"""Reset only TrayMD's appearance settings; preserve notes and other plugins."""
import datetime
import json
import os
from pathlib import Path
import shutil
import tempfile


def reset_settings(path):
    config = json.loads(path.read_text())
    changed = False
    for entries in config.get("bar", {}).get("layout", {}).values():
        if not isinstance(entries, list):
            continue
        for entry in entries:
            if isinstance(entry, dict) and entry.get("id") == "traymd-omarchy":
                for key in ("width", "height", "fontSize", "placement"):
                    changed |= key in entry
                    entry.pop(key, None)
    if not changed:
        print("TrayMD already uses first-run appearance settings.")
        return
    # Open the replacement before creating a backup, so a denied write leaves
    # the entire configuration directory untouched.
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", dir=path.parent, prefix=".traymd-reset-", delete=False) as output:
            temporary = Path(output.name)
            os.fchmod(output.fileno(), path.stat().st_mode & 0o777)
            json.dump(config, output, indent=2)
            output.write("\n")
            output.flush()
            os.fsync(output.fileno())
        backup = path.with_name(path.name + ".traymd-" + datetime.datetime.now().strftime("%Y%m%d_%H%M%S_%f") + ".bak")
        shutil.copy2(path, backup)
        os.replace(temporary, path)
        print(f"TrayMD appearance reset. Backup: {backup}")
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


if __name__ == "__main__":
    path = Path(os.environ.get("XDG_CONFIG_HOME", str(Path.home() / ".config"))) / "omarchy/shell.json"
    try:
        reset_settings(path)
    except (OSError, ValueError) as error:
        raise SystemExit(f"Cannot reset TrayMD settings: {error}")
