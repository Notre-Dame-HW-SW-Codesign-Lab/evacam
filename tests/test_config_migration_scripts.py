"""Focused tests for the legacy-to-v2 config migration helpers."""

import csv
import importlib.util
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import yaml


REPO_ROOT = Path(__file__).resolve().parents[1]


def load_script(name):
    spec = importlib.util.spec_from_file_location(name, REPO_ROOT / "scripts" / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


class ConfigMigrationScriptTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.migrate = load_script("migrate_configs_to_v2")

    def test_role_base_and_legacy_cell_base_reject_unknown_suffixes(self):
        with self.assertRaises(ValueError):
            self.migrate.role_base(Path("cell.yaml"), "_cell_config.yaml")
        with self.assertRaises(ValueError):
            self.migrate.legacy_cell_base(Path("cell.yaml"))
        self.assertEqual("demo", self.migrate.legacy_cell_base(Path("demo_cell_config.yaml")))
        self.assertEqual("demo", self.migrate.legacy_cell_base(Path("demo_config.yaml")))

    def test_write_yaml_replaces_destination_atomically(self):
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / "nested" / "config.yaml"
            target.parent.mkdir()
            target.write_text("old: value\n", encoding="utf-8")
            replace = os.replace
            observed = []

            def checking_replace(source, destination):
                observed.append((Path(source), Path(destination), target.read_text(encoding="utf-8")))
                replace(source, destination)

            with mock.patch.object(self.migrate.os, "replace", side_effect=checking_replace):
                self.migrate.write_yaml(target, {"new": "value"})

            self.assertEqual("old: value\n", observed[0][2])
            self.assertTrue(observed[0][0].is_absolute())
            self.assertEqual(target, observed[0][1])
            self.assertEqual({"new": "value"}, yaml.safe_load(target.read_text(encoding="utf-8")))
            self.assertEqual([], list(target.parent.glob("tmp*")))

    def test_conversion_resolves_references_and_is_idempotent(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            config_root = root / "config"
            demo = config_root / "demo"
            demo.mkdir(parents=True)
            arch = demo / "demo_architecture_config.yaml"
            cell = demo / "demo_cell_config.yaml"
            tool = demo / "demo_tool_config.yaml"
            arch.write_text(yaml.safe_dump({
                "design": {"rows": 4}, "memory": {"real_capacity": 32},
                "sensing": {"internal": False}, "advanced": {"bit_serial_width": 2},
            }), encoding="utf-8")
            cell.write_text(yaml.safe_dump({
                "cell": {"name": "demo", "type": "RRAM", "cell_process_node": "45nm",
                         "area": "10F2", "aspect_ratio": 1.0},
                "access_device": {"type": "cmos", "cmos_width": "2F"},
                "match": {"cmos_width": "3F"},
                "ports": {"row": {"search": {"type": "search", "num_cmos": 1,
                           "cmos_region": "none", "wire_width": "1F"}}},
                "resistance": {"on": "1kohm"}, "variation": {"with_variation": False},
            }), encoding="utf-8")
            tool.write_text(yaml.safe_dump({
                "architecture_file": arch.name, "cell_file": cell.name,
                "optimization": {"target": "latency"}, "modeling": {"use_updated_lib": True},
                "output": {"yaml_file": "result.yaml"},
            }), encoding="utf-8")

            with mock.patch.object(self.migrate, "ROOT", root), \
                 mock.patch.object(self.migrate, "CONFIG_ROOT", config_root):
                self.migrate.convert_architecture(arch)
                rows = []
                self.migrate.convert_cell(cell, rows)
                result = self.migrate.convert_tool(tool)
                first = result.read_text(encoding="utf-8")
                self.migrate.convert_architecture(arch)
                self.migrate.convert_cell(cell, [])
                self.migrate.convert_tool(tool)

            config = yaml.safe_load(result.read_text(encoding="utf-8"))
            architecture = yaml.safe_load((demo / "demo.architecture.yaml").read_text(encoding="utf-8"))
            device = yaml.safe_load((demo / "demo.memory_device.yaml").read_text(encoding="utf-8"))
            self.assertEqual(first, result.read_text(encoding="utf-8"))
            self.assertEqual("demo.architecture.yaml", config["architecture"])
            self.assertEqual("demo.cell.yaml", config["cell"])
            self.assertTrue(config["technology"].endswith("cmos.updated.yaml"))
            self.assertEqual("result.yaml", config["output"]["results"])
            self.assertEqual("3F", architecture["matchline"]["match_transistor"]["cmos_width"])
            self.assertEqual(32, architecture["memory"]["physical_capacity"])
            self.assertNotIn("variation", device)
            self.assertEqual("inline", rows[0]["v2_connection_kind"])

    def test_propagate_match_transistor_rejects_conflict(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cell = root / "cell.yaml"
            architecture = root / "architecture.yaml"
            cell.write_text("match:\n  cmos_width: 3F\n", encoding="utf-8")
            architecture.write_text("matchline:\n  match_transistor:\n    cmos_width: 2F\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "Conflicting match transistor widths"):
                self.migrate.propagate_match_transistor(cell, architecture)

    def test_main_converts_discovered_files_and_writes_report(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            architecture = root / "legacy_architecture_config.yaml"
            cell = root / "legacy_cell_config.yaml"
            tool = root / "legacy_tool_config.yaml"
            row = {"file": "legacy", "axis": "row"}
            with mock.patch.object(self.migrate, "ROOT", root), \
                 mock.patch.object(self.migrate, "iter_legacy_files", side_effect=[[architecture], [tool]]), \
                 mock.patch.object(self.migrate, "referenced_legacy_cell_files", return_value=[cell]), \
                 mock.patch.object(self.migrate, "convert_architecture", return_value=[root / "a.yaml"]), \
                 mock.patch.object(self.migrate, "convert_cell", side_effect=lambda _path, rows: rows.append(row) or [root / "c.yaml"]), \
                 mock.patch.object(self.migrate, "convert_tool", return_value=root / "t.yaml"), \
                 mock.patch.object(sys, "argv", ["migrate", "--report", "report.csv"]):
                self.assertEqual(0, self.migrate.main())
            with (root / "report.csv").open(newline="", encoding="utf-8") as handle:
                self.assertEqual([row], list(csv.DictReader(handle)))


if __name__ == "__main__":
    unittest.main()
