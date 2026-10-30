#!/usr/bin/env python3
"""Connect an installed Git checkout to Omarchy's existing update commands."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

MARKER = "# traymd-omarchy managed update hook"


def install_git_hook(project):
    if not (project / ".git").exists():
        return
    configured = subprocess.run(["git", "-C", str(project), "config", "--get", "core.hooksPath"], capture_output=True, text=True)
    if configured.returncode == 0:
        # A configured hook directory may be shared with other repositories.
        # Do not change its behavior as a side effect of installing this app.
        print("Custom core.hooksPath is configured; Git update hook was not installed.", file=sys.stderr)
        return
    location = subprocess.check_output(["git", "-C", str(project), "rev-parse", "--path-format=absolute", "--git-common-dir"], text=True).strip()
    hooks = Path(location) / "hooks"
    hooks.mkdir(exist_ok=True)
    target = hooks / "post-merge"
    previous = hooks / "post-merge.before-traymd"
    if target.exists() and MARKER not in target.read_text(errors="replace"):
        if previous.exists():
            raise RuntimeError(f"Cannot preserve existing hook: {previous} already exists")
        target.rename(previous)
    # The wrapper is local Git metadata; the implementation updates with the
    # plugin. Preserve any existing hook, including a different interpreter.
    body = f'''#!/usr/bin/env bash
{MARKER}
previous={shlex.quote(str(previous))}
status=0
if [[ -x $previous ]]; then "$previous" "$@" || status=$?; fi
project=$(git rev-parse --show-toplevel) || exit 1
bash "$project/scripts/after-update.sh" plugin || exit $?
exit "$status"
'''
    with tempfile.NamedTemporaryFile(mode="w", dir=hooks, delete=False) as output:
        temporary = Path(output.name)
        output.write(body)
    temporary.chmod(0o755)
    os.replace(temporary, target)


def main():
    project = Path(sys.argv[1]).resolve()
    install_git_hook(project)
    subprocess.run(["omarchy", "hook", "install", "post-update", str(project / "scripts/hooks/traymd-omarchy")], check=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"Could not install TrayMD update integration: {error}")
