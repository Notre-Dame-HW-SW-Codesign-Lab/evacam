#!/usr/bin/env python3
"""Analyze aggregate results from scripts/run_corner_sweep.py."""

import argparse
import csv
import math
import os
import statistics
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SWEEP_DIR = ROOT / "results" / "corner_sweep"
PARAMETERS = (
    ("on_var_percent", "Memory on"),
    ("off_var_percent", "Memory off"),
)
DERIVED_METRICS = (
    ("matchline_delay_range_pct", "Matchline delay range", "%", "max"),
    ("search_latency_range_pct", "Search latency range", "%", "max"),
    ("search_energy_range_pct", "Search energy range", "%", "max"),
    ("minimum_sense_margin_mv", "Minimum sense margin", "mV", "min"),
)
MAIN_EFFECT_BOUNDS = (
    (
        "matchline_delay_lower_deviation_pct",
        "matchline_delay_upper_deviation_pct",
        "Matchline delay deviation",
        "Deviation from nominal (%)",
    ),
    (
        "search_latency_lower_deviation_pct",
        "search_latency_upper_deviation_pct",
        "Search latency deviation",
        "Deviation from nominal (%)",
    ),
    (
        "search_energy_lower_deviation_pct",
        "search_energy_upper_deviation_pct",
        "Search energy deviation",
        "Deviation from nominal (%)",
    ),
    (
        "sense_margin_lower_deviation_mv",
        "sense_margin_upper_deviation_mv",
        "Sense margin deviation",
        "Deviation from nominal (mV)",
    ),
)
REQUIRED_SUMMARY_FIELDS = {
    "run_id",
    "corners",
    "matchline_delay_s_nominal",
    "matchline_delay_s_min",
    "matchline_delay_s_max",
    "search_latency_s_nominal",
    "search_latency_s_min",
    "search_latency_s_max",
    "search_dynamic_energy_j_nominal",
    "search_dynamic_energy_j_min",
    "search_dynamic_energy_j_max",
    "sense_margin_v_nominal",
    "sense_margin_v_min",
    "sense_margin_v_max",
    *(name for name, _label in PARAMETERS),
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Analyze an EvaCAM corner sweep without modifying its raw results."
    )
    parser.add_argument(
        "--sweep-dir",
        type=Path,
        default=DEFAULT_SWEEP_DIR,
        help="Directory containing manifest.csv and summary.csv.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        help="Analysis output directory (default: <sweep-dir>/analysis).",
    )
    parser.add_argument(
        "--sense-threshold-mv",
        type=float,
        default=70.0,
        help="Minimum acceptable sense margin in mV (default: 70).",
    )
    parser.add_argument(
        "--top",
        type=int,
        default=25,
        help="Number of cases written to each ranked CSV (default: 25).",
    )
    parser.add_argument(
        "--allow-incomplete",
        action="store_true",
        help="Analyze completed rows even when the manifest is incomplete or failed.",
    )
    parser.add_argument(
        "--no-plots",
        action="store_true",
        help="Write CSV and Markdown outputs without requiring matplotlib.",
    )
    return parser.parse_args()


def read_csv(path: Path) -> list[dict[str, str]]:
    if not path.is_file():
        raise RuntimeError(f"Required input does not exist: {path}")
    with path.open(newline="") as input_file:
        return list(csv.DictReader(input_file))


def validate_inputs(
    summary_rows: list[dict[str, str]],
    manifest_rows: list[dict[str, str]],
    allow_incomplete: bool,
) -> None:
    if not summary_rows:
        raise RuntimeError("summary.csv contains no data rows")
    missing = REQUIRED_SUMMARY_FIELDS - set(summary_rows[0])
    if missing:
        raise RuntimeError(
            "summary.csv is missing required columns: " + ", ".join(sorted(missing))
        )

    statuses = defaultdict(int)
    for row in manifest_rows:
        statuses[row.get("status", "")] += 1
    incomplete = sum(
        count for status, count in statuses.items() if status != "complete"
    )
    if incomplete and not allow_incomplete:
        detail = ", ".join(
            f"{status or '<blank>'}={count}" for status, count in sorted(statuses.items())
        )
        raise RuntimeError(
            f"manifest.csv is incomplete ({detail}); use --allow-incomplete to continue"
        )


