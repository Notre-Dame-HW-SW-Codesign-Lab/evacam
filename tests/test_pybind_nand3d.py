#!/usr/bin/env python3
"""The 3D NAND CLI and Python API share geometry, provenance, and SI results."""

import math
from pathlib import Path
import subprocess
import sys
import tempfile

import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import evacam_py

CONFIG = ROOT / "config/NAND_3D_TCAM/NAND_3D_TCAM.config.yaml"


def flatten(mapping, prefix=""):
    result = {}
    for key, value in mapping.items():
        name = f"{prefix}.{key}" if prefix else key
        if isinstance(value, dict):
            result.update(flatten(value, name))
        else:
            result[name] = value
    return result


def require_close(actual, expected, name):
    # PyYAML's YAML 1.1 resolver leaves some valid scientific scalars (1e-06)
    # as strings; result maps still contain numeric SI scalars, so parse them.
    actual, expected = float(actual), float(expected)
    assert math.isfinite(actual) and math.isfinite(expected), name
    assert math.isclose(actual, expected, rel_tol=1e-11, abs_tol=1e-30), (name, actual, expected)


def rejects(function, message):
    try:
        function()
    except (ValueError, RuntimeError) as error:
        assert message in str(error), str(error)
    else:
        raise AssertionError(f"expected rejection containing {message}")


def main():
    with tempfile.TemporaryDirectory(prefix="evacam-nand3d-api-") as temporary:
        directory = Path(temporary)
        cli_path, python_path = directory / "cli.yaml", directory / "python.yaml"
        subprocess.run([str(ROOT / "EvaCAM"), "--quiet", "--output", str(cli_path), str(CONFIG)],
                       cwd=ROOT, capture_output=True, text=True, check=True)
        run = evacam_py.run(str(CONFIG), threads=1, write_yaml=True,
                            output_yaml_path=str(python_path), stdout=False)
        assert run.num_solutions > 0
        assert not run.exploration_csv_path
        assert set(run.best_results) == {
            "SearchLatency", "SearchEnergy", "SearchEDP", "Area", "LeakagePower",
            "WriteLatency", "WriteDynamicEnergy", "WriteEDP",
        }
        assert all(name == value.optimization_target for name, value in run.best_results.items())
        design = run.best_results["SearchLatency"]
        cli = yaml.safe_load(cli_path.read_text())
        python = yaml.safe_load(python_path.read_text())
        assert cli["metadata"] == python["metadata"] == design.metadata
        assert design.metadata["model_identifier"] == "evacam-nand3d-tcam-v1"
        assert design.metadata["array_layout"] == "vertical_3d"
        assert design.metadata["model_backend"] == "transient_rc"
        assert design.metadata["calibration_status"] == "synthetic"
        assert design.metadata["device_validation"] == "not_performed_by_evacam"
        assert design.metadata["terminal_conductance_model"] == "dc_linear_resistor_network"
        assert "synthetic" in design.metadata["model_source"].lower()
        assert design.metadata["read_metrics"] == "unavailable"
        assert "transient_solver" in design.metadata
        assert cli["assumptions"]["model_identifier"] == "evacam-nand3d-tcam-v1"
        assert not design.variation.enabled
        for section in ("summary", "geometry", "breakdown"):
            cli_values, python_values = flatten(cli[section]), flatten(python[section])
            structured = getattr(design, section)
            assert cli_values.keys() == python_values.keys() == structured.keys(), section
            for name, value in cli_values.items():
                require_close(value, python_values[name], f"CLI/Python YAML {section}.{name}")
                require_close(value, structured[name], f"CLI/DTO {section}.{name}")

        assert "timing.slowest_match_time_constant_s" not in design.summary
        assert "timing.fastest_mismatch_time_constant_s" not in design.summary
        assert "timing.read_latency_s" not in design.summary
        assert "energy.read_dynamic_j" not in design.summary
        assert any(name.startswith("diagnostics.") for name in design.summary)
        geometry = design.geometry
        entries = geometry["string_rows"] * geometry["string_columns"]
        require_close(geometry["strings_per_block"], entries, "entries per vertical block")
        require_close(geometry["physical_page_bits"], geometry["string_columns"], "physical page selects one group")
        require_close(geometry["physical_cell_count"], entries * geometry["storage_layers"]
                      * geometry["block_count"], "physical storage cells")
        require_close(geometry["allocated_capacity_bits"], entries * geometry["logical_word_width_bits"]
                      * geometry["block_count"], "logical key capacity")
        assert geometry["vertical_stack_height_m"] > 0
        assert geometry["staircase_area_m2"] > 0
        assert geometry["occupied_footprint_area_m2"] > 0
        assert design.summary["timing.program_page_latency_s"] > 0
        assert design.summary["energy.erase_block_dynamic_j"] > 0

    matcher = evacam_py.EvaCAMMatch(str(CONFIG))
    width = matcher.word_width()
    zeros, masked = [0] * width, [-1] * width
    exact = matcher.evaluate_nand(zeros, zeros)
    assert exact.hit and exact.sense_margin_pass
    assert math.isfinite(exact.matchline_voltage) and exact.matchline_voltage >= 0
    assert exact.matchline_conductance > 0  # DC linear-network conductance, not a flash I-V measurement.
    assert exact.search_latency > exact.matchline_delay > 0
    assert matcher.evaluate_nand(zeros, masked).hit
    invalid = matcher.evaluate_nand(masked, masked, valid=False)
    assert not invalid.hit
    assert invalid.matchline_voltage > exact.matchline_voltage
    query = zeros.copy()
    query[0] = 1
    mismatch = matcher.evaluate_nand(zeros, query)
    assert not mismatch.hit
    assert mismatch.matchline_voltage > exact.matchline_voltage
    assert [row.hit for row in matcher.evaluate_array([zeros, masked], query)] == [False, True]
    rejects(lambda: matcher.evaluate_nand([0], query), "key")
    rejects(lambda: matcher.evaluate_mismatches(1), "vectors")
    print("3D NAND Python/CLI API tests passed")


if __name__ == "__main__":
    main()
