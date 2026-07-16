#!/usr/bin/env python3
"""Create concise, poster-ready EvaCAM variation figures from sample CSV files.

The generated SVGs use only headline metrics and put the nominal result and
worst-case/uncertainty information directly in the figure.  They complement,
rather than replace, the diagnostic histogram and corner plots.
"""

import argparse
import csv
import math
from pathlib import Path


METRICS = (
    ("search_latency_s", "Search latency", "ps", 1e12, "max"),
    ("search_dynamic_energy_j", "Search dynamic energy", "pJ", 1e12, "max"),
    ("exact_match_sense_margin_v", "Sense margin", "mV", 1e3, "min"),
)

COLORS = {
    "background": "#0c2340",
    "ink": "#c99700",
    "sample": "#c99700",
    "nominal": "#f2cb55",
    "worst": "#0a843d",
    "grid": "#31516f",
    "interval": "#50728f",
}


def read_rows(path):
    with path.open(newline="") as input_file:
        rows = list(csv.DictReader(input_file))
    if not rows:
        raise RuntimeError(f"No data rows found in {path}")
    return rows


def values(rows, column, scale):
    result = []
    for row in rows:
        raw = row.get(column)
        if raw not in (None, ""):
            result.append(float(raw) * scale)
    if not result:
        raise RuntimeError(f"No values found for {column}")
    return result


def nominal(rows, column, scale):
    key = f"nominal_{column}"
    for row in rows:
        raw = row.get(key)
        if raw not in (None, ""):
            return float(raw) * scale
    raise RuntimeError(f"Missing nominal column {key}")


def metric_rows(rows):
    return [metric for metric in METRICS if any(row.get(metric[0]) for row in rows)]


def percent_delta(value, baseline):
    return 100.0 * (value - baseline) / abs(baseline) if baseline else math.nan


def style_matplotlib():
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    plt.rcParams.update({
        "font.family": "DejaVu Sans",
        "font.size": 13,
        "axes.titleweight": "bold",
        "figure.facecolor": COLORS["background"],
        "axes.facecolor": COLORS["background"],
        "savefig.facecolor": COLORS["background"],
        "text.color": COLORS["ink"],
        "axes.titlecolor": COLORS["ink"],
        "axes.labelcolor": COLORS["ink"],
        "axes.edgecolor": COLORS["ink"],
        "xtick.color": COLORS["ink"],
        "ytick.color": COLORS["ink"],
        "legend.labelcolor": COLORS["ink"],
    })
    return plt


def clean_axis(axis):
    axis.spines["top"].set_visible(False)
    axis.spines["right"].set_visible(False)
    axis.spines["left"].set_color(COLORS["ink"])
    axis.spines["bottom"].set_color(COLORS["ink"])
    axis.grid(axis="y", color=COLORS["grid"], linewidth=0.8)
    axis.set_axisbelow(True)


def save_figure(fig, output_path, png_dpi):
    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, format="svg")
    fig.savefig(output_path.with_suffix(".png"), format="png", dpi=png_dpi)


def plot_monte_carlo(rows, output_path, png_dpi):
    """Plot distributions for headline metrics with nominal and 5--95% range."""
    plt = style_matplotlib()
    metrics = metric_rows(rows)
    fig, axes = plt.subplots(1, len(metrics), figsize=(5.2 * len(metrics), 4.7), squeeze=False)
    fig.subplots_adjust(left=0.07, right=0.98, bottom=0.20, top=0.76, wspace=0.30)
    fig.suptitle("Monte Carlo Variation", y=0.97, fontsize=20, fontweight="bold")
    fig.text(0.5, 0.85, f"{len(rows):,} samples  ·  shaded region: central 90%", ha="center", fontsize=12)

    for axis, (column, title, unit, scale, direction) in zip(axes[0], metrics):
        data = values(rows, column, scale)
        reference = nominal(rows, column, scale)
        low, high = sorted(data)[round(0.05 * (len(data) - 1))], sorted(data)[round(0.95 * (len(data) - 1))]
        adverse = max(data) if direction == "max" else min(data)
        bins = min(24, max(10, round(math.sqrt(len(data)))))
        axis.hist(data, bins=bins, color=COLORS["sample"], edgecolor="white", linewidth=0.8)
        axis.axvspan(low, high, color=COLORS["interval"], alpha=0.45, zorder=0)
        axis.axvline(reference, color=COLORS["nominal"], linewidth=2.2, label="Nominal")
        axis.axvline(adverse, color=COLORS["worst"], linewidth=2.2, linestyle="--", label="Worst sample")
        axis.set_title(title, fontsize=16, pad=12)
        axis.set_xlabel(unit)
        axis.set_ylabel("Samples" if axis is axes[0][0] else "")
        clean_axis(axis)

    handles, labels = axes[0][-1].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center", bbox_to_anchor=(0.5, 0.02), ncol=2, frameon=False, fontsize=11)
    save_figure(fig, output_path, png_dpi)
    plt.close(fig)


