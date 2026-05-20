"""Shared pytest hooks for host-arduino-core tests.

- Session fixture symlinks this repo into the arduino-cli sketchbook as
  `hardware/lang-ship/host` so `lang-ship:host:host` resolves to the local
  working tree. The symlink is removed after the session.
- Per-test hook wipes `output/` so host-profile artifacts don't leak.
"""

import os
import shutil
import subprocess
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parent.parent
PACKAGE_NAME = "lang-ship"
ARCH = "host"


def _sketchbook_dir() -> Path:
    out = subprocess.run(
        ["arduino-cli", "config", "get", "directories.user"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    if not out:
        out = str(Path.home() / "Arduino")
    return Path(out).expanduser()


@pytest.fixture(scope="session", autouse=True)
def _local_platform_symlink():
    target = _sketchbook_dir() / "hardware" / PACKAGE_NAME / ARCH
    created = False

    if target.exists() or target.is_symlink():
        # Already present — only reuse if it already points at this repo.
        if target.is_symlink() and Path(os.readlink(target)) == REPO_ROOT:
            pass
        else:
            pytest.fail(
                f"{target} already exists and does not point at {REPO_ROOT}. "
                "Remove it (or repoint it) before running tests."
            )
    else:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.symlink_to(REPO_ROOT, target_is_directory=True)
        created = True

    yield target

    if created and target.is_symlink():
        target.unlink()
        # Clean up the parent dir if we created it and it's now empty.
        parent = target.parent
        try:
            parent.rmdir()
        except OSError:
            pass


def pytest_runtest_setup(item):
    output_dir = Path(item.fspath).parent / "output"
    if output_dir.exists():
        shutil.rmtree(output_dir)
