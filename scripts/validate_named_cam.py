#!/usr/bin/env python3
"""Run the paper reference fixtures and report gaps without fitting the model."""
from __future__ import annotations

import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import tempfile

import yaml

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs/validation/named-cam.reference.yaml"
TIME_NS = {"s": 1e9, "ms": 1e6, "us": 1e3, "ns": 1, "ps": 1e-3, "fs": 1e-6}
ENERGY_PJ = {"J": 1e12, "mJ": 1e9, "uJ": 1e6, "nJ": 1e3, "pJ": 1, "fJ": 1e-3}
AREA_UM2 = {"m^2": 1e12, "mm^2": 1e6, "um^2": 1, "nm^2": 1e-6}


def quantity(value, units):
    """Convert a finite nonnegative result with an explicit recognized unit."""
    match = re.fullmatch(r"\s*([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)\s*(\S+)\s*", str(value))
    if not match or match[2] not in units:
        raise ValueError(f"Unsupported result quantity: {value!r}")
    number = float(match[1]) * units[match[2]]
    if not math.isfinite(number) or number < 0:
        raise ValueError(f"Invalid result quantity: {value!r}")
    return number


def compare_result(case, result):
    """Require the modeled geometry and feasible sensing before reporting metrics."""
    if not isinstance(result, dict) or "summary" not in result:
        raise ValueError("Run did not produce a numerical solution")
    expected = case["expected"]
    geometry = result["geometry"]
    capacity = expected["entries"] * expected["word_bits"]
    required = {
        "entry_count": expected["entries"],
        "logical_word_width_bits": expected["word_bits"],
        "physical_columns_per_word": expected["word_bits"],
        "logical_capacity_bits": capacity,
        "allocated_capacity_bits": capacity,
        "physical_cell_count": capacity,
        "comparison_columns_per_step": expected["word_bits"],
        "comparison_steps": 1,
    }
    for key, value in required.items():
        if geometry.get(key) != value:
            raise ValueError(f"Geometry drift: {key}={geometry.get(key)}, expected {value}")
    rows, columns = expected["subarray"]
    if rows * columns * expected["subarray_count"] != capacity:
        raise ValueError("Reference subarray count does not allocate its stated capacity")
    summary = result["summary"]
    if summary["area"]["subarray"]["dimensions"] != f"{rows}x{columns}":
        raise ValueError("Modeled subarray dimensions differ from the reference fixture")
    technology = result["assumptions"]["technology"]
    modeling = result["assumptions"]["modeling_options"]
    if modeling["search_latency_scope"] != "scheduled_full_search":
        raise ValueError("Reference timing must include the scheduled full search")
    if modeling["matchline_timing_model"] != expected.get("matchline_timing_model", "horowitz_50_percent"):
        raise ValueError("Matchline decision model drift")
    schedule = result["assumptions"].get("search_timing", {})
    for key, value in expected.get("search_timing", {}).items():
        if schedule.get(key) != value:
            raise ValueError(f"Search schedule drift: {key}")
    if not expected.get("search_timing") and schedule:
        raise ValueError("Unexpected explicit search schedule")
    topology = expected.get("cell_topology", "generic_ports")
    if result["assumptions"]["cell_topology"] != topology:
        raise ValueError("Cell topology drift")
    for key in ("process_node", "physical_feature_size"):
        if quantity(technology[key], {"nm": 1, "um": 1000, "m": 1e9}) != expected["node_nm"]:
            raise ValueError(f"Technology drift: {key}")
    if summary["timing"]["sense_margin_pass"] is not True:
        raise ValueError("Reference fixture failed its configured sense margin")
    latency = quantity(summary["timing"]["search_latency"], TIME_NS)
    energy = quantity(summary["power"]["search_dynamic_energy"], ENERGY_PJ)
    area = quantity(summary["area"]["total"]["area"], AREA_UM2)
    if min(latency, energy, area) <= 0:
        raise ValueError("Search latency, energy and total area must be positive")
    paper = float(case["reference"]["latency_ns"])
    if not math.isfinite(paper) or paper <= 0:
        raise ValueError("Paper latency must be finite and positive")
    return {
        "paper_latency_ns": paper,
        "search_latency_ns": latency,
        "matchline_delay_ns": quantity(result["breakdown"]["search_latency"]["matchline"], TIME_NS),
        "control_node_delay_ns": quantity(result["breakdown"]["search_latency"]["control_node"], TIME_NS),
        "sense_amplifier_delay_ns": quantity(result["breakdown"]["search_latency"]["sense_amplifier"], TIME_NS),
        "search_latency_scope": modeling["search_latency_scope"],
        "sense_amplifier_model": modeling["sense_amplifier_model"],
        "matchline_timing_model": modeling["matchline_timing_model"],
        "search_timing": schedule,
        "search_energy_pj": energy,
        "area_um2": area,
        "latency_gap_percent": 100 * (latency / paper - 1),
        "comparison_status": case["status"],
    }


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def render_table(runs):
    lines = [
        "# Named CAM reference configurations",
        "",
        "All fixtures are partial reconstructions. Gaps are raw numerical differences, not accuracy scores.",
        "Bank search includes configured peripherals and precharge; ML is only the matchline component.",
        "",
        "| Configuration | Paper ns | Bank search ns | Control node ns | ML ns | SA ns | Gap | Status |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]
    for run in runs:
        metrics = run.get("metrics")
        if metrics:
            lines.append(f"| {run['id']} | {metrics['paper_latency_ns']:.4g} | "
                         f"{metrics['search_latency_ns']:.6g} | {metrics['control_node_delay_ns']:.6g} | "
                         f"{metrics['matchline_delay_ns']:.6g} | {metrics['sense_amplifier_delay_ns']:.6g} | "
                         f"{metrics['latency_gap_percent']:+.1f}% | {metrics['comparison_status']} |")
        else:
            lines.append(f"| {run['id']} | — | — | — | — | — | — | FAILED |")
    return "\n".join(lines) + "\n"


def ablation_cases(case, directory):
    """Isolate timing/schedule changes while retaining the pre-change SA proxy."""
    source = ROOT / case["config"]
    original_top = yaml.safe_load(source.read_text())
    architecture_path = (source.parent / original_top["architecture"]).resolve()
    original_architecture = yaml.safe_load(architecture_path.read_text())
    sensing_path = (architecture_path.parent / original_architecture["sensing"]).resolve()
    original_sensing = yaml.safe_load(sensing_path.read_text())
    cases = []
    for name, decision, schedule in (("legacy", False, False), ("decision_only", True, False),
                                     ("schedule_only", False, True), ("combined_legacy_sa", True, True)):
        variant = copy.deepcopy(case)
        variant["id"] += "/" + name
        top, architecture, sensing = map(copy.deepcopy, (original_top, original_architecture, original_sensing))
        if not decision:
            sensing.pop("decision", None)
            variant["expected"].pop("matchline_timing_model", None)
        if not schedule:
            architecture.pop("search_timing", None)
            variant["expected"].pop("search_timing", None)
        sensing["sensing_mode"] = "nvsim_cur" if case["id"] == "ReRAM-3T1R-ISSCC15" else "nvsim_vol"
        variant["expected"]["sensing_mode"] = sensing["sensing_mode"]
        sensing["sense_amplifier"] = str((sensing_path.parent / sensing["sense_amplifier"]).resolve())
        for field in ("technology", "cell"):
            top[field] = str((source.parent / top[field]).resolve())
        folder = directory / "configs" / case["id"] / name
        folder.mkdir(parents=True)
        top["architecture"] = "architecture.yaml"
        architecture["sensing"] = "sensing.yaml"
        for filename, content in (("run.yaml", top), ("architecture.yaml", architecture), ("sensing.yaml", sensing)):
            (folder / filename).write_text(yaml.safe_dump(content, sort_keys=False))
        variant["config"] = str(folder / "run.yaml")
        cases.append(variant)
    return cases


def run_suite(binary, output, ablations=False):
    manifest = yaml.safe_load(MANIFEST.read_text())
    if manifest["schema"] != "named_cam_reference" or manifest["version"] != 1:
        raise ValueError("Unsupported reference manifest")
    cases = manifest["cases"]
    if len({case["id"] for case in cases}) != len(cases):
        raise ValueError("Duplicate reference identifier")
    # Each execution gets fresh paths; a failed run cannot reuse an old result YAML.
    output.mkdir(parents=True, exist_ok=True)
    directory = Path(tempfile.mkdtemp(prefix="run-", dir=output))
    inputs = {MANIFEST, *list((ROOT / "config/lib").rglob("*.yaml"))}
    for case in cases:
        inputs.update((ROOT / case["config"]).parent.glob("*.yaml"))
    if ablations:
        cases = [variant for case in cases if "search_timing" in case["expected"]
                 for variant in [*ablation_cases(case, directory), case]]
    runs = []
    for case in cases:
        config = ROOT / case["config"]
        inputs.update(config.parent.glob("*.yaml"))
        label = case["id"].replace("/", "--")
        target = directory / f"{label}.results.yaml"
        log = directory / f"{label}.log"
        command = [str(binary), "-t", "1", "-o", str(target), str(config)]
        run = {"id": case["id"], "command": command, "log": str(log), "result": str(target)}
        try:
            with log.open("w") as stream:
                process = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, timeout=60)
            run["exit_code"] = process.returncode
            if process.returncode != 0:
                raise ValueError(f"EvaCAM exited with code {process.returncode}")
            run["metrics"] = compare_result(case, yaml.safe_load(target.read_text()))
        except (OSError, ValueError, KeyError, TypeError, yaml.YAMLError, subprocess.TimeoutExpired) as error:
            run["error"] = str(error)
        runs.append(run)
    result = {
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "binary_sha256": digest(binary),
        "runner_sha256": digest(Path(__file__)),
        "input_sha256": {str(path.relative_to(ROOT) if path.is_relative_to(ROOT) else path): digest(path)
                         for path in sorted(inputs)},
        "reference_manifest": manifest,
        "runs": runs,
    }
    serialized = json.dumps(result, indent=2, allow_nan=False) + "\n"
    table = render_table(runs)
    for destination in (directory, output):
        (destination / "runs.json").write_text(serialized)
        (destination / "comparison.md").write_text(table)
    print(table)
    print(f"Evidence: {directory}")
    for run in runs:
        if "error" in run:
            print(f"{run['id']}: {run['error']}")
    return 1 if any("error" in run for run in runs) else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "EvaCAM")
    parser.add_argument("--output", type=Path, default=ROOT / "output/validation/paper-configs")
    parser.add_argument("--ablations", action="store_true", help="Separate decision, schedule and sense-amplifier effects")
    args = parser.parse_args()
    return run_suite(args.binary.resolve(), args.output.resolve(), args.ablations)


if __name__ == "__main__":
    raise SystemExit(main())
