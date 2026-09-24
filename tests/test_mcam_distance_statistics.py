import itertools
import csv
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, str(ROOT))
import evacam_py
import plot_mcam_distance_statistics as plot
from plot_mcam_distance_statistics import completion_counts, sample_vectors, summarize, varied_conductances
from plot_mcam_voltage import prepare_config


class DistanceStatisticsTests(unittest.TestCase):
    def test_presentation_reuses_points_but_replaces_statistical_bands(self):
        records = [{'squared_euclidean_distance': distance,
                    'possible_ordered_vectors': 8**32,
                    **summarize(np.array([.45, .48, .5]) / (distance + 1))}
                   for distance in range(3)]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'source/stdev05/32x32'
            source.mkdir(parents=True)
            prepare_config(ROOT / 'config/2FeFET_MCAM_variation/stdev05/2FeFET_MCAM_32x32.config.yaml',
                           source / 'inputs', samples=2)
            extrema = [dict(distance=r['squared_euclidean_distance'],
                            nominal_min_voltage_v=.46 / (i + 1), nominal_max_voltage_v=.49 / (i + 1),
                            min_voltage_v=.4 / (i + 1), max_voltage_v=.55 / (i + 1))
                       for i, r in enumerate(records)]
            csv_path = source / 'distance_statistics.csv'
            with csv_path.open('w', newline='') as stream:
                writer = csv.DictWriter(stream, fieldnames=list(records[0]))
                writer.writeheader()
                writer.writerows(records)
            original = csv_path.read_bytes()
            figures = []
            def capture(figure, *args, **kwargs):
                if figure not in figures:
                    figures.append(figure)
            with mock.patch('matplotlib.figure.Figure.savefig', autospec=True, side_effect=capture), \
                    mock.patch.object(plot, 'generate') as generate, \
                    mock.patch.object(plot, 'generate_nominal') as nominal, \
                    mock.patch.object(plot, 'plot_all_points') as points, \
                    mock.patch('plot_cam_extrema.calculate_extrema', return_value=extrema):
                plot.main(['--presentation-from', str(root / 'source'), '--output-dir', str(root / 'tv'),
                           '--sizes', '32', '--levels', '5'])
                generate.assert_not_called()
                nominal.assert_not_called()
                points.assert_not_called()
            self.assertEqual((root / 'tv/stdev05/32x32/distance_statistics.csv').read_bytes(), original)
            self.assertEqual(csv_path.read_bytes(), original)
            self.assertEqual(len(figures), 2)
            for figure in figures:
                self.assertEqual(len(figure.axes), 1)  # No point colorbar.
                axis = figure.axes[0]
                self.assertEqual(len(axis.collections), 2)  # Input and nominal extrema.
                labels = axis.get_legend_handles_labels()[1]
                self.assertIn('Extrema from ±3σ input resistance limits', labels)
                self.assertIn('Nominal composition extrema', labels)
                self.assertNotIn('Mean matchline voltage', labels)
                np.testing.assert_allclose(axis.lines[-1].get_ydata(),
                                           [r['nominal_max_voltage_v'] * 1000 for r in extrema])

            # Add saved raw data with coincident points and verify that the
            # presentation path restores every point at its original position.
            nominal_data = np.zeros(4, dtype=[('delta_counts', 'u1', (8,)),
                                            ('squared_distance', 'u2'), ('voltage_v', 'f8'),
                                            ('nonzero_delta_kinds', 'u1')])
            nominal_data['squared_distance'] = [0, 0, 1, 2]
            nominal_data['voltage_v'] = [.48, .48, .24, .16]
            np.save(source / 'nominal.npy', nominal_data)
            trials = np.array([[.45, .48, .5]]) / np.arange(1, 4)[:, None]
            np.savez(source / 'voltage_samples.npz', squared_distances=np.arange(3), voltage_v=trials)
            (source / 'metadata.json').write_text(json.dumps({'nominal_data': 'nominal.npy'}))
            figures.clear()
            with mock.patch('matplotlib.figure.Figure.savefig', autospec=True, side_effect=capture), \
                    mock.patch.object(plot, 'generate') as generate, \
                    mock.patch('plot_cam_extrema.calculate_extrema', return_value=extrema):
                plot.main(['--presentation-from', str(root / 'source'), '--with-points',
                           '--output-dir', str(root / 'tv_points'), '--sizes', '32', '--levels', '5'])
                generate.assert_not_called()
            from matplotlib.collections import PathCollection
            self.assertEqual(len(figures), 2)
            for figure in figures:
                self.assertEqual(len(figure.axes), 2)  # Restored diversity colorbar.
                scatters = [item for item in figure.axes[0].collections if isinstance(item, PathCollection)]
                self.assertEqual(sum(len(item.get_offsets()) for item in scatters), 13)
                np.testing.assert_array_equal(scatters[0].get_offsets()[:, 1], trials.ravel() * 1000)
                np.testing.assert_array_equal(scatters[1].get_offsets()[:, 0], [0, 0, 1, 2])
                np.testing.assert_array_equal(scatters[1].get_offsets()[:, 1], [480, 480, 240, 160])
            destination = root / 'tv_points/stdev05/32x32'
            self.assertEqual((destination / 'distance_statistics.csv').read_bytes(), original)
            metadata = json.loads((destination / 'metadata.json').read_text())
            self.assertEqual(metadata['nominal_composition_count'], 4)
            self.assertEqual(metadata['varied_trial_count'], 9)

    def test_32_cell_counts_and_conditional_probabilities(self):
        counts = completion_counts(32)
        self.assertEqual(sum(counts[-1]), 8**32)
        self.assertGreater(max(counts[-1]), np.iinfo(np.int64).max)
        distance = 500
        vectors = sample_vectors(counts, distance, 20000, np.random.default_rng(9))
        self.assertTrue(np.all(np.sum(vectors * vectors, axis=1) == distance))
        for symbol in range(8):
            expected = counts[-2, distance - symbol**2] / counts[-1, distance]
            self.assertAlmostEqual(np.mean(vectors[:, 0] == symbol), expected, delta=0.015)

    def test_unbinned_nominal_export_and_native_agreement(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            points = plot.generate_nominal(8, directory, 9876, chunk_size=1000)
            self.assertEqual(len(points), 6435)
            self.assertEqual(len(np.unique(points['delta_counts'], axis=0)), 6435)
            self.assertTrue(np.all(points['delta_counts'].sum(axis=1) == 8))
            np.testing.assert_array_equal(points['squared_distance'], points['delta_counts'] @ (np.arange(8)**2))
            matcher = evacam_py.EvaCAMMatch(str(directory / 'inputs/run.config.yaml'))
            for point in points[::500]:
                result = matcher.evaluate_zero_query_composition(point['delta_counts'].tolist(), 0)
                self.assertAlmostEqual(point['voltage_v'], result.matchline_voltage, places=13)
            axis = mock.Mock()
            samples = np.array([[.48, .48], [.4, .39], [.3, .2]])
            plot.plot_all_points(axis, points, np.array([0, 1, 4]), samples, 1, chunk_size=1000)
            calls = axis.scatter.call_args_list
            np.testing.assert_array_equal(calls[0].args[0], [0, 0, 1, 1])
            np.testing.assert_array_equal(calls[0].args[1], [480, 480, 400, 390])
            plotted = np.concatenate([call.args[0] for call in calls[1:]])
            np.testing.assert_array_equal(plotted, points['squared_distance'][points['squared_distance'] <= 1])
            self.assertTrue(all(call.kwargs['rasterized'] for call in calls))

    def test_default_cli_and_statistical_rendering(self):
        import plot_mcam_voltage
        with mock.patch.object(plot, 'main') as main:
            plot_mcam_voltage.main(['--sizes', '32'])
            main.assert_called_once_with(['--sizes', '32'])
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch.object(plot, 'generate_nominal', return_value=None), \
                mock.patch.object(plot, 'generate') as generate:
            plot.main(['--output-dir', str(Path(temporary) / 'results')])
            self.assertEqual([(call.args[0], call.args[1]) for call in generate.call_args_list],
                             [(size, level) for size in (8, 16, 32) for level in (0, 5, 10)])
        records = [{'squared_euclidean_distance': distance,
                    **summarize(np.array([.45, .48, .5]) / (distance + 1))}
                   for distance in range(3)]
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch('matplotlib.figure.Figure.savefig') as save:
            plot.draw_plot(records, 8, 5, Path(temporary))
            self.assertEqual(len(save.call_args_list), 6)

    def test_64_cell_band_only_skips_nominal_compositions(self):
        counts = completion_counts(64)
        vectors = sample_vectors(counts.astype(float), 640, 1000,
                                 np.random.default_rng(9))
        self.assertTrue(np.all(np.sum(vectors * vectors, axis=1) == 640))
        self.assertEqual(sum(counts[-1]), 8**64)
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch.object(plot, 'generate_nominal') as nominal, \
                mock.patch.object(plot, 'generate') as generate:
            output = Path(temporary) / 'results'
            plot.main(['--sizes', '64', '--levels', '5', '--band-only',
                       '--output-dir', str(output)])
            nominal.assert_not_called()
            generate.assert_called_once_with(64, 5, 10000, 9876,
                                             output / 'stdev05/64x64', None)

    def test_zero_variation_generation_exports_output_spread(self):
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(plot, 'draw_plot'):
            directory = Path(temporary)
            plot.generate(8, 0, 20, 9876, directory)
            with (directory / 'distance_statistics.csv').open() as stream:
                records = list(csv.DictReader(stream))
            with np.load(directory / 'voltage_samples.npz') as raw:
                self.assertEqual(raw['voltage_v'].shape, (len(records), 20))
                for record, values in zip(records, raw['voltage_v']):
                    self.assertAlmostEqual(float(record['mean_voltage_v']), np.mean(values))
                    self.assertAlmostEqual(float(record['stddev_voltage_v']), np.std(values, ddof=1))
            self.assertGreater(max(float(record['stddev_voltage_v']) for record in records), .01)
            metadata = json.loads((directory / 'metadata.json').read_text())
            self.assertEqual(metadata['stdev_fraction_by_delta'], [0.] * 8)

    def test_conditional_counts_and_samples(self):
        counts = completion_counts(3)
        expected = np.zeros(counts.shape[1], dtype=np.int64)
        for vector in itertools.product(range(8), repeat=3):
            expected[sum(value * value for value in vector)] += 1
        np.testing.assert_array_equal(counts[-1], expected)
        vectors = sample_vectors(completion_counts(2), 25, 12000, np.random.default_rng(9))
        self.assertTrue(np.all(np.sum(vectors * vectors, axis=1) == 25))
        for pair in ((0, 5), (5, 0), (3, 4), (4, 3)):
            probability = np.mean(np.all(vectors == pair, axis=1))
            self.assertAlmostEqual(probability, 0.25, delta=0.025)
        with self.assertRaises(ValueError):
            sample_vectors(completion_counts(1), 2, 10, np.random.default_rng(1))

    def test_variation_and_output_statistics(self):
        vectors = np.zeros((20000, 1), dtype=np.int64)
        resistance = np.array([100.0])
        varied = varied_conductances(vectors, resistance, np.array([0.1]), np.random.default_rng(9))
        samples = 1 / varied
        self.assertTrue(np.all((samples >= 70) & (samples <= 130)))
        self.assertGreater(np.std(samples), 9)
        self.assertLess(np.std(samples), 11)
        nominal = varied_conductances(vectors, resistance, np.array([0.0]), np.random.default_rng(9))
        self.assertTrue(np.all(nominal == 0.01))
        stats = summarize(np.array([1.0, 2.0, 3.0]))
        self.assertEqual(stats["mean_voltage_v"], 2)
        self.assertEqual(stats["stddev_voltage_v"], 1)
        self.assertEqual(stats["lower_3sigma_voltage_v"], -1)

    def test_native_voltage_conversion(self):
        source = ROOT / "config/2FeFET_MCAM_variation/stdev05/2FeFET_MCAM_8x8.config.yaml"
        with tempfile.TemporaryDirectory(prefix="evacam-distance-statistics-") as temporary:
            config, _ = prepare_config(source, Path(temporary), samples=2)
            matcher = evacam_py.EvaCAMMatch(str(config))
            results = matcher.evaluate_distance_samples([1, 2, 3, 0, 0, 0, 0, 0], [0] * 8)
            self.assertEqual(len(results), 2)
            actual = matcher.sense_mcam_conductances([result.matchline_conductance for result in results])
            np.testing.assert_allclose(actual, [result.matchline_voltage for result in results], rtol=1e-13)
            self.assertEqual(matcher.sense_mcam_conductances([]), [])
            for value in (0, -1, float("nan"), float("inf")):
                with self.assertRaises(ValueError):
                    matcher.sense_mcam_conductances([value])


if __name__ == "__main__":
    unittest.main()