def as_float(row: dict[str, str], column: str) -> float:
    try:
        value = float(row[column])
    except (KeyError, TypeError, ValueError) as error:
        raise RuntimeError(
            f"Invalid numeric value for {column!r} in run {row.get('run_id', '<unknown>')}"
        ) from error
    if not math.isfinite(value):
        raise RuntimeError(
            f"Non-finite value for {column!r} in run {row.get('run_id', '<unknown>')}"
        )
    return value


def percent_range(minimum: float, maximum: float, nominal: float) -> float:
    if nominal == 0.0:
        return math.nan
    return 100.0 * (maximum - minimum) / abs(nominal)


def percent_deviation(value: float, nominal: float) -> float:
    if nominal == 0.0:
        return math.nan
    return 100.0 * (value - nominal) / abs(nominal)


def derive_rows(summary_rows: list[dict[str, str]]) -> list[dict[str, str | float | int]]:
    derived = []
    for source in summary_rows:
        row: dict[str, str | float | int] = {
            "run_id": source["run_id"],
            "result_dir": source.get("result_dir", ""),
            "corners": int(source["corners"]),
        }
        for parameter, _label in PARAMETERS:
            row[parameter] = int(source[parameter])

        matchline_nominal = as_float(source, "matchline_delay_s_nominal")
        matchline_min = as_float(source, "matchline_delay_s_min")
        matchline_max = as_float(source, "matchline_delay_s_max")
        latency_nominal = as_float(source, "search_latency_s_nominal")
        latency_min = as_float(source, "search_latency_s_min")
        latency_max = as_float(source, "search_latency_s_max")
        energy_nominal = as_float(source, "search_dynamic_energy_j_nominal")
        energy_min = as_float(source, "search_dynamic_energy_j_min")
        energy_max = as_float(source, "search_dynamic_energy_j_max")
        sense_nominal = as_float(source, "sense_margin_v_nominal")
        sense_min = as_float(source, "sense_margin_v_min")
        sense_max = as_float(source, "sense_margin_v_max")

        row.update(
            {
                "matchline_delay_nominal_ps": matchline_nominal * 1e12,
                "matchline_delay_min_ps": matchline_min * 1e12,
                "matchline_delay_max_ps": matchline_max * 1e12,
                "matchline_delay_range_pct": percent_range(
                    matchline_min, matchline_max, matchline_nominal
                ),
                "matchline_delay_lower_deviation_pct": percent_deviation(
                    matchline_min, matchline_nominal
                ),
                "matchline_delay_upper_deviation_pct": percent_deviation(
                    matchline_max, matchline_nominal
                ),
                "search_latency_nominal_ps": latency_nominal * 1e12,
                "search_latency_min_ps": latency_min * 1e12,
                "search_latency_max_ps": latency_max * 1e12,
                "search_latency_range_pct": percent_range(
                    latency_min, latency_max, latency_nominal
                ),
                "search_latency_lower_deviation_pct": percent_deviation(
                    latency_min, latency_nominal
                ),
                "search_latency_upper_deviation_pct": percent_deviation(
                    latency_max, latency_nominal
                ),
                "search_energy_nominal_pj": energy_nominal * 1e12,
                "search_energy_min_pj": energy_min * 1e12,
                "search_energy_max_pj": energy_max * 1e12,
                "search_energy_range_pct": percent_range(
                    energy_min, energy_max, energy_nominal
                ),
                "search_energy_lower_deviation_pct": percent_deviation(
                    energy_min, energy_nominal
                ),
                "search_energy_upper_deviation_pct": percent_deviation(
                    energy_max, energy_nominal
                ),
                "sense_margin_nominal_mv": sense_nominal * 1e3,
                "minimum_sense_margin_mv": sense_min * 1e3,
                "maximum_sense_margin_mv": sense_max * 1e3,
                "sense_margin_lower_deviation_mv": (sense_min - sense_nominal)
                * 1e3,
                "sense_margin_upper_deviation_mv": (sense_max - sense_nominal)
                * 1e3,
            }
        )
        derived.append(row)
    return derived


