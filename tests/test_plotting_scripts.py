#!/usr/bin/env python3
"""Focused unit tests for EvaCAM plotting and normality scripts."""

import csv
import importlib.util
import io
import math
import os
from pathlib import Path
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from unittest import mock


os.environ.setdefault("MPLBACKEND", "Agg")
os.environ.setdefault("MPLCONFIGDIR", "/tmp/evacam-matplotlib-tests")
ROOT = Path(__file__).resolve().parents[1]


def load_script(name):
    module_name = f"phase7_{name}"
    spec = importlib.util.spec_from_file_location(module_name, ROOT / "scripts" / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


HIST = load_script("plot_variation_histograms")
QQ = load_script("plot_variation_qq")
CORNER = load_script("plot_corner_variation")
POSTER = load_script("poster_figures")
NORMALITY = load_script("test_variation_normality")


def variation_rows():
    return [
        {
            "sample": str(index),
            "matchline_delay_s": f"{1.0 + index * 0.1}e-12",
            "search_latency_s": f"{2.0 + index * 0.1}e-12",
            "search_dynamic_energy_j": f"{3.0 + index * 0.1}e-12",
            "exact_match_sense_margin_v": f"{0.08 - index * 0.001}",
            "reference_delay_s": f"{4.0 + index * 0.1}e-12",
            "nominal_reference_delay_s": "4e-12",
            "nominal_matchline_delay_s": "1e-12",
            "nominal_search_latency_s": "2e-12",
            "nominal_search_dynamic_energy_j": "3e-12",
            "nominal_exact_match_sense_margin_v": "0.08",
        }
        for index in range(4)
    ]


def corner_rows():
    rows = variation_rows()
    corners = [("low", "low"), ("low", "high"), ("high", "low"), ("high", "high")]
    for row, (on_corner, off_corner) in zip(rows, corners):
        row["memory_device_res_on_corner"] = on_corner
        row["memory_device_res_off_corner"] = off_corner
        row["corner_label"] = f"{on_corner}/{off_corner}"
    return rows


def write_csv(path, rows):
    with path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


class HistogramAndQqPlotTest(unittest.TestCase):
    def test_readers_columns_metric_discovery_statistics_and_default_paths(self):
        rows = variation_rows()
        rows[0]["search_latency_s"] = ""
        self.assertEqual([round(value, 8) for value in HIST.column_values(rows, "matchline_delay_s", 1e12)], [1.0, 1.1, 1.2, 1.3])
        self.assertEqual([round(value, 8) for value in QQ.column_values(rows, "search_latency_s", 1e12)], [2.1, 2.2, 2.3])
        self.assertEqual([metric[0] for metric in HIST.available_metrics(rows, False)], [
            "Matchline delay", "Search latency", "Search dynamic energy", "Exact Match Sense Margin",
        ])
        self.assertEqual(HIST.available_metrics(rows, True)[-1][0], "Reference delay")
        self.assertEqual(QQ.available_metrics(rows)[0][1], "ps")
        self.assertEqual(HIST.metric_stats([1, 2, 3]), (2.0, math.sqrt(2 / 3)))
        self.assertEqual(HIST.format_stat(1.2345, "ps"), "1.23 ps")
        self.assertEqual(HIST.default_output_path(Path("out/demo_variation_samples.csv")), Path("out/demo_variation_histograms.svg"))
        self.assertEqual(QQ.default_output_path(Path("out/demo.csv")), Path("out/demo_qq_plots.svg"))

    def test_read_samples_rejects_empty_and_metric_discovery_rejects_unknown_or_malformed(self):
        with tempfile.TemporaryDirectory() as temporary:
            empty = Path(temporary) / "empty.csv"
            empty.write_text("matchline_delay_s\n", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "No sample rows"):
                HIST.read_samples(empty)
            with self.assertRaisesRegex(RuntimeError, "No sample rows"):
                QQ.read_samples(empty)
        with self.assertRaisesRegex(RuntimeError, "No known"):
            HIST.available_metrics([{"unrelated": "1"}], False)
        with self.assertRaises(ValueError):
            QQ.column_values([{"matchline_delay_s": "not-a-number"}], "matchline_delay_s", 1)

    def test_histogram_and_qq_headless_smoke_render(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            HIST.plot_histograms(variation_rows(), directory / "nested" / "hist.svg", 3, True)
            QQ.plot_qq(variation_rows(), directory / "qq.svg")
            for output in (directory / "nested" / "hist.svg", directory / "qq.svg"):
                self.assertTrue(output.is_file())
                self.assertIn("<svg", output.read_text(encoding="utf-8"))

    def test_histogram_main_validates_bins_and_uses_default_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            csv_path = Path(temporary) / "input_variation_samples.csv"
            write_csv(csv_path, variation_rows())
            with mock.patch.object(sys, "argv", ["plot", str(csv_path), "--bins", "2"]):
                HIST.main()
            self.assertTrue((Path(temporary) / "input_variation_histograms.svg").is_file())
            with mock.patch.object(sys, "argv", ["plot", str(csv_path), "--bins", "0"]):
                with self.assertRaisesRegex(RuntimeError, "greater than 0"):
                    HIST.main()

    def test_qq_main_uses_default_output_path(self):
        with tempfile.TemporaryDirectory() as temporary:
            csv_path = Path(temporary) / "input_variation_samples.csv"
            write_csv(csv_path, variation_rows())
            with mock.patch.object(sys, "argv", ["qq", str(csv_path)]):
                QQ.main()
            self.assertTrue((Path(temporary) / "input_variation_qq_plots.svg").is_file())


class CornerAndPosterPlotTest(unittest.TestCase):
    def test_corner_points_bounds_codes_ordering_formatting_and_paths(self):
        rows = corner_rows()
        points = CORNER.metric_points(rows, "matchline_delay_s", 1e12)
        self.assertEqual([point["sample"] for point in CORNER.ordered_points(points, "sample")], [0, 1, 2, 3])
        self.assertEqual([CORNER.corner_code(point) for point in CORNER.ordered_points(points, "corner")], ["LL", "LH", "HL", "HH"])
        self.assertAlmostEqual(CORNER.bound_point(points, "max")["value"], 1.3)
        self.assertAlmostEqual(CORNER.bound_point(points, "min")["value"], 1.0)
        self.assertEqual(CORNER.opposite_bound("max"), "min")
        self.assertEqual(CORNER.format_value(1.2345, "mV"), "1.23 mV")
        self.assertEqual(CORNER.nominal_value(rows, "matchline_delay_s", 1e12), 1.0)
        self.assertEqual(CORNER.default_output_path(Path("x_variation_samples.csv"), "sample"), Path("x_variation_sample_order_corner.svg"))
        fallback = dict(points[0]); fallback["row"] = {"memory_device_res_on_corner": "other", "memory_device_res_off_corner": "other"}; fallback["corner_label"] = "custom"
        self.assertEqual(CORNER.corner_code(fallback), "custom")

    def test_corner_reader_and_metric_errors(self):
        with tempfile.TemporaryDirectory() as temporary:
            empty = Path(temporary) / "empty.csv"
            empty.write_text("corner_label\n", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "No corner sample rows"):
                CORNER.read_rows(empty)
            malformed = Path(temporary) / "malformed.csv"
            malformed.write_text("corner_label,memory_device_res_on_corner\nx,low\n", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "missing corner metadata"):
                CORNER.read_rows(malformed)
        with self.assertRaisesRegex(RuntimeError, "nominal metric"):
            CORNER.nominal_value([{ "matchline_delay_s": "1" }], "matchline_delay_s", 1)
        with self.assertRaisesRegex(RuntimeError, "No known"):
            CORNER.available_metrics([{"corner_label": "x"}], False)

    def test_corner_and_poster_headless_smoke_renders(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            rows = corner_rows()
            CORNER.plot_corner_variation(rows, directory / "corner.svg", True, "metric")
            POSTER.plot_monte_carlo(variation_rows(), directory / "poster_mc.svg", 72)
            POSTER.plot_corner(rows, directory / "poster_corner.svg", 72)
            for svg in (directory / "corner.svg", directory / "poster_mc.svg", directory / "poster_corner.svg"):
                self.assertTrue(svg.is_file())
                self.assertIn("<svg", svg.read_text(encoding="utf-8"))
            self.assertTrue((directory / "poster_mc.png").is_file())
            self.assertTrue((directory / "poster_corner.png").is_file())

    def test_corner_and_poster_entry_points_render_from_real_csvs(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            corner_csv = directory / "corner_variation_samples.csv"
            monte_carlo_csv = directory / "mc_variation_samples.csv"
            write_csv(corner_csv, corner_rows())
            write_csv(monte_carlo_csv, variation_rows())
            self.assertEqual(len(POSTER.read_rows(monte_carlo_csv)), 4)
            with mock.patch.object(sys, "argv", ["corner", str(corner_csv), "--order", "sample"]):
                CORNER.main()
            output_dir = directory / "poster"
            with mock.patch.object(sys, "argv", [
                "poster", "--monte-carlo", str(monte_carlo_csv), "--corner", str(corner_csv),
                "--output-dir", str(output_dir), "--png-dpi", "30",
            ]):
                POSTER.main()
            self.assertTrue((directory / "corner_variation_sample_order_corner.svg").is_file())
            self.assertTrue((output_dir / "monte_carlo_headline_metrics.svg").is_file())
            with mock.patch.object(sys, "argv", [
                "poster", "--monte-carlo", str(monte_carlo_csv), "--corner", str(corner_csv), "--png-dpi", "0",
            ]):
                with self.assertRaisesRegex(RuntimeError, "greater than zero"):
                    POSTER.main()

    def test_poster_readers_labels_percent_bounds_and_errors(self):
        rows = corner_rows()
        self.assertEqual(POSTER.values(rows, "search_latency_s", 1e12)[0], 2.0)
        self.assertEqual(POSTER.nominal(rows, "search_latency_s", 1e12), 2.0)
        self.assertEqual([metric[0] for metric in POSTER.metric_rows(rows)], [metric[0] for metric in POSTER.METRICS])
        self.assertEqual(POSTER.percent_delta(12, 10), 20.0)
        self.assertTrue(math.isnan(POSTER.percent_delta(1, 0)))
        self.assertEqual(POSTER.corner_label(rows[0]), "LL")
        self.assertEqual(POSTER.corner_label({}), "??")
        with self.assertRaisesRegex(RuntimeError, "No values"):
            POSTER.values(rows, "missing", 1)
        with self.assertRaisesRegex(RuntimeError, "Missing nominal"):
            POSTER.nominal(rows, "missing", 1)
        with self.assertRaisesRegex(RuntimeError, "Corner CSV"):
            POSTER.plot_corner(variation_rows(), Path("/tmp/unused.svg"), 72)


class NormalityScriptTest(unittest.TestCase):
    def test_readers_metric_discovery_and_statistical_decision_boundaries(self):
        rows = variation_rows()
        self.assertEqual(NORMALITY.column_values(rows, "matchline_delay_s"), [1e-12, 1.1e-12, 1.2e-12, 1.3e-12])
        self.assertEqual([metric[0] for metric in NORMALITY.available_metrics(rows)], [
            "matchline_delay_s", "search_latency_s", "search_dynamic_energy_j", "exact_match_sense_margin_v",
        ])
        self.assertEqual(NORMALITY.available_metrics(rows, True)[-1][0], "reference_delay_s")
        self.assertEqual(NORMALITY.normality_decision(0.049, 0.05), "reject normality")
        self.assertEqual(NORMALITY.normality_decision(0.05, 0.05), "do not reject normality")

    def test_empty_malformed_short_and_main_decisions(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            empty = directory / "empty.csv"
            empty.write_text("matchline_delay_s\n", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "No sample rows"):
                NORMALITY.read_samples(empty)
            short = directory / "short.csv"
            write_csv(short, variation_rows()[:2])
            with mock.patch.object(sys, "argv", ["normality", str(short)]):
                with self.assertRaisesRegex(RuntimeError, "at least 3"):
                    NORMALITY.main()
            csv_path = directory / "samples.csv"
            write_csv(csv_path, variation_rows())
            output = io.StringIO()
            with mock.patch.object(sys, "argv", ["normality", str(csv_path), "--alpha", "0.05"]), \
                 mock.patch.object(NORMALITY, "shapiro", return_value=(0.9, 0.04)) as shapiro, \
                 redirect_stdout(output):
                NORMALITY.main()
            self.assertEqual(shapiro.call_count, 4)
            self.assertIn("reject normality", output.getvalue())
            with mock.patch.object(sys, "argv", ["normality", str(csv_path), "--alpha", "1"]):
                with self.assertRaisesRegex(RuntimeError, "between 0 and 1"):
                    NORMALITY.main()
        with self.assertRaises(ValueError):
            NORMALITY.column_values([{ "matchline_delay_s": "bad" }], "matchline_delay_s")


if __name__ == "__main__":
    unittest.main()
