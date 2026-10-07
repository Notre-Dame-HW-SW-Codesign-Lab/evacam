"""Matched native NVSim inputs, independent RC limits, and energy boundaries."""
import contextlib
import copy
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
sys.path.insert(0, str(ROOT / 'scripts'))
import validate_nand_nvsim as validation


class NvsimValidationTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='nvsim-validation-')
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name)
        self.manifest = yaml.safe_load(validation.MANIFEST.read_text())
        self.baseline = json.loads(validation.FROZEN.read_text())

    def test_frozen_contract_rejects_changed_results_and_missing_fixtures(self):
        validation.validate_baseline(self.baseline, self.manifest)
        self.assertEqual(len(self.baseline['cases']), 16)
        self.assertEqual(validation.digest(validation.FROZEN), self.manifest['baseline_sha256'])
        for mutate in (
                lambda b: b.update(revision='unverified'),
                lambda b: b['cases'].pop(),
                lambda b: b['cases'][0].update(read_energy_j=0.),
                lambda b: b['cases'][0].update(read_energy_j=float('nan')),
                lambda b: b['cases'][0]['components']['row_decoder'].update(read_j=0.)):
            bad = copy.deepcopy(self.baseline)
            mutate(bad)
            with self.assertRaises(ValueError):
                validation.validate_baseline(bad, self.manifest)
        with self.assertRaisesRegex(ValueError, 'hash mismatch'):
            validation.validate_baseline(self.baseline, dict(self.manifest, baseline_sha256='bad'))

    def test_reference_recovers_analytic_single_and_two_node_limits(self):
        cap, res = np.array([2.]), np.array([3.])
        np.testing.assert_allclose(validation.reference(cap, res, 6., np.array([.6])), [.6/np.e])
        self.assertAlmostEqual(validation.threshold_time(cap, res, np.array([.6]), .2), 6*np.log(3))
        self.assertEqual(validation.threshold_time(cap, res, np.array([.1]), .2), 0.)
        np.testing.assert_allclose(validation.reference(cap, res, 6., np.array([0.]), (3., .6)), [.6*(1-1/np.e)])
        # Equal C and R: eigenvalues of the discharge matrix are (-3 +/- sqrt(5))/2.
        t = .7
        expected_far = np.exp(-1.5*t)*(np.cosh(np.sqrt(5)*t/2) + 3/np.sqrt(5)*np.sinh(np.sqrt(5)*t/2))
        self.assertAlmostEqual(validation.reference(np.ones(2), np.ones(2), t, np.ones(2))[-1], expected_far)
        case = self.baseline['cases'][0]
        delay_cap, _ = validation.circuit(case)
        power_cap, _ = validation.circuit(case, energy=True)
        lumped_cap, _ = validation.circuit(case, lumped=True)
        self.assertAlmostEqual(lumped_cap[0]/sum(delay_cap), 1.)
        self.assertGreater(sum(delay_cap), sum(power_cap))

    def test_energy_ledger_reconciles_native_totals_and_retains_erase_program_term(self):
        for case in self.baseline['cases']:
            ledger = validation.energy_ledger(case)
            self.assertLess(max(abs(r) for r in ledger['relative_residual'].values()), 1e-14)
            self.assertEqual(ledger['erase_embedded_program_j'], ledger['reconstructed_j']['program'])
            without_program = ledger['reconstructed_j']['erase']-ledger['erase_embedded_program_j']
            self.assertGreater(abs(without_program/case['erase_energy_j']-1), .01)
            self.assertNotEqual(ledger['reconstructed_j']['write'], ledger['reconstructed_j']['program'])

    def test_compiled_source_charge_and_nonuniform_initial_state(self):
        cap, res, initial = np.array([2e-14, 3e-14]), np.array([2e6, 8e5]), np.array([.1, .5])
        result = validation.compiled(cap, res, 1e-8, initial, self.path, 'nonuniform', self.manifest['criteria'])
        np.testing.assert_allclose(result['voltages_v'], validation.reference(cap, res, 1e-8, initial), atol=3e-6, rtol=0)
        charged = validation.compiled(cap, res, 1e-8, initial, self.path, 'charge', self.manifest['criteria'], (3e5, .6))
        charge_gain = np.dot(cap, np.array(charged['voltages_v'])-initial)
        self.assertAlmostEqual(charged['right_source_charge_c'], charge_gain, delta=1e-23)

    def test_evaluate_case_checks_matched_delay_and_finite_precharge(self):
        for index in (1, -1):
            case = self.baseline['cases'][index]
            row = validation.evaluate_case(case, self.path, self.manifest)
            self.assertLess(row['maximum_cpp_voltage_error_v'], self.manifest['criteria']['voltage_absolute_v'])
            self.assertGreater(row['nvsim_to_ideal_ratio'], 19.)
            self.assertLess(abs(row['uncorrected_relative_error']), .005)
            self.assertAlmostEqual(row['precharges'][-1]['fraction_of_full_cv2'], 1., delta=1e-5)
            self.assertAlmostEqual(row['precharges'][-1]['carried_decision_v'], row['energy_circuit_fully_charged_decision_v'], delta=3e-6)
            self.assertLess(row['precharges'][0]['carried_decision_v'], row['energy_circuit_fully_charged_decision_v'])
        bad = dict(case, bitline_delay_s=case['bitline_delay_s']/20)
        with self.assertRaisesRegex(AssertionError, 'replay NVSim timing'):
            validation.evaluate_case(bad, self.path, self.manifest)

    def test_pinned_export_rejects_source_hash_mismatch_before_build(self):
        with mock.patch.object(validation.subprocess, 'check_output', side_effect=[
                'SubArray.cpp\nsample_SLCNAND.cell\n', b'wrong source', b'wrong cell']), \
                mock.patch.object(validation.subprocess, 'run') as build:
            with self.assertRaisesRegex(ValueError, 'source hash mismatch'):
                validation.export_and_build(self.path, self.path, self.manifest)
            build.assert_not_called()

    def test_collect_baseline_requires_valid_cli_and_preserves_native_fields(self):
        outputs = [subprocess.CompletedProcess([], 0, 'Read Latency', '')]
        outputs += [subprocess.CompletedProcess([], 0, json.dumps(case), '') for case in self.baseline['cases']]
        with mock.patch.object(validation.subprocess, 'run', side_effect=outputs):
            baseline = validation.collect_baseline(self.path, self.manifest)
        validation.validate_baseline(baseline, self.manifest)
        self.assertEqual((self.path/'baseline.cfg').read_text(), validation.baseline_config())
        with mock.patch.object(validation.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, 'No valid solutions', '')):
            with self.assertRaises(RuntimeError):
                validation.collect_baseline(self.path, self.manifest)

    def test_run_and_plots_keep_hardware_improvement_unestablished(self):
        report = validation.run(self.path)
        self.assertEqual(len(report['cases']), 16)
        self.assertFalse(report['hardware_validation'])
        self.assertEqual(report['baseline_mode'], 'frozen_native_results')
        self.assertIn('unestablished', report['status'])
        self.assertGreater((self.path/'comparison.png').stat().st_size, 1000)
        self.assertIn('<svg', (self.path/'comparison.svg').read_text())
        with mock.patch.object(validation, 'run', return_value=report), contextlib.redirect_stdout(io.StringIO()) as output:
            validation.main(['--output', str(self.path)])
        self.assertIn('16 matched fixtures', output.getvalue())


if __name__ == '__main__':
    unittest.main()
