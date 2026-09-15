#!/usr/bin/env python3
"""Bounded-memory enumeration and aggregation of nominal MCAM compositions."""

from dataclasses import dataclass
import csv
from functools import lru_cache
import gzip
import heapq
import math
from pathlib import Path
import resource
import sys
import time

import numpy as np


METRICS = {
    "voltage": ("matchline_voltage", 1e3, "matchline_voltage_v"),
    "latency": ("search_latency", 1e9, "search_latency_s"),
}


@dataclass(frozen=True)
class SelectedComposition:
    counts: tuple
    composition_index: int
    nominal_result: object


@dataclass
class NominalStreamResult:
    representatives_by_metric: dict
    groups_by_metric: dict
    selected: list
    total_count: int
    active_distance: int


def enumerate_delta_counts(dimensions, states):
    """Yield all weak compositions as immutable delta-count tuples."""
    counts = [0] * states

    def visit(index, remaining):
        if index == states - 1:
            counts[index] = remaining
            yield tuple(counts)
            return
        for count in range(remaining + 1):
            counts[index] = count
            yield from visit(index + 1, remaining - count)

    yield from visit(0, dimensions)


def coordinate_permutations(counts):
    """Return the exact number of coordinate arrangements for delta counts."""
    if any(not isinstance(count, int) or count < 0 for count in counts):
        raise ValueError("delta counts must be non-negative integers")
    dimensions = sum(counts)
    denominator = math.prod(_factorial(count) for count in counts)
    return _factorial(dimensions) // denominator


def _bound_witnesses(bounds):
    witnesses = set()
    for bound in bounds:
        for name in ("minimum_conductance_delta_counts",
                     "maximum_conductance_delta_counts"):
            counts = tuple(getattr(bound, name, ()))
            if counts:
                witnesses.add(counts)
    return witnesses


def _stable_score(counts):
    """Return a deterministic 64-bit mixing score without Python hash salt."""
    value = 0xCBF29CE484222325
    for delta, count in enumerate(counts):
        value ^= (delta << 32) | count
        value = (value * 0x100000001B3) & ((1 << 64) - 1)
    return value


def _group_array(groups, y_bins):
    rows = []
    for packed, values in groups.items():
        distance, bin_index = divmod(packed, y_bins)
        composition_count, permutation_count, diversity, value_sum = values
        rows.append((distance, value_sum / composition_count, composition_count,
                     permutation_count, diversity, bin_index))
    rows.sort(key=lambda row: (row[0], row[5]))
    # Object dtype preserves arbitrarily large Python permutation integers.
    return np.asarray(rows, dtype=object)


def _representative_array(extrema, metric):
    rows = []
    attribute, scale, _ = METRICS[metric]
    for distance in sorted(extrema):
        for selected in extrema[distance][metric]:
            result = selected.nominal_result
            rows.append((distance, getattr(result, attribute) * scale,
                         result.matchline_conductance, selected.composition_index))
    return np.asarray(rows, dtype=object)


def _validate_extrema(extrema, nominal_bounds):
    by_distance = {int(bound.squared_euclidean_distance): bound
                   for bound in nominal_bounds}
    if set(extrema) != set(by_distance):
        missing = sorted(set(by_distance) - set(extrema))[:5]
        extra = sorted(set(extrema) - set(by_distance))[:5]
        raise ValueError(f"Nominal reachability differs from bounds; missing={missing}, extra={extra}")
    for distance, metric_extrema in extrema.items():
        bound = by_distance[distance]
        expected = {
            "voltage": (bound.minimum_voltage, bound.maximum_voltage),
            "latency": (bound.minimum_search_latency, bound.maximum_search_latency),
        }
        for metric, (minimum, maximum) in expected.items():
            attribute = METRICS[metric][0]
            actual = tuple(getattr(point.nominal_result, attribute)
                           for point in metric_extrema[metric])
            if not (math.isclose(actual[0], minimum, rel_tol=2e-10, abs_tol=1e-18)
                    and math.isclose(actual[1], maximum, rel_tol=2e-10, abs_tol=1e-18)):
                raise ValueError(
                    f"Streamed nominal {metric} extrema disagree with bounds at D^2={distance}")


