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


def corner_code(point):
    row = point["row"]
    on_corner = row.get("memory_device_res_on_corner", "")
    off_corner = row.get("memory_device_res_off_corner", "")
    abbreviations = {"low": "L", "high": "H", "nominal": "–"}
    if on_corner in abbreviations and off_corner in abbreviations:
        return abbreviations[on_corner] + abbreviations[off_corner]
    return point["corner_label"]


def ordered_points(points, order):
    if order == "sample":
        return sorted(points, key=lambda point: point["sample"])
    if order == "corner":
        corner_order = {"LL": 0, "LH": 1, "HL": 2, "HH": 3, "L–": 4, "H–": 5, "–L": 6, "–H": 7}
        return sorted(points, key=lambda point: (corner_order.get(corner_code(point), 99), point["sample"]))
    return sorted(points, key=lambda point: point["value"])


def format_value(value, unit):
    return f"{value:.3g} {unit}"


def plot_corner_variation(rows, output_path, include_internal=False, order="corner"):
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
        figsize=(14, 5.0 * panel_rows),
        squeeze=False,
        constrained_layout=True,
    )
    fig.suptitle(
        "EvaCAM Memory-Device Corner Analysis\n"
        f"{len(rows)} deterministic resistance corners  |  Corner code: R_on / R_off (L = low, H = high)",
        fontsize=16,
        fontweight="bold",
    )
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
        ax.scatter(
            [risky_x],
            [risky["value"]],
            s=76,
            color="#cf222e",
            edgecolor="#24292f",
            linewidth=0.8,
            label="Worst case",
            zorder=4,
        )
        ax.scatter(
            [best_x],
            [best["value"]],
            s=76,
            color="#1a7f37",
            edgecolor="#24292f",
            linewidth=0.8,
            label="Best case",
            zorder=4,
        )

        y_padding = max((max_value - min_value) * 0.18, abs(nominal) * 0.003)
        ax.set_ylim(min(min_value, nominal) - y_padding, max(max_value, nominal) + y_padding)
        ax.set_title(title, fontsize=14, fontweight="bold", pad=10)
        ax.set_xlabel("Corner")
        ax.set_ylabel(unit)
        ax.set_xticks(x_values, [corner_code(point) for point in plot_points])
        ax.tick_params(axis="x", labelsize=11, pad=5)
        ax.tick_params(axis="y", labelsize=10)
        ax.yaxis.set_major_locator(MaxNLocator(nbins=5))
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)
        ax.grid(axis="y", color="#d8dee4", linewidth=0.9, alpha=0.85)
        ax.set_axisbelow(True)
        ax.text(0.02, 0.95, f"Nominal: {format_value(nominal, unit)}", transform=ax.transAxes,
                ha="left", va="top", fontsize=10, fontweight="medium")
        handles, labels = ax.get_legend_handles_labels()
        if legend_handles is None:
            legend_handles = handles[:4]
            legend_labels = labels[:4]

    for index in range(len(metrics), panel_rows * cols):
        axes[index // cols][index % cols].set_visible(False)

    fig.legend(
        legend_handles,
        legend_labels,
        loc="upper center",
        bbox_to_anchor=(0.5, -0.01),
        ncol=4,
        fontsize=10,
        frameon=True,
        framealpha=0.95,
        edgecolor="#d0d7de",
    )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, format="svg", bbox_inches="tight")
    plt.close(fig)


def default_output_path(csv_path, order="corner"):
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
        choices=["corner", "metric", "sample"],
        default="corner",
        help="Plot in low/high corner order, metric-sorted order, or original sample-index order.",
    )
    args = parser.parse_args()

    output_path = args.output or default_output_path(args.csv_file, args.order)
    rows = read_rows(args.csv_file)
    plot_corner_variation(rows, output_path, args.include_internal, args.order)
    print(f"Wrote {output_path}")


if __name__ == "__main__":
    main()
