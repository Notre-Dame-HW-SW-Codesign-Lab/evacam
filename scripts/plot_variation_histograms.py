#!/usr/bin/env python3
"""Plot EvaCAM Monte Carlo variation sample histograms as an SVG."""

import argparse
import csv
import html
import math
from pathlib import Path


METRICS = [
    ("matchline_delay_s", "Matchline delay", "ps", 1e12),
    ("search_latency_s", "Search latency", "ps", 1e12),
    ("search_dynamic_energy_j", "Search dynamic energy", "pJ", 1e12),
    ("sense_margin_v", "Sense margin", "V", 1.0),
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


def histogram(values, bins):
    lo = min(values)
    hi = max(values)
    if math.isclose(lo, hi):
        pad = abs(lo) * 0.05 or 1.0
        lo -= pad
        hi += pad

    width = (hi - lo) / bins
    counts = [0] * bins
    for value in values:
        index = min(int((value - lo) / width), bins - 1)
        counts[index] += 1
    return lo, hi, counts


def fmt(value):
    if value == 0:
        return "0"
    if abs(value) >= 1000 or abs(value) < 0.01:
        return f"{value:.2e}"
    return f"{value:.3g}"


def svg_text(x, y, text, size=13, anchor="start", weight="normal"):
    return (
        f'<text x="{x}" y="{y}" font-size="{size}" text-anchor="{anchor}" '
        f'font-family="Arial, sans-serif" font-weight="{weight}">'
        f"{html.escape(text)}</text>"
    )


def panel_svg(x, y, width, height, title, unit, values, bins):
    margin_left = 74
    margin_right = 54
    margin_top = 52
    margin_bottom = 58
    plot_x = x + margin_left
    plot_y = y + margin_top
    plot_w = width - margin_left - margin_right
    plot_h = height - margin_top - margin_bottom

    lo, hi, counts = histogram(values, bins)
    max_count = max(counts) or 1
    bar_w = plot_w / bins

    parts = [
        svg_text(x + 18, y + 30, title, size=15, weight="bold"),
        f'<line x1="{plot_x}" y1="{plot_y + plot_h}" x2="{plot_x + plot_w}" y2="{plot_y + plot_h}" stroke="#24292f"/>',
        f'<line x1="{plot_x}" y1="{plot_y}" x2="{plot_x}" y2="{plot_y + plot_h}" stroke="#24292f"/>',
    ]

    for i, count in enumerate(counts):
        bar_h = plot_h * count / max_count
        bx = plot_x + i * bar_w + 1
        by = plot_y + plot_h - bar_h
        parts.append(
            f'<rect x="{bx:.2f}" y="{by:.2f}" width="{max(bar_w - 2, 1):.2f}" '
            f'height="{bar_h:.2f}" fill="#4f7cac"/>'
        )

    parts.extend([
        svg_text(plot_x, plot_y + plot_h + 22, fmt(lo), size=11, anchor="middle"),
        svg_text(plot_x + plot_w, plot_y + plot_h + 22, fmt(hi), size=11, anchor="middle"),
        svg_text(plot_x + plot_w / 2, y + height - 16, unit, size=12, anchor="middle"),
        svg_text(plot_x - 10, plot_y + 4, str(max_count), size=11, anchor="end"),
        svg_text(plot_x - 10, plot_y + plot_h + 4, "0", size=11, anchor="end"),
    ])
    return "\n".join(parts)


def build_svg(rows, bins, include_internal=False):
    available = []
    metrics = METRICS + (INTERNAL_METRICS if include_internal else [])
    for column, title, unit, scale in metrics:
        values = column_values(rows, column, scale)
        if values:
            available.append((title, unit, values))

    if not available:
        raise RuntimeError("No known EvaCAM variation metric columns found.")

    panel_w = 500
    panel_h = 320
    gap = 28
    page_margin = 36
    card_padding = 38
    header_h = 72
    cols = 2
    panel_rows = (len(available) + cols - 1) // cols
    grid_w = cols * panel_w + (cols - 1) * gap
    grid_h = panel_rows * panel_h + (panel_rows - 1) * gap
    card_w = grid_w + 2 * card_padding
    card_h = header_h + grid_h + 2 * card_padding
    width = card_w
    height = card_h
    card_x = 0
    card_y = 0
    grid_x = card_x + card_padding
    grid_y = card_y + card_padding + header_h

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        f'<rect x="0" y="0" width="{width}" height="{height}" fill="#ffffff"/>',
        svg_text(width / 2, card_y + 34, "EvaCAM Monte Carlo Variation Histograms", size=20, anchor="middle", weight="bold"),
        svg_text(width / 2, card_y + 58, f"{len(rows)} samples", size=13, anchor="middle"),
    ]

    for index, (title, unit, values) in enumerate(available):
        col = index % cols
        row = index // cols
        x = grid_x + col * (panel_w + gap)
        y = grid_y + row * (panel_h + gap)
        parts.append(panel_svg(x, y, panel_w, panel_h, title, unit, values, bins))

    parts.append("</svg>")
    return "\n".join(parts) + "\n"


def default_output_path(csv_path):
    return csv_path.with_name(csv_path.stem + "_histograms.svg")


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
    output_path.write_text(build_svg(samples, args.bins, args.include_internal))
    print(f"Wrote {output_path}")


if __name__ == "__main__":
    main()
