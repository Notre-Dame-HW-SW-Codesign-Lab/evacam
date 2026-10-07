#!/usr/bin/env python3
"""Compare pinned public EvaCAM and current code using shared diagnostic inputs.

This is a deliberately restricted audit exporter, not a legacy configuration
loader. New circuit options are run separately and never silently exported.
"""
from __future__ import annotations
import argparse
import copy
from datetime import datetime, timezone
import hashlib
import io
import json
import math
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import yaml

ROOT = Path(__file__).resolve().parents[1]
COMMIT = '3289a855c7ad4b74edd60920a9c5c6dc2edcad9b'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def numeric(value):
    """Strip the explicit units emitted by these restricted fixture templates."""
    match = re.fullmatch(r'([-+0-9.eE]+)(?:nm|K|B|bits|F\^2|F|V|uA|uW|mV|ns|pJ|ohm|fF)?', str(value))
    if not match or not math.isfinite(float(match[1])):
        raise ValueError('Unsupported audit quantity: ' + str(value))
    return float(match[1])


def load_deck(path):
    top = yaml.safe_load(path.read_text())
    arch_path = path.parent / top['architecture']
    cell_path = path.parent / top['cell']
    arch = yaml.safe_load(arch_path.read_text())
    cell = yaml.safe_load(cell_path.read_text())
    memory = yaml.safe_load((cell_path.parent / cell['memory_device']).read_text())
    sensing_path = arch_path.parent / arch['sensing']
    sensing = yaml.safe_load(sensing_path.read_text())
    top['technology'] = str((path.parent / top['technology']).resolve())
    sensing['sense_amplifier'] = str((sensing_path.parent / sensing['sense_amplifier']).resolve())
    return dict(top=top, architecture=arch, cell=cell, memory=memory, sensing=sensing)


def write_deck(deck, folder):
    folder.mkdir(parents=True, exist_ok=True)
    deck = copy.deepcopy(deck)
    deck['top'].update(architecture='architecture.yaml', cell='cell.yaml')
    deck['architecture']['sensing'] = 'sensing.yaml'
    deck['cell']['memory_device'] = 'memory.yaml'
    for name, data in deck.items():
        (folder / (name + '.yaml')).write_text(yaml.safe_dump(data, sort_keys=False))
    return folder / 'top.yaml'


