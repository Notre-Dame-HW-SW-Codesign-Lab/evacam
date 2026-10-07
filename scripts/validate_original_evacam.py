#!/usr/bin/env python3
"""Reproduce the original validation contract without equating unlike metrics."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import subprocess
import tempfile

import yaml
from validate_named_cam import ROOT, quantity, digest, TIME_NS, ENERGY_PJ, AREA_UM2

MANIFEST = ROOT / 'docs/validation/original-evacam.reference.yaml'
LEDGER = ROOT / 'docs/validation/original-evacam.inputs.yaml'


def compare_metric(observation, estimate, comparable):
    """Keep diagnostic differences distinct from validated errors and bounds."""
    actual = observation['value']
    if estimate is not None and (not math.isfinite(estimate) or estimate < 0):
        raise ValueError('Estimate must be finite and nonnegative')
    if actual is not None and (not math.isfinite(actual) or actual <= 0):
        raise ValueError('Reference must be finite and positive')
    operator = observation.get('operator', 'equal')
    if operator not in ('equal', 'lower_bound', 'upper_bound'):
        raise ValueError('Unsupported observation operator')
    bounded = operator != 'equal'
    gap = 100 * (estimate / actual - 1) if actual is not None and estimate is not None and not bounded else None
    return dict(reference=actual, estimate=estimate, unit=observation['unit'],
                reference_scope=observation['scope'], reference_kind=observation['kind'],
                diagnostic_gap_percent=gap, error_percent=gap if comparable else None,
                status='unavailable' if estimate is None or actual is None else
                'bound_not_point' if bounded else 'matched' if comparable else 'unmatched_scope_or_inputs')


def sapiens_schedule(workload):
    """Clock arithmetic only: measured clock/power are inputs, never predictions."""
    for key in ('physical_rram_devices', 'devices_per_bit', 'subarrays', 'vectors_per_subarray',
                'vector_bits', 'sense_amplifiers', 'bitline_mux', 'bits_per_cycle', 'clock_mhz', 'power_mw'):
        if not isinstance(workload[key], (int, float)) or not math.isfinite(workload[key]) or workload[key] <= 0:
            raise ValueError('Invalid SAPIENS workload input: ' + key)
    for key in ('physical_rram_devices', 'devices_per_bit', 'subarrays', 'vectors_per_subarray', 'vector_bits', 'sense_amplifiers', 'bitline_mux', 'bits_per_cycle'):
        if isinstance(workload[key], bool) or int(workload[key]) != workload[key]:
            raise ValueError('Workload counts must be integers')
    if workload['bits_per_cycle'] not in (1, 2):
        raise ValueError('SAPIENS supports one or two bits per cycle')
    capacity = workload['physical_rram_devices'] / workload['devices_per_bit']
    if capacity != workload['subarrays'] * workload['vectors_per_subarray'] * workload['vector_bits']:
        raise ValueError('Physical devices and logical capacity disagree')
    if workload['sense_amplifiers'] * workload['bitline_mux'] != workload['subarrays'] * workload['vectors_per_subarray']:
        raise ValueError('Sense amplifier mux does not cover stored vectors')
    cycle_ns = 1000 / workload['clock_mhz']
    cycles = math.ceil(workload['vector_bits'] / workload['bits_per_cycle'])
    query_ns = cycles * cycle_ns
    return dict(kind='derived_from_measured_inputs_not_model_validation', logical_bits=int(capacity),
                clock_period_ns=cycle_ns, cycles_per_32vector_query=cycles,
                query_ns=query_ns, serial_eight_bank_query_ns=query_ns * workload['bitline_mux'],
                measured_power_times_query_pj=workload['power_mw'] * query_ns,
                normalization_status='Published 270pJ chart normalization is unresolved; do not divide by eight to force agreement.')


def audit_inputs(root, ledger):
    """Fail when a numerical/configuration input loses provenance or drifts."""
    def leaves(node, pointer=''):
        if isinstance(node, (dict, list)):
            items = node.items() if isinstance(node, dict) else enumerate(node)
            for key, value in items:
                yield from leaves(value, pointer + '/' + str(key))
        else:
            yield pointer, node
    actual = {(str(path.relative_to(root)), pointer): value
              for path in (root / 'config/original_validation').rglob('*.yaml')
              for pointer, value in leaves(yaml.safe_load(path.read_text()))}
    entries = ledger['inputs']
    recorded = {(item['file'], item['field']): item['value'] for item in entries}
    if len(recorded) != len(entries) or actual != recorded:
        raise ValueError('Original validation fixture/provenance drift')
    allowed = {'paper', 'configuration', 'inherited', 'approximation', 'unavailable', 'illustrative_curve', 'analytical_model'}
    if any(item['status'] not in allowed or not item.get('basis') for item in entries):
        raise ValueError('Every input needs an evidence status and basis')
    return {status: sum(item['status'] == status for item in entries) for status in sorted(allowed)}


def extract_metrics(case, result):
    geometry = result['geometry']
    expected = case['geometry']
    for key, value in {'entry_count': expected['entries'], 'logical_word_width_bits': expected['word_bits'],
                       'physical_cell_count': expected['entries'] * expected['word_bits']}.items():
        if geometry.get(key) != value:
            raise ValueError('Geometry drift: ' + key)
    summary = result['summary']
    if summary['timing']['sense_margin_pass'] is not True:
        raise ValueError('Invalid sense margin')
    if summary['area']['subarray']['dimensions'] != 'x'.join(map(str, expected['subarray'])):
        raise ValueError('Subarray dimensions drift')
    node = quantity(result['assumptions']['technology']['process_node'], {'nm': 1})
    if node != expected['node_nm']:
        raise ValueError('Process node drift')
    return dict(latency=quantity(summary['timing']['search_latency'], TIME_NS),
                energy=quantity(summary['power']['search_dynamic_energy'], ENERGY_PJ),
                area=quantity(summary['area']['subarray' if case['observations']['area']['scope']=='subarray' else 'total']['area'], AREA_UM2),
                subarray_area=quantity(summary['area']['subarray']['area'], AREA_UM2))


def render_report(report):
    lines = ['# Original EvaCAM validation audit', '',
             'Published EvaCAM values are from DATE 2022 Tables I–II, not a rerun of a publication binary.',
             'Current estimates use the traced fixtures. Diagnostic gaps are not validated error percentages.', '',
             '| Case | Metric | Reference | Published EvaCAM | Current | Diagnostic gap | Status |',
             '| --- | --- | ---: | ---: | ---: | ---: | --- |']
    def number(value):
        return '—' if value is None else '–'.join(map(str, value)) if isinstance(value, list) else f'{value:.6g}'
    for run in report['runs']:
        for metric, data in run['metrics'].items():
            gap = data['diagnostic_gap_percent']
            gap_text = '—' if gap is None else f'{gap:+.1f}%'
            lines.append(f"| {run['id']} | {metric} ({data['unit']}) | {number(data['reference'])} | "
                         f"{number(run['published_evacam'][metric])} | {number(data['estimate'])} | "
                         f"{gap_text} | {data['status']} |")
    for run in report['runs']:
        lines += ['', f"**{run['id']}: {run['status']}**", '', *['- ' + item for item in run['blockers']], '']
        if 'error' in run:
            lines += ['Regression failure: ' + run['error'], '']
    schedule = report['sapiens_schedule']
    lines += ['SAPIENS scope audit: ' + f"{schedule['clock_period_ns']:g}ns clock, {schedule['query_ns']:g}ns per 32-vector query, "
              + f"{schedule['serial_eight_bank_query_ns']:g}ns for eight sequential mux groups (derived schedule).",
              f"Measured power × query time = {schedule['measured_power_times_query_pj']:g}pJ; the 270pJ chart normalization remains unresolved.",
              '', 'Input provenance counts: `' + json.dumps(report['input_audit'], sort_keys=True) + '`.', '']
    return '\n'.join(lines)


def run_suite(binary, output):
    manifest = yaml.safe_load(MANIFEST.read_text())
    if manifest['schema'] != 'original_evacam_validation' or manifest['version'] != 1:
        raise ValueError('Unsupported original validation manifest')
    if len({case['id'] for case in manifest['cases']}) != len(manifest['cases']):
        raise ValueError('Duplicate case')
    ledger = yaml.safe_load(LEDGER.read_text())
    audit = audit_inputs(ROOT, ledger)
    output.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix='run-', dir=output))
    runs = []
    for case in manifest['cases']:
        run = {key: case[key] for key in ('id', 'status', 'blockers', 'published_evacam')}
        estimates = {}
        if case['config']:
            result_path = folder / (case['id'] + '.results.yaml')
            log = folder / (case['id'] + '.log')
            command = [str(binary), '-t', '1', '-o', str(result_path), str(ROOT / case['config'])]
            run.update(command=command, result=str(result_path), log=str(log))
            try:
                with log.open('w') as stream:
                    process = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, timeout=60)
                if process.returncode != 0:
                    raise ValueError('EvaCAM failed: ' + str(process.returncode))
                result = yaml.safe_load(result_path.read_text())
                modeling = result['assumptions']['modeling_options']
                if any(modeling.get(key) != value for key, value in case.get('expected_model', {}).items()):
                    raise ValueError('Circuit model drift')
                has_solution = result.get('status') != 'no_valid_solutions' and 'search_latency' in result.get('summary', {}).get('timing', {})
                if has_solution != case['expected_solution']:
                    raise ValueError('Feasibility changed; review the evidence contract')
                if has_solution:
                    estimates = extract_metrics(case, result)
                    run['model_assumptions'] = result['assumptions']
                elif 'No valid solutions.' not in log.read_text():
                    raise ValueError('Missing solution without an explicit feasibility rejection')
                else:
                    run['status'] = 'infeasible_inherited_electrical_inputs'
            except (OSError, ValueError, KeyError, TypeError, yaml.YAMLError, subprocess.TimeoutExpired) as error:
                run['error'] = str(error)
                run['status'] = 'regression_failure'
        run['metrics'] = {name: compare_metric(obs, estimates.get(name), case['comparable'][name])
                          for name, obs in case['observations'].items()}
        runs.append(run)
    inputs = [MANIFEST, LEDGER, *list((ROOT / 'config/original_validation').rglob('*.yaml')),
              *list((ROOT / 'config/lib').rglob('*.yaml'))]
    report = dict(created_utc=datetime.now(timezone.utc).isoformat(), binary_sha256=digest(binary),
                  runner_sha256=digest(Path(__file__)),
                  input_sha256={str(p.relative_to(ROOT)): digest(p) for p in sorted(inputs)},
                  input_audit=audit, reference_manifest=manifest, runs=runs,
                  sapiens_schedule=sapiens_schedule(manifest['sapiens_workload']))
    for directory in (folder, output):
        (directory / 'runs.json').write_text(json.dumps(report, indent=2, allow_nan=False) + '\n')
        (directory / 'comparison.md').write_text(render_report(report))
    print(render_report(report))
    print('Evidence: ' + str(folder))
    return int(any('error' in run for run in runs))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT / 'EvaCAM')
    parser.add_argument('--output', type=Path, default=ROOT / 'output/validation/original-evacam')
    args = parser.parse_args()
    raise SystemExit(run_suite(args.binary.resolve(), args.output.resolve()))
