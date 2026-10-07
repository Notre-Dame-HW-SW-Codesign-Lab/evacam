#!/usr/bin/env python3
"""Pinned NVSim execution, matched linear circuits, and exact energy reconciliation.

No production model is retuned. Numerical reference agreement is reported
separately from the untested effect of empirical correction on real hardware.
"""
from __future__ import annotations

import argparse
import hashlib
import itertools
import json
import os
from pathlib import Path
import subprocess
import time

import numpy as np
from scipy.linalg import expm
from scipy.optimize import brentq
import yaml

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'docs/validation/nand-nvsim.reference.yaml'
FROZEN = ROOT / 'docs/validation/data/nvsim/baseline.json'
RC_PROBE = ROOT / 'test-bin/NandRcLadderProbe'
NV_PROBE_SOURCE = ROOT / 'tests/NvsimFlashProbe.cpp'


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def export_and_build(repository, output, manifest):
    """Read pinned tracked files only; never change the user's NVSim checkout."""
    source = output / 'source'
    source.mkdir(parents=True, exist_ok=True)
    revision = manifest['nvsim_revision']
    listing = subprocess.check_output(['git', '-C', str(repository), 'ls-tree', '-r', '--name-only', revision], text=True)
    names = [name for name in listing.splitlines() if '/' not in name and
             (name.endswith(('.cpp', '.h', '.cell', '.cfg')) or name in ('Makefile', 'README'))]
    for name in names:
        (source / name).write_bytes(subprocess.check_output(['git', '-C', str(repository), 'show', f'{revision}:{name}']))
    if digest(source / 'SubArray.cpp') != manifest['subarray_sha256'] or digest(source / 'sample_SLCNAND.cell') != manifest['cell_sha256']:
        raise ValueError('NVSim baseline source hash mismatch')
    # Compile the enumerated pinned files, never stray files left in an output directory.
    cpp = [str(source / name) for name in sorted(names) if name.endswith('.cpp') and name != 'main.cpp']
    commands = []
    with (output / 'build.log').open('w') as log:
        for entry, binary in [(source / 'main.cpp', output / 'nvsim'), (NV_PROBE_SOURCE, output / 'NvsimFlashProbe')]:
            command = ['g++', '-std=c++11', '-O2', '-I', str(source), *cpp, str(entry), '-o', str(binary)]
            subprocess.run(command, check=True, stdout=log, stderr=log, timeout=180)
            commands.append(command)
    metadata = dict(revision=revision, compiler=subprocess.check_output(['g++', '--version'], text=True).splitlines()[0],
                    commands=commands, source_hashes={name: digest(source/name) for name in names},
                    adapter_sha256=digest(NV_PROBE_SOURCE), binary_sha256=digest(output/'nvsim'))
    (output / 'build.json').write_text(json.dumps(metadata, indent=2) + '\n')
    return metadata


def baseline_config():
    """A specified nominal fixture, not reconstructed inputs for the 2012 chip."""
    return '''-DesignTarget: RAM
-OptimizationTarget: ReadLatency
-ProcessNode: 45
-Capacity (KB): 32768
-WordWidth (bit): 8192
-DeviceRoadmap: HP
-LocalWireType: LocalAggressive
-LocalWireRepeaterType: RepeatedNone
-LocalWireUseLowSwing: No
-GlobalWireType: GlobalAggressive
-GlobalWireRepeaterType: RepeatedNone
-GlobalWireUseLowSwing: No
-Routing: H-tree
-InternalSensing: true
-MemoryCellInputFile: source/sample_SLCNAND.cell
-Temperature (K): 350
-BufferDesignOptimization: latency
-ForceBank (Total AxB, Active CxD): 16x1, 1x1
-ForceMat (Total AxB, Active CxD): 1x1, 1x1
-ForceMuxSenseAmp: 2
-ForceMuxOutputLev1: 1
-ForceMuxOutputLev2: 1
-FlashPageSize (Byte): 1024
-FlashBlockSize (KB): 64
'''


