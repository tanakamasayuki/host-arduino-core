#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLATFORM_TXT = ROOT / "platform.txt"
EXAMPLES_DIR = ROOT / "libraries" / "Host" / "examples"
HOST_PLATFORM_RE = re.compile(r"^(\s*-\s*platform:\s*lang-ship:host\s*)\([^)]*\)(\s*)$")


def update_platform_version(version: str) -> None:
    lines = PLATFORM_TXT.read_text(encoding="utf-8").splitlines()
    for index, line in enumerate(lines):
        if line.startswith("version="):
            lines[index] = f"version={version}"
            PLATFORM_TXT.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")
            return
    raise RuntimeError("platform.txt must contain a 'version=' entry")


def update_example_sketch_versions(version: str) -> tuple[int, int]:
    matched = 0
    changed_count = 0
    for path in sorted(EXAMPLES_DIR.glob("*/*/sketch.yaml")):
        lines = path.read_text(encoding="utf-8").splitlines()
        changed = False
        for index, line in enumerate(lines):
            updated = HOST_PLATFORM_RE.sub(rf"\1({version})\2", line)
            if HOST_PLATFORM_RE.match(line):
                matched += 1
            if updated != line:
                lines[index] = updated
                changed = True
                changed_count += 1
        if changed:
            path.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")
    return matched, changed_count


def main() -> None:
    parser = argparse.ArgumentParser(description="Update host Arduino Core version metadata.")
    parser.add_argument("version", help="Release version, for example 0.1.0")
    args = parser.parse_args()

    version = args.version.strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise SystemExit("Version must be semantic format: X.Y.Z")

    update_platform_version(version)
    matched_sketches, changed_sketches = update_example_sketch_versions(version)
    print(f"Updated platform.txt version to {version}")
    print(f"Checked {matched_sketches} example sketch platform version(s); updated {changed_sketches}")


if __name__ == "__main__":
    main()
