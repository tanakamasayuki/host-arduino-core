#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHANGELOG = ROOT / "CHANGELOG.md"


def update_changelog(version: str) -> None:
    lines = CHANGELOG.read_text(encoding="utf-8").splitlines()
    try:
        unreleased_idx = lines.index("## Unreleased")
    except ValueError as exc:
        raise RuntimeError("CHANGELOG.md must contain '## Unreleased'") from exc

    tail_start = len(lines)
    for i in range(unreleased_idx + 1, len(lines)):
        if lines[i].startswith("## "):
            tail_start = i
            break

    unreleased_block = lines[unreleased_idx + 1 : tail_start]
    unreleased_items = [line for line in unreleased_block if line.strip()]

    new_lines: list[str] = []
    new_lines.extend(lines[: unreleased_idx + 1])
    new_lines.append("")
    new_lines.append(f"## {version}")
    if unreleased_items:
        new_lines.extend(unreleased_items)
    else:
        new_lines.append("- (EN) No user-facing changes recorded.")
        new_lines.append("- (JA) ユーザー向け変更の記録はありません。")
    if tail_start < len(lines):
        new_lines.append("")
        new_lines.extend(lines[tail_start:])

    CHANGELOG.write_text("\n".join(new_lines).rstrip() + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description="Prepare release changelog.")
    parser.add_argument("--version", required=True, help="Release version (e.g. 0.1.0)")
    args = parser.parse_args()

    version = args.version.strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise SystemExit("Version must be semantic format: X.Y.Z")

    update_changelog(version)
    print(f"Prepared release: {version}")


if __name__ == "__main__":
    main()
