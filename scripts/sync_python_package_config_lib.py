#!/usr/bin/env python3
"""Mirror the default config library into the Python package data tree."""

import argparse
import hashlib
import shutil
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "config" / "lib"
DESTINATION = ROOT / "evacam" / "data" / "config" / "lib"


def FileHashes(root: Path) -> dict[Path, str]:
    if not root.is_dir():
        return {}

    hashes = {}
    for path in sorted(root.rglob("*")):
        if path.is_file():
            hashes[path.relative_to(root)] = hashlib.sha256(path.read_bytes()).hexdigest()
    return hashes


def IsSynchronized() -> bool:
    return FileHashes(SOURCE) == FileHashes(DESTINATION)


def Sync() -> None:
    if DESTINATION.exists():
        shutil.rmtree(DESTINATION)
    DESTINATION.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(SOURCE, DESTINATION)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="exit nonzero when the package data does not match config/lib",
    )
    args = parser.parse_args()

    if not SOURCE.is_dir():
        raise FileNotFoundError(f"source config library is missing: {SOURCE}")

    if IsSynchronized():
        print("Python package config library is already synchronized")
        return 0

    if args.check:
        print("Python package config library is out of date; run make sync-python-package-data")
        return 1

    Sync()
    print(f"Synchronized {SOURCE.relative_to(ROOT)} to {DESTINATION.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
