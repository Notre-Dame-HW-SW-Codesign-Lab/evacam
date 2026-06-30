#!/usr/bin/env python3
"""Plot EvaCAM deterministic corner variation samples as an SVG."""

import argparse
import csv
import os
from pathlib import Path


METRICS = [
    ("matchline_delay_s", "Matchline delay", "ps", 1e12, "max"),
    ("search_latency_s", "Search latency", "ps", 1e12, "max"),
    ("search_dynamic_energy_j", "Search dynamic energy", "pJ", 1e12, "max"),
    ("exact_match_sense_margin_v", "Exact Match Sense Margin", "mV", 1e3, "min"),
]

INTERNAL_METRICS = [
    ("reference_delay_s", "Reference delay", "ps", 1e12, "max"),
]

CORNER_COLUMNS = [
    "corner_label",
    "memory_device_res_on_corner",
    "memory_device_res_off_corner",
]


def read_rows(path):
    with path.open(newline="") as csv_file:
        rows = list(csv.DictReader(csv_file))
    if not rows:
        raise RuntimeError(f"No corner sample rows found in {path}")
    missing = [column for column in CORNER_COLUMNS if column not in rows[0]]
    if missing:
        raise RuntimeError(
            "Corner variation CSV is missing corner metadata columns: "
            + ", ".join(missing)
        )
    return rows


def metric_points(rows, column, scale):
    points = []
    for row in rows:
        value = row.get(column)
        if value in (None, ""):
            continue
        points.append(
            {
                "sample": int(row.get("sample", len(points))),
                "value": float(value) * scale,
                "corner_label": row.get("corner_label", ""),
                "row": row,
            }
        )
    return points


def nominal_value(rows, column, scale):
    nominal_column = f"nominal_{column}"
    for row in rows:
        value = row.get(nominal_column)
        if value not in (None, ""):
            return float(value) * scale
    raise RuntimeError(
        f"Corner variation CSV is missing nominal metric column: {nominal_column}"
    )


def available_metrics(rows, include_internal):
    metrics = METRICS + (INTERNAL_METRICS if include_internal else [])
    available = []
    for column, title, unit, scale, bound in metrics:
        points = metric_points(rows, column, scale)
        if points:
            available.append((column, title, unit, scale, points, bound))
    if not available:
        raise RuntimeError("No known EvaCAM corner metric columns found.")
    return available


def bound_point(points, bound):
    key = lambda point: point["value"]
    return max(points, key=key) if bound == "max" else min(points, key=key)


def opposite_bound(bound):
    return "min" if bound == "max" else "max"


def ordered_points(points, order):
    if order == "sample":
        return sorted(points, key=lambda point: point["sample"])
    return sorted(points, key=lambda point: point["value"])


def format_value(value, unit):
    return f"{value:.3g} {unit}"


