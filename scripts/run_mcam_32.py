#!/usr/bin/env python3
"""Stream and plot the exhaustive 32x32 MCAM composition experiment."""

import argparse
import csv
import gzip
import json
import os
from pathlib import Path
import resource
import sys
import time
import traceback

import numpy as np

import plot_mcam_voltage as plot
from mcam_composition_stream import build_nominal_stream


REPO_ROOT = Path(__file__).resolve().parents[1]


def write_status(path, started, phase, **fields):
    """Atomically publish enough state to inspect a long-running job."""
    status = {
        "pid": os.getpid(),
        "phase": phase,
        "elapsed_seconds": time.monotonic() - started,
        "maximum_resident_set_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        **fields,
    }
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps(status, indent=2) + "\n")
    temporary.replace(path)


def sample_selected(matcher, selected, directory, states, variation_level, status_update):
    """Write the bounded selected-composition samples and retain plot coordinates."""
    query = [0] * 32
    x, voltage_mv, latency_ns = [], [], []
    total = sum(1 if plot.vector_from_counts(item.counts) ==
                list(reversed(plot.vector_from_counts(item.counts))) else 2
                for item in selected)
    completed = 0
    sample_rows = 0
    with gzip.open(directory / "samples.csv.gz", "wt", newline="", compresslevel=1) as stream:
        writer = csv.writer(stream)
        writer.writerow(["composition_index", "placement_index", "sample_index",
                         *[f"delta_{delta}_count" for delta in range(states)],
                         "squared_euclidean_distance", "matchline_conductance_s",
                         "matchline_voltage_v", "search_latency_s", "matchline_delay_s"])
        for item in selected:
            stored = plot.vector_from_counts(item.counts)
            placements = [stored]
            if stored and len(set(stored)) > 1:
                placements.append(list(reversed(stored)))
            for placement_index, placement in enumerate(placements):
                results = (matcher.evaluate_distance_samples(placement, query) if variation_level
                           else [item.nominal_result])
                for sample_index, result in enumerate(results):
                    writer.writerow([item.composition_index, placement_index, sample_index, *item.counts,
                                     result.squared_euclidean_distance, result.matchline_conductance,
                                     result.matchline_voltage, result.search_latency, result.matchline_delay])
                    x.append(result.squared_euclidean_distance)
                    voltage_mv.append(result.matchline_voltage * 1e3)
                    latency_ns.append(result.search_latency * 1e9)
                    sample_rows += 1
                completed += 1
                if completed % 25 == 0 or completed == total:
                    status_update(completed_placements=completed, total_placements=total,
                                  completed_sample_rows=sample_rows)
    return np.asarray(x), np.asarray(voltage_mv), np.asarray(latency_ns)


def plot_representatives(streamed, metric):
    """Adapt bounded streamed extrema to plot_voltage's nominal column schema."""
    source = streamed.representatives_by_metric[metric]
    result = np.zeros((len(source), 7), dtype=float)
    result[:, 0] = source[:, 0]
    result[:, 1] = source[:, 2]
    if metric == "voltage":
        result[:, 2] = source[:, 1] / 1e3
    else:
        result[:, 3] = source[:, 1] / 1e9
    return result


