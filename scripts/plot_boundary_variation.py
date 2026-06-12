#!/usr/bin/env python3
"""Plot EvaCAM deterministic boundary variation corners as an SVG."""

import argparse
import csv
import os
from pathlib import Path


METRICS = [
    ("matchline_delay_s", "Matchline delay", "ps", 1e12, "max"),
    ("search_latency_s", "Search latency", "ps", 1e12, "max"),
    ("search_dynamic_energy_j", "Search dynamic energy", "pJ", 1e12, "max"),
    ("sense_margin_v", "Sense margin", "mV", 1e3, "min"),
]

INTERNAL_METRICS = [
    ("reference_delay_s", "Reference delay", "ps", 1e12, "max"),
]

CORNER_COLUMNS = [
    "corner_label",
    "matchline_wire_res_corner",
    "access_res_on_corner",
    "access_res_off_corner",
    "match_res_on_corner",
    "match_res_off_corner",
]


def read_rows(path):
    with path.open(newline="") as csv_file:
        rows = list(csv.DictReader(csv_file))
    if not rows:
        raise RuntimeError(f"No boundary sample rows found in {path}")
    missing = [column for column in CORNER_COLUMNS if column not in rows[0]]
    if missing:
        raise RuntimeError(
            "Boundary variation CSV is missing corner metadata columns: "
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


def available_metrics(rows, include_internal):
    metrics = METRICS + (INTERNAL_METRICS if include_internal else [])
    available = []
    for column, title, unit, scale, bound in metrics:
        points = metric_points(rows, column, scale)
        if points:
            available.append((column, title, unit, points, bound))
    if not available:
        raise RuntimeError("No known EvaCAM boundary metric columns found.")
    return available


def bound_point(points, bound):
    key = lambda point: point["value"]
    return max(points, key=key) if bound == "max" else min(points, key=key)


def format_value(value, unit):
    return f"{value:.3g} {unit}"


def compact_corner_label(label):
    replacements = {
        "nominal": "nom",
        "access_on": "acc_on",
        "access_off": "acc_off",
        "match_on": "mat_on",
        "match_off": "mat_off",
    }
    compact = label
    for source, target in replacements.items():
        compact = compact.replace(source, target)
    return compact


def format_bound_summary(title, unit, point, bound):
    return (
        f"{title} {bound}: {format_value(point['value'], unit)}\n"
        f"sample {point['sample']}: {compact_corner_label(point['corner_label'])}"
    )


def plot_boundary(rows, output_path, include_internal=False):
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

    fig.suptitle("EvaCAM Boundary Variation Corners", fontsize=15, fontweight="bold")
    fig.text(0.5, 0.945, f"{len(rows)} deterministic corners", ha="center", fontsize=10)

    summaries = []
    for index, (_column, title, unit, points, bound) in enumerate(metrics):
        ax = axes[index // cols][index % cols]
        sorted_points = sorted(points, key=lambda point: point["value"])
        values = [point["value"] for point in sorted_points]
        x_values = list(range(len(sorted_points)))
        risky = bound_point(sorted_points, bound)
        risky_index = sorted_points.index(risky)
        min_value = values[0]
        max_value = values[-1]

        ax.scatter(x_values, values, s=38, color="#4f7cac", edgecolor="white", linewidth=0.7, zorder=3)
        ax.axhline(min_value, color="#2da44e", linewidth=1.0, linestyle="--", alpha=0.85)
        ax.axhline(max_value, color="#cf222e", linewidth=1.0, linestyle="--", alpha=0.85)
        ax.scatter(
            [risky_index],
            [risky["value"]],
            s=76,
            color="#cf222e" if bound == "max" else "#2da44e",
            edgecolor="#24292f",
            linewidth=0.8,
            zorder=4,
        )
        ax.annotate(
            f"{bound}: sample {risky['sample']}",
            xy=(risky_index, risky["value"]),
            xytext=(8, 10 if bound == "max" else -18),
            textcoords="offset points",
            fontsize=8.5,
            arrowprops={"arrowstyle": "->", "color": "#57606a", "linewidth": 0.8},
        )

        ax.set_title(title, fontsize=11, fontweight="bold")
        ax.set_xlabel("Corners sorted by metric value")
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
            f"min: {format_value(min_value, unit)}\nmax: {format_value(max_value, unit)}",
            transform=ax.transAxes,
            ha="left",
            va="top",
            fontsize=8.5,
            bbox={
                "boxstyle": "round,pad=0.28",
                "facecolor": "white",
                "edgecolor": "#d0d7de",
                "alpha": 0.92,
            },
        )
        summaries.append(format_bound_summary(title, unit, risky, bound))

    for index in range(len(metrics), panel_rows * cols):
        axes[index // cols][index % cols].set_visible(False)

    fig.text(
        0.5,
        0.01,
        "\n\n".join(summaries),
        ha="center",
        va="bottom",
        fontsize=8.5,
        bbox={
            "boxstyle": "round,pad=0.45",
            "facecolor": "white",
            "edgecolor": "#d0d7de",
            "alpha": 0.95,
        },
    )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, format="svg")
    plt.close(fig)


def default_output_path(csv_path):
    stem = csv_path.stem
    if stem.endswith("_variation_samples"):
        stem = stem[: -len("_variation_samples")] + "_variation"
    return csv_path.with_name(stem + "_boundary.svg")


def main():
    parser = argparse.ArgumentParser(
        description="Plot deterministic boundary corners from an EvaCAM variation samples CSV."
    )
    parser.add_argument("csv_file", type=Path, help="EvaCAM boundary variation samples CSV")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Output SVG path. Defaults to <csv_stem>_boundary.svg next to the CSV.",
    )
    parser.add_argument(
        "--include-internal",
        action="store_true",
        help="Include internal diagnostic metrics such as reference delay.",
    )
    args = parser.parse_args()

    output_path = args.output or default_output_path(args.csv_file)
    rows = read_rows(args.csv_file)
    plot_boundary(rows, output_path, args.include_internal)
    print(f"Wrote {output_path}")


if __name__ == "__main__":
    main()
