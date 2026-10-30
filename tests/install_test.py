"""Installer helper checks without changing the live desktop or using GitHub."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

PROJECT = Path(__file__).resolve().parent.parent


def load_script(name):
    spec = importlib.util.spec_from_file_location(name, PROJECT / "scripts" / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class InstallTests(unittest.TestCase):
    def test_backend_marker_detects_native_source_and_qt_changes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "scripts").mkdir()
            (root / "backend").mkdir()
            (root / "TrayMDBackend").mkdir()
            script = root / "scripts/backend-status.py"
            shutil.copy2(PROJECT / "scripts/backend-status.py", script)
            header = root / "backend/example.h"
            header.write_text("first version")
            (root / "CMakeLists.txt").write_text("project(test)")
            (root / "TrayMDBackend/qmldir").write_text("plugin test")
            library = root / "library.so"
            library.write_bytes(b"test-library")
            commands = root / "commands"
            commands.mkdir()
            pkgconfig = commands / "pkg-config"
            pkgconfig.write_text('#!/bin/sh\ncat "$(dirname "$0")/qt-version"\n')
            pkgconfig.chmod(0o755)
            version = commands / "qt-version"
            version.write_text("6.12.0")
            env = os.environ | {"PATH": str(commands) + ":" + os.environ["PATH"]}

            def status(*args):
                return subprocess.run([sys.executable, str(script), *args], env=env).returncode

            self.assertEqual(status(), 1)
            self.assertEqual(status("--record", str(library)), 0)
            self.assertEqual(status(), 0)
            (root / "NotesWidget.qml").write_text("// QML-only update")
            self.assertEqual(status(), 0)
            header.write_text("new version")
            self.assertEqual(status(), 1)
            self.assertEqual(status("--record", str(library)), 0)
            version.write_text("6.13.0")
            self.assertEqual(status(), 1)
            self.assertEqual(status("--record", str(library)), 0)
            library.unlink()
            self.assertEqual(status(), 1)

    def test_migration_preserves_data_preferences_and_chosen_section(self):
        migration = load_script("switch-to-release")
        for legacy in (False, True):
            with self.subTest(legacy=legacy), tempfile.TemporaryDirectory() as directory:
                home = Path(directory)
                config = home / ".config/omarchy/shell.json"
                config.parent.mkdir(parents=True)
                installed = config.parent / "plugins/traymd-omarchy"
                installed.mkdir(parents=True)
                notes = home / ".local/share/traymd/notes/note.md"
                notes.parent.mkdir(parents=True)
                notes.write_text("keep my notes")
                config.write_text(json.dumps({"bar": {"layout": {"left": [], "center": [], "right": [{"id": migration.PLUGIN, "width": 600, "placement": "center"}]}}, "idle": {"lock": 42}}))
                calls = []

                def fake_run(*args):
                    calls.append(args)
                    state = json.loads(config.read_text())
                    layout = state["bar"]["layout"]
                    if args[1:3] == ("plugin", "remove"):
                        shutil.move(installed, installed.with_name(".development-backup"))
                        layout["right"] = []
                    elif args[1:3] == ("plugin", "add"):
                        self.assertEqual(args[-1], "--enable")
                        installed.mkdir()
                        (installed / ".git").mkdir()
                        if not legacy:
                            (installed / "scripts").mkdir()
                            (installed / "scripts/backend-status.py").touch()
                        layout["left"] = [{"id": migration.PLUGIN}]
                    elif args[0] == "bash":
                        layout["right"], layout["left"] = layout["left"], []
                    elif args[1:3] == ("bar", "move"):
                        entry = next(entry for entries in layout.values() for entry in entries if entry["id"] == migration.PLUGIN)
                        for section in layout:
                            layout[section] = []
                        layout[args[5]] = [entry]
                    elif args[1:3] == ("bar", "set"):
                        for entries in layout.values():
                            for entry in entries:
                                if entry["id"] == migration.PLUGIN:
                                    entry[args[4]] = json.loads(args[5])
                    config.write_text(json.dumps(state))

                with patch.object(Path, "home", return_value=home), patch.dict(os.environ, {"XDG_STATE_HOME": str(home / "state")}), patch.object(migration, "run", fake_run):
                    migration.main()
                state = json.loads(config.read_text())
                self.assertEqual(state["bar"]["layout"]["left"], [{"id": migration.PLUGIN, "width": 600, "placement": "center"}])
                self.assertEqual(state["idle"], {"lock": 42})
                self.assertEqual(notes.read_text(), "keep my notes")
                self.assertTrue((installed / ".git").is_dir())
                self.assertTrue(installed.with_name(".development-backup").is_dir())


if __name__ == "__main__":
    unittest.main()
