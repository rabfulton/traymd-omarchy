"""Installer helper checks without changing the live desktop or using GitHub."""
import importlib.util
import fcntl
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
    def update_fixture(self, directory):
        root = Path(directory) / "plugin with spaces"
        (root / "scripts").mkdir(parents=True)
        shutil.copy2(PROJECT / "scripts/after-update.sh", root / "scripts/after-update.sh")
        (root / "scripts/backend-status.py").write_text('from pathlib import Path\nraise SystemExit(int((Path(__file__).parent.parent / "status").read_text()))\n')
        (root / "status").write_text("1")
        (root / "install.sh").write_text('''#!/usr/bin/env bash
set -e
printf 'install %s\\n' "$*" >> "$TRAYMD_TEST_EVENTS"
if [[ ${TRAYMD_TEST_BUILD_FAIL:-0} == 1 ]]; then exit 7; fi
printf 0 > "$(dirname "$0")/status"
''')
        commands = Path(directory) / "commands"
        commands.mkdir()
        omarchy = commands / "omarchy"
        omarchy.write_text('#!/bin/sh\nprintf "omarchy %s\\n" "$*" >> "$TRAYMD_TEST_EVENTS"\n')
        omarchy.chmod(0o755)
        notify = commands / "notify-send"
        notify.write_text('#!/bin/sh\nprintf "notification\\n" >> "$TRAYMD_TEST_EVENTS"\n')
        notify.chmod(0o755)
        events = Path(directory) / "events"
        events.touch()
        env = os.environ | {"PATH": str(commands) + ":" + os.environ["PATH"], "XDG_CACHE_HOME": str(Path(directory) / "cache"), "TRAYMD_TEST_EVENTS": str(events)}
        env.pop("OMARCHY_UPDATE_LOGGED", None)
        return root, events, env

    def test_system_update_builds_before_host_restart_and_skips_current_backend(self):
        with tempfile.TemporaryDirectory() as directory:
            root, events, env = self.update_fixture(directory)
            command = ["bash", str(root / "scripts/after-update.sh"), "system"]
            result = subprocess.run(command, env=env, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(events.read_text(), "install --no-deps --no-restart\n")
            events.write_text("")
            self.assertEqual(subprocess.run(command, env=env, capture_output=True).returncode, 0)
            self.assertEqual(events.read_text(), "")

    def test_real_fast_forward_merge_prepares_backend_and_preserves_existing_hook(self):
        with tempfile.TemporaryDirectory() as directory:
            root, events, env = self.update_fixture(directory)

            def git(*args):
                return subprocess.check_output(["git", "-C", str(root), "-c", "user.name=TrayMD tests", "-c", "user.email=tests@example.invalid", *args], env=env, stderr=subprocess.STDOUT, text=True).strip()

            git("init", "-q")
            (root / ".gitignore").write_text("status\n")
            (root / "version").write_text("one")
            git("add", ".")
            git("commit", "-qm", "first")
            initial = git("rev-parse", "HEAD")
            (root / "version").write_text("two")
            git("commit", "-qam", "second")
            updated = git("rev-parse", "HEAD")
            git("checkout", "-q", "--detach", initial)
            previous = root / ".git/hooks/post-merge"
            previous.write_text('#!/bin/sh\nprintf "previous hook\\n" >> "$TRAYMD_TEST_EVENTS"\n')
            previous.chmod(0o755)
            hooks = load_script("install-update-hooks")
            hooks.install_git_hook(root)
            hooks.install_git_hook(root)  # Idempotent; no repeated wrapping.
            notes = Path(directory) / "notes.md"
            notes.write_text("keep my note")
            git("merge", "--ff-only", updated)
            self.assertEqual(events.read_text(), "previous hook\ninstall --no-deps\n")
            self.assertEqual((root / "status").read_text(), "0")
            self.assertEqual(notes.read_text(), "keep my note")
            events.write_text("")
            result = subprocess.run(["bash", str(root / "scripts/after-update.sh"), "plugin"], env=env, capture_output=True)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(events.read_text(), "omarchy restart shell\n")

    def test_failed_update_retains_failure_status_and_log(self):
        with tempfile.TemporaryDirectory() as directory:
            root, events, env = self.update_fixture(directory)
            env["TRAYMD_TEST_BUILD_FAIL"] = "1"
            result = subprocess.run(["bash", str(root / "scripts/after-update.sh"), "plugin"], env=env, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual((root / "status").read_text(), "1")
            log = Path(env["XDG_CACHE_HOME"]) / "traymd-omarchy/update.log"
            self.assertIn("Preparing TrayMD", log.read_text())
            self.assertIn("notification", events.read_text())

    def test_launcher_does_not_open_terminal_during_build_or_system_update(self):
        launcher = load_script("launch-setup")
        for lock_name in ("setup.lock", "omarchy-update.lock"):
            with self.subTest(lock=lock_name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                cache = root / "cache/traymd-omarchy"
                cache.mkdir(parents=True)
                runtime = root / "runtime"
                runtime.mkdir()
                lock_path = cache / lock_name if lock_name == "setup.lock" else runtime / lock_name
                with lock_path.open("a") as lock, patch.dict(os.environ, {"XDG_CACHE_HOME": str(root / "cache"), "XDG_RUNTIME_DIR": str(runtime)}), patch.object(launcher.subprocess, "Popen") as terminal:
                    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    launcher.main()
                    terminal.assert_not_called()

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