def write_csv(path: Path, rows: list[dict], fieldnames=None) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if fieldnames is None:
        if not rows:
            raise RuntimeError(f"Cannot infer columns for empty output: {path}")
        fieldnames = list(rows[0])
    with path.open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def ranked_rows(
    rows: list[dict],
    metric: str,
    direction: str,
    top: int,
) -> list[dict]:
    reverse = direction == "max"
    return sorted(rows, key=lambda row: float(row[metric]), reverse=reverse)[:top]


def build_main_effects(rows: list[dict]) -> list[dict[str, str | float | int]]:
    effects = []
    for parameter, parameter_label in PARAMETERS:
        grouped: dict[int, list[dict]] = defaultdict(list)
        for row in rows:
            grouped[int(row[parameter])].append(row)
        for level in sorted(grouped):
            group = grouped[level]
            effect: dict[str, str | float | int] = {
                "parameter": parameter,
                "parameter_label": parameter_label,
                "level_percent": level,
                "run_count": len(group),
            }
            for metric, _label, _unit, _direction in DERIVED_METRICS:
                values = [float(row[metric]) for row in group]
                effect[f"{metric}_mean"] = statistics.fmean(values)
                effect[f"{metric}_median"] = statistics.median(values)
                effect[f"{metric}_min"] = min(values)
                effect[f"{metric}_max"] = max(values)
            for lower_metric, upper_metric, _label, _unit in MAIN_EFFECT_BOUNDS:
                for metric in (lower_metric, upper_metric):
                    values = [float(row[metric]) for row in group]
                    effect[f"{metric}_mean"] = statistics.fmean(values)
            effects.append(effect)
    return effects


def build_sensitivity(main_effects: list[dict]) -> list[dict[str, str | float]]:
    sensitivity = []
    for metric, metric_label, unit, direction in DERIVED_METRICS:
        for parameter, parameter_label in PARAMETERS:
            values = [
                float(row[f"{metric}_mean"])
                for row in main_effects
                if row["parameter"] == parameter
            ]
            low_level = values[0]
            high_level = values[-1]
            sensitivity.append(
                {
                    "metric": metric,
                    "metric_label": metric_label,
                    "unit": unit,
                    "parameter": parameter,
                    "parameter_label": parameter_label,
                    "mean_at_lowest_level": low_level,
                    "mean_at_highest_level": high_level,
                    "high_minus_low": high_level - low_level,
                    "main_effect_range": max(values) - min(values),
                    "adverse_direction": direction,
                }
            )
    return sensitivity


def aggregate_pair(
    rows: list[dict],
    x_parameter: str,
    y_parameter: str,
    metric: str,
) -> tuple[list[int], list[int], list[list[float]]]:
    grouped: dict[tuple[int, int], list[float]] = defaultdict(list)
    x_levels = sorted({int(row[x_parameter]) for row in rows})
    y_levels = sorted({int(row[y_parameter]) for row in rows})
    for row in rows:
        grouped[(int(row[x_parameter]), int(row[y_parameter]))].append(
            float(row[metric])
        )
    matrix = [
        [
            statistics.fmean(grouped[(x_level, y_level)])
            if grouped[(x_level, y_level)]
            else math.nan
            for x_level in x_levels
        ]
        for y_level in y_levels
    ]
    return x_levels, y_levels, matrix


def setup_matplotlib():
    os.environ.setdefault("MPLCONFIGDIR", "/tmp/evacam-matplotlib")
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError as error:
        raise RuntimeError(
            "matplotlib is required for plots; install it or use --no-plots"
        ) from error
    return plt


def plot_main_effects(main_effects: list[dict], output_dir: Path, plt) -> None:
    fig, axes = plt.subplots(2, 2, figsize=(12, 8), constrained_layout=True)
    for axis, (lower_metric, upper_metric, label, y_label) in zip(
        axes.flat, MAIN_EFFECT_BOUNDS
    ):
        for parameter, parameter_label in PARAMETERS:
            selected = [
                row for row in main_effects if row["parameter"] == parameter
            ]
            levels = [int(row["level_percent"]) for row in selected]
            lower_values = [
                float(row[f"{lower_metric}_mean"]) for row in selected
            ]
            upper_values = [
                float(row[f"{upper_metric}_mean"]) for row in selected
            ]
            upper_line = axis.plot(
                levels,
                upper_values,
                marker="o",
                linestyle="-",
                label=parameter_label,
            )[0]
            axis.plot(
                levels,
                lower_values,
                marker="o",
                linestyle="--",
                color=upper_line.get_color(),
            )
        axis.set_title(label)
        axis.set_xlabel("Variation bound (%)")
        axis.set_ylabel(y_label)
        axis.axhline(0.0, color="black", linewidth=0.8, alpha=0.6)
        axis.grid(axis="y", alpha=0.3)
        axis.spines["top"].set_visible(False)
        axis.spines["right"].set_visible(False)
    axes[0][0].legend(
        fontsize=8,
        title="Solid: upper; dashed: lower",
        title_fontsize=8,
    )
    fig.suptitle("Corner Sweep Main Effects")
    fig.savefig(output_dir / "main_effects.svg", format="svg")
    plt.close(fig)


