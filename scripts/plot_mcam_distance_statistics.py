#!/usr/bin/env python3
"""Legacy MCAM response: varied voltage distributions and per-distance mean +/-3 SD."""

import argparse
import csv
import json
import os
from pathlib import Path
import re
import sys

os.environ.setdefault("MPLCONFIGDIR", "/tmp/evacam-matplotlib")
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from plot_mcam_voltage import BLUE, GRAY, ORANGE, prepare_config

ROOT = Path(__file__).resolve().parents[1]


def completion_counts(dimensions, states=8):
    """Exact numbers of ordered zero-query vectors at each squared distance."""
    if dimensions < 1 or states < 2 or states ** dimensions > np.iinfo(np.int64).max:
        raise ValueError("Conditional sampler requires positive dimensions and counts fitting int64")
    maximum = dimensions * (states - 1) ** 2
    counts = np.zeros((dimensions + 1, maximum + 1), dtype=np.int64)
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
        weights = counts[dimensions - coordinate - 1, np.maximum(previous, 0)]
        weights = np.where(previous >= 0, weights, 0)
        cumulative = np.cumsum(weights, axis=1)
        tickets = rng.integers(0, cumulative[:, -1])
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


def draw_plot(records, size, level, directory):
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
            fig, axis = plt.subplots(figsize=(10.2, 6.0))
            fig.suptitle("MCAM Matchline Voltage by Squared Euclidean Distance",
                         fontsize=18, fontweight="bold", y=0.98)
            view = "active-region detail" if active else "full range"
            axis.set_title(f"{size}x{size} array - {level}% resistance variation - {view}",
                           color=GRAY, fontsize=12, pad=10)
            axis.axhspan(lower[0], upper[0], color=GRAY, alpha=0.10,
                         label=r"Exact match: mean $\pm 3\sigma$", linewidth=0, zorder=1)
            visible = x <= limit
            axis.fill_between(x[visible], lower[visible], upper[visible], color=ORANGE,
                              alpha=0.15, linewidth=0, zorder=2,
                              label=r"Varied distribution: mean $\pm 3\sigma$")
            axis.fill_between(x[visible], (mean - sd)[visible], (mean + sd)[visible],
                              color=BLUE, alpha=0.18, linewidth=0, zorder=3,
                              label=r"Varied distribution: mean $\pm 1\sigma$")
            axis.plot(x[visible], mean[visible], color=BLUE, linewidth=1.9,
                      label="Mean matchline voltage", zorder=4)
            axis.set_xlabel(r"Squared Euclidean distance, $D^2$")
            axis.set_ylabel("Matchline (ML) voltage (mV)")
            axis.set_xlim(0, limit * 1.02)
            axis.set_ylim(min(-0.025 * maximum_voltage, float(np.nanmin(lower)) * 1.05),
                          1.1 * maximum_voltage)
            axis.xaxis.set_major_locator(matplotlib.ticker.MaxNLocator(nbins=9, integer=True))
            axis.spines[["top", "right"]].set_visible(False)
            axis.grid(True, color="#d9dee3", linewidth=0.7, alpha=0.8)
            axis.set_axisbelow(True)
            axis.legend(loc="upper right", fontsize=10)
            fig.text(0.5, 0.025, f"{records[0]['samples']:,} varied trials per reachable distance; "
                     "uniform ordered stored vectors. Bands use output voltage standard deviation.",
                     ha="center", fontsize=9, color=GRAY)
            fig.text(0.5, 0.002, "Query symbol 0; fixed nominal sensing instant. "
                     "Statistical bands, not worst-case corners or guaranteed error bounds.",
                     ha="center", fontsize=9, color=GRAY)
            fig.subplots_adjust(left=0.115, right=0.97, bottom=0.17, top=0.86)
            stem = "voltage_mean_3sigma_active_region" if active else "voltage_mean_3sigma"
            for extension in ("svg", "pdf", "png"):
                fig.savefig(directory / f"{stem}.{extension}", dpi=300, bbox_inches="tight")
            plt.close(fig)


