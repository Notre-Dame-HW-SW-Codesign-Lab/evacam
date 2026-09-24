#!/usr/bin/env python3
"""Plot MCAM statistics; --mode extrema creates MCAM/TCAM TV input-extrema plots."""

import argparse
from collections import defaultdict
import csv
import gzip
import json
import math
import os
from pathlib import Path
import sys

os.environ.setdefault("MPLCONFIGDIR", "/tmp/evacam-matplotlib")

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import yaml


REPO_ROOT = Path(__file__).resolve().parents[1]
BLUE = "#3366a6"
ORANGE = "#d66b29"
GRAY = "#626b73"


def prepare_config(source, directory, *, samples=None, seed=None, granularity=None, nominal=False):
    """Snapshot the local fileset and apply overrides without editing its source."""
    source = Path(source).resolve()
    directory = Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    config = yaml.safe_load(source.read_text())
    architecture_source = (source.parent / config["architecture"]).resolve()
    architecture = yaml.safe_load(architecture_source.read_text())
    sensing_source = (architecture_source.parent / architecture["sensing"]).resolve()
    sensing = yaml.safe_load(sensing_source.read_text())
    sensing["sense_amplifier"] = str((sensing_source.parent / sensing["sense_amplifier"]).resolve())
    cell_source = (source.parent / config["cell"]).resolve()
    cell = yaml.safe_load(cell_source.read_text())
    memory_source = (cell_source.parent / cell["memory_device"]).resolve()
    memory = yaml.safe_load(memory_source.read_text())
    if nominal:
        memory.pop("variation", None)
    else:
        variation = memory.get("variation", {})
        if variation.get("mode") not in ("single_point", "monte_carlo"):
            raise ValueError("State-variation plots require single_point or monte_carlo mode")
        for key, value in (("samples", samples), ("seed", seed), ("monte_carlo_granularity", granularity)):
            if value is not None:
                variation[key] = value
        if "seed" not in variation:
            raise ValueError("State-variation plots require an explicit reproducible seed")
        memory["variation"] = variation
    config["technology"] = str((source.parent / config["technology"]).resolve())
    config["architecture"] = "architecture.yaml"
    config["cell"] = "cell.yaml"
    architecture["sensing"] = "sensing.yaml"
    cell["memory_device"] = "memory_device.yaml"
    for name, data in (("run.config.yaml", config), ("architecture.yaml", architecture),
                       ("sensing.yaml", sensing), ("cell.yaml", cell), ("memory_device.yaml", memory)):
        (directory / name).write_text(yaml.safe_dump(data, sort_keys=False))
    return directory / "run.config.yaml", memory


def bounds_arrays(bounds, metric="voltage"):
    """Represent reachable integer distances on a grid with gaps marked NaN."""
    if not bounds:
        raise ValueError("No reachable distance bounds")
    distances = np.arange(int(bounds[-1].squared_euclidean_distance) + 1)
    lower = np.full(len(distances), np.nan)
    upper = lower.copy()
    for bound in bounds:
        distance = int(bound.squared_euclidean_distance)
        lower[distance] = (bound.minimum_voltage * 1e3 if metric == "voltage"
                           else bound.minimum_search_latency * 1e9)
        upper[distance] = (bound.maximum_voltage * 1e3 if metric == "voltage"
                           else bound.maximum_search_latency * 1e9)
    return distances, lower, upper


def smooth_voltage_range(voltage_range):
    """Interpolate smoothly through every reachable bound without moving knots."""
    distances, lower, upper = (np.asarray(values, dtype=float) for values in voltage_range)
    valid = np.isfinite(lower) & np.isfinite(upper)
    if not np.any(valid) or np.any(lower[valid] > upper[valid]):
        raise ValueError("Expected finite, ordered bounds")
    knots = distances[valid]
    if len(knots) == 1:
        return knots.copy(), lower[valid].copy(), upper[valid].copy()
    dense = np.unique(np.concatenate((np.linspace(knots[0], knots[-1],
                                                   int(knots[-1] - knots[0]) * 4 + 1), knots)))

    interval = np.clip(np.searchsorted(knots, dense, side="right") - 1, 0, len(knots) - 2)
    fraction = (dense - knots[interval]) / (knots[interval + 1] - knots[interval])
    weight = fraction * fraction * (3 - 2 * fraction)

    def interpolate(values):
        values = values[valid]
        return values[interval] * (1 - weight) + values[interval + 1] * weight

    # The same convex interpolation weight preserves lower/upper ordering and
    # produces continuous, flat-tangent joins through every exact endpoint.
    return dense, interpolate(lower), interpolate(upper)


