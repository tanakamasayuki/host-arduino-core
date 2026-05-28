"""Shared pytest hooks for host-arduino-core tests.

# WARNING — this conftest does two things that look harmless but are
# load-bearing and can bite if copied blindly into another project:
#
# 1. It SYMLINKS the repository working tree into the arduino-cli
#    sketchbook so `lang-ship:host:host` resolves locally instead of
#    requiring a release install. The link is removed at session end,
#    BUT — if anything goes wrong between setup and teardown (kill -9,
#    OS crash, pytest --collect-only abort etc.) the link can be left
#    behind. Subsequent runs detect a pre-existing target that already
#    points at this repo and reuse it, which is fine, but a target
#    pointing somewhere else makes the fixture `pytest.fail` so we
#    don't silently overwrite a real install.
#
# 2. It WIPES `<sketch_dir>/output/` before every test. This is so
#    sketches that write artifacts (PNG captures, dumped files) start
#    from a clean state and the test can assert on freshly-produced
#    files. The path is computed from `item.fspath`, which under pytest
#    is the test file path — so this targets the same directory as the
#    .ino. Copying this hook into another repo is risky: any directory
#    literally named `output` under any pytest test file will be
#    `rmtree`d. Audit before you copy.
#
# Platform note: `Path.symlink_to` on Windows requires either
# Developer Mode or administrator privileges. On a stock Windows
# session this fixture will raise an OSError during session setup and
# the test run never starts. host-arduino-core itself supports Windows
# at build / runtime, but its **test suite is Linux / macOS only**
# because of this symlink dependency.
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
    # Place a symlink at <sketchbook>/hardware/lang-ship/host that
    # points at this repo. arduino-cli treats this as a manually-
    # installed platform, so the local cores/ + boards.txt + platform.txt
    # are used directly without going through a release.
    #
    # Failure modes worth knowing about:
    #   - target already exists and points elsewhere → fail loudly so we
    #     don't silently overwrite a real install (e.g. someone has a
    #     hand-cloned copy at the same path).
    #   - target already exists and points at this repo → reuse, do not
    #     attempt to unlink at session end (we didn't create it).
    #   - Windows without Developer Mode → symlink_to raises OSError.
    target = _sketchbook_dir() / "hardware" / PACKAGE_NAME / ARCH
    created = False

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
