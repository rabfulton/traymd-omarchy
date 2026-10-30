#!/usr/bin/env python3
"""Replace a non-git development install using Omarchy's normal plugin flow."""
import datetime
import json
import os
from pathlib import Path
import shutil
import subprocess

PLUGIN = "traymd-omarchy"
REPOSITORY = "https://github.com/rabfulton/traymd-omarchy.git"


def run(*args):
    subprocess.run(args, check=True)


def find_entry(config):
    for section, entries in config.get("bar", {}).get("layout", {}).items():
        for index, entry in enumerate(entries):
            if isinstance(entry, dict) and entry.get("id") == PLUGIN:
                return section, index, entry
    return None, None, {}


def main():
    # Omarchy's plugin CLI uses ~/.config, independently of XDG_CONFIG_HOME.
    config_dir = Path.home() / ".config/omarchy"
    installed = config_dir / "plugins" / PLUGIN
    if (installed / ".git").exists():
        print("Already Git-managed. Update with: omarchy plugin update traymd-omarchy")
        return
    config_file = config_dir / "shell.json"
    config = json.loads(config_file.read_text()) if config_file.exists() else {}
    _, _, entry = find_entry(config)
    state = Path(os.environ.get("XDG_STATE_HOME", str(Path.home() / ".local/state")))
    backup = state / PLUGIN / "migration" / datetime.datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    backup.mkdir(parents=True)
    if config_file.exists():
        shutil.copy2(config_file, backup / "shell.json")
    (backup / "widget-settings.json").write_text(json.dumps(entry, indent=2) + "\n")
    print(f"Settings backed up to {backup}", flush=True)
    if installed.exists() or installed.is_symlink():
        # Omarchy backs up non-git plugin folders rather than deleting them.
        run("omarchy", "plugin", "remove", PLUGIN, "--yes")
    run("omarchy", "plugin", "add", REPOSITORY, "--enable")
    section, index, _ = find_entry(json.loads(config_file.read_text()))
    # Older published releases still require the explicit native installer.
    # This keeps migration usable before the one-command bootstrap is released.
    legacy = not (installed / "scripts/backend-status.py").exists()
    if legacy:
        run("bash", str(installed / "install.sh"))
        if section is not None:
            run("omarchy", "bar", "move", PLUGIN, "--section", section, "--index", str(index))
    for key, value in entry.items():
        if key != "id":
            run("omarchy", "bar", "set", PLUGIN, key, json.dumps(value), "--json")
    print("Release checkout installed. Notes and app configuration were kept.")
    if not legacy:
        print("Editor setup starts automatically when the plugin loads.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"Release migration stopped: {error}")