def validate_containment(distances, voltages_mv, envelope):
    """Fail rendering if even one point falls outside the enclosing band."""
    x, lower, upper = envelope
    distances = np.asarray(distances)
    voltages_mv = np.asarray(voltages_mv)
    if (np.any(~np.isfinite(distances)) or np.any(~np.isfinite(voltages_mv))
            or np.any(distances < x[0]) or np.any(distances > x[-1])
            or np.any(voltages_mv < np.interp(distances, x, lower) - 1e-8)
            or np.any(voltages_mv > np.interp(distances, x, upper) + 1e-8)):
        raise ValueError("A point lies outside the range")


def enumerate_delta_counts(dimensions, states):
    """Yield all weak compositions of dimensions across the symbol deltas."""
    counts = [0] * states
    def visit(index, remaining):
        if index == states - 1:
            counts[index] = remaining
            yield tuple(counts)
            return
        for count in range(remaining + 1):
            counts[index] = count
            yield from visit(index + 1, remaining - count)
    yield from visit(0, dimensions)


def vector_from_counts(counts):
    return [delta for delta, count in enumerate(counts) for _ in range(count)]


def evaluate_nominal_compositions(nominal_matcher, dimensions, states):
    """Use the C++ batch path when available, retaining a test-friendly fallback."""
    if hasattr(nominal_matcher, "evaluate_zero_query_compositions"):
        return [(tuple(row.delta_counts), int(row.coordinate_permutations), row.result)
                for row in nominal_matcher.evaluate_zero_query_compositions()]
    query = [0] * dimensions
    rows = []
    for counts in enumerate_delta_counts(dimensions, states):
        permutations = math.factorial(dimensions)
        for count in counts:
            permutations //= math.factorial(count)
        rows.append((counts, permutations,
                     nominal_matcher.evaluate_distance(vector_from_counts(counts), query)))
    return rows


