#!/usr/bin/env python3
"""Export nominal table-based NAND distance bounds, margins, and position sweeps."""
import argparse
import csv
import copy
import hashlib
import json
import math
from pathlib import Path
import re
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))


class ConfigLoader(yaml.SafeLoader):
    # YAML 1.2 booleans: NAND's resistance key "off" must remain a string.
    yaml_implicit_resolvers = copy.deepcopy(yaml.SafeLoader.yaml_implicit_resolvers)


for initial, resolvers in ConfigLoader.yaml_implicit_resolvers.items():
    ConfigLoader.yaml_implicit_resolvers[initial] = [
        (tag, pattern) for tag, pattern in resolvers if tag != "tag:yaml.org,2002:bool"]
ConfigLoader.add_implicit_resolver("tag:yaml.org,2002:bool",
                                   re.compile(r"^(?:true|false|True|False|TRUE|FALSE)$"), list("tTfF"))


def snapshot_config(source, destination):
    """Copy referenced YAMLs and record original hashes; leave source inputs intact."""
    destination.mkdir(parents=True)
    copied, provenance = {}, []

    def copy(path):
        path = path.resolve()
        if path in copied:
            return copied[path]
        name = f"{len(copied):03d}-{path.name}"
        copied[path] = name
        raw = path.read_bytes()
        provenance.append({"path": str(path), "sha256": hashlib.sha256(raw).hexdigest()})
        node = yaml.load(raw, Loader=ConfigLoader)

        def rewrite(value):
            if isinstance(value, dict):
                return {k: rewrite(v) for k, v in value.items()}
            if isinstance(value, list):
                return [rewrite(v) for v in value]
            if isinstance(value, str) and value.endswith((".yaml", ".yml")):
                target = path.parent / value
                if target.is_file():
                    return copy(target)
            return value

        (destination / name).write_text(yaml.safe_dump(rewrite(node), sort_keys=False))
        return name

    root = destination / copy(Path(source))
    return root, provenance


def voltage(value):
    if isinstance(value, (float, int)):
        return float(value)
    match = re.fullmatch(r"\s*([+-]?[\d.]+(?:[eE][+-]?\d+)?)\s*(mV|uV|V)?\s*", value)
    if not match:
        raise ValueError(f"Unsupported reference voltage: {value}")
    return float(match[1]) * {None: 1, "V": 1, "mV": 1e-3, "uV": 1e-6}[match[2]]


def threshold_rows(bounds, invalid_voltage, reference, offset, required):
    """Every reachable threshold cut, including invalid entries in rejection."""
    suffix = [invalid_voltage] * (len(bounds) + 1)
    for i in range(len(bounds)-1, -1, -1):
        suffix[i] = min(suffix[i+1], bounds[i].minimum_voltage)
    accepted = 0
    rows = []
    for i, bound in enumerate(bounds):
        accepted = max(accepted, bound.maximum_voltage)
        rejected = suffix[i+1]
        ref = reference if reference != 0 else (accepted + rejected) / 2
        margin = min(ref-accepted, rejected-ref) - offset
        rows.append({"threshold": bound.squared_euclidean_distance,
                     "accepted_maximum_voltage_V": accepted,
                     "rejected_minimum_voltage_V": rejected,
                     "reference_voltage_V": ref, "sense_margin_V": margin,
                     "required_margin_V": required, "slack_V": margin-required,
                     "pass": margin >= required})
    return rows


def write_csv(path, rows):
    with path.open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def draw(directory, bounds, thresholds, positions, title):
    distance = [b.squared_euclidean_distance for b in bounds]
    low = [b.minimum_voltage for b in bounds]
    high = [b.maximum_voltage for b in bounds]
    for zoom in (False, True):
        fig, axes = plt.subplots(2, 1, figsize=(9, 7), sharex=True, layout="constrained")
        # Draw independent intervals, so unreachable distances are not invented.
        axes[0].vlines(distance, low, high, color="#2878a8", alpha=0.8, label="Exact nominal range")
        axes[0].scatter(distance, low, s=8, color="#2878a8")
        axes[0].scatter(distance, high, s=8, color="#2878a8")
        axes[0].set_ylabel("Bitline voltage (V)")
        axes[0].legend(loc="lower right")
        axes[1].axhline(0, color="black", linewidth=0.8)
        axes[1].plot(distance, [t["slack_V"] for t in thresholds], ".-", color="#a04d18")
        axes[1].set_ylabel("Threshold margin slack (V)")
        axes[1].set_xlabel("Squared symbol distance / threshold")
        for ax in axes:
            ax.grid(alpha=0.2)
        if zoom:
            axes[1].set_xlim(-0.25, min(16, max(distance)) + 0.25)
            visible = [i for i, d in enumerate(distance) if d <= 16]
            ymin = min(low[i] for i in visible)
            ymax = max(high[i] for i in visible)
            pad = max((ymax-ymin)*0.07, 1e-6)
            axes[0].set_ylim(ymin-pad, ymax+pad)
        fig.suptitle(title + "\nNominal fixed-resistance RC; no device variation")
        fig.savefig(directory / ("distance-active.png" if zoom else "distance-full.png"), dpi=160)
        plt.close(fig)
    fig, ax = plt.subplots(figsize=(9, 4), layout="constrained")
    ax.plot([p["symbol_position"] for p in positions], [p["voltage_V"] for p in positions], "o-", color="#2878a8")
    ax.set(xlabel="Symbol position (source to bitline)", ylabel="Bitline voltage (V)",
           title=title + "\nOne adjacent-state mismatch, squared distance = 1")
    ax.ticklabel_format(axis="y", style="plain", useOffset=False)
    ax.grid(alpha=0.2)
    fig.savefig(directory / "position.png", dpi=160)
    plt.close(fig)


