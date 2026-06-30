#!/usr/bin/env python3
"""Generate and optionally run the 2FeFET-TCAM corner-variation sweep."""

import argparse
import csv
import itertools
import os
import re
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BASE_CELL = ROOT / "config" / "2FeFET_TCAM" / "2FeFET_TCAM_corner_cell_config.yaml"
BASE_SYSTEM = ROOT / "config" / "2FeFET_TCAM" / "2FeFET_TCAM_corner_system_config.yaml"
EVA_CAM = ROOT / "EvaCAM"
SWEEP_ROOT = ROOT / "results" / "corner_sweep"
RUNS_ROOT = SWEEP_ROOT / "runs"
MANIFEST_PATH = SWEEP_ROOT / "manifest.csv"
SUMMARY_PATH = SWEEP_ROOT / "summary.csv"

DEFAULT_SWEEP_VALUES = (0, 2, 4, 6, 8)
SUPPORTED_VARIATION_FIELDS = (
    ("memory_device_resistance_on_max_var", "on"),
    ("memory_device_resistance_off_max_var", "off"),
)
FIELD_ALIASES = {
    short: (field, short) for field, short in SUPPORTED_VARIATION_FIELDS
}
FIELD_ALIASES.update({
    field: (field, short) for field, short in SUPPORTED_VARIATION_FIELDS
})
METRICS = (
    "matchline_delay_s",
    "search_latency_s",
    "search_dynamic_energy_j",
    "exact_match_sense_margin_v",
)
MANIFEST_FIELDS = (
    "run_id",
    "on_var_percent",
    "off_var_percent",
    "expected_corners",
    "status",
    "elapsed_seconds",
    "result_dir",
    "message",
)


@dataclass(frozen=True)
class RunSpec:
    fields: tuple[tuple[str, str], ...]
    values: tuple[int, ...]

    @property
    def run_id(self) -> str:
        labels = (
            f"{short}{value:02d}"
            for (_, short), value in zip(self.fields, self.values)
        )
        return "_".join(labels)

    @property
    def run_dir(self) -> Path:
        return RUNS_ROOT / self.run_id

    @property
    def expected_corners(self) -> int:
        return 2 ** sum(value > 0 for value in self.values)

    @property
    def cell_path(self) -> Path:
        return self.run_dir / "cell_config.yaml"

    @property
    def system_path(self) -> Path:
        return self.run_dir / "system_config.yaml"

    @property
    def result_path(self) -> Path:
        return self.run_dir / "corner_results.yaml"

    @property
    def samples_path(self) -> Path:
        return self.run_dir / "corner_variation_samples.csv"

    @property
    def log_path(self) -> Path:
        return self.run_dir / "run.log"


def all_specs(
    fields: tuple[tuple[str, str], ...],
    sweep_values: tuple[int, ...],
) -> list[RunSpec]:
    return [
        RunSpec(fields, values)
        for values in itertools.product(sweep_values, repeat=len(fields))
        if any(values)
    ]


def split_cli_values(raw_values: list[str]) -> list[str]:
    values = []
    for raw_value in raw_values:
        values.extend(part.strip() for part in raw_value.split(","))
    return [value for value in values if value]


def parse_corner_fields(raw_fields: list[str]) -> tuple[tuple[str, str], ...]:
    fields = []
    seen = set()
    for name in split_cli_values(raw_fields):
        try:
            field = FIELD_ALIASES[name]
        except KeyError as error:
            supported = ", ".join(sorted(FIELD_ALIASES))
            raise argparse.ArgumentTypeError(
                f"unsupported corner field {name!r}; supported values: {supported}"
            ) from error
        if field[0] in seen:
            raise argparse.ArgumentTypeError(f"duplicate corner field {name!r}")
        seen.add(field[0])
        fields.append(field)
    if not fields:
        raise argparse.ArgumentTypeError("at least one corner field is required")
    return tuple(fields)


def parse_corner_values(raw_values: list[str]) -> tuple[int, ...]:
    values = []
    for raw_value in split_cli_values(raw_values):
        try:
            value = int(raw_value)
        except ValueError as error:
            raise argparse.ArgumentTypeError(
                f"corner value must be an integer percent: {raw_value!r}"
            ) from error
        if value < 0 or value >= 100:
            raise argparse.ArgumentTypeError(
                f"corner value must be in the range [0, 99]: {value}"
            )
        values.append(value)
    unique_values = tuple(sorted(set(values)))
    if not unique_values:
        raise argparse.ArgumentTypeError("at least one corner value is required")
    return unique_values


