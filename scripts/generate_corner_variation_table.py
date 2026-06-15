#!/usr/bin/env python3
"""Generate a human-readable EvaCAM corner variation table."""

import argparse
import csv
from pathlib import Path


METRICS = [
    ("matchline_delay_s", "matchline delay", "ps", 1e12, "max"),
    ("search_latency_s", "search latency", "ps", 1e12, "max"),
    ("search_dynamic_energy_j", "search dynamic energy", "pJ", 1e12, "max"),
    ("sense_margin_v", "sense margin", "mV", 1e3, "min"),
]

INTERNAL_METRICS = [
    ("reference_delay_s", "reference delay", "ps", 1e12, "max"),
]

CORNER_COLUMNS = [
    "corner_label",
    "matchline_wire_res_corner",
    "access_res_on_corner",
    "access_res_off_corner",
    "match_res_on_corner",
    "match_res_off_corner",
]

TABLE_COLUMNS = [
    ("sample", "sample"),
    ("worst_case_for", "worst case for"),
    ("matchline_wire_res_corner", "ml"),
    ("access_res_on_corner", "access on"),
    ("access_res_off_corner", "access off"),
    ("match_res_on_corner", "match on"),
    ("match_res_off_corner", "match off"),
]


def sample_key(row):
    return int(row.get("sample", 0))


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


def available_metrics(rows, include_internal):
    metrics = METRICS + (INTERNAL_METRICS if include_internal else [])
    return [
        metric
        for metric in metrics
        if any(row.get(metric[0]) not in (None, "") for row in rows)
    ]


def metric_value(row, column, scale):
    value = row.get(column)
    return None if value in (None, "") else float(value) * scale


def bound_row(rows, column, scale, bound):
    rows_with_values = [
        row for row in rows if metric_value(row, column, scale) is not None
    ]
    if not rows_with_values:
        return None
    key = lambda row: metric_value(row, column, scale)
    return max(rows_with_values, key=key) if bound == "max" else min(rows_with_values, key=key)


def sorted_metric_rows(rows, column, scale):
    rows_with_values = [
        row for row in rows if metric_value(row, column, scale) is not None
    ]
    return sorted(rows_with_values, key=lambda row: (metric_value(row, column, scale), sample_key(row)))


def metric_ranks(rows, metrics):
    ranks = {}
    for column, _title, _unit, scale, _bound in metrics:
        for rank, row in enumerate(sorted_metric_rows(rows, column, scale)):
            ranks.setdefault(row.get("sample", ""), {})[column] = rank
    return ranks


def format_value(value):
    return "" if value is None else f"{value:.4g}"


def format_changed(previous_row, row):
    if previous_row is None:
        return "start"
    changes = []
    for column in CORNER_COLUMNS[1:]:
        previous = previous_row.get(column, "")
        current = row.get(column, "")
        if previous != current:
            changes.append(f"{column_name(column)} {previous}->{current}")
    return ", ".join(changes) if changes else "metric value changed"


def column_name(column):
    names = {
        "matchline_wire_res_corner": "ml",
        "access_res_on_corner": "access on",
        "access_res_off_corner": "access off",
        "match_res_on_corner": "match on",
        "match_res_off_corner": "match off",
    }
    return names.get(column, column)


def markdown_escape(value):
    return str(value).replace("|", "\\|")


def markdown_table(headers, rows):
    lines = [
        "| " + " | ".join(markdown_escape(header) for header in headers) + " |",
        "| " + " | ".join("---" for _header in headers) + " |",
    ]
    for row in rows:
        lines.append("| " + " | ".join(markdown_escape(value) for value in row) + " |")
    return "\n".join(lines) + "\n"


def build_table(rows, include_internal=False):
    metrics = available_metrics(rows, include_internal)
    ranks = metric_ranks(rows, metrics)
    worst_by_sample = {}
    for column, title, _unit, scale, bound in metrics:
        row = bound_row(rows, column, scale, bound)
        if row is None:
            continue
        sample = row.get("sample", "")
        worst_by_sample.setdefault(sample, []).append(f"{title} ({bound})")

    headers = [header for _column, header in TABLE_COLUMNS]
    headers.extend(f"{title} plot index" for column, title, _unit, _scale, _bound in metrics)
    headers.extend(f"{title} ({unit})" for column, title, unit, _scale, _bound in metrics)

    table_rows = []
    for row in sorted(rows, key=sample_key):
        table_row = []
        for column, _header in TABLE_COLUMNS:
            if column == "worst_case_for":
                value = ", ".join(worst_by_sample.get(row.get("sample", ""), []))
            else:
                value = row.get(column, "")
            table_row.append(value)
        for column, _title, _unit, _scale, _bound in metrics:
            table_row.append(ranks.get(row.get("sample", ""), {}).get(column, ""))
        for column, _title, _unit, scale, _bound in metrics:
            table_row.append(format_value(metric_value(row, column, scale)))
        table_rows.append(table_row)

    sections = [
        "# Corner Variation Table\n",
        "## Samples\n",
        markdown_table(headers, table_rows),
    ]

    for column, title, unit, scale, bound in metrics:
        sections.append(f"## {title.title()} Sorted By Plot Index\n")
        sorted_rows = sorted_metric_rows(rows, column, scale)
        metric_headers = [
            "plot index",
            "sample",
            f"{title} ({unit})",
            "changed from previous index",
            "ml",
            "access on",
            "access off",
            "match on",
            "match off",
            "worst case",
        ]
        metric_table_rows = []
        previous_row = None
        worst_row = bound_row(rows, column, scale, bound)
        worst_sample = worst_row.get("sample", "") if worst_row else ""
        for rank, row in enumerate(sorted_rows):
            metric_table_rows.append(
                [
                    rank,
                    row.get("sample", ""),
                    format_value(metric_value(row, column, scale)),
                    format_changed(previous_row, row),
                    row.get("matchline_wire_res_corner", ""),
                    row.get("access_res_on_corner", ""),
                    row.get("access_res_off_corner", ""),
                    row.get("match_res_on_corner", ""),
                    row.get("match_res_off_corner", ""),
                    "yes" if row.get("sample", "") == worst_sample else "",
                ]
            )
            previous_row = row
        sections.append(markdown_table(metric_headers, metric_table_rows))

    return "\n".join(sections)


def default_output_path(csv_path):
    stem = csv_path.stem
    if stem.endswith("_variation_samples"):
        stem = stem[: -len("_variation_samples")] + "_variation"
    return csv_path.with_name(stem + "_corner_table.md")


def main():
    parser = argparse.ArgumentParser(
        description="Generate a Markdown table from an EvaCAM corner variation samples CSV."
    )
    parser.add_argument("csv_file", type=Path, help="EvaCAM corner variation samples CSV")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Output Markdown path. Defaults to <csv_stem>_corner_table.md next to the CSV.",
    )
    parser.add_argument(
        "--include-internal",
        action="store_true",
        help="Include internal diagnostic metrics such as reference delay.",
    )
    args = parser.parse_args()

    output_path = args.output or default_output_path(args.csv_file)
    rows = read_rows(args.csv_file)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(build_table(rows, args.include_internal))
    print(f"Wrote {output_path}")


if __name__ == "__main__":
    main()
