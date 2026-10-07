#!/usr/bin/env python3
"""Contracts for reference reporting, including failed CLI runs and stale output."""
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import yaml

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("validate_named_cam", ROOT / "scripts/validate_named_cam.py")
validation = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validation)


class NamedCamValidationTest(unittest.TestCase):
    def setUp(self):
        self.case = {
            "id": "fixture", "config": "config/fixture/fixture.config.yaml", "status": "partial",
            "reference": {"latency_ns": 0.25},
            "expected": {"entries": 64, "word_bits": 32, "subarray": [64, 32],
                         "subarray_count": 1, "node_nm": 90},
        }
        self.result = {
            "geometry": {"entry_count": 64, "logical_word_width_bits": 32,
                         "physical_columns_per_word": 32, "logical_capacity_bits": 2048,
                         "allocated_capacity_bits": 2048, "physical_cell_count": 2048,
                         "comparison_columns_per_step": 32, "comparison_steps": 1},
            "assumptions": {"technology": {"process_node": "90nm", "physical_feature_size": "90nm"},
                            "cell_topology": "generic_ports",
                            "modeling_options": {"search_latency_scope": "scheduled_full_search",
                                                 "matchline_timing_model": "horowitz_50_percent",
                                                 "sense_amplifier_model": "generic"}},
            "summary": {"area": {"subarray": {"dimensions": "64x32"}, "total": {"area": "0.01mm^2"}},
                        "timing": {"search_latency": "500ps", "sense_margin_pass": True},
                        "power": {"search_dynamic_energy": "0.002nJ"}},
            "breakdown": {"search_latency": {"matchline": "300ps", "control_node": "0ps",
                                              "sense_amplifier": "20ps"}},
        }

    def test_explicit_units_and_raw_gap(self):
        metrics = validation.compare_result(self.case, self.result)
        self.assertEqual(metrics["search_latency_ns"], 0.5)
        self.assertEqual(metrics["latency_gap_percent"], 100)
        self.assertEqual(metrics["search_energy_pj"], 2)
        self.assertEqual(metrics["area_um2"], 10000)
        self.assertEqual(metrics["comparison_status"], "partial")
        self.assertEqual(metrics["control_node_delay_ns"], 0)
        self.assertEqual(metrics["sense_amplifier_delay_ns"], .02)
        self.assertEqual(metrics["search_latency_scope"], "scheduled_full_search")
        for invalid in ("nan ns", "inf ns", "-1ns", "1e999ns", "2pJ", "3", None):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                validation.quantity(invalid, validation.TIME_NS)

    def test_rejects_geometry_drift_infeasible_sensing_and_invalid_metrics(self):
        changes = [
            ("geometry", "entry_count", 128),
            ("geometry", "allocated_capacity_bits", 4096),
            ("geometry", "comparison_steps", 2),
            ("summary.timing", "sense_margin_pass", False),
            ("summary.timing", "search_latency", "0ns"),
            ("summary.area.subarray", "dimensions", "32x64"),
            ("assumptions.technology", "physical_feature_size", "65nm"),
            ("assumptions.modeling_options", "search_latency_scope", "legacy_single_sense"),
            ("assumptions.modeling_options", "matchline_timing_model", "analytical_differential"),
            ("assumptions", "cell_topology", "fefet_gate"),
        ]
        for path, key, value in changes:
            result = copy.deepcopy(self.result)
            target = result
            for part in path.split("."):
                target = target[part]
            target[key] = value
            with self.subTest(path=path, key=key), self.assertRaises(ValueError):
                validation.compare_result(self.case, result)
        scheduled_case = copy.deepcopy(self.case)
        scheduled_case["expected"]["search_timing"] = {"control": "broadcast"}
        with self.assertRaisesRegex(ValueError, "schedule drift"):
            validation.compare_result(scheduled_case, self.result)
        scheduled_result = copy.deepcopy(self.result)
        scheduled_result["assumptions"]["search_timing"] = {"control": "broadcast"}
        self.assertIn("search_timing", validation.compare_result(scheduled_case, scheduled_result))
        with self.assertRaisesRegex(ValueError, "Unexpected explicit"):
            validation.compare_result(self.case, scheduled_result)
        for result in (None, {}, {"status": "no_valid_solutions"}):
            with self.assertRaises(ValueError):
                validation.compare_result(self.case, result)
        inconsistent = copy.deepcopy(self.case)
        inconsistent["expected"]["subarray_count"] = 2
        with self.assertRaises(ValueError):
            validation.compare_result(inconsistent, self.result)
        invalid_reference = copy.deepcopy(self.case)
        invalid_reference["reference"]["latency_ns"] = 0
        with self.assertRaises(ValueError):
            validation.compare_result(invalid_reference, self.result)

    def test_ablation_configs_isolate_decision_schedule_and_sa(self):
        cases = yaml.safe_load(validation.MANIFEST.read_text())["cases"]
        (ROOT / "test-bin").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="timing-ablation-", dir=ROOT / "test-bin") as tmp:
            for case in cases:
                if "search_timing" not in case["expected"]:
                    continue
                before = copy.deepcopy(case)
                variants = validation.ablation_cases(case, Path(tmp))
                self.assertEqual(len(variants), 4)
                for variant in variants:
                    top_path = Path(variant["config"])
                    top = yaml.safe_load(top_path.read_text())
                    arch = yaml.safe_load((top_path.parent / top["architecture"]).read_text())
                    sensing = yaml.safe_load((top_path.parent / arch["sensing"]).read_text())
                    name = variant["id"].split("/")[-1]
                    self.assertEqual("decision" in sensing, name in ("decision_only", "combined_legacy_sa"))
                    self.assertEqual("search_timing" in arch, name in ("schedule_only", "combined_legacy_sa"))
                    self.assertEqual("search_timing" in arch, "search_timing" in variant["expected"])
                    self.assertEqual(sensing["sensing_mode"], variant["expected"]["sensing_mode"])
                    for path in (top["cell"], top["technology"], sensing["sense_amplifier"]):
                        self.assertTrue(Path(path).is_absolute() and Path(path).is_file())
                self.assertEqual(case, before)

    def test_runner_preserves_evidence_and_does_not_reuse_previous_success(self):
        (ROOT / "test-bin").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="named-cam-", dir=ROOT / "test-bin") as tmp:
            root = Path(tmp)
            binary = root / "EvaCAM"
            binary.write_text("test binary provenance")
            config = root / self.case["config"]
            config.parent.mkdir(parents=True)
            config.write_text("schema: config\n")
            manifest = root / "reference.yaml"
            manifest.write_text(yaml.safe_dump({"schema": "named_cam_reference", "version": 1, "cases": [self.case]}))
            output = root / "output"

            def successful_process(command, **kwargs):
                Path(command[4]).write_text(yaml.safe_dump(self.result))
                return subprocess.CompletedProcess(command, 0)

            with patch.object(validation, "ROOT", root), patch.object(validation, "MANIFEST", manifest), \
                    patch("builtins.print"), patch.object(validation.subprocess, "run", side_effect=successful_process), \
                    patch.object(sys, "argv", ["validate_named_cam", "--binary", str(binary), "--output", str(output)]):
                self.assertEqual(validation.main(), 0)
            first = json.loads((output / "runs.json").read_text())
            self.assertEqual(first["runs"][0]["metrics"]["comparison_status"], "partial")
            self.assertEqual(len(first["binary_sha256"]), 64)
            self.assertIn("+100.0%", (output / "comparison.md").read_text())
            # A zero CLI exit with no result must still fail, despite earlier output.
            with patch.object(validation, "ROOT", root), patch.object(validation, "MANIFEST", manifest), \
                    patch("builtins.print"), patch.object(validation.subprocess, "run", return_value=subprocess.CompletedProcess([], 0)):
                self.assertEqual(validation.run_suite(binary, output), 1)
            failed = json.loads((output / "runs.json").read_text())
            self.assertIn("error", failed["runs"][0])
            self.assertNotIn("metrics", failed["runs"][0])
            self.assertNotEqual(first["runs"][0]["result"], failed["runs"][0]["result"])
            self.assertTrue(Path(first["runs"][0]["result"]).exists())
            self.assertIn("FAILED", (output / "comparison.md").read_text())


if __name__ == "__main__":
    unittest.main()