def plot_distributions(rows: list[dict], output_dir: Path, plt) -> None:
    fig, axes = plt.subplots(2, 2, figsize=(11, 8), constrained_layout=True)
    for axis, (metric, label, unit, _direction) in zip(
        axes.flat, DERIVED_METRICS
    ):
        values = [float(row[metric]) for row in rows]
        axis.hist(values, bins=30, color="#4f7cac", edgecolor="white")
        axis.axvline(statistics.median(values), color="#8250df", linestyle="--")
        axis.set_title(label)
        axis.set_xlabel(unit)
        axis.set_ylabel("Runs")
        axis.spines["top"].set_visible(False)
        axis.spines["right"].set_visible(False)
    fig.suptitle("Corner Sweep Outcome Distributions")
    fig.savefig(output_dir / "distributions.svg", format="svg")
    plt.close(fig)


def plot_pairwise_heatmaps(rows: list[dict], output_dir: Path, plt) -> None:
    pairs = [
        (PARAMETERS[i], PARAMETERS[j])
        for i in range(len(PARAMETERS))
        for j in range(i + 1, len(PARAMETERS))
    ]
    for metric, metric_label, unit, _direction in DERIVED_METRICS:
        fig, axes = plt.subplots(2, 5, figsize=(18, 7), constrained_layout=True)
        pair_data = [
            (
                x_info,
                y_info,
                *aggregate_pair(rows, x_info[0], y_info[0], metric),
            )
            for x_info, y_info in pairs
        ]
        finite_values = [
            value
            for _x_info, _y_info, _x_levels, _y_levels, matrix in pair_data
            for matrix_row in matrix
            for value in matrix_row
            if math.isfinite(value)
        ]
        color_min = min(finite_values)
        color_max = max(finite_values)
        image = None
        for axis, (
            (_x_parameter, x_label),
            (_y_parameter, y_label),
            x_levels,
            y_levels,
            matrix,
        ) in zip(
            axes.flat, pair_data
        ):
            image = axis.imshow(
                matrix,
                origin="lower",
                aspect="auto",
                cmap="viridis",
                vmin=color_min,
                vmax=color_max,
            )
            axis.set_xticks(range(len(x_levels)), x_levels)
            axis.set_yticks(range(len(y_levels)), y_levels)
            axis.set_xlabel(f"{x_label} (%)", fontsize=8)
            axis.set_ylabel(f"{y_label} (%)", fontsize=8)
            axis.tick_params(labelsize=8)
        if image is not None:
            fig.colorbar(image, ax=axes.ravel().tolist(), label=unit, shrink=0.85)
        fig.suptitle(f"Pairwise Mean: {metric_label}")
        fig.savefig(output_dir / f"pairwise_{metric}.svg", format="svg")
        plt.close(fig)


def format_number(value: float) -> str:
    return f"{value:.6g}"


