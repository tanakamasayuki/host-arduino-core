#!/usr/bin/env python3
import argparse
import hashlib
import json
import shutil
from pathlib import Path


PACKAGE_SLUG = "host-arduino-core"
PACKAGE_NAME = "lang-ship"
PACKAGE_MAINTAINER = "tanakamasayuki"
PLATFORM_NAME = "Host Arduino Core"
ARCHITECTURE = "host"
DEFAULT_REPO = "tanakamasayuki/host-arduino-core"
PACKAGE_FILES = ("cores", "libraries", "platform.txt", "boards.txt", "README.md", "LICENSE")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def copy_package_item(root: Path, package_dir: Path, name: str) -> None:
    src = root / name
    if not src.exists():
        return
    dst = package_dir / name
    if src.is_dir():
        shutil.copytree(src, dst, dirs_exist_ok=True)
    else:
        shutil.copy2(src, dst)


def make_zip(root: Path, version: str, output_zip: str | None) -> Path:
    zip_name = output_zip or f"{PACKAGE_SLUG}-{version}.zip"
    zip_path = root / zip_name
    if zip_path.exists():
        zip_path.unlink()

    archive_base = root / zip_path.stem
    archive_path = shutil.make_archive(str(archive_base), "zip", root_dir=root / "package", base_dir=PACKAGE_SLUG)
    return Path(archive_path)


def load_index(path: Path) -> dict:
    if not path.exists():
        return {"packages": []}
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def github_url(repo: str) -> str:
    return f"https://github.com/{repo}"


def find_or_create_package(data: dict, repo: str) -> dict:
    for package in data.setdefault("packages", []):
        if package.get("name") == PACKAGE_NAME:
            package["maintainer"] = package.get("maintainer") or PACKAGE_MAINTAINER
            package["websiteURL"] = github_url(repo)
            package["help"] = {"online": github_url(repo)}
            package.setdefault("platforms", [])
            package.setdefault("tools", [])
            return package

    package = {
        "name": PACKAGE_NAME,
        "maintainer": PACKAGE_MAINTAINER,
        "websiteURL": github_url(repo),
        "email": "",
        "help": {"online": github_url(repo)},
        "platforms": [],
        "tools": [],
    }
    data["packages"].append(package)
    return package


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--repo", default=DEFAULT_REPO)
    parser.add_argument("--output-zip")
    parser.add_argument("--package-index", default="package_index.json")
    args = parser.parse_args()

    root = Path.cwd()
    package_root = root / "package"
    package_dir = package_root / PACKAGE_SLUG
    if package_root.exists():
        shutil.rmtree(package_root)
    package_dir.mkdir(parents=True)

    for name in PACKAGE_FILES:
        copy_package_item(root, package_dir, name)

    zip_path = make_zip(root, args.version, args.output_zip)
    checksum = sha256_file(zip_path)
    zip_name = zip_path.name

    index_path = root / args.package_index
    data = load_index(index_path)
    package = find_or_create_package(data, args.repo)
    url = f"https://github.com/{args.repo}/releases/download/v{args.version}/{zip_name}"
    platform = {
        "name": PLATFORM_NAME,
        "architecture": ARCHITECTURE,
        "version": args.version,
        "url": url,
        "archiveFileName": zip_name,
        "checksum": f"SHA-256:{checksum}",
    }

    platforms = package.setdefault("platforms", [])
    for index, existing in enumerate(platforms):
        if existing.get("version") == args.version and existing.get("architecture") == ARCHITECTURE:
            platforms[index] = platform
            break
    else:
        platforms.append(platform)

    with index_path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
        f.write("\n")
    with (package_root / "package_index.json").open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
        f.write("\n")

    print(f"Created {zip_path}")
    print(f"SHA-256:{checksum}")


if __name__ == "__main__":
    main()
