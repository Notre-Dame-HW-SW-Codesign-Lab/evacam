#!/usr/bin/env python3
"""Default MCAM plots: all compositions and trials with per-distance mean +/-3 SD.

Use plot_mcam_voltage.py --mode support-bounds for historical extrema plots.
"""

import argparse
import csv
import json
import itertools
import math
import os
from pathlib import Path
import re
import shutil
import sys

os.environ.setdefault("MPLCONFIGDIR", "/tmp/evacam-matplotlib")
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from plot_mcam_voltage import BLUE, GRAY, ORANGE, prepare_config
from mcam_composition_stream import enumerate_delta_counts

ROOT = Path(__file__).resolve().parents[1]


def completion_counts(dimensions, states=8):
    """Exact numbers of ordered zero-query vectors at each squared distance."""
    if dimensions < 1 or states < 2:
        raise ValueError("Conditional sampler requires positive dimensions and at least two states")
    maximum = dimensions * (states - 1) ** 2
    dtype = np.int64 if states ** dimensions <= np.iinfo(np.int64).max else object
    counts = np.zeros((dimensions + 1, maximum + 1), dtype=dtype)
    counts[0, 0] = 1
    for length in range(1, dimensions + 1):
        for symbol in range(states):
            step = symbol * symbol
            counts[length, step:] += counts[length - 1, :maximum + 1 - step]
    return counts


def sample_vectors(counts, distance, samples, rng, states=8):
    """Uniform ordered vectors conditional on D^2, NOT uniform compositions."""
    dimensions = counts.shape[0] - 1
    if samples < 1 or distance < 0 or distance >= counts.shape[1] or counts[-1, distance] == 0:
        raise ValueError("Require a reachable distance and positive sample count")
    remaining = np.full(samples, distance, dtype=np.int64)
    vectors = np.empty((samples, dimensions), dtype=np.int64)
    steps = np.arange(states, dtype=np.int64) ** 2
    for coordinate in range(dimensions):
        previous = remaining[:, None] - steps
        row = counts[dimensions - coordinate - 1]
        # Preserve exact integer counts in the export, using floating-point
        # probabilities when 32-cell completion counts exceed int64.
        if counts.dtype == object:
            row = row.astype(float)
        weights = row[np.maximum(previous, 0)]
        weights = np.where(previous >= 0, weights, 0)
        cumulative = np.cumsum(weights, axis=1)
        tickets = (rng.random(samples) * cumulative[:, -1]
                   if counts.dtype == object or np.issubdtype(counts.dtype, np.floating)
                   else rng.integers(0, cumulative[:, -1]))
        symbols = np.sum(tickets[:, None] >= cumulative, axis=1)
        vectors[:, coordinate] = symbols
        remaining -= symbols * symbols
    return vectors


def varied_conductances(vectors, resistance, fractions, rng):
    """Independent cell draws from the legacy +/-3 input-SD truncated Gaussian.

    The distribution matches VariationSampler, but NumPy uses a different random
    stream. No native seed-by-seed identity is claimed.
    """
    nominal = resistance[vectors]
    sigma = nominal * fractions[vectors]
    lower = np.maximum(nominal * 1e-12, nominal - 3 * sigma)
    upper = nominal + 3 * sigma
    sampled = rng.normal(nominal, sigma)
    for _ in range(256):
        outside = (sampled < lower) | (sampled > upper)
        if not np.any(outside):
            break
        sampled[outside] = rng.normal(nominal[outside], sigma[outside])
    sampled = np.clip(sampled, lower, upper)
    return np.sum(1.0 / sampled, axis=1)


def summarize(values):
    if len(values) < 2 or not np.all(np.isfinite(values)):
        raise ValueError("Statistics require at least two finite samples")
    mean = float(np.mean(values))
    sd = float(np.std(values, ddof=1))
    return {
        "samples": len(values), "mean_voltage_v": mean, "stddev_voltage_v": sd,
        "lower_3sigma_voltage_v": mean - 3 * sd,
        "upper_3sigma_voltage_v": mean + 3 * sd,
        "fraction_outside_3sigma": float(np.mean((values < mean - 3 * sd) | (values > mean + 3 * sd))),
        "q00135_voltage_v": float(np.quantile(values, 0.00135)),
        "q99865_voltage_v": float(np.quantile(values, 0.99865)),
    }


