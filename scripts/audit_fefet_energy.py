#!/usr/bin/env python3
"""Audit FeFET energy ownership and unverified sensing assumptions without fitting."""
from __future__ import annotations

import argparse
import copy
from datetime import datetime, timezone
import itertools
import json
import math
from pathlib import Path
import subprocess
import tempfile

import yaml

from compare_legacy_evacam import ROOT, check_budgets, load_deck, run_probe, sha, write_deck


def query_line_energy(capacitances, supply, previous, query, reset):
    """Ideal supply work on complementary binary lines; first line is high for 0.

    This accounts only for external line capacitance. Driver internals, matchlines,
    control nodes and sensing cannot be inferred from the query alone.
    """
    if len(capacitances) != 2 or any(not math.isfinite(c) or c < 0 for c in capacitances):
        raise ValueError('Two finite nonnegative line capacitances are required')
    if not math.isfinite(supply) or supply <= 0:
        raise ValueError('Supply must be finite and positive')
    if len(previous) != len(query) or not query or any(bit not in (0, 1) for bit in [*previous, *query]):
        raise ValueError('Queries must be equal-length nonempty binary vectors')
    energy = 0.0
    for old, new in zip(previous, query):
        before = (0, 0) if reset else (1 - old, old)
        after = (1 - new, new)
        for capacitance, initial, final in zip(capacitances, before, after):
            if final > initial:
                energy += capacitance * supply * supply
    return energy


def energy_audit(values):
    """Reconcile native budgets and enumerate independent query transitions."""
    check_budgets(values)
    supply = values['input.vdd_v']
    bits = int(values['input.comparison_bits'])
    if values['input.row_ports'] != 2 or bits <= 0:
        raise ValueError('Audit requires two complementary binary searchlines')
    caps, line_energy, native_driver = [], 0.0, 0.0
    for i in range(2):
        levels = [values[f'input.port0.{i}.search{bit}'] for bit in (0, 1)]
        if levels != ([supply, 0] if i == 0 else [0, supply]):
            raise ValueError('Audit requires complementary 0/Vdd searchline voltages')
        capacitance = values[f'current.row{i}_driver_load_f']
        physical = values[f'current.row{i}_wire_cap_f'] + values[f'current.row{i}_device_cap_f']
        if not math.isclose(capacitance, physical, rel_tol=1e-10, abs_tol=1e-25):
            raise ValueError('Reference driver must use the physical line load')
        caps.append(capacitance)
        line_energy += .5 * capacitance * supply**2 * bits
        native_driver += .5 * values[f'current.row{i}_driver_energy_j'] * bits
    recorded = values['current.energy.row_driver']
    if not math.isclose(native_driver, recorded, rel_tol=1e-10, abs_tol=1e-25) or line_energy > recorded:
        raise ValueError('Driver energy ownership does not reconcile')
    transitions = [(old, new) for old, new in itertools.product((0, 1), repeat=2)]
    reset = sum(query_line_energy(caps, supply, [a], [b], True) for a, b in transitions) / 4 * bits
    retained = sum(query_line_energy(caps, supply, [a], [b], False) for a, b in transitions) / 4 * bits
    if not math.isclose(reset, line_energy, rel_tol=1e-10, abs_tol=1e-25):
        raise ValueError('Independent reset charging does not match native load energy')
    return {
        'status': 'component_accounting_verified_workload_unmatched',
        'scope': 'single_subarray_full_configured_search',
        'native_bank_energy_j': values['energy_j'],
        'native_subarray_energy_j': values['current.sub_energy_j'],
        'searchline_load_j': line_energy,
        'driver_internal_j': recorded - line_energy,
        'matchline_recharge_j': values['current.energy.matchline'],
        'other_configured_subarray_j': values['current.sub_energy_j'] - recorded - values['current.energy.matchline'],
        'external_line_protocols_j': {
            'reset_each_query_equiprobable': reset,
            'retained_independent_equiprobable_queries': retained,
            'retained_repeated_query': 0.0,
            'retained_complemented_query_equiprobable': reset,
        },
        'line_only_sensitivity_bank_energy_j': values['energy_j'] - line_energy + retained,
        'sensitivity_limitation': 'Only external line charging changes. Driver-internal activity, ML discharge, control nodes and peripherals stay at native assumptions. This is not whole-workload energy or a paper accuracy score.',
    }


