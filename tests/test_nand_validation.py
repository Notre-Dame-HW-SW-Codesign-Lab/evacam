"""Audit contracts and compiled-model comparisons without claiming paper replication."""

import contextlib
import copy
import csv
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import numpy as np
import yaml


ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS))
SPEC = importlib.util.spec_from_file_location(
    "validate_nand_literature", SCRIPTS / "validate_nand_literature.py"
)
VALIDATION = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VALIDATION)
PROBE = ROOT / "test-bin/NandValidationProbe"
BINARY = ROOT / "EvaCAM"


def read_probe(config):
    result = subprocess.run([str(PROBE), str(config)], cwd=ROOT, text=True,
                            capture_output=True, check=True)
    return yaml.safe_load(result.stdout)


class AuditContractTest(unittest.TestCase):
    def test_energy_normalization_uses_logical_key_bits(self):
        self.assertEqual(VALIDATION.normalize_energy(0, 64, 32), 0)
        self.assertAlmostEqual(VALIDATION.normalize_energy(2048e-15, 64, 32), 1)
        self.assertAlmostEqual(VALIDATION.normalize_energy(2048e-15, 128, 32), 0.5)
        for values in ((-1, 64, 32), (np.nan, 64, 32), (np.inf, 64, 32),
                       (1, 0, 32), (1, -1, 32), (1, np.inf, 32), (1, np.nan, 32),
                       (1, 64, 0), (1, 64, -1), (1, 64, np.inf), (1, 64, np.nan)):
            with self.subTest(values=values), self.assertRaises(ValueError):
                VALIDATION.normalize_energy(*values)

    def test_complete_evidence_is_readiness_not_a_reproduction_claim(self):
        reference = yaml.safe_load(VALIDATION.REFERENCE.read_text())
        audit = VALIDATION.comparability(reference)
        self.assertFalse(audit["paper_reproduction_validated"])
        self.assertFalse(audit["ready_to_attempt_reproduction"])
        self.assertIn("full_text", audit["missing_evidence"])
        missing_parameter = next(name for name, value in
                                 reference["required_for_numerical_reproduction"].items()
                                 if value is None)
        self.assertIn(missing_parameter, audit["missing_evidence"])

        supplied = copy.deepcopy(reference)
        supplied["access"]["full_text_available"] = True
        supplied["required_for_numerical_reproduction"] = {
            name: "documented source" for name in supplied["required_for_numerical_reproduction"]
        }
        ready = VALIDATION.comparability(supplied)
        self.assertTrue(ready["ready_to_attempt_reproduction"])
        self.assertEqual(ready["missing_evidence"], [])
        self.assertFalse(ready["paper_reproduction_validated"])
        self.assertIn("independent", ready["reason"])

    def test_variant_snapshots_preserve_device_off_key_and_technology(self):
        base = VALIDATION.BASE
        original_text = base.read_text()
        original = yaml.safe_load(original_text)
        cell_path = (base.parent / original["cell"]).resolve()
        cell = yaml.safe_load(cell_path.read_text())
        device_text = (cell_path.parent / cell["memory_device"]).read_text()
        technology_text = (base.parent / original["technology"]).read_text()
        with tempfile.TemporaryDirectory(prefix="nand-variant-test-") as temporary:
            directory = Path(temporary) / "wl512"
            config_path = VALIDATION.prepare_variant(base, directory, 512, 255)
            config = yaml.safe_load(config_path.read_text())
            architecture = yaml.safe_load((directory / config["architecture"]).read_text())
            entries = architecture["organization"]["subarray"]["dimensions"][0]
            self.assertEqual(architecture["organization"]["subarray"]["dimensions"], [entries, 255])
            self.assertEqual(architecture["memory"]["word_width"], "255bits")
            self.assertEqual(architecture["memory"]["capacity"], f"{entries * 255 // 8}B")
            self.assertEqual(architecture["flash"]["block_size"], f"{entries * 512 // 8}B")
            self.assertEqual(architecture["flash"]["page_size"], f"{entries // 8}B")
            copied_device = (directory / "memory_device.yaml").read_text()
            self.assertEqual(copied_device, device_text)
            # YAML 1.1 safe_load interprets an unquoted 'off' key as False;
            # BaseLoader checks the actual spelling without that resolver.
            literal = yaml.load(copied_device, Loader=yaml.BaseLoader)
            self.assertEqual(literal["nand"]["resistance"]["off"], "1Gohm")
            self.assertNotIn("false", literal["nand"]["resistance"])
            self.assertEqual((directory / config["technology"]).read_text(), technology_text)
            self.assertFalse(Path(config["technology"]).is_absolute())
            self.assertEqual(base.read_text(), original_text)

    def test_variant_rejects_keys_without_validity_capacity(self):
        with tempfile.TemporaryDirectory(prefix="nand-invalid-geometry-") as temporary:
            for wordlines, width in ((64, 32), (16, 0), (16, -1)):
                with self.subTest(wordlines=wordlines, width=width), self.assertRaises(ValueError):
                    VALIDATION.prepare_variant(VALIDATION.BASE, Path(temporary), wordlines, width)

    def test_ladder_positions_include_validity_selects_and_padding(self):
        probe = {
            "geometry": {"data_wordlines": 8},
            "device": {"resistance_read_on_ohm": 11., "resistance_pass_ohm": 5.,
                       "resistance_off_ohm": 999., "resistance_select_ohm": 2.,
                       "capacitance_source_f": 7., "capacitance_internal_f": 3.,
                       "capacitance_bitline_f": 19.},
        }
        cases = (
            ("match0", [0, 0], [0, 0], True, [2, 11, 5, 11, 5, 11, 5, 5, 5, 2]),
            ("match1", [1, 1], [1, 1], True, [2, 11, 5, 5, 11, 5, 11, 5, 5, 2]),
            ("allwildcard", [-1, -1], [-1, -1], True, [2, 11, 5, 5, 5, 5, 5, 5, 5, 2]),
            ("source_mismatch", [1, -1], [0, -1], True, [2, 11, 5, 999, 5, 5, 5, 5, 5, 2]),
            ("drain_mismatch", [-1, 0], [-1, 1], True, [2, 11, 5, 5, 5, 5, 999, 5, 5, 2]),
            ("invalid_marker", [-1, -1], [-1, -1], False, [2, 999, 5, 5, 5, 5, 5, 5, 5, 2]),
            ("stored_wildcard", [-1, 1], [0, -1], True, [2, 11, 5, 11, 5, 5, 5, 5, 5, 2]),
        )
        for name, stored, query, valid, expected in cases:
            with self.subTest(name=name):
                r, c = VALIDATION.pattern_ladder(probe, {"stored": stored, "query": query, "valid": valid})
                np.testing.assert_array_equal(r, expected)
                np.testing.assert_array_equal(c, [7, 3, 3, 3, 3, 3, 3, 3, 3, 19])

    def test_require_paper_validation_returns_two_without_converting_diagnostic_to_success(self):
        report = {"comparison": {"paper_reproduction_validated": False},
                  "numerical_reference": {"pointwise_nonlinear_device_validation": False}}
        for extra_arguments, expected_status in (([], 0), (["--require-paper-validation"], 2)):
            with self.subTest(arguments=extra_arguments), mock.patch.object(
                    sys, "argv", ["validate_nand_literature.py", *extra_arguments]), mock.patch.object(
                    VALIDATION, "run_validation", return_value=report) as run:
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    status = VALIDATION.main()
                self.assertEqual(status, expected_status)
                self.assertFalse(json.loads(output.getvalue())["paper_reproduction_validated"])
                run.assert_called_once()

    def test_failed_validation_command_returns_one_with_actionable_error(self):
        error = subprocess.CalledProcessError(7, ["probe", "fixture.yaml"],
                                            stderr="NAND fixture could not be loaded")
        output, diagnostics = io.StringIO(), io.StringIO()
        with mock.patch.object(sys, "argv", ["validate_nand_literature.py"]), mock.patch.object(
                VALIDATION, "run_validation", side_effect=error), contextlib.redirect_stdout(
                output), contextlib.redirect_stderr(diagnostics):
            status = VALIDATION.main()
        self.assertEqual(status, 1)
        self.assertEqual(output.getvalue(), "")
        self.assertIn("7", diagnostics.getvalue())
        self.assertIn("fixture.yaml", diagnostics.getvalue())
        self.assertIn("NAND fixture could not be loaded", diagnostics.getvalue())


class CompiledValidationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for binary in (PROBE, BINARY):
            if not binary.is_file():
                raise AssertionError(f"Missing {binary}; run make test-nand-validation")
        cls.directory = tempfile.TemporaryDirectory(prefix="nand-validation-integration-")
        cls.addClassCleanup(cls.directory.cleanup)
        cls.root = Path(cls.directory.name)
        cls.probes = {}
        for wordlines, width in ((68, 32), (512, 255)):
            config = VALIDATION.prepare_variant(VALIDATION.BASE, cls.root / f"wl{wordlines}", wordlines, width)
            cls.probes[wordlines] = read_probe(config)

    def test_probe_cli_errors_do_not_emit_partial_yaml(self):
        for arguments, status in (([], 2), ([str(self.root / "missing.config.yaml")], 1)):
            with self.subTest(arguments=arguments):
                result = subprocess.run([str(PROBE), *arguments], cwd=ROOT, text=True, capture_output=True)
                self.assertEqual(result.returncode, status)
                self.assertEqual(result.stdout, "")
                self.assertIn("NandValidationProbe", result.stderr)
        malformed = self.root / "malformed.config.yaml"
        malformed.write_text("schema: config\narchitecture: [unterminated\n")
        result = subprocess.run([str(PROBE), str(malformed)], cwd=ROOT, text=True, capture_output=True)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, "")
        self.assertIn("NandValidationProbe", result.stderr)

    def test_probe_declares_zero_wire_scope_and_reports_failed_margin(self):
        probe = self.probes[68]
        self.assertEqual(probe["schema"], "nand_validation_probe")
        for flag in ("zero_wire", "bitline_wire_omitted", "wordline_wire_omitted", "bank_routing_omitted"):
            self.assertTrue(probe["metadata"][flag])
        self.assertEqual(probe["metadata"]["calibration_status"], "synthetic")
        self.assertIn("not an independent reference", probe["metadata"]["time_constant_source"])

        config = VALIDATION.prepare_variant(VALIDATION.BASE, self.root / "impossible-margin", 68, 32)
        device = config.parent / "memory_device.yaml"
        self.assertIn("min_margin: 0.1V", device.read_text())
        # Below Vpre/2 so the input is legal, but above Vpre/2 - offset so
        # even ideally separated signals cannot provide the requested margin.
        device.write_text(device.read_text().replace("min_margin: 0.1V", "min_margin: 0.395V"))
        impossible = read_probe(config)
        self.assertFalse(impossible["model"]["sense_margin_pass"])
        match = next(p for p in impossible["patterns"] if p["name"] == "match0")
        self.assertTrue(match["ideal_hit"])
        self.assertFalse(match["model_margin_pass"])

    def test_full_ladder_exposes_512_wordline_false_margin_pass(self):
        probe = self.probes[512]
        pattern = next(p for p in probe["patterns"] if p["name"] == "match0")
        row, curve = VALIDATION.compare_pattern(probe, pattern)
        self.assertTrue(row["model_margin_pass"])
        self.assertFalse(row["reference_margin_pass"])
        self.assertGreaterEqual(row["model_margin_v"], probe["device"]["min_sense_margin_v"])
        self.assertLess(row["reference_margin_v"], probe["device"]["min_sense_margin_v"])
        self.assertLess(row["moment_relative_error"], 1e-8)
        self.assertLess(row["conductance_relative_error"], 1e-10)
        self.assertGreater(row["max_sampled_waveform_absolute_error_v"], 0.01)
        self.assertFalse(row["sampled_waveform_within_10mv"])
        self.assertEqual(curve[0]["time_s"], 0)
        self.assertEqual(curve[0]["reference_voltage_v"], probe["device"]["voltage_precharge_v"])
        self.assertIn(probe["device"]["decision_time_s"], [point["time_s"] for point in curve])

    def test_reference_rejects_inconsistent_model_moment_and_conductance(self):
        probe = self.probes[68]
        matching = next(p for p in probe["patterns"] if p["name"] == "match0")
        for field in ("inferred_time_constant_s", "model_conductance_s"):
            with self.subTest(field=field):
                inconsistent = dict(matching)
                inconsistent[field] *= 1.1
                with self.assertRaisesRegex(AssertionError, "declared zero-wire circuit"):
                    VALIDATION.compare_pattern(probe, inconsistent)
        missing = dict(matching, inferred_time_constant_s=None)
        with self.assertRaisesRegex(ValueError, "cannot be reconstructed"):
            VALIDATION.compare_pattern(probe, missing)

    def test_precharge_diagnostic_distinguishes_short_and_long_strings(self):
        short = VALIDATION.compare_precharge(self.probes[68])
        long = VALIDATION.compare_precharge(self.probes[512])
        self.assertEqual(short["configured_precharge_s"], long["configured_precharge_s"])
        self.assertTrue(short["reaches_99_percent"])
        self.assertFalse(long["reaches_99_percent"])
        self.assertGreater(short["source_end_rail_fraction"], 0.99)
        self.assertLess(long["source_end_rail_fraction"], 0.02)
        self.assertLess(short["time_to_99_percent_s"], short["configured_precharge_s"])
        self.assertGreater(long["time_to_99_percent_s"], long["configured_precharge_s"])

    def test_end_to_end_audit_writes_evidence_and_preserves_unvalidated_status(self):
        output = self.root / "audit"
        report = VALIDATION.run_validation(output, PROBE, BINARY)
        saved = json.loads((output / "audit.json").read_text())
        self.assertEqual(saved, report)
        self.assertFalse(report["comparison"]["paper_reproduction_validated"])
        self.assertFalse(report["comparison"]["ready_to_attempt_reproduction"])
        self.assertEqual(report["baseline"]["calibration_status"], "synthetic")
        numerical = report["numerical_reference"]
        self.assertFalse(numerical["pointwise_nonlinear_device_validation"])
        self.assertGreater(numerical["margin_pass_disagreements"], 0)
        self.assertGreater(report["precharge_diagnostic"]["cases_below_99_percent"], 0)
        self.assertIn("synthetic", numerical["scope"])
        for name in ("baseline.yaml", "rc-comparison.csv", "waveforms.csv",
                     "baseline-energy-breakdown.csv", "rc-comparison.png", "rc-comparison.svg"):
            self.assertGreater((output / name).stat().st_size, 0, name)
        self.assertEqual((output / "rc-comparison.png").read_bytes()[:8], b"\x89PNG\r\n\x1a\n")

        with (output / "rc-comparison.csv").open() as source:
            rows = list(csv.DictReader(source))
        self.assertEqual(numerical["cases"], len(rows))
        witness = next(r for r in rows if r["wordlines"] == "512" and r["pattern"] == "match0")
        self.assertEqual(witness["model_margin_pass"], "True")
        self.assertEqual(witness["reference_margin_pass"], "False")
        baseline = yaml.safe_load((output / "baseline.yaml").read_text())
        geometry = baseline["geometry"]
        independent_normalization = (baseline["summary"]["energy"]["search_dynamic_j"] * 1e15
                                     / (geometry["entry_count"] * geometry["logical_word_width_bits"]))
        self.assertAlmostEqual(report["baseline"]["search_energy_fj_per_logical_bit"],
                               independent_normalization, places=12)
        self.assertTrue((output / "inputs/wl512/probe.yaml").is_file())
        for source, digest in report["source_sha256"].items():
            self.assertEqual(hashlib.sha256((ROOT / source).read_bytes()).hexdigest(), digest)
        for executable in (PROBE, BINARY):
            self.assertEqual(report["executable_sha256"][str(executable.resolve())],
                             hashlib.sha256(executable.read_bytes()).hexdigest())


if __name__ == "__main__":
    unittest.main()