def resistance_ohms(value):
    if isinstance(value, (int, float)):
        return float(value)
    match = re.fullmatch(r"\s*([0-9.eE+\-]+)\s*(ohm|kohm|Mohm)\s*", str(value))
    if not match:
        raise ValueError(f"Unsupported resistance quantity: {value!r}")
    return float(match[1]) * {"ohm": 1, "kohm": 1e3, "Mohm": 1e6}[match[2]]


def fraction(value):
    return float(str(value).rstrip("%")) / (100 if str(value).endswith("%") else 1)


def generate_nominal(size, directory, seed, chunk_size=100_000):
    """Export every composition once, using bounded native voltage batches."""
    import evacam_py
    source = ROOT / "config/2FeFET_MCAM_variation/stdev00" / f"2FeFET_MCAM_{size}x{size}.config.yaml"
    actual, memory = prepare_config(source, directory / "inputs", nominal=True, seed=seed)
    matcher = evacam_py.EvaCAMMatch(str(actual))
    resistance = np.sort([resistance_ohms(value) for value in memory["mcam"]["resistance_state"]])[::-1]
    states = len(resistance)
    total = math.comb(size + states - 1, states - 1)
    dtype = np.dtype([("delta_counts", "u1", (states,)),
                      ("squared_distance", "u2"), ("voltage_v", "f8"),
                      ("nonzero_delta_kinds", "u1")])
    path = directory / "nominal_compositions.npy"
    points = np.lib.format.open_memmap(path, mode="w+", dtype=dtype, shape=(total,))
    compositions = enumerate_delta_counts(size, states)
    for start in range(0, total, chunk_size):
        counts = np.asarray(list(itertools.islice(compositions, chunk_size)), dtype=np.uint8)
        stop = start + len(counts)
        points["delta_counts"][start:stop] = counts
        points["squared_distance"][start:stop] = counts @ (np.arange(states) ** 2)
        points["voltage_v"][start:stop] = matcher.sense_mcam_conductances((counts @ (1 / resistance)).tolist())
        points["nonzero_delta_kinds"][start:stop] = np.count_nonzero(counts[:, 1:], axis=1)
        if start // chunk_size % 10 == 0 or stop == total:
            print(f"{size}x{size}: nominal compositions {stop:,}/{total:,}", flush=True)
    points.flush()
    return np.load(path, mmap_mode="r")


def plot_all_points(axis, nominal, distances, voltage_samples, limit, chunk_size=100_000, *, tv=False):
    """Draw each raw point, including coincident points, without grouping."""
    for start in range(0, len(distances), 20):
        x = distances[start:start + 20]
        visible = x <= limit
        values = voltage_samples[start:start + 20][visible]
        axis.scatter(np.repeat(x[visible], voltage_samples.shape[1]), values.ravel() * 1000,
                     s=0.3, color="#B43B16" if tv else ORANGE, alpha=0.2 if tv else 0.12, linewidths=0,
                     rasterized=True, zorder=4,
                     label="All varied trials" if start == 0 else None)
    artist = None
    for start in range(0, len(nominal), chunk_size):
        points = nominal[start:start + chunk_size]
        points = points[points["squared_distance"] <= limit]
        if not len(points):
            continue
        artist = axis.scatter(points["squared_distance"], points["voltage_v"] * 1000,
                              c=points["nonzero_delta_kinds"], cmap="viridis", vmin=0,
                              vmax=nominal.dtype["delta_counts"].shape[0] - 1,
                              s=5 if tv else 3, alpha=0.95 if tv else 0.7,
                              linewidths=0, rasterized=True, zorder=5,
                              label="All nominal compositions (unbinned)" if artist is None else None)
    return artist


