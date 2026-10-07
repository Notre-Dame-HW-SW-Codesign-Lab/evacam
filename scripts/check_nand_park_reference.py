#!/usr/bin/env python3
"""Check and render the frozen Park 2025 source extraction, entirely offline.

This command never fits or runs an EvaCAM device model. TCAD observations and
the authors' Spectre results are separate populations. Missing operating
conditions and source ambiguities keep quantitative model validation pending.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import math
import os
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT/'docs/validation/nand-park-2025.reference.yaml'
DATA = ROOT/'docs/validation/data/park-2025'
SPLITS = ('calibration', 'validation_layers', 'validation_bias',
          'validation_position', 'geometry_challenge')


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def axis_value(axis, pixel):
    if (axis['scale'] not in ('linear', 'log10') or
            not all(math.isfinite(v) for v in (pixel, axis['slope'], axis['intercept'])) or
            axis['slope'] == 0):
        raise ValueError('invalid axis transform')
    value = axis['slope']*pixel+axis['intercept']
    try:
        result = 10**value if axis['scale'] == 'log10' else value
    except OverflowError as error:
        raise ValueError('axis overflow') from error
    if not math.isfinite(result) or (axis['scale'] == 'log10' and result <= 0):
        raise ValueError('nonfinite or nonpositive log coordinate')
    return result


def pixel_value(axis, value):
    if not math.isfinite(value) or (axis['scale'] == 'log10' and value <= 0):
        raise ValueError('invalid inverse-axis coordinate')
    axis_value(axis, 0.)  # Validate slope and transform before division.
    transformed = math.log10(value) if axis['scale'] == 'log10' else value
    return (transformed-axis['intercept'])/axis['slope']


def validate_condition(value):
    if (not isinstance(value, dict) or
            value.get('status') not in ('reported', 'derived', 'assumed', 'unavailable') or
            not value.get('unit') or not value.get('source')):
        raise ValueError('condition lacks status, units or source')
    if value['status'] == 'unavailable':
        if value.get('value') is not None or not value.get('reason'):
            raise ValueError('unavailable conditions require null and reason')
    elif value.get('value') is None:
        raise ValueError('known condition has null value')
    if value['status'] == 'assumed' and not value.get('reason'):
        raise ValueError('assumption needs a reason')
    if isinstance(value['value'], (float, int)) and not math.isfinite(value['value']):
        raise ValueError('nonfinite condition')


def validate_reference(manifest, dataset, extraction):
    if (manifest['schema'] != 'evacam_park_reference_v1' or
            dataset['schema'] != 'evacam_park_curves_v1' or
            extraction['schema'] != 'evacam_park_extraction_v1'):
        raise ValueError('unsupported reference schema')
    if manifest['version'] != dataset['version']:
        raise ValueError('reference version mismatch')
    if not manifest['partitions']['frozen_before_fitting'] or manifest['partitions']['fitting_performed']:
        raise ValueError('this reference version must remain frozen before fitting')
    for value in manifest['geometry'].values():
        validate_condition(value)
    for value in manifest['models'].values():
        if isinstance(value, dict):
            validate_condition(value)
    cases = {case['id']: case for case in manifest['cases']}
    if len(cases) != len(manifest['cases']):
        raise ValueError('duplicate case ID')
    membership = {}
    for split in SPLITS:
        for cid in manifest['partitions'][split]:
            if cid in membership or cid not in cases:
                raise ValueError('duplicate or unknown partition member')
            membership[cid] = split
    if set(membership) != set(cases):
        raise ValueError('unpartitioned case')
    for group in manifest['partitions']['dependency_groups']:
        if any(cid not in cases for cid in group):
            raise ValueError('unknown dependency-group case')
        roles = {membership[cid] == 'calibration' for cid in group}
        if len(roles) != 1:
            raise ValueError('correlated physical cases leak across calibration boundary')
    for cid, case in cases.items():
        if case['split'] != membership[cid]:
            raise ValueError('case split disagrees with partition')
        for value in case.values():
            if isinstance(value, dict):
                validate_condition(value)
        decisive = ('active_word_lines', 'bitline_voltage', 'source_voltage',
                    'temperature', 'threshold_charge_state', 'selected_word_line', 'pass_voltage')
        if case['quantitative_validation_eligible'] and (
                case['blocking_inputs'] or any(case[key]['status'] in ('unavailable', 'assumed') for key in decisive)):
            raise ValueError('missing or assumed inputs cannot silently become validated')
        if (case['quantitative_validation_eligible'] and case['taper_angle']['value'] > 0 and
                manifest['geometry']['taper_diameter_rule']['status'] == 'unavailable'):
            raise ValueError('taper validation needs independently specified diameters')
    axes = extraction['axes']
    tick_errors = []
    for name, axis in axes.items():
        if axis['x_unit'] not in ('V', '1') or axis['y_unit'] not in ('uA', 'uS'):
            raise ValueError('unknown publication units')
        if axis['x_si_factor'] != 1. or axis['y_si_factor'] != 1e-6:
            raise ValueError('wrong publication-to-SI unit conversion')
        if axis['image'] not in manifest['sources'] or axis['source_sha256'] != manifest['sources'][axis['image']]['sha256']:
            raise ValueError('axis source provenance mismatch')
        for direction in ('x', 'y'):
            transform = axis[direction]
            for pixel, value in transform['calibration_ticks']+axis['held_back_ticks'][direction]:
                error = abs(pixel_value(transform, value)-pixel)
                if error > axis['tick_uncertainty_px']:
                    raise ValueError('axis tick calibration does not match source')
                tick_errors.append(dict(axis=name, direction=direction, error_pixel=error))
    curve_ids, point_ids, groups = set(), set(), {}
    by_id = {}
    for curve in dataset['curves']:
        if curve['id'] in curve_ids or curve['case_id'] not in cases or not curve['points']:
            raise ValueError('duplicate/empty curve or unknown case')
        curve_ids.add(curve['id'])
        if curve['population'] not in ('tcad', 'spectre') or not curve['trace']:
            raise ValueError('unidentified source population')
        if curve['split'] != membership[curve['case_id']]:
            raise ValueError('curve crosses case partition')
        group = curve['observation_group']
        if group in groups and groups[group] != curve['split']:
            raise ValueError('repeated observations cross partition')
        groups[group] = curve['split']
        axis = axes[curve['axis']]
        if axis.get('unit_status'):
            raise ValueError('ambiguous source units cannot enter usable curves')
        expected_unit = 'S' if axis['y_unit'] == 'uS' else 'A'
        if curve['y_unit_si'] != expected_unit or curve['x_unit_si'] != axis['x_unit']:
            raise ValueError('observable units disagree with axis')
        if curve['observable'] == 'on_current':
            validate_condition(curve['fixed_selected_word_line_voltage'])
            if curve['sweep_variable'] != 'active_word_lines':
                raise ValueError('on-current inset sweeps active layer count')
        elif curve['sweep_variable'] != 'selected_word_line_voltage':
            raise ValueError('transfer curve must identify the selected-WL sweep')
        previous_x = -math.inf
        for point in curve['points']:
            if point['id'] in point_ids:
                raise ValueError('duplicate observation ID')
            point_ids.add(point['id'])
            by_id[point['id']] = (point, curve)
            numeric = point['pixel']+point['publication']+point['si']+point['pixel_uncertainty']+point['y_interval_si']+[point['x_uncertainty_si']]
            if not all(math.isfinite(v) for v in numeric):
                raise ValueError('nonfinite observation')
            if point['censored'] or point['interpolated']:
                raise ValueError('usable points must be direct uncensored observations; exclusions are separate')
            if min(point['pixel_uncertainty']) <= 0 or point['x_uncertainty_si'] <= 0:
                raise ValueError('invalid extraction uncertainty')
            for i, direction in enumerate(('x', 'y')):
                if not 0 <= point['pixel'][i] < axis['image_size'][i]:
                    raise ValueError('pixel outside source image')
                value = axis_value(axis[direction], point['pixel'][i])
                if not math.isclose(value, point['publication'][i], rel_tol=1e-10, abs_tol=1e-30):
                    raise ValueError('publication coordinate does not invert to source pixel')
                if not math.isclose(value*axis[direction+'_si_factor'], point['si'][i], rel_tol=1e-10, abs_tol=1e-30):
                    raise ValueError('incorrect SI coordinate')
            if point['si'][0] <= previous_x:
                raise ValueError('sweep coordinates must increase strictly')
            previous_x = point['si'][0]
            if not point['y_interval_si'][0] <= point['si'][1] <= point['y_interval_si'][1]:
                raise ValueError('uncertainty interval excludes observation')
            for endpoint, offset in zip(point['y_interval_si'], (-1, 1)):
                shifted = point['pixel'][1]+offset*point['pixel_uncertainty'][1]*math.copysign(1, axis['y']['slope'])
                expected = axis_value(axis['y'], shifted)*axis['y_si_factor']
                if not math.isclose(endpoint, expected, rel_tol=1e-10, abs_tol=1e-30):
                    raise ValueError('incorrect ordinate uncertainty transform')
            expected_x = abs(axis['x']['slope'])*point['pixel_uncertainty'][0]*axis['x_si_factor']
            if not math.isclose(expected_x, point['x_uncertainty_si'], rel_tol=1e-10):
                raise ValueError('incorrect abscissa uncertainty')
            if (curve['observable'] == 'on_current' and
                    abs(point['si'][0]-cases[curve['case_id']]['active_word_lines']['value']) > point['x_uncertainty_si']):
                raise ValueError('inset pixel does not agree with the named integer layer count')
            if axis['y']['scale'] == 'log10':
                if point['si'][1] <= 0 or not math.isclose(point['y_uncertainty_decades'], abs(axis['y']['slope'])*point['pixel_uncertainty'][1], rel_tol=1e-10):
                    raise ValueError('invalid log-current uncertainty')
            elif point['y_uncertainty_decades'] is not None:
                raise ValueError('linear observation has log uncertainty')
    picks = {p['point_id']: p for p in extraction['picks']}
    if {c['case_id'] for c in dataset['curves']} != set(cases):
        raise ValueError('case lacks a readable source series')
    if set(picks) != point_ids or len(picks) != len(extraction['picks']):
        raise ValueError('missing or duplicate raw extraction record')
    for pid, (point, curve) in by_id.items():
        pick = picks[pid]
        if pick['final_pixel'] != point['pixel'] or pick['axis'] != curve['axis'] or not pick['method']:
            raise ValueError('raw extraction provenance mismatch')
        if pick['initial_pixel'] != pick['final_pixel'] and not pick['edited']:
            raise ValueError('manual change lacks edit flag')
    for exclusion in extraction['unusable']+extraction['excluded_points']:
        if not exclusion.get('reason'):
            raise ValueError('exclusion lacks a reason')
    for point in extraction['log_unit_diagnostic']:
        axis = axes['fig7b']
        value = axis_value(axis['y'], point['pixel'][1])
        if (not math.isclose(point['selected_wl_voltage_v'], axis_value(axis['x'], point['pixel'][0]), abs_tol=1e-12) or
                not math.isclose(point['printed_numeric_ordinate'], value, rel_tol=1e-10) or
                not math.isclose(point['printed_uA_to_A'], value*1e-6, rel_tol=1e-10) or
                not math.isclose(point['alternative_if_label_should_be_A'], value, rel_tol=1e-10)):
            raise ValueError('log-unit diagnostic silently changed its interpretation')
    repeats = extraction['independent_recheck']
    for repeat in repeats:
        point, curve = by_id[repeat['point_id']]
        difference = repeat['independent_y_pixel']-repeat['manual_y_pixel']
        if (repeat['manual_y_pixel'] != point['pixel'][1] or repeat['axis'] != curve['axis'] or
                not math.isclose(difference, repeat['difference_pixel'], abs_tol=1e-12) or
                abs(difference) > point['pixel_uncertainty'][1]):
            raise ValueError('independent extraction exceeds stated uncertainty')
    return dict(status='reference_checked_model_validation_pending', cases=len(cases),
                curves=len(curve_ids), points=len(point_ids),
                cases_by_split=dict(Counter(c['split'] for c in cases.values())),
                points_by_population=dict(Counter(c['population'] for c in dataset['curves'] for _ in c['points'])),
                quantitative_validation_eligible_cases=sum(c['quantitative_validation_eligible'] for c in cases.values()),
                maximum_tick_error_pixel=max(t['error_pixel'] for t in tick_errors),
                independent_rechecks=len(repeats), maximum_repeat_difference_pixel=max(abs(r['difference_pixel']) for r in repeats),
                unusable=extraction['unusable'], tick_checks=tick_errors,
                note='No EvaCAM fitting or predictions; these checks validate extraction integrity only.')


def load_reference(reference_path=REFERENCE, data_dir=DATA):
    manifest = yaml.safe_load(Path(reference_path).read_text())
    for filename, digest in manifest['data_hashes'].items():
        if sha256(Path(data_dir)/filename) != digest:
            raise ValueError('reference hash mismatch: '+filename)
    dataset = json.loads((Path(data_dir)/'curves.json').read_text())
    extraction = json.loads((Path(data_dir)/'extraction.json').read_text())
    return manifest, dataset, extraction


def write_report(output, manifest, dataset, extraction, report, source_cache=None):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    os.environ.setdefault('MPLCONFIGDIR', str(output/'matplotlib-cache'))
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    plt.rcParams.update({'font.size': 9, 'axes.grid': True, 'grid.alpha': .2})
    panels = ['fig4c', 'fig5', 'fig5_inset', 'fig6', 'fig7a', 'fig7c', 'fig9a', 'fig9b']
    titles = ['Calibration: short strings', 'Held out: layer count', 'Held out: on-current inset',
              'Held out: bitline bias', 'Held out: selected WL', 'Published Gm peak samples',
              'Challenge: taper angle', 'Challenge: tapered WL position']
    fig, axs = plt.subplots(3, 3, figsize=(15, 12))
    for tag, title, ax in zip(panels, titles, axs.flat):
        selected = [c for c in dataset['curves'] if c['axis'] == tag]
        ids = list(dict.fromkeys(c['case_id'] for c in selected))
        for curve in selected:
            points = curve['points']
            color = f'C{ids.index(curve["case_id"])}'
            x = [p['si'][0] for p in points]
            y = [p['si'][1]*1e6 for p in points]
            if curve['population'] == 'tcad':
                ax.errorbar(x, y, xerr=[p['x_uncertainty_si'] for p in points],
                            yerr=[[p['si'][1]*1e6-p['y_interval_si'][0]*1e6 for p in points],
                                  [p['y_interval_si'][1]*1e6-p['si'][1]*1e6 for p in points]],
                            fmt='o', ms=3, color=color, label=curve['case_id'], capsize=2)
            else:
                ax.plot(x, y, ':', color=color, marker='.', ms=2)
        ax.set(title=title, xlabel='Active WL count' if tag == 'fig5_inset' else 'Selected WL voltage (V)',
               ylabel='Gm (uS)' if tag == 'fig7c' else 'BL current (uA)')
        if tag == 'fig5_inset':
            ax.set_yscale('log')
        ax.legend(fontsize=7)
    axs.flat[-1].axis('off')
    axs.flat[-1].text(.02, .9, 'Source audit\n\nCircles: TCAD markers\nDotted: authors\' Spectre fit\n\nPartial readable domains only.\nFigure 7b units remain ambiguous.\nNo EvaCAM prediction or fit.\nMissing operating inputs remain explicit.',
                      va='top', linespacing=1.5)
    fig.suptitle('Park 2025: frozen reference extraction (uncertainty from source pixels)', fontsize=14)
    fig.tight_layout()
    for suffix in ('png', 'svg'):
        fig.savefig(output/f'reference-curves.{suffix}', dpi=170)
    plt.close(fig)
    fig, axs = plt.subplots(1, 2, figsize=(12, 4))
    ticks = report['tick_checks']
    axs[0].plot([t['error_pixel'] for t in ticks], 'o', ms=3)
    axs[0].axhline(2., color='gray', ls='--', label='Declared tick tolerance')
    axs[0].set(xlabel='Calibration / held-back tick check', ylabel='Absolute error (pixel)',
               title='Axis consistency', ylim=(0, 2.3))
    axs[0].legend(fontsize=8)
    repeats = extraction['independent_recheck']
    axs[1].plot([r['difference_pixel'] for r in repeats], 'o', ms=4)
    for limit in (-6., 6.):
        axs[1].axhline(limit, color='gray', ls='--')
    axs[1].set(xlabel='Marker recheck index', ylabel='Threshold center minus manual center (pixel)',
               title='Second extraction method: 14 isolated plateaus', ylim=(-7, 7))
    fig.suptitle('Extraction checks only; these residuals are not device-model errors')
    fig.tight_layout()
    fig.savefig(output/'extraction-quality.png', dpi=170)
    plt.close(fig)
    report['source_cache_verified'] = False
    if source_cache is not None:
        source_cache = Path(source_cache)
        for filename, source in manifest['sources'].items():
            if sha256(source_cache/filename) != source['sha256']:
                raise ValueError('cached source hash mismatch: '+filename)
        report['source_cache_verified'] = True
        for tag, axis in extraction['axes'].items():
            fig, ax = plt.subplots(figsize=(10, 7))
            ax.imshow(plt.imread(source_cache/axis['image']))
            for curve in dataset['curves']:
                if curve['axis'] != tag:
                    continue
                ax.plot([p['pixel'][0] for p in curve['points']], [p['pixel'][1] for p in curve['points']],
                        'o' if curve['population'] == 'tcad' else 'x', ms=5, mfc='none',
                        label=curve['case_id']+' '+curve['population'])
            if tag == 'fig7b':
                for point in extraction['log_unit_diagnostic']:
                    ax.plot(*point['pixel'], 'o', color='blue', mfc='none', ms=8)
            left, top, right, bottom = axis['crop']
            ax.set(xlim=(left, right), ylim=(bottom, top), title=tag+' source overlay; native pixels')
            if ax.get_legend_handles_labels()[0]:
                ax.legend(fontsize=6, loc='upper left', bbox_to_anchor=(1, 1))
            fig.tight_layout()
            fig.savefig(output/f'source-overlay-{tag}.png', dpi=140)
            plt.close(fig)
    report['provenance'] = dict(reference_sha256=sha256(REFERENCE), checker_sha256=sha256(Path(__file__)),
                                data_hashes=manifest['data_hashes'])
    (output/'report.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'output/validation/nand-park-2025')
    parser.add_argument('--source-cache', type=Path, help='Optional hash-checked originals for source overlays')
    parser.add_argument('--require-model-validation', action='store_true')
    args = parser.parse_args(argv)
    manifest, dataset, extraction = load_reference()
    report = validate_reference(manifest, dataset, extraction)
    write_report(args.output, manifest, dataset, extraction, report, args.source_cache)
    print(f"Checked {report['cases']} cases, {report['curves']} series, {report['points']} observations. Model validation remains pending.")
    return 2 if args.require_model_validation else 0


if __name__ == '__main__':
    raise SystemExit(main())
