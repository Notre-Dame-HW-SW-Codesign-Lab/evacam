"""Focused tests for small config and report generation scripts."""

import csv
import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


REPO_ROOT = Path(__file__).resolve().parents[1]


def load_script(name):
    spec = importlib.util.spec_from_file_location(name, REPO_ROOT / "scripts" / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


class GenerationScriptsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.mc = load_script("generate_variation_mc_sweep")
        cls.corner = load_script("generate_corner_variation_table")

    def test_replace_or_append_variation_block_is_idempotent(self):
        template = "schema: memory_device\nname: demo\nvariation:\n  mode: single_point\nother: kept\n"
        generated = self.mc.replace_or_append_variation_block(template, 7)
        self.assertIn("mode: monte_carlo", generated)
        self.assertIn("memory_device_resistance_on_stdev: 7%", generated)
        self.assertIn("other: kept", generated)
        self.assertEqual(generated, self.mc.replace_or_append_variation_block(generated, 7))
        appended = self.mc.replace_or_append_variation_block("schema: memory_device\n", 2)
        self.assertEqual(1, appended.count("variation:\n"))

    def test_replace_scalar_changes_exact_key_and_reports_missing_key(self):
        self.assertEqual("name: new\ncell: old\n", self.mc.replace_scalar("name: old\ncell: old\n", "name", "new"))
        with self.assertRaisesRegex(RuntimeError, "Could not replace technology"):
            self.mc.replace_scalar("name: old\n", "technology", "x")

    def test_mc_main_generates_consistent_local_references_on_repeat(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            base = root / "config" / "2FeFET_TCAM_var"
            base.mkdir(parents=True)
            cell = root / "config" / "2FeFET_TCAM" / "2FeFET_TCAM.cell.yaml"
            device = root / "config" / "2FeFET_TCAM" / "2FeFET_TCAM.memory_device.yaml"
            cell.parent.mkdir(parents=True)
            cell.write_text("memory_device: old.yaml\n", encoding="utf-8")
            device.write_text("schema: memory_device\n", encoding="utf-8")
            config = base / "2FeFET_TCAM.config.yaml"
            config.write_text(
                "name: old\narchitecture: old\ncell: old\ntechnology: old\n", encoding="utf-8"
            )
            output = base / "mc_sweep"
            with mock.patch.object(self.mc, "ROOT", root), \
                 mock.patch.object(self.mc, "BASE_DIR", base), \
                 mock.patch.object(self.mc, "OUT_DIR", output), \
                 mock.patch.object(self.mc, "BASE_CELL", cell), \
                 mock.patch.object(self.mc, "BASE_MEMORY_DEVICE", device), \
                 mock.patch.object(self.mc, "BASE_CONFIG", config), \
                 mock.patch.object(self.mc, "STDEVS", [2, 5]):
                self.mc.main()
                first = (output / "2FeFET_TCAM_stdev02.memory_device.yaml").read_text(encoding="utf-8")
                self.mc.main()
            generated_cell = (output / "2FeFET_TCAM_stdev05.cell.yaml").read_text(encoding="utf-8")
            generated_config = (output / "2FeFET_TCAM_stdev05.config.yaml").read_text(encoding="utf-8")
            generated_device = (output / "2FeFET_TCAM_stdev05.memory_device.yaml").read_text(encoding="utf-8")
            self.assertEqual(first, (output / "2FeFET_TCAM_stdev02.memory_device.yaml").read_text(encoding="utf-8"))
            self.assertIn("memory_device: ./2FeFET_TCAM_stdev05.memory_device.yaml", generated_cell)
            self.assertIn("cell: 2FeFET_TCAM_stdev05.cell.yaml", generated_config)
            self.assertIn("memory_device_resistance_off_stdev: 5%", generated_device)

    def test_corner_table_discovers_metrics_ranks_and_rejects_invalid_csv(self):
        rows = [
            {"sample": "2", "corner_label": "slow", "memory_device_res_on_corner": "high",
             "memory_device_res_off_corner": "low", "matchline_delay_s": "2e-12", "search_latency_s": ""},
            {"sample": "1", "corner_label": "fast", "memory_device_res_on_corner": "low",
             "memory_device_res_off_corner": "high", "matchline_delay_s": "1e-12", "search_latency_s": "3e-12"},
        ]
        table = self.corner.build_table(rows)
        self.assertIn("Matchline Delay Sorted By Plot Index", table)
        self.assertIn("Search Latency Sorted By Plot Index", table)
        self.assertIn("memory on low->high", table)
        self.assertEqual(Path("demo_variation_corner_table.md"), self.corner.default_output_path(Path("demo_variation_samples.csv")))
        self.assertEqual("a\\|b", self.corner.markdown_escape("a|b"))
        with tempfile.TemporaryDirectory() as temporary:
            empty = Path(temporary) / "empty.csv"
            empty.write_text("sample,corner_label\n", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "No corner sample rows"):
                self.corner.read_rows(empty)
            bad = Path(temporary) / "bad.csv"
            with bad.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.DictWriter(handle, fieldnames=["sample", "corner_label"])
                writer.writeheader()
                writer.writerow({"sample": "1", "corner_label": "x"})
            with self.assertRaisesRegex(RuntimeError, "missing corner metadata"):
                self.corner.read_rows(bad)


if __name__ == "__main__":
    unittest.main()
