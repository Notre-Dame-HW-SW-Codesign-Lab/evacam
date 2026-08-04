#!/usr/bin/env python3
"""Run Shapiro-Wilk normality tests on EvaCAM variation samples."""

import argparse
import csv
from pathlib import Path

from scipy.stats import shapiro


METRICS = [
    ("matchline_delay_s", "Matchline delay"),
    ("search_latency_s", "Search latency"),
    ("search_dynamic_energy_j", "Search dynamic energy"),
    ("exact_match_sense_margin_v", "Exact match sense margin"),
]

INTERNAL_METRICS = [
    ("reference_delay_s", "Reference delay"),
]


def read_samples(path):
    with path.open(newline="") as csv_file:
        rows = list(csv.DictReader(csv_file))
    if not rows:
        raise RuntimeError(f"No sample rows found in {path}")
    return rows


def column_values(rows, column):
    return [
        float(row[column])
        for row in rows
        if row.get(column) not in (None, "")
    ]


def available_metrics(rows, include_internal=False):
    """Return known metric names together with their non-empty sample values."""
    metrics = METRICS + (INTERNAL_METRICS if include_internal else [])
    return [
        (column, title, column_values(rows, column))
        for column, title in metrics
        if column_values(rows, column)
    ]


def normality_decision(p_value, alpha):
    """Classify a Shapiro-Wilk p-value at the supplied significance level."""
    return "reject normality" if p_value < alpha else "do not reject normality"


def main():
    parser = argparse.ArgumentParser(
        description="Run Shapiro-Wilk tests on an EvaCAM variation sample CSV."
    )
    parser.add_argument("csv_file", type=Path, help="EvaCAM variation samples CSV")
    parser.add_argument(
        "--alpha",
        type=float,
        default=0.05,
        help="Significance level used for the decision (default: 0.05)",
    )
    parser.add_argument(
        "--include-internal",
        action="store_true",
        help="Include internal diagnostic metrics such as reference delay",
    )
    args = parser.parse_args()

    if not 0.0 < args.alpha < 1.0:
        raise RuntimeError("--alpha must be between 0 and 1")

    rows = read_samples(args.csv_file)
    tested = 0

    print(f"Shapiro-Wilk normality test (alpha={args.alpha:g})")
    for column, title, values in available_metrics(rows, args.include_internal):
        if len(values) < 3:
            raise RuntimeError(f"{column} requires at least 3 samples")

        statistic, p_value = shapiro(values)
        decision = normality_decision(p_value, args.alpha)
        print(
            f"{title:28} n={len(values):5d}  "
            f"W={statistic:.6f}  p={p_value:.6g}  {decision}"
        )
        tested += 1

    if tested == 0:
        raise RuntimeError("No known EvaCAM variation metric columns found")


if __name__ == "__main__":
    main()
