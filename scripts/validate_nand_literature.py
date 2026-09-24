#!/usr/bin/env python3
"""Reproducible NAND comparison audit; never treats synthetic inputs as calibration.

The published scalar is contextual until all operating conditions are known.
An independent linear-ladder calculation checks the RC waveform approximation
separately. It is not HSPICE, measured flash, or a reproduction of Yang et al.
"""

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

os.environ.setdefault("MPLCONFIGDIR", "/tmp/evacam-matplotlib")
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import scipy
import yaml
from scipy.optimize import brentq

from nand_rc_reference import RcLadderReference

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "config/NAND_TCAM/NAND_TCAM.config.yaml"
REFERENCE = ROOT / "docs/validation/nand-yang-2023.reference.yaml"


def normalize_energy(energy_j, entries, key_width):
    """Normalize by searched logical key bits, never physical flash cells."""
    if (not np.all(np.isfinite([energy_j, entries, key_width])) or energy_j < 0
            or entries <= 0 or key_width <= 0):
        raise ValueError("finite nonnegative energy and positive logical geometry required")
    return float(energy_j * 1e15 / (entries * key_width))


def comparability(reference):
    """Missing characterization prevents an error metric against the paper."""
    missing = [name for name, value in reference["required_for_numerical_reproduction"].items()
               if value is None]
    if not reference["access"]["full_text_available"]:
        missing.insert(0, "full_text")
    return {"paper_reproduction_validated": False,
            "ready_to_attempt_reproduction": not missing,
            "missing_evidence": missing,
            "reason": "Missing operating conditions" if missing else "Requires independent reference-point comparisons"}


def prepare_variant(base_path, directory, wordlines, key_width):
    """Keep synthetic parameters fixed and change only logical/physical geometry."""
    if wordlines < 2 * (key_width + 1) or key_width <= 0:
        raise ValueError("wordlines must accommodate complementary keys and validity")
    directory.mkdir(parents=True, exist_ok=True)
    config = yaml.safe_load(base_path.read_text())
    architecture = yaml.safe_load((base_path.parent / config["architecture"]).read_text())
    cell_path = (base_path.parent / config["cell"]).resolve()
    cell = yaml.safe_load(cell_path.read_text())
    device_path = (cell_path.parent / cell["memory_device"]).resolve()
    entries = architecture["organization"]["subarray"]["dimensions"][0]
    if entries * key_width % 8 or entries * wordlines % 8:
        raise ValueError("geometry must be expressible as whole bytes")
    architecture["memory"]["word_width"] = f"{key_width}bits"
    architecture["memory"]["capacity"] = f"{entries * key_width // 8}B"
    architecture["organization"]["subarray"]["dimensions"] = [entries, key_width]
    architecture["flash"]["page_size"] = f"{entries // 8}B"
    architecture["flash"]["block_size"] = f"{entries * wordlines // 8}B"
    (directory / "technology.yaml").write_text(
        (base_path.parent / config["technology"]).read_text())
    config["technology"] = "technology.yaml"
    config["architecture"], config["cell"] = "architecture.yaml", "cell.yaml"
    cell["memory_device"] = "memory_device.yaml"
    for name, node in (("case.config.yaml", config), ("architecture.yaml", architecture),
                       ("cell.yaml", cell)):
        (directory / name).write_text(yaml.safe_dump(node, sort_keys=False))
    # Preserve YAML 1.2 keys such as `off`: PyYAML's YAML 1.1 resolver would
    # otherwise rewrite this key as `false` and invalidate the device input.
    (directory / "memory_device.yaml").write_text(device_path.read_text())
    return directory / "case.config.yaml"


def pattern_ladder(probe, pattern):
    """Map declared stored/query symbols to the independently solved series net."""
    d = probe["device"]
    count = probe["geometry"]["data_wordlines"]
    data = np.full(count, d["resistance_pass_ohm"])
    data[0] = d["resistance_read_on_ohm"] if pattern["valid"] else d["resistance_off_ohm"]
    for bit, (stored, query) in enumerate(zip(pattern["stored"], pattern["query"])):
        if query != -1:
            data[2 + 2 * bit + query] = (d["resistance_read_on_ohm"]
                    if stored == -1 or stored == query else d["resistance_off_ohm"])
    resistances = np.r_[d["resistance_select_ohm"], data, d["resistance_select_ohm"]]
    capacitances = np.r_[d["capacitance_source_f"],
                        np.full(count, d["capacitance_internal_f"]), d["capacitance_bitline_f"]]
    return resistances, capacitances


