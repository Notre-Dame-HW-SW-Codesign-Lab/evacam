#!/usr/bin/env python3
"""Focused unit tests for the non-plot sweep, analysis, and table scripts."""

import csv
import importlib.util
import io
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]


def load_script(name: str):
    spec = importlib.util.spec_from_file_location(name, ROOT / "scripts" / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


MC = load_script("generate_variation_mc_sweep")
SWEEP = load_script("run_corner_sweep")
ANALYSIS = load_script("analyze_corner_sweep")
TABLE = load_script("generate_corner_variation_table")


def summary_row(run_id="on02_off00", on=2, off=0, margin_min=0.065):
    return {
        "run_id": run_id, "result_dir": f"runs/{run_id}", "corners": "2",
        "on_var_percent": str(on), "off_var_percent": str(off),
        "matchline_delay_s_nominal": "1e-12", "matchline_delay_s_min": "0.9e-12", "matchline_delay_s_max": "1.2e-12",
        "search_latency_s_nominal": "2e-12", "search_latency_s_min": "1.8e-12", "search_latency_s_max": "2.6e-12",
        "search_dynamic_energy_j_nominal": "3e-12", "search_dynamic_energy_j_min": "2.7e-12", "search_dynamic_energy_j_max": "3.9e-12",
        "exact_match_sense_margin_v_nominal": "0.08", "exact_match_sense_margin_v_min": str(margin_min), "exact_match_sense_margin_v_max": "0.09",
    }


class SweepScriptTest(unittest.TestCase):
    def test_mc_generator_replaces_variation_and_required_scalars(self):
        original = "name: old\nvariation:\n  mode: single_point\n  samples: 1\n"
        updated = MC.replace_or_append_variation_block(original, 7)
        self.assertIn("mode: monte_carlo", updated)
        self.assertIn("on_stdev: 7%", updated)
        self.assertNotIn("single_point", updated)
        self.assertEqual(MC.replace_scalar("name: old\n", "name", "new"), "name: new\n")
        with self.assertRaisesRegex(RuntimeError, "Could not replace"):
            MC.replace_scalar("other: value\n", "name", "new")

    def test_cli_parsing_and_spec_generation_selects_corners(self):
        self.assertEqual(SWEEP.split_cli_values([" on, off ", "2"]), ["on", "off", "2"])
        fields = SWEEP.parse_corner_fields(["on,memory_device_resistance_off_max_var"])
        self.assertEqual(fields, SWEEP.SUPPORTED_VARIATION_FIELDS)
        self.assertEqual(SWEEP.parse_corner_values(["4,0", "2", "4"]), (0, 2, 4))
        specs = SWEEP.all_specs(fields, (0, 2))
        self.assertEqual([spec.run_id for spec in specs], ["on00_off02", "on02_off00", "on02_off02"])
        self.assertEqual([spec.expected_corners for spec in specs], [2, 2, 4])
        self.assertEqual(SWEEP.selected_percent(specs[0], "on"), 0)
        with self.assertRaisesRegex(Exception, "duplicate"):
            SWEEP.parse_corner_fields(["on", "memory_device_resistance_on_max_var"])
        with self.assertRaisesRegex(Exception, "range"):
            SWEEP.parse_corner_values(["100"])

    def test_completion_resume_summary_and_atomic_csv(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            with mock.patch.object(SWEEP, "RUNS_ROOT", root / "runs"), mock.patch.object(SWEEP, "ROOT", root):
                spec = SWEEP.RunSpec(SWEEP.SUPPORTED_VARIATION_FIELDS, (2, 0))
                spec.run_dir.mkdir(parents=True)
                spec.result_path.write_text("result")
                with spec.samples_path.open("w", newline="") as handle:
                    writer = csv.DictWriter(handle, fieldnames=[*SWEEP.METRICS, "nominal_matchline_delay_s"])
                    writer.writeheader()
                    writer.writerow({"matchline_delay_s": "2", "search_latency_s": "3", "search_dynamic_energy_j": "4", "exact_match_sense_margin_v": "0.05", "nominal_matchline_delay_s": "1"})
                    writer.writerow({"matchline_delay_s": "1", "search_latency_s": "4", "search_dynamic_energy_j": "2", "exact_match_sense_margin_v": "0.06", "nominal_matchline_delay_s": "1"})
                self.assertTrue(SWEEP.completed(spec))
                row = SWEEP.summary_row(spec)
                self.assertEqual(row["corners"], 2)
                self.assertEqual(row["matchline_delay_s_min"], 1.0)
                self.assertEqual(row["matchline_delay_s_nominal"], "1")
                path = root / "out.csv"
                SWEEP.write_csv_atomic(path, ("name",), [{"name": "a,b"}])
                with path.open(newline="") as handle:
                    self.assertEqual(list(csv.DictReader(handle))[0]["name"], "a,b")
                self.assertFalse(path.with_suffix(".csv.tmp").exists())

    def test_run_one_propagates_tool_and_artifact_subprocess_failures(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            with mock.patch.object(SWEEP, "RUNS_ROOT", root / "runs"):
                spec = SWEEP.RunSpec((SWEEP.SUPPORTED_VARIATION_FIELDS[0],), (2,))
                spec.run_dir.mkdir(parents=True)
                with mock.patch.object(SWEEP.subprocess, "run", return_value=subprocess.CompletedProcess([], 9)) as run:
                    status, _elapsed, message = SWEEP.run_one(spec, force=True, artifacts=False)
                self.assertEqual((status, message), ("failed", "EvaCAM exited with code 9"))
                self.assertEqual(run.call_count, 1)
                with mock.patch.object(SWEEP, "completed", return_value=True), mock.patch.object(SWEEP, "generate_artifacts", side_effect=RuntimeError("table generation exited with code 4")), mock.patch.object(SWEEP.subprocess, "run", return_value=subprocess.CompletedProcess([], 0)):
                    status, _elapsed, message = SWEEP.run_one(spec, force=True, artifacts=True)
                self.assertEqual((status, message), ("failed", "table generation exited with code 4"))

    def test_main_generates_configs_or_runs_sweep(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fields = (SWEEP.SUPPORTED_VARIATION_FIELDS[0],)
            specs = SWEEP.all_specs(fields, (2,))
            with mock.patch.object(SWEEP, "RUNS_ROOT", root / "runs"), \
                 mock.patch.object(SWEEP, "MANIFEST_PATH", root / "manifest.csv"), \
                 mock.patch.object(SWEEP, "relative_to_root", side_effect=lambda path: str(path)), \
                 mock.patch.object(SWEEP, "generate_configs") as generate, \
                 mock.patch.object(SWEEP, "write_csv_atomic") as write:
                with mock.patch.object(sys, "argv", ["sweep", "--corner-fields", "on", "--corner-values", "2"]):
                    SWEEP.main()
                generate.assert_called_once_with(specs, overwrite=False)
                self.assertTrue(write.called)
                generate.reset_mock()
                with mock.patch.object(SWEEP, "run_sweep") as run, \
                     mock.patch.object(sys, "argv", ["sweep", "--run", "--jobs", "1", "--corner-fields", "on", "--corner-values", "2"]):
                    SWEEP.main()
                generate.assert_called_once_with(specs, overwrite=False)
                run.assert_called_once_with(specs, 1, False, False)

    def test_config_generation_artifacts_and_summary_helpers(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            templates = root / "templates"
            templates.mkdir()
            cell = templates / "cell.yaml"
            device = templates / "device.yaml"
            tool = templates / "tool.yaml"
            architecture = templates / "architecture.yaml"
            for path, text in ((cell, "memory_device: old.yaml\n"), (device, "variation:\n  mode: single_point\n"), (tool, "cell: old\narchitecture: old\ntechnology: old\n"), (architecture, "schema: architecture\n")):
                path.write_text(text, encoding="utf-8")
            runs = root / "runs"
            spec = SWEEP.RunSpec((SWEEP.SUPPORTED_VARIATION_FIELDS[0],), (2,))
            with mock.patch.object(SWEEP, "ROOT", root), \
                 mock.patch.object(SWEEP, "RUNS_ROOT", runs), \
                 mock.patch.object(SWEEP, "BASE_CELL", cell), \
                 mock.patch.object(SWEEP, "BASE_MEMORY_DEVICE", device), \
                 mock.patch.object(SWEEP, "BASE_TOOL", tool), \
                 mock.patch.object(SWEEP, "BASE_ARCHITECTURE", architecture), \
                 mock.patch.object(SWEEP, "SUMMARY_PATH", root / "summary.csv"), \
                 mock.patch.object(SWEEP, "MANIFEST_PATH", root / "manifest.csv"):
                SWEEP.generate_configs([spec], overwrite=False)
                self.assertIn("mode: corner", spec.memory_device_path.read_text())
                self.assertIn("memory_device: ./memory_device.yaml", spec.cell_path.read_text())
                self.assertEqual("pending", SWEEP.manifest_row(spec, "pending")["status"])
                spec.result_path.write_text("result", encoding="utf-8")
                with spec.samples_path.open("w", newline="") as handle:
                    writer = csv.DictWriter(handle, fieldnames=SWEEP.METRICS)
                    writer.writeheader()
                    writer.writerows({metric: "1" for metric in SWEEP.METRICS} for _ in range(spec.expected_corners))
                SWEEP.write_summary([spec])
                self.assertTrue((root / "summary.csv").is_file())
                with mock.patch.object(SWEEP.subprocess, "run", return_value=subprocess.CompletedProcess([], 0)) as run:
                    with (root / "log.txt").open("w") as log:
                        SWEEP.generate_artifacts(spec, log)
                self.assertEqual(2, run.call_count)
                executable = root / "EvaCAM"
                executable.write_text("", encoding="utf-8")
                with mock.patch.object(SWEEP, "EVA_CAM", executable), \
                     mock.patch.object(SWEEP, "run_one", return_value=("complete", 0.01, "")):
                    SWEEP.run_sweep([spec], jobs=1, force=True, artifacts=False)
                self.assertTrue((root / "manifest.csv").is_file())


class AnalysisAndTableScriptTest(unittest.TestCase):
    def test_percent_helpers_derived_metrics_ranking_and_validation(self):
        self.assertEqual(ANALYSIS.percent_range(8, 12, 10), 40.0)
        self.assertEqual(ANALYSIS.percent_deviation(8, 10), -20.0)
        self.assertTrue(math.isnan(ANALYSIS.percent_range(1, 2, 0)))
        rows = ANALYSIS.derive_rows([summary_row(), summary_row("on00_off02", 0, 2, 0.075)])
        self.assertAlmostEqual(rows[0]["matchline_delay_range_pct"], 30.0)
        self.assertAlmostEqual(rows[0]["minimum_exact_match_sense_margin_mv"], 65.0)
        self.assertEqual(ANALYSIS.ranked_rows(rows, "minimum_exact_match_sense_margin_mv", "min", 1)[0]["run_id"], "on02_off00")
        with self.assertRaisesRegex(RuntimeError, "incomplete"):
            ANALYSIS.validate_inputs([summary_row()], [{"status": "failed"}], False)
        with self.assertRaisesRegex(RuntimeError, "Non-finite"):
            ANALYSIS.as_float({"run_id": "bad", "x": "nan"}, "x")

    def test_analysis_main_generates_csv_rankings_and_report_without_plots(self):
        with tempfile.TemporaryDirectory() as temp:
            sweep_dir = Path(temp) / "sweep"
            sweep_dir.mkdir()
            rows = [summary_row(), summary_row("on00_off02", 0, 2, 0.075)]
            with (sweep_dir / "summary.csv").open("w", newline="") as handle:
                writer = csv.DictWriter(handle, fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)
            with (sweep_dir / "manifest.csv").open("w", newline="") as handle:
                writer = csv.DictWriter(handle, fieldnames=["status"]); writer.writeheader(); writer.writerows([{"status": "complete"}, {"status": "complete"}])
            output = sweep_dir / "report"
            with mock.patch.object(sys, "argv", ["analyze", "--sweep-dir", str(sweep_dir), "--output-dir", str(output), "--top", "1", "--no-plots"]):
                ANALYSIS.main()
            self.assertTrue((output / "derived_metrics.csv").is_file())
            self.assertTrue((output / "rankings" / "worst_matchline_delay_range_pct.csv").is_file())
            report = (output / "analysis_summary.md").read_text()
            self.assertIn("Runs below threshold: 1", report)
            self.assertNotIn("main_effects.svg", report)

    def test_analysis_plot_helpers_render_headlessly(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            rows = ANALYSIS.derive_rows([summary_row(), summary_row("on00_off02", 0, 2, 0.075)])
            effects = ANALYSIS.build_main_effects(rows)
            sensitivity = ANALYSIS.build_sensitivity(effects)
            self.assertTrue(sensitivity)
            plt = ANALYSIS.setup_matplotlib()
            ANALYSIS.plot_main_effects(effects, output, plt)
            ANALYSIS.plot_distributions(rows, output, plt)
            self.assertTrue((output / "main_effects.svg").is_file())
            self.assertTrue((output / "distributions.svg").is_file())

    def test_table_reads_metrics_ranks_escapes_and_writes_markdown(self):
        rows = [
            {"sample": "1", "corner_label": "a|b", "memory_device_res_on_corner": "low", "memory_device_res_off_corner": "high", "matchline_delay_s": "2e-12", "exact_match_sense_margin_v": "0.08"},
            {"sample": "0", "corner_label": "nominal", "memory_device_res_on_corner": "nominal", "memory_device_res_off_corner": "nominal", "matchline_delay_s": "1e-12", "exact_match_sense_margin_v": "0.06", "reference_delay_s": "3e-12"},
        ]
        self.assertEqual(TABLE.available_metrics(rows, False)[0][0], "matchline_delay_s")
        self.assertEqual(TABLE.bound_row(rows, "exact_match_sense_margin_v", 1e3, "min")["sample"], "0")
        markdown = TABLE.build_table(rows, include_internal=True)
        self.assertIn("matchline delay (max)", markdown)
        self.assertIn("reference delay", markdown)
        self.assertEqual(TABLE.default_output_path(Path("x_variation_samples.csv")), Path("x_variation_corner_table.md"))
        with tempfile.TemporaryDirectory() as temp:
            csv_path = Path(temp) / "samples.csv"
            with csv_path.open("w", newline="") as handle:
                fieldnames = list(dict.fromkeys(key for row in rows for key in row))
                writer = csv.DictWriter(handle, fieldnames=fieldnames); writer.writeheader(); writer.writerows(rows)
            self.assertEqual(len(TABLE.read_rows(csv_path)), 2)
            bad_path = Path(temp) / "bad.csv"
            bad_path.write_text("sample,corner_label\n0,nominal\n")
            with self.assertRaisesRegex(RuntimeError, "metadata"):
                TABLE.read_rows(bad_path)
            output = Path(temp) / "table.md"
            with mock.patch.object(sys, "argv", ["table", str(csv_path), "--output", str(output)]):
                TABLE.main()
            self.assertIn("Corner Variation Table", output.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
