"""Offline circuit/measurement checks; literature disagreement must stay visible."""
import copy
import json
import contextlib
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import numpy as np
from scipy.linalg import expm
import yaml

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import validate_nand_kondo as k
import investigate_nand_kondo as audit


class KondoValidationTest(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory(prefix='kondo-validation-')
        self.addCleanup(self.directory.cleanup)
        self.path=Path(self.directory.name)

    def test_reference_hashes_pixels_and_source_contract(self):
        manifest,curves,extraction=k.load_reference()
        self.assertEqual(len(curves),32)
        self.assertEqual(sum(len(c['points']) for c in curves),296)
        self.assertTrue(manifest['acceptance']['frozen_before_comparison'])
        self.assertIsNone(manifest['source_audit']['spice_section_count'])
        self.assertEqual(k.sha256(k.DATA/'curves.json'),manifest['data_hashes']['curves.json'])
        # Held-back labeled ticks, independently picked from the image, should
        # map within the declared pixel uncertainty of the calibration endpoints.
        for axis in extraction['axes'].values():
            x0,y0,x1,y1=axis['box'];xmin,ymin,xmax,ymax=axis['limits']
            for pixel,value,direction in axis['held_back_ticks']:
                scale=(xmax-xmin)/(x1-x0) if direction=='x' else (ymax-ymin)/(y1-y0)
                actual=(pixel-(x0 if direction=='x' else y0))*scale+(xmin if direction=='x' else ymin)
                self.assertLessEqual(abs(actual-value),abs(scale)*3)

    def test_line_conserves_rc_and_retains_open_tail_at_exact_tap(self):
        for position in (.25,.33,1/3,.66,2/3,1.):
            circuit=k.build_line(24,position,1)
            self.assertAlmostEqual(sum(circuit['capacitances_f'])+circuit['source_capacitance_f'],3e-12,delta=1e-25)
            self.assertAlmostEqual(sum(circuit['series_resistances_ohm'])+circuit['left_resistance_ohm'],1e6)
            self.assertAlmostEqual(circuit['positions'][circuit['tap']],position,places=13)
            self.assertEqual(circuit['positions'][-1],1.)
            reference=k.LineReference(circuit)
            expected=.5/(position*1e6+5e6)
            self.assertAlmostEqual(reference.targets()['sense_current'],expected,delta=1e-20)
            self.assertAlmostEqual(reference.steady[circuit['tap']]*.5/5e6,expected,delta=1e-18)
            np.testing.assert_allclose(reference.steady[circuit['tap']:],reference.steady[-1],atol=1e-12)
        for segments,position,data in [(1,.5,1),(10,0,0),(10,1.1,1),(10,.5,2)]:
            with self.assertRaises(ValueError):k.build_line(segments,position,data)

    def test_independent_reference_matches_dense_exponential_and_dc(self):
        circuit=k.build_line(8,.33,1); ref=k.LineReference(circuit)
        # Assemble a separate nonsymmetric generator directly from edge KCL.
        n=len(circuit['capacitances_f']);g=np.diag(circuit['shunt_conductances_s'])
        for i,r in enumerate(circuit['series_resistances_ohm']):
            g[i,i]+=1/r;g[i+1,i+1]+=1/r;g[i,i+1]-=1/r;g[i+1,i]-=1/r
        drive=np.zeros(n);drive[0]=1/circuit['left_resistance_ohm'];g[0,0]+=drive[0]
        aug=np.zeros((n+1,n+1));aug[:n,:n]=-g/np.array(circuit['capacitances_f'])[:,None]
        aug[:n,-1]=drive*.6/np.array(circuit['capacitances_f'])
        initial=np.r_[np.zeros(n),1.]
        charged=expm(aug*2.4e-6)@initial
        aug[:n,-1]*=.5/.6
        expected=expm(aug*.7e-6)@charged
        np.testing.assert_allclose(ref.state([3.1e-6],2.4e-6)[0],expected[:n],atol=1e-11)
        obs=ref.observe([3.1e-6],2.4e-6)
        self.assertAlmostEqual(obs['voltage'][0],expected[circuit['tap']],places=11)
        self.assertAlmostEqual(obs['sense_current'][0],(.5-expected[0])/circuit['left_resistance_ohm'],delta=1e-16)
        with self.assertRaises(ValueError):ref.state([-1],0)
        with self.assertRaises(ValueError):ref.observe([0],-1)

    def test_settling_uses_last_entry_and_allows_before_transition(self):
        measured=k.settling([0,1,2,3,4,5],[0,1,1,1.2,1,1],(.9,1.1),True)
        self.assertAlmostEqual(measured['time_s'],3.5)
        self.assertEqual(measured['bracket_s'],[3.,4.])
        before=k.settling([0,1,2,2,3],[0,1,1,1.01,1],(.9,1.1),True)
        self.assertAlmostEqual(before['time_s'],.9)
        jump=k.settling([0,1,1,2],[2,2,1,1],(.9,1.1),True)
        self.assertEqual(jump['time_s'],1)
        self.assertEqual(k.settling([0,1],[1,1],(.9,1.1),True)['time_s'],0)
        for values,tail in [([1,1],False),([1,2],True)]:
            self.assertEqual(k.settling([0,1],values,(.9,1.1),tail)['status'],'unresolved')
        with self.assertRaises(ValueError):k.settling([1,0],[0,1],(.9,1.1),True)

    def test_windows_preserve_nominal_and_data0_ambiguity(self):
        circuit=k.build_line(8,1.,1)
        self.assertLess(k.windows(circuit)['voltage'][0],.4)
        self.assertEqual(k.windows(circuit,'nominal')['voltage'][0],.45)
        self.assertFalse(k.tail_is_safe(k.LineReference(circuit).steady*.5,circuit,'voltage',k.windows(circuit,'nominal')['voltage']))
        zero=k.build_line(8,1.,0)
        self.assertEqual(k.windows(zero,'data0_upper_only')['sense_current'][0],float('-inf'))
        with self.assertRaises(ValueError):k.windows(zero,'invented')

    def test_schedule_and_transition_current_limits(self):
        circuit=k.build_line(8,.25,1); ref=k.LineReference(circuit)
        times=k.observation_times(2.4e-6,spacing=1e-6)
        self.assertIn(2.4e-6,times)
        t,obs,state=k.reference_trace(ref,2.4e-6,times)
        indices=np.flatnonzero(t==2.4e-6)
        self.assertEqual(len(indices),2)
        self.assertAlmostEqual(obs['sense_current'][indices[0]]-obs['sense_current'][indices[1]],.1/circuit['left_resistance_ohm'],delta=1e-18)
        self.assertAlmostEqual(obs['voltage'][indices[0]],obs['voltage'][indices[1]])
        self.assertTrue(k.tail_is_safe(state,circuit,'sense_current',k.windows(circuit)['sense_current']))
        self.assertEqual(k.measure_trace(t,obs,state,circuit)['voltage']['status'],'settled')
        with self.assertRaises(ValueError):k.observation_times(1,0)

    def test_compiled_waveform_carries_state_and_matches_reference(self):
        circuit=k.build_line(12,.33,1)
        pulse=2.4e-6;times=k.observation_times(pulse,spacing=20e-9)
        t,obs,result=k.run_probe(circuit,pulse,times,self.path)
        exact=k.LineReference(circuit).observe(t,pulse)
        indices=np.flatnonzero(np.isclose(t,pulse,atol=1e-20,rtol=0))
        self.assertEqual(len(indices),2)
        self.assertEqual(result['samples'][indices[0]]['voltages_v'],result['samples'][indices[1]]['voltages_v'])
        exact['sense_current'][indices[0]]+=.1/circuit['left_resistance_ohm']
        np.testing.assert_allclose(obs['voltage'],exact['voltage'],rtol=0,atol=10e-6)
        np.testing.assert_allclose(obs['sense_current'],exact['sense_current'],rtol=0,atol=.01e-9)
        final=result['samples'][-1]
        charge=np.dot(circuit['capacitances_f'],result['final_voltages_v'])
        self.assertAlmostEqual(charge,final['left_source_charge_c']-final['ground_shunt_charge_c'],delta=1e-22)
        self.assertGreater(result['source_endpoint_switch_charge_c'],0)
        self.assertGreater(result['source_endpoint_capacitor_energy_change_j'],0)
        self.assertIsNone(result['source_endpoint_supply_energy_j'])
        self.assertLess(result['source_endpoint_switches'][1]['charge_c'],0)

    def test_compiled_grounded_load_nonuniform_state_and_two_drives(self):
        payload=dict(capacitances_f=[2.,3.],series_resistances_ohm=[2.],
                     initial_voltages_v=[-.2,.7],shunt_conductances_s=[.2,.3],duration_s=.8,
                     left=dict(connected=True,resistance_ohm=1.,voltage_v=.4),
                     right=dict(connected=True,resistance_ohm=2.,voltage_v=-.1),
                     solver=dict(max_step_s=.01,tolerance_v=1e-10,max_steps=100000))
        path=self.path/'two.yaml';path.write_text(yaml.safe_dump(payload))
        proc=subprocess.run([str(k.PROBE),str(path)],capture_output=True,text=True,check=True)
        actual=yaml.safe_load(proc.stdout)
        g=np.array([[1.7,-.5],[-.5,1.3]])
        a=np.zeros((3,3));a[:2,:2]=-g/np.array([2,3])[:,None];a[:2,2]=np.array([.4,-.05])/[2,3]
        expected=expm(a*.8)@np.array([-.2,.7,1])
        np.testing.assert_allclose(actual['voltages_v'],expected[:2],rtol=0,atol=1e-8)
        self.assertAlmostEqual(actual['capacitor_charge_change_c'],actual['left_source_charge_c']+actual['right_source_charge_c']-actual['ground_shunt_charge_c'],delta=1e-12)

    def test_compiled_probe_rejects_schedule_and_budget_errors(self):
        circuit=k.build_line(8,1.,1)
        k.run_probe(circuit,0,np.array([0,1e-6]),self.path)
        payload=yaml.safe_load((self.path/'circuit.yaml').read_text())
        for mutate in (lambda p:p['phases'][0].update(observation_times_s=[0,2e-6,1e-6]),
                       lambda p:p.update(observed_nodes=[100]),
                       lambda p:p['solver'].update(max_steps=1)):
            bad=copy.deepcopy(payload);mutate(bad)
            path=self.path/'bad.yaml';path.write_text(yaml.safe_dump(bad))
            process=subprocess.run([str(k.PROBE),str(path)],capture_output=True,text=True)
            self.assertNotEqual(process.returncode,0)

    def test_evaluate_case_refines_crossings_and_checks_timing(self):
        row,trace=k.evaluate_case(k.build_line(12,1.,1),2e-6,self.path,'case',spacing=80e-9)
        self.assertLess(row['numerical_error']['voltage']['maximum'],10e-6)
        self.assertLess(row['numerical_error']['sense_current']['maximum'],.01e-9)
        for obs in ('voltage','sense_current'):
            self.assertLess(abs(row['numerical_timing_error_s'][obs]),5e-9)
            bounds=row['measurements']['loaded'][obs]['bracket_s']
            self.assertLessEqual(bounds[1]-bounds[0],1.01e-9)
        # Nominal targets that are unreachable stay unresolved.
        self.assertEqual(row['measurements']['nominal']['voltage']['status'],'unresolved')
        exact=k.independent_delays(k.LineReference(k.build_line(12,1.,1)),2e-6)
        self.assertAlmostEqual(exact['voltage']['time_s'],row['measurements']['loaded']['voltage']['time_s'],delta=5e-9)

    def test_residuals_keep_sign_and_do_not_hide_disagreement(self):
        result=k.residual_metrics([1.,2.],[.8,2.3],[.01,.01],2.,timing=True)
        np.testing.assert_allclose(result['signed_residuals'],[-.2,.3])
        self.assertFalse(result['within_rule'])
        self.assertAlmostEqual(result['bias'],.05)
        self.assertEqual(result['point_count'],2)

    def test_late_data0_alternative_has_a_refined_crossing(self):
        circuit=k.build_line(8,.25,0)
        row,_=k.evaluate_case(circuit,0,self.path,'late-data0')
        actual=row['measurements']['data0_upper_only']['sense_current']
        expected=k.independent_delays(k.LineReference(circuit),0,'data0_upper_only',spacing=2e-9)['sense_current']
        self.assertEqual(actual['status'],'settled')
        self.assertLess(actual['bracket_s'][1]-actual['bracket_s'][0],1.01e-9)
        self.assertAlmostEqual(actual['time_s'],expected['time_s'],delta=5e-9)

    def test_main_rejects_invalid_mesh_before_starting_sweeps(self):
        with self.assertRaises(SystemExit) as error:
            k.main(['--segments','1','--output',str(self.path)])
        self.assertEqual(error.exception.code,2)
        self.assertFalse((self.path/'report.json').exists())

    def test_plot_report_writes_all_figures_with_source_points(self):
        manifest,curves,_=k.load_reference()
        row,trace=k.evaluate_case(k.build_line(8,1.,1),2e-6,self.path,'plot-case',spacing=100e-9)
        selected=[c for c in curves if c['id']=='fig7_0_cell_current']
        comparisons={c['id']:dict(predicted=[p['y'] for p in c['points']]) for c in selected}
        k.plot_report(self.path,selected,comparisons,[row],{(1.,2e-6):trace},
                      dict(mesh=[dict(segments=8,voltage=1e-6,sense_current=2e-6)]))
        for name in ('current-waveforms','voltage-waveforms','delay-comparisons','convergence-and-definitions'):
            self.assertGreater((self.path/(name+'.png')).stat().st_size,1000)
            self.assertIn('<svg',(self.path/(name+'.svg')).read_text())

    def test_main_preserves_unvalidated_status_when_numerics_pass(self):
        row,trace=k.evaluate_case(k.build_line(8,1.,1),2e-6,self.path,'entry',spacing=100e-9)
        manifest,_,extraction=k.load_reference()
        with mock.patch.object(k,'load_reference',return_value=(manifest,[],extraction)), \
             mock.patch.object(k,'evaluate_case',return_value=(row,trace)), \
             mock.patch.object(k,'plot_report'), contextlib.redirect_stdout(io.StringIO()):
            status=k.main(['--segments','8','--jobs','1','--output',str(self.path),'--require-reproduction'])
        self.assertEqual(status,2)
        report=json.loads((self.path/'report.json').read_text())
        self.assertTrue(report['numerical']['passes'])
        self.assertEqual(report['status'],'incomplete_paper_reproduction')
        self.assertEqual(report['provenance']['executable_sha256'],k.sha256(k.PROBE))

    def test_circuit_hypotheses_conserve_totals_and_reject_off_grid_taps(self):
        for topology in audit.TOPOLOGIES:
            circuit=audit.build_hypothesis(topology,.25,1)
            self.assertAlmostEqual(sum(circuit['capacitances_f'])+circuit['source_capacitance_f'],3e-12,delta=1e-25)
            self.assertAlmostEqual(sum(circuit['series_resistances_ohm'])+circuit['left_resistance_ohm'],1e6)
            self.assertEqual(circuit['cell_resistance_ohm'],5e6)
        lumped=audit.build_hypothesis('l_sections_8',.25,1)
        self.assertEqual(lumped['capacitances_f'],[3e-12/8]*8)
        self.assertEqual(lumped['source_capacitance_f'],0)
        self.assertEqual(lumped['tap'],1)
        self.assertEqual(lumped['series_resistances_ohm'],[1e6/8]*7)
        with self.assertRaises(ValueError):audit.build_hypothesis('l_sections_8',.33,1)
        with self.assertRaises(ValueError):audit.build_hypothesis('invented',1.,0)

    def test_diagnostic_current_rules_and_analytic_crossings(self):
        circuit=k.build_line(2,1.,0)
        circuit.update(capacitances_f=[3e-12],series_resistances_ohm=[],
                       initial_voltages_v=[0.],positions=[1.],tap=0,
                       shunt_conductances_s=[1/50e6],left_resistance_ohm=1e6,
                       source_capacitance_f=0.)
        reference=k.LineReference(circuit)
        dc=.5/51e6
        tau=3e-12/(1/1e6+1/50e6)
        for rule in audit.RULES:
            window=audit.current_window(circuit,rule)
            expected=tau*np.log((.5/1e6-dc)/(window[1]-dc))
            result=audit.diagnostic_delays(reference,0,rule)['sense_current']
            self.assertAlmostEqual(result['time_s'],expected,delta=1e-9)
            self.assertLessEqual(result['bracket_s'][1]-result['bracket_s'][0],.501e-9)
        self.assertEqual(audit.current_window(circuit,'data0_upper_20na'),(float('-inf'),20e-9))
        one=audit.build_hypothesis('l_sections_8',1.,1)
        self.assertEqual(audit.current_window(one,'data0_upper_20na'),k.windows(one)['sense_current'])
        with self.assertRaises(ValueError):audit.current_window(circuit,'invented')

    def test_assumption_comparison_retains_raw_residuals(self):
        _,curves,_=k.load_reference()
        curve=next(c for c in curves if c['id']=='fig7_0_cell_current')
        reference=k.LineReference(audit.build_hypothesis('l_sections_8',1.,1))
        result=audit.compare_curve(curve,reference)
        exact=reference.observe([p['x'] for p in curve['points']],2e-6)['cell_current']
        np.testing.assert_allclose(result['predicted'],exact,rtol=0,atol=1e-20)
        np.testing.assert_allclose(result['signed_residuals'],exact-np.array([p['y'] for p in curve['points']]))
        self.assertEqual(result['status'],'exploratory_unconfirmed_circuit')
        self.assertEqual(result['point_count'],len(curve['points']))

    def test_assumptions_report_exclusions_provenance_and_plots(self):
        _,curves,_=k.load_reference()
        # Keep all curve identities/branches with a small deterministic case set.
        selected=copy.deepcopy(curves)
        for curve in selected:curve['points']=curve['points'][:1]
        result=audit.run_audit(self.path,selected,jobs=2)
        self.assertEqual(result['status'],'exploratory_incomplete_paper_reproduction')
        self.assertTrue(result['numerical']['passes'])
        self.assertIn('fig4a_third1',result['exclusions'])
        self.assertIn('fig4b_sense_delay',result['exclusions'])
        self.assertEqual(set(result['comparisons']),set(audit.TOPOLOGIES))
        self.assertEqual(len(result['inferred_data0_thresholds']),1)
        for filename,digest in result['provenance']['source_hashes'].items():
            self.assertEqual(k.sha256(ROOT/filename),digest)
        for name in ('voltage-circuit-assumptions','current-circuit-assumptions','current-measurement-assumptions'):
            self.assertGreater((self.path/(name+'.png')).stat().st_size,1000)
            self.assertIn('<svg',(self.path/(name+'.svg')).read_text())
        self.assertEqual(json.loads((self.path/'report.json').read_text())['status'],result['status'])

    def test_assumptions_main_cannot_claim_reproduction(self):
        with mock.patch.object(audit,'run_audit',return_value={'numerical':{'passes':True}}), \
             contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(audit.main(['--output',str(self.path),'--require-reproduction']),2)
        with self.assertRaises(SystemExit) as error:
            audit.main(['--jobs','0'])
        self.assertEqual(error.exception.code,2)


if __name__=='__main__':unittest.main()