def draw_plot(records, size, level, directory, nominal=None, voltage_samples=None, *, tv=False):
    text_color = "#262626" if tv else GRAY
    mean_color = "#092A50" if tv else BLUE
    maximum_distance = records[-1]["squared_euclidean_distance"]
    x = np.arange(maximum_distance + 1)
    mean = np.full(x.shape, np.nan, dtype=float)
    sd = np.full(x.shape, np.nan, dtype=float)
    for record in records:
        distance = record["squared_euclidean_distance"]
        mean[distance] = 1000 * record["mean_voltage_v"]
        sd[distance] = 1000 * record["stddev_voltage_v"]
    lower, upper = mean - 3 * sd, mean + 3 * sd
    near = np.flatnonzero(upper > mean[0] * 0.04)
    zoom = min(maximum_distance, max(12, int(near[-1]) + 2))
    maximum_voltage = float(np.nanmax(upper))
    with plt.rc_context({"font.family": "DejaVu Sans", "font.size": 12,
                         "axes.labelsize": 13, "legend.frameon": False,
                         "figure.facecolor": "white", "axes.facecolor": "white",
                         "savefig.facecolor": "white"}):
        for active, limit in ((False, maximum_distance), (True, zoom)):
            fig, axis = plt.subplots(figsize=(12.0, 7.0))
            fig.suptitle("MCAM Matchline Voltage by Squared Euclidean Distance",
                         fontsize=18, fontweight="bold", y=0.98)
            view = "active-region detail" if active else "full range"
            axis.set_title(f"{size}x{size} array - {level}% resistance variation - {view}",
                           color=text_color, fontsize=13 if tv else 12, pad=10)
            axis.axhspan(lower[0], upper[0], color="#C9C9C9" if tv else GRAY,
                         alpha=1 if tv else 0.10,
                         label=r"Exact match: mean $\pm 3\sigma$", linewidth=0, zorder=1)
            if tv:
                for boundary in (lower[0], upper[0]):
                    axis.axhline(boundary, color="#555555", linestyle="--", linewidth=1.2, zorder=1)
            visible = x <= limit
            axis.fill_between(x[visible], lower[visible], upper[visible],
                              facecolor="#E7AA37" if tv else ORANGE,
                              edgecolor="#A35E00" if tv else ORANGE,
                              alpha=1 if tv else 0.15, linewidth=0.9 if tv else 0, zorder=2,
                              label=r"Varied distribution: mean $\pm 3\sigma$")
            axis.fill_between(x[visible], (mean - sd)[visible], (mean + sd)[visible],
                              facecolor="#6FA6CE" if tv else BLUE,
                              edgecolor="#2873A0" if tv else BLUE,
                              alpha=1 if tv else 0.18, linewidth=0.8 if tv else 0, zorder=3,
                              label=r"Varied distribution: mean $\pm 1\sigma$")
            axis.plot(x[visible], mean[visible], color=mean_color, linewidth=3.2 if tv else 1.9,
                      label="Mean matchline voltage", zorder=6)
            if nominal is not None:
                distances = np.array([record["squared_euclidean_distance"] for record in records])
                artist = plot_all_points(axis, nominal, distances, voltage_samples, limit, tv=tv)
                colorbar = fig.colorbar(artist, ax=axis, pad=0.02, fraction=0.028)
                colorbar.set_label("Distinct nonzero deltas in composition")
            axis.set_xlabel(r"Squared Euclidean distance, $D^2$", fontsize=15 if tv else 13)
            axis.set_ylabel("Matchline (ML) voltage (mV)", fontsize=15 if tv else 13)
            axis.set_xlim(0, limit * 1.02)
            axis.set_ylim(min(-0.025 * maximum_voltage, float(np.nanmin(lower)) * 1.05),
                          1.1 * maximum_voltage)
            axis.xaxis.set_major_locator(matplotlib.ticker.MaxNLocator(nbins=9, integer=True))
            axis.spines[["top", "right"]].set_visible(False)
            axis.grid(True, color="#d9dee3", linewidth=0.7, alpha=0.8)
            axis.set_axisbelow(True)
            axis.legend(loc="upper right", fontsize=11 if tv else 9, markerscale=2,
                        frameon=tv, facecolor="white", edgecolor="white", framealpha=1)
            fig.text(0.5, 0.025, f"{records[0]['samples']:,} varied trials per reachable distance; "
                     "uniform ordered stored vectors. Bands use output voltage standard deviation.",
                     ha="center", fontsize=10 if tv else 9, color=text_color)
            fig.text(0.5, 0.002, "Query symbol 0; fixed nominal sensing instant. "
                     + ("Statistical bands are not extrema." if nominal is None else
                        "All points plotted without binning; statistical bands are not extrema."),
                     ha="center", fontsize=10 if tv else 9, color=text_color)
            fig.subplots_adjust(left=0.115, right=0.97, bottom=0.17, top=0.86)
            stem = "voltage_mean_3sigma_active_region" if active else "voltage_mean_3sigma"
            for extension in ("svg", "pdf", "png"):
                fig.savefig(directory / f"{stem}.{extension}", dpi=300, bbox_inches="tight")
                if tv and nominal is not None:
                    print(f"Wrote {directory / f'{stem}.{extension}'}", flush=True)
            plt.close(fig)


