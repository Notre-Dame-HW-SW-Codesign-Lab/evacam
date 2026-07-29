#!/usr/bin/env python3

import pathlib
import re
import sys
import tempfile


def parse_yaml_scalars(path):
    scalars = {}
    stack = []

    for raw_line in path.read_text().splitlines():
        if not raw_line.strip() or raw_line.lstrip().startswith("#"):
            continue

        line = raw_line.split("  #", 1)[0].rstrip()
        indent = len(line) - len(line.lstrip(" "))
        stripped = line.strip()
        if ":" not in stripped:
            continue

        key, value = stripped.split(":", 1)
        key = key.strip()
        value = value.strip()

        while stack and stack[-1][0] >= indent:
            stack.pop()
        stack.append((indent, key))

        if value:
            scalars[".".join(part for _, part in stack)] = value

    return scalars


def parse_quantity(value):
    match = re.fullmatch(r"([-+0-9.eE]+)([A-Za-z/%^0-9]*)", value.strip())
    if not match:
        raise AssertionError(f"cannot parse quantity: {value}")

    number = float(match.group(1))
    unit = match.group(2)
    factors = {
        "ps": 1e-12,
        "ns": 1e-9,
        "us": 1e-6,
        "ms": 1e-3,
        "s": 1.0,
        "pJ": 1e-12,
        "nJ": 1e-9,
        "uJ": 1e-6,
        "mJ": 1e-3,
        "J": 1.0,
        "pW": 1e-12,
        "nW": 1e-9,
        "uW": 1e-6,
        "mW": 1e-3,
        "W": 1.0,
        "pm": 1e-12,
        "nm": 1e-9,
        "um": 1e-6,
        "mm": 1e-3,
        "m": 1.0,
        "nm^2": 1e-18,
        "um^2": 1e-12,
        "mm^2": 1e-6,
        "m^2": 1.0,
    }
    if unit not in factors:
        raise AssertionError(f"unsupported unit in quantity: {value}")
    return number * factors[unit], factors[unit]


def assert_yaml_close(yaml_scalars, yaml_key, actual):
    expected, unit_factor = parse_quantity(yaml_scalars[yaml_key])
    # Results YAML rounds rendered values to three decimals in the displayed unit.
    abs_tol = 0.0005 * unit_factor + 1e-24
    if abs(expected - actual) > abs_tol:
        raise AssertionError(
            f"{yaml_key}: yaml={expected:.18e}, api={actual:.18e}, abs_tol={abs_tol:.18e}"
        )


def assert_run_result_matches_yaml(run_result, output_yaml_path):
    yaml_scalars = parse_yaml_scalars(output_yaml_path)
    design_result = run_result.best_results.get("LeakagePower")
    if design_result is None:
        design_result = next(iter(run_result.best_results.values()))

    summary = design_result.summary
    breakdown = design_result.breakdown

    assert_yaml_close(yaml_scalars, "summary.area.total.area", summary["area.total.area_m2"])
    # The current YAML writer labels bank read latency as summary.timing.search_latency.
    assert_yaml_close(yaml_scalars, "summary.timing.search_latency", summary["timing.read_latency_s"])
    assert_yaml_close(yaml_scalars, "summary.power.read_dynamic_energy", summary["energy.read_dynamic_j"])
    assert_yaml_close(yaml_scalars, "summary.power.write_dynamic_energy", summary["energy.write_dynamic_j"])
    assert_yaml_close(yaml_scalars, "summary.power.leakage_power", summary["power.leakage_w"])

    assert_yaml_close(yaml_scalars, "breakdown.subarray_area.total_cell_area", breakdown["subarray_area.total_cell_area_m2"])
    assert_yaml_close(yaml_scalars, "breakdown.search_latency.matchline", breakdown["search_latency.matchline_s"])
    assert_yaml_close(yaml_scalars, "breakdown.search_dynamic_energy.cell_read", breakdown["search_dynamic_energy.cell_read_j"])
    assert_yaml_close(yaml_scalars, "breakdown.leakage.row_decoder", breakdown["leakage.row_decoder_w"])


def main():
    if len(sys.argv) != 2:
        raise SystemExit("Usage: test_pybind_run.py <config.yaml>")

    repo_root = pathlib.Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(repo_root))

    import evacam_py

    result = evacam_py.run(sys.argv[1], threads=1)

    assert result.num_solutions > 0
    assert result.output_yaml_path == ""
    assert result.best_results

    search_result = result.best_results.get("SearchLatency")
    if search_result is None:
        search_result = next(iter(result.best_results.values()))

    assert search_result.optimization_target
    assert search_result.summary["timing.search_latency_s"] > 0
    assert search_result.summary["energy.search_dynamic_j"] > 0
    assert search_result.breakdown["search_latency.matchline_s"] > 0
    assert search_result.geometry["bit_serial_width"] > 0
    assert not search_result.variation.enabled or search_result.variation.samples >= 0

    with tempfile.TemporaryDirectory() as tmp_dir:
        output_yaml_path = pathlib.Path(tmp_dir) / "run_results.yaml"
        yaml_result = evacam_py.run(
            sys.argv[1],
            threads=1,
            output_yaml_path=str(output_yaml_path),
            write_yaml=True,
        )

        assert yaml_result.output_yaml_path == str(output_yaml_path)
        assert output_yaml_path.exists()
        assert "summary:" in output_yaml_path.read_text()
        yaml_scalars = parse_yaml_scalars(output_yaml_path)
        assert yaml_scalars["assumptions.model_identifier"] == "evacam-cam-v1"
        assert yaml_scalars["assumptions.design_target"] == "CAM"
        assert yaml_scalars["assumptions.routing"] == "h_tree"
        assert yaml_scalars["assumptions.technology.process_node"].endswith("nm")
        assert yaml_scalars["assumptions.limitations_reference"] == "docs/limitations.md"
        assert_run_result_matches_yaml(yaml_result, output_yaml_path)

    print("Pybind run test passed")


if __name__ == "__main__":
    main()