def relative_to_root(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def replace_variation_block(text: str, spec: RunSpec) -> str:
    selected = {
        field: value
        for (field, _short), value in zip(spec.fields, spec.values)
    }
    lines = [
        "variation:",
        "  with_variation: true",
        "  mode: corner",
    ]
    lines.extend(
        f"  {field}: {selected.get(field, 0)}%"
        for field, _short in SUPPORTED_VARIATION_FIELDS
    )
    block = "\n".join(lines) + "\n"
    updated, count = re.subn(
        r"^variation:\n(?:^  .*\n)*",
        block,
        text,
        count=1,
        flags=re.MULTILINE,
    )
    if count != 1:
        raise RuntimeError(f"Could not replace variation block in {BASE_CELL}")
    return updated


def replace_cell_file(text: str, cell_path: Path) -> str:
    updated, count = re.subn(
        r"^  cell_file: .*$",
        f"  cell_file: ./{relative_to_root(cell_path)}",
        text,
        count=1,
        flags=re.MULTILINE,
    )
    if count != 1:
        raise RuntimeError(f"Could not replace memory.cell_file in {BASE_SYSTEM}")
    return updated


def set_output_yaml(text: str, output_path: Path) -> str:
    replacement = f"  output_yaml_file: {relative_to_root(output_path)}"
    if re.search(r"^  output_yaml_file: .*$", text, flags=re.MULTILINE):
        return re.sub(
            r"^  output_yaml_file: .*$",
            replacement,
            text,
            count=1,
            flags=re.MULTILINE,
        )
    if re.search(r"^extra:$", text, flags=re.MULTILINE):
        return re.sub(
            r"^extra:$",
            f"extra:\n{replacement}",
            text,
            count=1,
            flags=re.MULTILINE,
        )
    return text.rstrip() + f"\n\nextra:\n{replacement}\n"


def manifest_row(
    spec: RunSpec,
    status: str,
    elapsed_seconds: str = "",
    message: str = "",
) -> dict[str, str | int]:
    return {
        "run_id": spec.run_id,
        "on_var_percent": selected_percent(spec, "on"),
        "off_var_percent": selected_percent(spec, "off"),
        "expected_corners": spec.expected_corners,
        "status": status,
        "elapsed_seconds": elapsed_seconds,
        "result_dir": relative_to_root(spec.run_dir),
        "message": message,
    }


def selected_percent(spec: RunSpec, short_name: str) -> int:
    for (_field, short), value in zip(spec.fields, spec.values):
        if short == short_name:
            return value
    return 0


def write_csv_atomic(path: Path, fieldnames: tuple[str, ...], rows: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    with temporary.open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)
    temporary.replace(path)


def generate_configs(specs: list[RunSpec], overwrite: bool) -> None:
    cell_template = BASE_CELL.read_text()
    system_template = BASE_SYSTEM.read_text()
    RUNS_ROOT.mkdir(parents=True, exist_ok=True)

    for spec in specs:
        spec.run_dir.mkdir(parents=True, exist_ok=True)
        if overwrite or not spec.cell_path.exists():
            spec.cell_path.write_text(replace_variation_block(cell_template, spec))
        if overwrite or not spec.system_path.exists():
            system = replace_cell_file(system_template, spec.cell_path)
            spec.system_path.write_text(set_output_yaml(system, spec.result_path))


def completed(spec: RunSpec) -> bool:
    if not spec.result_path.is_file() or not spec.samples_path.is_file():
        return False
    try:
        with spec.samples_path.open(newline="") as sample_file:
            rows = list(csv.DictReader(sample_file))
    except (OSError, csv.Error):
        return False
    return len(rows) == spec.expected_corners


def generate_artifacts(spec: RunSpec, log_file) -> None:
    commands = (
        (
            "plot",
            ROOT / "scripts" / "plot_corner_variation.py",
            spec.run_dir / "corner_plot.svg",
            ("--order", "sample"),
        ),
        (
            "table",
            ROOT / "scripts" / "generate_corner_variation_table.py",
            spec.run_dir / "corner_table.md",
            (),
        ),
    )
    for label, script, output, extra_args in commands:
        process = subprocess.run(
            [
                "python3",
                str(script),
                str(spec.samples_path),
                "--output",
                str(output),
                *extra_args,
            ],
            cwd=ROOT,
            stdout=log_file,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )
        if process.returncode != 0:
            raise RuntimeError(f"{label} generation exited with code {process.returncode}")


def run_one(spec: RunSpec, force: bool, artifacts: bool) -> tuple[str, float, str]:
    if not force and completed(spec):
        return "complete", 0.0, "existing valid output"

    start = time.monotonic()
    with spec.log_path.open("w") as log_file:
        process = subprocess.run(
            [
                str(EVA_CAM),
                "--threads",
                "1",
                "--quiet",
                "--output",
                str(spec.result_path),
                str(spec.system_path),
            ],
            cwd=ROOT,
            stdout=log_file,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )
        if process.returncode != 0:
            return (
                "failed",
                time.monotonic() - start,
                f"EvaCAM exited with code {process.returncode}",
            )
        if not completed(spec):
            return (
                "failed",
                time.monotonic() - start,
                "output files are missing or contain an unexpected number of corners",
            )
        if artifacts:
            try:
                generate_artifacts(spec, log_file)
            except RuntimeError as error:
                return "failed", time.monotonic() - start, str(error)
    return "complete", time.monotonic() - start, ""


def summary_row(spec: RunSpec) -> dict[str, str | int]:
    with spec.samples_path.open(newline="") as sample_file:
        rows = list(csv.DictReader(sample_file))

    row: dict[str, str | int] = {
        "run_id": spec.run_id,
        "on_var_percent": selected_percent(spec, "on"),
        "off_var_percent": selected_percent(spec, "off"),
        "corners": len(rows),
        "result_dir": relative_to_root(spec.run_dir),
    }
    for metric in METRICS:
        values = [float(sample[metric]) for sample in rows if sample.get(metric)]
        nominal_values = [
            sample[f"nominal_{metric}"]
            for sample in rows
            if sample.get(f"nominal_{metric}")
        ]
        row[f"{metric}_nominal"] = nominal_values[0] if nominal_values else ""
        row[f"{metric}_min"] = min(values) if values else ""
        row[f"{metric}_max"] = max(values) if values else ""
    return row


def write_summary(specs: list[RunSpec]) -> None:
    rows = [summary_row(spec) for spec in specs if completed(spec)]
    fields = (
        "run_id",
        "on_var_percent",
        "off_var_percent",
        "corners",
        *(f"{metric}_{bound}" for metric in METRICS for bound in ("nominal", "min", "max")),
        "result_dir",
    )
    write_csv_atomic(SUMMARY_PATH, fields, rows)


def run_sweep(specs: list[RunSpec], jobs: int, force: bool, artifacts: bool) -> None:
    if not EVA_CAM.is_file():
        raise RuntimeError("EvaCAM is not built. Run `make -j` before starting the sweep.")

    manifest = {
        spec.run_id: manifest_row(
            spec,
            "complete" if completed(spec) and not force else "pending",
        )
        for spec in specs
    }
    write_csv_atomic(MANIFEST_PATH, MANIFEST_FIELDS, list(manifest.values()))

    with ThreadPoolExecutor(max_workers=jobs) as executor:
        futures = {
            executor.submit(run_one, spec, force, artifacts): spec for spec in specs
        }
        finished = 0
        for future in as_completed(futures):
            spec = futures[future]
            try:
                status, elapsed, message = future.result()
            except Exception as error:  # Preserve the rest of a long sweep.
                status, elapsed, message = "failed", 0.0, str(error)
            manifest[spec.run_id] = manifest_row(
                spec,
                status,
                f"{elapsed:.3f}",
                message,
            )
            finished += 1
            if finished % 25 == 0 or finished == len(specs):
                write_csv_atomic(
                    MANIFEST_PATH,
                    MANIFEST_FIELDS,
                    list(manifest.values()),
                )
                print(f"Completed {finished}/{len(specs)} processes")

    write_summary(specs)


def parse_args() -> argparse.Namespace:
    default_jobs = min(16, os.cpu_count() or 1)
    parser = argparse.ArgumentParser(
        description=(
            "Generate the 0/2/4/6/8 percent memory-device corner sweep. "
            "The invalid all-zero corner configuration is excluded. "
            "EvaCAM is run only when --run is supplied."
        )
    )
    parser.add_argument(
        "--run",
        action="store_true",
        help="Run pending EvaCAM cases after generating their configs.",
    )
    parser.add_argument(
        "--jobs",
        type=int,
        default=default_jobs,
        help=f"Concurrent single-threaded EvaCAM processes (default: {default_jobs}).",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Overwrite generated configs and rerun cases with valid existing outputs.",
    )
    parser.add_argument(
        "--artifacts",
        action="store_true",
        help="Generate a corner SVG and Markdown table for every completed run.",
    )
    parser.add_argument(
        "--corner-fields",
        nargs="+",
        default=["on", "off"],
        metavar="FIELD",
        help=(
            "Corner max-var inputs to sweep. Use aliases 'on' and 'off' or full "
            "YAML field names. Default: on off."
        ),
    )
    parser.add_argument(
        "--corner-values",
        nargs="+",
        default=[str(value) for value in DEFAULT_SWEEP_VALUES],
        metavar="PERCENT",
        help=(
            "Integer percent levels to sweep. Values may be space- or "
            "comma-separated. Default: 0 2 4 6 8."
        ),
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.jobs <= 0:
        raise SystemExit("--jobs must be positive")

    try:
        fields = parse_corner_fields(args.corner_fields)
        sweep_values = parse_corner_values(args.corner_values)
    except argparse.ArgumentTypeError as error:
        raise SystemExit(str(error)) from error

    specs = all_specs(fields, sweep_values)
    if not specs:
        raise SystemExit(
            "corner values produced no valid cases; provide at least one positive "
            "percentage"
        )
    total_corners = sum(spec.expected_corners for spec in specs)
    print(
        f"Sweep contains {len(specs):,} runs and {total_corners:,} total corners."
    )
    generate_configs(specs, overwrite=args.force)

    if not args.run:
        rows = [manifest_row(spec, "pending") for spec in specs]
        write_csv_atomic(MANIFEST_PATH, MANIFEST_FIELDS, rows)
        print(f"Generated configs under {relative_to_root(RUNS_ROOT)}")
        print("EvaCAM was not run. Supply --run when ready.")
        return

    run_sweep(specs, args.jobs, args.force, args.artifacts)


if __name__ == "__main__":
    main()