def run(args):
    started = time.monotonic()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    status_path = output / "run_status.json"

    def status(phase, **fields):
        write_status(status_path, started, phase, **fields)

    try:
        status("preparing", complete=False)
        runs = []
        for level in args.levels:
            config = (args.config_root / f"stdev{level:02d}"
                      / "2FeFET_MCAM_32x32.config.yaml")
            directory = output / f"stdev{level:02d}" / "32x32"
            generated = plot.generate_run(config, directory, samples=args.samples,
                                          seed=args.seed, granularity=args.granularity)
            runs.append((level, directory, generated))

        envelopes = {}
        maximum_y = {}
        for metric in ("voltage", "latency"):
            for level, _, generated in runs:
                matcher = generated[0]
                nominal_bounds, varied_bounds = generated[5:7]
                envelopes[level, metric] = (
                    plot.smooth_voltage_range(plot.bounds_arrays(nominal_bounds, metric)),
                    plot.smooth_voltage_range(plot.bounds_arrays(varied_bounds, metric)))
            maximum_y[metric] = max(np.max(envelopes[level, metric][1][2]) for level in args.levels)

        all_bounds = [bound for _, _, generated in runs for bounds in generated[5:7] for bound in bounds]
        shared = output / "shared_nominal"
        shared.mkdir(exist_ok=True)
        total = 15_380_937

        def nominal_progress(completed, stream_total=total):
            status("streaming_nominal", complete=False, completed_nominal=completed,
                   total_nominal=stream_total)

        status("streaming_nominal", complete=False, completed_nominal=0, total_nominal=total)
        nominal = build_nominal_stream(
            runs[0][2][1], shared, 8, all_bounds, maximum_y, args.sample_compositions,
            progress_interval=args.progress_interval, plot_y_bins=2000,
            progress_callback=nominal_progress)
        if nominal.total_count != total:
            raise ValueError(f"Expected {total:,} nominal compositions, received {nominal.total_count:,}")

        for level, directory, generated in runs:
            matcher, _, memory = generated[:3]
            status("sampling", complete=False, level=level, completed_placements=0,
                   completed_sample_rows=0)

            def sample_progress(**fields):
                status("sampling", complete=False, level=level, **fields)

            sample_x, sample_mv, sample_ns = sample_selected(
                matcher, nominal.selected, directory, 8, level, sample_progress)
            label = f"resistance sigma/R = {level}%, {memory['variation']['monte_carlo_granularity']}"
            for metric, sample_y in (("voltage", sample_mv), ("latency", sample_ns)):
                nominal_envelope, varied_envelope = envelopes[level, metric]
                plot.validate_containment(sample_x, sample_y, varied_envelope)
                representatives = plot_representatives(nominal, metric)
                groups = nominal.groups_by_metric[metric]
                for active in (False, True):
                    status("rendering", complete=False, level=level, metric=metric,
                           view="active" if active else "full")
                    maximum_distance = nominal.active_distance if active else 32 * 7**2
                    plot.plot_voltage(directory, 32, label, representatives, groups,
                                      sample_x, sample_y, nominal_envelope, varied_envelope,
                                      maximum_distance, maximum_y[metric], generated[6],
                                      active=active, metric=metric)
            metadata_path = directory / "metadata.json"
            metadata = json.loads(metadata_path.read_text())
            metadata.update({
                "nominal_composition_count": nominal.total_count,
                "nominal_compositions_file": str((shared / "nominal_compositions.csv.gz").relative_to(output)),
                "nominal_plot_groups_directory": str(shared.relative_to(output)),
                "monte_carlo_selected_compositions": len(nominal.selected),
                "monte_carlo_composition_target": args.sample_compositions,
                "nominal_streaming": True,
            })
            metadata_path.write_text(json.dumps(metadata, indent=2) + "\n")
            status("level_complete", complete=False, level=level)
        status("complete", complete=True, completed_nominal=nominal.total_count,
               total_nominal=nominal.total_count)
    except BaseException as error:
        status("error", complete=False, error=str(error), traceback=traceback.format_exc())
        raise


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config-root", type=Path,
                        default=REPO_ROOT / "config/2FeFET_MCAM_variation")
    parser.add_argument("--output-dir", type=Path,
                        default=REPO_ROOT / "results/mcam_state_variation_32x32")
    parser.add_argument("--levels", type=int, nargs="+", choices=(0, 5, 10), default=[0, 5, 10])
    parser.add_argument("--samples", type=int, default=1000)
    parser.add_argument("--seed", type=int, default=9876)
    parser.add_argument("--granularity", choices=("cell", "effective"), default="cell")
    parser.add_argument("--sample-compositions", type=int, default=2000)
    parser.add_argument("--progress-interval", type=int, default=100000)
    args = parser.parse_args(argv)
    if args.samples < 2 or args.sample_compositions < 1 or args.progress_interval < 1:
        parser.error("samples must be at least 2; composition and progress counts must be positive")
    if not 0 <= args.seed <= 2**32 - 1:
        parser.error("--seed must fit an unsigned 32-bit integer")
    run(args)


if __name__ == "__main__":
    main()