def select_sample_compositions(rows, bounds, budget):
    """Choose mixed interiors across distance plus every exact bound witness."""
    by_distance = defaultdict(list)
    lookup = {counts: index for index, (counts, _, _) in enumerate(rows)}
    for index, (counts, _, result) in enumerate(rows):
        by_distance[int(result.squared_euclidean_distance)].append(index)
    selected = set()
    for bound in bounds:
        for field in ("minimum_conductance_delta_counts", "maximum_conductance_delta_counts"):
            counts = tuple(getattr(bound, field, ()))
            if counts in lookup:
                selected.add(lookup[counts])
    distances = sorted(by_distance)
    remaining = max(0, budget - len(selected))
    if remaining:
        per_distance = max(1, remaining // max(1, len(distances)))
        for distance in distances:
            candidates = sorted(by_distance[distance],
                                key=lambda i: (sum(c > 0 for c in rows[i][0][1:]), rows[i][0]), reverse=True)
            if len(candidates) <= per_distance:
                selected.update(candidates)
            else:
                positions = np.linspace(0, len(candidates) - 1, per_distance, dtype=int)
                selected.update(candidates[position] for position in positions)
    if len(selected) < budget:
        remaining_indices = [i for i in range(len(rows)) if i not in selected]
        positions = np.linspace(0, len(remaining_indices) - 1,
                                min(budget - len(selected), len(remaining_indices)), dtype=int)
        selected.update(remaining_indices[position] for position in positions)
    return sorted(selected)


def generate_samples(matcher, nominal_matcher, directory, states, bounds=None, sample_budget=1600,
                     variation_level=1):
    """Export every nominal composition and a documented stratified MC subset."""
    dimensions = matcher.vector_dimensions()
    query = [0] * dimensions
    rows = evaluate_nominal_compositions(nominal_matcher, dimensions, states)
    expected = math.comb(dimensions + states - 1, states - 1)
    if len(rows) != expected:
        raise ValueError(f"Expected {expected} compositions, received {len(rows)}")
    nominal = []
    with (directory / "nominal_compositions.csv").open("w", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["composition_index", *[f"delta_{delta}_count" for delta in range(states)],
                         "coordinate_permutations", "nonzero_delta_kinds", "changed_coordinates",
                         "squared_euclidean_distance", "matchline_conductance_s", "matchline_voltage_v",
                         "search_latency_s", "matchline_delay_s"])
        for composition_index, (counts, permutations, result) in enumerate(rows):
            values = [int(result.squared_euclidean_distance), result.matchline_conductance,
                      result.matchline_voltage, result.search_latency, result.matchline_delay]
            writer.writerow([composition_index, *counts, permutations, sum(c > 0 for c in counts[1:]),
                             dimensions - counts[0], *values])
            nominal.append([values[0], values[1], values[2], values[3], values[4],
                            sum(c > 0 for c in counts[1:]), permutations])

    selected = select_sample_compositions(rows, bounds or [], min(sample_budget, len(rows)))
    sample_distances, sample_voltages, sample_latencies = [], [], []
    with gzip.open(directory / "samples.csv.gz", "wt", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["composition_index", "placement_index", "sample_index",
                         *[f"delta_{delta}_count" for delta in range(states)],
                         "squared_euclidean_distance", "matchline_conductance_s", "matchline_voltage_v",
                         "search_latency_s", "matchline_delay_s"])
        for composition_index in selected:
            counts, _, nominal_result = rows[composition_index]
            stored = vector_from_counts(counts)
            placements = [stored]
            if stored and len(set(stored)) > 1:
                alternate = list(reversed(stored))
                if alternate != stored:
                    placements.append(alternate)
            for placement_index, placement in enumerate(placements):
                samples = (matcher.evaluate_distance_samples(placement, query) if variation_level
                           else [nominal_result])
                for sample_index, result in enumerate(samples):
                    writer.writerow([composition_index, placement_index, sample_index, *counts,
                                     result.squared_euclidean_distance, result.matchline_conductance,
                                     result.matchline_voltage, result.search_latency, result.matchline_delay])
                    sample_distances.append(result.squared_euclidean_distance)
                    sample_voltages.append(result.matchline_voltage * 1e3)
                    sample_latencies.append(result.search_latency * 1e9)
    return (np.asarray(nominal), np.asarray(sample_distances),
            np.asarray(sample_voltages), np.asarray(sample_latencies), len(selected))


def group_nominal_for_metric(nominal, metric, maximum_y, directory=None):
    """Bin nominal values at a fixed fraction of the common plotted y range."""
    value_index = 2 if metric == "voltage" else 3
    scale = 1e3 if metric == "voltage" else 1e9
    values = nominal[:, value_index] * scale
    bin_width = maximum_y / 2000 if maximum_y > 0 else 1.0
    groups = defaultdict(lambda: [0.0, 0, 0, 0])
    for row, value in zip(nominal, values):
        key = (int(row[0]), int(np.floor(value / bin_width)))
        group = groups[key]
        group[0] += value
        group[1] += 1
        group[2] += int(row[6])
        group[3] = max(group[3], int(row[5]))
    grouped = np.asarray([[distance, total / count, count, permutations, diversity]
                          for (distance, _), (total, count, permutations, diversity)
                          in groups.items()], dtype=float)
    if directory is not None:
        unit = "v" if metric == "voltage" else "s"
        inverse_scale = 1 / scale
        with (directory / f"nominal_{metric}_plot_groups.csv").open("w", newline="") as stream:
            writer = csv.writer(stream)
            writer.writerow(["squared_euclidean_distance", f"mean_{metric}_{unit}",
                             "composition_count", "coordinate_permutations",
                             "maximum_nonzero_delta_kinds", f"display_bin_width_{unit}"])
            for distance, mean, count, permutations, diversity in grouped:
                writer.writerow([int(distance), mean * inverse_scale, int(count), int(permutations),
                                 int(diversity), bin_width * inverse_scale])
    return grouped


def write_bounds(directory, nominal, varied):
    with (directory / "voltage_bounds.csv").open("w", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["squared_euclidean_distance", "nominal_minimum_voltage_v", "nominal_maximum_voltage_v",
                         "variation_minimum_voltage_v", "variation_maximum_voltage_v",
                         "variation_minimum_conductance_s", "variation_maximum_conductance_s",
                         "minimum_conductance_delta_counts", "maximum_conductance_delta_counts"])
        if len(nominal) != len(varied):
            raise ValueError("Nominal and variation reachability differ")
        for base, bound in zip(nominal, varied):
            if base.squared_euclidean_distance != bound.squared_euclidean_distance:
                raise ValueError("Nominal and variation distances differ")
            writer.writerow([bound.squared_euclidean_distance, base.minimum_voltage, base.maximum_voltage,
                             bound.minimum_voltage, bound.maximum_voltage,
                             bound.minimum_conductance, bound.maximum_conductance,
                             json.dumps(list(getattr(bound, "minimum_conductance_delta_counts", ()))),
                             json.dumps(list(getattr(bound, "maximum_conductance_delta_counts", ())))])
    with (directory / "latency_bounds.csv").open("w", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["squared_euclidean_distance", "nominal_minimum_search_latency_s",
                         "nominal_maximum_search_latency_s", "variation_minimum_search_latency_s",
                         "variation_maximum_search_latency_s"])
        for base, bound in zip(nominal, varied):
            writer.writerow([bound.squared_euclidean_distance, base.minimum_search_latency,
                             base.maximum_search_latency, bound.minimum_search_latency,
                             bound.maximum_search_latency])
    with (directory / "bound_witnesses.csv").open("w", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["squared_euclidean_distance", "endpoint", "delta_counts",
                         "conductance_s", "matchline_voltage_v", "search_latency_s",
                         "resistance_support"])
        for bound in varied:
            writer.writerow([bound.squared_euclidean_distance, "minimum_conductance",
                             json.dumps(list(getattr(bound, "minimum_conductance_delta_counts", ()))),
                             bound.minimum_conductance, bound.maximum_voltage,
                             bound.maximum_search_latency, "+/-3 sigma"])
            writer.writerow([bound.squared_euclidean_distance, "maximum_conductance",
                             json.dumps(list(getattr(bound, "maximum_conductance_delta_counts", ()))),
                             bound.maximum_conductance, bound.minimum_voltage,
                             bound.minimum_search_latency, "+/-3 sigma"])


def evaluate_bound_witnesses(matcher, directory, bounds):
    """Evaluate and export the actual corner vectors used by the DP bounds."""
    if not hasattr(matcher, "evaluate_zero_query_composition"):
        return
    with (directory / "corner_results.csv").open("w", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["squared_euclidean_distance", "endpoint", "resistance_sigma_offset",
                         "delta_counts", "matchline_conductance_s", "matchline_voltage_v",
                         "search_latency_s", "matchline_delay_s"])
        for bound in bounds:
            cases = (("minimum_conductance", +3,
                      tuple(bound.minimum_conductance_delta_counts), bound.minimum_conductance,
                      bound.maximum_voltage, bound.maximum_search_latency),
                     ("maximum_conductance", -3,
                      tuple(bound.maximum_conductance_delta_counts), bound.maximum_conductance,
                      bound.minimum_voltage, bound.minimum_search_latency))
            for endpoint, offset, counts, conductance, voltage, latency in cases:
                result = matcher.evaluate_zero_query_composition(counts, offset)
                expected = (bound.squared_euclidean_distance, conductance, voltage, latency)
                actual = (result.squared_euclidean_distance, result.matchline_conductance,
                          result.matchline_voltage, result.search_latency)
                if not np.allclose(actual, expected, rtol=2e-10, atol=1e-18):
                    raise ValueError(f"Corner evaluator disagrees with {endpoint} bound at D^2={expected[0]}")
                writer.writerow([result.squared_euclidean_distance, endpoint, offset,
                                 json.dumps(list(counts)), result.matchline_conductance,
                                 result.matchline_voltage, result.search_latency, result.matchline_delay])


def active_region_distance(nominal_points):
    voltage = nominal_points[:, 2]
    cutoff = np.min(voltage) + 0.05 * np.ptp(voltage)
    return max(1, int(np.max(nominal_points[voltage >= cutoff, 0])))


def plot_voltage(directory, size, label, nominal_points, nominal_groups, sample_x, sample_mv,
                 nominal_envelope, variation_envelope, maximum_distance, maximum_voltage_mv,
                 exact_bounds, active=False, metric="voltage"):
    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 12,
                         "axes.labelsize": 13, "legend.frameon": False, "savefig.facecolor": "white"})
    fig, axis = plt.subplots(figsize=(10.2, 6.0))
    quantity = "Matchline Voltage" if metric == "voltage" else "Search Latency"
    nominal_y = nominal_points[:, 2] * 1e3 if metric == "voltage" else nominal_points[:, 3] * 1e9
    fig.suptitle(f"MCAM {quantity} by Squared Euclidean Distance", fontsize=18, fontweight="bold", y=0.98)
    view = "active-region detail" if active else "full range"
    axis.set_title(f"{size}×{size} array — {label} — {view}", color=GRAY, fontsize=12, pad=10)
    validate_containment(sample_x, sample_mv, variation_envelope)
    validate_containment(nominal_points[:, 0], nominal_y, nominal_envelope)
    for envelope, color, alpha, legend in (
        (variation_envelope, ORANGE, 0.15, "Composition + state variation bounds"),
        (nominal_envelope, BLUE, 0.18, "Nominal composition bounds"),
    ):
        x, lower, upper = envelope
        visible = x <= maximum_distance
        axis.fill_between(x[visible], lower[visible], upper[visible], color=color,
                          alpha=alpha, linewidth=0, label=legend, zorder=2)
    visible = sample_x <= maximum_distance
    axis.scatter(sample_x[visible], sample_mv[visible], color=ORANGE, s=2, alpha=0.035,
                 edgecolors="none", rasterized=True, zorder=3)
    # Streamed large-array groups use object dtype so exact permutation totals
    # can exceed float precision; only display columns are converted here.
    group_x = np.asarray(nominal_groups[:, 0], dtype=float)
    group_y = np.asarray(nominal_groups[:, 1], dtype=float)
    group_counts = np.asarray(nominal_groups[:, 2], dtype=float)
    group_diversity = np.asarray(nominal_groups[:, 4], dtype=float)
    visible = group_x <= maximum_distance
    sizes = 8 + 7 * np.log1p(group_counts[visible])
    points = axis.scatter(group_x[visible], group_y[visible],
                          c=group_diversity[visible], cmap="viridis", vmin=0,
                          vmax=min(size, 7), s=sizes, alpha=0.72,
                          linewidths=0.25, edgecolors="white", rasterized=True, zorder=4,
                          label="Nominal compositions (binned)")
    for count in (1, 10, 100):
        axis.scatter([], [], color=BLUE, alpha=0.72, edgecolors="white", linewidths=0.25,
                     s=8 + 7 * np.log1p(count), label=f"{count} compositions/marker")
    axis.set_xlabel(r"Squared Euclidean distance, $D^2$")
    axis.set_ylabel("Matchline (ML) voltage (mV)" if metric == "voltage" else "Search latency (ns)")
    axis.set_xlim(0, maximum_distance * 1.02)
    axis.set_ylim(-0.025 * maximum_voltage_mv, 1.1 * maximum_voltage_mv)
    axis.xaxis.set_major_locator(matplotlib.ticker.MaxNLocator(nbins=9, integer=True))
    axis.spines[["top", "right"]].set_visible(False)
    axis.grid(True, color="#d9dee3", linewidth=0.7, alpha=0.8)
    axis.set_axisbelow(True)
    axis.legend(loc="upper right", fontsize=10)
    colorbar = fig.colorbar(points, ax=axis, pad=0.015,
                            ticks=np.arange(0, min(size, 7) + 1))
    colorbar.set_label("Maximum distinct nonzero deltas in display bin")
    fig.text(0.5, 0.025, "Marker area shows compositions per display bin; orange points are a stratified Monte Carlo subset.",
             ha="center", fontsize=9, color=GRAY)
    convention = ("fixed nominal sensing instant" if metric == "voltage"
                  else "exact matches use boundary timing; includes peripherals")
    fig.text(0.5, 0.002, f"Query symbol 0; {convention}. Outer band is theoretical output support from ±3σ resistance.",
             ha="center", fontsize=9, color=GRAY)
    fig.subplots_adjust(left=0.115, right=0.97, bottom=0.17, top=0.86)
    stem = f"mcam_{metric}_active_region" if active else f"mcam_{metric}"
    for suffix in ("svg", "pdf", "png"):
        fig.savefig(directory / f"{stem}.{suffix}", dpi=300, bbox_inches="tight")
    plt.close(fig)


