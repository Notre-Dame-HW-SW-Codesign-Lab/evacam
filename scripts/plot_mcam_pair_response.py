#!/usr/bin/env python3
"""Plot nominal all-pair bounds, not Monte Carlo confidence intervals."""

import argparse
import csv
import json
from pathlib import Path
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import evacam_py


def plot_bounds(dimensions, output):
    config = ROOT / "config" / "2FeFET_MCAM_pair" / f"2FeFET_MCAM_{dimensions}x{dimensions}.config.yaml"
    matcher = evacam_py.EvaCAMMatch(str(config))
    bounds = matcher.distance_voltage_bounds([], False)
    directory = output / f"{dimensions}x{dimensions}"
    directory.mkdir(parents=True, exist_ok=True)
    exact = bounds[0]
    records = []
    for bound in bounds:
        records.append({
            "squared_euclidean_distance": int(bound.squared_euclidean_distance),
            "minimum_conductance_s": bound.minimum_conductance,
            "maximum_conductance_s": bound.maximum_conductance,
            "minimum_voltage_v": bound.minimum_voltage,
            "maximum_voltage_v": bound.maximum_voltage,
            "minimum_search_latency_s": bound.minimum_search_latency,
            "maximum_search_latency_s": bound.maximum_search_latency,
            "sensing_time_s": bound.sensing_time,
            "exact_gap_v": exact.minimum_voltage - bound.maximum_voltage,
            "minimum_conductance_stored": json.dumps(bound.minimum_conductance_stored),
            "minimum_conductance_query": json.dumps(bound.minimum_conductance_query),
            "maximum_conductance_stored": json.dumps(bound.maximum_conductance_stored),
            "maximum_conductance_query": json.dumps(bound.maximum_conductance_query),
        })
    with (directory / "nominal_all_pair_bounds.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)

    # NaN slots keep unreachable integer distances out of the drawn envelope.
    distance = np.arange(int(bounds[-1].squared_euclidean_distance) + 1)
    minimum = np.full(distance.size, np.nan)
    maximum = np.full(distance.size, np.nan)
    for bound in bounds:
        index = int(bound.squared_euclidean_distance)
        minimum[index] = 1000 * bound.minimum_voltage
        maximum[index] = 1000 * bound.maximum_voltage

    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 11,
                         "axes.spines.top": False, "axes.spines.right": False})
    figure, axes = plt.subplots(1, 2, figsize=(13, 5), layout="constrained")
    for axis, limit, title in zip(axes, (distance[-1], min(30, distance[-1])),
                                  ("Full distance range", "Near exact match")):
        axis.axhspan(1000 * exact.minimum_voltage, 1000 * exact.maximum_voltage,
                     color="#b47a32", alpha=0.16, label="All exact-match voltages")
        axis.fill_between(distance, minimum, maximum, color="#287d8e", alpha=0.22,
                          label="All stored/query pairs: nominal bounds")
        axis.plot(distance, maximum, color="#216a79", linewidth=1.4)
        axis.plot(distance, minimum, color="#216a79", linewidth=1.4)
        axis.set(xlim=(0, limit), ylim=(0, max(maximum) * 1.1),
                 xlabel="Squared Euclidean distance", ylabel="Sensed matchline voltage (mV)", title=title)
        axis.grid(alpha=0.18)
    axes[0].legend(loc="upper right", fontsize=9)
    figure.suptitle(f"{dimensions}-cell dual-FeFET row | provisional pair-response model\n"
                   f"Common sensing time: {exact.sensing_time * 1e9:.3g} ns; "
                   "digitized curves, assumed 1.1 V characterization bias", fontsize=12)
    for extension in ("svg", "pdf", "png"):
        figure.savefig(directory / f"nominal_all_pair_voltage.{extension}", dpi=180)
    plt.close(figure)

    mismatch_bounds = [bound for bound in bounds if bound.squared_euclidean_distance > 0]
    summary = {
        "dimensions": dimensions,
        "model": "provisional digitized dual-cell pair resistance; nominal only",
        "config": str(config.relative_to(ROOT)),
        "assumed_characterization_bias_v": 1.1,
        "sensing_time_s": exact.sensing_time,
        "exact_minimum_voltage_v": exact.minimum_voltage,
        "exact_maximum_voltage_v": exact.maximum_voltage,
        "worst_exact_vs_mismatch_gap_v": exact.minimum_voltage - max(
            bound.maximum_voltage for bound in mismatch_bounds),
        "distances_whose_voltage_envelopes_overlap_exact": [
            int(bound.squared_euclidean_distance) for bound in mismatch_bounds
            if bound.maximum_voltage >= exact.minimum_voltage
            and bound.minimum_voltage <= exact.maximum_voltage],
        "warning": "Bounds describe this approximate constant-resistance table, not statistical confidence or measured hardware guarantees.",
    }
    (directory / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "results" / "mcam_pair_response")
    args = parser.parse_args()
    for dimensions in (8, 16):
        plot_bounds(dimensions, args.output)


if __name__ == "__main__":
    main()
