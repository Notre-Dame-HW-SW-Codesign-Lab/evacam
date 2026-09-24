#!/usr/bin/env python3
"""TV plots of exact MCAM distance and TCAM Hamming-distance voltage extrema."""

import argparse
import csv
import json
from pathlib import Path
import sys

import numpy as np
import yaml

from plot_mcam_voltage import prepare_config, plt, matplotlib

ROOT = Path(__file__).resolve().parents[1]


def prepare_inputs(model, size, level, directory, source=None):
    """Snapshot inputs without a Monte Carlo sweep, retaining input support."""
    if source is None:
        source = (ROOT / 'config/2FeFET_MCAM_variation' / f'stdev{level:02d}' /
                  f'2FeFET_MCAM_{size}x{size}.config.yaml' if model == 'mcam' else
                  ROOT / 'config/2FeFET_TCAM/2FeFET_TCAM_match.config.yaml')
    actual, memory = prepare_config(source, directory, nominal=True)
    memory['variation'] = {'mode': 'single_point', 'seed': 9876}
    if model == 'tcam':
        memory['variation'].update(memory_device_resistance_on_stdev=f'{level}%',
                                   memory_device_resistance_off_stdev=f'{level}%')
        architecture_path = directory / 'architecture.yaml'
        architecture = yaml.safe_load(architecture_path.read_text())
        architecture['memory'].update(capacity=f'{size * size // 8}B', word_width=f'{size}bits')
        architecture['organization']['subarray']['dimensions'] = [size, size]
        architecture_path.write_text(yaml.safe_dump(architecture, sort_keys=False))
    (directory / 'memory_device.yaml').write_text(yaml.safe_dump(memory, sort_keys=False))
    return actual


def calculate_extrema(matcher, model, size):
    """Return exact support endpoints, with no sampling or output-SD estimate."""
    if model == 'mcam':
        nominal = matcher.distance_voltage_bounds([0] * size, include_variation=False)
        varied = matcher.distance_voltage_bounds([0] * size)
        return [dict(distance=int(n.squared_euclidean_distance),
                     nominal_min_voltage_v=n.minimum_voltage, nominal_max_voltage_v=n.maximum_voltage,
                     min_voltage_v=v.minimum_voltage, max_voltage_v=v.maximum_voltage,
                     sensing_time_s=v.sensing_time)
                for n, v in zip(nominal, varied)]
    return [dict(distance=h,
                 nominal_min_voltage_v=matcher.sense_tcam_mismatches(h),
                 nominal_max_voltage_v=matcher.sense_tcam_mismatches(h),
                 min_voltage_v=matcher.sense_tcam_mismatches(h, -3),
                 max_voltage_v=matcher.sense_tcam_mismatches(h, 3)) for h in range(size + 1)]


