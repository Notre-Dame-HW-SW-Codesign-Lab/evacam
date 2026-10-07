#!/usr/bin/env python3
"""Contract tests for the pinned old/new comparison, without needing an old checkout."""
import contextlib
import copy
import io
import json
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import compare_legacy_evacam as audit


class LegacyComparisonTests(unittest.TestCase):
    def setUp(self):
        self.folder = Path(tempfile.mkdtemp(prefix='legacy-comparison-', dir=ROOT / 'test-bin'))
        self.decks = audit.cases()

    def test_shared_deck_export_and_native_probe(self):
        name, deck = self.decks[0]
        path = audit.write_deck(deck, self.folder)
        legacy = audit.export_legacy(deck, self.folder)
        text = legacy.read_text()
        self.assertIn('-AdditionalCapOnML (fF): 0.0', text)
        self.assertIn('-WithWriteDriver: No', text)
        self.assertIn('-BitSerialWidth: 64', text)
        self.assertIn('-RowPort:volSearch0 (V): 0:1.0', (self.folder / 'legacy.cell').read_text())
        self.assertIn('-ResistanceOff (ohm): 1600000000.0', (self.folder / 'legacy.cell').read_text())
        result = audit.run_probe(ROOT / 'test-bin/LegacyComparisonProbe', path, self.folder, 'native')
        self.assertEqual(result['input.entries'], 64)
        self.assertEqual(result['input.bits_per_ml'], 64)
        self.assertEqual(result['input.additional_cap_f'], 0)
        self.assertEqual(result['input.write_driver'], 0)
        self.assertGreater(result['sa_s'], 0)
        self.assertGreater(audit.check_inputs(result, result), 30)
        self.assertEqual(len(audit.sha(path)), 64)
        reloaded = audit.load_deck(path)
        self.assertEqual(reloaded['memory'], deck['memory'])
        self.assertEqual(reloaded['cell']['ports'], deck['cell']['ports'])

    def test_export_rejects_nonshared_options_and_invalid_units(self):
        for section, key, value in [('sensing', 'decision', {'model': 'voltage_threshold'}),
                                    ('architecture', 'search_timing', {'control': 'broadcast'})]:
            deck = copy.deepcopy(self.decks[0][1]); deck[section][key] = value
            with self.assertRaises(ValueError): audit.export_legacy(deck, self.folder)
        deck = copy.deepcopy(self.decks[0][1])
        deck['architecture']['organization']['banks']['total'] = [2, 1]
        with self.assertRaises(ValueError): audit.export_legacy(deck, self.folder)
        for value in ('NaN', 'inf', '2garbage', '1e999V'):
            with self.assertRaises(ValueError): audit.numeric(value)
        self.assertEqual(audit.numeric('1.2V'), 1.2)
        self.assertEqual(audit.numeric('1e-5uA'), 1e-5)

    def test_probe_rejects_missing_nonfinite_duplicate_and_input_drift(self):
        for text in ('Finished!', 'BASELINE feasible=1\n', 'BASELINE feasible=nan\n',
                     'BASELINE feasible=0\nBASELINE feasible=0\n'):
            with self.assertRaises(ValueError): audit.parse_probe(text)
        self.assertEqual(audit.parse_probe('No valid solutions.'), {'feasible': 0})
        self.assertEqual(audit.parse_probe('No solutions found.'), {'feasible': 0})
        with self.assertRaises(ValueError): audit.check_inputs({'input.ron': 1}, {'input.ron': 2})
        with self.assertRaises(ValueError): audit.check_inputs({'input.ron': 1}, {})
        with mock.patch.object(audit.subprocess, 'run', return_value=mock.Mock(returncode=1, stdout='', stderr='failed')):
            with self.assertRaises(ValueError): audit.run_probe(Path('binary'), self.folder/'input', self.folder, 'bad')
        self.assertEqual((self.folder/'bad.log').read_text(), 'failed')

    def test_archive_is_pinned_and_instrumentation_is_output_only(self):
        files = {'CAM_Result.cpp': 'void CAM_Result::print() {\n}\n',
                 '2FeFET_TCAM.cfg': 'config', '2FeFET_TCAM.cell': 'cell'}
        archive = io.BytesIO()
        with tarfile.open(fileobj=archive, mode='w') as tar:
            for name, text in files.items():
                entry = tarfile.TarInfo(name); entry.size = len(text)
                tar.addfile(entry, io.BytesIO(text.encode()))
        def build(*args, **kwargs):
            (kwargs['cwd'] / 'Eva-CAM').write_text('test executable')
            return mock.Mock(returncode=0)
        with mock.patch.object(audit.subprocess, 'check_output', return_value=archive.getvalue()) as git, \
             mock.patch.object(audit.subprocess, 'run', side_effect=build):
            source, provenance = audit.prepare_old(Path('repository'), self.folder)
        self.assertEqual(git.call_args.args[0][-1], audit.COMMIT)
        self.assertEqual(provenance['commit'], audit.COMMIT)
        self.assertEqual((source / '2FeFET_TCAM.cell').read_text(), 'cell')
        instrumented = (source/'CAM_Result.cpp').read_text()
        self.assertIn('FIELD("sa_s", audit_sub.senseAmpLatency)', instrumented)
        self.assertIn('FIELD("search_s", audit_bank.searchLatency)', instrumented)
        self.assertNotIn('audit_bank.searchLatency =', instrumented)
        self.assertNotEqual(provenance['original_sha256']['CAM_Result.cpp'], audit.sha(source/'CAM_Result.cpp'))
        # A changed archive layout must fail before writing outside the run directory.
        invalid = self.folder/'invalid'; invalid.mkdir()
        with mock.patch.object(audit.subprocess, 'check_output', return_value=archive.getvalue().replace(b'CAM_Result.cpp', b'../evilxxx.cpp')):
            with self.assertRaises((ValueError, tarfile.TarError)):
                audit.prepare_old(Path('repository'), invalid)

    def test_diode_ablation_changes_only_inner_cell_count(self):
        source = self.folder/'source'; source.mkdir()
        line = '( CalculateGateCap(CellPort.widthCmos * {t}->featureSize, *{t}) * numCell\n + drain ) * numCell;\n'
        (source/'CAM_Line.cpp').write_text(line.format(t='tech') + line.format(t='FEFET_tech'))
        (source/'Eva-CAM').write_text('test executable')
        with mock.patch.object(audit.subprocess, 'run', return_value=mock.Mock(returncode=0)), \
             mock.patch.object(audit, 'run_probe', return_value={'feasible': 1, 'search_s': 1e-9}):
            result = audit.old_diode_ablation(source, self.folder, [('MRAM-test', {})])
        changed = (self.folder/'old-linear-diode-cap/CAM_Line.cpp').read_text()
        self.assertEqual(changed.count(') * numCell;'), 2)
        self.assertNotIn(') * numCell\n', changed)
        self.assertEqual(result['runs'][0]['id'], 'MRAM-test')

    def test_circuit_stages_use_native_models(self):
        stages = audit.circuit_stages(ROOT / 'test-bin/LegacyComparisonProbe', self.folder)
        self.assertEqual(len(stages), 6)
        fe = stages[2]['current']; mr = stages[5]['current']
        self.assertAlmostEqual(fe['path_on_ohm'], 10000)
        self.assertEqual(fe['input.bits_per_ml'], 64)
        self.assertEqual(mr['input.bits_per_ml'], 32)
        self.assertGreater(mr['search_s'], stages[3]['current']['search_s'])
        self.assertLess(fe['energy_j'], stages[0]['current']['energy_j'])

    def test_report_keeps_read_search_and_paper_claims_separate(self):
        old = dict(feasible=1, search_s=1e-9, read_s=2e-9, energy_j=1e-12)
        new = dict(feasible=1, search_s=1.5e-9, read_s=2e-9, energy_j=2e-12)
        text = audit.render({'runs': [dict(id='example', old=old, current=new)]})
        self.assertIn('| example | 2.000000 | 1.000000 | 1.500000 | +50.0%', text)
        self.assertIn('not paper accuracy scores', text)
        self.assertIn('not recovered publication decks', text)

    def test_runner_fails_on_drift_instead_of_reusing_success(self):
        source = self.folder/'source'; source.mkdir()
        (source/'Eva-CAM').write_text('binary')
        (source/'2FeFET_TCAM.cfg').write_text('root')
        v = dict(feasible=1, search_s=1e-9, read_s=2e-9, energy_j=1e-12, area_m2=1e-9,
                 **{'input.entries': 64})
        probe = self.folder/'probe'; probe.write_text('binary')
        output = self.folder/'output'
        with mock.patch.object(audit, 'prepare_old', return_value=(source, {})), \
             mock.patch.object(audit, 'cases', return_value=[self.decks[0]]), \
             mock.patch.object(audit, 'circuit_stages', return_value=[]), \
             mock.patch.object(audit, 'old_diode_ablation', return_value={'runs': []}), \
             mock.patch.object(audit.shutil, 'which', return_value=None), \
             contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            with mock.patch.object(audit, 'run_probe', return_value=v):
                self.assertEqual(audit.run_suite(Path('repo'), probe, output), 0)
            previous = (output/'runs.json').read_text()
            drift = {**v, 'input.entries': 32}
            with mock.patch.object(audit, 'run_probe', side_effect=[v, v, drift]):
                self.assertEqual(audit.run_suite(Path('repo'), probe, output), 1)
            self.assertEqual(json.loads((output/'runs.json').read_text())['status'], 'failed')
            self.assertEqual(json.loads((output/'runs.json').read_text())['runs'], [])
            self.assertIn('Resolved inputs differ', (output/'comparison.md').read_text())
            self.assertTrue(any(p.read_text() == previous for p in output.glob('run-*/runs.json')))
            self.assertEqual(len(list(output.glob('run-*'))), 2)
            self.assertEqual(json.loads(previous)['runs'][0]['matched_input_fields'], 1)


if __name__ == '__main__':
    unittest.main()
