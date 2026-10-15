#!/usr/bin/env python3
"""Native binding and plot exports agree on nominal NAND threshold bounds."""
import csv
import importlib.util
import json
import math
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import evacam_py

spec = importlib.util.spec_from_file_location("plot_nand_distance", ROOT / "scripts/plot_nand_distance.py")
plot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(plot)


class NandDistancePlotTest(unittest.TestCase):
    def test_native_bounds_exports_and_thresholds(self):
        for name in ("NAND_MLC_TCAM", "NAND_3D_MLC_TCAM"):
            source = ROOT / "config" / name / f"{name}.config.yaml"
            original = source.read_bytes()
            with tempfile.TemporaryDirectory() as temporary:
                destination = Path(temporary) / "study"
                self.assertEqual(plot.main(["--config", str(source), "--output-dir", str(destination),
                                            "--decision-times-ns", "50", "100"]), 0)
                case = destination / "case-000"
                metadata = json.loads((case / "metadata.json").read_text())
                self.assertEqual(metadata["query"], [0]*16)
                self.assertEqual(metadata["calibration_status"], "synthetic")
                self.assertEqual(source.read_bytes(), original)
                matcher = evacam_py.EvaCAMMatch(str(next((case / "config").glob("000-*"))))
                with (case / "thresholds.csv").open() as file:
                    rows = list(csv.DictReader(file))
                # Compare all exported threshold cuts to fresh native evaluations.
                for row in rows:
                    result = matcher.evaluate_distance_threshold([0]*16, [0]*16, float(row["threshold"]))
                    self.assertAlmostEqual(result.sense_margin, float(row["sense_margin_V"]), places=12)
                    self.assertAlmostEqual(result.sense_reference_voltage, float(row["reference_voltage_V"]), places=12)
                    self.assertEqual(result.sense_margin_pass, row["pass"] == "True")
                self.assertTrue(any(float(row["slack_V"]) < 0 for row in rows))
                for witness in json.loads((case / "witnesses.json").read_text()):
                    result = matcher.evaluate_distance(witness["maximum_voltage_stored"], witness["query"])
                    self.assertEqual(result.squared_euclidean_distance, witness["squared_distance"])
                with (case / "positions.csv").open() as file:
                    positions = list(csv.DictReader(file))
                conductance = [float(p["conductance_S"]) for p in positions]
                self.assertTrue(all(math.isclose(g, conductance[0], rel_tol=1e-12) for g in conductance))
                self.assertGreater(float(positions[0]["voltage_V"]), float(positions[-1]["voltage_V"]))
                for name in ("distance-active.png", "distance-full.png", "position.png"):
                    self.assertGreater((case / name).stat().st_size, 10000)
                with self.assertRaises(FileExistsError):
                    plot.main(["--config", str(source), "--output-dir", str(destination)])

    def test_fixed_reference_and_nonzero_query(self):
        with tempfile.TemporaryDirectory() as temporary:
            source, _ = plot.snapshot_config(ROOT / "config/NAND_3D_MLC_TCAM/NAND_3D_MLC_TCAM.config.yaml",
                                            Path(temporary) / "source")
            import yaml
            root = yaml.safe_load(source.read_text())
            cell = yaml.safe_load((source.parent / root["cell"]).read_text())
            device_path = source.parent / cell["memory_device"]
            device = yaml.safe_load(device_path.read_text())
            device["nand3d"]["sensing"]["reference_voltage"] = "400mV"
            device_path.write_text(yaml.safe_dump(device))
            destination = Path(temporary) / "study"
            metadata = plot.run_case(source, destination, [1, 2]*8)
            self.assertEqual(metadata["reference_policy"], "fixed")
            with (destination / "thresholds.csv").open() as file:
                self.assertTrue(all(float(row["reference_voltage_V"]) == .4 for row in csv.DictReader(file)))
            with self.assertRaisesRegex(ValueError, "finite and positive"):
                plot.run_case(source, Path(temporary) / "bad", decision_time_ns=float("nan"))
            with self.assertRaisesRegex(ValueError, "ordinary"):
                plot.run_case(source, Path(temporary) / "wildcard", [-1]*16)

    def test_voltage_units(self):
        for value in (0.4, "0.4V", "400mV", "4e5uV"):
            self.assertAlmostEqual(plot.voltage(value), 0.4)
        with self.assertRaises(ValueError):
            plot.voltage("2ns")


if __name__ == "__main__":
    unittest.main()