def run_case(config, directory, query=None, decision_time_ns=None):
    import evacam_py
    directory.mkdir(parents=True, exist_ok=False)
    snapshot, sources = snapshot_config(config, directory / "config")
    root = yaml.load(snapshot.read_text(), Loader=ConfigLoader)
    arch_path = snapshot.parent / root["architecture"]
    architecture = yaml.load(arch_path.read_text(), Loader=ConfigLoader)
    architecture["design"]["search_function"] = "TH"
    architecture["sensing"]["strict_sense_margin"] = False
    arch_path.write_text(yaml.safe_dump(architecture, sort_keys=False))
    cell = yaml.load((snapshot.parent / root["cell"]).read_text(), Loader=ConfigLoader)
    device_path = snapshot.parent / cell["memory_device"]
    device = yaml.load(device_path.read_text(), Loader=ConfigLoader)
    electrical = device["nand3d" if device["type"] == "NAND3D" else "nand"]
    if not electrical.get("resistance_table"):
        raise ValueError("Distance plots require a NAND resistance_table")
    if decision_time_ns is not None:
        if not math.isfinite(decision_time_ns) or decision_time_ns <= 0:
            raise ValueError("Decision times must be finite and positive")
        electrical["sensing"]["decision_time"] = f"{decision_time_ns}ns"
        device_path.write_text(yaml.safe_dump(device, sort_keys=False))
    matcher = evacam_py.EvaCAMMatch(str(snapshot))
    query = [0]*matcher.symbol_width() if query is None else list(query)
    bounds = matcher.distance_voltage_bounds(query, include_variation=False)
    if not query:
        raise ValueError("Plotting requires a fixed, nonempty query; global bounds are available through the API")
    all_accepted = matcher.evaluate_distance_threshold(query, query, bounds[-1].squared_euclidean_distance)
    zero = matcher.evaluate_distance_threshold(query, query, 0)
    offset = min(zero.sense_reference_voltage-zero.accepted_maximum_voltage,
                 zero.rejected_minimum_voltage-zero.sense_reference_voltage) - zero.sense_margin
    reference = voltage(electrical["sensing"].get("reference_voltage", 0))
    thresholds = threshold_rows(bounds, all_accepted.rejected_minimum_voltage, reference,
                                offset, zero.required_sense_margin)
    records, witnesses = [], []
    for b in bounds:
        records.append({"squared_distance": b.squared_euclidean_distance,
                        "minimum_voltage_V": b.minimum_voltage, "maximum_voltage_V": b.maximum_voltage,
                        "minimum_tau_s": b.minimum_time_constant, "maximum_tau_s": b.maximum_time_constant,
                        "minimum_conductance_S": b.minimum_conductance,
                        "maximum_conductance_S": b.maximum_conductance,
                        "decision_time_s": b.sensing_time})
        witnesses.append({"squared_distance": b.squared_euclidean_distance,
                          "minimum_voltage_stored": b.minimum_voltage_stored,
                          "maximum_voltage_stored": b.maximum_voltage_stored, "query": query})
    positions = []
    states = len(electrical["resistance_table"])
    for index, symbol in enumerate(query):
        stored = query.copy()
        stored[index] = symbol+1 if symbol+1 < states else symbol-1
        result = matcher.evaluate_distance(stored, query)
        positions.append({"symbol_position": index, "voltage_V": result.matchline_voltage,
                          "conductance_S": result.matchline_conductance,
                          "squared_distance": result.squared_euclidean_distance})
    write_csv(directory / "bounds.csv", records)
    write_csv(directory / "thresholds.csv", thresholds)
    write_csv(directory / "positions.csv", positions)
    (directory / "witnesses.json").write_text(json.dumps(witnesses, indent=2)+"\n")
    metadata = {"sources": sources, "query": query, "states": states,
                "symbols": len(query), "decision_time_s": bounds[0].sensing_time,
                "calibration_status": electrical.get("calibration_status", "unspecified"),
                "scope": "Exact nominal bounds within fixed-resistance first-moment RC; no statistical variation",
                "mode": "TH", "strict_sense_margin": False,
                "invalid_entry_minimum_voltage_V": all_accepted.rejected_minimum_voltage,
                "reference_policy": "midpoint per threshold" if reference == 0 else "fixed",
                "passing_threshold_count": sum(t["pass"] for t in thresholds)}
    (directory / "metadata.json").write_text(json.dumps(metadata, indent=2)+"\n")
    title = f"{device['type']} · {states} states · {len(query)} symbols · {bounds[0].sensing_time*1e9:g} ns · {metadata['calibration_status']}"
    draw(directory, bounds, thresholds, positions, title)
    return metadata


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=ROOT / "config/NAND_3D_MLC_TCAM/NAND_3D_MLC_TCAM.config.yaml")
    parser.add_argument("--output-dir", type=Path, required=True, help="New directory; never overwrites a study")
    parser.add_argument("--query", type=int, nargs="+", help="One state index per symbol; default all zero")
    parser.add_argument("--decision-times-ns", type=float, nargs="+", help="Optional sensing-time sweep")
    args = parser.parse_args(argv)
    args.output_dir.mkdir(parents=True, exist_ok=False)
    for index, time in enumerate(args.decision_times_ns or [None]):
        metadata = run_case(args.config, args.output_dir / f"case-{index:03d}", args.query, time)
        print(f"case-{index:03d}: {metadata['passing_threshold_count']} threshold cuts meet the required margin")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
