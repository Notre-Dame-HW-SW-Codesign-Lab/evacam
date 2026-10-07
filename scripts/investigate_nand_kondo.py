#!/usr/bin/env python3
"""Offline, explicitly exploratory audit of Kondo circuit/measurement assumptions.

No parameter optimization. Keep the original benchmark and its frozen inputs
unchanged. Eight plotted spatial samples motivate (but do not prove) an
eight-section, series-R/shunt-C circuit. All publication observations are reused
unchanged; this follow-up is not an independent validation dataset.
"""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path

import numpy as np
import yaml

import validate_nand_kondo as k

CONTRACT = k.ROOT / 'docs/validation/nand-kondo-2022.assumptions.yaml'
TOPOLOGIES = ('distributed_96', 'distributed_8', 'l_sections_8')
RULES = ('loaded', 'data0_upper_10na', 'data0_upper_20na')
PULSES = np.round(np.unique(np.r_[np.arange(0, 5.0001e-6, .2e-6), 2.3e-6]), 14)


def build_hypothesis(topology, position, data):
    if topology not in TOPOLOGIES:
        raise ValueError('unknown circuit hypothesis')
    circuit = k.build_line(96 if topology == 'distributed_96' else 8, position, data)
    if topology == 'l_sections_8':
        if abs(position * 8 - round(position * 8)) > 1e-12:
            raise ValueError('tap is not a node of the eight-section circuit')
        circuit['capacitances_f'] = [3e-12 / 8] * 8
        circuit['source_capacitance_f'] = 0.
    return circuit


def current_window(circuit, rule):
    if rule not in RULES:
        raise ValueError('unknown diagnostic current rule')
    if circuit['data'] == 0 and rule != 'loaded':
        return (float('-inf'), 10e-9 if rule == 'data0_upper_10na' else 20e-9)
    return k.windows(circuit)['sense_current']


def diagnostic_delays(reference, pulse, rule='loaded'):
    bounds = dict(k.windows(reference.circuit),
                  sense_current=current_window(reference.circuit, rule))
    times = k.observation_times(pulse)
    t, observations, state = k.reference_trace(reference, pulse, times)
    extra = []
    for key, window in bounds.items():
        measurement = k.settling(t, observations[key], window,
                                k.tail_is_safe(state, reference.circuit, key, window))
        if measurement['bracket_s']:
            a, b = measurement['bracket_s']
            extra.extend(np.linspace(a, b, max(2, int(np.ceil((b-a)/.5e-9))+1)))
    t, observations, state = k.reference_trace(reference, pulse, np.unique(np.r_[times, extra]))
    return {key: k.settling(t, observations[key], window,
                           k.tail_is_safe(state, reference.circuit, key, window))
            for key, window in bounds.items()}


def compare_curve(curve, reference, rule='loaded'):
    observable = curve['observable']
    timing = observable.endswith('_delay')
    predicted, uncertainties, budgets, pulse_grid = [], [], [], []
    for point in curve['points']:
        if timing:
            pulse = float(PULSES[np.argmin(abs(PULSES-point['x']))])
            key = 'voltage' if observable == 'voltage_delay' else 'sense_current'
            value = diagnostic_delays(reference, pulse, rule)[key]['time_s']
            shifts = [diagnostic_delays(reference, shifted, rule)[key]['time_s']
                      for shifted in (max(0., pulse-point['x_uncertainty']),
                                      pulse+point['x_uncertainty'])]
            numerical = 6e-9  # Original 5 ns solver + 1 ns event allowance.
            pulse_grid.append(pulse)
        else:
            pulse = curve['case']['pulse_s']
            value = float(reference.observe([point['x']], pulse)[observable][0])
            shifts = reference.observe([max(0., point['x']-point['x_uncertainty']),
                                        point['x']+point['x_uncertainty']], pulse)[observable]
            numerical = .01e-9  # Original current solver allowance.
        if value is None or any(shift is None for shift in shifts):
            raise RuntimeError('unresolved diagnostic measurement')
        budget = dict(digitization=point['y_uncertainty'],
                      abscissa_sensitivity=max(abs(shift-value) for shift in shifts),
                      numerical=numerical)
        predicted.append(value)
        budgets.append(budget)
        uncertainties.append(sum(budget.values()))
    metrics = k.residual_metrics([point['y'] for point in curve['points']], predicted,
                                 uncertainties, 200e-9, timing=timing)
    return dict(metrics, predicted=predicted, uncertainty_budget=budgets,
                pulse_grid_s=pulse_grid, status='exploratory_unconfirmed_circuit',
                observable=observable, case=curve['case'])