def build_nominal_stream(matcher, directory, states, all_bounds,
                         maximum_y_by_metric, budget, *, progress_interval=100_000,
                         plot_y_bins=2_000, progress_callback=None):
    """Enumerate every nominal composition while retaining bounded plot state.

    ``all_bounds`` may contain nominal and varied bound records; all available
    witness compositions are retained. The first contiguous block must be the
    nominal bound set because it supplies the exact extrema oracle.
    """
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    dimensions = int(matcher.vector_dimensions())
    states = int(states)
    all_bounds = list(all_bounds)
    if dimensions < 1 or states < 2 or budget < 1 or plot_y_bins < 2:
        raise ValueError("dimensions, states, budget, and plot_y_bins must be positive")
    if set(maximum_y_by_metric) != set(METRICS):
        raise ValueError("maximum_y_by_metric must provide voltage and latency")
    if any(not math.isfinite(float(value)) or float(value) <= 0
           for value in maximum_y_by_metric.values()):
        raise ValueError("plot metric maxima must be finite and positive")
    if not all_bounds:
        raise ValueError("all_bounds must start with a complete nominal bound sequence")
    nominal_bounds = []
    previous_distance = -1
    for bound in all_bounds:
        distance = int(bound.squared_euclidean_distance)
        if distance <= previous_distance:
            break
        nominal_bounds.append(bound)
        previous_distance = distance
    reachable = {0}
    squared_deltas = [delta * delta for delta in range(states)]
    for _ in range(dimensions):
        reachable = {distance + square for distance in reachable
                     for square in squared_deltas}
    if [int(bound.squared_euclidean_distance) for bound in nominal_bounds] != sorted(reachable):
        raise ValueError("all_bounds does not start with the complete nominal reachability sequence")
    for counts in _bound_witnesses(all_bounds):
        if len(counts) != states or sum(counts) != dimensions or any(count < 0 for count in counts):
            raise ValueError("bound witness delta counts are inconsistent with the stream geometry")

    expected_count = math.comb(dimensions + states - 1, states - 1)
    witnesses = _bound_witnesses(all_bounds)
    witness_rows = {}
    best_mixed = {}
    # Negated scores make heap[0] the worst retained deterministic sample.
    sample_heap = []
    extrema = {}
    groups = {metric: {} for metric in METRICS}
    start = time.monotonic()
    output_path = directory / "nominal_compositions.csv.gz"

    if progress_callback is not None:
        progress_callback(0, expected_count)
    with gzip.open(output_path, "wt", newline="", compresslevel=1) as stream:
        writer = csv.writer(stream)
        writer.writerow(["composition_index", *[f"delta_{delta}_count" for delta in range(states)],
                         "coordinate_permutations", "nonzero_delta_kinds", "changed_coordinates",
                         "squared_euclidean_distance", "matchline_conductance_s", "matchline_voltage_v",
                         "search_latency_s", "matchline_delay_s"])
        for composition_index, counts in enumerate(enumerate_delta_counts(dimensions, states)):
            result = matcher.evaluate_zero_query_composition(counts, 0)
            distance = int(result.squared_euclidean_distance)
            permutations = coordinate_permutations(counts)
            diversity = sum(count > 0 for count in counts[1:])
            selected = SelectedComposition(counts, composition_index, result)
            writer.writerow([composition_index, *counts, permutations, diversity,
                             dimensions - counts[0], distance, result.matchline_conductance,
                             result.matchline_voltage, result.search_latency, result.matchline_delay])

            if counts in witnesses:
                witness_rows[counts] = selected
            score = _stable_score(counts)
            if diversity >= 2:
                prior = best_mixed.get(distance)
                rank = (diversity, score)
                if prior is None or rank > prior[0]:
                    best_mixed[distance] = (rank, selected)
            item = (-score, composition_index, selected)
            if len(sample_heap) < budget:
                heapq.heappush(sample_heap, item)
            elif score < -sample_heap[0][0]:
                heapq.heapreplace(sample_heap, item)

            distance_extrema = extrema.setdefault(distance, {})
            for metric, (attribute, scale, _) in METRICS.items():
                raw_value = getattr(result, attribute)
                endpoints = distance_extrema.get(metric)
                if endpoints is None:
                    distance_extrema[metric] = [selected, selected]
                else:
                    if raw_value < getattr(endpoints[0].nominal_result, attribute):
                        endpoints[0] = selected
                    if raw_value > getattr(endpoints[1].nominal_result, attribute):
                        endpoints[1] = selected
                plot_value = raw_value * scale
                bin_index = min(plot_y_bins - 1, max(0, int(
                    plot_value / float(maximum_y_by_metric[metric]) * plot_y_bins)))
                packed = distance * plot_y_bins + bin_index
                aggregate = groups[metric].get(packed)
                if aggregate is None:
                    groups[metric][packed] = [1, permutations, diversity, plot_value]
                else:
                    aggregate[0] += 1
                    aggregate[1] += permutations
                    aggregate[2] = max(aggregate[2], diversity)
                    aggregate[3] += plot_value

            if progress_interval and (composition_index + 1) % progress_interval == 0:
                elapsed = time.monotonic() - start
                rss_mib = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024
                print(f"Nominal compositions: {composition_index + 1:,}/{expected_count:,} "
                      f"({elapsed:.1f}s, peak RSS {rss_mib:.0f} MiB)", file=sys.stderr, flush=True)
                if progress_callback is not None:
                    progress_callback(composition_index + 1, expected_count)

    if expected_count != composition_index + 1:
        raise ValueError(f"Expected {expected_count} compositions, wrote {composition_index + 1}")
    if progress_callback is not None and (not progress_interval
                                          or expected_count % progress_interval):
        progress_callback(expected_count, expected_count)
    missing_witnesses = witnesses - set(witness_rows)
    if missing_witnesses:
        raise ValueError(f"Missing {len(missing_witnesses)} bound witness compositions")

    _validate_extrema(extrema, nominal_bounds)

    selected_by_counts = dict(witness_rows)
    selected_by_counts.update((value[1].counts, value[1]) for value in best_mixed.values())
    for _, _, selected in sample_heap:
        if len(selected_by_counts) >= max(budget, len(witness_rows) + len(best_mixed)):
            break
        selected_by_counts.setdefault(selected.counts, selected)
    selected = sorted(selected_by_counts.values(), key=lambda row: row.composition_index)

    groups_by_metric = {}
    representatives_by_metric = {}
    for metric, (_, scale, csv_name) in METRICS.items():
        group_array = _group_array(groups[metric], plot_y_bins)
        groups_by_metric[metric] = group_array
        representatives_by_metric[metric] = _representative_array(extrema, metric)
        with (directory / f"nominal_{metric}_plot_groups.csv").open("w", newline="") as stream:
            writer = csv.writer(stream)
            writer.writerow(["squared_euclidean_distance", csv_name, "composition_count",
                             "coordinate_permutations", "maximum_nonzero_delta_kinds",
                             "display_bin", "display_bin_count", "binning"])
            for row in group_array:
                distance, plot_value, composition_count, _, diversity, bin_index = row
                packed = int(distance) * plot_y_bins + int(bin_index)
                exact_permutations = groups[metric][packed][1]
                writer.writerow([int(distance), plot_value / scale, int(composition_count),
                                 exact_permutations, int(diversity), int(bin_index), plot_y_bins,
                                 "distance exact; metric uniformly display-binned"])

    voltage_min_by_distance = {
        distance: values["voltage"][0].nominal_result.matchline_voltage
        for distance, values in extrema.items()
    }
    voltage_max_by_distance = {
        distance: values["voltage"][1].nominal_result.matchline_voltage
        for distance, values in extrema.items()
    }
    global_min = min(voltage_min_by_distance.values())
    global_max = max(voltage_max_by_distance.values())
    cutoff = global_min + 0.05 * (global_max - global_min)
    active_distance = max(1, max(distance for distance, value in voltage_max_by_distance.items()
                                 if value >= cutoff))
    return NominalStreamResult(representatives_by_metric, groups_by_metric, selected,
                               expected_count, active_distance)
@lru_cache(maxsize=None)
def _factorial(value):
    return math.factorial(value)