def export_legacy(deck, folder):
    """Export the fixed single-subarray, generic-voltage-SA comparison subset."""
    a, c, m, s = [deck[k] for k in ('architecture', 'cell', 'memory', 'sensing')]
    if set(s) - {'schema', 'internal', 'sense_amplifier'} or 'search_timing' in a:
        raise ValueError('Circuit/timing options cannot be exported to the old engine')
    if Path(s['sense_amplifier']).name != 'nvsim_vol.sense_amp.yaml':
        raise ValueError('Only generic voltage sensing is shared')
    if a['design']['search_function'] != 'EX' or c['cam_type'] != 'TCAM':
        raise ValueError('Only exact TCAM is shared')
    org = a['organization']
    if any(org[k][v] != [1, 1] for k in ('banks', 'mats') for v in ('total', 'active')):
        raise ValueError('Only a single local subarray is exported')
    p = a['peripherals']
    if p['input']['encoder'] or p['input']['custom_encoder'] or any(p['output'].values()):
        raise ValueError('Unsupported audit peripheral')
    cfg = {}
    def put(key, value):
        cfg[key] = value
    put('DesignTarget', 'CAM')
    put('CAMType', 'TCAM')
    put('SearchFunction', 'EX')
    for dst, src in [('ProcessNode', 'system_process_node'), ('Temperature (K)', 'temperature')]:
        put(dst, int(numeric(a['design'][src])))
    put('DeviceRoadmap', a['design']['device_roadmap'])
    put('MemoryCellInputFile', './legacy.cell')
    put('Capacity (B)', int(numeric(a['memory']['capacity'])))
    put('WordWidth (bit)', int(numeric(a['memory']['word_width'])))
    put('Routing', a['routing']['type'])
    for key, value in [('WithInputBuffer', p['input']['buffer']), ('WithInputEnc', False),
                       ('CustomInputEnc', False), ('WithWriteDriver', p['write_driver']),
                       ('WithOutputAcc', False), ('WithPriorityEnc', False), ('WithOutputBuffer', False)]:
        put(key, 'Yes' if value else 'No')
    put('InternalSensing', 'true')
    put('TypeSenseAmp', 'nvsim_vol')
    put('CustomSenseAmp', 'No')
    put('OptimizationTarget', deck['top']['optimization']['target'])
    for loc in ('local', 'global'):
        w = a['wires'][loc]
        for key, val in [('WireType', w['type']), ('WireRepeaterType', w['repeater']),
                         ('WireUseLowSwing', 'Yes' if w['low_swing'] else 'No')]:
            put(loc.title() + key, val)
    for dst, src in [('BufferDesignOptimization', 'buffer_design'), ('RowDriverOptimization', 'row_driver'),
                     ('PriorityEncOptimization', 'priority_encoder')]:
        put(dst, deck['top']['optimization'][src])
    put('ForceBank (Total AxB, Active CxD)', '1x1, 1x1')
    put('ForceMat (Total AxB, Active CxD)', '1x1, 1x1')
    for dst, src in [('ForceMuxSenseAmp', 'sense_amp'), ('ForceMuxOutputLev1', 'output_level1'),
                     ('ForceMuxOutputLev2', 'output_level2')]:
        put(dst, org['mux'][src])
    put('BitSerialWidth', int(numeric(a['memory']['word_width'])))
    put('AdditionalCapOnML (fF)', numeric(a.get('matchline', {}).get('additional_cap', '0fF')))
    cell = {'MemCellType': m['type'], 'CAMType': 'TCAM', 'ProcessNode': int(numeric(c['layout']['cell_process_node'])),
            'CellArea (F^2)': numeric(c['layout']['area']), 'CellAspectRatio': c['layout']['aspect_ratio'],
            'AccessType': c['access_device']['type'], 'AccessCMOSWidth (F)': numeric(c['access_device']['cmos_width']),
            'VoltageDropAccessDevice (V)': numeric(c['access_device'].get('voltage_drop', '0V')),
            'ResistanceOn (ohm)': numeric(m['resistance']['on']), 'ResistanceOff (ohm)': numeric(m['resistance']['off']),
            'ReadMode': m['read']['mode'], 'MLC': 'false',
            'MatchCMOSWidth (F)': numeric(a['matchline']['match_transistor']['cmos_width'])}
    for dst, src, default in [('ReadVoltage (V)', 'voltage', '0V'), ('ReadCurrent (uA)', 'current', '0uA'),
                              ('ReadPower (uW)', 'power', '0uW'), ('MinSenseVoltage (mV)', 'min_sense_voltage', '80mV')]:
        cell[dst] = numeric(m['read'].get(src, default))
    for op in ('set', 'reset'):
        v = m['write'][op]
        cell[op.title() + 'Mode'] = v['mode']
        for dst, src, default in [('Voltage (V)', 'voltage', '0V'), ('Current (uA)', 'current', '0uA'),
                                  ('Pulse (ns)', 'pulse', '0ns'), ('Energy (pJ)', 'energy', '0pJ')]:
            cell[op.title() + dst] = numeric(v.get(src, default))
    for axis, legacy in [('row', 'Row'), ('column', 'Col')]:
        ports = c['ports'][axis]
        cell['Num' + legacy + 'Port'] = len(ports)
        for i, port in ports.items():
            for dst, src in [('PortType', 'type'), ('CmosRegion', 'cmos_region'), ('numCmos', 'num_cmos'),
                             ('widthCmos (F)', 'cmos_width'), ('isNMOS', 'is_nmos'), ('widthWire', 'wire_width')]:
                val = port[src]
                if src in ('cmos_width', 'wire_width'): val = numeric(val)
                if isinstance(val, bool): val = str(val).lower()
                if src == 'type': val = val.capitalize()
                cell[f'{legacy}Port:{dst}: {i}'] = val
            for dst, src in [('volSetLRS', 'set_lrs'), ('volSetMRS', 'set_mrs'), ('volReset', 'reset'),
                             ('volSearch0', 'search0'), ('volSearch1', 'search1')]:
                cell[f'{legacy}Port:{dst} (V): {i}'] = numeric(port['voltages'][src])
    # Port records use an index separator without whitespace after the last colon.
    for name, data in [('legacy.cfg', cfg), ('legacy.cell', cell)]:
        lines = [f'-{key}:{"" if "Port:" in key else " "}{value}' for key, value in data.items()]
        (folder / name).write_text('\n'.join(lines) + '\n')
    return folder / 'legacy.cfg'