def collect_baseline(output, manifest):
    config = output / 'baseline.cfg'
    config.write_text(baseline_config())
    started = time.perf_counter()
    cli = subprocess.run([str(output/'nvsim'), str(config)], cwd=output, text=True,
                         capture_output=True, check=True, timeout=60)
    (output/'baseline.stdout.txt').write_text(cli.stdout)
    (output/'baseline.stderr.txt').write_text(cli.stderr)
    if 'No valid solutions' in cli.stdout or 'Read Latency' not in cli.stdout:
        raise RuntimeError('NVSim CLI did not produce a valid baseline')
    cases = []
    condition = manifest['conditions']
    for pages, rows, columns in itertools.product(condition['page_counts'], condition['rows'], condition['columns']):
        command = [str(output/'NvsimFlashProbe'), str(output/'source/sample_SLCNAND.cell'), str(pages), str(rows), str(columns)]
        process = subprocess.run(command, capture_output=True, text=True, check=True, timeout=30)
        case = json.loads(process.stdout)
        case['id'] = f'p{pages}-r{rows}-c{columns}'
        cases.append(case)
    result = dict(revision=manifest['nvsim_revision'], adapter_sha256=digest(NV_PROBE_SOURCE),
                  config_sha256=digest(config), wall_time_s=time.perf_counter()-started, cases=cases)
    (output/'baseline.json').write_text(json.dumps(result, indent=2, allow_nan=False)+'\n')
    return result


def validate_baseline(baseline, manifest):
    """Require the frozen source/geometry contract, including native energy fields."""
    if digest(FROZEN) != manifest['baseline_sha256']:
        raise ValueError('frozen NVSim baseline hash mismatch')
    frozen = json.loads(FROZEN.read_text())
    condition = manifest['conditions']
    expected_ids = [f'p{p}-r{r}-c{c}' for p, r, c in itertools.product(
        condition['page_counts'], condition['rows'], condition['columns'])]
    for record in (frozen, baseline):
        if (record['revision'] != manifest['nvsim_revision'] or
                record['adapter_sha256'] != digest(NV_PROBE_SOURCE) or
                record['config_sha256'] != hashlib.sha256(baseline_config().encode()).hexdigest()):
            raise ValueError('baseline revision/adapter/config mismatch')
        if [case['id'] for case in record['cases']] != expected_ids:
            raise ValueError('baseline fixture set mismatch')
    for expected, actual in zip(frozen['cases'], baseline['cases']):
        if expected.keys() != actual.keys() or expected['components'].keys() != actual['components'].keys():
            raise ValueError('baseline field set mismatch')
        pairs = [(value, actual[key]) for key, value in expected.items() if key not in ('id', 'components')]
        for name, unit in expected['components'].items():
            if unit.keys() != actual['components'][name].keys():
                raise ValueError('baseline component field set mismatch')
            pairs.extend((value, actual['components'][name][key]) for key, value in unit.items())
        if not all(np.isfinite(a) and np.isclose(a, e, rtol=1e-10, atol=0) for e, a in pairs):
            raise ValueError('native NVSim result drifted from frozen baseline')


def circuit(case, energy=False, lumped=False):
    """A pi circuit with exactly NVSim's far-node first-moment formula."""
    capacitance = [case['cell_capacitance_f'] + case['bitline_capacitance_f']/2,
                   case['bitline_capacitance_f']/2 + case['mux_energy_capacitance_f' if energy else 'mux_delay_capacitance_f']]
    if lumped:
        return np.array([sum(capacitance)]), np.array([case['string_resistance_ohm']])
    return np.array(capacitance), np.array([case['string_resistance_ohm'], case['bitline_resistance_ohm']])


def reference(capacitance, resistance, duration, initial, precharge=None):
    """Dense incidence stamping and matrix exponential, independent of C++ steps."""
    count = len(capacitance)
    incidence = np.zeros((count-1, count))
    for edge in range(count-1):
        incidence[edge, edge] = 1
        incidence[edge, edge+1] = -1
    conductance = incidence.T @ np.diag(1/resistance[1:]) @ incidence
    equilibrium = np.zeros(count)
    if precharge is None:
        conductance[0, 0] += 1/resistance[0]
    else:
        driver, voltage = precharge
        conductance[-1, -1] += 1/driver
        equilibrium[:] = voltage
    return equilibrium + expm(-conductance / capacitance[:, None] * duration) @ (initial-equilibrium)