def generate(size, level, samples, seed, directory):
    sys.path.insert(0, str(ROOT))
    import evacam_py
    source = ROOT / "config/2FeFET_MCAM_variation" / f"stdev{level:02d}" / f"2FeFET_MCAM_{size}x{size}.config.yaml"
    actual, memory = prepare_config(source, directory / "inputs", samples=2, seed=seed, granularity="cell")
    if memory["mcam"].get("pair_resistance"):
        raise ValueError("These statistics intentionally use the legacy distance table, not a pair table")
    resistance = np.array([resistance_ohms(value) for value in memory["mcam"]["resistance_state"]])
    fractions = np.array([fraction(value) for value in memory["mcam"]["state_variation"]])
    if not np.any(fractions > 0):
        raise ValueError("This plot requires nonzero resistance variation")
    order = np.argsort(-resistance, kind="stable")
    resistance, fractions = resistance[order], fractions[order]
    matcher = evacam_py.EvaCAMMatch(str(actual))
    counts = completion_counts(size, len(resistance))
    distances = np.flatnonzero(counts[-1])
    records, voltage_samples = [], []
    for distance in distances:
        # Independent streams for vector composition and device noise. Fixed
        # streams across levels facilitate comparison without pooling samples.
        vectors_rng, noise_rng = [np.random.default_rng(child) for child in
                                 np.random.SeedSequence([seed, size, int(distance)]).spawn(2)]
        vectors = sample_vectors(counts, int(distance), samples, vectors_rng, len(resistance))
        conductances = varied_conductances(vectors, resistance, fractions, noise_rng)
        voltages = np.asarray(matcher.sense_mcam_conductances(conductances.tolist()))
        record = {"squared_euclidean_distance": int(distance),
                  "possible_ordered_vectors": int(counts[-1, distance]), **summarize(voltages)}
        records.append(record)
        voltage_samples.append(voltages)
    exact_lower = records[0]["lower_3sigma_voltage_v"]
    for record in records:
        record["exact_lower_minus_upper_3sigma_v"] = exact_lower - record["upper_3sigma_voltage_v"]
    with (directory / "distance_statistics.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)
    np.savez_compressed(directory / "voltage_samples.npz", squared_distances=distances,
                        voltage_v=np.stack(voltage_samples))
    metadata = {
        "model": "legacy sorted resistance indexed by abs(stored-query); no pair table",
        "source_config": str(source.relative_to(ROOT)), "vector_dimensions": size,
        "query": [0] * size, "samples_per_distance": samples, "seed": seed,
        "vector_sampling": "uniform ordered stored vectors conditional on squared distance; sampling with replacement",
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
    (directory / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    draw_plot(records, size, level, directory)
    print(f"{size} cells, {level}% variation: {len(records)} distances x {samples} varied trials; "
          f"no positive 3-SD separation at D^2={metadata['distances_without_positive_3sigma_separation']}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "results/mcam_legacy_distance_statistics")
    parser.add_argument("--sizes", type=int, nargs="+", choices=(8, 16), default=[8, 16])
    parser.add_argument("--levels", type=int, nargs="+", choices=(5, 10), default=[5, 10])
    parser.add_argument("--samples", type=int, default=10000)
    parser.add_argument("--seed", type=int, default=9876)
    args = parser.parse_args()
    if args.samples < 2:
        parser.error("--samples must be at least two")
    # Exclusive creation protects historical results and any earlier invocation.
    args.output_dir.mkdir(parents=True, exist_ok=False)
    for level in args.levels:
        for size in args.sizes:
            directory = args.output_dir / f"stdev{level:02d}" / f"{size}x{size}"
            directory.mkdir(parents=True)
            generate(size, level, args.samples, args.seed, directory)


if __name__ == "__main__":
    main()