def generate_run(config, directory, *, samples=None, seed=None, granularity=None):
    """Prepare one run and its exact bounds; postpone sampling until rendering."""
    sys.path.insert(0, str(REPO_ROOT))
    import evacam_py

    directory.mkdir(parents=True, exist_ok=True)
    actual_config, memory = prepare_config(config, directory / "inputs", samples=samples, seed=seed, granularity=granularity)
    nominal_config, _ = prepare_config(config, directory / "nominal_inputs", nominal=True)
    matcher = evacam_py.EvaCAMMatch(str(actual_config))
    nominal_matcher = evacam_py.EvaCAMMatch(str(nominal_config))
    query = [0] * matcher.vector_dimensions()
    nominal_bounds = matcher.distance_voltage_bounds(query, include_variation=False)
    variation_bounds = matcher.distance_voltage_bounds(query)
    write_bounds(directory, nominal_bounds, variation_bounds)
    evaluate_bound_witnesses(matcher, directory, variation_bounds)
    nominal_envelope = smooth_voltage_range(bounds_arrays(nominal_bounds))
    variation_envelope = smooth_voltage_range(bounds_arrays(variation_bounds))
    validate_containment(bounds_arrays(nominal_bounds)[0][np.isfinite(bounds_arrays(nominal_bounds)[1])],
                         bounds_arrays(nominal_bounds)[1][np.isfinite(bounds_arrays(nominal_bounds)[1])],
                         variation_envelope)
    validate_containment(bounds_arrays(nominal_bounds)[0][np.isfinite(bounds_arrays(nominal_bounds)[2])],
                         bounds_arrays(nominal_bounds)[2][np.isfinite(bounds_arrays(nominal_bounds)[2])],
                         variation_envelope)
    metadata = {
        "source_config": str(config.resolve()), "vector_dimensions": len(query), "query": query,
        "state_variation": memory["mcam"]["state_variation"], "resistance_state": memory["mcam"]["resistance_state"],
        "variation": memory["variation"], "voltage_unit": "V", "conductance_unit": "S",
        "latency_unit": "s", "latency": "search latency including nominal peripherals; exact matches use one-unit-mismatch boundary timing",
        "sensing": "fixed nominal sensing instant",
        "band": "smoothstep interpolation through exact per-distance endpoints; theoretical bounded model support",
        "resistance_support": "[max(R*1e-12, R-3*R*stdev), R+3*R*stdev]",
        "resistance_sigma_over_R": memory["mcam"]["state_variation"],
        "nominal_compositions": "exhaustive weak compositions of vector dimensions across all symbol deltas",
        "nominal_plot_grouping": "nominal_voltage_plot_groups.csv and nominal_latency_plot_groups.csv use separate per-metric display bins at common plotted y maximum / 2000, grouped within exact D^2; marker position is mean output and nominal_compositions.csv is unbinned",
        "monte_carlo_sampling": "deterministic stratified subset by distance and composition diversity, including nominal and variation bound witnesses; budget is a target when mandatory coverage is larger",
        "coordinate_placements": "sorted representative and its reverse for mixed compositions",
        "sample_identity": "sampler draw is keyed by seed + sample index + raw state + coordinate (cell) or shared coordinate 0 (effective)",
        "sample_csv_row_identity": "composition index + placement index + sample index",
        "calibration": "illustrative sensitivity experiment; provisional device data",
    }
    (directory / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    return (matcher, nominal_matcher, memory, nominal_envelope, variation_envelope,
            nominal_bounds, variation_bounds)


def main(argv=None):
    mode_parser = argparse.ArgumentParser(add_help=False)
    mode_parser.add_argument("--mode", choices=("statistics", "support-bounds", "extrema"), default="statistics")
    mode, remaining = mode_parser.parse_known_args(argv)
    if mode.mode == "extrema":
        from plot_cam_extrema import main as extrema_main
        return extrema_main(remaining)
    if mode.mode == "statistics":
        from plot_mcam_distance_statistics import main as statistics_main
        return statistics_main(remaining)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config-root", type=Path, default=REPO_ROOT / "config/2FeFET_MCAM_variation")
    parser.add_argument("--output-dir", type=Path,
                        default=REPO_ROOT / "results/mcam_state_variation_compositions")
    parser.add_argument("--sizes", type=int, nargs="+", choices=(8, 16), default=[8, 16],
                        help="Exhaustive composition plotting is intentionally limited to 8 and 16")
    parser.add_argument("--levels", type=int, nargs="+", choices=(0, 5, 10), default=[0, 5, 10])
    parser.add_argument("--samples", type=int)
    parser.add_argument("--seed", type=int)
    parser.add_argument("--granularity", choices=("cell", "effective"))
    parser.add_argument("--sample-compositions", type=int, default=1600,
                        help="Target compositions for Monte Carlo; mandatory distance/witness coverage may exceed it (default: 1600)")
    parser.add_argument("--metric", choices=("voltage", "latency", "both"), default="both",
                        help="Figures to render (default: both); raw data always includes both metrics")
    args = parser.parse_args(remaining)
    if args.samples is not None and args.samples < 2:
        parser.error("--samples must be at least 2 for Monte Carlo")
    if args.seed is not None and not 0 <= args.seed <= 2**32 - 1:
        parser.error("--seed must fit an unsigned 32-bit integer")
    if args.sample_compositions < 1:
        parser.error("--sample-compositions must be positive")
    for size in args.sizes:
        runs = []
        for level in args.levels:
            config = args.config_root / f"stdev{level:02d}" / f"2FeFET_MCAM_{size}x{size}.config.yaml"
            directory = args.output_dir / f"stdev{level:02d}" / f"{size}x{size}"
            runs.append((level, directory, generate_run(config, directory, samples=args.samples, seed=args.seed,
                                                       granularity=args.granularity)))
        metrics = ("voltage", "latency") if args.metric == "both" else (args.metric,)
        envelopes = {}
        for metric in metrics:
            for level, _, run in runs:
                if metric == "voltage":
                    envelopes[level, metric] = run[3:5]
                    continue
                matcher = run[0]
                nominal_envelope = smooth_voltage_range(bounds_arrays(
                    matcher.distance_voltage_bounds([0] * size, include_variation=False), metric))
                variation_envelope = smooth_voltage_range(bounds_arrays(matcher.distance_voltage_bounds([0] * size), metric))
                envelopes[level, metric] = nominal_envelope, variation_envelope
        for level, directory, run in runs:
            (matcher, nominal_matcher, memory, nominal_envelope, variation_envelope,
             nominal_bounds, variation_bounds) = run
            print(f"Sampling {size}×{size}, {level}% state stdev...", flush=True)
            nominal, sample_x, sample_mv, sample_ns, selected_count = generate_samples(
                matcher, nominal_matcher, directory, int(memory["mcam"]["num_resistance_state"]),
                [*nominal_bounds, *variation_bounds], args.sample_compositions, level)
            metadata_path = directory / "metadata.json"
            metadata = json.loads(metadata_path.read_text())
            metadata.update({"nominal_composition_count": len(nominal),
                             "monte_carlo_composition_budget": args.sample_compositions,
                             "monte_carlo_selected_compositions": selected_count,
                             "monte_carlo_zero_variation_deduplicated": level == 0})
            metadata_path.write_text(json.dumps(metadata, indent=2) + "\n")
            label = f"resistance σ/R = {level}%, {memory['variation']['monte_carlo_granularity']}"
            for metric in metrics:
                sample_y = sample_mv if metric == "voltage" else sample_ns
                nominal_envelope, variation_envelope = envelopes[level, metric]
                validate_containment(sample_x, sample_y, variation_envelope)
                maximum_y = max(np.max(envelopes[other_level, metric][1][2]) for other_level in args.levels)
                grouped = group_nominal_for_metric(nominal, metric, maximum_y, directory)
                for active in (False, True):
                    maximum_distance = active_region_distance(nominal) if active else size * (int(memory["mcam"]["num_resistance_state"]) - 1)**2
                    plot_voltage(directory, size, label, nominal, grouped, sample_x, sample_y, nominal_envelope,
                                 variation_envelope, maximum_distance, maximum_y, variation_bounds,
                                 active=active, metric=metric)
            print(f"Wrote {directory} ({len(sample_x):,} samples; containment passed)", flush=True)
    print(f"Completed plots and raw data in {args.output_dir}")


if __name__ == "__main__":
    main()