def parse_probe(output):
    values = {}
    for key, raw in re.findall(r'^BASELINE ([\w.]+)=([^\s]+)$', output, re.M):
        if key in values:
            raise ValueError('Duplicate probe field: ' + key)
        value = float(raw)
        if not math.isfinite(value):
            raise ValueError('Nonfinite probe field: ' + key)
        values[key] = value
    if not values and re.search(r'No (?:valid )?solutions', output, re.I):
        return {'feasible': 0}
    if 'feasible' not in values or values['feasible'] not in (0, 1):
        raise ValueError('Missing valid probe result')
    if values['feasible']:
        for key in ('search_s', 'read_s', 'energy_j', 'area_m2', 'ml_s', 'sa_s', 'input.entries', 'input.bits_per_ml'):
            if key not in values or values[key] < 0:
                raise ValueError('Invalid required probe field: ' + key)
    return values


def check_inputs(old, current):
    keys = {k for k in old if k.startswith('input.')}
    if keys != {k for k in current if k.startswith('input.')}:
        raise ValueError('Resolved input inventory differs')
    drift = {k: [old[k], current[k]] for k in sorted(keys)
             if not math.isclose(old[k], current[k], rel_tol=1e-10, abs_tol=1e-25)}
    if drift:
        raise ValueError('Resolved inputs differ: ' + json.dumps(drift))
    return len(keys)


def run_probe(binary, config, folder, label):
    process = subprocess.run([str(binary), str(config)], cwd=config.parent, capture_output=True, text=True, timeout=60)
    (folder / (label + '.log')).write_text(process.stdout + process.stderr)
    if process.returncode:
        raise ValueError(f'{label} exited {process.returncode}; see {folder}')
    return parse_probe(process.stdout)


def instrument_old(source):
    probe = (ROOT / 'tests/LegacyComparisonProbe.cpp').read_text()
    body = probe[probe.index('    std::cout << std::scientific'):probe.index('#undef FIELD') + len('#undef FIELD')]
    for a, b in [('config->input.temperature', 'inputParameter->temperature'),
                 ('config->peripherals.addCapOnML', 'inputParameter->AddCapOnML'),
                 ('sub.CAM_opt.BitSerialWidth', 'bank.numBitSerial'),
                 ('port.ConnectedRegion', 'port.ConnectedRegoin'),
                 ('sub.matchlineDelay', 'sub.bitlineDelay')]:
        body = body.replace(a, b)
    body = re.sub(r'sub\.(\w+(?:\[[^\]]+\])?)->', r'sub.\1.', body)
    body = re.sub(r'tech\.(\w+)\(\)', r'tech.\1', body)
    for var in ('bank', 'sub', 'cell', 'tech'):
        body = re.sub(r'\b' + var + r'\.', 'audit_' + var + '.', body)
    prefix = ('\n    std::cout << "\\n";\n    const auto &audit_bank = *bank;\n    const auto &audit_sub = bank->mat.subarray;\n'
              '    const auto &audit_cell = *CAM_cell;\n    const auto &audit_tech = *tech;\n')
    target = source / 'CAM_Result.cpp'
    text = target.read_text()
    marker = 'void CAM_Result::print() {'
    if text.count(marker) != 1: raise ValueError('Unexpected legacy reporter')
    target.write_text('#include <iomanip>\n' + text.replace(marker, marker + prefix + body + '\n'))


def prepare_old(repository, folder):
    archive = subprocess.check_output(['git', '-C', str(repository), 'archive', COMMIT])
    source = folder / 'old-source'
    source.mkdir()
    with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
        for member in tar:
            path = Path(member.name)
            if not member.isfile() or len(path.parts) != 1:
                raise ValueError('Unexpected public archive layout')
            (source / path).write_bytes(tar.extractfile(member).read())
    original_hashes = {p.name: sha(p) for p in source.iterdir()}
    instrument_old(source)
    command = ['make', '-j4', 'CC=g++ -std=c++11 -O0 -g']
    with (folder / 'old-build.log').open('w') as stream:
        subprocess.run(command, cwd=source, stdout=stream, stderr=subprocess.STDOUT, check=True, timeout=180)
    return source, dict(commit=COMMIT, archive_sha256=hashlib.sha256(archive).hexdigest(),
                        original_sha256=original_hashes, instrumented_reporter_sha256=sha(source / 'CAM_Result.cpp'),
                        binary_sha256=sha(source / 'Eva-CAM'), build_command=command)