def compare_pattern(probe, pattern, tolerance_v=0.01):
    r, c = pattern_ladder(probe, pattern)
    reference = RcLadderReference(r, c)
    d = probe["device"]
    vpre, decision = d["voltage_precharge_v"], d["decision_time_s"]
    initial = np.full(len(c), vpre)
    moment = reference.first_moments[-1]
    model_tau = pattern["inferred_time_constant_s"]
    if model_tau is None or not np.isfinite(model_tau) or model_tau <= 0:
        raise ValueError("model waveform cannot be reconstructed from its sampled voltage")
    moment_error = abs(model_tau / moment - 1)
    conductance_error = abs(pattern["model_conductance_s"] * np.sum(r) - 1)
    if moment_error > 1e-8 or conductance_error > 1e-10:
        raise AssertionError("model moment/conductance does not match the declared zero-wire circuit")
    # Linear samples resolve the dominant decay; logarithmic samples retain
    # early internal-node modes even when a blocking device makes tau large.
    times = np.unique(np.r_[np.linspace(0, 6 * moment, 401),
                            np.geomspace(np.min(r * c) / 100, 6 * moment, 401), decision])
    exact = reference.voltages(times, initial)[:, -1]
    modeled = vpre * np.exp(-times / model_tau)
    decision_voltage = float(reference.voltages([decision], initial)[0, -1])
    reference_voltage = probe["model"]["reference_voltage_v"]
    margin = (reference_voltage - decision_voltage if pattern["ideal_hit"]
              else decision_voltage - reference_voltage) - d["sense_offset_v"]
    half_time = brentq(lambda time: reference.voltages([time], initial)[0, -1] - vpre / 2,
                      0, 10 * moment, xtol=moment * 1e-11)
    difference = float(np.max(np.abs(exact - modeled)))
    decision_error = abs(decision_voltage - pattern["model_voltage_v"])
    row = {
        "wordlines": probe["geometry"]["data_wordlines"],
        "key_width": probe["geometry"]["key_width"], "pattern": pattern["name"],
        "ideal_hit": pattern["ideal_hit"], "model_voltage_v": pattern["model_voltage_v"],
        "reference_voltage_v": decision_voltage, "decision_absolute_error_v": decision_error,
        "decision_time_s": decision, "sense_reference_voltage_v": reference_voltage,
        "sense_offset_v": d["sense_offset_v"], "required_margin_v": d["min_sense_margin_v"],
        "max_sampled_waveform_absolute_error_v": difference,
        "model_first_moment_s": model_tau, "reference_first_moment_s": float(moment),
        "moment_relative_error": float(moment_error), "conductance_relative_error": float(conductance_error),
        "model_t50_s": float(model_tau * np.log(2)), "reference_t50_s": half_time,
        "t50_relative_error": float((model_tau * np.log(2) - half_time) / half_time),
        "reference_margin_v": float(margin),
        "model_margin_v": pattern["model_margin_v"],
        "model_margin_pass": pattern["model_margin_pass"],
        "reference_margin_pass": bool(margin >= d["min_sense_margin_v"]),
        "decision_within_10mv": bool(decision_error <= tolerance_v),
        "sampled_waveform_within_10mv": bool(difference <= tolerance_v),
    }
    curve = [{"wordlines": row["wordlines"], "pattern": row["pattern"], "time_s": float(t),
              "model_voltage_v": float(m), "reference_voltage_v": float(v)}
             for t, m, v in zip(times, modeled, exact)]
    return row, curve


