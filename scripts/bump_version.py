#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLATFORM_TXT = ROOT / "platform.txt"


def update_platform_version(version: str) -> None:
    lines = PLATFORM_TXT.read_text(encoding="utf-8").splitlines()
    for index, line in enumerate(lines):
        if line.startswith("version="):
            lines[index] = f"version={version}"
            PLATFORM_TXT.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")
            return
    raise RuntimeError("platform.txt must contain a 'version=' entry")


def main() -> None:
    parser = argparse.ArgumentParser(description="Update host Arduino Core version metadata.")
    parser.add_argument("version", help="Release version, for example 0.1.0")
    args = parser.parse_args()

    version = args.version.strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise SystemExit("Version must be semantic format: X.Y.Z")

    update_platform_version(version)
    print(f"Updated platform.txt version to {version}")


if __name__ == "__main__":
    main()