def corner_label(row):
    abbreviations = {"low": "L", "high": "H", "nominal": "–"}
    return abbreviations.get(row.get("memory_device_res_on_corner", ""), "?") + abbreviations.get(row.get("memory_device_res_off_corner", ""), "?")


def plot_corner(rows, output_path, png_dpi):
    """Plot physical resistance corners, ordered as LL, LH, HL, HH."""
    required = {"memory_device_res_on_corner", "memory_device_res_off_corner"}
    missing = required - set(rows[0])
    if missing:
        raise RuntimeError("Corner CSV is missing: " + ", ".join(sorted(missing)))

    plt = style_matplotlib()
    order = {"LL": 0, "LH": 1, "HL": 2, "HH": 3, "L–": 4, "H–": 5, "–L": 6, "–H": 7}
    rows = sorted(rows, key=lambda row: order.get(corner_label(row), 99))
    labels = [corner_label(row) for row in rows]
    metrics = metric_rows(rows)
    fig, axes = plt.subplots(1, len(metrics), figsize=(5.2 * len(metrics), 4.7), squeeze=False)
    fig.subplots_adjust(left=0.07, right=0.98, bottom=0.22, top=0.76, wspace=0.30)
    fig.suptitle("Corner Analysis", y=0.97, fontsize=20, fontweight="bold")
    fig.text(0.5, 0.85, "Corner code: R_on / R_off  (L = low resistance, H = high resistance)", ha="center", fontsize=12)

    for axis, (column, title, unit, scale, direction) in zip(axes[0], metrics):
        raw_data = [float(row[column]) * scale for row in rows]
        reference = nominal(rows, column, scale)
        data = [percent_delta(value, reference) for value in raw_data]
        adverse_index = data.index(max(data) if direction == "max" else min(data))
        colors = [COLORS["sample"]] * len(data)
        colors[adverse_index] = COLORS["worst"]
        axis.bar(range(len(data)), data, color=colors, width=0.7)
        axis.axhline(0.0, color=COLORS["nominal"], linewidth=2.0, label="Nominal")
        axis.set_title(title, fontsize=16, pad=12)
        axis.set_xticks(range(len(data)), labels)
        axis.set_xlabel("Resistance corner")
        axis.set_ylabel("Change from nominal (%)" if axis is axes[0][0] else "")
        adverse = data[adverse_index]
        lower = min(min(data), 0.0)
        upper = max(max(data), 0.0)
        padding = max((upper - lower) * 0.28, 0.12)
        axis.set_ylim(lower - padding, upper + padding)
        axis.annotate(f"{labels[adverse_index]}\n{adverse:+.2f}%", (adverse_index, adverse), xytext=(0, 8 if direction == "max" else -28), textcoords="offset points", ha="center", fontsize=11, fontweight="bold", color=COLORS["worst"])
        clean_axis(axis)

    handles, labels = axes[0][-1].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center", bbox_to_anchor=(0.5, 0.02), frameon=False, fontsize=11)
    save_figure(fig, output_path, png_dpi)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description="Create poster-ready EvaCAM Monte Carlo and corner figures.")
    parser.add_argument("--monte-carlo", type=Path, required=True, help="Monte Carlo variation samples CSV")
    parser.add_argument("--corner", type=Path, required=True, help="Corner variation samples CSV")
    parser.add_argument("--output-dir", type=Path, default=Path("results/poster_figures"), help="Directory for SVG outputs")
    parser.add_argument("--png-dpi", type=int, default=1200, help="PNG export resolution (default: 1200 DPI)")
    args = parser.parse_args()
    if args.png_dpi <= 0:
        raise RuntimeError("--png-dpi must be greater than zero")

    monte_carlo_path = args.output_dir / "monte_carlo_headline_metrics.svg"
    corner_path = args.output_dir / "corner_physical_extremes.svg"
    plot_monte_carlo(read_rows(args.monte_carlo), monte_carlo_path, args.png_dpi)
    plot_corner(read_rows(args.corner), corner_path, args.png_dpi)
    print(f"Wrote {monte_carlo_path}")
    print(f"Wrote {monte_carlo_path.with_suffix('.png')} ({args.png_dpi} DPI)")
    print(f"Wrote {corner_path}")
    print(f"Wrote {corner_path.with_suffix('.png')} ({args.png_dpi} DPI)")


if __name__ == "__main__":
    main()
