"""Shared pytest hooks for host-arduino-core tests.

# WARNING — this conftest does two things that look harmless but are
# load-bearing and can bite if copied blindly into another project:
#
# 1. It registers the repository working tree into the arduino-cli
#    sketchbook so `lang-ship:host:host` resolves locally instead of
#    requiring a release install. It tries a symlink first; if that
#    raises OSError (Windows without Developer Mode), it falls back to
#    a full directory copy. The link/copy is removed at session end,
#    BUT — if anything goes wrong between setup and teardown (kill -9,
#    OS crash, pytest --collect-only abort etc.) the entry can be left
#    behind. Subsequent runs detect a pre-existing symlink that already
#    points at this repo and reuse it, which is fine, but a target
#    pointing somewhere else makes the fixture `pytest.fail` so we
#    don't silently overwrite a real install. A pre-existing plain
#    directory at that path is also treated as a conflict.
#
# 2. It WIPES `<sketch_dir>/output/` before every test. This is so
#    sketches that write artifacts (PNG captures, dumped files) start
#    from a clean state and the test can assert on freshly-produced
#    files. The path is computed from `item.fspath`, which under pytest
#    is the test file path — so this targets the same directory as the
#    .ino. Copying this hook into another repo is risky: any directory
#    literally named `output` under any pytest test file will be
#    `rmtree`d. Audit before you copy.
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
    # Register <sketchbook>/hardware/lang-ship/host so arduino-cli resolves
    # the local platform without a release install.  Strategy:
    #   1. Try a symlink (fast, zero disk cost, requires Developer Mode on
    #      Windows or an admin/elevated process).
    #   2. On OSError fall back to a full directory copy (always works).
    #
    # Failure modes:
    #   - target is a symlink pointing elsewhere → fail loudly.
    #   - target is a plain directory → fail loudly (conflict with real install).
    #   - target is a symlink pointing at this repo → reuse, skip cleanup.
    target = _sketchbook_dir() / "hardware" / PACKAGE_NAME / ARCH
    created = ""  # "symlink" | "copy" | "" (pre-existing, skip cleanup)

    if target.exists() or target.is_symlink():
        if target.is_symlink() and Path(os.readlink(target)) == REPO_ROOT:
            pass
        else:
            pytest.fail(
                f"{target} already exists and does not point at {REPO_ROOT}. "
                "Remove it (or repoint it) before running tests."
            )
    else:
        target.parent.mkdir(parents=True, exist_ok=True)
        try:
            target.symlink_to(REPO_ROOT, target_is_directory=True)
            created = "symlink"
        except OSError:
            shutil.copytree(str(REPO_ROOT), str(target))
            created = "copy"

    yield target

    if created == "symlink" and target.is_symlink():
        target.unlink()
        parent = target.parent
        try:
            parent.rmdir()
        except OSError:
            pass
    elif created == "copy" and target.exists():
        shutil.rmtree(target)
        parent = target.parent
        try:
            parent.rmdir()
        except OSError:
            pass


def pytest_runtest_setup(item):
    # Wipe <sketch_dir>/output/ before each test so the sketch can
    # `mkdir("output", 0755)` + write fresh artifacts that the test
    # then asserts on.
    #
    # DO NOT copy this hook into another repository without auditing.
    # `shutil.rmtree` follows the directory recursively — any
    # `output/` sitting next to a pytest test module will be deleted
    # at the start of that test. The intent here is the
    # tests/graphics/* sketch dirs where `output/` is always
    # generated content (in .gitignore); apply the same convention
    # before reusing the hook.
    output_dir = Path(item.fspath).parent / "output"
    if output_dir.exists():
        shutil.rmtree(output_dir)