def render_presentation(source, directory, size, level, *, with_points=False):
    """Recompute exact extrema from saved inputs; optionally overlay saved points."""
    source = Path(source).resolve()
    directory = Path(directory)
    with (source / "distance_statistics.csv").open() as stream:
        integer_fields = {"squared_euclidean_distance", "possible_ordered_vectors", "samples"}
        records = [{key: int(value) if key in integer_fields else float(value)
                    for key, value in row.items()} for row in csv.DictReader(stream)]
    nominal, voltage_samples = None, None
    if with_points:
        source_metadata = json.loads((source / "metadata.json").read_text())
        nominal_path = (source / source_metadata["nominal_data"]).resolve()
        nominal = np.load(nominal_path, mmap_mode="r")
        with np.load(source / "voltage_samples.npz") as archive:
            voltage_samples = archive["voltage_v"]
            if not np.array_equal(archive["squared_distances"],
                                  [record["squared_euclidean_distance"] for record in records]):
                raise ValueError("Saved sample distances do not match the statistical records")
        if voltage_samples.shape != (len(records), records[0]["samples"]):
            raise ValueError("Saved trial count does not match the statistical records")
    from plot_cam_extrema import generate_extrema
    generate_extrema('mcam', size, level, directory, source / 'inputs/run.config.yaml',
                     nominal, voltage_samples)
    shutil.copyfile(source / "distance_statistics.csv", directory / "distance_statistics.csv")
    metadata = {
        "source_results": os.path.relpath(source, directory),
        "statistics": "source distance_statistics.csv copied byte-for-byte; no resampling or recomputation",
        "presentation": ("all unbinned nominal and sampled points with a diversity colorbar; " if with_points else
                         "no nominal or sampled points; no colorbar; ") +
                        "exact nominal extrema in blue; input +/-3-SD support extrema in gold; no sampled output-SD bands",
        "vector_dimensions": size, "resistance_stdev_percent": level,
    }
    if with_points:
        metadata.update({
            "nominal_data": os.path.relpath(nominal_path, directory),
            "voltage_samples": os.path.relpath(source / "voltage_samples.npz", directory),
            "nominal_composition_count": len(nominal),
            "varied_trial_count": int(voltage_samples.size),
            "point_rendering": "every original point; no binning, grouping, deduplication, or subsampling; larger higher-opacity nominal markers and darker orange trials; rasterized scatter in PDF/SVG",
        })
    (directory / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(f"Rendered presentation: {directory}", flush=True)


def generate(size, level, samples, seed, directory, nominal=None):
    sys.path.insert(0, str(ROOT))
    import evacam_py
    source = ROOT / "config/2FeFET_MCAM_variation" / f"stdev{level:02d}" / f"2FeFET_MCAM_{size}x{size}.config.yaml"
    actual, memory = prepare_config(source, directory / "inputs", samples=2, seed=seed, granularity="cell")
    if memory["mcam"].get("pair_resistance"):
        raise ValueError("These statistics intentionally use the legacy distance table, not a pair table")
    resistance = np.array([resistance_ohms(value) for value in memory["mcam"]["resistance_state"]])
    fractions = np.array([fraction(value) for value in memory["mcam"]["state_variation"]])
    order = np.argsort(-resistance, kind="stable")
    resistance, fractions = resistance[order], fractions[order]
    matcher = evacam_py.EvaCAMMatch(str(actual))
    counts = completion_counts(size, len(resistance))
    distances = np.flatnonzero(counts[-1])
    # At 64 cells the exact table uses Python integers. Convert it once for
    # conditional draws while retaining exact counts for the CSV export.
    sampling_counts = counts.astype(float) if size == 64 else counts
    records, voltage_samples = [], []
    for distance in distances:
        # Independent streams for vector composition and device noise. Fixed
        # streams across levels facilitate comparison without pooling samples.
        vectors_rng, noise_rng = [np.random.default_rng(child) for child in
                                 np.random.SeedSequence([seed, size, int(distance)]).spawn(2)]
        vectors = sample_vectors(sampling_counts, int(distance), samples, vectors_rng, len(resistance))
        conductances = varied_conductances(vectors, resistance, fractions, noise_rng)
        voltages = np.asarray(matcher.sense_mcam_conductances(conductances.tolist()))
        record = {"squared_euclidean_distance": int(distance),
                  "possible_ordered_vectors": int(counts[-1, distance]), **summarize(voltages)}
        records.append(record)
        voltage_samples.append(voltages)
        if len(records) % 100 == 0 or distance == distances[-1]:
            print(f"{size}x{size}, {level}%: sampled {len(records)}/{len(distances)} distances", flush=True)
    exact_lower = records[0]["lower_3sigma_voltage_v"]
    for record in records:
        record["exact_lower_minus_upper_3sigma_v"] = exact_lower - record["upper_3sigma_voltage_v"]
    with (directory / "distance_statistics.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)
    voltage_samples = np.stack(voltage_samples)
    np.savez_compressed(directory / "voltage_samples.npz", squared_distances=distances,
                        voltage_v=voltage_samples)
    metadata = {
        "model": "legacy sorted resistance indexed by abs(stored-query); no pair table",
        "source_config": str(source.relative_to(ROOT)), "vector_dimensions": size,
        "query": [0] * size, "samples_per_distance": samples, "seed": seed,
        "vector_sampling": "uniform ordered stored vectors conditional on squared distance; sampling with replacement",
        "completion_counts": "exact integers; sampling uses float64 probabilities when counts exceed int64",
        "variation_sampling": "independent per-cell truncated Gaussian resistance; same distribution as legacy VariationSampler, different NumPy random stream",
        "resistance_support": "[max(R*1e-12,R-3*R*fraction), R+3*R*fraction]",
        "resistance_ohms_by_delta": resistance.tolist(), "stdev_fraction_by_delta": fractions.tolist(),
        "voltage_conversion": "native McamSensedVoltage; unchanged legacy fixed nominal one-step-mismatch sensing instant",
        "statistics": "mean and sample standard deviation (ddof=1) of varied voltage trials at each distance; includes composition and device variation",
        "band": "unclipped mean +/-3 output standard deviations, not input resistance corners",
        "coverage": "not assumed Gaussian; measured outside-band fractions and empirical tail quantiles included in CSV",
        "quantile_warning": "0.135% tails have few samples; neither these quantiles nor +/-3 SD are certified error bounds",
        "raw_samples": "voltage_samples.npz: voltage_v[distance_index, trial_index]; squared_distances identifies rows",
        "exact_match_band_v": [records[0]["lower_3sigma_voltage_v"], records[0]["upper_3sigma_voltage_v"]],
        "distances_without_positive_3sigma_separation": [record["squared_euclidean_distance"] for record in records[1:]
                                                        if record["exact_lower_minus_upper_3sigma_v"] <= 0],
    }
    if nominal is not None:
        metadata.update({
            "nominal_composition_count": len(nominal),
            "nominal_data": f"../../shared_nominal/{size}x{size}/nominal_compositions.npy",
            "nominal_fields": "delta_counts, squared_distance, voltage_v, nonzero_delta_kinds; one row per composition",
            "point_rendering": "every nominal composition and every varied trial; no binning, grouping, deduplication, or subsampling; scatter layers rasterized in PDF/SVG",
        })
    (directory / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(f"{size}x{size}, {level}%: rendering full and active-region plots", flush=True)
    draw_plot(records, size, level, directory, nominal, voltage_samples)
    print(f"{size} cells, {level}% variation: {len(records)} distances x {samples} varied trials; "
          f"no positive 3-SD separation at D^2={metadata['distances_without_positive_3sigma_separation']}", flush=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--presentation-from", type=Path,
                        help="Render exact input extrema from saved input snapshots; no Monte Carlo for bands")
    parser.add_argument("--with-points", action="store_true",
                        help="With --presentation-from, overlay every saved nominal point and varied trial")
    parser.add_argument("--band-only", action="store_true",
                        help="Generate statistics without enumerating nominal compositions (required for 64x64)")
    parser.add_argument("--sizes", type=int, nargs="+", choices=(8, 16, 32, 64), default=[8, 16, 32])
    parser.add_argument("--levels", type=int, nargs="+", choices=(0, 5, 10), default=[0, 5, 10])
    parser.add_argument("--samples", type=int, default=10000)
    parser.add_argument("--seed", type=int, default=9876)
    args = parser.parse_args(argv)
    if args.with_points and not args.presentation_from:
        parser.error("--with-points requires --presentation-from")
    if args.band_only and args.presentation_from:
        parser.error("--band-only is for new statistics; --presentation-from already omits points")
    if 64 in args.sizes and not (args.band_only or args.presentation_from):
        parser.error("64x64 requires --band-only to avoid nominal composition enumeration")
    if args.samples < 2:
        parser.error("--samples must be at least two")
    if not 0 <= args.seed <= 2**32 - 1:
        parser.error("--seed must fit an unsigned 32-bit integer")
    if args.output_dir is None:
        args.output_dir = ROOT / ("results/mcam_distance_statistics_tv_all_points" if args.with_points else
                                  "results/mcam_distance_statistics_tv" if args.presentation_from
                                  else "results/mcam_distance_statistics_all_points")
    if args.presentation_from:
        for size in args.sizes:
            for level in args.levels:
                source = args.presentation_from / f"stdev{level:02d}" / f"{size}x{size}"
                if not (source / "distance_statistics.csv").is_file():
                    parser.error(f"Missing saved statistics: {source}")
    # Exclusive creation protects historical results and any earlier invocation.
    args.output_dir.mkdir(parents=True, exist_ok=False)
    if args.presentation_from:
        for size in args.sizes:
            for level in args.levels:
                relative = Path(f"stdev{level:02d}/{size}x{size}")
                render_presentation(args.presentation_from / relative, args.output_dir / relative, size, level,
                                    with_points=args.with_points)
        return
    sys.path.insert(0, str(ROOT))
    for size in args.sizes:
        nominal = None if args.band_only else generate_nominal(
            size, args.output_dir / "shared_nominal" / f"{size}x{size}", args.seed)
        for level in args.levels:
            directory = args.output_dir / f"stdev{level:02d}" / f"{size}x{size}"
            directory.mkdir(parents=True)
            generate(size, level, args.samples, args.seed, directory, nominal)


if __name__ == "__main__":
    main()
