#!/usr/bin/env python3
"""Check that the native backend matches this source checkout and Qt version."""
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile


def fingerprint(project):
    version = subprocess.check_output(["pkg-config", "--modversion", "Qt6Core"], stderr=subprocess.DEVNULL)
    digest = hashlib.sha256(version + platform.machine().encode())
    files = [project / "CMakeLists.txt"] + sorted((project / "backend").glob("*"))
    for path in files:
        if path.is_file():
            digest.update(path.name.encode())
            digest.update(path.read_bytes())
    return digest.hexdigest()


def main():
    project = Path(__file__).resolve().parent.parent
    marker = project / ".native-build.json"
    value = fingerprint(project)
    if len(sys.argv) == 3 and sys.argv[1] == "--record":
        library = Path(sys.argv[2]).resolve()
        if not library.is_file():
            return 1
        with tempfile.NamedTemporaryFile(mode="w", dir=project, prefix=".native-build-", delete=False) as output:
            temporary = output.name
            json.dump({"fingerprint": value, "library": str(library)}, output)
        os.replace(temporary, marker)
        return 0
    state = json.loads(marker.read_text())
    return 0 if state.get("fingerprint") == value and Path(state["library"]).is_file() and (project / "TrayMDBackend/qmldir").is_file() else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError):
        raise SystemExit(1)