def threshold_time(capacitance, resistance, initial, target):
    if initial[-1] <= target:
        return 0.
    moment = float(np.dot(np.cumsum(resistance), capacitance))
    normalized = brentq(lambda factor: reference(capacitance, resistance, factor*moment, initial)[-1]-target,
                        0., 100., xtol=1e-12)
    return normalized * moment


def compiled(capacitance, resistance, duration, initial, output, name, criteria, precharge=None):
    right = {} if precharge is None else dict(connected=True, resistance_ohm=float(precharge[0]), voltage_v=float(precharge[1]))
    left = dict(connected=True, resistance_ohm=float(resistance[0]), voltage_v=0.) if precharge is None else {}
    request = dict(capacitances_f=capacitance.tolist(), series_resistances_ohm=resistance[1:].tolist(),
                   initial_voltages_v=np.asarray(initial).tolist(), duration_s=float(duration), left=left, right=right,
                   solver=dict(max_step_s=float(duration/10), tolerance_v=criteria['solver_tolerance_v'],
                               max_steps=criteria['solver_max_steps']))
    path = output/'inputs'/f'{name}.yaml'
    path.parent.mkdir(exist_ok=True)
    path.write_text(yaml.safe_dump(request))
    started = time.perf_counter()
    process = subprocess.run([str(RC_PROBE), str(path)], check=True, capture_output=True, text=True, timeout=30)
    result = yaml.safe_load(process.stdout)
    result['process_wall_time_s'] = time.perf_counter()-started
    (output/'inputs'/f'{name}.result.yaml').write_text(yaml.safe_dump(result))
    return result


def energy_ledger(case):
    """Reconstruct the executed pinned code, including its reset accumulation."""
    c = case['cell_capacitance_f'] + case['bitline_capacitance_f'] + case['mux_energy_capacitance_f']
    units = case['components']
    peripheral_read = sum(unit['read_j'] for unit in units.values())
    peripheral_write = sum(unit['write_j'] for name, unit in units.items() if name not in ('row_decoder', 'precharger'))
    read_bl = c * case['precharge_v']**2 * case['columns']
    program_bl = c * case['program_voltage_v']**2 * case['columns']
    tunnel = case['threshold_change_v'] * case['tunnel_current_density_a_per_m2'] * case['cell_area_m2'] * case['program_time_s'] * case['columns']
    erase_bl = c * (case['erase_voltage_v']-case['builtin_voltage_v'])**2 * (case['columns']+1)
    erase_well = case['junction_capacitance_f_per_m2'] * case['cell_area_m2'] * case['block_bits'] * case['erase_voltage_v']**2
    program = program_bl+tunnel+units['row_decoder']['set_j']+peripheral_write
    erase = erase_bl+erase_well+program+units['row_decoder']['reset_j']+peripheral_write
    write = (program_bl+tunnel+(erase_bl+erase_well)/case['pages'])/2 + units['row_decoder']['write_j']+peripheral_write
    predicted = dict(read=read_bl+peripheral_read, program=program, erase=erase, write=write)
    residual = {name: (value-case[f'{name}_energy_j'])/case[f'{name}_energy_j'] for name, value in predicted.items()}
    return dict(read_bitline_charging_j=read_bl, read_peripherals_j=peripheral_read,
                program_bitline_charging_j=program_bl, program_tunneling_j=tunnel,
                program_row_driver_j=units['row_decoder']['set_j'], common_write_peripherals_j=peripheral_write,
                erase_bitline_charging_j=erase_bl, erase_well_charging_j=erase_well,
                erase_embedded_program_j=program, erase_row_driver_j=units['row_decoder']['reset_j'],
                raw_write_average_j=write, reconstructed_j=predicted, relative_residual=residual,
                scope='one initialized subarray; exact executed totals, no correction of inherited accounting')


