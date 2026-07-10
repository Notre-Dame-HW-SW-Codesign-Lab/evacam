#!/usr/bin/env python3

from pathlib import Path
import sys


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT))

import evacam  # noqa: E402


def main():
    config_lib = evacam.config_lib_path()
    assert config_lib.is_dir()

    expected_files = {
        "sense_amp/nvsim_vol.sense_amp.yaml",
        "sensing/nvsim_vol.sensing.yaml",
        "technology/cmos.legacy.yaml",
        "technology/cmos.legacy_fefet.yaml",
        "technology/cmos.legacy_planar.yaml",
        "technology/cmos.updated.yaml",
    }

    for relative in expected_files:
        path = config_lib / relative
        assert path.is_file(), path
        assert path.read_text(encoding="utf-8").strip(), path

    repo_config_lib = REPO_ROOT / "config" / "lib"
    packaged_files = {
        path.relative_to(config_lib).as_posix()
        for path in config_lib.rglob("*.yaml")
    }
    repo_files = {
        path.relative_to(repo_config_lib).as_posix()
        for path in repo_config_lib.rglob("*.yaml")
    }
    assert packaged_files == repo_files

    print("Python package data test passed")


if __name__ == "__main__":
    main()
