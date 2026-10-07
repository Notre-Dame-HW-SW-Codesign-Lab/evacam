#!/usr/bin/env python3
import copy
import contextlib
import io
from unittest import mock
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

import yaml
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import validate_original_evacam as validation


class OriginalValidationTests(unittest.TestCase):
    def setUp(self):
        self.manifest = yaml.safe_load(validation.MANIFEST.read_text())

    def test_error_requires_matching_scope(self):
        observation = dict(value=2.5,unit='ns',scope='ML_ST_to_OUT',kind='silicon')
        unmatched = validation.compare_metric(observation, 2.72, False)
        self.assertAlmostEqual(unmatched['diagnostic_gap_percent'], 8.8)
        self.assertIsNone(unmatched['error_percent'])
        self.assertAlmostEqual(validation.compare_metric(observation,2.72,True)['error_percent'],8.8)
        observation['operator']='lower_bound'
        self.assertIsNone(validation.compare_metric(observation,2,True)['error_percent'])
        self.assertIsNone(validation.compare_metric(observation,2,False)['diagnostic_gap_percent'])
        observation['value']=None
        self.assertEqual(validation.compare_metric(observation,2,False)['status'],'unavailable')
        for value in (float('nan'),float('inf'),-1):
            with self.assertRaises(ValueError): validation.compare_metric(observation,value,False)

    def test_original_paper_is_not_the_2fe1t_case(self):
        case = self.manifest['cases'][0]
        self.assertEqual(case['id'],'FeFET-2Fe-TCAS19')
        self.assertEqual(case['geometry']['entries'],64)
        self.assertEqual(case['observations']['latency']['value'],.350)
        self.assertEqual(case['published_evacam']['latency'],.345)
        self.assertTrue(all(not any(c['comparable'].values()) for c in self.manifest['cases']))
        self.assertIsNone(self.manifest['cases'][1]['observations']['energy']['value'])

    def test_sapiens_clock_and_query_are_different_operations(self):
        workload = copy.deepcopy(self.manifest['sapiens_workload'])
        result = validation.sapiens_schedule(workload)
        self.assertEqual(result['clock_period_ns'],5)
        self.assertEqual(result['logical_bits'],32768)
        self.assertEqual(result['query_ns'],640)
        self.assertEqual(result['serial_eight_bank_query_ns'],5120)
        self.assertAlmostEqual(result['measured_power_times_query_pj'],2169.6)
        workload['bits_per_cycle']=2
        self.assertEqual(validation.sapiens_schedule(workload)['query_ns'],320)
        workload['physical_rram_devices']=32768
        with self.assertRaises(ValueError):validation.sapiens_schedule(workload)
        workload['clock_mhz']=0
        with self.assertRaises(ValueError):validation.sapiens_schedule(workload)

    def test_every_fixture_leaf_has_current_provenance(self):
        ledger=yaml.safe_load(validation.LEDGER.read_text())
        audit=validation.audit_inputs(ROOT,ledger)
        self.assertGreater(audit['paper'],0)
        self.assertGreater(audit['unavailable'],0)
        broken=copy.deepcopy(ledger); broken['inputs'][0]['value']='changed'
        with self.assertRaises(ValueError):validation.audit_inputs(ROOT,broken)
        broken=copy.deepcopy(ledger);broken['inputs'].append(broken['inputs'][0])
        with self.assertRaises(ValueError):validation.audit_inputs(ROOT,broken)

    def test_report_never_calls_unmatched_gaps_errors(self):
        case=self.manifest['cases'][0]
        run=dict(id=case['id'],status='partial',blockers=case['blockers'],published_evacam=case['published_evacam'],
                 metrics={name:validation.compare_metric(obs,None,False) for name,obs in case['observations'].items()})
        text=validation.render_report(dict(runs=[run],input_audit={},sapiens_schedule=validation.sapiens_schedule(self.manifest['sapiens_workload'])))
        self.assertIn('Diagnostic gap',text)
        self.assertIn('not validated error',text)
        self.assertNotRegex(text, r'\b(?:nan|inf)\b')



    def test_runner_uses_fresh_evidence_and_rejects_missing_results(self):
        case=copy.deepcopy(self.manifest['cases'][0])
        manifest=copy.deepcopy(self.manifest);manifest['cases']=[case]
        geometry=case['geometry']
        result=dict(assumptions={'modeling_options':case['expected_model'], 'technology':{'process_node':'45nm'}},
                    geometry=dict(entry_count=64,logical_word_width_bits=64,physical_cell_count=4096),
                    summary=dict(timing={'sense_margin_pass':True,'search_latency':'0.3ns'},
                                 power={'search_dynamic_energy':'2pJ'},
                                 area={'total':{'area':'2500um^2'},'subarray':{'area':'2300um^2','dimensions':'64x64'}}))
        extracted=validation.extract_metrics(case,result)
        self.assertEqual(extracted['area'],2300)
        broken=copy.deepcopy(result);broken['geometry']['entry_count']=32
        with self.assertRaises(ValueError):validation.extract_metrics(case,broken)
        with tempfile.TemporaryDirectory(dir=ROOT) as directory:
            folder=Path(directory);manifest_path=folder/'manifest.yaml';manifest_path.write_text(yaml.safe_dump(manifest))
            binary=folder/'EvaCAM';binary.write_text('mock binary')
            output=folder/'output'
            def fake_run(command,**kwargs):
                Path(command[command.index('-o')+1]).write_text(yaml.safe_dump(result))
                return mock.Mock(returncode=0)
            with mock.patch.object(validation,'MANIFEST',manifest_path), mock.patch.object(validation.subprocess,'run',side_effect=fake_run), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(validation.run_suite(binary,output),0)
                self.assertEqual(json.loads((output/'runs.json').read_text())['runs'][0]['metrics']['latency']['estimate'],.3)
            with mock.patch.object(validation,'MANIFEST',manifest_path), mock.patch.object(validation.subprocess,'run',return_value=mock.Mock(returncode=0)), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(validation.run_suite(binary,output),1)
                failed=json.loads((output/'runs.json').read_text())
                self.assertIn('error',failed['runs'][0])
                self.assertIsNone(failed['runs'][0]['metrics']['latency']['estimate'])
            self.assertEqual(len(list(output.glob('run-*'))),2)

if __name__=='__main__':unittest.main()