def evaluate_case(case, output, manifest):
    limits = manifest['criteria']
    capacitance, resistance = circuit(case)
    vpre = case['precharge_v']
    target = vpre-case['sense_drop_v']
    initial = np.full(len(capacitance), vpre)
    moment = float(np.dot(np.cumsum(resistance), capacitance))
    tau_log = moment*np.log(vpre/target)
    beta = 1/(resistance[0]*case['minimum_nmos_gm_s'])
    horowitz = tau_log*np.sqrt(np.log(.5)**2 + beta/(case['row_decoder_ramp']*tau_log))
    replay = max(horowitz, 20*tau_log)
    if not np.isclose(replay, case['bitline_delay_s'], rtol=1e-12, atol=0):
        raise AssertionError('extracted circuit does not replay NVSim timing')
    exact = threshold_time(capacitance, resistance, initial, target)
    # Bracketing the known reference crossing verifies C++ event timing
    # with a fixed +/- 1e-4 window, without a costly process per root iteration.
    samples = []
    for factor in [1-limits['threshold_time_relative'], 1., 1+limits['threshold_time_relative']]:
        actual = compiled(capacitance, resistance, exact*factor, initial, output,
                          f'{case["id"]}-discharge-{factor}', limits)
        expected = reference(capacitance, resistance, exact*factor, initial)
        error = float(np.max(np.abs(np.array(actual['voltages_v'])-expected)))
        if error > limits['voltage_absolute_v']:
            raise AssertionError('EvaCAM voltage reference error exceeded')
        samples.append(dict(factor=factor, error_v=error, **actual))
    if not samples[0]['voltages_v'][-1] > target > samples[-1]['voltages_v'][-1]:
        raise AssertionError('EvaCAM crossing lies outside the declared timing bracket')
    # Exact limiting agreement: eliminate the wire resistance, combining its nodes.
    lump_cap, lump_res = circuit(case, lumped=True)
    lump_time = float(lump_cap[0]*lump_res[0]*np.log(vpre/target))
    lump = compiled(lump_cap, lump_res, lump_time, [vpre], output, f'{case["id"]}-lumped', limits)
    if abs(lump['voltages_v'][0]-target) > limits['voltage_absolute_v']:
        raise AssertionError('lumped analytical limit failed')
    # Separate energy/precharge experiment: use the power load, source open.
    power_cap, power_res = circuit(case, energy=True)
    driver = case['precharge_driver_resistance_ohm']
    charge_scale = driver*sum(power_cap)+power_res[1]*power_cap[0]
    fully_charged_decision = reference(power_cap, power_res, exact, np.full(2, vpre))[-1]
    precharges = []
    for factor in manifest['conditions']['precharge_time_constants']:
        duration = factor*charge_scale
        expected = reference(power_cap, power_res, duration, np.zeros(2), (driver, vpre))
        actual = compiled(power_cap, power_res, duration, [0., 0.], output,
                          f'{case["id"]}-charge-{factor}', limits, (driver, vpre))
        source_energy = vpre*actual['right_source_charge_c']
        expected_energy = float(vpre*np.dot(power_cap, expected))
        if np.max(np.abs(actual['voltages_v']-expected)) > limits['voltage_absolute_v'] or abs(source_energy/expected_energy-1) > limits['supply_energy_relative']:
            raise AssertionError('finite precharge/energy comparison failed')
        carried = compiled(power_cap, power_res, exact, actual['voltages_v'], output,
                           f'{case["id"]}-carried-{factor}', limits)
        carried_expected = reference(power_cap, power_res, exact, expected)
        if np.max(np.abs(carried['voltages_v']-carried_expected)) > limits['voltage_absolute_v']:
            raise AssertionError('carried-state comparison failed')
        precharges.append(dict(time_scale=factor, duration_s=duration, voltages_v=actual['voltages_v'],
                               energy_j_per_bitline=source_energy, independent_energy_j_per_bitline=expected_energy,
                               fraction_of_full_cv2=source_energy/(sum(power_cap)*vpre**2),
                               carried_decision_v=carried['voltages_v'][-1]))
    if abs(precharges[-1]['fraction_of_full_cv2']-1) > limits['supply_energy_relative']:
        raise AssertionError('long precharge did not recover the NVSim CV² energy limit')
    ledger = energy_ledger(case)
    if max(abs(value) for value in ledger['relative_residual'].values()) > limits['ledger_relative']:
        raise AssertionError('NVSim executed energy ledger did not reconcile')
    return dict(id=case['id'], independent_crossing_s=exact, nvsim_bitline_delay_s=case['bitline_delay_s'],
                uncorrected_moment_crossing_s=float(tau_log), horowitz_s=float(horowitz),
                nvsim_to_ideal_ratio=case['bitline_delay_s']/exact,
                uncorrected_relative_error=float(tau_log/exact-1),
                maximum_cpp_voltage_error_v=max(sample['error_v'] for sample in samples),
                cpp_steps_at_crossing=samples[1]['accepted_steps'],
                cpp_process_wall_time_s=samples[1]['process_wall_time_s'],
                energy_circuit_fully_charged_decision_v=float(fully_charged_decision),
                lumped_voltage_error_v=abs(lump['voltages_v'][0]-target), precharges=precharges, energy_ledger=ledger)