def plot_audit(output, curves, comparisons, alternatives):
    os.environ.setdefault('MPLCONFIGDIR', str(output/'matplotlib-cache'))
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    plt.rcParams.update({'font.size': 10, 'axes.grid': True, 'grid.alpha': .2})

    fig, axes = plt.subplots(2, 2, figsize=(11, 7), sharex=True, sharey=True)
    for curve, ax in zip([c for c in curves if c['id'].startswith('fig3b')], axes.flat):
        x = np.array([p['x'] for p in curve['points']])*1e6
        for topology, style, color, label in [
                ('distributed_96', '--', '#777777', 'Distributed line (96 sections)'),
                ('distributed_8', ':', '#a67830', '8 finite volumes'),
                ('l_sections_8', '-', '#226bb0', '8 series-R/shunt-C sections')]:
            ax.plot(x, np.array(comparisons[topology][curve['id']]['predicted'])*1e6,
                    style, color=color, label=label)
        ax.errorbar(x, [p['y']*1e6 for p in curve['points']],
                    yerr=[p['y_uncertainty']*1e6 for p in curve['points']],
                    fmt='o', ms=4, mfc='white', color='black', label='Publication')
        ax.set(title=f"Cell at {curve['case']['position']:.0%}; data {curve['case']['data']}",
               xlabel='Pulse width (us)', ylabel='Voltage settling time (us)')
    axes[0, 0].legend(fontsize=8)
    fig.suptitle('Figure 3b: circuit construction, with identical total R and C')
    fig.tight_layout()
    for suffix in ('png', 'svg'):
        fig.savefig(output/f'voltage-circuit-assumptions.{suffix}', dpi=170)
    plt.close(fig)

    fig, axes = plt.subplots(2, 3, figsize=(13, 7), sharex=True, sharey=True)
    for curve in [c for c in curves if c['observable'] in ('cell_current', 'sense_current')]:
        case = curve['case']
        ax = axes[0 if case['position'] == .25 else 1,
                  [2., 2.4, 2.8].index(round(case['pulse_s']*1e6, 1))]
        color = '#226bb0' if curve['observable'] == 'cell_current' else '#c04c22'
        x = np.array([p['x'] for p in curve['points']])*1e6
        for topology, style in [('distributed_96', '--'), ('l_sections_8', '-')]:
            ax.plot(x, np.array(comparisons[topology][curve['id']]['predicted'])*1e9,
                    style, color=color,
                    label=f"{curve['observable'].replace('_', ' ')}: " +
                          ('8 sections' if topology == 'l_sections_8' else 'distributed'))
        ax.errorbar(x, [p['y']*1e9 for p in curve['points']],
                    yerr=[p['y_uncertainty']*1e9 for p in curve['points']],
                    fmt='o', ms=3, mfc='white', color=color)
        ax.set(title=f"Cell at {case['position']:.0%}; pulse {case['pulse_s']*1e6:g} us",
               xlabel='Time (us)', ylabel='Current (nA)', xlim=(0, 10), ylim=(-40, 160))
    axes[0, 0].legend(fontsize=7)
    fig.suptitle('Figures 6/7: predictions at unchanged publication points (open circles)')
    fig.tight_layout()
    for suffix in ('png', 'svg'):
        fig.savefig(output/f'current-circuit-assumptions.{suffix}', dpi=170)
    plt.close(fig)

    fig, axes = plt.subplots(1, 2, figsize=(11, 4.8))
    for ax, data in zip(axes, (1, 0)):
        curve = next(c for c in curves if c['id'] == f'fig4a_far{data}')
        x = np.array([p['x'] for p in curve['points']])*1e6
        ax.plot(x, np.array(comparisons['l_sections_8'][curve['id']]['predicted'])*1e6,
                label='Loaded DC +/-10%', color='#226bb0')
        if data == 0:
            for rule, label, style in [
                    ('data0_upper_10na', 'Below 10 nA (page 6 interpretation)', '--'),
                    ('data0_upper_20na', 'Below 20 nA (exploratory)', '-.')]:
                ax.plot(x, np.array(alternatives[rule]['predicted'])*1e6, style, label=label)
        ax.errorbar(x, [p['y']*1e6 for p in curve['points']],
                    yerr=[p['y_uncertainty']*1e6 for p in curve['points']],
                    fmt='o', ms=4, mfc='white', color='black', label='Publication')
        ax.set(title=f'Far cell; data {data}', xlabel='Pulse width (us)',
               ylabel='Sense-current settling time (us)')
        ax.legend(fontsize=8)
    fig.suptitle('Figure 4a: current measurement remains unconfirmed (8-section circuit)')
    fig.tight_layout()
    for suffix in ('png', 'svg'):
        fig.savefig(output/f'current-measurement-assumptions.{suffix}', dpi=170)
    plt.close(fig)


