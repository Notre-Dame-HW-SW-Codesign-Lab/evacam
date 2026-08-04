#!/usr/bin/env python3

from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT))

import evacam  # noqa: E402


EXPECTED_FILES = {
    "sense_amp/nvsim_vol.sense_amp.yaml",
    "sensing/nvsim_vol.sensing.yaml",
    "technology/cmos.legacy.yaml",
    "technology/cmos.legacy_fefet.yaml",
    "technology/cmos.legacy_planar.yaml",
    "technology/cmos.updated.yaml",
}


class ConfigLibPathTest(unittest.TestCase):
    def test_returns_path_to_packaged_directory(self):
        config_lib = evacam.config_lib_path()
        self.assertIsInstance(config_lib, Path)
        self.assertTrue(config_lib.is_dir())

    def test_required_bundled_files_are_nonempty(self):
        config_lib = evacam.config_lib_path()
        for relative in EXPECTED_FILES:
            with self.subTest(relative=relative):
                path = config_lib / relative
                self.assertTrue(path.is_file(), path)
                self.assertTrue(path.read_text(encoding="utf-8").strip(), path)

    def test_packaged_yaml_inventory_matches_repository(self):
        config_lib = evacam.config_lib_path()
        repo_config_lib = REPO_ROOT / "config" / "lib"
        packaged_files = {
            path.relative_to(config_lib).as_posix()
            for path in config_lib.rglob("*.yaml")
        }
        repo_files = {
            path.relative_to(repo_config_lib).as_posix()
            for path in repo_config_lib.rglob("*.yaml")
        }
        self.assertEqual(packaged_files, repo_files)

    def test_resolves_installed_package_resource_tree(self):
        with tempfile.TemporaryDirectory() as temporary:
            package_root = Path(temporary) / "installed-evacam"
            expected = package_root / "data" / "config" / "lib"
            expected.mkdir(parents=True)
            with mock.patch.object(evacam.resources, "files", return_value=package_root) as files:
                self.assertEqual(evacam.config_lib_path(), expected)
            files.assert_called_once_with("evacam")

    def test_missing_packaged_resource_raises(self):
        with tempfile.TemporaryDirectory() as temporary:
            package_root = Path(temporary) / "installed-evacam"
            package_root.mkdir()
            with mock.patch.object(evacam.resources, "files", return_value=package_root):
                with self.assertRaisesRegex(FileNotFoundError, "config library is missing"):
                    evacam.config_lib_path()


if __name__ == "__main__":
    unittest.main()