def plot_corner_variation(rows, output_path, include_internal=False, order="metric"):
    os.environ.setdefault("MPLCONFIGDIR", "/tmp/evacam-matplotlib")

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
        figsize=(11.4, 4.1 * panel_rows),
        squeeze=False,
        constrained_layout=True,
    )

    fig.suptitle("EvaCAM Corner Variation", fontsize=14, fontweight="bold")
    subtitle = f"{len(rows)} deterministic corners"
    if order == "sample":
        subtitle += ", original sample order"
    else:
        subtitle += ", sorted by metric value"
    fig.text(0.5, 0.945, subtitle, ha="center", fontsize=10)
    legend_handles = None
    legend_labels = None

    for index, (column, title, unit, scale, points, bound) in enumerate(metrics):
        ax = axes[index // cols][index % cols]
        plot_points = ordered_points(points, order)
        values = [point["value"] for point in plot_points]
        x_values = (
            [point["sample"] for point in plot_points]
            if order == "sample"
            else list(range(len(plot_points)))
        )
        risky = bound_point(plot_points, bound)
        risky_x = risky["sample"] if order == "sample" else plot_points.index(risky)
        best_bound = opposite_bound(bound)
        best = bound_point(plot_points, best_bound)
        best_x = best["sample"] if order == "sample" else plot_points.index(best)
        nominal = nominal_value(rows, column, scale)
        min_value = min(values)
        max_value = max(values)

        ax.scatter(
            x_values,
            values,
            s=34,
            color="#4f7cac",
            edgecolor="white",
            linewidth=0.7,
            label="Corner",
            zorder=3,
        )
        ax.axhline(
            nominal,
            color="#8250df",
            linewidth=1.2,
            linestyle="--",
            alpha=0.95,
            label="Nominal",
            zorder=2,
        )
        ax.axhline(
            min_value,
            color="#8c959f",
            linewidth=1.0,
            linestyle="--",
            alpha=0.9,
            label="Min/max range",
        )
        ax.axhline(max_value, color="#8c959f", linewidth=1.0, linestyle="--", alpha=0.9)
        ax.scatter(
            [risky_x],
            [risky["value"]],
            s=76,
            color="#cf222e",
            edgecolor="#24292f",
            linewidth=0.8,
            label=f"Worst case ({bound})",
            zorder=4,
        )
        ax.scatter(
            [best_x],
            [best["value"]],
            s=76,
            color="#1a7f37",
            edgecolor="#24292f",
            linewidth=0.8,
            label=f"Best case ({best_bound})",
            zorder=4,
        )

        ax.set_title(title, fontsize=11.5, fontweight="bold", pad=7)
        ax.set_xlabel("Sample Index" if order == "sample" else "Sorted Corner Index")
        ax.set_ylabel(unit)
        ax.xaxis.set_major_locator(MaxNLocator(nbins=6, integer=True))
        ax.yaxis.set_major_locator(MaxNLocator(nbins=6))
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)
        ax.grid(axis="y", color="#d8dee4", linewidth=0.8, alpha=0.8)
        ax.set_axisbelow(True)
        ax.text(
            0.02,
            0.96,
            f"nominal {format_value(nominal, unit)}\n"
            f"min {format_value(min_value, unit)}\n"
            f"max {format_value(max_value, unit)}",
            transform=ax.transAxes,
            ha="left",
            va="top",
            fontsize=8.0,
            bbox={
                "boxstyle": "round,pad=0.28",
                "facecolor": "white",
                "edgecolor": "#d0d7de",
                "alpha": 0.92,
            },
        )
        handles, labels = ax.get_legend_handles_labels()
        if legend_handles is None:
            legend_handles = handles[:5]
            legend_labels = labels[:5]

    for index in range(len(metrics), panel_rows * cols):
        axes[index // cols][index % cols].set_visible(False)

    fig.legend(
        legend_handles,
        legend_labels,
        loc="upper center",
        bbox_to_anchor=(0.5, -0.01),
        ncol=5,
        fontsize=8.0,
        frameon=True,
        framealpha=0.95,
        edgecolor="#d0d7de",
    )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, format="svg", bbox_inches="tight")
    plt.close(fig)


def default_output_path(csv_path, order="metric"):
    stem = csv_path.stem
    if stem.endswith("_variation_samples"):
        stem = stem[: -len("_variation_samples")] + "_variation"
    if order == "sample":
        stem += "_sample_order"
    return csv_path.with_name(stem + "_corner.svg")


def main():
    parser = argparse.ArgumentParser(
        description="Plot deterministic corner samples from an EvaCAM variation samples CSV."
    )
    parser.add_argument("csv_file", type=Path, help="EvaCAM corner variation samples CSV")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Output SVG path. Defaults to <csv_stem>_corner.svg next to the CSV.",
    )
    parser.add_argument(
        "--include-internal",
        action="store_true",
        help="Include internal diagnostic metrics such as reference delay.",
    )
    parser.add_argument(
        "--order",
        choices=["metric", "sample"],
        default="metric",
        help="Plot corners sorted by each metric value or in original sample-index order.",
    )
    args = parser.parse_args()

    output_path = args.output or default_output_path(args.csv_file, args.order)
    rows = read_rows(args.csv_file)
    plot_corner_variation(rows, output_path, args.include_internal, args.order)
    print(f"Wrote {output_path}")


if __name__ == "__main__":
    main()