def compare_precharge(probe):
    """Ideal BL step, source select open, all data WLs at pass, empty nodes.

    This checks whether the configured precharge time can establish the
    uniform initial condition. It is an optimistic linear-network diagnostic,
    not a finite-driver or voltage-dependent flash-device model.
    """
    d = probe["device"]
    count = probe["geometry"]["data_wordlines"]
    r = np.r_[d["resistance_select_ohm"], np.full(count, d["resistance_pass_ohm"])]
    c = np.r_[np.full(count, d["capacitance_internal_f"]), d["capacitance_source_f"]]
    reference = RcLadderReference(r, c)
    vpre = d["voltage_precharge_v"]
    duration = d["precharge_latency_s"]
    source_voltage = float(reference.charging_voltages([duration], vpre)[0, -1])
    moment = reference.first_moments[-1]
    time_99 = brentq(lambda t: reference.charging_voltages([t], vpre)[0, -1] - .99 * vpre,
                     0, 20 * moment, xtol=moment * 1e-11)
    return {"wordlines": count, "configured_precharge_s": duration,
            "source_end_voltage_v": source_voltage, "precharge_rail_v": vpre,
            "source_end_rail_fraction": source_voltage / vpre,
            "time_to_99_percent_s": time_99,
            "reaches_99_percent": bool(source_voltage >= .99 * vpre)}


def write_csv(path, records):
    if not records:
        raise ValueError("CSV requires at least one record")
    with path.open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)


def draw_curves(curves, rows, output):
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.4), layout="constrained")
    for count, color in ((68, "#245e9b"), (512, "#af5b16")):
        selected = [p for p in curves if p["wordlines"] == count and p["pattern"] == "match0"]
        axes[0].plot([p["time_s"] * 1e9 for p in selected], [p["reference_voltage_v"] for p in selected],
                     color=color, label=f"{count} WL, full RC ladder")
        axes[0].plot([p["time_s"] * 1e9 for p in selected], [p["model_voltage_v"] for p in selected],
                     color=color, ls="--", label=f"{count} WL, EvaCAM exponential")
    selected = [r for r in rows if r["pattern"] == "match0"]
    axes[1].plot([r["wordlines"] for r in selected],
                 [r["max_sampled_waveform_absolute_error_v"] * 1e3 for r in selected], "o-", color="#245e9b")
    axes[1].axhline(10, color="#af5b16", ls="--", label="Audit threshold: 10 mV")
    decision_ns = rows[0]["decision_time_s"] * 1e9
    axes[0].axvline(decision_ns, color="#666666", ls=":", label=f"Decision: {decision_ns:g} ns")
    axes[0].set(xlabel="Time (ns)", ylabel="Bitline voltage (V)", title="Uniformly precharged matching string")
    axes[1].set(xlabel="Data wordlines per string", ylabel="Maximum sampled voltage error (mV)",
                title="Approximation error: match0")
    axes[1].set_xscale("log", base=2)
    for ax in axes:
        ax.grid(alpha=.2)
        ax.legend(fontsize=8)
    fig.suptitle("Synthetic zero-wire RC diagnostic — not a paper reproduction", fontsize=12)
    fig.savefig(output / "rc-comparison.png", dpi=180)
    fig.savefig(output / "rc-comparison.svg")
    plt.close(fig)


