"""Offline reference integrity, axis mathematics and evidence-boundary checks."""
import contextlib
import copy
import io
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
import check_nand_park_reference as park


class ParkReferenceTest(unittest.TestCase):
    def setUp(self):
        self.manifest, self.dataset, self.extraction = park.load_reference(park.REFERENCE, park.DATA)
        self.directory = tempfile.TemporaryDirectory(prefix='park-reference-')
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name)

    def test_known_linear_and_log_coordinates(self):
        # Independent fixtures: x=0 V at 20 px and 4 V at 220 px;
        # I=1e-3 A at 100 px and 1e-9 A at 400 px.
        linear = dict(scale='linear', slope=.02, intercept=-.4)
        log = dict(scale='log10', slope=-.02, intercept=-1.)
        self.assertAlmostEqual(park.axis_value(linear, 145), 2.5)
        self.assertAlmostEqual(park.pixel_value(linear, 2.5), 145.)
        self.assertAlmostEqual(park.axis_value(log, 250), 1e-6, delta=1e-20)
        self.assertAlmostEqual(park.pixel_value(log, 1e-6), 250.)
        # A five-pixel uncertainty is 0.1 decade, multiplicative rather than additive.
        self.assertAlmostEqual(park.axis_value(log, 245)/park.axis_value(log, 250), 10**.1)

    def test_invalid_axis_domains(self):
        for axis in (dict(scale='natural_log', slope=1., intercept=0.),
                     dict(scale='linear', slope=0., intercept=0.),
                     dict(scale='linear', slope=float('nan'), intercept=0.),
                     dict(scale='log10', slope=1., intercept=1000.),
                     dict(scale='log10', slope=1., intercept=-1000.)):
            with self.subTest(axis=axis), self.assertRaises(ValueError):
                park.axis_value(axis, 1.)
        for value in (0., -1., float('inf')):
            with self.subTest(value=value), self.assertRaises(ValueError):
                park.pixel_value(dict(scale='log10', slope=-.02, intercept=-1.), value)

    def test_condition_status_and_missing_inputs(self):
        unavailable = dict(value=None, unit='K', status='unavailable', source='article', reason='not disclosed')
        park.validate_condition(unavailable)
        for field, value in (('value', 300), ('reason', ''), ('source', ''), ('status', 'guessed')):
            with self.subTest(field=field), self.assertRaises(ValueError):
                park.validate_condition(dict(unavailable, **{field: value}))
        with self.assertRaises(ValueError):
            park.validate_condition(dict(value=300., unit='K', status='assumed', source='default'))
        with self.assertRaises(ValueError):
            park.validate_condition(dict(value=float('nan'), unit='K', status='reported', source='paper'))

    def test_frozen_corpus_and_evidence_boundary(self):
        report = park.validate_reference(self.manifest, self.dataset, self.extraction)
        self.assertEqual((report['cases'], report['curves'], report['points']), (20, 56, 347))
        self.assertEqual(report['quantitative_validation_eligible_cases'], 0)
        self.assertEqual(report['independent_rechecks'], 14)
        self.assertLessEqual(report['maximum_tick_error_pixel'], 2.)
        self.assertLessEqual(report['maximum_repeat_difference_pixel'], 6.)
        self.assertEqual(report['status'], 'reference_checked_model_validation_pending')
        self.assertEqual(len(self.manifest['partitions']['calibration']), 3)
        self.assertEqual(park.sha256(park.DATA/'curves.json'), self.manifest['data_hashes']['curves.json'])
        self.assertFalse(any(c['axis'] == 'fig7b' for c in self.dataset['curves']))
        self.assertEqual(sum(u['id'].startswith('fig7b_') for u in self.extraction['unusable']), 6)
        for curve in self.dataset['curves']:
            if curve['axis'] == 'fig5_inset':
                self.assertFalse(curve['independent_of_other_panels'])
                self.assertEqual(len(curve['points']), 1)

    def test_frozen_files_detect_tampering(self):
        for filename in ('curves.json', 'extraction.json'):
            shutil.copy(park.DATA/filename, self.path/filename)
        with (self.path/'curves.json').open('a') as handle:
            handle.write(' ')
        with self.assertRaisesRegex(ValueError, 'hash mismatch'):
            park.load_reference(park.REFERENCE, self.path)

    def test_unique_cases_curves_and_observations(self):
        for kind in ('case', 'curve', 'point'):
            manifest, dataset = copy.deepcopy(self.manifest), copy.deepcopy(self.dataset)
            if kind == 'case':
                manifest['cases'].append(copy.deepcopy(manifest['cases'][0]))
            elif kind == 'curve':
                dataset['curves'].append(copy.deepcopy(dataset['curves'][0]))
            else:
                dataset['curves'][0]['points'].append(copy.deepcopy(dataset['curves'][0]['points'][0]))
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                park.validate_reference(manifest, dataset, self.extraction)

    def test_whole_case_partition_and_correlated_case_leakage(self):
        self.dataset['curves'][0]['split'] = 'validation_layers'
        with self.assertRaisesRegex(ValueError, 'partition'):
            park.validate_reference(self.manifest, self.dataset, self.extraction)
        self.dataset['curves'][0]['split'] = 'calibration'
        # Even consistent reassignment of every point must not move a repeated
        # physical case across the calibration/held-out boundary.
        cid = 'layers_n32'
        self.manifest['partitions']['validation_layers'].remove(cid)
        self.manifest['partitions']['calibration'].append(cid)
        next(c for c in self.manifest['cases'] if c['id'] == cid)['split'] = 'calibration'
        for curve in self.dataset['curves']:
            if curve['case_id'] == cid:
                curve['split'] = 'calibration'
        with self.assertRaisesRegex(ValueError, 'leak'):
            park.validate_reference(self.manifest, self.dataset, self.extraction)

    def test_assumptions_cannot_silently_become_validation(self):
        self.manifest['cases'][0]['quantitative_validation_eligible'] = True
        self.manifest['cases'][0]['blocking_inputs'] = []
        with self.assertRaisesRegex(ValueError, 'missing or assumed'):
            park.validate_reference(self.manifest, self.dataset, self.extraction)
        self.manifest['cases'][0]['quantitative_validation_eligible'] = False
        self.manifest['partitions']['fitting_performed'] = True
        with self.assertRaisesRegex(ValueError, 'before fitting'):
            park.validate_reference(self.manifest, self.dataset, self.extraction)

    def test_units_and_image_provenance(self):
        for field, value in (('y_si_factor', 1.), ('source_sha256', 'changed'), ('y_unit', 'mA')):
            extraction = copy.deepcopy(self.extraction)
            extraction['axes']['fig4c'][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                park.validate_reference(self.manifest, self.dataset, extraction)
        self.dataset['curves'][0]['axis'] = 'fig7b'
        with self.assertRaisesRegex(ValueError, 'ambiguous source units'):
            park.validate_reference(self.manifest, self.dataset, self.extraction)

    def test_source_tick_and_pixel_coordinates(self):
        self.extraction['axes']['fig4c']['held_back_ticks']['x'][0][0] += 10
        with self.assertRaisesRegex(ValueError, 'tick calibration'):
            park.validate_reference(self.manifest, self.dataset, self.extraction)
        self.extraction['axes']['fig4c']['held_back_ticks']['x'][0][0] -= 10
        point = self.dataset['curves'][0]['points'][0]
        for field, value in (('pixel', [-1., 350.]), ('publication', [1., 44.]),
                             ('si', [1., 44.]), ('si', [float('nan'), 1.])):
            original = point[field]
            point[field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                park.validate_reference(self.manifest, self.dataset, self.extraction)
            point[field] = original

    def test_sweep_monotonicity(self):
        self.dataset['curves'][0]['points'].reverse()
        with self.assertRaisesRegex(ValueError, 'increase strictly'):
            park.validate_reference(self.manifest, self.dataset, self.extraction)

    def test_uncertainty_and_censoring(self):
        point = self.dataset['curves'][0]['points'][0]
        for field, value in (('pixel_uncertainty', [4., 0.]), ('x_uncertainty_si', .1),
                             ('y_interval_si', [0., 1.]), ('censored', True), ('interpolated', True)):
            original = point[field]
            point[field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                park.validate_reference(self.manifest, self.dataset, self.extraction)
            point[field] = original
        log_point = next(c for c in self.dataset['curves'] if c['axis'] == 'fig5_inset')['points'][0]
        log_point['y_uncertainty_decades'] = 0.
        with self.assertRaisesRegex(ValueError, 'log-current uncertainty'):
            park.validate_reference(self.manifest, self.dataset, self.extraction)

    def test_raw_picks_rechecks_and_exclusion_reasons(self):
        self.extraction['picks'][0]['final_pixel'] = [1., 1.]
        with self.assertRaisesRegex(ValueError, 'raw extraction provenance'):
            park.validate_reference(self.manifest, self.dataset, self.extraction)
        self.extraction['picks'][0]['final_pixel'] = self.dataset['curves'][0]['points'][0]['pixel']
        repeat = self.extraction['independent_recheck'][0]
        repeat['independent_y_pixel'] = repeat['manual_y_pixel']+7.
        repeat['difference_pixel'] = 7.
        with self.assertRaisesRegex(ValueError, 'exceeds stated uncertainty'):
            park.validate_reference(self.manifest, self.dataset, self.extraction)
        repeat['independent_y_pixel'] = repeat['manual_y_pixel']
        repeat['difference_pixel'] = 0.
        self.extraction['unusable'][0]['reason'] = ''
        with self.assertRaisesRegex(ValueError, 'exclusion lacks'):
            park.validate_reference(self.manifest, self.dataset, self.extraction)

    def test_log_units_cannot_be_silently_corrected(self):
        diagnostic = self.extraction['log_unit_diagnostic'][-1]
        self.assertAlmostEqual(diagnostic['alternative_if_label_should_be_A']/diagnostic['printed_uA_to_A'], 1e6)
        diagnostic['printed_uA_to_A'] = diagnostic['alternative_if_label_should_be_A']
        with self.assertRaisesRegex(ValueError, 'silently changed'):
            park.validate_reference(self.manifest, self.dataset, self.extraction)

    def test_inset_layer_count_and_fixed_voltage_contract(self):
        curve = next(c for c in self.dataset['curves'] if c['axis'] == 'fig5_inset')
        self.assertEqual(curve['fixed_selected_word_line_voltage']['value'], 6.)
        self.assertEqual(curve['fixed_selected_word_line_voltage']['unit'], 'V')
        case = next(c for c in self.manifest['cases'] if c['id'] == curve['case_id'])
        case['active_word_lines']['value'] = 42
        with self.assertRaisesRegex(ValueError, 'integer layer count'):
            park.validate_reference(self.manifest, self.dataset, self.extraction)
        case['active_word_lines']['value'] = 32
        curve['sweep_variable'] = 'selected_word_line_voltage'
        with self.assertRaisesRegex(ValueError, 'inset sweeps active layer count'):
            park.validate_reference(self.manifest, self.dataset, self.extraction)

    def test_report_plots_and_strict_exit_preserve_pending_status(self):
        with contextlib.redirect_stdout(io.StringIO()):
            code = park.main(['--output', str(self.path), '--require-model-validation'])
        self.assertEqual(code, 2)
        report = json.loads((self.path/'report.json').read_text())
        self.assertEqual(report['status'], 'reference_checked_model_validation_pending')
        self.assertFalse(report['source_cache_verified'])
        for filename in ('reference-curves.png', 'reference-curves.svg', 'extraction-quality.png'):
            self.assertGreater((self.path/filename).stat().st_size, 1000)
        self.assertEqual(report['provenance']['data_hashes'], self.manifest['data_hashes'])

    def test_source_overlay_verifies_hash_and_uses_native_coordinates(self):
        # Tiny synthetic source makes the optional overlay route offline, too.
        import matplotlib.pyplot as plt
        import numpy as np
        cache = self.path/'cache'
        cache.mkdir()
        plt.imsave(cache/'synthetic.png', np.ones((4, 4, 3)))
        manifest = copy.deepcopy(self.manifest)
        manifest['sources'] = {'synthetic.png': {'sha256': park.sha256(cache/'synthetic.png')}}
        extraction = copy.deepcopy(self.extraction)
        for axis in extraction['axes'].values():
            axis['image'] = 'synthetic.png'
        report = park.validate_reference(self.manifest, self.dataset, self.extraction)
        park.write_report(self.path, manifest, self.dataset, extraction, report, cache)
        self.assertTrue(report['source_cache_verified'])
        for tag in extraction['axes']:
            self.assertTrue((self.path/f'source-overlay-{tag}.png').is_file())
        manifest['sources']['synthetic.png']['sha256'] = 'wrong'
        with self.assertRaisesRegex(ValueError, 'cached source hash mismatch'):
            park.write_report(self.path, manifest, self.dataset, extraction, report, cache)


if __name__ == '__main__':
    unittest.main()