def run_audit(output, curves, jobs=4):
    output.mkdir(parents=True, exist_ok=True)
    selected = [c for c in curves if c['case'].get('position') in (.25, 1.)
                and not c['case'].get('ambiguous_data_label')]
    references = {(topology, position, data): k.LineReference(build_hypothesis(topology, position, data))
                  for topology in TOPOLOGIES for position in (.25, 1.) for data in (0, 1)}
    comparisons = {topology: {
        c['id']: compare_curve(c, references[(topology, c['case']['position'], c['case']['data'])])
        for c in selected} for topology in TOPOLOGIES}
    far0 = next(c for c in selected if c['id'] == 'fig4a_far0')
    alternatives = {rule: compare_curve(far0, references[('l_sections_8', 1., 0)], rule)
                    for rule in RULES[1:]}
    # Verify the candidate circuit using the compiled production solver at
    # every selected publication case. No C++ model or flash parameters change.
    cases = set()
    for curve in selected:
        case = curve['case']
        pulses = comparisons['l_sections_8'][curve['id']]['pulse_grid_s']
        for pulse in pulses or [case['pulse_s']]:
            cases.add((case['position'], case['data'], pulse))
    inputs = output/'inputs'
    inputs.mkdir(exist_ok=True)
    rows = []
    print(f'Checking {len(cases)} eight-section publication cases with the C++ solver', flush=True)
    with ThreadPoolExecutor(max_workers=jobs) as executor:
        futures = [executor.submit(k.evaluate_case, build_hypothesis('l_sections_8', position, data),
                                   pulse, inputs, f'x{position:g}_d{data}_p{pulse*1e6:g}')
                   for position, data, pulse in sorted(cases)]
        for future in futures:
            row, _ = future.result()
            rows.append(row)
    numerical = dict(cases=len(rows),
                     maximum_voltage_error_v=max(r['numerical_error']['voltage']['maximum'] for r in rows),
                     maximum_current_error_a=max(r['numerical_error']['sense_current']['maximum'] for r in rows),
                     maximum_settling_error_s=max(abs(v) for r in rows for v in r['numerical_timing_error_s'].values() if v is not None),
                     unresolved_cases=sum(any(m['time_s'] is None for m in r['measurements']['loaded'].values()) for r in rows))
    numerical['passes'] = (numerical['maximum_voltage_error_v'] <= 10e-6 and
                           numerical['maximum_current_error_a'] <= .01e-9 and
                           numerical['maximum_settling_error_s'] <= 5e-9 and
                           numerical['unresolved_cases'] == 0)
    # Inverse check of what current exists at the published data-0 delay.
    # The 20 nA alternative is deliberately marked exploratory in the contract.
    inferred = []
    for point, pulse in zip(far0['points'], comparisons['l_sections_8'][far0['id']]['pulse_grid_s']):
        if pulse > 2.2e-6:
            continue
        reference = references[('l_sections_8', 1., 0)]
        current = float(reference.observe([point['y']], pulse)['sense_current'][0])
        inferred.append(dict(pulse_s=pulse, published_delay_s=point['y'],
                             current_at_published_delay_a=current,
                             excess_over_loaded_dc_a=current-reference.targets()['sense_current']))
    source_paths = [Path(__file__), Path(k.__file__), CONTRACT, k.REFERENCE,
                    k.DATA/'curves.json', k.DATA/'extraction.json',
                    k.ROOT/'src/model/NandRcLadder.cpp', k.ROOT/'include/model/NandRcLadder.h',
                    k.ROOT/'tests/NandRcLadderProbe.cpp']
    report = dict(status='exploratory_incomplete_paper_reproduction',
                  contract=yaml.safe_load(CONTRACT.read_text()), comparisons=comparisons,
                  current_rule_alternatives=alternatives, inferred_data0_thresholds=inferred,
                  numerical=numerical, compiled_cases=rows,
                  exclusions=[c['id'] for c in curves if c not in selected],
                  provenance=dict(source_hashes={str(p.relative_to(k.ROOT)): k.sha256(p) for p in source_paths},
                                  executable_sha256=k.sha256(k.PROBE)))
    (output/'report.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    plot_audit(output, selected, comparisons, alternatives)
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path,
                        default=k.ROOT/'output/validation/nand-kondo-2022/assumptions')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--require-reproduction', action='store_true')
    args = parser.parse_args(argv)
    if not 1 <= args.jobs <= 16:
        parser.error('--jobs must be between 1 and 16')
    _, curves, _ = k.load_reference()
    report = run_audit(args.output, curves, args.jobs)
    print(f'Exploratory report: {args.output / "report.json"}', flush=True)
    return 2 if args.require_reproduction else (0 if report['numerical']['passes'] else 1)


if __name__ == '__main__':
    raise SystemExit(main())