def cases():
    root = load_deck(ROOT / 'config/2FeFET_TCAM/2FeFET_TCAM.config.yaml')
    root['architecture']['design']['search_function'] = 'EX'
    root['architecture']['peripherals']['write_driver'] = False
    root['architecture']['matchline']['additional_cap'] = '0fF'
    root['cell']['access_device']['voltage_drop'] = '0.15V'
    out = [('FeFET-public-example', root)]
    for name in ('FeFET-added-6fF', 'FeFET-write-driver'):
        deck = copy.deepcopy(root)
        if name.endswith('6fF'):
            deck['architecture']['matchline']['additional_cap'] = '6fF'
        else:
            deck['architecture']['peripherals']['write_driver'] = True
        out.append((name, deck))
    for name, template in [('FeFET-paper-inputs', 'FeFET-2Fe-TCAS19'), ('MRAM-paper-inputs', 'MRAM-4T2R-VLSIC12')]:
        deck = load_deck(ROOT / f'config/original_validation/{template}/{template}.config.yaml')
        deck['architecture'].pop('search_timing')
        deck['sensing'] = copy.deepcopy(root['sensing'])
        out.append((name, deck))
        if name.startswith('MRAM'):
            legacy_margin = copy.deepcopy(deck)
            legacy_margin['memory']['read']['min_sense_voltage'] = '500mV'
            out.append(('MRAM-inherited-500mV', legacy_margin))
    pcm = load_deck(ROOT / 'config/paper_reference/PCM-2T2R-JSSC14/PCM-2T2R-JSSC14.config.yaml')
    pcm['architecture']['memory']['capacity'] = '4096B'
    for kind in ('banks', 'mats'):
        pcm['architecture']['organization'][kind] = {'total': [1, 1], 'active': [1, 1]}
    out.append(('PCM-local-512x64', pcm))
    return out


def circuit_stages(probe, folder):
    """Keep physical inputs fixed while enabling the current analytical options."""
    runs = []
    for name in ('FeFET-2Fe-TCAS19', 'MRAM-4T2R-VLSIC12'):
        original = load_deck(ROOT / f'config/original_validation/{name}/{name}.config.yaml')
        deck = copy.deepcopy(original)
        deck['sensing'] = {'schema': 'sensing', 'internal': True,
                           'sense_amplifier': str(ROOT / 'config/lib/sense_amp/nvsim_vol.sense_amp.yaml')}
        stages = [('explicit-timing', copy.deepcopy(deck))]
        if name.startswith('FeFET'):
            deck['sensing'].update(circuit=original['sensing']['circuit'],
                                   decision={'model': 'voltage_threshold', 'threshold': '0.5V'})
            stages.append(('direct-path-generic-SA-0.5V', copy.deepcopy(deck)))
        else:
            deck['sensing'].update(circuit={**original['sensing']['circuit'], 'model': 'clamped_keeper'},
                                   decision={'model': 'voltage_threshold', 'threshold': '0.515V'})
            stages.append(('linear-clamps-midpoint', copy.deepcopy(deck)))
        stages.append(('current-circuit-fixture', original))
        for stage, d in stages:
            subdir = folder / (name + '-' + stage)
            config = write_deck(d, subdir)
            values = run_probe(probe, config, subdir, 'current')
            if not values['feasible']: raise ValueError('Unexpected circuit stage rejection')
            runs.append(dict(id=name, stage=stage, current=values))
    return runs


def old_diode_ablation(source, folder, decks):
    """A diagnostic copy changes only the erroneous repeated gate-cap factor."""
    target = folder / 'old-linear-diode-cap'
    shutil.copytree(source, target)
    path = target / 'CAM_Line.cpp'
    text = path.read_text()
    pattern = r'(CalculateGateCap\(CellPort.widthCmos \* (?:FEFET_tech|tech)->featureSize, \*(?:FEFET_tech|tech)\)) \* numCell\n'
    changed, count = re.subn(pattern, r'\1\n', text)
    if count != 2: raise ValueError('Unexpected old diode capacitance formula')
    path.write_text(changed)
    with (folder / 'old-diode-ablation-build.log').open('w') as stream:
        subprocess.run(['make', '-j4', 'CC=g++ -std=c++11 -O0 -g'], cwd=target,
                       stdout=stream, stderr=subprocess.STDOUT, check=True, timeout=60)
    return dict(source_sha256=sha(path), binary_sha256=sha(target / 'Eva-CAM'),
                changes='Remove inner numCell from diode gate capacitance; retain outer terminal count.',
                runs=[dict(id=name, old=run_probe(target / 'Eva-CAM', folder / name / 'legacy.cfg',
                                                folder / name, 'old-linear-diode-cap'))
                      for name, _ in decks if name.startswith('MRAM')])