def write_report(
    output_dir: Path,
    rows: list[dict],
    manifest_rows: list[dict[str, str]],
    sensitivity: list[dict],
    threshold_mv: float,
    top: int,
    plots_written: bool,
) -> None:
    below_threshold = [
        row for row in rows if float(row["minimum_sense_margin_mv"]) < threshold_mv
    ]
    total_corners = sum(int(row["corners"]) for row in rows)
    status_counts = defaultdict(int)
    for row in manifest_rows:
        status_counts[row.get("status", "")] += 1

    lines = [
        "# Corner Sweep Analysis",
        "",
        f"- Analyzed runs: {len(rows):,}",
        f"- Evaluated corners represented: {total_corners:,}",
        f"- Sense-margin threshold: {threshold_mv:g} mV",
        f"- Runs below threshold: {len(below_threshold):,}",
        "- Manifest status: "
        + ", ".join(
            f"{status or '<blank>'}={count:,}"
            for status, count in sorted(status_counts.items())
        ),
        "",
        "## Worst Cases",
        "",
    ]
    for metric, label, unit, direction in DERIVED_METRICS:
        worst = ranked_rows(rows, metric, direction, 1)[0]
        lines.append(
            f"- {label}: `{worst['run_id']}` at "
            f"{format_number(float(worst[metric]))} {unit}"
        )

    lines.extend(
        [
            "",
            "## Main-Effect Sensitivity",
            "",
            "The table ranks parameters by the range of their level-averaged outcome. "
            "This is a screening statistic, not a causal decomposition.",
            "",
            "| Metric | Parameter | Main-effect range | Unit |",
            "| --- | --- | ---: | --- |",
        ]
    )
    for metric, metric_label, unit, _direction in DERIVED_METRICS:
        selected = sorted(
            (row for row in sensitivity if row["metric"] == metric),
            key=lambda row: float(row["main_effect_range"]),
            reverse=True,
        )
        for row in selected:
            lines.append(
                f"| {metric_label} | {row['parameter_label']} | "
                f"{format_number(float(row['main_effect_range']))} | {unit} |"
            )

    lines.extend(
        [
            "",
            "## Outputs",
            "",
            "- `derived_metrics.csv`: one normalized row per sweep configuration.",
            "- `main_effects.csv`: grouped statistics at each parameter level.",
            "- `sensitivity.csv`: parameter screening ranges.",
            f"- `rankings/`: the top {top} adverse cases for each outcome.",
        ]
    )
    if plots_written:
        lines.extend(
            [
                "- `main_effects.svg`: level-averaged response curves.",
                "- `distributions.svg`: outcome distributions.",
                "- `pairwise_*.svg`: pairwise means averaged over remaining inputs.",
            ]
        )
    (output_dir / "analysis_summary.md").write_text("\n".join(lines) + "\n")


def main() -> None:
    args = parse_args()
    if args.top <= 0:
        raise SystemExit("--top must be positive")
    if args.sense_threshold_mv < 0:
        raise SystemExit("--sense-threshold-mv must be non-negative")

    sweep_dir = args.sweep_dir.resolve()
    output_dir = (args.output_dir or sweep_dir / "analysis").resolve()
    summary_rows = read_csv(sweep_dir / "summary.csv")
    manifest_rows = read_csv(sweep_dir / "manifest.csv")
    validate_inputs(summary_rows, manifest_rows, args.allow_incomplete)
    rows = derive_rows(summary_rows)

    output_dir.mkdir(parents=True, exist_ok=True)
    write_csv(output_dir / "derived_metrics.csv", rows)

    rankings_dir = output_dir / "rankings"
    for metric, _label, _unit, direction in DERIVED_METRICS:
        write_csv(
            rankings_dir / f"worst_{metric}.csv",
            ranked_rows(rows, metric, direction, args.top),
        )

    below_threshold = [
        row
        for row in rows
        if float(row["minimum_sense_margin_mv"]) < args.sense_threshold_mv
    ]
    write_csv(
        rankings_dir / "sense_margin_below_threshold.csv",
        sorted(below_threshold, key=lambda row: float(row["minimum_sense_margin_mv"])),
        fieldnames=list(rows[0]),
    )

    main_effects = build_main_effects(rows)
    sensitivity = build_sensitivity(main_effects)
    write_csv(output_dir / "main_effects.csv", main_effects)
    write_csv(output_dir / "sensitivity.csv", sensitivity)

    plots_written = not args.no_plots
    if plots_written:
        plt = setup_matplotlib()
        plot_main_effects(main_effects, output_dir, plt)
        plot_distributions(rows, output_dir, plt)
        plot_pairwise_heatmaps(rows, output_dir, plt)

    write_report(
        output_dir,
        rows,
        manifest_rows,
        sensitivity,
        args.sense_threshold_mv,
        args.top,
        plots_written,
    )
    print(f"Analyzed {len(rows):,} runs")
    print(f"Wrote analysis to {output_dir}")


if __name__ == "__main__":
    main()