def run_validation(output, probe_path, binary_path, base_path=BASE, reference_path=REFERENCE):
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    reference = yaml.safe_load(reference_path.read_text())
    audit = comparability(reference)
    subprocess.run([str(binary_path.resolve()), "--quiet", "--output", str(output / "baseline.yaml"),
                    str(base_path.resolve())], check=True, cwd=ROOT, capture_output=True, text=True)
    baseline = yaml.safe_load((output / "baseline.yaml").read_text())
    geometry = baseline["geometry"]
    energy = normalize_energy(baseline["summary"]["energy"]["search_dynamic_j"],
                              geometry["entry_count"], geometry["logical_word_width_bits"])
    rows, curves, precharge = [], [], []
    for wordlines, width in ((16, 7), (32, 15), (64, 31), (68, 32), (128, 63), (256, 127), (512, 255)):
        config = prepare_variant(base_path, output / "inputs" / f"wl{wordlines}", wordlines, width)
        result = subprocess.run([str(probe_path.resolve()), str(config)], check=True,
                                cwd=ROOT, capture_output=True, text=True)
        (config.parent / "probe.yaml").write_text(result.stdout)
        probe = yaml.safe_load(result.stdout)
        precharge.append(compare_precharge(probe))
        for pattern in probe["patterns"]:
            row, curve = compare_pattern(probe, pattern)
            rows.append(row)
            curves.extend(curve)
    write_csv(output / "rc-comparison.csv", rows)
    write_csv(output / "waveforms.csv", curves)
    write_csv(output / "precharge-comparison.csv", precharge)
    write_csv(output / "baseline-energy-breakdown.csv", [
        {"component": name, "energy_j": value,
         "fj_per_logical_bit": normalize_energy(value, geometry["entry_count"], geometry["logical_word_width_bits"])}
        for name, value in baseline["breakdown"]["block_search_dynamic_energy"].items()])
    draw_curves(curves, rows, output)
    source_files = [ROOT / "src/model/NandCamModel.cpp", ROOT / "scripts/nand_rc_reference.py",
                    ROOT / "scripts/validate_nand_literature.py", ROOT / "tests/NandValidationProbe.cpp", reference_path]
    source_files += sorted((base_path.parent).glob("*.yaml"))
    source_files.append((base_path.parent / yaml.safe_load(base_path.read_text())["technology"]).resolve())
    report = {
        "comparison": audit,
        "published_search_energy_fj_per_bit": reference["observations"]["search_energy_fj_per_bit"],
        "baseline": {"calibration_status": baseline["metadata"]["calibration_status"],
                     "logical_entries": geometry["entry_count"], "logical_key_width": geometry["logical_word_width_bits"],
                     "whole_query_latency_s": baseline["summary"]["timing"]["search_latency_s"],
                     "search_energy_fj_per_logical_bit": energy},
        "numerical_reference": {
            "scope": "Exact linear RC transient; synthetic parameters; both models have zero wire parasitics",
            "uniform_internal_node_precharge_assumed": True,
            "waveform_tolerance_v": 0.01,
            "time_sampling": "401 linear points from 0 to 6*tau; 401 logarithmic points from min(R*C)/100 to 6*tau; decision time",
            "tolerance_origin": "Engineering diagnostic chosen for this audit; not from Yang et al.",
            "cases": len(rows),
            "maximum_moment_relative_error": max(r["moment_relative_error"] for r in rows),
            "maximum_sampled_waveform_error_v": max(r["max_sampled_waveform_absolute_error_v"] for r in rows),
            "maximum_decision_error_v": max(r["decision_absolute_error_v"] for r in rows),
            "waveform_cases_exceeding_10mv": sum(not r["sampled_waveform_within_10mv"] for r in rows),
            "decision_cases_exceeding_10mv": sum(not r["decision_within_10mv"] for r in rows),
            "margin_pass_disagreements": sum(r["model_margin_pass"] != r["reference_margin_pass"] for r in rows),
            "pointwise_nonlinear_device_validation": False},
        "precharge_diagnostic": {
            "scope": "Ideal bitline step, source select open, pass-state resistors, initially discharged nodes",
            "threshold_origin": "99 percent of rail is an audit diagnostic, not a paper specification",
            "cases": precharge,
            "cases_below_99_percent": sum(not p["reaches_99_percent"] for p in precharge)},
        "source_sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files},
        "executable_sha256": {str(p.resolve()): hashlib.sha256(p.read_bytes()).hexdigest() for p in (binary_path, probe_path)},
        "versions": {"python": sys.version, "numpy": np.__version__, "scipy": scipy.__version__,
                     "matplotlib": matplotlib.__version__, "pyyaml": yaml.__version__},
    }
    (output / "audit.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "results/nand-validation")
    parser.add_argument("--probe", type=Path, default=ROOT / "test-bin/NandValidationProbe")
    parser.add_argument("--binary", type=Path, default=ROOT / "EvaCAM")
    parser.add_argument("--require-paper-validation", action="store_true")
    args = parser.parse_args()
    try:
        report = run_validation(args.output, args.probe, args.binary)
    except subprocess.CalledProcessError as error:
        print(f"Validation command failed ({error.returncode}): {error.cmd}\n{error.stderr}", file=sys.stderr)
        return 1
    print(json.dumps({"paper_reproduction_validated": report["comparison"]["paper_reproduction_validated"],
                      "numerical_reference": report["numerical_reference"], "artifacts": str(args.output)}, indent=2))
    return 2 if args.require_paper_validation and not report["comparison"]["paper_reproduction_validated"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