def render(report):
    lines = ['# Matched input comparison of public and current EvaCAM', '',
             'Old source: `' + COMMIT + '`. All rows use a single subarray and generic voltage sensing.',
             'Inputs are shared; circuit equations and search scheduling differ. These are software comparisons, not paper accuracy scores.', '',
             '| Case | Old printed latency (ns) | Old actual search (ns) | Current search (ns) | Current vs old search | Old energy (pJ) | Current energy (pJ) |',
             '| --- | ---: | ---: | ---: | ---: | ---: | ---: |']
    for row in report['runs']:
        old, new = row['old'], row['current']
        if old['feasible'] and new['feasible']:
            lines.append(f"| {row['id']} | {old['read_s']*1e9:.6f} | {old['search_s']*1e9:.6f} | {new['search_s']*1e9:.6f} | {100*(new['search_s']/old['search_s']-1):+.1f}% | {old['energy_j']*1e12:.6g} | {new['energy_j']*1e12:.6g} |")
        else:
            lines.append(f"| {row['id']} | — | {'infeasible' if not old['feasible'] else old['search_s']*1e9} | {'infeasible' if not new['feasible'] else new['search_s']*1e9} | — | — | — |")
    lines += ['', 'The old console labels bank.readLatency as Search Latency. Both fields above come directly from evaluated bank objects.',
              'FeFET-public-example reproduces the committed root deck; paper-input rows are reconstructions, not recovered publication decks.',
              'PCM is one local subarray, excludes the complete CSRSS/reference path, and is not a 1Mb paper reproduction.',
              'SAPIENS has no matched serial architecture in this audit; its clock period is not whole-query latency.', '']
    lines += ['## Current circuit stages', '', '| Case | Stage | Search (ns) | Energy (pJ) |', '| --- | --- | ---: | ---: |']
    for row in report.get('circuit_stages', []):
        v = row['current']
        lines.append(f"| {row['id']} | {row['stage']} | {v['search_s']*1e9:.6f} | {v['energy_j']*1e12:.6g} |")
    lines += ['', '## Old diode capacitance diagnostic', '',
              'Only the repeated gate-capacitance word-length factor is removed in this separate old-code copy.', '',
              '| Case | Old with linear gate capacitance (ns) |', '| --- | ---: |']
    for row in report.get('old_diode_ablation', {}).get('runs', []):
        lines.append(f"| {row['id']} | {row['old']['search_s']*1e9:.6f} |")
    lines += ['', '## Paper observations and executable results', '',
              'Both executable columns share the reconstructed device and geometry inputs. Current uses the new circuit options,',
              'including model-specific assumptions the old engine cannot express. Gaps are diagnostics, not validated accuracy.', '',
              '| Case | Reference (ns) | Published EvaCAM (ns) | Old executable (ns) | Current circuit (ns) | Old gap | Current gap |',
              '| --- | ---: | ---: | ---: | ---: | ---: | ---: |']
    for row in report.get('publication_comparison', []):
        lines.append(f"| {row['id']} | {row['reference_ns']:.6g} | {row['published_ns']:.6g} | {row['old_ns']:.6f} | {row['current_ns']:.6f} | {row['old_gap_percent']:+.1f}% | {row['current_gap_percent']:+.1f}% |")
    lines.append('')
    return '\n'.join(lines)