def plot_results(cases, output):
    os.environ.setdefault('MPLCONFIGDIR', str(output/'matplotlib-cache'))
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    x = np.arange(len(cases))
    fig, axes = plt.subplots(1, 2, figsize=(12, 4), layout='constrained')
    axes[0].plot(x, [c['nvsim_to_ideal_ratio'] for c in cases], 'o-', label='NVSim including empirical floor')
    axes[0].plot(x, [1+c['uncorrected_relative_error'] for c in cases], 's-', label='Uncorrected first moment')
    axes[0].axhline(1, color='gray', linestyle=':')
    axes[0].set(xlabel='Fixture index', ylabel='Delay / independent ideal RC crossing',
                title='Same extracted R, C, initial voltage and threshold')
    axes[0].legend(fontsize=8)
    for i, label in enumerate(['0.1 × charging scale', '1 × charging scale', '5 × charging scale', '30 × charging scale']):
        axes[1].plot(x, [c['precharges'][i]['fraction_of_full_cv2'] for c in cases], 'o-', label=label)
    axes[1].set(xlabel='Fixture index', ylabel='Integrated supply energy / full CV²', title='Finite precharge; source open')
    axes[1].legend(fontsize=8)
    fig.suptitle('Numerical circuit comparison only — no chip-accuracy ranking', fontsize=12)
    for extension in ('png', 'svg'):
        fig.savefig(output/f'comparison.{extension}', dpi=170)
    plt.close(fig)


def run(output, repository=None):
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    manifest = yaml.safe_load(MANIFEST.read_text())
    if repository is not None:
        build = export_and_build(Path(repository).resolve(), output, manifest)
        baseline = collect_baseline(output, manifest)
    else:
        build = None
        baseline = json.loads(FROZEN.read_text())
    validate_baseline(baseline, manifest)
    cases = [evaluate_case(case, output, manifest) for case in baseline['cases']]
    report = dict(status='numerical_checks_pass_hardware_improvement_unestablished', cases=cases,
                  reference_sha256=digest(MANIFEST), evaluator_sha256=digest(__file__),
                  evacam_probe_sha256=digest(RC_PROBE), evacam_solver_sha256=digest(ROOT/'src/model/NandRcLadder.cpp'),
                  nvsim_build=build, runtime_scope='RC subprocess wall time includes startup and serialization; no speedup claim',
                  baseline_mode='native_execution' if repository is not None else 'frozen_native_results',
                  baseline_sha256=manifest['baseline_sha256'],
                  criteria=manifest['criteria'], hardware_validation=False,
                  energy_scope='subarray; bank routing and predecoder overhead remain outside this reconciliation')
    (output/'report.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    plot_results(cases, output)
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--nvsim-source', type=Path, help='local git checkout containing the pinned revision; otherwise replay frozen extracted inputs')
    parser.add_argument('--output', type=Path, default=ROOT/'output/validation/nand-nvsim')
    args = parser.parse_args(argv)
    report = run(args.output, args.nvsim_source)
    print(f"{report['status']}: {len(report['cases'])} matched fixtures; {args.output/'report.json'}")


if __name__ == '__main__':
    main()
