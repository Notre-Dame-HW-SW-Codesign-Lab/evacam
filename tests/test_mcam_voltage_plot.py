"""Regression tests for MCAM voltage envelopes, copied inputs, and plot artifacts."""
import contextlib
import csv
import gzip
import importlib.util
import io
import json
import math
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

import numpy as np
import yaml

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('plot_mcam_voltage', ROOT / 'scripts/plot_mcam_voltage.py')
plot = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(plot)
STREAM_SPEC = importlib.util.spec_from_file_location(
    'mcam_composition_stream', ROOT / 'scripts/mcam_composition_stream.py')
stream_plot = importlib.util.module_from_spec(STREAM_SPEC)
sys.modules[STREAM_SPEC.name] = stream_plot
STREAM_SPEC.loader.exec_module(stream_plot)
RUNNER_SPEC = importlib.util.spec_from_file_location(
    'run_mcam_32', ROOT / 'scripts/run_mcam_32.py')
run32 = importlib.util.module_from_spec(RUNNER_SPEC)
with mock.patch.dict(sys.modules, {'plot_mcam_voltage': plot,
                                   'mcam_composition_stream': stream_plot}):
    RUNNER_SPEC.loader.exec_module(run32)
CONFIG_ROOT = ROOT / 'config/2FeFET_MCAM_variation'
SOURCE = CONFIG_ROOT / 'stdev05/2FeFET_MCAM_8x8.config.yaml'


def bound(distance, lower, upper, minimum_counts=(), maximum_counts=()):
    return SimpleNamespace(squared_euclidean_distance=distance, minimum_voltage=lower,
                           maximum_voltage=upper, minimum_conductance=0.01, maximum_conductance=0.02,
                           minimum_search_latency=lower * 1e-9, maximum_search_latency=upper * 1e-9,
                           minimum_conductance_delta_counts=minimum_counts,
                           maximum_conductance_delta_counts=maximum_counts)


class FakeMatcher:
    def __init__(self, config=None):
        pass

    def vector_dimensions(self):
        return 2

    def evaluate_distance(self, stored, query):
        distance = sum((a - b)**2 for a, b in zip(stored, query))
        return SimpleNamespace(squared_euclidean_distance=distance,
                               matchline_conductance=0.01, matchline_voltage=0.5 - distance * 0.01,
                               search_latency=(0.5 - distance * 0.01) * 1e-9, matchline_delay=0.1e-9)

    def evaluate_distance_samples(self, stored, query):
        base = self.evaluate_distance(stored, query)
        return [SimpleNamespace(**{**vars(base), 'matchline_voltage': base.matchline_voltage + shift})
                for shift in (-0.001, 0.001)]

    def distance_voltage_bounds(self, query, include_variation=True):
        padding = 0.01 if include_variation else 0
        return [bound(i, 0.5 - i * 0.004 - padding, 0.5 - i * 0.004 + padding)
                for i in range(99)]


class StreamFakeMatcher:
    def vector_dimensions(self):
        return 4

    def evaluate_zero_query_composition(self, counts, resistance_sigma_offset=0):
        distance = sum(count * delta * delta for delta, count in enumerate(counts))
        conductance = .001 + sum(count * delta for delta, count in enumerate(counts)) * .0001
        voltage = 1.0 - conductance * 100
        latency = (1.0 + conductance * 10) * 1e-9
        return SimpleNamespace(squared_euclidean_distance=distance,
                               matchline_conductance=conductance,
                               matchline_voltage=voltage,
                               search_latency=latency,
                               matchline_delay=latency / 2)


def stream_fake_bounds(matcher, states=3):
    grouped = {}
    for counts in stream_plot.enumerate_delta_counts(matcher.vector_dimensions(), states):
        result = matcher.evaluate_zero_query_composition(counts)
        grouped.setdefault(result.squared_euclidean_distance, []).append((counts, result))
    bounds = []
    for distance, rows in sorted(grouped.items()):
        by_conductance = sorted(rows, key=lambda row: row[1].matchline_conductance)
        values = [row[1] for row in rows]
        bounds.append(SimpleNamespace(
            squared_euclidean_distance=distance,
            minimum_conductance=by_conductance[0][1].matchline_conductance,
            maximum_conductance=by_conductance[-1][1].matchline_conductance,
            minimum_voltage=min(row.matchline_voltage for row in values),
            maximum_voltage=max(row.matchline_voltage for row in values),
            minimum_search_latency=min(row.search_latency for row in values),
            maximum_search_latency=max(row.search_latency for row in values),
            minimum_conductance_delta_counts=by_conductance[0][0],
            maximum_conductance_delta_counts=by_conductance[-1][0]))
    return bounds


