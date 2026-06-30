#!/usr/bin/env python3
"""Plot EvaCAM Monte Carlo variation sample histograms as an SVG."""

import argparse
import csv
import math
from pathlib import Path


METRICS = [
    ("matchline_delay_s", "Matchline delay", "ps", 1e12),
    ("search_latency_s", "Search latency", "ps", 1e12),
    ("search_dynamic_energy_j", "Search dynamic energy", "pJ", 1e12),
    ("exact_match_sense_margin_v", "Exact Match Sense Margin", "V", 1.0),
]

INTERNAL_METRICS = [
    ("reference_delay_s", "Reference delay", "ns", 1e9),
]


def read_samples(path):
    with path.open(newline="") as csv_file:
        rows = list(csv.DictReader(csv_file))
    if not rows:
        raise RuntimeError(f"No sample rows found in {path}")
    return rows


def column_values(rows, column, scale):
    values = []
    for row in rows:
        value = row.get(column)
        if value in (None, ""):
            continue
        values.append(float(value) * scale)
    return values


def available_metrics(rows, include_internal):
    metrics = METRICS + (INTERNAL_METRICS if include_internal else [])
    available = []
    for column, title, unit, scale in metrics:
        values = column_values(rows, column, scale)
        if values:
            available.append((title, unit, values))
    if not available:
        raise RuntimeError("No known EvaCAM variation metric columns found.")
    return available


def metric_stats(values):
    mean = sum(values) / len(values)
    variance = sum((value - mean) ** 2 for value in values) / len(values)
    return mean, math.sqrt(variance)


def format_stat(value, unit):
    return f"{value:.3g} {unit}"


def plot_histograms(rows, output_path, bins, include_internal=False):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.ticker import MaxNLocator

    metrics = available_metrics(rows, include_internal)
    cols = 2
    panel_rows = (len(metrics) + cols - 1) // cols
    fig, axes = plt.subplots(
        panel_rows,
        cols,
        figsize=(10.6, 3.7 * panel_rows),
        squeeze=False,
        constrained_layout=True,
    )

    fig.suptitle("EvaCAM Monte Carlo Variation Histograms", fontsize=15, fontweight="bold")
    fig.text(0.5, 0.945, f"{len(rows)} samples", ha="center", fontsize=10)

    for index, (title, unit, values) in enumerate(metrics):
        ax = axes[index // cols][index % cols]
        mean, stddev = metric_stats(values)
        ax.hist(values, bins=bins, color="#4f7cac", edgecolor="white", linewidth=0.7)
        ax.set_title(title, loc="center", fontsize=11, fontweight="bold")
        ax.set_xlabel(unit)
        ax.set_ylabel("Samples")
        ax.text(
            0.98,
            0.94,
            f"mean: {format_stat(mean, unit)}\nstddev: {format_stat(stddev, unit)}",
            transform=ax.transAxes,
            ha="right",
            va="top",
            fontsize=8.5,
            bbox={
                "boxstyle": "round,pad=0.28",
                "facecolor": "white",
                "edgecolor": "#d0d7de",
                "alpha": 0.9,
            },
        )
        ax.xaxis.set_major_locator(MaxNLocator(nbins=6))
        ax.yaxis.set_major_locator(MaxNLocator(nbins=5, integer=True))
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)
        ax.grid(axis="y", color="#d8dee4", linewidth=0.8, alpha=0.8)
        ax.set_axisbelow(True)

    for index in range(len(metrics), panel_rows * cols):
        axes[index // cols][index % cols].set_visible(False)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, format="svg")
    plt.close(fig)


def default_output_path(csv_path):
    stem = csv_path.stem
    if stem.endswith("_variation_samples"):
        stem = stem[: -len("_variation_samples")] + "_variation"
    return csv_path.with_name(stem + "_histograms.svg")


def main():
    parser = argparse.ArgumentParser(
        description="Plot histograms from an EvaCAM *_variation_samples.csv file."
    )
    parser.add_argument("csv_file", type=Path, help="EvaCAM variation samples CSV")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Output SVG path. Defaults to <csv_stem>_histograms.svg next to the CSV.",
    )
    parser.add_argument("--bins", type=int, default=40, help="Histogram bin count")
    parser.add_argument(
        "--include-internal",
        action="store_true",
        help="Include internal diagnostic metrics such as reference delay.",
    )
    args = parser.parse_args()

    if args.bins <= 0:
        raise RuntimeError("--bins must be greater than 0")

    output_path = args.output or default_output_path(args.csv_file)
    samples = read_samples(args.csv_file)
    plot_histograms(samples, output_path, args.bins, args.include_internal)
    print(f"Wrote {output_path}")


if __name__ == "__main__":
    main()
