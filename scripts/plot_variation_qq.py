#!/usr/bin/env python3
"""Plot normal Q-Q plots for EvaCAM Monte Carlo variation samples."""

import argparse
import csv
from pathlib import Path

from scipy.stats import probplot


METRICS = [
    ("matchline_delay_s", "Matchline delay", "ps", 1e12),
    ("search_latency_s", "Search latency", "ps", 1e12),
    ("search_dynamic_energy_j", "Search dynamic energy", "pJ", 1e12),
    ("exact_match_sense_margin_v", "Exact Match Sense Margin", "V", 1.0),
]


def read_samples(path):
    with path.open(newline="") as csv_file:
        rows = list(csv.DictReader(csv_file))
    if not rows:
        raise RuntimeError(f"No sample rows found in {path}")
    return rows


def column_values(rows, column, scale):
    return [
        float(row[column]) * scale
        for row in rows
        if row.get(column) not in (None, "")
    ]


def available_metrics(rows):
    available = []
    for column, title, unit, scale in METRICS:
        values = column_values(rows, column, scale)
        if values:
            available.append((title, unit, values))
    if not available:
        raise RuntimeError("No known EvaCAM variation metric columns found")
    return available


def plot_qq(rows, output_path):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    metrics = available_metrics(rows)
    cols = 2
    panel_rows = (len(metrics) + cols - 1) // cols
    fig, axes = plt.subplots(
        panel_rows,
        cols,
        figsize=(10.6, 3.7 * panel_rows),
        squeeze=False,
        constrained_layout=True,
    )

    fig.suptitle("EvaCAM Monte Carlo Variation Normal Q-Q Plots", fontsize=15, fontweight="bold")
    fig.text(0.5, 0.945, f"{len(rows)} samples", ha="center", fontsize=10)

    for index, (title, unit, values) in enumerate(metrics):
        ax = axes[index // cols][index % cols]
        (theoretical, ordered), (slope, intercept, correlation) = probplot(
            values, dist="norm"
        )
        ax.scatter(theoretical, ordered, s=12, color="#4f7cac", alpha=0.75)
        ax.plot(
            theoretical,
            slope * theoretical + intercept,
            color="#c44e52",
            linewidth=1.5,
        )
        ax.set_title(title, fontsize=11, fontweight="bold")
        ax.set_xlabel("Theoretical normal quantiles")
        ax.set_ylabel(f"Ordered values ({unit})")
        ax.text(
            0.03,
            0.95,
            f"R² = {correlation ** 2:.6f}",
            transform=ax.transAxes,
            ha="left",
            va="top",
            fontsize=8.5,
            bbox={
                "boxstyle": "round,pad=0.28",
                "facecolor": "white",
                "edgecolor": "#d0d7de",
                "alpha": 0.9,
            },
        )
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)
        ax.grid(color="#d8dee4", linewidth=0.8, alpha=0.8)
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
    return csv_path.with_name(stem + "_qq_plots.svg")


def main():
    parser = argparse.ArgumentParser(
        description="Plot normal Q-Q plots from an EvaCAM variation samples CSV."
    )
    parser.add_argument("csv_file", type=Path, help="EvaCAM variation samples CSV")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Output SVG path. Defaults to <csv_stem>_qq_plots.svg next to the CSV.",
    )
    args = parser.parse_args()

    samples = read_samples(args.csv_file)
    output_path = args.output or default_output_path(args.csv_file)
    plot_qq(samples, output_path)
    print(f"Wrote {output_path}")


if __name__ == "__main__":
    main()