class VoltagePlotTest(unittest.TestCase):
    def test_streaming_multiplicity_uses_exact_large_integers(self):
        permutations = stream_plot.coordinate_permutations((4,) * 8)
        self.assertIs(type(permutations), int)
        self.assertEqual(permutations, 2390461829733887910000000)

    def test_streamed_nominal_csv_groups_extrema_and_selection(self):
        matcher = StreamFakeMatcher()
        bounds = stream_fake_bounds(matcher)
        witnesses = {tuple(getattr(item, field)) for item in bounds for field in
                     ('minimum_conductance_delta_counts', 'maximum_conductance_delta_counts')}
        with tempfile.TemporaryDirectory() as temp:
            result = stream_plot.build_nominal_stream(
                matcher, Path(temp), 3, bounds,
                {'voltage': 1000, 'latency': 10}, budget=2,
                progress_interval=0, plot_y_bins=8)
            self.assertEqual(result.total_count, math.comb(6, 2))
            self.assertFalse(hasattr(result, 'nominal'))
            self.assertEqual(sum(int(row[2]) for row in result.groups_by_metric['voltage']),
                             result.total_count)
            self.assertEqual(sum(int(row[2]) for row in result.groups_by_metric['latency']),
                             result.total_count)
            for metric, lower_name, upper_name, scale in (
                    ('voltage', 'minimum_voltage', 'maximum_voltage', 1e3),
                    ('latency', 'minimum_search_latency', 'maximum_search_latency', 1e9)):
                representatives = result.representatives_by_metric[metric]
                by_distance = {}
                for row in representatives:
                    by_distance.setdefault(int(row[0]), []).append(float(row[1]))
                for item in bounds:
                    self.assertEqual(sorted(by_distance[item.squared_euclidean_distance]),
                                     sorted([getattr(item, lower_name) * scale,
                                             getattr(item, upper_name) * scale]))
            self.assertTrue(witnesses.issubset({row.counts for row in result.selected}))
            self.assertTrue(any(sum(count > 0 for count in row.counts[1:]) >= 2
                                for row in result.selected))
            with gzip.open(Path(temp)/'nominal_compositions.csv.gz', 'rt') as source:
                rows = list(csv.DictReader(source))
            self.assertEqual(len(rows), result.total_count)
            for metric in ('voltage', 'latency'):
                with (Path(temp)/f'nominal_{metric}_plot_groups.csv').open() as source:
                    grouped = list(csv.DictReader(source))
                self.assertEqual(sum(int(row['composition_count']) for row in grouped),
                                 result.total_count)
            with self.assertRaisesRegex(ValueError, 'complete nominal reachability'):
                stream_plot.build_nominal_stream(
                    matcher, Path(temp)/'bad', 3, bounds[:-1],
                    {'voltage': 1000, 'latency': 10}, budget=2,
                    progress_interval=0, plot_y_bins=8)

    def test_smooth_bounds_pass_through_every_reachable_extremum(self):
        x = np.arange(9)
        lower = np.array([90, 15, 80, 5, 55, 10, 20, 0, 0])
        upper = np.array([100, 85, 90, 70, 80, 25, 60, 5, 0])
        envelope = plot.smooth_voltage_range((x, lower, upper))
        np.testing.assert_allclose(np.interp(x, envelope[0], envelope[1]), lower)
        np.testing.assert_allclose(np.interp(x, envelope[0], envelope[2]), upper)
        self.assertTrue(np.all(envelope[1] <= envelope[2]))
        self.assertTrue(np.all(envelope[1] >= 0))

    def test_bounds_gaps_singletons_and_zero_ranges(self):
        grid = plot.bounds_arrays([bound(0, .3, .4), bound(3, .1, .2)])
        self.assertTrue(np.isnan(grid[1][1:3]).all())
        envelope = plot.smooth_voltage_range(grid)
        plot.validate_containment([0, 3], [300, 100], envelope)
        plot.validate_containment([0, 3], [400, 200], envelope)
        for original in (([0], [0], [0]), ([0], [100], [200]),
                         ([0, 1, 2], [0, 0, 0], [0, 0, 0])):
            result = plot.smooth_voltage_range(original)
            plot.validate_containment(original[0], original[1], result)
            plot.validate_containment(original[0], original[2], result)
        with self.assertRaises(ValueError):
            plot.bounds_arrays([])
        for invalid in (([0], [2], [1]), ([0], [np.nan], [np.nan])):
            with self.assertRaises(ValueError):
                plot.smooth_voltage_range(invalid)

    def test_exhaustive_delta_counts_include_mixed_compositions_and_multiplicity(self):
        for dimensions, expected in ((8, 6435), (16, 245157)):
            compositions = list(plot.enumerate_delta_counts(dimensions, 8))
            self.assertEqual(len(compositions), expected)
            self.assertTrue(all(len(counts) == 8 and sum(counts) == dimensions
                                for counts in compositions))
        rows = plot.evaluate_nominal_compositions(FakeMatcher(), 2, 3)
        self.assertEqual(len(rows), math.comb(4, 2))
        self.assertEqual(sum(permutations for _, permutations, _ in rows), 3**2)
        mixed = next(row for row in rows if row[0] == (0, 1, 1))
        self.assertEqual(mixed[1], 2)
        self.assertEqual(mixed[2].squared_euclidean_distance, 5)

    def test_sample_selection_keeps_both_bound_witnesses_even_over_budget(self):
        rows = plot.evaluate_nominal_compositions(FakeMatcher(), 2, 3)
        low = (2, 0, 0)
        high = (0, 0, 2)
        selected = plot.select_sample_compositions(
            rows, [bound(0, 0, 1, low, high)], budget=1)
        selected_counts = {rows[index][0] for index in selected}
        self.assertEqual(selected_counts, {low, high})

    def test_containment_rejects_excluded_nonfinite_and_outside_points(self):
        envelope = (np.array([0, 1]), np.array([1, 1]), np.array([2, 2]))
        for x, y in (([0], [0]), ([1], [3]), ([-1], [1]), ([2], [1]),
                     ([np.nan], [1]), ([0], [np.inf])):
            with self.subTest(x=x, y=y), self.assertRaises(ValueError):
                plot.validate_containment(x, y, envelope)

    def test_prepare_preserves_source_and_resolves_references(self):
        original = {p: p.read_bytes() for p in SOURCE.parent.glob('*.yaml')}
        with tempfile.TemporaryDirectory() as temp:
            path, memory = plot.prepare_config(SOURCE, Path(temp)/'varied', samples=7,
                                               seed=123, granularity='effective')
            self.assertEqual(memory['variation'], dict(mode='monte_carlo', samples=7,
                                                      seed=123, monte_carlo_granularity='effective'))
            config = yaml.safe_load(path.read_text())
            for key in ('architecture', 'cell'):
                self.assertTrue((path.parent/config[key]).is_file())
            self.assertEqual(Path(config['technology']), ROOT/'config/lib/technology/cmos.legacy.yaml')
            sensing = yaml.safe_load((path.parent/'sensing.yaml').read_text())
            self.assertTrue(Path(sensing['sense_amplifier']).is_file())
            _, nominal = plot.prepare_config(SOURCE, Path(temp)/'nominal', nominal=True)
            self.assertNotIn('variation', nominal)
            self.assertEqual(nominal['mcam'], memory['mcam'])
        self.assertEqual(original, {p: p.read_bytes() for p in original})

    def test_copied_config_tree_has_all_sizes_and_controls(self):
        configs = list(CONFIG_ROOT.glob('*/*.config.yaml'))
        self.assertEqual(len(configs), 15)
        for level in (0, 5, 10):
            directory = CONFIG_ROOT/f'stdev{level:02d}'
            memory = yaml.safe_load((directory/'2FeFET_MCAM.memory_device.yaml').read_text())
            self.assertEqual(memory['mcam']['state_variation'], [f'{level}%']*8)
            self.assertEqual(memory['variation'], dict(mode='monte_carlo', samples=1000,
                                                      seed=9876, monte_carlo_granularity='cell'))
            for size in (8, 16, 32, 64, 128):
                config = yaml.safe_load((directory/f'2FeFET_MCAM_{size}x{size}.config.yaml').read_text())
                arch = yaml.safe_load((directory/config['architecture']).read_text())
                self.assertEqual(arch['memory']['vector_dimensions'], size)
                self.assertEqual(arch['organization']['comparison_columns_per_step'], size)
                self.assertEqual(arch['organization']['subarray']['dimensions'], [size, size])
            for path in directory.glob('*.yaml'):
                data = yaml.safe_load(path.read_text())
                for key in ('architecture', 'cell', 'technology', 'memory_device', 'sensing', 'sense_amplifier'):
                    if key in data and isinstance(data[key], str):
                        self.assertTrue((path.parent/data[key]).is_file(), str(path))

    def test_samples_and_bounds_csv_preserve_raw_values(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            nominal, x, mv, ns, selected = plot.generate_samples(
                FakeMatcher(), FakeMatcher(), directory, 3, sample_budget=6)
            self.assertEqual(nominal.shape, (6, 7))
            self.assertEqual(selected, 6)
            self.assertGreaterEqual(len(x), 12)
            self.assertIn(5, x)
            exact_index = int(np.flatnonzero(x == 0)[0])
            self.assertAlmostEqual(mv[exact_index], 499)
            self.assertAlmostEqual(ns[exact_index], .5)
            with gzip.open(directory/'samples.csv.gz', 'rt') as stream:
                rows = list(csv.DictReader(stream))
            self.assertGreaterEqual(len(rows), 12)
            exact_row = next(row for row in rows if row['squared_euclidean_distance'] == '0')
            self.assertAlmostEqual(float(exact_row['matchline_voltage_v']), .499)
            self.assertEqual(float(exact_row['search_latency_s']), .5e-9)
            self.assertEqual(float(exact_row['matchline_delay_s']), .1e-9)
            self.assertIn('placement_index', rows[0])
            with (directory/'nominal_compositions.csv').open() as stream:
                nominal_rows = list(csv.DictReader(stream))
            self.assertEqual(len(nominal_rows), 6)
            mixed = next(row for row in nominal_rows
                         if row['delta_1_count'] == '1' and row['delta_2_count'] == '1')
            self.assertEqual(mixed['coordinate_permutations'], '2')
            self.assertEqual(mixed['squared_euclidean_distance'], '5')
            base = [bound(0, .4, .5, (2, 0, 0), (2, 0, 0))]
            varied = [bound(0, .3, .6, (1, 1, 0), (0, 2, 0))]
            plot.write_bounds(directory, base, varied)
            with (directory/'voltage_bounds.csv').open() as stream:
                row = next(csv.DictReader(stream))
            self.assertEqual(float(row['variation_minimum_voltage_v']), .3)
            self.assertEqual(float(row['nominal_maximum_voltage_v']), .5)
            self.assertEqual(json.loads(row['minimum_conductance_delta_counts']), [1, 1, 0])
            self.assertEqual(json.loads(row['maximum_conductance_delta_counts']), [0, 2, 0])
            with (directory/'latency_bounds.csv').open() as stream:
                row = next(csv.DictReader(stream))
            self.assertEqual(float(row['variation_minimum_search_latency_s']), .3e-9)
            latency_range = plot.bounds_arrays(varied, 'latency')
            np.testing.assert_allclose(latency_range[1:], [[.3], [.6]])
            for bad in ([], [bound(1, .3, .6)]):
                with self.assertRaises(ValueError):
                    plot.write_bounds(directory, base, bad)

    def test_display_grouping_combines_visually_shared_points_and_conserves_counts(self):
        nominal = np.array([
            [5, .01, .1000, .20e-9, .1e-9, 1, 2],
            [5, .01, .1004, .20e-9, .1e-9, 2, 3],
            [5, .01, .1010, .20e-9, .1e-9, 1, 4],
        ])
        with tempfile.TemporaryDirectory() as temp:
            groups = plot.group_nominal_for_metric(nominal, 'voltage', 1000, Path(temp))
            self.assertEqual(len(groups), 2)
            self.assertEqual(int(np.sum(groups[:, 2])), 3)
            self.assertEqual(int(np.sum(groups[:, 3])), 9)
            shared = groups[groups[:, 2] == 2][0]
            self.assertAlmostEqual(shared[1], 100.2)
            self.assertEqual(shared[4], 2)
            with (Path(temp)/'nominal_voltage_plot_groups.csv').open() as stream:
                exported = list(csv.DictReader(stream))
            self.assertEqual(sum(int(row['composition_count']) for row in exported), 3)

    def test_generate_run_metadata_and_nested_envelopes(self):
        with tempfile.TemporaryDirectory() as temp, mock.patch.dict('sys.modules',
                {'evacam_py': SimpleNamespace(EvaCAMMatch=FakeMatcher)}):
            directory = Path(temp)
            _, _, _, nominal, varied, _, _ = plot.generate_run(SOURCE, directory, samples=4, seed=42)
            self.assertTrue(np.all(varied[1] <= nominal[1]))
            self.assertTrue(np.all(varied[2] >= nominal[2]))
            metadata = json.loads((directory/'metadata.json').read_text())
            self.assertEqual(metadata['variation']['seed'], 42)
            self.assertEqual(metadata['variation']['samples'], 4)
            self.assertEqual(metadata['query'], [0, 0])
            self.assertEqual(metadata['sensing'], 'fixed nominal sensing instant')
            self.assertTrue((directory/'voltage_bounds.csv').is_file())

    def test_plot_saves_all_formats_and_active_region(self):
        nominal = np.array([[0, .01, .5, .5e-9, .1e-9, 0, 1],
                            [1, .01, .1, .1e-9, .1e-9, 1, 2],
                            [2, .01, 0, 0, .1e-9, 1, 1]])
        self.assertEqual(plot.active_region_distance(nominal), 1)
        groups = np.array([[0, 500., 1, 2390461829733887910000000, 0],
                           [1, 100., 2, 2, 1], [2, 0., 1, 1, 1]], dtype=object)
        envelope = (np.arange(3), np.zeros(3), np.full(3, 500))
        exact_bounds = [bound(i, 0, .5) for i in range(3)]
        with tempfile.TemporaryDirectory() as temp, mock.patch('matplotlib.figure.Figure.savefig') as save:
            for metric in ('voltage', 'latency'):
                for active in (False, True):
                    plot.plot_voltage(Path(temp), 2, 'test', nominal, groups, np.array([0, 1]),
                                      np.array([499, 99]), envelope, envelope, 2, 500,
                                      exact_bounds, active=active, metric=metric)
            self.assertEqual([call.args[0].name for call in save.call_args_list],
                             [f'{stem}.{ext}' for stem in ('mcam_voltage', 'mcam_voltage_active_region',
                                                         'mcam_latency', 'mcam_latency_active_region')
                              for ext in ('svg', 'pdf', 'png')])

    def test_cli_rejects_invalid_sample_counts_and_seeds_before_generation(self):
        with mock.patch.object(plot, 'generate_run') as generate:
            for args in (['--samples', '1'], ['--samples', '-1'], ['--seed', '-1'],
                         ['--seed', str(2**32)], ['--sample-compositions', '0'], ['--sizes', '32']):
                with self.subTest(args=args), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                    plot.main(['--mode', 'support-bounds', *args])
                self.assertEqual(error.exception.code, 2)
            generate.assert_not_called()

    def test_32_runner_defaults_and_has_no_larger_size_option(self):
        with mock.patch.object(run32, 'run') as run:
            run32.main([])
            args = run.call_args.args[0]
            self.assertEqual(args.levels, [0, 5, 10])
            self.assertEqual(args.samples, 1000)
            self.assertEqual(args.seed, 9876)
            self.assertEqual(args.sample_compositions, 2000)
            self.assertEqual(args.output_dir.name, 'mcam_state_variation_32x32')
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
            run32.main(['--sizes', '64'])
        self.assertEqual(error.exception.code, 2)


if __name__ == '__main__':
    unittest.main()
