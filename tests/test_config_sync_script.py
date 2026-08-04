"""Focused tests for Python-package config-library synchronization."""

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


REPO_ROOT = Path(__file__).resolve().parents[1]


def load_script():
    spec = importlib.util.spec_from_file_location(
        "sync_python_package_config_lib", REPO_ROOT / "scripts" / "sync_python_package_config_lib.py"
    )
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class ConfigSyncScriptTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sync = load_script()

    def test_file_hashes_discovers_only_files_and_missing_root_is_empty(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "nested").mkdir()
            (root / "nested" / "one.yaml").write_text("one", encoding="utf-8")
            (root / "two.yaml").write_text("two", encoding="utf-8")
            hashes = self.sync.FileHashes(root)
            self.assertEqual({Path("nested/one.yaml"), Path("two.yaml")}, set(hashes))
            self.assertEqual({}, self.sync.FileHashes(root / "missing"))

    def test_sync_replaces_stale_tree_and_is_idempotent(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            destination = root / "destination"
            (source / "technology").mkdir(parents=True)
            (source / "technology" / "cmos.yaml").write_text("revision: 1\n", encoding="utf-8")
            destination.mkdir()
            (destination / "stale.yaml").write_text("stale\n", encoding="utf-8")
            with mock.patch.object(self.sync, "SOURCE", source), \
                 mock.patch.object(self.sync, "DESTINATION", destination):
                self.assertFalse(self.sync.IsSynchronized())
                self.sync.Sync()
                self.assertTrue(self.sync.IsSynchronized())
                first_hashes = self.sync.FileHashes(destination)
                self.sync.Sync()
                self.assertEqual(first_hashes, self.sync.FileHashes(destination))
            self.assertFalse((destination / "stale.yaml").exists())

    def test_main_checks_and_synchronizes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            destination = root / "destination"
            source.mkdir()
            source.joinpath("data.yaml").write_text("value: 1\n", encoding="utf-8")
            with mock.patch.object(self.sync, "ROOT", root), \
                 mock.patch.object(self.sync, "SOURCE", source), \
                 mock.patch.object(self.sync, "DESTINATION", destination), \
                 mock.patch.object(sys, "argv", ["sync", "--check"]):
                self.assertEqual(1, self.sync.main())
            with mock.patch.object(self.sync, "ROOT", root), \
                 mock.patch.object(self.sync, "SOURCE", source), \
                 mock.patch.object(self.sync, "DESTINATION", destination), \
                 mock.patch.object(sys, "argv", ["sync"]):
                self.assertEqual(0, self.sync.main())
            self.assertEqual("value: 1\n", destination.joinpath("data.yaml").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
