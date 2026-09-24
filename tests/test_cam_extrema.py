import itertools
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

import numpy as np
import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path[:0] = [str(ROOT), str(ROOT / 'scripts')]
import evacam_py
import plot_cam_extrema as plot


class ExtremaTests(unittest.TestCase):
    def test_mcam_exact_extrema_include_all_compositions_and_input_corners(self):
        with tempfile.TemporaryDirectory() as temporary:
            actual = plot.prepare_inputs('mcam', 8, 10, Path(temporary))
            matcher = evacam_py.EvaCAMMatch(str(actual))
            records = plot.calculate_extrema(matcher, 'mcam', 8)
            from mcam_composition_stream import enumerate_delta_counts
            expected = {}
            for counts in enumerate_delta_counts(8, 8):
                d = sum(i*i*n for i, n in enumerate(counts))
                corners = [matcher.evaluate_zero_query_composition(counts, offset).matchline_voltage
                           for offset in (-3, 0, 3)]
                entry = expected.setdefault(d, [float('inf'), -float('inf'), float('inf'), -float('inf')])
                entry[0] = min(entry[0], corners[0])
                entry[1] = max(entry[1], corners[2])
                entry[2] = min(entry[2], corners[1])
                entry[3] = max(entry[3], corners[1])
            for row in records:
                np.testing.assert_allclose([row[k] for k in ('min_voltage_v', 'max_voltage_v',
                    'nominal_min_voltage_v', 'nominal_max_voltage_v')], expected[row['distance']], atol=1e-14)
            self.assertGreater(max(r['nominal_max_voltage_v'] - r['nominal_min_voltage_v'] for r in records), .01)

    def test_tcam_fixed_sensing_time_and_independent_on_off_corners(self):
        with tempfile.TemporaryDirectory() as temporary:
            actual = plot.prepare_inputs('tcam', 8, 10, Path(temporary))
            matcher = evacam_py.EvaCAMMatch(str(actual))
            records = plot.calculate_extrema(matcher, 'tcam', 8)
            self.assertEqual(matcher.word_width(), 8)
            nominal_config = plot.prepare_inputs('tcam', 8, 0, Path(temporary) / 'nominal')
            nominal_matcher = evacam_py.EvaCAMMatch(str(nominal_config))
            # Existing nominal sense margins independently verify voltage differences.
            for h in range(1, 9):
                self.assertAlmostEqual(records[h-1]['nominal_min_voltage_v'] - records[h]['nominal_min_voltage_v'],
                                       nominal_matcher.evaluate_mismatches(h).sense_margin, places=12)
            memory_path = actual.parent / 'memory_device.yaml'
            for on, off in itertools.product((0, 10), repeat=2):
                memory = yaml.safe_load(memory_path.read_text())
                memory['variation']['memory_device_resistance_on_stdev'] = f'{on}%'
                memory['variation']['memory_device_resistance_off_stdev'] = f'{off}%'
                memory_path.write_text(yaml.safe_dump(memory))
                mixed = evacam_py.EvaCAMMatch(str(actual))
                for h in range(9):
                    for offset in (-3, 0, 3):
                        v = mixed.sense_tcam_mismatches(h, offset)
                        self.assertGreaterEqual(v + 1e-14, records[h]['min_voltage_v'])
                        self.assertLessEqual(v - 1e-14, records[h]['max_voltage_v'])
            for h, offset in ((-1, 0), (9, 0), (0, 4), (0, float('nan'))):
                with self.assertRaises(ValueError):
                    matcher.sense_tcam_mismatches(h, offset)
            actual = plot.prepare_inputs('mcam', 8, 0, Path(temporary) / 'mcam')
            with self.assertRaises(RuntimeError):
                evacam_py.EvaCAMMatch(str(actual)).sense_tcam_mismatches(0)

    def test_zero_variation_and_plot_labels(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for model in ('mcam', 'tcam'):
                actual = plot.prepare_inputs(model, 8, 0, root / model)
                records = plot.calculate_extrema(evacam_py.EvaCAMMatch(str(actual)), model, 8)
                for row in records:
                    self.assertAlmostEqual(row['min_voltage_v'], row['nominal_min_voltage_v'])
                    self.assertAlmostEqual(row['max_voltage_v'], row['nominal_max_voltage_v'])
                figures = []
                def capture(fig, *args, **kwargs):
                    if fig not in figures:
                        figures.append(fig)
                with mock.patch('matplotlib.figure.Figure.savefig', autospec=True, side_effect=capture):
                    plot.draw_extrema(records, model, 8, 0, root)
                self.assertEqual(len(figures), 2)
                for fig in figures:
                    labels = fig.axes[0].get_legend_handles_labels()[1]
                    self.assertFalse(any('σ' in label or 'Mean' in label for label in labels))

    def test_generation_exports_and_cli(self):
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(plot, 'draw_extrema'):
            root = Path(temporary) / 'plots'
            plot.main(['--sizes', '8', '--levels', '0', '5', '--output-dir', str(root)])
            for model, level in itertools.product(('mcam', 'tcam'), (0, 5)):
                directory = root / model / f'stdev{level:02d}' / '8x8'
                self.assertTrue((directory / 'voltage_extrema.csv').is_file())
                self.assertTrue((directory / 'extrema_metadata.json').is_file())
                self.assertFalse((directory / 'voltage_samples.npz').exists())
            with self.assertRaises(FileExistsError):
                plot.main(['--output-dir', str(root)])


if __name__ == '__main__':
    unittest.main()