def draw_extrema(records, model, size, level, directory, nominal=None, voltage_samples=None):
    """Render full and active ranges, preserving gaps at unreachable distances."""
    maximum = records[-1]['distance']
    x = np.arange(maximum + 1)
    arrays = {key: np.full(maximum + 1, np.nan) for key in
              ('nominal_min_voltage_v', 'nominal_max_voltage_v', 'min_voltage_v', 'max_voltage_v')}
    for record in records:
        for key, values in arrays.items():
            values[record['distance']] = 1000 * record[key]
    low, high = arrays['min_voltage_v'], arrays['max_voltage_v']
    nlow, nhigh = arrays['nominal_min_voltage_v'], arrays['nominal_max_voltage_v']
    near = np.flatnonzero(high > high[0] * .04)
    zoom = min(maximum, max(4 if model == 'tcam' else 12, int(near[-1]) + 2))
    metric = 'Squared Euclidean Distance' if model == 'mcam' else 'Hamming Distance'
    with plt.rc_context({'font.family': 'DejaVu Sans', 'font.size': 13,
                         'figure.facecolor': 'white', 'axes.facecolor': 'white'}):
        for active, limit in ((False, maximum), (True, zoom)):
            fig, axis = plt.subplots(figsize=(12, 7))
            fig.suptitle(f'{model.upper()} Matchline Voltage by {metric}', fontsize=18, fontweight='bold')
            axis.set_title(f'{size}x{size} array — {level}% resistance standard deviation — '
                           + ('active-region detail' if active else 'full range'), pad=12)
            axis.axhspan(low[0], high[0], color='#C9C9C9', label='Exact-match extrema')
            for boundary in np.unique([low[0], high[0]]):
                axis.axhline(boundary, color='#555555', linestyle='--', linewidth=1.3)
            visible = x <= limit
            if level:
                axis.fill_between(x[visible], low[visible], high[visible], color='#E7AA37',
                                  label='Extrema from ±3σ input resistance limits')
                for values in (low, high):
                    axis.plot(x[visible], values[visible], color='#A35E00', linewidth=1.5)
            axis.fill_between(x[visible], nlow[visible], nhigh[visible], color='#6FA6CE',
                              label='Nominal composition extrema' if model == 'mcam' else 'Nominal voltage')
            for values in (nlow, nhigh) if model == 'mcam' else (nlow,):
                axis.plot(x[visible], values[visible], color='#092A50', linewidth=2.5)
            if nominal is not None:
                from plot_mcam_distance_statistics import plot_all_points
                artist = plot_all_points(axis, nominal, np.array([r['distance'] for r in records]),
                                         voltage_samples, limit, tv=True)
                fig.colorbar(artist, ax=axis, pad=.02).set_label('Distinct nonzero deltas in composition')
            axis.set_xlabel(r'Squared Euclidean distance, $D^2$' if model == 'mcam' else 'Hamming distance (mismatched cells)')
            axis.set_ylabel('Matchline (ML) voltage (mV)')
            axis.set_xlim(0, limit * 1.02)
            axis.set_ylim(-.025 * np.nanmax(high), 1.1 * np.nanmax(high))
            axis.xaxis.set_major_locator(matplotlib.ticker.MaxNLocator(nbins=9, integer=True))
            axis.spines[['top', 'right']].set_visible(False)
            axis.grid(True, color='#d9dee3', linewidth=.7)
            axis.set_axisbelow(True)
            axis.legend(loc='upper right', fontsize=11, framealpha=1)
            fig.text(.5, .035, 'Fixed nominal one-mismatch sensing instant; '
                     + ('exact nominal extrema.' if not level else
                        'exact extrema over bounded input resistances; no output-distribution coverage implied.'),
                     ha='center', fontsize=10)
            fig.subplots_adjust(left=.115, right=.97, bottom=.16, top=.86)
            stem = 'voltage_extrema_active_region' if active else 'voltage_extrema'
            for extension in ('png', 'pdf', 'svg'):
                fig.savefig(directory / f'{stem}.{extension}', dpi=200, bbox_inches='tight')
            plt.close(fig)


def generate_extrema(model, size, level, directory, source=None, nominal=None, voltage_samples=None):
    sys.path.insert(0, str(ROOT))
    import evacam_py
    directory.mkdir(parents=True, exist_ok=False)
    actual = prepare_inputs(model, size, level, directory / 'inputs', source)
    records = calculate_extrema(evacam_py.EvaCAMMatch(str(actual)), model, size)
    with (directory / 'voltage_extrema.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)
    (directory / 'extrema_metadata.json').write_text(json.dumps({
        'model': model, 'size': size, 'resistance_stdev_percent': level,
        'method': 'native distance dynamic programming' if model == 'mcam' else 'native parallel on/off resistance endpoints',
        'support': '[max(R*1e-12, R-3*sigma), R+3*sigma]',
        'sensing': 'fixed nominal one-mismatch sensing instant; access and wire resistance held nominal',
        'sampling': 'none; extrema are support bounds, not output standard deviations or quantiles',
        'query': 'all zero; TCAM Hamming distance counts mismatched cells',
        'inputs': 'inputs/run.config.yaml',
    }, indent=2) + '\n')
    draw_extrema(records, model, size, level, directory, nominal, voltage_samples)
    print(f'Rendered extrema: {directory}', flush=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--models', nargs='+', choices=('mcam', 'tcam'), default=['mcam', 'tcam'])
    parser.add_argument('--sizes', nargs='+', type=int, choices=(8, 16, 32, 64), default=[8, 16, 32, 64])
    parser.add_argument('--levels', nargs='+', type=int, choices=(0, 5, 10), default=[0, 5, 10])
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'results/cam_voltage_extrema_tv')
    args = parser.parse_args(argv)
    args.output_dir.mkdir(parents=True, exist_ok=False)
    for model in args.models:
        for size in args.sizes:
            for level in args.levels:
                generate_extrema(model, size, level, args.output_dir / model / f'stdev{level:02d}' / f'{size}x{size}')


if __name__ == '__main__':
    main()
