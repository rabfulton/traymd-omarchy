#!/usr/bin/env python3
"""First-install/retry terminal; update workers already have their own terminal."""
import fcntl
import os
from pathlib import Path
import subprocess
import time


def busy(path):
    with path.open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return True
    return False


def main():
    project = Path(__file__).resolve().parent.parent
    cache = Path(os.environ.get("XDG_CACHE_HOME", str(Path.home() / ".cache"))) / "traymd-omarchy"
    cache.mkdir(parents=True, exist_ok=True)
    with (cache / "launch.lock").open("a") as launching:
        try:
            fcntl.flock(launching, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return
        runtime = Path(os.environ.get("XDG_RUNTIME_DIR", "/tmp"))
        if busy(cache / "setup.lock") or busy(runtime / "omarchy-update.lock"):
            return
        process = subprocess.Popen(["omarchy-launch-terminal", "bash", str(project / "scripts/first-run.sh")], start_new_session=True)
        # Keep concurrent widgets from opening duplicate terminals while the
        # terminal starts and the setup worker takes over the setup lock.
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if busy(cache / "setup.lock") or (process.poll() is not None and process.returncode != 0):
                return
            time.sleep(0.05)


if __name__ == "__main__":
    main()