def collect_report(repository, probe, output):
    output.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix='run-', dir=output))
    source, provenance = prepare_old(repository, folder)
    original = run_probe(source / 'Eva-CAM', source / '2FeFET_TCAM.cfg', folder, 'public-root')
    runs = []
    decks = cases()
    for name, deck in decks:
        subdir = folder / name
        config = write_deck(deck, subdir)
        legacy = export_legacy(deck, subdir)
        old = run_probe(source / 'Eva-CAM', legacy, subdir, 'old')
        new = run_probe(probe, config, subdir, 'current')
        matched = check_inputs(old, new) if old['feasible'] and new['feasible'] else None
        if name == 'FeFET-public-example':
            # Explicit defaults and unit formatting must preserve the released deck.
            for key in ('search_s', 'read_s', 'energy_j', 'area_m2'):
                if not math.isclose(original[key], old[key], rel_tol=1e-10):
                    raise ValueError('Translated public example differs: ' + key)
        runs.append(dict(id=name, old=old, current=new, matched_input_fields=matched,
                         files={p.name: sha(p) for p in subdir.iterdir() if p.suffix in ('.yaml', '.cfg', '.cell')}))
    inputs = [*ROOT.glob('config/lib/**/*.yaml'), *ROOT.glob('config/original_validation/**/*.yaml'),
              *ROOT.glob('config/2FeFET_TCAM/*.yaml'), *ROOT.glob('config/paper_reference/PCM-2T2R-JSSC14/*.yaml')]
    report = dict(status='complete', evidence_directory=str(folder),
                  created_utc=datetime.now(timezone.utc).isoformat(), old_provenance=provenance,
                  compiler=subprocess.check_output(['g++', '--version'], text=True).splitlines()[0],
                  current_probe_sha256=sha(probe), runner_sha256=sha(__file__), public_root=original, runs=runs,
                  probe_source_sha256=sha(ROOT / 'tests/LegacyComparisonProbe.cpp'),
                  current_source_sha256={str(p.relative_to(ROOT)): sha(p) for parent in ('src', 'include')
                                         for p in sorted((ROOT / parent).rglob('*')) if p.suffix in ('.cpp', '.h')},
                  input_sha256={str(p.relative_to(ROOT)): sha(p) for p in sorted(inputs)})
    report['circuit_stages'] = circuit_stages(probe, folder)
    report['old_diode_ablation'] = old_diode_ablation(source, folder, decks)
    references = yaml.safe_load((ROOT / 'docs/validation/original-evacam.reference.yaml').read_text())
    report['reference_manifest_sha256'] = sha(ROOT / 'docs/validation/original-evacam.reference.yaml')
    report['publication_comparison'] = []
    for stage in report['circuit_stages']:
        name = 'FeFET-paper-inputs' if stage['id'].startswith('FeFET') else 'MRAM-paper-inputs'
        old = next(row['old'] for row in runs if row['id'] == name)
        stage['matched_input_fields'] = check_inputs(old, stage['current'])
        if stage['stage'] != 'current-circuit-fixture': continue
        ref = next(row for row in references['cases'] if row['id'] == stage['id'])
        target = ref['observations']['latency']['value']
        before = old['search_s'] * 1e9; after = stage['current']['search_s'] * 1e9
        report['publication_comparison'].append(dict(id=stage['id'], reference_ns=target,
                published_ns=ref['published_evacam']['latency'], old_ns=before, current_ns=after,
                old_gap_percent=100*(before/target-1), current_gap_percent=100*(after/target-1),
                validated_accuracy=False))
    if shutil.which('valgrind'):
        command = ['valgrind', '--track-origins=yes', '--log-file=' + str(folder / 'old-valgrind.log'),
                   str(source / 'Eva-CAM'), '2FeFET_TCAM.cfg']
        process = subprocess.run(command, cwd=source, capture_output=True, text=True, timeout=60)
        (folder / 'old-valgrind-stdout.log').write_text(process.stdout + process.stderr)
        report['old_valgrind'] = dict(command=command, exit_code=process.returncode,
                                     summary=(folder / 'old-valgrind.log').read_text())
    for dest in (folder, output):
        (dest / 'runs.json').write_text(json.dumps(report, indent=2, allow_nan=False) + '\n')
        (dest / 'comparison.md').write_text(render(report))
    print(render(report))
    print('Evidence: ' + str(folder))
    return 0


def run_suite(repository, probe, output):
    """An unsuccessful attempt must not leave a stale passing summary in place."""
    output.mkdir(parents=True, exist_ok=True)
    try:
        return collect_report(repository, probe, output)
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError, yaml.YAMLError, tarfile.TarError) as error:
        failure = dict(status='failed', created_utc=datetime.now(timezone.utc).isoformat(),
                       error=str(error), runs=[], note='See the fresh run directory for logs; prior runs remain preserved.')
        (output / 'runs.json').write_text(json.dumps(failure, indent=2) + '\n')
        (output / 'comparison.md').write_text('# Matched legacy comparison failed\n\n' + str(error) + '\n')
        print('Legacy comparison failed: ' + str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--legacy-repo', type=Path, required=True)
    parser.add_argument('--probe', type=Path, default=ROOT / 'test-bin/LegacyComparisonProbe')
    parser.add_argument('--output', type=Path, default=ROOT / 'output/validation/matched-legacy')
    args = parser.parse_args()
    raise SystemExit(run_suite(args.legacy_repo.resolve(), args.probe.resolve(), args.output.resolve()))