def sensing_stages():
    """Return explicit alternatives, each labelled with its change and parent."""
    name = 'FeFET-2Fe1T-DATE21'
    native = load_deck(ROOT / f'config/paper_reference/{name}/{name}.config.yaml')
    inverter = yaml.safe_load(Path(native['sensing']['sense_amplifier']).read_text())
    stages = [('reference', native, None, 'Current assumed inverter and 1F pull-down.')]
    generic = copy.deepcopy(native)
    generic['sensing']['sense_amplifier'] = str(ROOT / 'config/lib/sense_amp/nvsim_vol.sense_amp.yaml')
    generic['sensing']['decision'] = {'model': 'voltage_threshold', 'threshold': '0.5V'}
    stages.append(('generic-SA-fixed-0.5V', generic, None, 'Prior generic SA and fixed decision, with current consistent energy accounting.'))
    fixed = copy.deepcopy(native)
    fixed['sensing']['decision'] = {'model': 'voltage_threshold', 'threshold': '0.5V'}
    stages.append(('inverter-fixed-0.5V', fixed, None, 'Versus generic-SA-fixed-0.5V: replace only the sensing circuit. Versus reference: replace only the decision.'))
    for width in (1.5, 2, 10):
        deck = copy.deepcopy(native)
        deck['cell']['ports']['column'][0]['cmos_width'] = f'{width}F'
        stages.append((f'pulldown-{width}F', deck, None, 'Change only the unverified pull-down width; not an evidence-backed uncertainty bound.'))
    for load in (1, 5):
        model = copy.deepcopy(inverter)
        model['output_capacitance'] = f'{load}fF'
        stages.append((f'inverter-output-{load}fF', copy.deepcopy(native), model, 'Change only external inverter output load; not paper-characterized.'))
    for width in (1, 3):
        model = copy.deepcopy(inverter)
        model['transistors'] = {'n_sense_width': f'{width}F', 'p_sense_width': f'{width}F'}
        stages.append((f'inverter-width-{width}F', copy.deepcopy(native), model, 'Change both inverter widths together; illustrative sizing alternative.'))
    return stages


def render_report(report):
    lines = ['# FeFET energy and sensing audit', '',
             'All reference fixtures remain partial. Alternative inputs are sensitivity cases, not fitted configurations or confidence bounds.', '',
             '| Fixture | Bank pJ | ML recharge pJ | SL load pJ | Driver internal pJ | Other subarray pJ |',
             '| --- | ---: | ---: | ---: | ---: | ---: |']
    for run in report['energy_runs']:
        if 'error' in run:
            lines += [f"| {run['id']} | FAILED | — | — | — | — |"]
            continue
        a = run['audit']
        lines.append('| ' + run['id'] + ' | ' + ' | '.join(f'{a[k]*1e12:.6f}' for k in (
            'native_bank_energy_j', 'matchline_recharge_j', 'searchline_load_j', 'driver_internal_j', 'other_configured_subarray_j')) + ' |')
    lines += ['', 'SL load is already inside row-driver energy. It must not be added again.',
              'For independent equiprobable queries, retaining the previous query halves external SL charging compared with resetting both lines. This does not establish whole-query energy.', '',
              '| Fixture | Reset SL pJ | Retained random SL pJ | Repeated-query SL pJ | Bank pJ changing only external SL activity |',
              '| --- | ---: | ---: | ---: | ---: |']
    for run in report['energy_runs']:
        if 'error' in run: continue
        a = run['audit']
        p = a['external_line_protocols_j']
        lines.append(f"| {run['id']} | {p['reset_each_query_equiprobable']*1e12:.6f} | {p['retained_independent_equiprobable_queries']*1e12:.6f} | 0 | {a['line_only_sensitivity_bank_energy_j']*1e12:.6f} |")
    lines += ['', 'The last column freezes all other terms; it is a sensitivity projection, not a new production result.', '',
              '| FeFET-1T case | Search ps | ML ps | SA ps | Energy pJ | SA input fF |',
              '| --- | ---: | ---: | ---: | ---: | ---: |']
    for run in report['sensing_runs']:
        if 'error' in run:
            lines.append(f"| {run['id']} | FAILED | — | — | — | — |")
            continue
        v = run['values']
        lines.append(f"| {run['id']} | {v['search_s']*1e12:.3f} | {v['ml_s']*1e12:.3f} | {v['sa_s']*1e12:.3f} | {v['energy_j']*1e12:.6f} | {v['sa_cap_f']*1e15:.6f} |")
    for run in [*report['energy_runs'], *report['sensing_runs']]:
        if 'error' in run: lines += ['', run['id'] + ': ' + run['error']]
    lines += ['', 'Paper workload, exact pull-down sizing, inverter sizing/output load, and full peripheral boundary remain unresolved.', '']
    return '\n'.join(lines)


