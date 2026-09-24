#!/usr/bin/env python3
"""Public NAND matching and run-result contracts through the Python binding."""

import math
import pathlib
import sys
import tempfile

import yaml

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import evacam_py


def rejects(callback, message):
    try:
        callback()
    except (ValueError, RuntimeError) as error:
        assert message in str(error), str(error)
    else:
        raise AssertionError(f"expected rejection containing {message}")


def main():
    config = str(ROOT / "config/NAND_TCAM/NAND_TCAM.config.yaml")
    matcher = evacam_py.EvaCAMMatch(config)
    width = matcher.word_width()
    stored, query, masked = [0] * width, [0] * width, [-1] * width
    exact = matcher.evaluate_nand(stored, query)
    assert exact.hit and exact.sense_margin_pass
    assert matcher.evaluate_vector(stored, masked).hit
    assert matcher.evaluate_nand(masked, query).hit
    invalid = matcher.evaluate_nand(stored, masked, valid=False)
    assert not invalid.hit and invalid.sense_margin_pass
    query[0] = 1
    mismatch = matcher.evaluate_nand(stored, query)
    assert not mismatch.hit
    assert mismatch.matchline_voltage > exact.matchline_voltage
    rows = matcher.evaluate_array([stored, masked], query)
    assert [row.hit for row in rows] == [False, True]
    rejects(lambda: matcher.evaluate_mismatches(1), "vectors")
    rejects(lambda: matcher.evaluate_threshold(0, 1), "Threshold")
    rejects(lambda: matcher.evaluate_nand([0], query), "keyWidth")
    with tempfile.TemporaryDirectory(prefix="evacam-nand-py-") as directory:
        output = pathlib.Path(directory) / "nand.yaml"
        run = evacam_py.run(config, threads=2, write_yaml=True, output_yaml_path=str(output))
        assert run.num_solutions > 0
        assert not any(name.startswith("Read") for name in run.best_results)
        design = run.best_results["SearchLatency"]
        assert design.metadata["model_identifier"] == "evacam-nand-tcam-v1"
        assert design.metadata["calibration_status"] == "synthetic"
        assert design.metadata["read_metrics"] == "unavailable"
        assert "timing.read_latency_s" not in design.summary
        assert not design.variation.enabled
        result = yaml.safe_load(output.read_text())
        assert result["metadata"] == design.metadata
        assert math.isclose(result["summary"]["timing"]["search_latency_s"],
                            design.summary["timing.search_latency_s"], rel_tol=1e-12)
        assert math.isclose(exact.search_latency, design.summary["timing.search_latency_s"], rel_tol=1e-12)
        assert math.isclose(exact.search_dynamic_energy, design.summary["energy.search_dynamic_j"], rel_tol=1e-12)
        assert design.geometry["physical_cell_count"] > design.geometry["logical_capacity_bits"]
    print("NAND Python API tests passed")


if __name__ == "__main__":
    main()
