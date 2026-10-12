#!/usr/bin/env python3
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import audit_fefet_energy as audit


class FeFetEnergyAuditTest(unittest.TestCase):
    def test_supply_charge_for_each_binary_transition(self):
        # Unequal loads make swapped complementary rails observable. Each
        # 0->1 capacitor draws Q=C*V from the high rail, hence supply work V*Q.
        caps, voltage = [2e-15, 3e-15], 2.0
        expected = {(0, 0): 0, (0, 1): 12e-15, (1, 0): 8e-15, (1, 1): 0}
        for (old, new), joules in expected.items():
            self.assertAlmostEqual(audit.query_line_energy(caps, voltage, [old], [new], False) / 1e-15,
                                   joules / 1e-15)
            self.assertAlmostEqual(audit.query_line_energy(caps, voltage, [old], [new], True) / 1e-15,
                                   8 if new == 0 else 12)
        self.assertAlmostEqual(audit.query_line_energy(caps, voltage, [0, 1, 1], [1, 0, 1], False) / 1e-15, 20)
        for args in [([-1, 1], 1, [0], [1]), ([1, float('nan')], 1, [0], [1]),
                     ([1, 1], 0, [0], [1]), ([1, 1], 1, [0], []),
                     ([1, 1], 1, [0], [2])]:
            with self.assertRaises(ValueError): audit.query_line_energy(*args, False)

    def test_reference_and_alternatives_preserve_one_authoritative_width(self):
        stages = audit.sensing_stages()
        self.assertEqual(len(stages), 10)
        reference = stages[0][1]
        for label, deck, amp, reason in stages:
            self.assertTrue(reason)
            self.assertNotIn('match_transistor', deck['architecture'].get('matchline', {}))
            if label.startswith('pulldown-'):
                self.assertNotEqual(deck['cell']['ports']['column'][0]['cmos_width'], '1F')
                self.assertEqual(deck['sensing'], reference['sensing'])
            if amp is not None:
                self.assertEqual(deck, reference)
                self.assertEqual(amp['model'], 'analytical_inverter')

    def test_native_budget_audit_and_failed_rerun(self):
        (ROOT / 'test-bin').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='fefet-audit-', dir=ROOT / 'test-bin') as tmp:
            output = Path(tmp)
            with patch('builtins.print'):
                self.assertEqual(audit.run_suite(ROOT / 'test-bin/LegacyComparisonProbe', output), 0)
            report = json.loads((output / 'runs.json').read_text())
            self.assertEqual(len(report['energy_runs']), 2)
            for run in report['energy_runs']:
                values = run['values']
                result = run['audit']
                self.assertGreater(result['driver_internal_j'], 0)
                protocols = result['external_line_protocols_j']
                self.assertAlmostEqual(protocols['retained_independent_equiprobable_queries']
                                       / protocols['reset_each_query_equiprobable'], .5)
                self.assertAlmostEqual((result['searchline_load_j'] + result['driver_internal_j'])
                                       / values['current.energy.row_driver'], 1)
                for key in ('current.row0_driver_load_f', 'current.energy.row_driver'):
                    broken = copy.deepcopy(values)
                    broken[key] *= 2
                    with self.assertRaises(ValueError): audit.energy_audit(broken)
            stages = {r['id']: r['values'] for r in report['sensing_runs']}
            base = stages['reference']
            self.assertLess(base['sa_cap_f'], stages['generic-SA-fixed-0.5V']['sa_cap_f'])
            self.assertLess(base['sa_s'], stages['inverter-output-1fF']['sa_s'])
            self.assertLess(stages['pulldown-10F']['path_off_ohm'], base['path_off_ohm'])
            self.assertGreater(stages['pulldown-10F']['current.gate_cap_f'], base['current.gate_cap_f'])
            previous_config = Path(report['energy_runs'][0]['config'])
            self.assertTrue(previous_config.exists())
            with patch('builtins.print'), patch.object(audit, 'run_probe', side_effect=ValueError('failed run')):
                self.assertEqual(audit.run_suite(ROOT / 'test-bin/LegacyComparisonProbe', output), 1)
            failed = json.loads((output / 'runs.json').read_text())
            self.assertTrue(all('error' in r for r in failed['energy_runs'] + failed['sensing_runs']))
            self.assertTrue(previous_config.exists())
            self.assertIn('FAILED', (output / 'comparison.md').read_text())


if __name__ == '__main__':
    unittest.main()