def run_suite(probe, output):
    output.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix='run-', dir=output))
    energy_runs, sensing_runs = [], []
    for name, group in [('FeFET-2Fe-TCAS19', 'original_validation'), ('FeFET-2Fe1T-DATE21', 'paper_reference')]:
        source = ROOT / f'config/{group}/{name}/{name}.config.yaml'
        target = folder / name
        config = write_deck(load_deck(source), target)
        run = {'id': name, 'config': str(config)}
        try:
            run['values'] = run_probe(probe, config, target, 'current')
            run['audit'] = energy_audit(run['values'])
        except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
            run['error'] = str(error)
        energy_runs.append(run)
    for label, deck, inverter, reason in sensing_stages():
        target = folder / label
        target.mkdir()
        if inverter is not None:
            amp = target / 'inverter.sense_amp.yaml'
            amp.write_text(yaml.safe_dump(inverter, sort_keys=False))
            deck['sensing']['sense_amplifier'] = str(amp)
        config = write_deck(deck, target)
        run = {'id': label, 'config': str(config), 'change': reason}
        try:
            run['values'] = run_probe(probe, config, target, 'current')
            check_budgets(run['values'])
            if not run['values']['feasible']: raise ValueError('Infeasible sensing case')
        except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
            run['error'] = str(error)
        sensing_runs.append(run)
    inputs = set(folder.rglob('*.yaml'))
    inputs.update((ROOT / 'config/lib').rglob('*.yaml'))
    inputs.update((ROOT / 'config/original_validation/FeFET-2Fe-TCAS19').glob('*.yaml'))
    inputs.update((ROOT / 'config/paper_reference/FeFET-2Fe1T-DATE21').glob('*.yaml'))
    report = {'created_utc': datetime.now(timezone.utc).isoformat(), 'probe_sha256': sha(probe),
              'runner_sha256': sha(Path(__file__)), 'shared_runner_sha256': sha(ROOT / 'scripts/compare_legacy_evacam.py'),
              'input_sha256': {str(p): sha(p) for p in sorted(inputs)},
              'energy_runs': energy_runs, 'sensing_runs': sensing_runs}
    text = render_report(report)
    for directory in (folder, output):
        (directory / 'runs.json').write_text(json.dumps(report, indent=2, allow_nan=False) + '\n')
        (directory / 'comparison.md').write_text(text)
    print(text)
    print('Evidence: ' + str(folder))
    return int(any('error' in r for r in [*energy_runs, *sensing_runs]))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, default=ROOT / 'test-bin/LegacyComparisonProbe')
    parser.add_argument('--output', type=Path, default=ROOT / 'output/validation/fefet-energy-sensing')
    args = parser.parse_args()
    raise SystemExit(run_suite(args.probe.resolve(), args.output.resolve()))
